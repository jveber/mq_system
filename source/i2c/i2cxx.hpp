#pragma once
// Copyright: (c) Jaromir Veber 2019
// Version: 25032019
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************
// i2cxx controlls I2C device with easy-to-use interface and efficent internal overhead

#include "spdlog/spdlog.h"
#include <cstdint>
#include <string>

class i2cxx {
    public:
        i2cxx(const std::string& path, unsigned int i2c_addr, std::shared_ptr<spdlog::logger>& logger);
        ~i2cxx() noexcept;

        void write_byte(unsigned bVal);
        void write_byte_data(unsigned reg, unsigned bVal);
        uint8_t read_byte();
        uint8_t read_byte_data(unsigned reg);
        void read(char *buf, size_t count);

    private:
        int smbus_data(char rw, uint8_t cmd, int size, union i2c_smbus_data *data);

        std::shared_ptr<spdlog::logger>& _logger;
        int _fd;
        unsigned _addr;
};
