// Copyright: (c) Jaromir Veber 2017-2018
// Version: 08102018
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************

#include "I2CMaster.hpp"
#include <MaximInterfaceCore/None.hpp>

namespace MaximInterfaceCore {

Result<void> piI2CMaster::start(uint_least8_t address) {
    if (_pigpio_handle < 0)
        _pigpio_handle = pigpio_start(NULL, NULL);  // technically we also could support GPIO read from another RPI but well not yet needed TODO?
    if (_pigpio_handle < 0) {
        return I2CMaster::NackError;
    }
    int result = i2c_open(_pigpio_handle, 1, address, 0);
    if (result < 0) {
        stop();
        return I2CMaster::NackError;
    }
    _device_handle = static_cast<unsigned>(result);
    return MaximInterfaceCore::None(0);
}

Result<void> piI2CMaster::stop() {
    i2c_close(_pigpio_handle, _device_handle);
    pigpio_stop(_pigpio_handle);
    _pigpio_handle = -1;
    return MaximInterfaceCore::None(0);
}

Result<void> piI2CMaster::writeByte(uint_least8_t data) {
    int result = i2c_write_byte(_pigpio_handle, _device_handle, data);
    if (result < 0)
        return I2CMaster::NackError; 
    return MaximInterfaceCore::None(0);
}

Result<void> piI2CMaster::writePacket(uint_least8_t address, span<const uint_least8_t> data, DoStop doStop) {
    int result = 0;
    switch (data.size()) {
    case 1:
        result = i2c_write_byte(_pigpio_handle, _device_handle, data[0]);
        break;
    case 2:
        result = i2c_write_byte_data(_pigpio_handle, _device_handle, data[0], data[1]);
        break;
    default:
        return I2CMaster::NackError;
    }
    // technically we might support also 3 bytes using write_word_data.. but write_block_data would not work becuase it also writes count of data!
    if (result < 0)
        return I2CMaster::NackError;
    return MaximInterfaceCore::None(0);
}

Result<uint_least8_t> piI2CMaster::readByte(I2CMaster::DoAck doAck) {
    int result = i2c_read_byte(_pigpio_handle, _device_handle);
    if (result < 0)
        return I2CMaster::NackError;
    return static_cast<uint_least8_t>(result);
}

Result<void> piI2CMaster::readPacket(uint_least8_t address, span<uint_least8_t> data, DoStop doStop) {
    int result = 0;
    switch (data.size()) {
        case 1:
            result = i2c_read_byte(_pigpio_handle, _device_handle);
            if (result >= 0)
                data[0] = static_cast<uint8_t>(result);
            break;
        default:
            return I2CMaster::NackError;
    }
    // well this is kind of problematic because if sequence S Addr Wr [A] Comm [A] S Addr Rd [A] [DataLow] A [DataHigh] NA P is needed it must be done all in once
    // we'd need to make a change in code (of device to support it)
    if (result < 0)
        return I2CMaster::NackError; 
    return MaximInterfaceCore::None(0);

/*
	int result = 0;
	for (int i = 0; i < dataLen - 1; ++i) {	
		result = i2c_read_byte(_pigpio_handle, _device_handle);
		if (result < 0) break;
		data[i] = static_cast<uint8_t>(result);
	}
	if (result < 0) {
		return pi_make_error_code(UnknownError, result);
	}
	return error_code();
*/
}

} // namespace MaximInterface