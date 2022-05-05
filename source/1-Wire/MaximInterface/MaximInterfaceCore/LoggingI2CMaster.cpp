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

#include <utility>
#include "HexString.hpp"
#include "LoggingI2CMaster.hpp"

using std::string;

namespace MaximInterfaceCore {

static const char startString[] = "S ";
static const char stopString[] = "P";

static string formatDataString(span<const uint_least8_t> data, bool read) {
  string dataBuilder;
  for (span<const uint_least8_t>::index_type i = 0; i < data.size(); ++i) {
    if (read) {
      dataBuilder.append(1, '[');
    }
    dataBuilder.append(toHexString(data.subspan(i, 1)));
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

Result<void> LoggingI2CMaster::start(uint_least8_t address) {
  messageBuilder.append(startString);
  messageBuilder.append(formatDataString(make_span(&address, 1), false));
  return I2CMasterDecorator::start(address);
}

Result<void> LoggingI2CMaster::stop() {
  messageBuilder.append(stopString);
  tryWriteMessage();
  return I2CMasterDecorator::stop();
}

Result<void> LoggingI2CMaster::writeByte(uint_least8_t data) {
  messageBuilder.append(formatDataString(make_span(&data, 1), false));
  return I2CMasterDecorator::writeByte(data);
}

Result<void> LoggingI2CMaster::writeBlock(span<const uint_least8_t> data) {
  messageBuilder.append(formatDataString(data, false));
  return I2CMasterDecorator::writeBlock(data);
}

Result<void> LoggingI2CMaster::writePacket(uint_least8_t address,
                                           span<const uint_least8_t> data,
                                           DoStop doStop) {
  messageBuilder.append(startString);
  messageBuilder.append(formatDataString(make_span(&address, 1), false));
  const Result<void> result =
      I2CMasterDecorator::writePacket(address, data, doStop);
  if (result) {
    messageBuilder.append(formatDataString(data, false));
  }
  if (doStop == Stop || (doStop == StopOnError && !result)) {
    messageBuilder.append(stopString);
    tryWriteMessage();
  }
  return result;
}

Result<uint_least8_t> LoggingI2CMaster::readByte(DoAck doAck) {
  const Result<uint_least8_t> data = I2CMasterDecorator::readByte(doAck);
  if (data) {
    messageBuilder.append(formatDataString(make_span(&data.value(), 1), true));
  }
  return data;
}

Result<void> LoggingI2CMaster::readBlock(span<uint_least8_t> data,
                                         DoAck doAck) {
  const Result<void> result = I2CMasterDecorator::readBlock(data, doAck);
  if (result) {
    messageBuilder.append(formatDataString(data, true));
  }
  return result;
}

Result<void> LoggingI2CMaster::readPacket(uint_least8_t address,
                                          span<uint_least8_t> data,
                                          DoStop doStop) {
  messageBuilder.append(startString);
  messageBuilder.append(formatDataString(make_span(&address, 1), false));
  const Result<void> result =
      I2CMasterDecorator::readPacket(address, data, doStop);
  if (result) {
    messageBuilder.append(formatDataString(data, true));
  }
  if (doStop == Stop || (doStop == StopOnError && !result)) {
    messageBuilder.append(stopString);
    tryWriteMessage();
  }
  return result;
}

} // namespace MaximInterfaceCore
