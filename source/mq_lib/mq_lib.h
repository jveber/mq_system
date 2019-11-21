#pragma once
// Copyright: (c) Jaromir Veber 2017-2019
// Version: 29032019
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************
// MQLib is supposed to siplify deamon stubs that are part of all deamons in mq_system

#include "../config.h"
#include <mosquitto.h>  // struct mosquitto...

#include <cstdint>
#include <string>

#include "spdlog/spdlog.h"

namespace MQ_System {


class Daemon {
 public:
    Daemon(const char* demon_name, const char* pid_name, bool no_daemon = false);  // may throw std::runtime_error if error happened (always shall log reason)
    virtual ~Daemon() noexcept;
    virtual void CallBack(const std::string& topic , const std::string& message) {} // user may overload this one if he needs callback function - proxy for message system - Subscribe (Callback)
    void Unsubscribe(const std::string& topic) noexcept;  // proxy for message system - Unsubscribe
    void Subscribe(const std::string& topic) noexcept; // proxy for message system - Subscribe
    void Publish(const std::string& topic, const std::string& message); // proxy for message system - Publish
    const std::shared_ptr<spdlog::logger> _logger;  // logger for general use (logging)
    void SleepForever();  // it is better to avoid this one as much as possible but sometimes I found that hard to avoid it so it's here.
 private:
    void load_mq_system_configuration();
    void daemonize(bool no_deamon) const;
    void connect_mqtt();
    std::shared_ptr<spdlog::sinks::sink> conect_log_db();

    static const char* kMqSystemConfigFile;
    static const char* kDefaultHost;
    static constexpr int kDefaultPort = 1887;

    struct mosquitto* _mosquitto_object;
    int _connection_port;
    std::string _connection_host;
    std::string _log_db;
    std::string _log_file = "/var/log/mq_system/system.log";
    const std::string _pid_file;
    bool _log_mqtt;
};

}  // namespace MQ_System
