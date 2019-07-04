// Copyright: (c) Jaromir Veber 2018-2019
// Version: 01042019
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************
#include <libconfig.h++>  // parse configuration file
#include <json-c/linkhash.h>  // access JSON-C dictionary object (json_foreach_..)

#include <stdexcept>    // for excpetion
#include <regex>        // regex_match
#include <future>       // packaged_task
#include <ctime>

#include "mq_exe_daemon.h"

void time_thread_start(Exe_Service *exe_service) {
    exe_service->time_thread_loop();
}

const std::array<std::string, 1 > Exe_Service::kTableDefinitions = {
        "CREATE TABLE IF NOT EXISTS script     (name TEXT PRIMARY KEY, script TEXT)",  // basic table for scripts
};

std::string log_init = "CREATE TABLE IF NOT EXISTS log (timestamp TIMESTAMP PRIMARY KEY, level INTEGER, thread INTEGER, msgid INTEGER, logger STRING, message STRING)";

const std::array<std::string, 1 > Exe_Service::kStatementDefinitions = {
    "SELECT * FROM script",
};

const std::string Exe_Service::kReloadTopic = "app/exe/reload";

Exe_Service::Exe_Service() : Daemon("mq_exe_daemon", "/var/run/mq_exe_daemon.pid"), _tokener(json_tokener_new()), _terminate_time_thread(false) {}

void Exe_Service::load_daemon_configuration() {
    spdlog::set_pattern("[%x %H:%M:%S.%e][%n][%t][%l] %v");
    
    static const char* config_file = "/etc/mq_system/mq_exe_daemon.conf";
    static const char* default_db_uri = "/var/db/mq_exe_system.db";
    Config cfg;

    try {
        cfg.readFile(config_file);
    } catch(const FileIOException &fioex) {
        _logger->error("I/O error while reading system configuration file: {}", config_file);
        throw std::runtime_error("");
    } catch(const ParseException &pex) {
        _logger->error("Parse error at {} : {} - {}", pex.getFile(), pex.getLine(), pex.getError());
        throw std::runtime_error("");
    }
    try {
        const auto& root = cfg.getRoot();
        if (root.exists("uri"))
            root.lookupValue("uri", _db_uri);
        else
            _db_uri = default_db_uri;
        if  (cfg.exists("log_level")) {
            int level;
            cfg.lookupValue("log_level", level);
            _logger->set_level(static_cast<spdlog::level::level_enum>(level));		
        }
    } catch(const SettingNotFoundException &nfex) {
        _logger->error("Required setting not found in system configuration file");
        throw std::runtime_error("");
    } catch (const SettingTypeException &nfex) {
        _logger->error("Seting type error (at system configuaration) at: {}", nfex.getPath());
        throw std::runtime_error("");
    }
}

void Exe_Service::check_and_init_database() {
    for (const auto& table_definition :  kTableDefinitions) {
        char *errmsg = nullptr;
        if (SQLITE_OK != sqlite3_exec(_pDb, table_definition.c_str(), NULL, NULL, &errmsg))
            _logger->warn("Sqlite3: create table error: {}", errmsg);
    }
    for (const auto& kStatementDefinition : kStatementDefinitions) {
        sqlite3_stmt *temp_stmt;
        if (sqlite3_prepare_v2(_pDb, kStatementDefinition.c_str(), kStatementDefinition.length(), &temp_stmt, nullptr) != SQLITE_OK)
            _logger->error("Prepare table statement error: {}", kStatementDefinition);
        _statements.push_back(temp_stmt);
    }
}

bool Exe_Service::scan_script(const std::string& content, std::unordered_set<std::string>& sensor_list) {
    const std::string regval_string("register_value("); 
    const std::regex sensor_regex("(\\w|\\/)+\\:\\w+");

    for (size_t regval_string_scan_position = 0; regval_string_scan_position != std::string::npos; ) {
        size_t&& result = content.find(regval_string, regval_string_scan_position);
        if (result == std::string::npos)
            break;
        regval_string_scan_position = result + regval_string.size();
        result = content.find(')', regval_string_scan_position);
        if (result == std::string::npos)
            break;
        const auto& func_end = result;
        for (size_t iter = regval_string_scan_position;;) {
            size_t&& result = content.find('"', iter);
            if (result == std::string::npos || result > func_end)
                break;
            const auto& str_start = result + 1;
            result = content.find('"', str_start);
            if (result == std::string::npos || result > func_end)
                break;
            iter = result + 1;
            const auto& extracted_sensor_value_string = content.substr(str_start, result - str_start);
            if (!std::regex_match(extracted_sensor_value_string, sensor_regex))
                return false;
            const auto& sensor_value_separator_position = extracted_sensor_value_string.find(":");
            auto&& sensor_name = std::string("status/") + extracted_sensor_value_string.substr(0, sensor_value_separator_position);
            _logger->debug("scan script - adding string \"{}\" to sensor list", sensor_name);
            sensor_list.emplace(sensor_name);
        }
        regval_string_scan_position = result;
    }
    return true;
}

/**
 * Right now this scan works well; however it does not work with LUA comments. It is kind of complicated to implement it so right now we 
 * accept this feature (BUG?) as it is. It should not affect users too much - if it is at least documented.
 */
void Exe_Service::load_and_run_scripts() {
    auto select_from_script = _statements[0];
    std::vector<std::pair<std::string, std::string>> script_store;
    std::unordered_set<std::string> sensor_list;

    auto sqresult = sqlite3_step(select_from_script);
    while(sqresult != SQLITE_DONE){
        if (sqresult != SQLITE_ROW) {
            _logger->error("Sqlite error unexpected result {} : {}", sqresult, sqlite3_errmsg(_pDb));
            break;
        }
        std::string script_name = reinterpret_cast<const char*>(sqlite3_column_text(select_from_script, 0));
        std::string script_content = reinterpret_cast<const char*>(sqlite3_column_text(select_from_script, 1));
        _logger->debug("Script {}  ; Content: {}", script_name, script_content);
        if (scan_script(script_content, sensor_list))
            script_store.emplace_back(script_name, script_content);
        else
            _logger->warn("Script {} - scan error - not adding it - please fix the content so it may pass static analysis", script_name);
        sqresult = sqlite3_step(select_from_script);
    }
    sqlite3_reset(select_from_script);

    for (const auto& sensor : sensor_list)
        Subscribe(sensor);

    _terminate_lua_threads = false;
    for (const auto& script : script_store)
        execute_lua_script(script.first, script.second);
}

void Exe_Service::start_all() {
    _logger->trace("start all");
    load_and_run_scripts();  // load & start all the lua scripts (this takes some time ~2 secs)
    Subscribe(kReloadTopic);  // subscribe to reaload event
}

void Exe_Service::stop_all() {
    _logger->trace("stop_all");
    Unsubscribe("#");  // stop reacting to any MQTT message (I hope this works..)
    _terminate_lua_threads = true;  // issue command to stop all the treads that will wake (in wait function) - they will end with error "terminate requested"
    _logger->trace("lock event map");
    std::unique_lock<std::mutex> value_wait_lock(_map_mutex);  // get lock to event map and wake all the waiting LUA scripts
    for (auto && el : _value_wait_map) {  // issue events LUA scripts wait for (wake them)
        if (el.second != nullptr) {
            ++el.second->num;
            el.second->cond_var.notify_all();
        }
    }
    value_wait_lock.unlock();  // release lock so the threads may access the map (they gonna clean it if possible)
    _logger->trace("lock time map");
    std::unique_lock<std::mutex> time_lock(_time_mutex); // lock the time map and wake all the waiting LUA scripts
    for (auto && el : _time_wait_map) {
        if (el.second != nullptr) {  // issue events LUA scripts wait for (wake them)
            ++el.second->num;
            el.second->cond_var.notify_all();
        }
    }
    time_lock.unlock();
    _logger->debug("join script threads");
    for (auto && script : _script_threads)
        script.join();
    _script_threads.clear();
    value_wait_lock.lock();     // well mosquitto (main) thread may still try to access the map but since it should be already unsubscribed it is highly unprobable scenario, but to be sure...
    _value_wait_map.clear();    // map should be already clear unless something bad happend but to be sure it's clear.. we may clear it now!
    value_wait_lock.unlock();
    time_lock.lock();           // time thread is still running we're not terminating it so the lock here is necessary
    _time_wait_map.clear();     // this map may be acessed by time thread so lock it be4 emergency cleanup (map should be already clear unless something bad happend but to be sure it's clear.. we may clear it now!)
}

void Exe_Service::main() {
    _logger->trace("Daemon Start");
    load_daemon_configuration();
    if (!sqlite3_threadsafe()) {
        if(sqlite3_config(SQLITE_CONFIG_SERIALIZED)) {
            _logger->warn("Unable to set serialized mode for SQLite! Tt is necessary to recompile it SQLITE_THREADSAFE!");
            _logger->warn("We Continue using unsafe interface but beware of possible crash");
        }
    }
    sqlite3_initialize();
    if (SQLITE_OK != sqlite3_open_v2(_db_uri.c_str(), &_pDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL)) {
        _logger->error("Sqlite3: Unable to open file:  {}", _db_uri);
        throw std::runtime_error("");
    }
    sqlite3_extended_result_codes(_pDb, true);
    check_and_init_database();
    _time_thread = std::move(std::thread(time_thread_start, this));
    _logger->trace("Exe system initialized");
    start_all();
    _logger->info("--- Threads started ---");
    std::this_thread::sleep_until(std::chrono::time_point<std::chrono::system_clock>::max());
}

void Exe_Service::CallBack(const std::string& topic, const std::string& message) {
    auto topic_type = topic.substr(0, topic.find_first_of('/'));
    _logger->debug(topic_type);
    if (topic_type == "status")
        parse_status_message(topic, message);
    else if (topic_type == "app")
        parse_app_message(topic);
    return;
}

void Exe_Service::parse_app_message(const std::string& topic) {
    // this is slow so we do it in separate thread because original thread would stop all the reporting to mosquitto
    std::packaged_task<void(void)> reload_scripts([this](void) {
        _logger->trace("----------- Runtime reload -----------");
        stop_all();
        start_all();
        _logger->trace("----------- Reload completed -----------");
    });
    
    _logger->debug(topic);
    if (topic == kReloadTopic) {
        reload_scripts();
    }
}

void Exe_Service::parse_status_message(const std::string& topic, const std::string& message) {
    const std::string& message_topic = topic;
    struct json_object* message_json_root_object = json_tokener_parse_ex(_tokener, message.c_str(), message.size());

    if (json_object_get_type(message_json_root_object) != json_type_object) {
        _logger->warn("Did not recieve object as initial json type - bad (unexpected) json format: {}", message);
        return;
    }

    json_object_object_foreach(message_json_root_object, key_string, current_object) {
        const auto value_name = std::string(key_string);
        const auto sensor_value_name = message_topic + ":" + value_name;
        //std::string units;  // we do not need units string at all
        if (json_object_get_type(current_object) == json_type_array) {
            //units = json_object_get_string(json_object_array_get_idx (current_object, 1));  //uints are second element in array
            current_object = json_object_array_get_idx (current_object, 0); //while first is the value
        }
        switch (json_object_get_type(current_object)) {
            case json_type_int:
            case json_type_double: 
                {
                    double value = json_object_get_double(current_object);
                    _logger->trace("Received sensor {} name {} value: {}", message_topic, value_name, value);
                    std::unique_lock<std::mutex> value_lock(_value_mutex);
                    _last_val_double_map[sensor_value_name] = value;
                }
                break;
            case json_type_boolean: 
                {
                    bool value = json_object_get_boolean(current_object);
                    std::unique_lock<std::mutex> value_lock(_value_mutex);
                    _last_val_boolean_map[sensor_value_name] = value;
                }
                break;
            default:
                _logger->debug("Json unexpected type of object for value name: {} payload: {} ", value_name, message);
                break;
        }
        // notify threads about update AFTER values were already updated in value map!
        std::unique_lock<std::mutex> value_wait_lock(_map_mutex);
        auto range = _value_wait_map.equal_range(sensor_value_name);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second != nullptr) {
                ++it->second->num;
                it->second->cond_var.notify_all();
                it->second = nullptr;
            }
        }
    }
    json_object_put(message_json_root_object);  // free message object tree
}

Exe_Service::~Exe_Service() noexcept {
    _logger->debug("~Exe_Service()");
    _terminate_time_thread = true;
    stop_all();
    _time_thread.join();
    json_tokener_free(_tokener);
    for (auto&& statement : _statements)
        sqlite3_finalize(statement);
    int i = 0;
    for (; i < 2; ++i) {
      if (sqlite3_close(_pDb) == SQLITE_OK)
          break;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    if (i >= 2)
        _logger->critical("Sqlite exe not closed the way it should - it may be inconsistent!");
}

int main() {
    try {
        ::tzset();
        Exe_Service *d = new Exe_Service();
        d->main();
        delete d;
    } catch (const std::exception& error) {
        return -1;
    }
    return 0;
}

void Exe_Service::time_thread_loop() {
    _logger->trace("Time thread start");
    while ( !_terminate_time_thread ) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::unique_lock<std::mutex> time_lock(_time_mutex);
        if (!_time_wait_map.empty()) {
            auto now = std::chrono::system_clock::now();
            for (auto it = _time_wait_map.begin(); it != _time_wait_map.end() && it->first <= now; ++it) {
                if (it->second != nullptr) {
                    _logger->trace("time_map event!");
                    ++it->second->num;
                    it->second->cond_var.notify_all();
                    it->second = nullptr;
                }
            }
        }
    }
    _logger->trace("time_thread_loop - terminate");
}
