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

#include "Error.hpp"
#include "I2CMaster.hpp"

namespace MaximInterfaceCore {

Result<void> I2CMaster::writeBlock(span<const uint_least8_t> data) {
  for (span<const uint_least8_t>::index_type i = 0; i < data.size(); ++i) {
    MaximInterfaceCore_TRY(writeByte(data[i]));
  }
  return none;
}

Result<void> I2CMaster::writePacket(uint_least8_t address,
                                    span<const uint_least8_t> data,
                                    DoStop doStop) {
  Result<void> result = start(address & 0xFE);
  if (result) {
    result = writeBlock(data);
  }
  if (doStop == Stop || (doStop == StopOnError && !result)) {
    const Result<void> stopResult = stop();
    if (result) {
      result = stopResult;
    }
  }
  return result;
}

Result<void> I2CMaster::readBlock(span<uint_least8_t> data, DoAck doAck) {
  for (span<uint_least8_t>::index_type i = 0; i < data.size(); ++i) {
    MaximInterfaceCore_TRY_VALUE(
        data[i], readByte(i == (data.size() - 1) ? doAck : Ack));
  }
  return none;
}

Result<void> I2CMaster::readPacket(uint_least8_t address,
                                   span<uint_least8_t> data, DoStop doStop) {
  Result<void> result = start(address | 0x01);
  if (result) {
    result = readBlock(data, Nack);
  }
  if (doStop == Stop || (doStop == StopOnError && !result)) {
    const Result<void> stopResult = stop();
    if (result) {
      result = stopResult;
    }
  }
  return result;
}

const error_category & I2CMaster::errorCategory() {
  static class : public error_category {
  public:
    virtual const char * name() const { return "I2CMaster"; }

    virtual std::string message(int condition) const {
      switch (condition) {
      case NackError:
        return "Nack Error";
      }
      return defaultErrorMessage(condition);
    }
  } instance;
  return instance;
}

} // namespace MaximInterfaceCore
