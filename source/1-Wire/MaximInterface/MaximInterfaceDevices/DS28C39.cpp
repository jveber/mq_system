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
#include "DS28C39.hpp"

namespace MaximInterfaceDevices {

using namespace Core;
using std::copy;

static const int readMemoryTimeMs = 30;
static const int writeMemoryTimeMs = 65;
static const int writeStateTimeMs = 15;
static const int generateEccKeyPairTimeMs = 200;
static const int generateEcdsaSignatureTimeMs = 130;
static const int trngOnDemandCheckTimeMs = 20;
static const int trngGenerationTimeMs = 10;
static const int verifyEcdsaSignatureTimeMs = 180;

const int DS28C39::authorityPublicKeyXPage;
const int DS28C39::authorityPublicKeyYPage;
const int DS28C39::writePublicKeyXPage;
const int DS28C39::writePublicKeyYPage;
const int DS28C39::memoryPages;

Result<void> DS28C39::writeMemory(int pageNum, Page::const_span page) {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return InvalidParameterError;
  }

  uint_least8_t request[2 + Page::size];
  request[0] = 0x96;
  request[1] = pageNum;
  copy(page.begin(), page.end(), request + 2);
  return runCommand(request, writeMemoryTimeMs);
}

Result<DS28C39::Page::array> DS28C39::readMemory(int pageNum) const {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return InvalidParameterError;
  }

  uint_least8_t buffer[1 + Page::size];
  buffer[0] = 0x44;
  buffer[1] = pageNum;
  const Result<span<uint_least8_t> > response =
      runCommand(make_span(buffer, 2), readMemoryTimeMs, buffer);
  if (!response) {
    return response.error();
  }
  Page::array page;
  copy(response.value().begin(), response.value().end(), page.begin());
  return page;
}

Result<DS28C39::Status> DS28C39::readStatus(bool entropyHealthTest) const {
  int delay = readMemoryTimeMs;
  if (entropyHealthTest) {
    delay += trngOnDemandCheckTimeMs;
  }
  uint_least8_t buffer[Status::PageProtectionList::size + RomId::size +
                       ManId::size + Status::RomVersion::size + 2];
  buffer[0] = 0xAA;
  buffer[1] = entropyHealthTest ? 0x01 : 0x00;
  const Result<span<uint_least8_t> > response =
      runCommand(make_span(buffer, 2), delay, buffer);
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
  span<uint_least8_t>::const_iterator responseItEnd =
      responseIt + status.romId.size();
  copy(responseIt, responseItEnd, status.romId.begin());
  responseIt = responseItEnd;
  responseItEnd = responseIt + status.manId.size();
  copy(responseIt, responseItEnd, status.manId.begin());
  responseIt = responseItEnd;
  responseItEnd = responseIt + status.romVersion.size();
  copy(responseIt, responseItEnd, status.romVersion.begin());
  responseIt = responseItEnd;
  switch (*responseIt) {
  case Status::TestNotPerformed:
  case Status::EntropyHealthy:
  case Status::EntropyNotHealthy:
    status.entropyHealthTestStatus =
        static_cast<Status::EntropyHealthTestStatus>(*responseIt);
    break;

  default:
    return InvalidResponseError;
  }
  return status;
}

Result<void> DS28C39::setPageProtection(int pageNum,
                                        const PageProtection & protection) {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return InvalidParameterError;
  }

  const uint_least8_t request[] = {
      0xC3, static_cast<uint_least8_t>(pageNum),
      static_cast<uint_least8_t>(protection.to_ulong())};
  return runCommand(request, writeStateTimeMs);
}

Result<Ecc256::Signature::array>
DS28C39::computeAndReadPageAuthentication(int pageNum, bool anonymous,
                                          Page::const_span challenge) const {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return InvalidParameterError;
  }

  const size_t requestSize = 2 + Page::size;
  const size_t responseSize = 1 + 2 * Ecc256::Scalar::size;
  uint_least8_t buffer[MaximInterfaceCore_MAX(requestSize, responseSize)];
  buffer[0] = 0xA5;
  buffer[1] = pageNum | (anonymous ? 0xE0 : 0x00);
  copy(challenge.begin(), challenge.end(), buffer + 2);
  const Result<span<uint_least8_t> > response =
      runCommand(make_span(buffer, requestSize), generateEcdsaSignatureTimeMs,
                 make_span(buffer, responseSize));
  if (!response) {
    return response.error();
  }
  Ecc256::Signature::array signature;
  span<uint_least8_t>::const_iterator begin = response.value().begin();
  span<uint_least8_t>::const_iterator end = begin + signature.s.size();
  copy(begin, end, signature.s.begin());
  begin = end;
  end = begin + signature.r.size();
  copy(begin, end, signature.r.begin());
  return signature;
}

Result<void> DS28C39::disableDevice() {
  const uint_least8_t request[] = {0x33, 0x9E, 0xA7, 0x49, 0xFB,
                                   0x10, 0x62, 0x0A, 0x26};
  return runCommand(request, writeStateTimeMs);
}

Result<Ecc256::PublicKey::array> DS28C39::readDevicePublicKey() const {
  uint_least8_t buffer[1 + 2 * Ecc256::Scalar::size];
  buffer[0] = 0xCB;
  const Result<span<uint_least8_t> > response =
      runCommand(make_span(buffer, 1), generateEccKeyPairTimeMs, buffer);
  if (!response) {
    return response.error();
  }
  Ecc256::PublicKey::array devicePublicKey;
  span<uint_least8_t>::const_iterator begin = response.value().begin();
  span<uint_least8_t>::const_iterator end = begin + devicePublicKey.x.size();
  copy(begin, end, devicePublicKey.x.begin());
  begin = end;
  end = begin + devicePublicKey.y.size();
  copy(begin, end, devicePublicKey.y.begin());
  return devicePublicKey;
}

Result<void> DS28C39::readRng(span<uint_least8_t> data) const {
  const span<uint_least8_t>::index_type maxDataSize = 64;
  if ((data.size() < 1) || (data.size() > maxDataSize)) {
    return InvalidParameterError;
  }

  uint_least8_t buffer[1 + maxDataSize];
  buffer[0] = 0xD2;
  buffer[1] = static_cast<uint_least8_t>(data.size() - 1);
  const Result<span<uint_least8_t> > response =
      runCommand(make_span(buffer, 2), trngGenerationTimeMs,
                 make_span(buffer, 1 + data.size()));
  if (!response) {
    return response.error();
  }
  copy(response.value().begin(), response.value().end(), data.begin());
  return none;
}

Result<void>
DS28C39::authenticatePublicKey(Ecc256::Signature::const_span certificate,
                               span<const uint_least8_t> customization) {
  static const span<const uint_least8_t>::index_type maxCustomizationSize = 32;
  static const span<const uint_least8_t>::index_type signatureSize =
      2 * Ecc256::Scalar::size;

  if (customization.size() < 1 || customization.size() > maxCustomizationSize) {
    return InvalidParameterError;
  }

  uint_least8_t request[1 + signatureSize + maxCustomizationSize];
  uint_least8_t * requestIt = request;
  *requestIt++ = 0x59;
  requestIt = copy(certificate.r.begin(), certificate.r.end(), requestIt);
  requestIt = copy(certificate.s.begin(), certificate.s.end(), requestIt);
  requestIt = copy(customization.begin(), customization.end(), requestIt);
  return runCommand(make_span(request, requestIt), verifyEcdsaSignatureTimeMs);
}

Result<void>
DS28C39::authenticatedWriteMemory(int pageNum, Page::const_span page,
                                  Ecc256::Signature::const_span signature) {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return InvalidParameterError;
  }

  uint_least8_t request[2 + Page::size + 2 * Ecc256::Scalar::size];
  uint_least8_t * requestIt = request;
  *requestIt++ = 0x89;
  *requestIt++ = pageNum;
  requestIt = copy(page.begin(), page.end(), requestIt);
  requestIt = copy(signature.r.begin(), signature.r.end(), requestIt);
  copy(signature.s.begin(), signature.s.end(), requestIt);
  return runCommand(request, verifyEcdsaSignatureTimeMs + writeMemoryTimeMs);
}

Result<span<uint_least8_t> >
DS28C39::runCommand(span<const uint_least8_t> request, int delayTime,
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

Result<void> DS28C39::runCommand(span<const uint_least8_t> request,
                                 int delayTime) {
  uint_least8_t buffer;
  MaximInterfaceCore_TRY(runCommand(request, delayTime, make_span(&buffer, 1)));
  return none;
}

const error_category & DS28C39::errorCategory() {
  static class : public error_category {
  public:
    virtual const char * name() const { return "DS28C39"; }

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

DS28C39::PageAuthenticationData &
DS28C39::PageAuthenticationData::setAnonymousRomId() {
  std::fill(romId().begin(), romId().end(), 0xFF);
  return *this;
}

} // namespace MaximInterfaceDevices
