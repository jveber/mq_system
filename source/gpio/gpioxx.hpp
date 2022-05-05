#pragma once
// Copyright: (c) Jaromir Veber 2019
// Version: 10122019
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************
// gpiocxx controlls GPIO defice with easy-to-use interface and efficent internal overhead

#include "spdlog/spdlog.h"
#include <cstdint>
#include <vector>
#include <string>
#include <linux/gpio.h> // we might move this one away.. need it for struct gpioevent_data (but we might redefine it here)

class gpiocxx {
    public:
        enum class event_req {
            RISING_EDGE,
            FALLING_EDGE,
            BOTH_EDGES
        };

        gpiocxx(const std::string& path, const std::shared_ptr<spdlog::logger>& logger);
        ~gpiocxx();
        
        bool is_output(uint32_t gpio);
        bool is_input(uint32_t gpio);
        void set_input(uint32_t gpio);
        void set_output(uint32_t gpio);
        bool get_value(uint32_t gpio);
        void set_value(uint32_t gpio, bool value);
        void reset(uint32_t gpio);
        void watch_event(uint32_t gpio, event_req event);
        int  wait_event(uint32_t gpio, const struct timespec *timeout_ts);
        std::vector<uint32_t>  wait_events(const std::vector<uint32_t>& l, const struct timespec *timeout);
        void read_event(uint32_t gpio, struct gpioevent_data& evdata);
    private:
        inline void check_offset(uint32_t gpio);
        void request(uint32_t gpio, bool input);

        const std::shared_ptr<spdlog::logger> _logger;
        int _chip_fd;

        struct line {
            line () : input(false), event(false), handle_fd(-1) {}
            bool input;
            bool event;
            int handle_fd;
        };
        std::vector<struct line*> _lines;
};
