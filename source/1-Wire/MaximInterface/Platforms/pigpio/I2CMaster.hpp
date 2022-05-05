// Copyright: (c) Jaromir Veber 2017-2018
// Version: 08102018
// License: MPL-2.0
// *******************************************************************************
//  This Source Code Form is subject to the terms of the Mozilla Public
//  License, v. 2.0. If a copy of the MPL was not distributed with this
//  file, You can obtain one at http ://mozilla.org/MPL/2.0/.
// *******************************************************************************

#pragma once

#include <pigpiod_if2.h>            // GPIO connector
#include <MaximInterfaceCore/I2CMaster.hpp>

namespace MaximInterfaceCore {

class piI2CMaster : public I2CMaster {
public:

  piI2CMaster(): _pigpio_handle(-1), _device_handle(-1) {}

  virtual Result<void> start(uint_least8_t address);

  virtual Result<void> stop();

  virtual Result<void> writeByte(uint_least8_t data);

  virtual Result<uint_least8_t> readByte(DoAck doAck);

  virtual Result<void> writePacket(uint_least8_t address, span<const uint_least8_t> data, DoStop doStop);
  virtual Result<void> readPacket(uint_least8_t address, span<uint_least8_t> data, DoStop doStop);

private:
  int _pigpio_handle;
  int _device_handle;
};

} // namespace MaximInterfaceCore
