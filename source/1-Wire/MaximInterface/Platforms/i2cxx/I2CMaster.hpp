// Copyright: (c) Jaromir Veber 2017-2019
// Version: 09122019
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************

#pragma once

#include <i2c/i2cxx.hpp>         // GPIO connector
#include <MaximInterfaceCore/I2CMaster.hpp>

namespace MaximInterfaceCore {

class xxI2CMaster : public I2CMaster {
public:
  xxI2CMaster(const std::string& path, const std::shared_ptr<spdlog::logger>& logger): _dev(nullptr), _path(path), _logger(logger) {}

  virtual Result<void> start(uint_least8_t address);
  virtual Result<void> stop();
  virtual Result<void> writeByte(uint_least8_t data);
  virtual Result<uint_least8_t> readByte(DoAck doAck);

protected:
  virtual Result<void> readPacket(uint_least8_t address, span<uint_least8_t> data, DoStop doStop); 
  virtual Result<void> writePacket(uint_least8_t address, span<const uint_least8_t> data, DoStop doStop);

private:
  i2cxx* _dev;
  std::string _path;
  const std::shared_ptr<spdlog::logger> _logger;
};

} // namespace MaximInterfaceCore
