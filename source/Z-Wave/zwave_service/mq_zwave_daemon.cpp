// Copyright: (c) Jaromir Veber 2017-2019
// Version: 01042019
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************

// MQ_Sysytem
#include "mq_lib.h"
// JSON
#include <json-c/json_object.h>     // JSON format for communication
#include <json-c/json_tokener.h>    // JSON format translation (reading)
#include <json-c/linkhash.h>        // access JSON-C dictionary object - (for cycle json_object_object_foreach)
// Z-Wave
#include "Options.h"                // Zwave options object
#include "Manager.h"                // Zwave manager object
#include "Notification.h"
#include "platform/Log.h"
// config++
#include <libconfig.h++>
// std
#include <thread>           // sleep_for function
#include <stdexcept>        // runtime_error exception definition
#include <atomic>
#include <limits>
#include <list>
#include <unordered_map>
#include <chrono>           // for interval measurement
#include <algorithm>

using namespace MQ_System;
using namespace OpenZWave;
using namespace libconfig;

class ZWLog : public OpenZWave::i_LogImpl {
public:
    ZWLog (const std::shared_ptr<spdlog::logger>& loger,LogLevel log_level = LogLevel::LogLevel_Alert): _level(log_level), _logger(loger) {}
    virtual ~ZWLog() { }
    virtual void Write (LogLevel _logLevel, uint8 const _nodeId, char const *_format, va_list _args) {
        if (_logLevel > _level) return;
        char lineBuf[1024] = {};
        if ( _format && _format[0] != '\0') {
            va_list saveargs;
            va_copy(saveargs, _args);
            vsnprintf(lineBuf, sizeof(lineBuf), _format, _args);
            va_end( saveargs );
            _logger->info("ZWLog({}):{}: {}", LogLevelString[_logLevel], _nodeId, lineBuf);
        }
    }
    virtual void QueueDump () {}
    virtual void QueueClear () {} 
    virtual void SetLoggingState (LogLevel _saveLevel, LogLevel, LogLevel) { _level = _saveLevel; }
    virtual void SetLogFileName (const string& ) {}
private:
    LogLevel _level;
    const std::shared_ptr<spdlog::logger> _logger;
};

class Zwave_Service : public Daemon {
 public:
    Zwave_Service();
    void main();
    void on_notification(Notification const *pNotification);
    virtual void CallBack(const std::string& topic, const std::string& message) override;
    virtual ~Zwave_Service() noexcept;
 private:
    struct json_object* ozw_value_to_json_object(const ValueID &value) const;
    void load_daemon_configuration(std::string& zw_driver_path, std::string& zw_config_path, std::string& zw_network_data_path);
    void peprare_data_structures();
    void main_loop();

    OpenZWave::Manager * _manager;
    std::chrono::time_point<std::chrono::steady_clock> _last_refresh;  // we use this value to measure 24hour cycle to call heal network and write config
    std::atomic<uint32_t> _home_id;
    std::atomic<bool> _initialized;
    std::string _zw_driver_path;
    struct ValueData {
        ValueData(uint64_t rv, const std::string& sn, bool r, bool w, uint32_t r_limit, uint32_t ref) :
             sensor_name(sn), read(r), write(w), raw_val(rv), refresh_limit(r_limit), refresh(ref) {}
        const std::string sensor_name;
        std::chrono::time_point<std::chrono::steady_clock> last_refresh;
        std::string label;
        std::string units;
        bool read;
        bool write;
        union {
            ValueID val;
            uint64_t raw_val;
        };
        const uint32_t refresh_limit;  // min limit for value report 
        // well this value makes it kind of complicated but the Z-Wave device may not report every change at least my device does not. Sometimes they report change but sometimes 
        // they do after a long-long time and sometimes they just don't. Fortunatelly Z-wave is able to force value refresh. Forced value refresh wakes up the sensor and ask it 
        // for specified value. This value controlls how often this may happen (in seconds!). Ofc once the device reports value by it's own it is counted as value report (refresh). 
        // Also take care about the refresh value. If you refresh it too often it might deplete sensor battery, and overload the Z-Wave network.
        const uint32_t refresh;
    };

    struct SensorData {
        SensorData(const std::string &n): name(n) {}
        const std::string name;
        std::list<ValueData> values;
    };

    std::list <SensorData> _sensors;  // real data storage for values related to sensors
    std::unordered_map <uint64_t, decltype(_sensors.begin()->values.begin())> _sensor_id_map;  // for effective search using value_id (Z-Wave events)
    std::unordered_map <std::string, decltype(_sensors.begin())> _sensor_name_map;  // for effective search using name (broker events)
    struct json_tokener* _tokener;
};

Zwave_Service::Zwave_Service(): Daemon("mq_zwave_daemon", "/var/run/mq_zwave_daemon.pid"), _home_id(std::numeric_limits<uint32_t>::max()), _initialized(false), _tokener(json_tokener_new()), _manager(nullptr) {}

void Zwave_Service::load_daemon_configuration(std::string& zw_driver_path, std::string& zw_config_path, std::string& zw_network_data_path) {
    static const char* kConfig_file = "/etc/mq_system/mq_zwave_daemon.conf";
    static const char* kDefaultDriverPath = "/dev/ttyACM0";
    static const char* kDefaultConfigPath = "/usr/config/";
    static const char* kDefaultLogFile = "/var/log/mq_system/zwave.log";
    static const char* kDefaultNetworkDataPath = "/etc/mq_system/";
    Config cfg;
    //spdlog::set_pattern("[%x %H:%M:%S.%e][%n][%t][%l] %v");

    try {
        cfg.readFile(kConfig_file);
    } catch(const FileIOException& fioex) {
        _logger->error("I/O error while reading system configuration file: {}", kConfig_file);
        throw std::runtime_error("");
    } catch(const ParseException& pex) {
        _logger->error("Parse error at {} : {} - {}", pex.getFile(), pex.getLine(), pex.getError());
        throw std::runtime_error("");
    }
    try {
        if (cfg.exists("log_level")) {
            int level;
            cfg.lookupValue("log_level", level);
            _logger->set_level(static_cast<spdlog::level::level_enum>(level));
        }
        if (!cfg.exists("driver_path"))
            zw_driver_path = kDefaultDriverPath;
        else
            cfg.lookupValue("driver_path", zw_driver_path);
        if (!cfg.exists("config_path"))
            zw_config_path = kDefaultConfigPath;
        else
            cfg.lookupValue("config_path", zw_config_path);
        if (!cfg.exists("network_data_path"))
            zw_network_data_path = kDefaultNetworkDataPath;
        else
            cfg.lookupValue("network_data_path", zw_network_data_path);
        Setting& sensors = cfg.lookup("sensors");
        for (const auto& sensor : sensors) {
            const std::string sensor_name = sensor.lookup("name");
            decltype(_sensors)::value_type inner(sensor_name);
            for (const auto& value : sensor.lookup("values")) {
                const unsigned long long value_id = value.lookup("value_id");
                bool value_read = false;
                if (value.exists("status"))
                     value.lookupValue("status", value_read);
                bool value_write = false;
                if (value.exists("set"))
                     value.lookupValue("set", value_write);
                uint32_t value_refresh_limit = 0;
                if (value.exists("refresh_limit"))
                     value.lookupValue("refresh_limit", value_refresh_limit);
                uint32_t value_refresh = 0;
                if (value.exists("refresh"))
                    value.lookupValue("refresh", value_refresh);
                if (value_refresh && value_refresh < value_refresh_limit)
                    value_refresh = value_refresh_limit;
                inner.values.emplace_back(value_id, inner.name, value_read, value_write, value_refresh_limit, value_refresh);
            }
            _sensors.emplace_back(inner);
        }
    } catch(const SettingNotFoundException& nfex) {
        _logger->error("Required setting not found in system configuration file");
        throw std::runtime_error("");
    } catch (const SettingTypeException& nfex) {
        _logger->error("Seting type error (at system configuaration) at: {}", nfex.getPath());
        throw std::runtime_error("");
    }
}

void Zwave_Service::CallBack(const std::string& topic, const std::string& message){
    _logger->trace("callback notification {}", topic);
    const auto found_sensor_iterator = _sensor_name_map.find(topic);
    // ignore all messages not related to listed sensors
    if (found_sensor_iterator == _sensor_name_map.cend())
        return;
    auto json_root_object = json_tokener_parse_ex(_tokener, message.c_str(), message.size());
    if (json_object_get_type(json_root_object) != json_type_object) {
        _logger->warn("Did not receive object as initial json type - bad json format: {}", message);
        json_object_put(json_root_object);
        return;
    }
    json_object_object_foreach(json_root_object, current_key, current_object) {
        const auto& found_sensor_data = found_sensor_iterator->second;
        auto current_value_iterator = std::find_if(found_sensor_data->values.cbegin(), found_sensor_data->values.cend(), [current_key](const ValueData& value){ return value.label == current_key; });
        if (current_value_iterator == found_sensor_data->values.cend()) {
            _logger->debug("Value {} not found (registered) on sensor {} - so it was not written", current_key, topic);
            continue;
        }
        auto current_object_type = json_object_get_type(current_object);
        if (current_object_type == json_type_array) {  // it may be an array - [value, unit]; we don't care about unit much here; so extract only the value
            current_object = json_object_array_get_idx(current_object, 0);
            if (!current_object)
                continue;
            current_object_type = json_object_get_type(current_object);
        }
        try {
            switch (current_object_type) {
                case json_type_double:
                    _logger->trace("SetValue {} double {}", topic, json_object_get_double(current_object));
                    _manager->SetValue(current_value_iterator->val, static_cast<float>(json_object_get_double(current_object)));  // what happen if ZW is not expecting double?? TODO
                    break;
                case json_type_int: {  // well it is much more complicated for integral value types
                    auto value = json_object_get_int(current_object);
                    switch (current_value_iterator->val.GetType()) {
                        case ValueID::ValueType::ValueType_Byte:
                            _logger->trace("SetValue {} Byte {}", topic, static_cast<uint8_t>(value));
                            _manager->SetValue(current_value_iterator->val, static_cast<uint8_t>(value));
                            break;
                        case ValueID::ValueType::ValueType_Int:
                            _logger->trace("SetValue Int {}", static_cast<int32_t>(value));
                            _manager->SetValue(current_value_iterator->val,static_cast<int32_t>(value));
                            break;
                        default:
                            _logger->trace("SetValue Short {}", static_cast<int16_t>(value));
                            _manager->SetValue(current_value_iterator->val, static_cast<int16_t>(value));
                    }
                    break;
                }
                case json_type_boolean: {
                    auto value = json_object_get_boolean(current_object);
                    switch (current_value_iterator->val.GetType()) {
                        case ValueID::ValueType::ValueType_Button:
                            // well it is a bit complicated with buttons so we handle it like this. - write true to press it and false to release it
                            if (value)
                                _manager->PressButton(current_value_iterator->val);
                            else
                                _manager->ReleaseButton(current_value_iterator->val);
                            break;
                        default:
                            _manager->SetValue(current_value_iterator->val, static_cast<bool>(value));
                    }
                    break;
                }
                case json_type_string:
                    _manager->SetValue(current_value_iterator->val,std::string(json_object_get_string(current_object)));
                    break;
                default:
                    _logger->warn("Unhanded {} JSON type - TODO?", current_object_type);
            }
        } catch (OZWException& e) {
            _logger->warn("Exception caught while setting ZW value: {}", e.GetMsg());
        }
    }
    json_object_put(json_root_object); 
}

static void OnNotification(Notification const *pNotification, void *context) {
    reinterpret_cast<Zwave_Service*>(context)->on_notification(pNotification);
}

void Zwave_Service::peprare_data_structures() {
    _logger->trace("Setup value id's");
    auto requested_time = std::chrono::steady_clock::now();
    std::list<std::pair<unsigned char, decltype(requested_time)>> status_map;
    // init value IDs of sensor_values and prepare initialization map (status_map)
    for (auto&& sensor : _sensors)
        for (auto&& sensor_value : sensor.values) {
            sensor_value.val = ValueID(_home_id, (uint64) sensor_value.raw_val);
            status_map.emplace_back(sensor_value.val.GetNodeId(), requested_time);
        }
    _logger->trace("Wait until all sensors are ready");
    while (!status_map.empty()) { // this may take up to 10 minutes (in case of uninitialized Z-Wave network)!
        auto now = std::chrono::steady_clock::now();
        for (auto status_map_iterator = status_map.cbegin(); status_map_iterator != status_map.cend(); ) {
            auto seconds_since_last_check = (now - status_map_iterator->second).count() / 1000000000.0;
            _logger->debug("Node status {} {}", status_map_iterator->first, seconds_since_last_check);
            if (_manager->IsNodeInfoReceived(_home_id, status_map_iterator->first)) {
                status_map_iterator = status_map.erase(status_map_iterator);
                _logger->debug("Node info recieved after {} seconds", seconds_since_last_check);
            } else {
                if (seconds_since_last_check >= 600.0) {  // we wait max 10 mins
                    if (_manager->IsNodeFailed(_home_id, status_map_iterator->first))
                        _logger->warn("Node {} is failed removing it (it wont't work - fix it (in mangement program?) and restart daemon)!", status_map_iterator->first);
                    else if (!_manager->IsNodeAwake(_home_id, status_map_iterator->first))
                        _logger->warn("Node {} is sleeping removing it (it wont't work - pls wake it manually and restart daemon)!", status_map_iterator->first);
                    else
                        _logger->error("Node {} info not received yet after {} second (unexpected behavior) node is not sleeping and not failed (probably non-existant valueid)", status_map_iterator->first, seconds_since_last_check);
                    // cleanup values for nodes without Nodeinfo
                    for (auto sensor_iterator = _sensors.begin(); sensor_iterator != _sensors.end(); ) {
                        //std::remove_if(sensor_iterator->values.begin(), sensor_iterator->values.end(),
                        //        [status_map_iterator](const ValueData& element){ return element.val.GetNodeId() == status_map_iterator->first; });    // this one shows the intent but requires assignment operator that is not declared for ValueData...
                        for (auto value_iterator = sensor_iterator->values.begin(); value_iterator != sensor_iterator->values.end(); ) {
                            if (value_iterator->val.GetNodeId() == status_map_iterator->first)
                                value_iterator = sensor_iterator->values.erase(value_iterator);
                            else
                                ++value_iterator;
                        }
                        if (sensor_iterator->values.empty())
                            sensor_iterator = _sensors.erase(sensor_iterator);
                        else
                            ++sensor_iterator;
                    }
                    status_map_iterator = status_map.erase(status_map_iterator);
                } else
                    ++status_map_iterator;
            }
        }
        if (!status_map.empty())
            std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    _logger->trace("ZW Network ready"); // now the Z-Wave is ready to provide node information (sensor value information) so we parse it.
    for (auto sensor_iterator = _sensors.begin(); sensor_iterator != _sensors.end(); ++sensor_iterator) 
        for (auto value_iterator = sensor_iterator->values.begin(); value_iterator != sensor_iterator->values.end(); ++value_iterator) {
            try {
                value_iterator->label = _manager->GetValueLabel(value_iterator->val);  // this may all throw OZWEXCEPTION_INVALID_VALUEID
                value_iterator->units = _manager->GetValueUnits(value_iterator->val);
                const bool value_readable = !_manager->IsValueWriteOnly(value_iterator->val);
                const bool value_writeable = !_manager->IsValueReadOnly(value_iterator->val);
                if (!value_readable && value_iterator->read) {
                    _logger->warn ("Sensor {} value {} is marked write only by ZWave network - setting read (from configuration) to false", sensor_iterator->name, value_iterator->val.GetId());
                    value_iterator->read = false;
                }
                // this map is used for sensor reading facility
                if (value_iterator->read)
                    _sensor_id_map.emplace(value_iterator->val.GetId(), value_iterator);
                if (!value_writeable && value_iterator->write) {
                    _logger->warn ("Sensor {} value {} is marked read only - setting write to false", sensor_iterator->name, value_iterator->val.GetId());
                    value_iterator->write = false;
                }
                // this map is used for sensor writing facility
                if (value_iterator->write) {
                    const std::string write_name = std::string("set/") + sensor_iterator->name;
                    const auto& result = _sensor_name_map.emplace(write_name, sensor_iterator);
                    if (result.second)
                        Subscribe(write_name);
                }
            } catch (OpenZWave::OZWException& oze) {
                _logger->error("Sensor {} OZWException: {} - deamon may not work well for this sensor", sensor_iterator->name, oze.GetMsg());
            }
        }
}

void Zwave_Service::main_loop() {
    while ( true ) {
        float next_refresh = 600.0f;
        auto now = std::chrono::steady_clock::now();
        for (const auto& sensor : _sensors)
            for (auto&& sensor_value : sensor.values) {
                if (!sensor_value.refresh)
                    continue;
                if (_manager->IsNodeFailed(_home_id, sensor_value.val.GetNodeId()))	 // ignore failed nodes
                    continue;
                auto sec_since_last_refresh = static_cast<double>((now - sensor_value.last_refresh).count()) / 1000000000.0;
                if (sec_since_last_refresh > sensor_value.refresh) {
                    _manager->RefreshValue(sensor_value.val);
                    sec_since_last_refresh = 0.0;
                    _logger->debug("Normal Refresh value on node {} since last refresh {}", sensor_value.val.GetNodeId(), sec_since_last_refresh);
                }
                if (sec_since_last_refresh > sensor_value.refresh) {
                    next_refresh = 2.1f;
                    _logger->debug("Node {} Req Refresh 0 {}; Ref {}; Since Last {} ", sensor_value.val.GetNodeId(), next_refresh, sensor_value.refresh, sec_since_last_refresh);
                } else if ((sensor_value.refresh - sec_since_last_refresh) < next_refresh) {
                    next_refresh = sensor_value.refresh - sec_since_last_refresh;
                    _logger->debug("Node {} Req Refresh 1 {}; Ref {}; Since Last {} ", sensor_value.val.GetNodeId(), next_refresh, sensor_value.refresh, sec_since_last_refresh);
                } else {
                    _logger->debug("Node {} Refresh Next {}; Ref {}; Since Last {} ", sensor_value.val.GetNodeId(), (sensor_value.refresh - sec_since_last_refresh), sensor_value.refresh, sec_since_last_refresh);
                }
            }
        if (_last_refresh - now > std::chrono::hours(24)) {
            _last_refresh = now;
            _manager->HealNetwork(_home_id , true);
        }
        if (next_refresh < 2.1f)
            next_refresh = 2.1f;
        _logger->debug("Final Refresh {}", next_refresh);
        std::this_thread::sleep_for(std::chrono::duration<float>(next_refresh));
    }
}

void Zwave_Service::main() {
    std::string zw_config_path, zw_network_data_path;
    load_daemon_configuration(_zw_driver_path, zw_config_path, zw_network_data_path);
    _logger->info("Entered main");
    _last_refresh = std::chrono::steady_clock::now();
    try {
        auto zw_log = new ZWLog(_logger);  // setup custom logging interface - to see the messages also in MQ_System logging interface
        zw_log->SetLoggingState(LogLevel_Alert, LogLevel_Alert, LogLevel_Alert);
        Log::SetLoggingClass(zw_log);
        _logger->trace("Options Create");
        Options::Create(zw_config_path, zw_network_data_path, "")->Lock();
        _logger->trace("Manager Create");
        _manager = Manager::Create();
        if (!_manager) {
            _logger->critical("Manager Create failed!");
            return;
        }
        _manager->AddWatcher(OnNotification, this);
        _logger->trace("Add Driver");
        if (!_manager->AddDriver(_zw_driver_path))
            _logger->warn("Driver add error.. driver already exists {}", _zw_driver_path);
        _logger->trace("ZWave initialization");
        for (int counter = 0; _home_id == std::numeric_limits<uint32_t>::max() && counter < 20; ++counter) {
            _logger->trace("Wait for home_id");
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        _logger->trace("Got Home ID!");
        if (_home_id != std::numeric_limits<uint32_t>::max()) {
            _logger->debug("Prep Data structures");
            peprare_data_structures();
            _initialized = true;
            _logger->debug("Main loop");
            main_loop();
        }
    } catch (OZWException& oze) {
        _logger->error("OpenZWave exception {}", oze.GetMsg());
    }
}

void Zwave_Service::on_notification(Notification const *pNotification) {
    try {
        switch (pNotification->GetType()) {
            case Notification::NotificationType::Type_DriverReady:
                _home_id = pNotification->GetHomeId();
                break;
            case Notification::NotificationType::Type_ButtonOn:
            case Notification::NotificationType::Type_ButtonOff:
            case Notification::NotificationType::Type_ValueChanged:
            case Notification::NotificationType::Type_ValueRefreshed:
            {
                if (!_initialized) {
                    _logger->trace ("Value event - system not initialized break");
                    break;
                }
                const auto value = pNotification->GetValueID();                
                const auto sensor_search_result = _sensor_id_map.find(value.GetId());
                if (sensor_search_result == _sensor_id_map.end())
                    break;
                auto found_value_data = sensor_search_result->second;
                const auto now = std::chrono::steady_clock::now();
                const std::chrono::duration<float> diff = now - found_value_data->last_refresh;
                if (static_cast<uint32_t>(diff.count()) < found_value_data->refresh_limit){
                    _logger->trace ("Value event - refresh limit break");
                    break;
                }
                found_value_data->last_refresh = now;
                auto json_value_object = ozw_value_to_json_object(value);
                if (!json_value_object) {
                    _logger->warn ("Unable to convert ZW value to JSON value");
                    break;
                }
                auto temp = [&]() { 
                    if (found_value_data->units.empty())
                        return json_value_object;
                    auto arr = json_object_new_array();
                    json_object_array_add(arr, json_value_object);
                    json_object_array_add(arr, json_object_new_string(found_value_data->units.c_str()));
                    return arr;
                }();
                auto j_object = json_object_new_object();
                json_object_object_add(j_object, found_value_data->label.c_str(), temp);
                const char* json_string = json_object_to_json_string_ext(j_object, JSON_C_TO_STRING_PLAIN);
                if (!json_string) {
                    _logger->warn ("Value event - unable to covert JSON to string");
                    json_object_put(j_object);
                    break;
                }
                Publish(std::string("status/") + found_value_data->sensor_name, std::string(json_string));
                json_object_put(j_object);
                break;
            }
            case Notification::NotificationType::Type_NodeEvent:
            {
                _logger->info("Node event");
                break;
            }
            case Notification::NotificationType::Type_SceneEvent:
            {
                _logger->info("Scene event ");
                break;
            }
            case Notification::NotificationType::Type_AllNodesQueriedSomeDead:
            case Notification::NotificationType::Type_AllNodesQueried:
            {
                _logger->info("All nodes have been queried");
                break;
            }
            case Notification::NotificationType::Type_NodeQueriesComplete:
            {
                _logger->info("All the initialization queries on a node have been completed: {}", pNotification->GetNodeId());
                break;
            }
            default:
                //_logger->trace ("Unhandled event {}", pNotification->GetType());
                break;
        }
    } catch (OpenZWave::OZWException& oze) { 
            _logger->warn ("OZWException {} {} {} {}", oze.GetFile(), oze.GetLine(), oze.GetMsg(), oze.GetType());
    }
}

struct json_object* Zwave_Service::ozw_value_to_json_object(const ValueID &value) const {
    switch (value.GetType()) {
        case ValueID::ValueType::ValueType_Decimal: {
            float data = 0.0;
            _manager->GetValueAsFloat(value, &data);
            return json_object_new_double(data);
        }
        case ValueID::ValueType::ValueType_Byte: {
            uint8_t data = 0;
            _manager->GetValueAsByte(value, &data);
            return json_object_new_int(static_cast<int32_t>(data));
        }
        case ValueID::ValueType::ValueType_Short: {
            int16_t data = 0;
            _manager->GetValueAsShort(value, &data);
            return json_object_new_int(static_cast<int32_t>(data));
        }
        case ValueID::ValueType::ValueType_Int: {
            int32_t data = 0;
            _manager->GetValueAsInt(value, &data);
            return json_object_new_int(data);
        }
        case ValueID::ValueType::ValueType_Bool:
        case ValueID::ValueType::ValueType_Button: {
            bool data = false;
            _manager->GetValueAsBool(value, &data);
            return json_object_new_boolean(data); 
        }
        case ValueID::ValueType::ValueType_String: {
            std::string data;
            _manager->GetValueAsString(value, &data);
            return json_object_new_string(data.c_str());
        }
        case ValueID::ValueType::ValueType_List:
        case ValueID::ValueType::ValueType_Schedule:
        case ValueID::ValueType::ValueType_Raw:
        default:
            _logger->warn("Value type {} not handled - TODO devel", value.GetType());
            return nullptr;
    }
}

Zwave_Service::~Zwave_Service() noexcept {
    _manager->RemoveWatcher(OnNotification, this);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    Unsubscribe("#");  //unsubscribe all - makes it safe because we destroy lot of objects that might by used by concurent thread.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    _manager->RemoveDriver(_zw_driver_path);
    std::this_thread::sleep_for(std::chrono::seconds(2));  // this may be removed
    Manager::Destroy();
    Options::Destroy();
    json_tokener_free(_tokener);
}

int main() {
    try {
        Zwave_Service d;
        d.main();
    } catch (const std::runtime_error& error) {
        return -1;
    }
    return 0;
}
