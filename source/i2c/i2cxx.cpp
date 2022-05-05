// Copyright: (c) Jaromir Veber 2019
// Version: 10122019
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************
// i2cxx controlls I2C device with easy-to-use interface and efficent internal overhead

#include <i2cxx.hpp>

#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

i2cxx::i2cxx(const std::string& path, unsigned int i2c_addr, const std::shared_ptr<spdlog::logger>& logger): _logger(logger), _fd(-1), _addr(i2c_addr) {
    auto fd = open(path.c_str(), O_RDWR);
    if (fd < 0) {
        _logger->error("I2Cxx unable to open specified i2c device {} : {}", path, strerror(errno));
        throw std::runtime_error("");
    }
    if (ioctl(fd, I2C_SLAVE, i2c_addr) < 0) {
      close(fd);
      _logger->error("I2Cxx unable to select specified slave device {} : {}", i2c_addr, strerror(errno));
      throw std::runtime_error("");
    }
    _fd = fd;
}

i2cxx::~i2cxx() noexcept {
    if (_fd >= 0)
        close(_fd);
}

int i2cxx::smbus_data(char rw, uint8_t cmd, unsigned int size, union i2c_smbus_data *data) {
   struct i2c_smbus_ioctl_data args { rw, cmd, size, data };
   return ioctl(_fd, I2C_SMBUS, &args);
}

void i2cxx::write_byte(unsigned bVal) {
    if (smbus_data(I2C_SMBUS_WRITE, bVal, I2C_SMBUS_BYTE, NULL) < 0) {
        _logger->error("I2Cxx write byte error {} : {}", _addr, strerror(errno));
        throw std::runtime_error("");
    }
}

void i2cxx::write_byte_data(unsigned reg, unsigned bVal) {
    union i2c_smbus_data data = {};
    data.byte = bVal;
    if (smbus_data(I2C_SMBUS_WRITE, reg, I2C_SMBUS_BYTE_DATA, &data) < 0) {
        _logger->error("I2Cxx write byte error {} : {}", _addr, strerror(errno));
        throw std::runtime_error("");
    }
}

uint8_t i2cxx::read_byte() {
    union i2c_smbus_data data = {};
    if (smbus_data(I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data) < 0) {
        _logger->error("I2Cxx read byte error {} : {}", _addr, strerror(errno));
        throw std::runtime_error("");
    }
    return data.byte;
}

uint8_t i2cxx::read_byte_data(unsigned reg) {
    union i2c_smbus_data data = {};
    if (smbus_data(I2C_SMBUS_READ, reg, I2C_SMBUS_BYTE_DATA, &data) < 0) {
        _logger->error("I2Cxx read byte data error {} : {}", _addr, strerror(errno));
        throw std::runtime_error("");
    }
    return data.byte;
}

void i2cxx::read(char *buf, size_t count) {
    auto result = ::read(_fd, static_cast<void*>(buf), count);
    if (result != static_cast<decltype(result)>(count)) {
        _logger->error("I2Cxx read error {} : {}", _addr, strerror(errno));
        throw std::runtime_error("");
    }
}