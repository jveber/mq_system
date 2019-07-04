// Copyright: (c) Jaromir Veber 2018-2019
// Version: 20032019
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************
// File containing LUA bindings to exe module
// to make it all working well lua should be built using C++ Othrewise there's risk of leaking some resources in the case of bugged scripts (we need support for exceptions).

#include <regex>
#include <chrono>
#include <ctime>
#include <stdexcept>

#include "mq_exe_daemon.h"

// here defined functions
int lua_debug_function(lua_State *l);
int lua_warn_function(lua_State *l);
int lua_register_sensor(lua_State *l);
int lua_wait_and(lua_State * l);
int lua_wait_or(lua_State * l);
int lua_req_value(lua_State * l);
int lua_write_value(lua_State * l);
int lua_report_value(lua_State * l);
int lua_set_global(lua_State *l);
int lua_get_global(lua_State *l);
int lua_string_destructor(lua_State * l);

// part of lua os-lib (mq_loslib.c)
int os_difftime (lua_State *L);
int os_time (lua_State *L);
int os_date (lua_State *L);
int os_clock (lua_State *L);

void Exe_Service::thread_start(lua_State *lua_state, const std::string& script_name) {
    _logger->trace("Script {} started", script_name);
    auto status = lua_pcall(lua_state, 0, LUA_MULTRET, 0);
    if (status != LUA_OK)
        _logger->info("Script {} terminated with error {}", script_name, lua_tostring(lua_state, -1));
    else
        _logger->info("Script {} sccessfully ended", script_name);
    lua_close(lua_state);
}

void Exe_Service::execute_lua_script(const std::string& script_name, const std::string& script_content) {
    static const luaL_Reg mq_procedures[] = {
        { "debug", lua_debug_function },
        { "warn", lua_warn_function },
        { "register_value", lua_register_sensor },
        { "wait_and", lua_wait_and },
        { "wait_or", lua_wait_or },
        { "request_value", lua_req_value },
        { "write_value", lua_write_value },
        { "report_value", lua_report_value },
        { "set_global", lua_set_global},
        { "get_global", lua_get_global},
        { "clock", os_clock },
        { "date", os_date },
        { "difftime", os_difftime },
        { "time", os_time }
    };

    luaL_Reg sStrRegs[] =
    {
        { "__gc", lua_string_destructor },
        {NULL, NULL}
    };

    auto lua_state = luaL_newstate();
    lua_pushlightuserdata(lua_state, this);
    lua_setglobal(lua_state, "___exe_object___");   // this one is used to pass the object to library functions
    for (auto const& procedure : mq_procedures)     // register all functions - use rather setfuncs?
        lua_register(lua_state, procedure.name, procedure.func);
    luaL_newmetatable(lua_state, "std_string");
    luaL_setfuncs(lua_state, sStrRegs, 0);
    auto lua_load_result = luaL_loadbuffer(lua_state, script_content.c_str(), script_content.length(), script_name.c_str());
    switch (lua_load_result) {
        case LUA_OK:
            _script_threads.emplace_back([this, lua_state, script_name]{thread_start(lua_state, script_name);});
            return;
        case LUA_ERRSYNTAX: 
            _logger->warn("Syntax error while loading LUA script {} : {}", script_name, lua_tostring(lua_state, -1));
            break;
        case LUA_ERRMEM:
            _logger->warn("Memory allocation error while loading LUA script {} : {}", script_name, lua_tostring(lua_state, -1));
            break;
        case LUA_ERRGCMM:
            _logger->warn("GC error while loading LUA script {} : {}", script_name, lua_tostring(lua_state, -1));
            break;
        default:
            _logger->warn("Unexpected error while loading LUA script {} : {}", script_name, lua_tostring(lua_state, -1));
            break;
    }
    lua_close(lua_state);
}

// ************** Lua library ********************************//
Exe_Service* get_exe_object(lua_State *l) {
    int exe_type = lua_getglobal(l, "___exe_object___");
    if (exe_type != LUA_TLIGHTUSERDATA)
        luaL_error(l, "LUA - Exe object bad type! (internal error) - might result in daemon crash");
    Exe_Service* exe_object = reinterpret_cast<Exe_Service*>(lua_touserdata(l, -1));
    lua_remove(l, -1);
    return exe_object;
}

int lua_debug_function(lua_State *l) {
    return get_exe_object(l)->print(l, false);
}

int lua_warn_function(lua_State *l) {
    return get_exe_object(l)->print(l, true);
}

int Exe_Service::print(lua_State *l, bool warning) {
    int argc = lua_gettop(l);
    std::string message;
    for (int i = 1; i <= argc; ++i) {
        auto str = lua_tostring(l, i);
        if (str != nullptr)
            message += str;
        else
            message += " NIL ";
    }
    if (warning)
        _logger->warn("[LUA] {}", message);
    else
        _logger->debug("[LUA] {}", message);
    return 0;
}

// tell exe module this sensor is interesting and also get reference to that sensor.
int lua_register_sensor(lua_State *l) {
    return get_exe_object(l)->register_sensor(l);
}

int lua_string_destructor(lua_State * l)
{
    if (lua_isuserdata(l, 1)) {
        std::string* str = reinterpret_cast<std::string*>(lua_touserdata(l,1));
        str->std::string::~string();  //lua object is consturcted by pacemant constructor so we need to call only destructor!
    } else {
        luaL_error(l, "string destructor type error");
    }
    return 0;
}


int Exe_Service::register_sensor(lua_State *l) {
    const std::regex txt_regex("(\\w|\\/)+\\:\\w+");
    int values_on_stack = 0;    
    int argc = lua_gettop(l);
    _logger->trace("[LUA] register_sensor enter arguments {}", argc);
    for (int i = 1; i <= argc; ++i) {
        if (lua_isstring(l, i)) {
            std::string sensor_value_text(lua_tostring(l, i));
            if (std::regex_match(sensor_value_text, txt_regex)) {
                void* data = lua_newuserdata(l, sizeof(std::string));                
                new(data) std::string(std::move(std::string("status/") + sensor_value_text));
                luaL_setmetatable(l, "std_string");   // add destructor for GC
                ++values_on_stack;
                _logger->debug("[LUA] sensor registered {}", sensor_value_text);
            } else
                luaL_error(l, "register_sensor: sensor name not in expected format %s", sensor_value_text);
        } else
            luaL_error (l, "register_sensor: unexpected input type %d (expecting string)", lua_type(l, i));
    }
    return values_on_stack;
}

int lua_wait_and(lua_State * l) {
    return get_exe_object(l)->wait(l, false);
}

int lua_wait_or(lua_State * l) {
    return get_exe_object(l)->wait(l, true);
}

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    if (std::string::npos == first)
        return str;
    return str.substr(first, str.find_last_not_of(' ') - first + 1);
}

int month_days(const int in_year, int month) {
    switch (month) {
        case 1: {  // resolve february problem
            int year = in_year + 1900;
            if (( year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
                return 29;
            else
                return 28;
        }
        case 0:
        case 2:
        case 4:
        case 6:
        case 7:
        case 9:
        case 11:
            return 31;
        default:
            return 30;
    }
}

long long string_value(const std::string& input_string, std::string& output_string) {
    auto string_value = trim(input_string);
    size_t pos = 0;
    long long value = std::stoll(string_value, &pos);
    output_string = std::move(trim(string_value.substr(pos)));
    return value;
}

int mod (int a, int b)
{
   int ret = a % b;
   if(ret < 0)
     ret+=b;
   return ret;
}

bool value_since_now(long long value, long long modulo, int16_t current, int_fast8_t& result) {
    result = value - current;
    if (result < 0) {
        result = mod(result, modulo);
        return true;  // underflow
    }
    result = mod(result, modulo);
    return false;
}

int_fast16_t parse_month(struct tm* tm, int_fast8_t req_monthday) {
    int8_t additional_days = 0;
    req_monthday = (req_monthday > 0) ? req_monthday : req_monthday + 1;  // days are [1-31] but negative numbers have differrent meaning
    int8_t current_day = tm->tm_mday;
    int8_t requested_day = (req_monthday > 0) ? req_monthday : month_days(tm->tm_year, tm->tm_mon) + req_monthday;
    if (requested_day < 0 || requested_day > month_days(tm->tm_year, tm->tm_mon)) {  // current month does not have enough days
        // we move one month forward - that one should definitely have enuff days because every second month have 31 days
        additional_days = month_days(tm->tm_year, tm->tm_mon) - current_day + 1;
        if (++tm->tm_mon > 11) {
            tm->tm_mon = 0;
            ++tm->tm_year;
        }
        if (requested_day < 0)
            requested_day = month_days(tm->tm_year, tm->tm_mon) + req_monthday;
        current_day = 1;
    }
    int_fast16_t days_since_now = requested_day - current_day;
    if (days_since_now < 0)
        days_since_now = days_since_now + month_days(tm->tm_year, tm->tm_mon);
    days_since_now += additional_days;
    return days_since_now;
}

std::chrono::system_clock::time_point Exe_Service::parse_time_string(const std::string& time_string) {
    std::regex regex_now("NOW [0-9]+ (second|minute|hour|day|week|month)");
    std::regex regex_every("EVERY(( MONTHDAY -?[0-9]+)|( WEEKDAY [0-6]))?( DAYHOUR [0-9]+)?( HOURMINUTE [0-9]+)?( MINUTESECOND [0-9]+)?");

    if (!std::regex_match(time_string, regex_now) && !std::regex_match(time_string, regex_every))
        throw std::runtime_error("string does not match expected format!");
    if (time_string.substr(0,3) == "NOW") {
        auto rel = trim(time_string.substr(3));
        size_t numterm = 0;
        auto num = std::stoll(rel, &numterm);
        if (!num)
            throw std::runtime_error("NOW number of metric not defined or 0");
        auto metric = trim(rel.substr(numterm));
        if (metric == "second") {
             return std::chrono::system_clock::now() + std::chrono::seconds(num);
        } else if (metric == "minute") {
             return std::chrono::system_clock::now() + std::chrono::minutes(num);
        } else if (metric == "hour") {
             return std::chrono::system_clock::now() + std::chrono::hours(num);
        } else if (metric == "day") {
             return std::chrono::system_clock::now() + std::chrono::hours(24*num);
        } else if (metric == "week") {
             return std::chrono::system_clock::now() + std::chrono::hours(7*24*num);
        } else if (metric == "month") {
             return std::chrono::system_clock::now() + std::chrono::hours(30*24*num);
        } else {
             throw std::runtime_error(std::string("unexpected metric type: ") +  metric);
        }
    } else if (time_string.substr(0,5) == "EVERY") {
        auto rel = trim(time_string.substr(5));
        time_t tt;
        ::time(&tt);
        struct tm tm;
        ::localtime_r(&tt, &tm);  // this one should be thread-safe so we use this one as we can't guarantee theread safety without locks..
        _logger->debug("Date now {}.{}.{} {}:{}:{}", tm.tm_mday, tm.tm_mon, 1900 + tm.tm_year, tm.tm_hour, tm.tm_min, tm.tm_sec);
        long long req_monthday = 0, req_weekday = -1, req_dayhour = -1, req_hourminute = -1, req_minutesecond = -1;
        if (rel.substr(0,8) == "MONTHDAY") {
            auto string_value = trim(rel.substr(8));
            size_t pos = 0;
            req_monthday = std::stoll(string_value, &pos);
            rel = trim(string_value.substr(pos));  // update rel
            if (!req_monthday || req_monthday > 31 || req_monthday < -31)
                throw std::runtime_error(std::string("MONTHDAY must be in range [1...31] or [-1...-30] & it is:") + std::to_string(req_monthday));
        } else if (rel.substr(0,7) == "WEEKDAY") {
            _logger->debug("WEEKDAY");
            req_weekday = string_value(rel.substr(7), rel);
        }
        if (rel.substr(0,7) == "DAYHOUR") {
            req_dayhour = string_value(rel.substr(7), rel);
        }
        if (rel.substr(0,10) == "HOURMINUTE") {
            req_hourminute = string_value(rel.substr(10), rel);
        }
        if (rel.substr(0,12) == "MINUTESECOND") {
            req_minutesecond = string_value(rel.substr(12), rel);
        }
        _logger->debug("Requested month day {} week day {} - {}:{}:{}", req_monthday, req_weekday, req_dayhour, req_hourminute, req_minutesecond);
        int_fast8_t result_minute_second = 0, result_hour_minute = 0, result_day_hour = 0;
        int_fast16_t result_days = 0;
        if (req_minutesecond == -1) { 
            if ((req_hourminute == -1) && (req_dayhour == -1) && (req_weekday == -1) && !req_monthday)
                req_minutesecond = tm.tm_sec + 1;
            else
                req_minutesecond = 0;
        }
        bool underflow = value_since_now(req_minutesecond, 60, tm.tm_sec, result_minute_second);
        if (req_hourminute == -1) {
            if ((req_dayhour == -1) && (req_weekday == -1) && !req_monthday)
                req_hourminute = underflow ? tm.tm_min + 1 : tm.tm_min;
            else 
                req_hourminute = underflow ? 59 : 0;
        } else
            if (underflow)
                --req_hourminute;
        underflow = value_since_now(req_hourminute, 60, tm.tm_min, result_hour_minute);
        if (req_dayhour == -1) {
            if ((req_weekday == -1) && !req_monthday)
                req_dayhour = underflow ? tm.tm_hour + 1 : tm.tm_hour;
            else
                req_dayhour =  underflow ? 23 : 0;
        } else
             if (underflow)
                --req_dayhour;
        underflow = value_since_now(req_dayhour, 24, tm.tm_hour, result_day_hour);
        if (req_monthday != 0) {
            if (underflow) req_monthday++;
            result_days = parse_month(&tm, req_monthday);
        } else if (req_weekday != -1) {
            if (underflow) req_weekday++;
            int8_t days = 0;
            value_since_now(req_weekday, 7, tm.tm_wday, days);
            result_days = days;
        } else {
            //result_days = underflow ? 1 : 0;
        }
        _logger->debug("waiting since now - {} days; {}:{}.{} ", result_days, result_day_hour, result_hour_minute, result_minute_second);
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        auto reminder = millis % 1000;  // fine tuning to reach ~ one second accuracy
        return now + std::chrono::milliseconds(950 - reminder) + std::chrono::minutes(result_hour_minute) + std::chrono::seconds(result_minute_second) + std::chrono::hours(result_day_hour) + std::chrono::hours(24*result_days);
    } else // this should never happen unless someone change the code
        throw std::runtime_error("Unexpected program behavior - plese report it!");
}

int Exe_Service::wait(lua_State * l, bool _or) {
    _logger->trace("[LUA] wait enter");
    {
        std::vector<std::string*> sensor_value_list;
        std::vector<std::chrono::system_clock::time_point> time_list;
        int argc = lua_gettop(l);
        for (int i = 1; i <= argc; ++i) {
            if (lua_isuserdata(l, i))
                sensor_value_list.emplace_back(reinterpret_cast<std::string*>(lua_touserdata(l,i)));
            else if (lua_isstring(l, i)) {
                try {
                    time_list.emplace_back(parse_time_string(std::string(lua_tostring(l, i))));
                } catch (const std::exception &e) {
                    luaL_error (l, "wait_and/or: - Error: %s", e.what());
                }
            } else
                luaL_error (l, "wait_and/or: unexpected input type %d at %d", lua_type(l, i), i);
        }
        if (sensor_value_list.empty() && time_list.empty())
            luaL_error (l, "wait_and/or: nothing to wait for!");
        {
            struct SynchronizationObject sync_obj;  // sync_obj is temporary and controlled by this thread
            if (!sensor_value_list.empty()) {
                std::unique_lock<std::mutex> value_map_lock(_map_mutex);
                for (const auto& sensor_value : sensor_value_list)
                    _value_wait_map.emplace(*sensor_value, &sync_obj);
            }
            if (!time_list.empty()) {
                std::unique_lock<std::mutex> time_map_lock(_time_mutex);
                for (const auto& time_point : time_list)
                    _time_wait_map.emplace(time_point, &sync_obj);
            }
            {
                std::mutex mtx;
                std::unique_lock<std::mutex> wait_lock(mtx);
                if (!_or) {  //wait and
                    size_t wait_and_val = sensor_value_list.size() + time_list.size();
                    sync_obj.cond_var.wait(wait_lock, [&]() -> bool {
                        return _terminate_lua_threads || sync_obj.num >= wait_and_val;
                    }); 
                } else  //wait or
                    sync_obj.cond_var.wait(wait_lock, [&]() -> bool {
                        return _terminate_lua_threads || sync_obj.num > 0;
                    }); 
                _logger->trace("[LUA] wait - wake");
            }
            if (!sensor_value_list.empty()) {
                std::unique_lock<std::mutex> value_map_lock(_map_mutex);
                for (const auto& sensor_value : sensor_value_list) {
                    auto range = _value_wait_map.equal_range(*sensor_value);
                    for (auto it = range.first; it != range.second; ) {
                        if (it->second == nullptr || it->second == &sync_obj)
                            it = _value_wait_map.erase(it);
                        else
                            ++it;
                    }
                }
            }
            if (!time_list.empty()) {
                std::unique_lock<std::mutex> time_map_lock(_time_mutex);
                for (const auto& time_point : time_list) {
                    auto range = _time_wait_map.equal_range(time_point);
                    for (auto it = range.first; it != range.second; ) {
                        if (it->second == nullptr || it->second == &sync_obj)
                            it = _time_wait_map.erase(it);
                        else
                            ++it;
                    }
                }
            }
        }
    }
    if (_terminate_lua_threads)
        luaL_error (l, "Terminate thread internally requested");
    _logger->trace("[LUA] wait exit");
    return 0;
}

int lua_req_value(lua_State * l) {
    return get_exe_object(l)->req_value(l);
}

int Exe_Service::req_value(lua_State * l) {
    std::vector<std::string*> sensor_value_list;
    int argc = lua_gettop(l);
    _logger->trace("[LUA] req_value: enter arguments {}", argc);
    for (int i = 1; i <= argc; ++i) {
        if (lua_isuserdata(l, i)) {
            sensor_value_list.emplace_back(reinterpret_cast<std::string*>(lua_touserdata(l,i)));
        } else
            luaL_error(l, "req_value: unexpected input type {} : {}", lua_type(l, i), i);
    }
    if (sensor_value_list.empty()) 
        return 0;

    std::unique_lock<std::mutex> value_map_lock(_value_mutex);
    for (const auto& sensor_value : sensor_value_list) {
        auto it = _last_val_double_map.find(*sensor_value);
        if (it == _last_val_double_map.end()) {
            auto b_it = _last_val_boolean_map.find(*sensor_value);
            if (b_it == _last_val_boolean_map.end())
                lua_pushnil(l);
            else
                lua_pushboolean(l, static_cast<int>(b_it->second));
        } else {
            lua_pushnumber(l, it->second);
        }
    }
    return sensor_value_list.size();
}
//
int lua_write_value(lua_State * l) {
    return get_exe_object(l)->write_value(l, false);
}

int lua_report_value(lua_State * l) {
    return get_exe_object(l)->write_value(l, true);
}

int Exe_Service::write_value(lua_State * l, bool report) {
    std::regex txt_regex("(\\w|\\/)+\\:\\w+");
    int argc = lua_gettop(l);
    _logger->trace("[LUA] write_value enter arguments {}", argc);
    if (argc != 2)
        luaL_error(l, "write_value: wrong argument count! (2 expected got %d)", argc);
    if (!lua_isstring(l, 1))
        luaL_error(l, "write_value: wrong argument type of first argument - string expected!");
    std::string sensor_value_text = lua_tostring(l, 1);
    if (!std::regex_match(sensor_value_text, txt_regex))
        luaL_error(l, "write_value: wrong argument format of first argument - \"must be path/path/path:value\"!");
    struct json_object* j_object = json_object_new_object();
    auto sensor_value_separator_position = sensor_value_text.find(":");
    auto sensor_name = sensor_value_text.substr(0, sensor_value_separator_position);
    sensor_name = (report ? std::string("status/") : std::string("set/")) + sensor_name;
    auto sensor_value = sensor_value_text.substr(sensor_value_separator_position + 1);
    struct json_object* j_value = nullptr;
    switch (lua_type(l, 2)) {
        case LUA_TBOOLEAN:
        {
            j_value = json_object_new_boolean(lua_toboolean(l,2));
            break;
        }
        case LUA_TNUMBER:
        {
            if (lua_isinteger(l,2)) {
                j_value = json_object_new_int64(lua_tointeger(l, 2));
            } else {
                j_value = json_object_new_double(lua_tonumber(l, 2));
            }
            break;
        }
        default:
            luaL_error(l, "write_value: wrong argument type of the second argument %d - bool & number are supported", lua_type(l, 2));
    }
    json_object_object_add(j_object, sensor_value.c_str(), j_value);
    const std::string json_string = json_object_to_json_string_ext(j_object, JSON_C_TO_STRING_PLAIN);
    Publish(sensor_name, json_string);
    json_object_put(j_object);
    return 0;
}

int lua_set_global(lua_State *l) {
    return get_exe_object(l)->set_global(l);
}

int lua_get_global(lua_State *l) {
    return get_exe_object(l)->get_global(l);
}

int Exe_Service::set_global(lua_State * l) {
    int argc = lua_gettop(l);
    _logger->trace("[LUA] set_global: enter arguments {}", argc);
    if (argc != 2)
        luaL_error(l, "set_global: wrong argument count! (there must be exactly two arguments)"); 
    if (!lua_isstring(l, 1))
        luaL_error(l, "set_global: wrong argument type of the first argument - string expected! (nothing written)");
    std::string value_name_text = lua_tostring(l, 1);
    std::unique_lock<std::mutex> global_lock(_global_mutex);
    auto val_type = lua_type(l, 2);
    if (val_type != LUA_TBOOLEAN && val_type != LUA_TNUMBER)
        luaL_error(l, "set_global: unsupported value type %d", lua_type(l, 2));
    auto result = _global_map.find(value_name_text);
    if (result == _global_map.end())
        _global_map.emplace(value_name_text, val_type == LUA_TBOOLEAN ?  lua_toboolean(l,2) :  lua_tonumber(l, 2));
    else {
        if (val_type == LUA_TBOOLEAN)
            result->second.set_value_b(lua_toboolean(l,2));
        else
            result->second.set_value_n(lua_tonumber(l, 2));
    }
    return 0;
}

int Exe_Service::get_global(lua_State * l) {
    int argc = lua_gettop(l);
    _logger->trace("[LUA] get_global: enter arguments {}", argc);
    if (argc != 1)
        luaL_error(l, "get_global: wrong argument count! (there must be exactly one argument)");
    if (!lua_isstring(l, 1))
        luaL_error(l, "get_global: wrong argument type of the first argument - string expected! (nothing written)");
    std::string value_name_text = lua_tostring(l, 1);
    std::unique_lock<std::mutex> global_lock(_global_mutex);
    auto result = _global_map.find(value_name_text);
    if (result == _global_map.end())
        lua_pushnil (l);
    else {
        if(result->second._luatype == LUA_TBOOLEAN)
            lua_pushboolean(l, result->second._value.boolean);
        else 
            lua_pushnumber(l, result->second._value.number);
    }
    return 1;
}