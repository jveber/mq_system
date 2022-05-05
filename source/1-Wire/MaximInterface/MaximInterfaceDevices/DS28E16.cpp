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

#include <stddef.h>
#include <algorithm>
#include <MaximInterfaceCore/Error.hpp>
#include "DS28E16.hpp"

namespace MaximInterfaceDevices {

using namespace Core;
using std::copy;
using std::fill;

static const int readMemoryTimeMs = 5;
static const int writeMemoryTimeMs = 60;
static const int shortWriteMemoryTimeMs = 15;
static const int computationTimeMs = 15;

const int DS28E16::decrementCounterPage;
const int DS28E16::masterSecretPage;
const int DS28E16::memoryPages;

Result<void> DS28E16::writeMemory(int pageNum, Page::const_span page) {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return InvalidParameterError;
  }

  uint_least8_t request[2 + Page::size];
  request[0] = 0x96;
  request[1] = pageNum;
  copy(page.begin(), page.end(), request + 2);
  return runCommand(request, writeMemoryTimeMs);
}

Result<DS28E16::Page::array> DS28E16::readMemory(int pageNum) const {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return InvalidParameterError;
  }

  uint_least8_t buffer[1 + Page::size * 2];
  buffer[0] = 0x44;
  buffer[1] = pageNum;
  Result<span<uint_least8_t> > response =
      runCommand(make_span(buffer, 2), readMemoryTimeMs, buffer);
  if (!response) {
    return response.error();
  }
  Page::array page;
  response.value() = response.value().first(Page::size);
  copy(response.value().begin(), response.value().end(), page.begin());
  return page;
}

Result<DS28E16::Status> DS28E16::readStatus() const {
  uint_least8_t buffer[1 + Status::PageProtectionList::size + 2];
  buffer[0] = 0xAA;
  const Result<span<uint_least8_t> > response =
      runCommand(make_span(buffer, 1), readMemoryTimeMs, buffer);
  if (!response) {
    return response.error();
  }
  Status status;
  span<uint_least8_t>::const_iterator responseIt = response.value().begin();
  for (Status::PageProtectionList::array::iterator it =
           status.pageProtection.begin();
       it != status.pageProtection.end(); ++it) {
    *it = *responseIt;
    ++responseIt;
  }
  status.manId = *responseIt;
  ++responseIt;
  status.deviceVersion = *responseIt;
  return status;
}

Result<void> DS28E16::setPageProtection(int pageNum,
                                        const PageProtection & protection) {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return InvalidParameterError;
  }

  const uint_least8_t request[] = {
      0xC3, static_cast<uint_least8_t>(pageNum),
      static_cast<uint_least8_t>(protection.to_ulong())};
  return runCommand(request, shortWriteMemoryTimeMs);
}

Result<DS28E16::DoublePage::array> DS28E16::computeAndReadPageAuthentication(
    int pageNum, bool anonymous, DoublePage::const_span challenge) const {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return InvalidParameterError;
  }

  const size_t requestSize = 3 + DoublePage::size;
  const size_t responseSize = 1 + DoublePage::size;
  uint_least8_t buffer[MaximInterfaceCore_MAX(requestSize, responseSize)];
  buffer[0] = 0xA5;
  buffer[1] = pageNum;
  if (anonymous) {
    buffer[1] |= 0xE0;
  }
  buffer[2] = 0x02;
  copy(challenge.begin(), challenge.end(), buffer + 3);
  const Result<span<uint_least8_t> > response =
      runCommand(make_span(buffer, requestSize), computationTimeMs,
                 make_span(buffer, responseSize));
  if (!response) {
    return response.error();
  }
  DoublePage::array hmac;
  copy(response.value().begin(), response.value().end(), hmac.begin());
  return hmac;
}

Result<void> DS28E16::computeSecret(int bindingDataPageNum,
                                    bool constantBindingData, bool anonymous,
                                    DoublePage::const_span partialSecret) {
  if (bindingDataPageNum < 0 || bindingDataPageNum >= memoryPages) {
    return InvalidParameterError;
  }

  uint_least8_t request[3 + DoublePage::size];
  request[0] = 0x3C;
  request[1] = bindingDataPageNum;
  if (constantBindingData) {
    request[1] |= 0x04;
  }
  if (anonymous) {
    request[1] |= 0xE0;
  }
  request[2] = 0x08;
  copy(partialSecret.begin(), partialSecret.end(), request + 3);
  return runCommand(request, computationTimeMs);
}

Result<void> DS28E16::decrementCounter() {
  const uint_least8_t request = 0xC9;
  return runCommand(make_span(&request, 1), writeMemoryTimeMs);
}

Result<void> DS28E16::lockOutDisableDevice() {
  const DisableDevicePassword::array password = {0};
  return disableDevice(LockOutDisableDevice, password);
}

Result<void>
DS28E16::setDisableDevicePassword(DisableDevicePassword::const_span password) {
  return disableDevice(SetDisableDevicePassword, password);
}

Result<void>
DS28E16::disableDevice(DisableDevicePassword::const_span password) {
  return disableDevice(DisableDevice, password);
}

Result<void>
DS28E16::disableDevice(DisableDeviceOperation operation,
                       DisableDevicePassword::const_span password) {
  const uint_least8_t request[] = {
      0x33,        static_cast<uint_least8_t>(operation),
      password[0], password[1],
      0x71,        0x35,
      0x0E,        0xAC,
      0x95,        0xF8};
  return runCommand(request, shortWriteMemoryTimeMs);
}

Result<span<uint_least8_t> >
DS28E16::runCommand(span<const uint_least8_t> request, int delayTime,
                    span<uint_least8_t> response) const {
  const Result<span<uint_least8_t> > responseOutput =
      doRunCommand(request, delayTime, response);
  if (!responseOutput) {
    return responseOutput;
  }
  if (responseOutput.value().empty()) {
    return InvalidResponseError;
  }
  // Parse command result byte.
  switch (responseOutput.value().front()) {
  case 0xAA:
    // Success response.
    break;

  case 0x00:
    return AuthenticationError;

  default:
    return error_code(responseOutput.value().front(), errorCategory());
  }
  if (responseOutput.value().size() != response.size()) {
    return InvalidResponseError;
  }
  return responseOutput.value().subspan(1);
}

Result<void> DS28E16::runCommand(span<const uint_least8_t> request,
                                 int delayTime) {
  uint_least8_t buffer;
  MaximInterfaceCore_TRY(runCommand(request, delayTime, make_span(&buffer, 1)));
  return none;
}

const error_category & DS28E16::errorCategory() {
  static class : public error_category {
  public:
    virtual const char * name() const { return "DS28E16"; }

    virtual std::string message(int condition) const {
      switch (condition) {
      case InvalidOperationError:
        return "Invalid Operation Error";

      case InvalidParameterError:
        return "Invalid Parameter Error";

      case InvalidSequenceError:
        return "Invalid Sequence Error";

      case InternalError:
        return "Internal Error";

      case DeviceDisabledError:
        return "Device Disabled Error";

      case AuthenticationError:
        return "Authentication Error";

      case InvalidResponseError:
        return "Invalid Response Error";
      }
      return defaultErrorMessage(condition);
    }
  } instance;
  return instance;
}

DS28E16::PageAuthenticationData &
DS28E16::PageAuthenticationData::setAnonymousRomId() {
  fill(romId().begin(), romId().end(), 0xFF);
  return *this;
}

DS28E16::ComputeSecretData &
DS28E16::ComputeSecretData::setConstantBindingData(bool constantBindingData) {
  if (constantBindingData) {
    data.setPageNum(data.pageNum() | constantBindingDataMask);
    fill(bindingData().begin(), bindingData().end(), 0);
  } else {
    data.setPageNum(data.pageNum() & ~constantBindingDataMask);
  }
  return *this;
}

} // namespace MaximInterfaceDevices
