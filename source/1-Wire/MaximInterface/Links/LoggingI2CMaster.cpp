/*******************************************************************************
* Copyright (C) 2017 Maxim Integrated Products, Inc., All Rights Reserved.
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

#include <utility>
#include <MaximInterface/Utilities/HexConversions.hpp>
#include "LoggingI2CMaster.hpp"

using std::string;

namespace MaximInterface {

static const char startString[] = "S ";
static const char stopString[] = "P";

static string formatDataString(const uint_least8_t * data, size_t dataSize,
                               bool read) {
  string dataBuilder;
  for (size_t i = 0; i < dataSize; i++) {
    if (read) {
      dataBuilder.append(1, '[');
    }
    dataBuilder.append(byteArrayToHexString(data + i, 1));
    if (read) {
      dataBuilder.append(1, ']');
    }
    dataBuilder.append(1, ' ');
  }
  return dataBuilder;
}

void LoggingI2CMaster::tryWriteMessage() {
  if (writeMessage) {
    writeMessage(messageBuilder);
  }
  messageBuilder.clear();
}

error_code LoggingI2CMaster::start(uint_least8_t address) {
  messageBuilder.append(startString);
  messageBuilder.append(formatDataString(&address, 1, false));
  return I2CMasterDecorator::start(address);
}

error_code LoggingI2CMaster::stop() {
  messageBuilder.append(stopString);
  tryWriteMessage();
  return I2CMasterDecorator::stop();
}

error_code LoggingI2CMaster::writeByte(uint_least8_t data) {
  messageBuilder.append(formatDataString(&data, 1, false));
  return I2CMasterDecorator::writeByte(data);
}

error_code LoggingI2CMaster::writeBlock(const uint_least8_t * data,
                                        size_t dataLen) {
  messageBuilder.append(formatDataString(data, dataLen, false));
  return I2CMasterDecorator::writeBlock(data, dataLen);
}

error_code LoggingI2CMaster::writePacketImpl(uint_least8_t address,
                                             const uint_least8_t * data,
                                             size_t dataLen, bool sendStop) {
  messageBuilder.append(startString);
  messageBuilder.append(formatDataString(&address, 1, false));
  error_code result =
      I2CMasterDecorator::writePacketImpl(address, data, dataLen, sendStop);
  if (!result) {
    messageBuilder.append(formatDataString(data, dataLen, false));
  }
  if (sendStop || result) {
    messageBuilder.append(stopString);
    tryWriteMessage();
  }
  return result;
}

error_code LoggingI2CMaster::readByte(AckStatus status, uint_least8_t & data) {
  error_code result = I2CMasterDecorator::readByte(status, data);
  if (!result) {
    messageBuilder.append(formatDataString(&data, 1, true));
  }
  return result;
}

error_code LoggingI2CMaster::readBlock(AckStatus status, uint_least8_t * data,
                                       size_t dataLen) {
  error_code result = I2CMasterDecorator::readBlock(status, data, dataLen);
  if (!result) {
    messageBuilder.append(formatDataString(data, dataLen, true));
  }
  return result;
}

error_code LoggingI2CMaster::readPacketImpl(uint_least8_t address,
                                            uint_least8_t * data,
                                            size_t dataLen, bool sendStop) {
  messageBuilder.append(startString);
  messageBuilder.append(formatDataString(&address, 1, false));
  error_code result =
      I2CMasterDecorator::readPacketImpl(address, data, dataLen, sendStop);
  if (!result) {
    messageBuilder.append(formatDataString(data, dataLen, true));
  }
  if (sendStop || result) {
    messageBuilder.append(stopString);
    tryWriteMessage();
  }
  return result;
}

} // namespace MaximInterface