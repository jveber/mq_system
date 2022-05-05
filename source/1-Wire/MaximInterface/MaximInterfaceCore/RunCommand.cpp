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

#include "Crc.hpp"
#include "Error.hpp"
#include "I2CMaster.hpp"
#include "OneWireMaster.hpp"
#include "RunCommand.hpp"
#include "Sleep.hpp"

namespace MaximInterfaceCore {

const error_category & RunCommandWithOneWireMaster::errorCategory() {
  static class : public error_category {
  public:
    virtual const char * name() const { return "RunCommandWithOneWireMaster"; }

    virtual std::string message(int condition) const {
      switch (condition) {
      case CrcError:
        return "CRC Error";

      case InvalidResponseError:
        return "Invalid Response Error";
      }
      return defaultErrorMessage(condition);
    }
  } instance;
  return instance;
}

Result<span<uint_least8_t> > RunCommandWithOneWireMaster::
operator()(span<const uint_least8_t> request, int delayTime,
           span<uint_least8_t> response) const {
  // Write request.
  Result<void> result = selectRom(*master);
  if (!result) {
    return result.error();
  }
  uint_least8_t xpcBuffer[2] = {0x66,
                                static_cast<uint_least8_t>(request.size())};
  result = master->writeBlock(xpcBuffer);
  if (!result) {
    return result.error();
  }
  result = master->writeBlock(request);
  if (!result) {
    return result.error();
  }
  uint_fast16_t expectedCrc =
      calculateCrc16(calculateCrc16(xpcBuffer), request) ^ 0xFFFF;
  result = master->readBlock(xpcBuffer);
  if (!result) {
    return result.error();
  }
  if (expectedCrc !=
      ((static_cast<uint_fast16_t>(xpcBuffer[1]) << 8) | xpcBuffer[0])) {
    return CrcError;
  }
  result = master->writeBytePower(0xAA);
  if (!result) {
    return result.error();
  }

  // Wait for device to process.
  sleep->invoke(delayTime);

  // Read response.
  result = master->setLevel(OneWireMaster::NormalLevel);
  if (!result) {
    return result.error();
  }
  result = master->readBlock(xpcBuffer);
  if (!result) {
    return result.error();
  }
  if (xpcBuffer[1] > response.size()) {
    return InvalidResponseError;
  }
  response = response.first(xpcBuffer[1]);
  result = master->readBlock(response);
  if (!result) {
    return result.error();
  }
  expectedCrc =
      calculateCrc16(calculateCrc16(make_span(xpcBuffer + 1, 1)), response) ^
      0xFFFF;
  result = master->readBlock(xpcBuffer);
  if (!result) {
    return result.error();
  }
  if (expectedCrc !=
      ((static_cast<uint_fast16_t>(xpcBuffer[1]) << 8) | xpcBuffer[0])) {
    return CrcError;
  }
  return response;
}

const error_category & RunCommandWithI2CMaster::errorCategory() {
  static class : public error_category {
  public:
    virtual const char * name() const { return "RunCommandWithI2CMaster"; }

    virtual std::string message(int condition) const {
      switch (condition) {
      case InvalidResponseError:
        return "Invalid Response Error";
      }
      return defaultErrorMessage(condition);
    }
  } instance;
  return instance;
}

Result<span<uint_least8_t> > RunCommandWithI2CMaster::
operator()(span<const uint_least8_t> request, int delayTime,
           span<uint_least8_t> response) const {
  // Write request.
  Result<void> result = master->start(address_);
  if (!result && result.error() == make_error_condition(I2CMaster::NackError) &&
      address_ != 0) {
    result = master->start(0);
  }
  if (!result) {
    master->stop();
    return result.error();
  }
  if (!request.empty()) {
    result = master->writeByte(request[0]);
    if (!result) {
      master->stop();
      return result.error();
    }
    request = request.subspan(1);
    if (!request.empty()) {
      result = master->writeByte(static_cast<uint_least8_t>(request.size()));
      if (!result) {
        master->stop();
        return result.error();
      }
      result = master->writeBlock(request);
      if (!result) {
        master->stop();
        return result.error();
      }
    }
  }
  result = master->stop();
  if (!result) {
    return result.error();
  }

  // Wait for device to process.
  sleep->invoke(delayTime);

  // Read response.
  result = master->start(address_ | 1);
  if (!result) {
    master->stop();
    return result.error();
  }
  uint_least8_t length;
  if (const Result<uint_least8_t> result = master->readByte(I2CMaster::Ack)) {
    length = result.value();
  } else {
    master->stop();
    return result.error();
  }
  if (length > response.size()) {
    master->stop();
    return InvalidResponseError;
  }
  response = response.first(length);
  result = master->readBlock(response, I2CMaster::Nack);
  if (!result) {
    master->stop();
    return result.error();
  }
  result = master->stop();
  if (!result) {
    return result.error();
  }
  return response;
}

} // namespace MaximInterfaceCore
