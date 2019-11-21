// Copyright: (c) Jaromir Veber 2017-2019
// Version: 18032019
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************

#include "./mq_lib.h"
// system
#include <signal.h>  // to handle signals (SIGTERM)
#include <unistd.h>  // getpid(2)
// stdlib
#include <cstdlib>  // daemon(3)
#include <cstdio>  // fopen(3), fwrite for pid file preparation
// c++lib
#include <stdexcept> // runtime_error
// external lib
#include <libconfig.h++>  // configuration file parsing
// spdlog (logging library)
#include "spdlog/sinks/dist_sink.h"
#include "spdlog/sinks/syslog_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/spdlog.h"
//custom logging interfaces
#include "mosq_log.h"
#include "db_log.h"

// sanitizers
#ifndef NDEBUG 
    #include <sanitizer/asan_interface.h>
#endif


namespace MQ_System {

Daemon *g_daemon = nullptr;  // this is needed to support signal handler access to terminate the daemon if called

} // namespace MQ_System

static void signal_handler(int sig) {
    if (sig == SIGTERM) {
        MQ_System::g_daemon->~Daemon();  // This may lead to leak; however we expect Daemon to be stack constructed so we can't let normal destructor to be here and placement destructor would do the same... 
        exit(0);
    } else if (sig == SIGHUP){
        //TODO reload configuration - not implemented
    }
}

void sqlog_error_callback(void *pArg, int iErrCode, const char *zMsg) {
    MQ_System::Daemon * vData = reinterpret_cast<MQ_System::Daemon *>(pArg);
    vData->_logger->debug("Sqlite code: {} message: {}", iErrCode, zMsg);
}

#ifndef NDEBUG
extern "C" const char *__asan_default_options() {
  return "log_path=/home/jaromir/debug/mq_system:check_initialization_order=true:detect_stack_use_after_return=true:detect_leaks=1";
}

extern "C" const char* __ubsan_default_options() {
    return "log_path=/home/jaromir/debug/mq_system";
}
#endif

namespace MQ_System {
const char* Daemon::kMqSystemConfigFile = "/etc/mq_system/system.conf";  // hard coded path (might be altered but...)
const char* Daemon::kDefaultHost = "127.0.0.1";


Daemon::Daemon(const char* daemon_name, const char* pid_name, bool no_daemon) : _log_mqtt(false), _pid_file(pid_name), _logger(std::make_shared<spdlog::logger>(daemon_name, std::make_shared<spdlog::sinks::dist_sink_mt>())) {
    g_daemon = this;  // save the daemon ID for signal handler purposes
    auto _log_sink = std::dynamic_pointer_cast<spdlog::sinks::dist_sink_mt>(_logger->sinks()[0]);
    _logger->set_level(spdlog::level::trace);
    auto basic_sink = std::make_shared<spdlog::sinks::syslog_sink_mt>("mq_system", 0, LOG_USER, false);  // shall we use systemd sink for systemd?
    _log_sink->add_sink(basic_sink);
    daemonize(no_daemon);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    load_mq_system_configuration();
	_logger->flush();
    if (_log_file.size()) {
        try {
            _log_sink->add_sink(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(_log_file, 1024*1024, 5));
            _logger->flush();
            _log_sink->remove_sink(basic_sink);
            _logger->trace("file_log initialized");
        } catch (const spdlog::spdlog_ex& spdex) {
            _logger->error(spdex.what());
        }
    }
    if (_log_db.size()) {
        try {
            auto log_db_sink = conect_log_db();
            if (log_db_sink) {
                _log_sink->add_sink(log_db_sink);
                _logger->flush();
                _log_sink->remove_sink(basic_sink);
                _logger->trace("db_log initialized");
            } else {
                _logger->error("failed to initialize db_log");
            }
        } catch (const spdlog::spdlog_ex& spdex) {
            _logger->error(spdex.what());
        }
    }
    _logger->trace("log setup done");
    _logger->flush();
    connect_mqtt();
    if (_log_mqtt) {
        _log_sink->add_sink(std::make_shared<mosq_sink>(_mosquitto_object));  // this sink is not to presistent storage
        _logger->flush();
        _log_sink->remove_sink(basic_sink);
        _logger->trace("mqtt_log initialized");
    }
    _logger->info("Demon initialization finished");
    _logger->flush();
}

std::shared_ptr<spdlog::sinks::sink> Daemon::conect_log_db() {
    static const char log_init[] = "CREATE TABLE IF NOT EXISTS log (timestamp TIMESTAMP PRIMARY KEY, level INTEGER, thread INTEGER, msgid INTEGER, logger STRING, message STRING)";
    sqlite3* _pLog = nullptr;
    if (!sqlite3_threadsafe()) {
        if (sqlite3_config(SQLITE_CONFIG_SERIALIZED)) {
            _logger->warn("Unable to set serialized mode for SQLite! Tt is necessary to recompile it SQLITE_THREADSAFE!");
            _logger->warn("We Continue using unsafe interface but beware of possible crash");
        }
    }
    sqlite3_config(SQLITE_CONFIG_LOG, sqlog_error_callback, this);

    sqlite3_initialize();
    if (SQLITE_OK != sqlite3_open_v2(_log_db.c_str(), &_pLog, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL)) {
        _logger->warn("Sqlite3: Unable to open log database file! {}", _log_db);
        return nullptr;
    }
    sqlite3_extended_result_codes(_pLog, true);
    char *errmsg = nullptr;
    if (SQLITE_OK != sqlite3_exec(_pLog, log_init, NULL, NULL, &errmsg)) {
        _logger->warn("Sqlite3: create table error: {}", errmsg);
        return nullptr;
    }
    return std::make_shared<db_sink>(_pLog);
}

void Daemon::load_mq_system_configuration() {
    // read mq_system global configuration - libconfig++
    libconfig::Config cfg;
    try {
        cfg.readFile(kMqSystemConfigFile);
    } catch(const libconfig::FileIOException &fioex) {
        _logger->error("I/O error while reading system configuration file: {}", kMqSystemConfigFile);
        _logger->flush();
        throw std::runtime_error("");
    } catch(const libconfig::ParseException &pex) {
        _logger->error("Parse error at {} : {} - {}",pex.getFile(), pex.getLine(), pex.getError());
        _logger->flush();
        throw std::runtime_error("");
    }
    try {
        if (cfg.exists("mqtt_connection.host"))
            cfg.lookupValue("mqtt_connection.host", _connection_host);
        else
            _connection_host = kDefaultHost;
        if (cfg.exists("mqtt_connection.port"))
            cfg.lookupValue("mqtt_connection.port", _connection_port);
        else
            _connection_port = kDefaultPort;
        if (cfg.exists("log_db"))
            cfg.lookupValue("log_db", _log_db);
        if (cfg.exists("log_file"))
            cfg.lookupValue("log_file", _log_file);
        if (cfg.exists("log_mqtt"))
            cfg.lookupValue("log_mqtt", _log_mqtt);
        if  (cfg.exists("log_level")) {
            int level;
            cfg.lookupValue("log_level", level);
            _logger->set_level(static_cast<spdlog::level::level_enum>(level));		
        }
    } catch(const libconfig::SettingNotFoundException &nfex) {
        _logger->error("Required setting not found in system configuration file");
        _logger->flush();
        throw std::runtime_error("");
    } catch (const libconfig::SettingTypeException &nfex) {
        _logger->error("Seting type error (at system configuaration) at: {}", nfex.getPath());
        _logger->flush();
        throw std::runtime_error("");
    }
}

void Daemon::daemonize(bool no_daemon) const {
#if DAEMON_MANAGER==1
    if (no_daemon) {
        _logger->debug("Daemonize no-daemon!");
        return;
    }
    if (daemon(0, 0)) {
        _logger->error("Daemonize error");
        throw std::runtime_error("");
    }
    FILE* pid_file = fopen(_pid_file.c_str(), "wx");
    if (pid_file == NULL) {
        _logger->error("Unable to open pid file for writing or file already exists: {}", _pid_file);
        throw std::runtime_error("");
    }
    if (0 > fprintf(pid_file, "%u", static_cast<uint32_t>(getpid()))) {
        _logger->error("Unable to write pid to pid file: {}", _pid_file);
        throw std::runtime_error("");
    }
    fclose(pid_file);
    _logger->debug("Deamonize done");
#else
    _logger->debug("Daemonize skipped!");
#endif // systemd does not need to fork & PID file at all
}

static void OnMessage (struct mosquitto *mosq __attribute__((unused)), void * context, const struct mosquitto_message * message) {
    const std::string topic(message->topic);
    const std::string smessage(reinterpret_cast<const char*>(message->payload), message->payloadlen);
    reinterpret_cast<Daemon*>(context)->CallBack(topic, smessage);
}

void Daemon::connect_mqtt() {
    _logger->trace("connect_mqtt");
    _logger->flush();
    mosquitto_lib_init();
    _mosquitto_object = mosquitto_new(NULL, true, this);
    if (!_mosquitto_object) {
        _logger->error("Mosquitto Error: Out of memory.");
        _logger->flush();
        throw std::runtime_error("");
    }
    int connection_attempts = 10;  // we wait up to 10s for connection (it may start the same time the mosquitto dows so it's gonna wait for it to init)
    for(; connection_attempts > 0; --connection_attempts) {
        if (mosquitto_connect(_mosquitto_object, _connection_host.c_str(), _connection_port, 60) == MOSQ_ERR_SUCCESS)
            break;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    if (!connection_attempts) {
        _logger->error("Mosquitto Error: Unable to connect to {}:{}", _connection_host, _connection_port);
        mosquitto_destroy(_mosquitto_object);   // we're still in constructor so we need to call this one explicitly.
        mosquitto_lib_cleanup();
        _logger->flush();
        throw std::runtime_error("");
    }
    _logger->trace("connect_mqtt done after {} seconds", 10 - connection_attempts);
    _logger->flush();
    mosquitto_message_callback_set(_mosquitto_object, OnMessage);
    mosquitto_loop_start(_mosquitto_object);
}

Daemon::~Daemon() noexcept {
    _logger->trace("~Daemon()");
#if DAEMON_MANAGER==1
    _logger->trace("Unlink {}", _pid_file);
    auto res = unlink(_pid_file.c_str());
    if (res < 0) {
        _logger->error("unlink pid error {}", strerror(errno));
    } else
    _logger->trace("Unlink successful");
#endif
    _logger->info("Terminating");
    if (!_logger.unique())
        _logger->warn("Logger terminate - Pointer not unique!");
    _logger->flush();
    // const_cast<std::shared_ptr<spdlog::logger>(_logger).reset();  // now we release and cleanup all sinks 
    spdlog::shutdown();
    mosquitto_disconnect(_mosquitto_object);
    mosquitto_loop_stop(_mosquitto_object, true);
    mosquitto_destroy(_mosquitto_object);
    mosquitto_lib_cleanup();
}

void Daemon::Subscribe(const std::string& topic) noexcept {
    if (MOSQ_ERR_SUCCESS != mosquitto_subscribe(_mosquitto_object, NULL, topic.c_str(), 2)) {
        _logger->error("Subscribe topic {} error!", topic);
    }
}

void Daemon::Unsubscribe(const std::string& topic) noexcept {
    if (MOSQ_ERR_SUCCESS != mosquitto_unsubscribe(_mosquitto_object, NULL, topic.c_str())) {
        _logger->error("Unsubscribe topic {} error!", topic);
    }
}

void Daemon::Publish(const std::string& topic, const std::string& message) {
    int mosresult = mosquitto_publish(_mosquitto_object, NULL, topic.c_str(), message.length(), message.c_str(), 2, false);
    if (mosresult != MOSQ_ERR_SUCCESS)
        _logger->warn("Publish error: {} ", mosresult);
}
void Daemon::SleepForever() {
    std::condition_variable cv;
    std::mutex m;
    std::unique_lock<std::mutex> lock(m);
    cv.wait(lock, [] {return false;});  // wait for something that would never happen
    //std::this_thread::sleep_until(std::chrono::time_point<std::chrono::system_clock>::max());  // this one proved to be unsafe (overflow (undefined) - may not work)
}

}  // namespace MQ_System
