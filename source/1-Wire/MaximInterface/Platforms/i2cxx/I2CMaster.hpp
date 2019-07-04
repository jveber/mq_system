// Copyright: (c) Jaromir Veber 2017-2018
// Version: 08102018
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************

#pragma once

#include <i2c/i2cxx.hpp>         // GPIO connector
#include <MaximInterface/Links/I2CMaster.hpp>
#include <MaximInterface/Utilities/Error.hpp>
#include <MaximInterface/Utilities/system_error.hpp>

namespace MaximInterface {

class xxI2CMaster : public MaximInterface::I2CMaster {
public:

  enum ErrorValue {
    CommunicationError = 1,
    OpenDeviceError,
    AlreadyConnectedError,
	  WriteByteError,
	  WriteMultiByteError,
	  ReadByteError,
	  ToMuchDataError,
	  BadHandleError,
	  BadParamError,
	  WriteFailedError,
	  UnknownError
  };

  xxI2CMaster(const std::string& path, std::shared_ptr<spdlog::logger>& logger): _dev(nullptr), _path(path), _logger(logger) {}

  virtual error_code start(uint_least8_t address);
  virtual error_code stop();
  virtual error_code writeByte(uint_least8_t data);
  virtual error_code readByte(AckStatus status, uint_least8_t & data);  
  MaximInterface_EXPORT static const error_category & errorCategory(int);
  
  

protected:
  virtual error_code readPacketImpl(uint_least8_t address, uint_least8_t * data, size_t dataLen, bool sendStop); 
  virtual error_code writePacketImpl(uint_least8_t address, const uint_least8_t * data, size_t dataLen, bool sendStop);

private:
  i2cxx* _dev;
  std::string _path;
  std::shared_ptr<spdlog::logger> _logger;
};

inline error_code pi_make_error_code(xxI2CMaster::ErrorValue e, int code = 0) {
     return error_code(e, xxI2CMaster::errorCategory(code));
}

} // namespace MaximInterface
