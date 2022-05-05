/*******************************************************************************
* Copyright (C) Maxim Integrated Products, Inc., All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
* OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of Maxim Integrated
* Products, Inc. shall not be used except as stated in the Maxim Integrated
* Products, Inc. Branding Policy.
*
* The mere transfer of this software does not imply any licenses
* of trade secrets, proprietary technology, copyrights, patents,
* trademarks, maskwork rights, or any other form of intellectual
* property whatsoever. Maxim Integrated Products, Inc. retains all
* ownership rights.
*******************************************************************************/

#ifndef MaximInterfaceMbed_I2CMaster_hpp
#define MaximInterfaceMbed_I2CMaster_hpp

#include <MaximInterfaceCore/I2CMaster.hpp>
#include <mbed-os/drivers/I2C.h>

namespace MaximInterfaceMbed {

/// Wrapper for mbed::I2C.
class I2CMaster : public MaximInterfaceCore::I2CMaster {
public:
  explicit I2CMaster(mbed::I2C & i2c) : i2c(&i2c) {}

  void setI2C(mbed::I2C & i2c) { this->i2c = &i2c; }

  virtual MaximInterfaceCore::Result<void> start(uint_least8_t address);

  virtual MaximInterfaceCore::Result<void> stop();

  virtual MaximInterfaceCore::Result<void> writeByte(uint_least8_t data);

  virtual MaximInterfaceCore::Result<void>
  writePacket(uint_least8_t address,
              MaximInterfaceCore::span<const uint_least8_t> data,
              DoStop doStop);

  virtual MaximInterfaceCore::Result<uint_least8_t> readByte(DoAck doAck);

  virtual MaximInterfaceCore::Result<void>
  readPacket(uint_least8_t address,
             MaximInterfaceCore::span<uint_least8_t> data, DoStop doStop);

private:
  mbed::I2C * i2c;
};

} // namespace MaximInterfaceMbed

#endif
