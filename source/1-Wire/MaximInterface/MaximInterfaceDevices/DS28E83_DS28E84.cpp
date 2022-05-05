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
#include "DS28E83_DS28E84.hpp"

namespace MaximInterfaceDevices {

using namespace Core;
using std::copy;
using std::pair;

static const int readMemoryTimeMs = 2;
static const int writeMemoryTimeMs = 100;
static const int writeStateTimeMs = 15;
static const int generateEccKeyPairTimeMs = 350;
static const int generateEcdsaSignatureTimeMs = 80;
static const int computeTimeMs = 4;
static const int verifyEcdsaTimeMs = 160;
static const int trngGenerationTimeMs = 40;
static const int trngOnDemandCheckTimeMs = 65;

static const uint_least8_t computeAndReadPageAuthenticationCmd = 0xA5;
static const uint_least8_t readRngCmd = 0xD2;

static const int memoryPages =
    MaximInterfaceCore_MAX(DS28E83::memoryPages, DS28E84::memoryPages);
static const int protectionBlocks = MaximInterfaceCore_MAX(
    DS28E83::protectionBlocks, DS28E84::protectionBlocks);

const int DS28E83_DS28E84::publicKeyAxPage;
const int DS28E83_DS28E84::publicKeyAyPage;
const int DS28E83_DS28E84::publicKeyBxPage;
const int DS28E83_DS28E84::publicKeyByPage;
const int DS28E83_DS28E84::authorityPublicKeyAxPage;
const int DS28E83_DS28E84::authorityPublicKeyAyPage;
const int DS28E83_DS28E84::authorityPublicKeyBxPage;
const int DS28E83_DS28E84::authorityPublicKeyByPage;
const int DS28E83_DS28E84::privateKeyAPage;
const int DS28E83_DS28E84::privateKeyBPage;
const int DS28E83_DS28E84::secretAPage;
const int DS28E83_DS28E84::secretBPage;
const int DS28E83_DS28E84::romOptionsPage;
const int DS28E83_DS28E84::gpioControlPage;
const int DS28E83_DS28E84::publicKeySxPage;
const int DS28E83_DS28E84::publicKeySyPage;

const int DS28E83::memoryPages;
const int DS28E83::protectionBlocks;

const int DS28E84::publicKeySxBackupPage;
const int DS28E84::publicKeySyBackupPage;
const int DS28E84::decrementCounterPage;
const int DS28E84::memoryPages;
const int DS28E84::protectionBlocks;

Result<void> DS28E83_DS28E84::writeMemory(int pageNum, Page::const_span page) {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return InvalidParameterError;
  }

  uint_least8_t request[2 + Page::size];
  request[0] = 0x96;
  request[1] = pageNum;
  copy(page.begin(), page.end(), request + 2);
  return runCommand(request, writeMemoryTimeMs);
}

Result<DS28E83_DS28E84::Page::array>
DS28E83_DS28E84::readMemory(int pageNum) const {
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

Result<pair<DS28E83_DS28E84::EncryptionChallenge::array,
            DS28E83_DS28E84::Page::array> >
DS28E83_DS28E84::encryptedReadMemory(int pageNum, KeySecret secret) const {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return InvalidParameterError;
  }

  const size_t requestSize = 3;
  const size_t responseSize = 1 + EncryptionChallenge::size + Page::size;
  uint_least8_t buffer[MaximInterfaceCore_MAX(requestSize, responseSize)];
  buffer[0] = 0x4B;
  buffer[1] = pageNum;
  buffer[2] = secret;
  const Result<span<uint_least8_t> > response = runCommand(
      make_span(buffer, requestSize), readMemoryTimeMs + computeTimeMs,
      make_span(buffer, responseSize));
  if (!response) {
    return response.error();
  }
  pair<EncryptionChallenge::array, Page::array> data;
  span<uint_least8_t>::const_iterator begin = response.value().begin();
  span<uint_least8_t>::const_iterator end = begin + data.first.size();
  copy(begin, end, data.first.begin());
  begin = end;
  end = begin + data.second.size();
  copy(begin, end, data.second.begin());
  return data;
}

Result<pair<Optional<DS28E83_DS28E84::KeySecret>,
            DS28E83_DS28E84::BlockProtection> >
DS28E83_DS28E84::readBlockProtection(int blockNumber) const {
  if (blockNumber < 0 || blockNumber >= protectionBlocks) {
    return InvalidParameterError;
  }

  const size_t requestSize = 2;
  const size_t responseSize = 3;
  uint_least8_t buffer[MaximInterfaceCore_MAX(requestSize, responseSize)];
  buffer[0] = 0xAA;
  buffer[1] = blockNumber;
  const Result<span<uint_least8_t> > response =
      runCommand(make_span(buffer, requestSize), readMemoryTimeMs,
                 make_span(buffer, responseSize));
  if (!response) {
    return response.error();
  }
  if ((response.value()[0] & 0x3F) != blockNumber) {
    return InvalidResponseError;
  }
  pair<Optional<KeySecret>, BlockProtection> data;
  switch (response.value()[0] >> 6) {
  case 0:
    data.first = none;
    break;
  case 1:
    data.first = KeySecretA;
    break;
  case 2:
    data.first = KeySecretB;
    break;
  default:
    return InvalidResponseError;
  }
  if ((response.value()[1] & 0x20) != 0) {
    return InvalidResponseError;
  }
  data.second = response.value()[1];
  return data;
}

Result<void>
DS28E83_DS28E84::setBlockProtection(int blockNum, KeySecret keySecret,
                                    const BlockProtection & protection) {
  if (blockNum < 0 || blockNum >= protectionBlocks || keySecret == KeySecretS) {
    return InvalidParameterError;
  }

  const uint_least8_t request[] = {
      0xC3,
      static_cast<uint_least8_t>((keySecret == KeySecretB ? 0x80 : 0x40) |
                                 blockNum),
      static_cast<uint_least8_t>(protection.to_ulong())};
  return runCommand(request, writeStateTimeMs);
}

Result<Ecc256::Signature::array>
DS28E83_DS28E84::computeAndReadEcdsaPageAuthentication(
    int pageNum, KeySecret key, Page::const_span challenge) const {
  if (pageNum < 0 || pageNum >= memoryPages || key == KeySecretS) {
    return InvalidParameterError;
  }

  const size_t requestSize = 3 + Page::size;
  const size_t responseSize = 1 + 2 * Ecc256::Scalar::size;
  uint_least8_t buffer[MaximInterfaceCore_MAX(requestSize, responseSize)];
  buffer[0] = computeAndReadPageAuthenticationCmd;
  buffer[1] = pageNum;
  buffer[2] = key + 3;
  copy(challenge.begin(), challenge.end(), buffer + 3);
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

Result<DS28E83_DS28E84::Page::array>
DS28E83_DS28E84::computeAndReadSha256PageAuthentication(
    int pageNum, KeySecret secret, Page::const_span challenge) const {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return InvalidParameterError;
  }

  const size_t requestSize = 3 + Page::size;
  const size_t responseSize = 1 + Page::size;
  uint_least8_t buffer[MaximInterfaceCore_MAX(requestSize, responseSize)];
  buffer[0] = computeAndReadPageAuthenticationCmd;
  buffer[1] = pageNum;
  buffer[2] = secret;
  copy(challenge.begin(), challenge.end(), buffer + 3);
  const Result<span<uint_least8_t> > response =
      runCommand(make_span(buffer, requestSize), computeTimeMs,
                 make_span(buffer, responseSize));
  if (!response) {
    return response.error();
  }
  Page::array hmac;
  copy(response.value().begin(), response.value().end(), hmac.begin());
  return hmac;
}

Result<void>
DS28E83_DS28E84::computeMultiblockHash(bool firstBlock, bool lastBlock,
                                       span<const uint_least8_t> data) {
  const span<const uint_least8_t>::index_type maxDataSize = 64;

  if (data.size() < 1 || data.size() > maxDataSize) {
    return InvalidParameterError;
  }

  uint_least8_t buffer[2 + maxDataSize];
  buffer[0] = 0x33;
  buffer[1] = 0;
  if (firstBlock) {
    buffer[1] |= 0x40;
  }
  if (lastBlock) {
    buffer[1] |= 0x80;
  }
  copy(data.begin(), data.end(), buffer + 2);
  return runCommand(make_span(buffer, 2 + data.size()), computeTimeMs);
}

Result<void> DS28E83_DS28E84::verifyEcdsaSignature(
    KeySecret key, bool authorityKey, GpioState gpioState,
    Ecc256::Signature::const_span signature, span<const uint_least8_t> data) {
  return verifyEcdsaSignature(key, authorityKey, DataInput, gpioState,
                              signature, data);
}

Result<void> DS28E83_DS28E84::verifyEcdsaSignature(
    KeySecret key, bool authorityKey, GpioState gpioState,
    Ecc256::Signature::const_span signature, Page::const_span hash) {
  return verifyEcdsaSignature(key, authorityKey, HashInput, gpioState,
                              signature, hash);
}

Result<void>
DS28E83_DS28E84::verifyEcdsaSignature(KeySecret key, bool authorityKey,
                                      GpioState gpioState,
                                      Ecc256::Signature::const_span signature) {
  return verifyEcdsaSignature(key, authorityKey, THASH, gpioState, signature,
                              span<const uint_least8_t>());
}

Result<void> DS28E83_DS28E84::verifyEcdsaSignature(
    KeySecret key, bool authorityKey, HashType hashType, GpioState gpioState,
    Ecc256::Signature::const_span signature, span<const uint_least8_t> buffer) {
  const span<const uint_least8_t>::index_type maxBufferSize = 61;

  if (buffer.size() > maxBufferSize) {
    return InvalidParameterError;
  }

  uint_least8_t request[2 + 2 * Ecc256::Scalar::size + maxBufferSize];
  uint_least8_t * requestIt = request;
  *requestIt++ = 0x59;
  switch (key) {
  case KeySecretA:
    if (authorityKey) {
      *requestIt = 2;
    } else {
      *requestIt = 0;
    }
    break;
  case KeySecretB:
    if (authorityKey) {
      *requestIt = 3;
    } else {
      *requestIt = 1;
    }
    break;
  case KeySecretS:
    if (!authorityKey) {
      *requestIt = 4;
      break;
    }
    // else: Go to default case.
  default:
    return InvalidParameterError;
  }
  *requestIt |= hashType << 3;
  if (gpioState != Unchanged) {
    *requestIt |= 0x40;
  }
  if (gpioState == Conducting) {
    *requestIt |= 0x20;
  }
  requestIt = copy(signature.r.begin(), signature.r.end(), ++requestIt);
  requestIt = copy(signature.s.begin(), signature.s.end(), requestIt);
  requestIt = copy(buffer.begin(), buffer.end(), requestIt);
  return runCommand(make_span(request, requestIt),
                    verifyEcdsaTimeMs +
                        (hashType == DataInput ? computeTimeMs : 0));
}

Result<void> DS28E83_DS28E84::authenticateEcdsaPublicKey(
    KeySecret key, Ecc256::Signature::const_span cert,
    span<const uint_least8_t> certCustomization) {
  return authenticateEcdsaPublicKey(key, true, cert, certCustomization, NULL);
}

Result<void> DS28E83_DS28E84::authenticateEcdsaPublicKey(
    KeySecret key, bool authWrites, Ecc256::Signature::const_span cert,
    span<const uint_least8_t> certCustomization,
    span<const uint_least8_t> ecdhCustomization) {
  return authenticateEcdsaPublicKey(key, authWrites, cert, certCustomization,
                                    &ecdhCustomization);
}

Result<void> DS28E83_DS28E84::authenticateEcdsaPublicKey(
    KeySecret key, bool authWrites, Ecc256::Signature::const_span cert,
    span<const uint_least8_t> certCustomization,
    const span<const uint_least8_t> * ecdhCustomization) {
  const span<const uint_least8_t>::index_type minCustomizationSize = 1;
  const span<const uint_least8_t>::index_type maxCertCustomizationSize = 32;
  const span<const uint_least8_t>::index_type maxEcdhCustomizationSize = 48;
  const span<const uint_least8_t>::index_type maxTotalCustomizationSize = 60;

  if (!(certCustomization.size() >= minCustomizationSize &&
        certCustomization.size() <= maxCertCustomizationSize &&
        (!ecdhCustomization ||
         (ecdhCustomization->size() >= minCustomizationSize &&
          ecdhCustomization->size() <= maxEcdhCustomizationSize &&
          certCustomization.size() + ecdhCustomization->size() <=
              maxTotalCustomizationSize)))) {
    return InvalidParameterError;
  }

  if (key == KeySecretS) {
    return InvalidParameterError;
  }

  uint_least8_t
      request[2 + 2 * Ecc256::Scalar::size + maxTotalCustomizationSize];
  uint_least8_t * requestIt = request;
  *requestIt++ = 0xA8;
  *requestIt++ = static_cast<uint_least8_t>(
      ((certCustomization.size() - 1) << 3) | (key << 2) |
      ((ecdhCustomization != NULL) << 1) | (authWrites << 0));
  requestIt = copy(cert.r.begin(), cert.r.end(), requestIt);
  requestIt = copy(cert.s.begin(), cert.s.end(), requestIt);
  requestIt =
      copy(certCustomization.begin(), certCustomization.end(), requestIt);
  int delay = verifyEcdsaTimeMs;
  if (ecdhCustomization) {
    const span<const uint_least8_t>::index_type certCustomizationPaddingSize =
        maxCertCustomizationSize - certCustomization.size();
    std::fill_n(requestIt, certCustomizationPaddingSize, 0);
    requestIt += certCustomizationPaddingSize;
    requestIt =
        copy(ecdhCustomization->begin(), ecdhCustomization->end(), requestIt);
    delay += verifyEcdsaTimeMs;
  }
  return runCommand(make_span(request, requestIt), delay);
}

Result<void> DS28E83_DS28E84::authenticatedEcdsaWriteMemory(
    int pageNum, bool useKeyS, Page::const_span newPageData,
    Ecc256::Signature::const_span signature) {
  return authenticatedEcdsaWriteMemory(pageNum, useKeyS, newPageData, signature,
                                       NULL);
}

Result<void> DS28E83_DS28E84::authenticatedEcdsaWriteMemory(
    int pageNum, bool useKeyS, Page::const_span newPageData,
    Ecc256::Signature::const_span signature,
    EncryptionChallenge::const_span challenge) {
  return authenticatedEcdsaWriteMemory(pageNum, useKeyS, newPageData, signature,
                                       &challenge);
}

Result<void> DS28E83_DS28E84::authenticatedEcdsaWriteMemory(
    int pageNum, bool useKeyS, Page::const_span newPageData,
    Ecc256::Signature::const_span signature,
    const EncryptionChallenge::const_span * challenge) {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return InvalidParameterError;
  }

  uint_least8_t request[2 + Page::size + 2 * Ecc256::Scalar::size +
                        EncryptionChallenge::size];
  uint_least8_t * requestIt = request;
  *requestIt++ = 0x89;
  *requestIt = pageNum;
  if (useKeyS) {
    *requestIt |= 0x80;
  }
  requestIt = copy(newPageData.begin(), newPageData.end(), ++requestIt);
  requestIt = copy(signature.r.begin(), signature.r.end(), requestIt);
  requestIt = copy(signature.s.begin(), signature.s.end(), requestIt);
  int delay = verifyEcdsaTimeMs + writeMemoryTimeMs;
  if (challenge) {
    requestIt = copy(challenge->begin(), challenge->end(), requestIt);
    delay += computeTimeMs;
  }
  return runCommand(make_span(request, requestIt), delay);
}

Result<void>
DS28E83_DS28E84::authenticatedSha256WriteMemory(int pageNum, bool useSecretS,
                                                Page::const_span newPageData,
                                                Page::const_span hmac) {
  return authenticatedSha256WriteMemory(pageNum, useSecretS, newPageData, hmac,
                                        NULL);
}

Result<void> DS28E83_DS28E84::authenticatedSha256WriteMemory(
    int pageNum, bool useSecretS, Page::const_span newPageData,
    Page::const_span hmac, EncryptionChallenge::const_span challenge) {
  return authenticatedSha256WriteMemory(pageNum, useSecretS, newPageData, hmac,
                                        &challenge);
}

Result<void> DS28E83_DS28E84::authenticatedSha256WriteMemory(
    int pageNum, bool useSecretS, Page::const_span newPageData,
    Page::const_span hmac, const EncryptionChallenge::const_span * challenge) {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return InvalidParameterError;
  }

  uint_least8_t request[3 + 2 * Page::size + EncryptionChallenge::size];
  uint_least8_t * requestIt = request;
  *requestIt++ = 0x99;
  *requestIt++ = pageNum;
  *requestIt++ = useSecretS ? 2 : 0;
  requestIt = copy(newPageData.begin(), newPageData.end(), requestIt);
  requestIt = copy(hmac.begin(), hmac.end(), requestIt);
  int delay = writeMemoryTimeMs + computeTimeMs;
  if (challenge) {
    requestIt = copy(challenge->begin(), challenge->end(), requestIt);
    delay += computeTimeMs;
  }
  return runCommand(make_span(request, requestIt), delay);
}

Result<void> DS28E83_DS28E84::computeAndWriteSha256Secret(
    int pageNum, KeySecret masterSecret, KeySecret destinationSecret,
    Page::const_span partialSecret) {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return InvalidParameterError;
  }

  uint_least8_t request[3 + Page::size];
  request[0] = 0x3C;
  request[1] = pageNum;
  request[2] = (destinationSecret << 2) | masterSecret;
  copy(partialSecret.begin(), partialSecret.end(), request + 3);
  return runCommand(request, writeMemoryTimeMs + computeTimeMs);
}

Result<void> DS28E83_DS28E84::generateEcc256KeyPair(KeySecret key) {
  if (key == KeySecretS) {
    return InvalidParameterError;
  }

  const uint_least8_t request[] = {0xCB, key == KeySecretB};
  return runCommand(request, generateEccKeyPairTimeMs);
}

Result<void> DS28E83_DS28E84::readRng(span<uint_least8_t> data) const {
  const span<uint_least8_t>::index_type maxDataSize = 64;
  if ((data.size() < 1) || (data.size() > maxDataSize)) {
    return InvalidParameterError;
  }

  uint_least8_t buffer[1 + maxDataSize];
  buffer[0] = readRngCmd;
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

Result<void> DS28E83_DS28E84::entropyHealthTest() const {
  const uint_least8_t request[] = {readRngCmd, 0x80};
  return runCommand(request, trngOnDemandCheckTimeMs);
}

Result<span<uint_least8_t> >
DS28E83_DS28E84::runCommand(span<const uint_least8_t> request, int delayTime,
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

Result<void> DS28E83_DS28E84::runCommand(span<const uint_least8_t> request,
                                         int delayTime) const {
  uint_least8_t buffer;
  MaximInterfaceCore_TRY(runCommand(request, delayTime, make_span(&buffer, 1)));
  return none;
}

const error_category & DS28E83_DS28E84::errorCategory() {
  static class : public error_category {
  public:
    virtual const char * name() const { return "DS28E83_DS28E84"; }

    virtual std::string message(int condition) const {
      switch (condition) {
      case InvalidOperationError:
        return "Invalid Operation Error";

      case InvalidParameterError:
        return "Invalid Parameter Error";

      case InvalidSequenceError:
        return "Invalid Sequence Error";

      case AuthenticationError:
        return "Authentication Error";

      case InternalError:
        return "Internal Error";

      case DeviceDisabledError:
        return "Device Disabled Error";

      case InvalidResponseError:
        return "Invalid Response Error";
      }
      return defaultErrorMessage(condition);
    }
  } instance;
  return instance;
}

Result<void> DS28E84::decrementCounter() {
  const uint_least8_t request = 0xC9;
  return runCommand(make_span(&request, 1), writeMemoryTimeMs);
}

Result<void> DS28E84::deviceStateControl(StateOperation operation) {
  const uint_least8_t request[] = {0x55, operation == Backup};
  return runCommand(request, writeMemoryTimeMs);
}

Result<void> computeMultiblockHash(DS28E83_DS28E84 & device,
                                   span<const uint_least8_t> data) {
  span<const uint_least8_t>::index_type dataIdx = 0;
  while (dataIdx < data.size()) {
    const span<const uint_least8_t>::index_type remainingSize =
        data.size() - dataIdx;
    const span<const uint_least8_t>::index_type chunkSize =
        std::min<span<const uint_least8_t>::index_type>(remainingSize, 64);
    MaximInterfaceCore_TRY(
        device.computeMultiblockHash(dataIdx == 0, remainingSize == chunkSize,
                                     data.subspan(dataIdx, chunkSize)));
    dataIdx += chunkSize;
  }
  return none;
}

static void setAnonymous(RomId::span romId) {
  std::fill(romId.begin(), romId.end(), 0xFF);
}

DS28E83_DS28E84::PageAuthenticationData &
DS28E83_DS28E84::PageAuthenticationData::setAnonymousRomId() {
  setAnonymous(romId());
  return *this;
}

DS28E83_DS28E84::ComputeSecretData::ComputeSecretData() : data() {
  setPageNum(0);
  setManId(ManId::array());
}

DS28E83_DS28E84::ComputeSecretData &
DS28E83_DS28E84::ComputeSecretData::setManId(ManId::const_span manId) {
  ManId::array validatedManId;
  copy(manId, make_span(validatedManId));
  validatedManId[1] |= 0x80;
  data.setManId(validatedManId);
  return *this;
}

DS28E83_DS28E84::DecryptionHmacData &
DS28E83_DS28E84::DecryptionHmacData::setAnonymousRomId() {
  setAnonymous(romId());
  return *this;
}

} // namespace MaximInterfaceDevices
