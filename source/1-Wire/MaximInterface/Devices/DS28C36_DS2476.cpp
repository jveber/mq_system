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

#include <MaximInterface/Links/I2CMaster.hpp>
#include <MaximInterface/Utilities/Error.hpp>
#include <MaximInterface/Utilities/RomId.hpp>
#include "DS28C36_DS2476.hpp"

using std::copy;

namespace MaximInterface {
using namespace Ecc256;

// DS28C36 commands.
enum Command {
  WriteMemory = 0x96,
  ReadMemory = 0x69,
  WriteBuffer = 0x87,
  ReadBuffer = 0x5A,
  ReadPageProtection = 0xAA,
  SetPageProtection = 0xC3,
  DecrementCounter = 0xC9,
  ReadRng = 0xD2,
  EncryptedReadMemory = 0x4B,
  ComputeAndReadPathAuthentication = 0xA5,
  AuthenticatedSha2WriteMemory = 0x99,
  ComputeAndLockSha2Secret = 0x3C,
  GenerateEcc256KeyPair = 0xCB,
  ComputeMultiblockHash = 0x33,
  VerifyEcdsaSignature = 0x59,
  AuthenticateEcdsaPublicKey = 0xA8,
  AuthenticatedEcdsaWriteMemory = 0x89
};

static error_code convertResultByte(uint_least8_t resultByte) {
  error_code errorCode;
  if (resultByte != 0xAA) {
    errorCode.assign((resultByte == 0x00)
                         ? static_cast<int>(DS28C36::AuthenticationError)
                         : resultByte,
                     DS28C36::errorCategory());
  }
  return errorCode;
}

error_code DS28C36::writeMemory(int pageNum, const Page & page) {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return make_error_code(InvalidParameterError);
  }

  array<uint_least8_t, 1 + Page::csize> buffer;
  buffer[0] = pageNum;
  copy(page.begin(), page.end(), buffer.begin() + 1);
  error_code result = writeCommand(WriteMemory, buffer.data(), buffer.size());
  if (!result) {
    sleep(writeMemoryTimeMs);
    result = readFixedLengthResponse(buffer.data(), 1);
    if (!result) {
      result = convertResultByte(buffer[0]);
    }
  }
  return result;
}

error_code DS28C36::readMemory(int pageNum, Page & page) {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return make_error_code(InvalidParameterError);
  }

  const uint_least8_t parameter = pageNum;
  error_code result = writeCommand(ReadMemory, &parameter, 1);
  if (!result) {
    sleep(readMemoryTimeMs);
    array<uint_least8_t, 1 + Page::csize> response;
    result = readFixedLengthResponse(response.data(), response.size());
    if (!result) {
      result = convertResultByte(response[0]);
      copy(response.begin() + 1, response.end(), page.begin());
    }
  }
  return result;
}

error_code DS28C36::writeBuffer(const uint_least8_t * data, size_t dataSize) {
  return writeCommand(WriteBuffer, data, dataSize);
}

error_code DS28C36::readBuffer(std::vector<uint_least8_t> & data) {
  error_code result = writeCommand(ReadBuffer);
  if (!result) {
    data.resize(80);
    size_t dataSize = data.size();
    result = readVariableLengthResponse(&data[0], dataSize);
    if (result) {
      data.clear();
    } else {
      data.resize(dataSize);
    }
  }
  return result;
}

error_code DS28C36::readPageProtection(int pageNum,
                                       PageProtection & protection) {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return make_error_code(InvalidParameterError);
  }

  uint_least8_t buffer = pageNum;
  error_code result = writeCommand(ReadPageProtection, &buffer, 1);
  if (!result) {
    sleep(readMemoryTimeMs);
    result = readFixedLengthResponse(&buffer, 1);
    if (!result) {
      protection = buffer;
    }
  }
  return result;
}

error_code DS28C36::setPageProtection(int pageNum,
                                      const PageProtection & protection) {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return make_error_code(InvalidParameterError);
  }

  array<uint_least8_t, 2> buffer = {
      static_cast<uint_least8_t>(pageNum),
      static_cast<uint_least8_t>(protection.to_ulong())};
  error_code result =
      writeCommand(SetPageProtection, buffer.data(), buffer.size());
  if (!result) {
    sleep(writeMemoryTimeMs);
    result = readFixedLengthResponse(buffer.data(), 1);
    if (!result) {
      result = convertResultByte(buffer[0]);
    }
  }
  return result;
}

error_code DS28C36::decrementCounter() {
  error_code result = writeCommand(DecrementCounter);
  if (!result) {
    sleep(writeMemoryTimeMs);
    uint_least8_t response;
    result = readFixedLengthResponse(&response, 1);
    if (!result) {
      result = convertResultByte(response);
    }
  }
  return result;
}

error_code DS28C36::readRng(uint_least8_t * data, size_t dataSize) {
  if ((dataSize < 1) || (dataSize > 64)) {
    return make_error_code(InvalidParameterError);
  }

  data[0] = static_cast<uint_least8_t>(dataSize - 1);
  error_code result = writeCommand(ReadRng, data, 1);
  if (!result) {
    sleep(sha256ComputationTimeMs);
    result = readFixedLengthResponse(data, dataSize);
  }
  return result;
}

error_code DS28C36::encryptedReadMemory(int pageNum, SecretNum secretNum,
                                        EncryptedPage & page) {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return make_error_code(InvalidParameterError);
  }

  const uint_least8_t parameter = (secretNum << 6) | pageNum;
  error_code result = writeCommand(EncryptedReadMemory, &parameter, 1);
  if (!result) {
    sleep(readMemoryTimeMs + sha256ComputationTimeMs);
    typedef array<uint_least8_t, 1 + EncryptedPage::size> Response;
    Response response;
    result = readFixedLengthResponse(response.data(), response.size());
    if (!result) {
      result = convertResultByte(response[0]);
      Response::const_iterator begin = response.begin() + 1;
      Response::const_iterator end = begin + page.challenge.size();
      copy(begin, end, page.challenge.begin());
      begin = end;
      end = begin + page.data.size();
      copy(begin, end, page.data.begin());
    }
  }
  return result;
}

error_code DS28C36::computeAndReadPageAuthentication(int pageNum,
                                                     AuthType authType) {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return make_error_code(InvalidParameterError);
  }

  const uint_least8_t parameter = (authType << 5) | pageNum;
  return writeCommand(ComputeAndReadPathAuthentication, &parameter, 1);
}

error_code
DS28C36::computeAndReadEcdsaPageAuthentication(int pageNum, KeyNum keyNum,
                                               Signature & signature) {
  AuthType authType;
  switch (keyNum) {
  case KeyNumA:
    authType = EcdsaWithKeyA;
    break;
  case KeyNumB:
    authType = EcdsaWithKeyB;
    break;
  case KeyNumC:
    authType = EcdsaWithKeyC;
    break;
  default:
    return make_error_code(InvalidParameterError);
  }
  error_code result = computeAndReadPageAuthentication(pageNum, authType);
  if (!result) {
    sleep(sha256ComputationTimeMs);
    typedef array<uint_least8_t, 1 + Signature::size> Response;
    Response response;
    result = readFixedLengthResponse(response.data(), response.size());
    if (!result) {
      result = convertResultByte(response[0]);
      Response::const_iterator begin = response.begin() + 1;
      Response::const_iterator end = begin + signature.s.size();
      copy(begin, end, signature.s.begin());
      begin = end;
      end = begin + signature.r.size();
      copy(begin, end, signature.r.begin());
    }
  }
  return result;
}

error_code DS28C36::computeAndReadHmacPageAuthentication(int pageNum,
                                                         SecretNum secretNum,
                                                         Sha256::Hash & hmac) {
  AuthType authType;
  switch (secretNum) {
  case SecretNumA:
    authType = HmacWithSecretA;
    break;
  case SecretNumB:
    authType = HmacWithSecretB;
    break;
  case SecretNumS:
    authType = HmacWithSecretS;
    break;
  default:
    return make_error_code(InvalidParameterError);
  }
  error_code result = computeAndReadPageAuthentication(pageNum, authType);
  if (!result) {
    sleep(generateEcdsaSignatureTimeMs);
    array<uint_least8_t, 1 + Sha256::Hash::csize> response;
    result = readFixedLengthResponse(response.data(), response.size());
    if (!result) {
      result = convertResultByte(response[0]);
      copy(response.begin() + 1, response.end(), hmac.begin());
    }
  }
  return result;
}

error_code DS28C36::authenticatedSha2WriteMemory(int pageNum,
                                                 SecretNum secretNum,
                                                 const Page & page) {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return make_error_code(InvalidParameterError);
  }

  array<uint_least8_t, 1 + Page::csize> buffer;
  buffer[0] = (secretNum << 6) | pageNum;
  copy(page.begin(), page.end(), buffer.begin() + 1);
  error_code result =
      writeCommand(AuthenticatedSha2WriteMemory, buffer.data(), buffer.size());
  if (!result) {
    sleep(writeMemoryTimeMs + (2 * sha256ComputationTimeMs));
    result = readFixedLengthResponse(buffer.data(), 1);
    if (!result) {
      result = convertResultByte(buffer[0]);
    }
  }
  return result;
}

error_code DS28C36::computeAndLockSha2Secret(int pageNum, SecretNum msecretNum,
                                             SecretNum dsecretNum,
                                             bool writeProtectEnable) {
  // User pages only
  if (pageNum < UserData0 || pageNum > UserData15) {
    return make_error_code(InvalidParameterError);
  }

  array<uint_least8_t, 2> buffer = {
      static_cast<uint_least8_t>((dsecretNum << 6) | (msecretNum << 4) |
                                 pageNum),
      static_cast<uint_least8_t>(writeProtectEnable ? 0x80 : 0x00)};
  error_code result =
      writeCommand(ComputeAndLockSha2Secret, buffer.data(), buffer.size());
  if (!result) {
    sleep(sha256ComputationTimeMs +
          ((writeProtectEnable ? 2 : 1) * writeMemoryTimeMs));
    result = readFixedLengthResponse(buffer.data(), 1);
    if (!result) {
      result = convertResultByte(buffer[0]);
    }
  }
  return result;
}

error_code DS28C36::generateEcc256KeyPair(KeyNum keyNum,
                                          bool writeProtectEnable) {
  if (keyNum == KeyNumS) {
    return make_error_code(InvalidParameterError);
  }

  uint_least8_t buffer = keyNum;
  if (writeProtectEnable) {
    buffer |= 0x80;
  }
  error_code result = writeCommand(GenerateEcc256KeyPair, &buffer, 1);
  if (!result) {
    sleep(generateEccKeyPairTimeMs);
    result = readFixedLengthResponse(&buffer, 1);
    if (!result) {
      result = convertResultByte(buffer);
    }
  }
  return result;
}

error_code DS28C36::computeMultiblockHash(bool firstBlock, bool lastBlock,
                                          const uint_least8_t * data,
                                          size_t dataSize) {
  const size_t maxDataSize = 64;

  if (dataSize < 1 || dataSize > maxDataSize) {
    return make_error_code(InvalidParameterError);
  }

  array<uint_least8_t, 1 + maxDataSize> buffer;
  buffer[0] = 0;
  if (firstBlock) {
    buffer[0] |= 0x40;
  }
  if (lastBlock) {
    buffer[0] |= 0x80;
  }
  copy(data, data + dataSize, buffer.begin() + 1);
  error_code result =
      writeCommand(ComputeMultiblockHash, buffer.data(), dataSize + 1);
  if (!result) {
    sleep(sha256ComputationTimeMs);
    result = readFixedLengthResponse(buffer.data(), 1);
    if (!result) {
      result = convertResultByte(buffer[0]);
    }
  }
  return result;
}

error_code DS28C36::verifyEcdsaSignature(KeyNum keyNum, HashType hashType,
                                         const Signature & signature,
                                         PioState pioa, PioState piob) {
  typedef array<uint_least8_t, 1 + Signature::size> Buffer;
  Buffer buffer;
  buffer[0] = keyNum | (hashType << 2);
  if (pioa != Unchanged) {
    buffer[0] |= 0x20;
  }
  if (pioa == Conducting) {
    buffer[0] |= 0x10;
  }
  if (piob != Unchanged) {
    buffer[0] |= 0x80;
  }
  if (piob == Conducting) {
    buffer[0] |= 0x40;
  }
  Buffer::iterator bufferIt =
      copy(signature.r.begin(), signature.r.end(), buffer.begin() + 1);
  copy(signature.s.begin(), signature.s.end(), bufferIt);
  error_code result =
      writeCommand(VerifyEcdsaSignature, buffer.data(), buffer.size());
  if (!result) {
    sleep(verifyEsdsaSignatureOrComputeEcdhTimeMs +
          ((hashType == DataInBuffer) ? sha256ComputationTimeMs : 0));
    result = readFixedLengthResponse(buffer.data(), 1);
    if (!result) {
      result = convertResultByte(buffer[0]);
    }
  }
  return result;
}

error_code DS28C36::authenticateEcdsaPublicKey(bool authWrites, bool ecdh,
                                               KeyNum keyNum, int csOffset,
                                               const Signature & signature) {
  if (((keyNum != KeyNumA) && (keyNum != KeyNumB)) || (csOffset < 0) ||
      (csOffset > 31)) {
    return make_error_code(InvalidParameterError);
  }

  typedef array<uint_least8_t, 1 + Signature::size> Buffer;
  Buffer buffer;
  buffer[0] = (csOffset << 3) | (keyNum << 2);
  if (ecdh) {
    buffer[0] |= 0x02;
  }
  if (authWrites) {
    buffer[0] |= 0x01;
  }
  Buffer::iterator bufferIt =
      copy(signature.r.begin(), signature.r.end(), buffer.begin() + 1);
  copy(signature.s.begin(), signature.s.end(), bufferIt);
  error_code result =
      writeCommand(AuthenticateEcdsaPublicKey, buffer.data(), buffer.size());
  if (!result) {
    sleep((ecdh ? 2 : 1) * verifyEsdsaSignatureOrComputeEcdhTimeMs);
    result = readFixedLengthResponse(buffer.data(), 1);
    if (!result) {
      result = convertResultByte(buffer[0]);
    }
  }
  return result;
}

error_code DS28C36::authenticatedEcdsaWriteMemory(int pageNum,
                                                  const Page & page) {
  if (pageNum < 0 || pageNum >= memoryPages) {
    return make_error_code(InvalidParameterError);
  }

  array<uint_least8_t, 1 + Page::csize> buffer;
  buffer[0] = pageNum;
  copy(page.begin(), page.end(), buffer.begin() + 1);
  error_code result =
      writeCommand(AuthenticatedEcdsaWriteMemory, buffer.data(), buffer.size());
  if (!result) {
    sleep(verifyEsdsaSignatureOrComputeEcdhTimeMs + writeMemoryTimeMs +
          sha256ComputationTimeMs);
    result = readFixedLengthResponse(buffer.data(), 1);
    if (!result) {
      result = convertResultByte(buffer[0]);
    }
  }
  return result;
}

error_code DS28C36::writeCommand(uint_least8_t command,
                                 const uint_least8_t * parameters,
                                 size_t parametersSize) {
  error_code result = master->start(address_);
  if (result) {
    master->stop();
    return result;
  }
  result = master->writeByte(command);
  if (result) {
    master->stop();
    return result;
  }
  if (parameters) {
    result = master->writeByte(static_cast<uint_least8_t>(parametersSize));
    if (result) {
      master->stop();
      return result;
    }
    result = master->writeBlock(parameters, parametersSize);
    if (result) {
      master->stop();
      return result;
    }
  }
  result = master->stop();
  return result;
}

error_code DS28C36::readVariableLengthResponse(uint_least8_t * response,
                                               size_t & responseSize) {
  error_code result = master->start(address_ | 1);
  if (result) {
    master->stop();
    return result;
  }
  uint_least8_t length;
  result = master->readByte(I2CMaster::Ack, length);
  if (result) {
    master->stop();
    return result;
  }
  if (length > responseSize) {
    master->stop();
    return make_error_code(InvalidResponseError);
  }
  if (length > 0) {
    result = master->readBlock(I2CMaster::Nack, response, length);
    if (result) {
      master->stop();
      return result;
    }
  }
  responseSize = length;
  result = master->stop();
  return result;
}

error_code DS28C36::readFixedLengthResponse(uint_least8_t * response,
                                            size_t responseSize) {
  const size_t requestedResponseSize = responseSize;
  error_code result = readVariableLengthResponse(response, responseSize);
  if (!result && responseSize != requestedResponseSize) {
    result = make_error_code(InvalidResponseError);
  }
  return result;
}

DS28C36::PageAuthenticationData
DS28C36::createPageAuthenticationData(const RomId & romId, const Page & page,
                                      const Page & challenge, int pageNum,
                                      const ManId & manId) {
  PageAuthenticationData data;
  PageAuthenticationData::iterator dataIt = data.begin();
  dataIt = copy(romId.begin(), romId.end(), dataIt);
  dataIt = copy(page.begin(), page.end(), dataIt);
  dataIt = copy(challenge.begin(), challenge.end(), dataIt);
  *dataIt = static_cast<uint_least8_t>(pageNum);
  ++dataIt;
  copy(manId.begin(), manId.end(), dataIt);
  return data;
}

const error_category & DS28C36::errorCategory() {
  static class : public error_category {
  public:
    virtual const char * name() const { return "DS28C36"; }

    virtual std::string message(int condition) const {
      switch (condition) {
      case ProtectionError:
        return "Protection Error";

      case InvalidParameterError:
        return "Invalid Parameter Error";

      case InvalidSequenceError:
        return "Invalid Sequence Error";

      case InvalidEcdsaInputOrResultError:
        return "Invalid ECDSA Input or Result Error";

      case AuthenticationError:
        return "Authentication Error";

      case InvalidResponseError:
        return "Invalid Response Error";

      default:
        return defaultErrorMessage(condition);
      }
    }
  } instance;
  return instance;
}

error_code readRomIdAndManId(DS28C36 & ds28c36, RomId * romId, ManId * manId) {
  DS28C36::Page page;
  const error_code result = ds28c36.readMemory(DS28C36::RomOptions, page);
  if (!result) {
    DS28C36::Page::const_iterator pageIt = page.begin() + 22;
    if (manId) {
      copy(pageIt, pageIt + ManId::size(), manId->begin());
    }
    pageIt += ManId::size();
    if (romId) {
      copy(pageIt, pageIt + RomId::size(), romId->begin());
    }
  }
  return result;
}

error_code computeMultiblockHash(DS28C36 & ds28c36, const uint_least8_t * data,
                                 const size_t dataSize) {
  error_code result;
  size_t remainingSize = dataSize;
  while ((remainingSize > 0) && !result) {
    const size_t chunkSize = std::min<size_t>(remainingSize, 64);
    result = ds28c36.computeMultiblockHash(
        remainingSize == dataSize, remainingSize == chunkSize, data, chunkSize);
    data += chunkSize;
    remainingSize -= chunkSize;
  }
  return result;
}

error_code verifyEcdsaSignature(DS28C36 & ds28c36, DS28C36::KeyNum publicKey,
                                const uint_least8_t * data, size_t dataSize,
                                const Signature & signature,
                                DS28C36::PioState pioa,
                                DS28C36::PioState piob) {
  error_code result = computeMultiblockHash(ds28c36, data, dataSize);
  if (!result) {
    result = ds28c36.verifyEcdsaSignature(publicKey, DS28C36::THASH, signature,
                                          pioa, piob);
  }
  return result;
}

error_code verifyEcdsaSignature(DS28C36 & ds28c36, const PublicKey & publicKey,
                                const uint_least8_t * data, size_t dataSize,
                                const Signature & signature,
                                DS28C36::PioState pioa,
                                DS28C36::PioState piob) {
  error_code result = ds28c36.writeMemory(DS28C36::PublicKeySX, publicKey.x);
  if (!result) {
    result = ds28c36.writeMemory(DS28C36::PublicKeySY, publicKey.y);
  }
  if (!result) {
    result = verifyEcdsaSignature(ds28c36, DS28C36::KeyNumS, data, dataSize,
                                  signature, pioa, piob);
  }
  return result;
}

// DS2476 additional commands.
enum DS2476_Command {
  GenerateEcdsaSignature = 0x1E,
  ComputeSha2UniqueSecret = 0x55,
  ComputeSha2Hmac = 0x2D
};

error_code DS2476::generateEcdsaSignature(KeyNum keyNum,
                                          Signature & signature) {
  if (keyNum == KeyNumS) {
    return make_error_code(InvalidParameterError);
  }

  const uint_least8_t parameter = keyNum;
  error_code result = writeCommand(GenerateEcdsaSignature, &parameter, 1);
  if (!result) {
    sleep(generateEcdsaSignatureTimeMs);
    typedef array<uint_least8_t, 1 + Signature::size> Response;
    Response response;
    result = readFixedLengthResponse(response.data(), response.size());
    if (!result) {
      result = convertResultByte(response[0]);
      Response::const_iterator begin = response.begin() + 1;
      Response::const_iterator end = begin + signature.s.size();
      copy(begin, end, signature.s.begin());
      begin = end;
      end = begin + signature.r.size();
      copy(begin, end, signature.r.begin());
    }
  }
  return result;
}

error_code DS2476::computeSha2UniqueSecret(SecretNum msecretNum) {
  uint_least8_t buffer = msecretNum << 4;
  error_code result = writeCommand(ComputeSha2UniqueSecret, &buffer, 1);
  if (!result) {
    sleep(sha256ComputationTimeMs);
    result = readFixedLengthResponse(&buffer, 1);
    if (!result) {
      convertResultByte(buffer);
    }
  }
  return result;
}

error_code DS2476::computeSha2Hmac(Sha256::Hash & hmac) {
  error_code result = writeCommand(ComputeSha2Hmac);
  if (!result) {
    sleep(sha256ComputationTimeMs);
    array<uint_least8_t, 1 + Sha256::Hash::csize> response;
    result = readFixedLengthResponse(response.data(), response.size());
    if (!result) {
      result = convertResultByte(response[0]);
      copy(response.begin() + 1, response.end(), hmac.begin());
    }
  }
  return result;
}

error_code enableCoprocessor(DS2476 & ds2476) {
  DS2476::Page gpioControl;
  error_code result = ds2476.readMemory(DS2476::GpioControl, gpioControl);
  if (!result && gpioControl[0] != 0xAA) {
    gpioControl[0] = 0xAA;
    result = ds2476.writeMemory(DS2476::GpioControl, gpioControl);
  }
  return result;
}

} // namespace MaximInterface
