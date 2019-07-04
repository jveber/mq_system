// Copyright: (c) Jaromir Veber 2018-2019
// Version: 18062019
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include <sqlite3.h>  // SQLite communication
#include <json-c/json_tokener.h>  // JSON format translation (reading)

#include <array>  // for constant C++11 iterable arrays
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <condition_variable>
#include <vector>
#include <mutex>
#include <atomic>

#include "mq_lib.h"  // MQ_System utility library

using namespace libconfig;

class Exe_Service : public MQ_System::Daemon {
public:
    Exe_Service();
    virtual ~Exe_Service() noexcept;
    void main();
    virtual void CallBack(const std::string& topic, const std::string& message) override;
    int register_sensor(lua_State *l);
    int req_value(lua_State *l);
    int wait(lua_State * l, bool);
    int print(lua_State * l, bool);
    int write_value(lua_State * l, bool);
    void time_thread_loop();
    void thread_start(lua_State *lua_state, const std::string& script_name);
    int set_global(lua_State * l);
    int get_global(lua_State * l);

 private:
    struct SynchronizationObject {
        SynchronizationObject():num(0){}
        std::condition_variable cond_var;
        std::atomic<size_t> num;
    };

    struct LuaValue {
        explicit LuaValue(int b) { set_value_b(b); }
        explicit LuaValue(lua_Number n) { set_value_n(n); }
        
        void set_value_n(lua_Number number) { _luatype = LUA_TNUMBER; _value.number = number; }
        void set_value_b(int b) { _luatype = LUA_TBOOLEAN; _value.boolean = b; }
        
        int _luatype;
        union {
            int boolean;
            lua_Number number;
        } _value;
    };

    static const std::array<std::string, 1 > kTableDefinitions;
    static const std::array<std::string, 1 > kStatementDefinitions;
    static const uint64_t flush_interval;
    static const std::string kReloadTopic;

    std::vector<sqlite3_stmt *> _statements;
    std::string _db_uri;
    sqlite3* _pDb;
    struct json_tokener* _tokener;

    void load_daemon_configuration();
    void check_and_init_database();
    void load_and_run_scripts();
    void execute_lua_script(const std::string& script_name, const std::string& script_content);
    void parse_status_message(const std::string& topic, const std::string& message);
    void parse_app_message(const std::string& topic);
    void stop_all();
    void start_all();
    bool scan_script(const std::string& content, std::unordered_set<std::string>& sensor_list);

    std::chrono::system_clock::time_point parse_time_string(const std::string& time_string);
    std::vector<std::thread> _script_threads;
    std::thread _time_thread;
    std::atomic<bool> _terminate_lua_threads;
    std::atomic<bool> _terminate_time_thread;

    std::mutex _map_mutex;
    std::unordered_multimap<std::string, SynchronizationObject*> _value_wait_map;

    std::mutex _time_mutex;
    std::multimap<std::chrono::system_clock::time_point, SynchronizationObject*> _time_wait_map;  // we need std::multimap coz it shall be sorted in order to make the sequential search in time thread possible

    std::mutex _value_mutex;
    std::unordered_map<std::string, double> _last_val_double_map;
    std::unordered_map<std::string, bool> _last_val_boolean_map;
    
    std::mutex _global_mutex;
    std::unordered_map<std::string, LuaValue> _global_map;
};
