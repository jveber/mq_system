// Copyright: (c) Jaromir Veber 2017-2019
// Version: 09122019
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************

#include "I2CMaster.hpp"

namespace MaximInterfaceCore {

Result<void> xxI2CMaster::start(uint_least8_t address) {
    if (_dev != nullptr)
        return I2CMaster::NackError;
    try {
        _dev = new i2cxx(_path, address, _logger);
    } catch(const std::runtime_error& error) {
        return I2CMaster::NackError; 
    }
    return MaximInterfaceCore::None(0);
}

Result<void> xxI2CMaster::stop() {
    delete _dev;
    _dev = nullptr;
    return MaximInterfaceCore::None(0);
}

Result<void> xxI2CMaster::writeByte(uint_least8_t data) {
    if (_dev == nullptr)
        return I2CMaster::NackError;
    try {
        _dev->write_byte(data);
    } catch(const std::runtime_error& error) {
        return I2CMaster::NackError;
    }
    return MaximInterfaceCore::None(0);
}

Result<void> xxI2CMaster::writePacket(uint_least8_t address, span<const uint_least8_t> data, DoStop doStop) {
    if (_dev == nullptr)
         return I2CMaster::NackError;
    try {
        switch (data.size()) {
            case 1:
                _dev->write_byte(data[0]);
                break;
            case 2:
                _dev->write_byte_data(data[0], data[1]);
                break;
            default:
                return I2CMaster::NackError;
        }
        // technically we might support also 3 bytes using write_word_data.. but write_block_data would not work becuase it also writes count of data!
    } catch(const std::runtime_error& error) {
        return I2CMaster::NackError;
    }
    return MaximInterfaceCore::None(0);
}

Result<uint_least8_t> xxI2CMaster::readByte(DoAck doAck) {
    if (_dev == nullptr)
        return I2CMaster::NackError;
    try {
        return _dev->read_byte();
    } catch (const std::runtime_error& error) {
        return I2CMaster::NackError;
    }
}

Result<void> xxI2CMaster::readPacket(uint_least8_t address, span<uint_least8_t> data, DoStop doStop) {
    try {
        switch (data.size()) {
            case 1:
                data[0] = _dev->read_byte();
                break;
            default:
                return I2CMaster::NackError;
        }
    } catch (const std::runtime_error& error) {
        return I2CMaster::NackError;
    }
    return MaximInterfaceCore::None(0);
}

} // namespace MaximInterface