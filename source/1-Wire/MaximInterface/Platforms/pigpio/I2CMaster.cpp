// Copyright: (c) Jaromir Veber 2017-2018
// Version: 08102018
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************

#include "I2CMaster.hpp"

namespace MaximInterface {

error_code piI2CMaster::start(uint_least8_t address) {
  	if (_pigpio_handle >= 0)
		return pi_make_error_code(AlreadyConnectedError);

	_pigpio_handle = pigpio_start(NULL, NULL);  // technically we also could support GPIO read from another RPI but well not yet needed TODO?
    if (_pigpio_handle < 0) {
        return pi_make_error_code(CommunicationError);
	}
	int result = i2c_open(_pigpio_handle, 1, address, 0);
	
	if (result < 0) {
		stop();
		return pi_make_error_code(OpenDeviceError);
	}
	_device_handle = static_cast<unsigned>(result);
	return error_code();
}

error_code piI2CMaster::stop() {
  i2c_close(_pigpio_handle, _device_handle);
  pigpio_stop(_pigpio_handle);
  _pigpio_handle = -1;
  return error_code();
}

error_code piI2CMaster::writeByte(uint_least8_t data) {
  int result = i2c_write_byte(_pigpio_handle, _device_handle, data);
  if (result < 0)
	  return pi_make_error_code(UnknownError, result);
  return error_code();
}

error_code piI2CMaster::writePacketImpl(uint_least8_t address, const uint_least8_t * data, size_t dataLen, bool sendStop) {
	int result = 0;
	
	switch (dataLen) {
	  case 1:		
		result = i2c_write_byte(_pigpio_handle, _device_handle, data[0]);
		break;
	  case 2:
		result = i2c_write_byte_data(_pigpio_handle, _device_handle, data[0], data[1]);
		break;
	  default:
		return pi_make_error_code(ToMuchDataError, dataLen);
	}
	// technically we might support also 3 bytes using write_word_data.. but write_block_data would not work becuase it also writes count of data!
	
	if (result < 0) {
		return pi_make_error_code(UnknownError, result);
	}
	return error_code();
}

error_code piI2CMaster::readByte(AckStatus status, uint_least8_t & data) {
  int result = i2c_read_byte(_pigpio_handle, _device_handle);
  if (result < 0)
	  return pi_make_error_code(UnknownError, result);
	  //return pi_make_error_code(ReadByteError);
  data = static_cast<uint_least8_t>(result);
  return error_code();
}

error_code piI2CMaster::readPacketImpl(uint_least8_t address, uint_least8_t * data, size_t dataLen, bool sendStop) {
	int result = 0;
	switch (dataLen) {
		case 1:
			result = i2c_read_byte(_pigpio_handle, _device_handle);			
			if (result >= 0)
				data[0] = static_cast<uint8_t>(result);
			break;
		default:
			return pi_make_error_code(ToMuchDataError, dataLen);
	}
	// well thi is kind of problematic because if sequence S Addr Wr [A] Comm [A] S Addr Rd [A] [DataLow] A [DataHigh] NA P is needed it must be done all in once
	// we'd need to make a change in code (of device to support it)
	
	if (result < 0) {
		return pi_make_error_code(UnknownError, result);
	}
	return error_code();
	
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

const error_category & piI2CMaster::errorCategory(int code) {
  static class i2c_cat : public error_category {
  public:
	i2c_cat(int code) : _code(code){}
	
    virtual const char * name() const { return "piI2CMaster"; }

    virtual std::string message(int condition) const {
      switch (condition) {
      case CommunicationError:
        return "Communication Error";

      case OpenDeviceError:
        return "Open Device Error";

      case WriteByteError:
        return "Write Byte Error";

      case WriteMultiByteError:
        return "Write Multi-Byte Error";

      case ReadByteError:
		return "Read Byte Errror";
		
	  case AlreadyConnectedError:
        return "Already Connected Error";
		
	  case ToMuchDataError:
		return (std::string("Too Much Data for Transfer Error: ") + std::to_string(_code));
		
	  case BadHandleError:
		return "Bad Handle Error";
	  case BadParamError:
		return "Bad Parameter Error";
	  case WriteFailedError:
		return "Write Failed Error";
	  case UnknownError:
		return (std::string("Unknown Error: ") + std::to_string(_code));
		
      default:
        return defaultErrorMessage(condition);
      }
    }
	
  private:
	int _code;
  } instance(code);
  return instance;
}

} // namespace MaximInterface