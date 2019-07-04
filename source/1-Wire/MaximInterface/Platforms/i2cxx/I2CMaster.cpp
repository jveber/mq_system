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

error_code xxI2CMaster::start(uint_least8_t address) {
   if (_dev != nullptr)
        return pi_make_error_code(AlreadyConnectedError);
   try {
        _dev = new i2cxx(_path, address, _logger);
   } catch(const std::runtime_error& error) {
        return pi_make_error_code(OpenDeviceError); 
   }
	 return error_code();
}

error_code xxI2CMaster::stop() {
  delete _dev;
  _dev = nullptr;
	return error_code();
}

error_code xxI2CMaster::writeByte(uint_least8_t data) {
	if (_dev == nullptr)
		return pi_make_error_code(BadHandleError);
	try {
		_dev->write_byte(data);
	} catch(const std::runtime_error& error) {
		return pi_make_error_code(WriteByteError);
	}
  return error_code();
}

error_code xxI2CMaster::writePacketImpl(uint_least8_t address, const uint_least8_t * data, size_t dataLen, bool sendStop) {
	if (_dev == nullptr)
		return pi_make_error_code(BadHandleError);
	try {	
		switch (dataLen) {
	  	case 1:
				_dev->write_byte(data[0]);
				break;
	  case 2:
				_dev->write_byte_data(data[0], data[1]);
				break;
	  default:
				return pi_make_error_code(ToMuchDataError, dataLen);
		}
		// technically we might support also 3 bytes using write_word_data.. but write_block_data would not work becuase it also writes count of data!
	} catch(const std::runtime_error& error) {
		return pi_make_error_code(WriteByteError);
	}
	return error_code();
}

error_code xxI2CMaster::readByte(AckStatus status, uint_least8_t & data) {
	if (_dev == nullptr)
		return pi_make_error_code(BadHandleError);
	try {
		data = _dev->read_byte();
	} catch (const std::runtime_error& error) {
		return pi_make_error_code(ReadByteError);
	}
  return error_code();
}

error_code xxI2CMaster::readPacketImpl(uint_least8_t address, uint_least8_t * data, size_t dataLen, bool sendStop) {
	try {
		switch (dataLen) {
			case 1:
				data[0] = _dev->read_byte();
				break;
			default:
				return pi_make_error_code(ToMuchDataError, dataLen);
		}
	} catch (const std::runtime_error& error) {
		return pi_make_error_code(ReadByteError);
	}
	return error_code();
}

const error_category & xxI2CMaster::errorCategory(int code) {
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