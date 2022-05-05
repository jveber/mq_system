// Copyright: (c) Jaromir Veber 2018-2019
// Version: 11092019
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************
// Description: Sink implementation for logging into defined mosquitto message

#include <mosquitto.h>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/base_sink.h"
#include "spdlog/fmt/fmt.h"

namespace spd = spdlog;

class mosq_sink : public spdlog::sinks::base_sink<spdlog::details::null_mutex> {
 public:
    explicit mosq_sink(struct mosquitto* mosquitto_object): _mosquitto_object(mosquitto_object) {}
 protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<spdlog::details::null_mutex>::formatter_->format(msg, formatted);
        mosquitto_publish(_mosquitto_object, NULL, "app/log/message", formatted.size(), formatted.data(), 2, false);
    }

    void flush_() override {}
 private:
    struct mosquitto* _mosquitto_object;
};