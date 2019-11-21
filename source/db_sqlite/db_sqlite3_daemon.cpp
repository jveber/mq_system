// Copyright: (c) Jaromir Veber 2017-2019
// Version: 29032019
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************
// Database writing deamon that is part of MQ System.
// This file is core of the daemon
#include <json-c/json_object.h>     // JSON format for communication (writing)
#include <json-c/json_tokener.h>    // JSON format translation (reading)
#include <json-c/linkhash.h>        // access JSON-C dictionary object - (may not be ncessary)
#include <sqlite3.h>                // SQLite communication

#include <stdexcept>      // for excpetion
#include <array>          // for constant C++11 iterable arrays
#include <unordered_map>  // for fast search and storage of statements
#include <vector>
#include <chrono>
#include <limits>

#include <libconfig.h++>  // parse configuration file
#include "mq_lib.h"       // MQ_System utility library

using namespace MQ_System;
using namespace libconfig;

class SQLite_DB_Service : public Daemon {
public:
    SQLite_DB_Service();
    virtual ~SQLite_DB_Service() noexcept;
    void main();
    virtual void CallBack(const std::string& topic, const std::string& message) override;
 private:
    static const std::array<std::string, 7> kTableDefinitions;
    static const std::array<std::string, 9> kStatementDefinitions;

    struct ValueEvent {
        ValueEvent(double val, std::chrono::time_point<std::chrono::steady_clock> t) : value(val), time_mark(t) {}
        double value;
        std::chrono::time_point<std::chrono::steady_clock> time_mark;
    };

    struct Value_data {
        Value_data(uint64_t i, bool a, double pre): averaging(a), interval(i), precision(pre), last_val(std::numeric_limits<decltype(last_val)>::quiet_NaN()) {}
        const bool averaging;
        const uint64_t interval;
        const double precision;
        double last_val;
        std::chrono::time_point<std::chrono::steady_clock> last_update;
        std::vector<ValueEvent> ValEvents;
    };
    // one sensor may report multiple values so two maps "sensor name" : "value name" : "actual value"
    struct SensorData {
        SensorData() : interval(std::numeric_limits<decltype(interval)>::max()) {}
        uint64_t interval;
        std::chrono::time_point<std::chrono::steady_clock> last_update;
        std::unordered_map<std::string, Value_data> values;
    };

    std::unordered_map<std::string, SensorData> _sensors;
    std::vector<sqlite3_stmt *> _statements;
    std::unordered_map<std::string, sqlite3_int64> _known_sensors;
    std::unordered_map<std::string, sqlite3_int64> _known_names;
    std::unordered_map<std::string, sqlite3_int64> _known_units;
    std::string _db_uri;
    sqlite3* _pDb;
    struct json_tokener* const _tokener;
    
    void load_daemon_configuration();
    void check_and_init_database();

    std::unordered_map<std::string, sqlite3_int64>::const_iterator get_name_id(std::unordered_map<std::string, sqlite3_int64>&,const std::string&, sqlite3_stmt *, sqlite3_stmt *, sqlite3_int64 = std::numeric_limits<sqlite3_int64>::max());
};

SQLite_DB_Service::~SQLite_DB_Service() noexcept {
    Unsubscribe("#");  //unsubscribe all - makes it safe because we destroy lot of objects that might by used by concurent thread.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    for (auto&& statement : _statements) {
        sqlite3_finalize(statement);
    }
    json_tokener_free(_tokener);
    constexpr int db_closure_attempts = 3;
    const auto db_close_result = [this]() {
        for (int i = 0; i < db_closure_attempts; ++i) {
            if (sqlite3_close(_pDb) == SQLITE_OK)
                return i;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        return db_closure_attempts;
    }();
    if (db_close_result == db_closure_attempts)
        _logger->critical("Sqlite db not closed the way it should - it may be inconsistent!");
}

// TODO - fix & finish valbool (not everything may work well) - eg. valsensor is real only (so it may need to be finished. 
const std::array<std::string, 7 > SQLite_DB_Service::kTableDefinitions = {
    "PRAGMA optimize; CREATE TABLE IF NOT EXISTS sensor (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE)",							// basic table for list of devices & their IDs
    "CREATE TABLE IF NOT EXISTS unit       (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE)",										// basic table for list of unit_name & their IDs
    "CREATE TABLE IF NOT EXISTS valname    (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, unit_id INT REFERENCES unit(id))",		// basic table for list of value names & their IDs
    "CREATE TABLE IF NOT EXISTS valreal    (timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL, sensor_id INT REFERENCES sensor(id) NOT NULL, valname_id INT REFERENCES valname(id), value REAL, PRIMARY KEY(timestamp, sensor_id, valname_id))",  				// table to store values of type REAL
    "CREATE TABLE IF NOT EXISTS valbool    (timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL, sensor_id INT REFERENCES sensor(id) NOT NULL, valname_id INT REFERENCES valname(id), value BOOLEAN, PRIMARY KEY(timestamp, sensor_id, valname_id))",			// table to store values of type BOOLEAN
    "CREATE TABLE IF NOT EXISTS valsensor  (valname_id INT REFERENCES valname(id) NOT NULL, sensor_id INT REFERENCES sensor(id) NOT NULL, timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL, value REAL, PRIMARY KEY(valname_id, sensor_id))",					// advanced table for optimization of valuename<->sensor lookup in application
    "CREATE TRIGGER IF NOT EXISTS valsensor_valreal_trigger AFTER INSERT ON valreal BEGIN INSERT OR REPLACE INTO valsensor (valname_id, sensor_id, value) VALUES (NEW.valname_id, NEW.sensor_id, NEW.value); END;",													// this automatically fills (updates) previous table
    // If it proves to be somehow useful to have also string and blob I'll add them but for now ...
    // "CREATE TABLE IF NOT EXISTS valstring    (timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL, sensor_hash INT, name_hash INT, value STRING)",
    // "CREATE TABLE IF NOT EXISTS valblob    (timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL, sensor_hash INT, name_hash INT, value BLOB)",
};

const std::array<std::string, 9 > SQLite_DB_Service::kStatementDefinitions = {
    "INSERT INTO sensor (name) VALUES (?)",
    "INSERT INTO unit (name) VALUES (?)",
    "INSERT INTO valname (name, unit_id) VALUES (?, ?)",
    "INSERT INTO valreal (sensor_id, valname_id, value) VALUES (?, ?, ?)",
    "INSERT INTO valbool (sensor_id, valname_id, value) VALUES (?, ?, ?)",
    "SELECT id FROM sensor WHERE name = ?",
    "SELECT id FROM unit WHERE name = ?",
    "SELECT id FROM valname WHERE name = ?",
    "SELECT value FROM valreal LEFT JOIN sensor ON valreal.sensor_id = sensor.id LEFT JOIN valname ON valreal.valname_id = valname.id WHERE sensor.name = ? AND valname.name = ? ORDER BY timestamp DESC LIMIT 1",
};

constexpr size_t INSERT_sensor_index = 0;
constexpr size_t INSERT_unit_index = 1;
constexpr size_t INSERT_valname_index = 2;
constexpr size_t INSERT_valreal_index = 3;
constexpr size_t INSERT_valbool_index = 4;
constexpr size_t SELECT_id_sensor_index = 5;
constexpr size_t SELECT_id_unit_index = 6;
constexpr size_t SELECT_id_valname_index = 7;
constexpr size_t SELECT_value_valreal_index = 8;

SQLite_DB_Service::SQLite_DB_Service(): Daemon("mq_db_daemon", "/var/run/mq_db_daemon.pid"), _tokener(json_tokener_new()) {}

// well there is now std::gcd in C++17 but since we require C++11 so C++17 may not be present we'll 
// use this code snippet that may not be so effective but...  
uint64_t gcd( uint64_t x, uint64_t y ) {
       if ( x < y )
          std::swap( x, y );
       while ( y > 0 ) {
          uint64_t f = x % y;
          x = y;
          y = f;
       }
       return x;
}

void SQLite_DB_Service::load_daemon_configuration() {
    static const char* config_file = "/etc/mq_system/mq_db_daemon.conf";
    static const char* default_db_uri = "/var/db/mq_system.db";
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
        const auto& db_elements = root.lookup("db");
        for (auto db_element = db_elements.begin(); db_element != db_elements.end(); ++db_element) {
            const std::string sensor_name = db_element->lookup("name");
            if (!db_element->exists("values")) {
                _logger->warn("Configuration: Sensor: {} is missing value definitions - ignoring it!", sensor_name);
                continue;
            }
            decltype(_sensors)::mapped_type inner;
            const auto& db_values = db_element->lookup("values");
            for (const auto& db_value : db_values) {
                // value name
                const std::string value_name = db_value.lookup("name");
                // value averaging
                const bool value_averaging = db_value.exists("averaging") ? db_value.lookup("averaging") : false;
                // value interval
                const uint64_t value_interval = [&db_value, this]() -> uint64_t {
                    if (db_value.exists("interval")) {
                        const auto& value_type = db_value.lookup("interval");
                        switch (value_type.getType()) {
                            case Setting::Type::TypeInt:
                            {
                                int int_value = value_type; 
                                return static_cast<uint64_t>(int_value) * 1000000000L;
                            }
                            case Setting::Type::TypeInt64:
                            {
                                uint64_t long_value = static_cast<uint64_t>(value_type);
                                return static_cast<uint64_t>(long_value) * 1000000000L;
                            }
                            case Setting::Type::TypeFloat:
                            {
                                float float_value = static_cast<float>(value_type);
                                return static_cast<uint64_t>(float_value) * 1000000000L;
                            }
                            default:
                                _logger->warn("Value type not handled (interval) ignoring it! (please use number)");
                        }
                    }
                    return 0;
                }();
                const double precision = db_value.exists("precision") ? db_value.lookup("precision") : 0.0;
                // if there is a single value with averaging we need to read all messages to get the average
                if (value_averaging)
                    inner.interval = 0;
                // well this may seem to be a bit complicated but if there is no averaging on values of sensor - we use GCD for interval
                if (inner.interval != 0) {
                    if (inner.interval == std::numeric_limits<decltype(inner.interval)>::max())
                        inner.interval = value_interval;
                    else {
                        inner.interval = gcd(value_interval, inner.interval);
                    }
                }
                _logger->debug("Sensor {} Value {}, averaging {}, interval {}, precision {}",sensor_name, value_name, value_averaging, value_interval, precision);
                inner.values.emplace(std::piecewise_construct, std::forward_as_tuple(value_name), std::forward_as_tuple(value_interval, value_averaging, precision));
            }
            if (inner.values.size()) {
                _sensors.emplace(std::string("status/") + sensor_name, inner);
            } else {
                _logger->warn("Sensor {} contain no values in configuration - ignoring the record", sensor_name);
            }
        }
    } catch(const SettingNotFoundException &nfex) {
        _logger->error("Required setting not found in system configuration file");
        throw std::runtime_error("");
    } catch (const SettingTypeException &nfex) {
        _logger->error("Seting type error (at system configuaration) at: {}", nfex.getPath());
        throw std::runtime_error("");
    }
}

void SQLite_DB_Service::check_and_init_database() {
    for (const auto& table_definition :  kTableDefinitions) {
        char *errmsg = nullptr;
        if (SQLITE_OK != sqlite3_exec(_pDb, table_definition.c_str(), NULL, NULL, &errmsg)) {
            _logger->warn("Sqlite3: fixed statement {} error: {}", table_definition, errmsg);
        }
    }
    for (const auto& kStatementDefinition : kStatementDefinitions) {
        sqlite3_stmt *temp_stmt;
        if (sqlite3_prepare_v2(_pDb, kStatementDefinition.c_str(), kStatementDefinition.length(), &temp_stmt, nullptr) != SQLITE_OK) {
            _logger->warn("Prepare table statement error: {}", kStatementDefinition);
        }
        _statements.push_back(temp_stmt);
    }
    // load last values (if exist for precision change check)
    const auto last_val_stmt = _statements[SELECT_value_valreal_index];
    for (auto&& sensor : _sensors) {
        for (auto && value : sensor.second.values) {
            if (value.second.precision != 0.0) {
                if (SQLITE_OK != sqlite3_bind_text(last_val_stmt, 1, sensor.first.c_str(), sensor.first.size(), SQLITE_STATIC))
                    _logger->error("Sqlite error {}", 700);
                if (SQLITE_OK != sqlite3_bind_text(last_val_stmt, 2, value.first.c_str(), value.first.size(), SQLITE_STATIC))
                    _logger->error("Sqlite error {}", 701);
                auto sqresult = sqlite3_step(last_val_stmt);
                if (sqresult == SQLITE_DONE) {
                    // value stays NAN
                } else if (sqresult == SQLITE_ROW) {
                    value.second.last_val = sqlite3_column_double(last_val_stmt, 0);
                } else {
                    // value stays NAN
                    _logger->error("Sqlite error {} unexpected result {} : {}", 703, sqresult, sqlite3_errmsg(_pDb));
                }
                if (SQLITE_OK != sqlite3_reset(last_val_stmt))
                    _logger->error("Sqlite error {}", 705);
            }
        }
    }
    if (SQLITE_OK != sqlite3_finalize(last_val_stmt)) {
        _logger->error("Unable to finalize statement: {}", sqlite3_errmsg(_pDb));
    } else {
        _statements[SELECT_value_valreal_index] = nullptr;
    }
}

void SQLite_DB_Service::main() {
    _logger->trace("Daemon Start");
    load_daemon_configuration();
    _logger->trace("Config done");
    if (!sqlite3_threadsafe()) {
        if (sqlite3_config(SQLITE_CONFIG_SERIALIZED)) {
            _logger->warn("Unable to set serialized mode for SQLite! Tt is necessary to recompile it SQLITE_THREADSAFE!");
            _logger->warn("Continue using unsafe interface but beware of possible crash");
        }
    }

    if (SQLITE_OK != sqlite3_open_v2(_db_uri.c_str(), &_pDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL)) {
        _logger->error("Sqlite3: Unable to open file:  {}", _db_uri);
        throw std::runtime_error("");
    }

    if (SQLITE_OK != sqlite3_extended_result_codes(_pDb, true))
        _logger->warn("Unable to set extended result codes");

    _logger->trace("Sqlite Init done");
    check_and_init_database(); 
    _logger->trace("Sqlite initialized");
    for (const auto& sensor : _sensors)
        Subscribe(sensor.first);
    _logger->trace("Subscribed - Sleeping");
    SleepForever();
}

std::unordered_map<std::string, sqlite3_int64>::const_iterator SQLite_DB_Service::get_name_id(std::unordered_map<std::string, sqlite3_int64>& map, const std::string& message_value_name, sqlite3_stmt* insert, sqlite3_stmt* request, sqlite3_int64 unit_id) {
    std::unordered_map<std::string, sqlite3_int64>::const_iterator search_result = map.find(message_value_name);
    if (search_result == map.cend()) {
        if (SQLITE_OK != sqlite3_bind_text(request, 1, message_value_name.c_str(), message_value_name.size(), SQLITE_STATIC))
            _logger->error("Sqlite error {}", 600);
        auto sqresult = sqlite3_step(request);
        if (sqresult == SQLITE_DONE) {
            // no result found so insert value name it into "valname" table
            if (SQLITE_OK != sqlite3_bind_text(insert, 1, message_value_name.c_str(), message_value_name.size(), SQLITE_STATIC))
                _logger->error("Sqlite error {}", 601);
            if (unit_id != std::numeric_limits<decltype(unit_id)>::max()) {
                if (SQLITE_OK != sqlite3_bind_int64(insert, 2, unit_id))
                    _logger->error("Sqlite error {}", 602);
            }
            if (SQLITE_DONE != sqlite3_step(insert))
                _logger->error("Sqlite error {} : {}", 603, sqlite3_errmsg(_pDb));
            if (SQLITE_OK != sqlite3_reset(insert))
                _logger->error("Sqlite error {}", 604);
            sqlite3_clear_bindings(insert);
            //now we ask it for once more in order to know assigned id
            if (SQLITE_OK != sqlite3_reset(request))
                _logger->error("Sqlite error {}", 605);
            sqresult = sqlite3_step(request);
        }
        if (sqresult == SQLITE_ROW) {
            search_result = map.emplace(message_value_name, sqlite3_column_int64(request, 0)).first;
        } else {
            _logger->error("Sqlite error unexpected result {} : {}", sqresult, sqlite3_errmsg(_pDb));
        }
        if (SQLITE_OK != sqlite3_reset(request))
            _logger->error("Sqlite error {}", 606);
        sqlite3_clear_bindings(request);    // all the clear_bindings may be removed in production version
    }
    return search_result;
}

void SQLite_DB_Service::CallBack(const std::string& topic, const std::string& message) {
    _logger->trace("SQLite_DB_Service::CallBack - start");
    const auto now = std::chrono::steady_clock::now();
    const std::string& message_sensor_name = topic;
    const auto& sensor_search_result = _sensors.find(message_sensor_name);
    if (sensor_search_result == _sensors.cend()) {
        _logger->error("Sensor name {} not found in map!!?", message_sensor_name); // well this should never happen so log it as error
        return;
    }
    auto&& mapped_sensor_data = sensor_search_result->second;
    if (mapped_sensor_data.interval) {
        auto since_last_sensor_update = std::chrono::duration_cast<std::chrono::nanoseconds>(now - mapped_sensor_data.last_update);
        if (static_cast<uint64_t>(since_last_sensor_update.count()) < mapped_sensor_data.interval)
            return;
    }
    const auto sensor_req_stmt = _statements[SELECT_id_sensor_index];
    const auto sensor_insert_stmt = _statements[INSERT_sensor_index];
    const auto search_sensor_name_id_result = get_name_id(_known_sensors, message_sensor_name, sensor_insert_stmt, sensor_req_stmt);
    if (search_sensor_name_id_result == _known_sensors.cend())
        return;
    const auto& sensor_name_database_id = search_sensor_name_id_result->second;
    struct json_object* message_json_root_object = json_tokener_parse_ex(_tokener, message.c_str(), message.size());
    if (json_object_get_type(message_json_root_object) != json_type_object) {
        _logger->warn("Did not receive object as initial JSON type - bad (unexpected) JSON format: {}", message);
        return;
    }
    json_object_object_foreach(message_json_root_object, message_cstr_value_name, message_json_value_data) {
        const std::string message_value_name = message_cstr_value_name;
        const auto search_value_result = mapped_sensor_data.values.find(message_value_name);
        if (search_value_result == mapped_sensor_data.values.cend())
            continue;
        auto& current_value_data = search_value_result->second;
        const auto ns_since_first_value_update = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - current_value_data.last_update).count());
        _logger->trace("Sensor: {} Value: {} since last measure: {} ns interval: {} ns", message_sensor_name, message_value_name,  ns_since_first_value_update, current_value_data.interval);
        if (!current_value_data.averaging && current_value_data.interval && ns_since_first_value_update < current_value_data.interval)
            continue;
        const std::string unit_name = [&message_json_value_data]() {
            std::string result;
            if (json_object_get_type(message_json_value_data) == json_type_array) {
                result = json_object_get_string(json_object_array_get_idx (message_json_value_data, 1));   //uints are second element in array
                message_json_value_data = json_object_array_get_idx (message_json_value_data, 0);          //and first is the value
            }
            return result;
        }();
        const auto unit_req_stmt = _statements[SELECT_id_unit_index];
        const auto unit_insert_stmt = _statements[INSERT_unit_index];
        const auto search_unit_name_id_result = get_name_id(_known_units, unit_name, unit_insert_stmt, unit_req_stmt);
        if (search_unit_name_id_result == _known_units.cend())
            return;
        const auto& unit_name_database_id = search_unit_name_id_result->second;
        const auto name_req_stmt = _statements[SELECT_id_valname_index];
        const auto name_insert_stmt = _statements[INSERT_valname_index];
        const auto search_name_name_id_result = get_name_id(_known_names, message_value_name, name_insert_stmt, name_req_stmt, unit_name_database_id);
        if (search_name_name_id_result == _known_names.cend())
            return;
        const auto& name_name_database_id = search_name_name_id_result->second;
        if (current_value_data.averaging && ns_since_first_value_update < current_value_data.interval) {
            switch (json_object_get_type(message_json_value_data)) {
                case json_type_double:
                case json_type_int:
                    current_value_data.ValEvents.emplace_back(json_object_get_double(message_json_value_data), now);
                    break;
                default:
                    _logger->warn("Averaging set on non int/real type! (fix [disable] it in config!); sensor: {}  value: {}", message_sensor_name, message_value_name);
            }
        } else {
            auto stmt = _statements[INSERT_valreal_index];
            bool statement_value_bound = true;
            switch (json_object_get_type(message_json_value_data)) {
                case json_type_int:
                case json_type_double:
                {
                    double value = json_object_get_double(message_json_value_data);
                    if (current_value_data.averaging && current_value_data.ValEvents.size()) {
                        current_value_data.ValEvents.emplace_back(value, now);
                        _logger->debug("Averaging sensor {} value {} number of events {} ", message_sensor_name, message_value_name, current_value_data.ValEvents.size());
                        double average_value = 0;
                        auto last_event_time_mark = current_value_data.ValEvents.cbegin()->time_mark;
                        const auto total_time_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_event_time_mark).count();
                        auto last_event_value = current_value_data.ValEvents.cbegin()->value;
                        for (auto ValEventIterator = current_value_data.ValEvents.cbegin() + 1; ValEventIterator != current_value_data.ValEvents.cend(); ++ValEventIterator) {
                            const auto event_time_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(ValEventIterator->time_mark - last_event_time_mark).count();
                            average_value += ((last_event_value + ValEventIterator->value)/2) * (static_cast<double>(event_time_elapsed) / static_cast<double>(total_time_elapsed));
                            if (current_value_data.ValEvents.size() < 10)  // we print only small sequences
                                _logger->debug("Averaging sensor {} value {} last_event_value {} elapsed_time {} accumulator {}", message_sensor_name, message_value_name, last_event_value, (static_cast<double>(event_time_elapsed) / static_cast<double>(total_time_elapsed)), average_value);
                            last_event_time_mark = ValEventIterator->time_mark;
                            last_event_value = ValEventIterator->value;
                        }
                        // add current value
                        const auto event_time_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_event_time_mark).count();
                        average_value += ((last_event_value + value)/2) * (static_cast<double>(event_time_elapsed) / static_cast<double>(total_time_elapsed));
                        value = average_value;
                    }
                    _logger->debug("Precision sensor {} name {} last_event_value {} value: {} precision {}", message_sensor_name, message_value_name, current_value_data.last_val, value, current_value_data.precision);
                    if (!std::isnan(current_value_data.last_val) && fabs(current_value_data.last_val - value) < current_value_data.precision) {
                        statement_value_bound = false;
                        _logger->debug("Precision break");
                        break;
                    }
                    current_value_data.last_val = value;
                    current_value_data.ValEvents.clear();
                    current_value_data.ValEvents.emplace_back(value, now);
                    if (SQLITE_OK != sqlite3_bind_double(stmt, 3, value))
                        _logger->error("Sqlite error {}", 16);
                    _logger->debug("Store sensor {} name {} value: {}", message_sensor_name, message_value_name, value);
                    break;
                }
                case json_type_boolean:	// booleans are not supposed to have unit_name/ and averaging does not make sense to me to somehow support
                    stmt = _statements[INSERT_valbool_index];
                    if (SQLITE_OK != sqlite3_bind_int(stmt, 3, json_object_get_boolean(message_json_value_data))) 
                        _logger->error("Sqlite error {}", 19);
                    break;
                default:
                    statement_value_bound = false;
                    _logger->error("Json unexpected type of object for message_value_name: {} payload: {} ", message_value_name, message);
                    break;
            }
            if (statement_value_bound) {
                if (SQLITE_OK != sqlite3_bind_int64(stmt, 1, sensor_name_database_id))
                    _logger->error("Sqlite error {}", 21);
                if (SQLITE_OK != sqlite3_bind_int64(stmt, 2, name_name_database_id))
                    _logger->error("Sqlite error {}", 23);
                if (SQLITE_DONE != sqlite3_step(stmt))
                    _logger->error("Sqlite error on position {} : {} ", 24, sqlite3_errmsg(_pDb));
                sqlite3_clear_bindings(stmt);   // this one may not be necessary
                current_value_data.last_update = now;
                mapped_sensor_data.last_update = now;
                if (SQLITE_OK != sqlite3_reset(stmt))
                    _logger->error("Sqlite error on position {} : {}", 25, sqlite3_errmsg(_pDb));                
            }
        }
    }
    json_object_put(message_json_root_object);  // free message object tree
    _logger->trace("SQLite_DB_Service::CallBack - end");
}

int main() {
    try {
        SQLite_DB_Service d;
        d.main();
    } catch (const std::exception& error) {
        return -1;
    }
    return 0;
}