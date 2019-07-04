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

#include <algorithm>
#include "DS28E15_22_25.hpp"
#include <MaximInterface/Utilities/crc.hpp>
#include <MaximInterface/Utilities/Error.hpp>

using std::copy;

namespace MaximInterface {

using namespace Sha256;

static const int shaComputationDelayMs = 3;
static const int eepromWriteDelayMs = 10;
static inline int secretEepromWriteDelayMs(bool lowPower) {
  return lowPower ? 200 : 100;
}
static const int ds28e22_25_pagesPerBlock = 2;

static error_code
writeDataWithCrc(OneWireMaster & master, const uint_least8_t * data,
                 size_t dataLen,
                 OneWireMaster::Level level = OneWireMaster::NormalLevel,
                 uint_fast16_t crcStart = 0) {
  error_code result = master.writeBlock(data, dataLen);
  if (result) {
    return result;
  }
  uint_least8_t response[2];
  result = master.readByte(response[0]);
  if (result) {
    return result;
  }
  result = master.readByteSetLevel(response[1], level);
  if (result) {
    return result;
  }
  if (calculateCrc16(response, sizeof(response) / sizeof(response[0]),
                     calculateCrc16(data, dataLen, crcStart)) != 0xB001) {
    result = make_error_code(DS28E15_22_25::CrcError);
  }
  return result;
}

error_code
DS28E15_22_25::writeCommandWithCrc(Command command, uint_least8_t parameter,
                                   OneWireMaster::Level level) const {
  error_code result = selectRom(*master);
  if (!result) {
    const uint_least8_t data[] = {static_cast<uint_least8_t>(command),
                                  parameter};
    result =
        writeDataWithCrc(*master, data, sizeof(data) / sizeof(data[0]), level);
  }
  return result;
}

static error_code readDataWithCrc(OneWireMaster & master, uint_least8_t * data,
                                  size_t dataLen) {
  error_code result = master.readBlock(data, dataLen);
  if (result) {
    return result;
  }
  uint_least8_t response[2];
  const size_t responseLen = sizeof(response) / sizeof(response[0]);
  result = master.readBlock(response, responseLen);
  if (result) {
    return result;
  }
  if (calculateCrc16(response, responseLen, calculateCrc16(data, dataLen)) !=
      0xB001) {
    result = make_error_code(DS28E15_22_25::CrcError);
  }
  return result;
}

static error_code readCsByte(OneWireMaster & master) {
  uint_least8_t response;
  error_code result = master.readByte(response);
  if (result) {
    return result;
  }
  if (response != 0xAA) {
    result = make_error_code(DS28E15_22_25::OperationFailure);
  }
  return result;
}

static error_code releaseSequence(OneWireMaster & master, const Sleep & sleep,
                                  int delayTimeMs) {
  error_code result = master.writeBytePower(0xAA);
  if (result) {
    return result;
  }
  sleep(delayTimeMs);
  result = master.setLevel(OneWireMaster::NormalLevel);
  if (result) {
    return result;
  }
  return readCsByte(master);
}

DS28E15_22_25::BlockProtection::BlockProtection(bool readProtection,
                                                bool writeProtection,
                                                bool eepromEmulation,
                                                bool authProtection,
                                                int blockNum) {
  setReadProtection(readProtection);
  setWriteProtection(writeProtection);
  setEepromEmulation(eepromEmulation);
  setAuthProtection(authProtection);
  setBlockNum(blockNum);
}

void DS28E15_22_25::BlockProtection::setBlockNum(int blockNum) {
  status &= ~blockNumMask;
  status |= (blockNum & blockNumMask);
}

bool DS28E15_22_25::BlockProtection::noProtection() const {
  return !readProtection() && !writeProtection() && !eepromEmulation() &&
         !authProtection();
}

void DS28E15_22_25::BlockProtection::setReadProtection(bool readProtection) {
  if (readProtection) {
    status |= readProtectionMask;
  } else {
    status &= ~readProtectionMask;
  }
}

void DS28E15_22_25::BlockProtection::setWriteProtection(bool writeProtection) {
  if (writeProtection) {
    status |= writeProtectionMask;
  } else {
    status &= ~writeProtectionMask;
  }
}

void DS28E15_22_25::BlockProtection::setEepromEmulation(bool eepromEmulation) {
  if (eepromEmulation) {
    status |= eepromEmulationMask;
  } else {
    status &= ~eepromEmulationMask;
  }
}

void DS28E15_22_25::BlockProtection::setAuthProtection(bool authProtection) {
  if (authProtection) {
    status |= authProtectionMask;
  } else {
    status &= ~authProtectionMask;
  }
}

error_code
DS28E15_22_25::writeAuthBlockProtection(const BlockProtection & newProtection,
                                        const Hash & mac) {
  error_code result =
      writeCommandWithCrc(AuthWriteBlockProtection, newProtection.statusByte(),
                          OneWireMaster::StrongLevel);
  if (result) {
    return result;
  }

  (*sleep)(shaComputationDelayMs);
  result = master->setLevel(OneWireMaster::NormalLevel);
  if (result) {
    return result;
  }

  result = writeDataWithCrc(*master, mac.data(), mac.size());
  if (result) {
    return result;
  }

  result = readCsByte(*master);
  if (result) {
    return result;
  }

  result = releaseSequence(*master, *sleep, eepromWriteDelayMs);
  return result;
}

error_code
DS28E15_22_25::writeBlockProtection(const BlockProtection & protection) {
  error_code result =
      writeCommandWithCrc(WriteBlockProtection, protection.statusByte());
  if (result) {
    return result;
  }

  result = releaseSequence(*master, *sleep, eepromWriteDelayMs);
  return result;
}

error_code DS28E15_22_25::doReadBlockProtection(int blockNum,
                                                BlockProtection & protection,
                                                Variant variant) const {
  uint_least8_t buffer = blockNum;
  if (variant == DS28E22 || variant == DS28E25) {
    buffer *= ds28e22_25_pagesPerBlock;
  }
  error_code result = writeCommandWithCrc(ReadStatus, buffer);
  if (!result) {
    result = master->readByte(buffer);
    if (!result) {
      protection.setStatusByte(buffer);
    }
  }
  return result;
}

AuthMacData DS28E15_22_25::createAuthMacData(const Page & pageData, int pageNum,
                                             const Scratchpad & challenge,
                                             const RomId & romId,
                                             const ManId & manId) {
  AuthMacData authMacData;
  AuthMacData::iterator authMacDataIt = authMacData.begin();
  authMacDataIt = copy(pageData.begin(), pageData.end(), authMacDataIt);
  authMacDataIt = copy(challenge.begin(), challenge.end(), authMacDataIt);
  authMacDataIt = copy(romId.begin(), romId.end(), authMacDataIt);
  *authMacDataIt = manId[1];
  *(++authMacDataIt) = manId[0];
  *(++authMacDataIt) = pageNum;
  *(++authMacDataIt) = 0x00;
  return authMacData;
}

AuthMacData DS28E15_22_25::createAnonAuthMacData(const Page & pageData,
                                                 int pageNum,
                                                 const Scratchpad & challenge,
                                                 const ManId & manId) {
  RomId romId;
  romId.fill(0xFF);
  return createAuthMacData(pageData, pageNum, challenge, romId, manId);
}

error_code DS28E15_22_25::computeReadPageMac(int page_num, bool anon,
                                             Hash & mac) const {
  error_code result =
      writeCommandWithCrc(ComputePageMac, (anon ? 0xE0 : 0x00) | page_num,
                          OneWireMaster::StrongLevel);
  if (result) {
    return result;
  }

  (*sleep)(shaComputationDelayMs * 2);
  result = master->setLevel(OneWireMaster::NormalLevel);
  if (result) {
    return result;
  }

  result = readCsByte(*master);
  if (result) {
    return result;
  }

  result = readDataWithCrc(*master, mac.data(), mac.size());
  return result;
}

error_code DS28E15_22_25::doComputeSecret(int page_num, bool lock,
                                          bool lowPower) {
  error_code result = writeCommandWithCrc(ComputeAndLockSecret,
                                          lock ? (0xE0 | page_num) : page_num);
  if (result) {
    return result;
  }

  result = releaseSequence(*master, *sleep,
                           shaComputationDelayMs * 2 +
                               secretEepromWriteDelayMs(lowPower));
  return result;
}

error_code DS28E15_22_25::doWriteScratchpad(const Scratchpad & data,
                                            Variant variant) {
  const uint_least8_t parameter =
      (variant == DS28E22 || variant == DS28E25) ? 0x20 : 0x00;
  error_code result = writeCommandWithCrc(ReadWriteScratchpad, parameter);
  if (result) {
    return result;
  }

  result = writeDataWithCrc(*master, data.data(), data.size());
  return result;
}

error_code DS28E15_22_25::doReadScratchpad(Scratchpad & data,
                                           Variant variant) const {
  const uint_least8_t parameter =
      (variant == DS28E22 || variant == DS28E25) ? 0x2F : 0x0F;
  error_code result = writeCommandWithCrc(ReadWriteScratchpad, parameter);
  if (result) {
    return result;
  }

  result = readDataWithCrc(*master, data.data(), data.size());
  return result;
}

error_code DS28E15_22_25::doLoadSecret(bool lock, bool lowPower) {
  error_code result =
      writeCommandWithCrc(LoadAndLockSecret, lock ? 0xE0 : 0x00);
  if (result) {
    return result;
  }

  result = releaseSequence(*master, *sleep, secretEepromWriteDelayMs(lowPower));
  return result;
}

error_code DS28E15_22_25::readPage(int page, Page & rdbuf) const {
  error_code result = writeCommandWithCrc(ReadMemory, page);
  if (result) {
    return result;
  }

  result = continueReadPage(rdbuf);
  return result;
}

error_code DS28E15_22_25::continueReadPage(Page & rdbuf) const {
  return readDataWithCrc(*master, rdbuf.data(), rdbuf.size());
}

error_code DS28E15_22_25::doWriteAuthSegment(const Segment & newData,
                                             const Hash & mac, Variant variant,
                                             bool continuing) {
  // CRC gets calculated with CS byte when continuing on DS28E22 and DS28E25.
  const uint_fast16_t crcStart =
      ((variant == DS28E22 || variant == DS28E25) && continuing)
          ? calculateCrc16(0xAA)
          : 0;
  error_code result = writeDataWithCrc(*master, newData.data(), newData.size(),
                                       OneWireMaster::StrongLevel, crcStart);
  if (result) {
    return result;
  }

  (*sleep)(shaComputationDelayMs);
  result = master->setLevel(OneWireMaster::NormalLevel);
  if (result) {
    return result;
  }

  result = writeDataWithCrc(*master, mac.data(), mac.size());
  if (result) {
    return result;
  }

  result = readCsByte(*master);
  if (result) {
    return result;
  }

  result = releaseSequence(*master, *sleep, eepromWriteDelayMs);
  return result;
}

error_code DS28E15_22_25::doWriteAuthSegment(int pageNum, int segmentNum,
                                             const Segment & newData,
                                             const Hash & mac,
                                             Variant variant) {
  error_code result =
      writeCommandWithCrc(AuthWriteMemory, (segmentNum << 5) | pageNum);
  if (result) {
    return result;
  }

  result = doWriteAuthSegment(newData, mac, variant, false);
  return result;
}

error_code DS28E15_22_25::doContinueWriteAuthSegment(const Segment & newData,
                                                     const Hash & mac,
                                                     Variant variant) {
  return doWriteAuthSegment(newData, mac, variant, true);
}

WriteMacData DS28E15_22_25::createSegmentWriteMacData(
    int pageNum, int segmentNum, const Segment & newData,
    const Segment & oldData, const RomId & romId, const ManId & manId) {
  WriteMacData MT;

  // insert ROM number
  copy(romId.begin(), romId.end(), MT.begin());

  MT[11] = segmentNum;
  MT[10] = pageNum;
  MT[9] = manId[0];
  MT[8] = manId[1];

  // insert old data
  copy(oldData.begin(), oldData.end(), MT.begin() + 12);

  // insert new data
  copy(newData.begin(), newData.end(), MT.begin() + 16);

  return MT;
}

WriteMacData DS28E15_22_25::createProtectionWriteMacData(
    const BlockProtection & newProtection,
    const BlockProtection & oldProtection, const RomId & romId,
    const ManId & manId) {
  WriteMacData MT;

  // insert ROM number
  copy(romId.begin(), romId.end(), MT.begin());

  // instert block and page
  MT[11] = 0;
  MT[10] = newProtection.blockNum();

  MT[9] = manId[0];
  MT[8] = manId[1];

  // old data
  MT[12] = oldProtection.authProtection() ? 0x01 : 0x00;
  MT[13] = oldProtection.eepromEmulation() ? 0x01 : 0x00;
  MT[14] = oldProtection.writeProtection() ? 0x01 : 0x00;
  MT[15] = oldProtection.readProtection() ? 0x01 : 0x00;
  // new data
  MT[16] = newProtection.authProtection() ? 0x01 : 0x00;
  MT[17] = newProtection.eepromEmulation() ? 0x01 : 0x00;
  MT[18] = newProtection.writeProtection() ? 0x01 : 0x00;
  MT[19] = newProtection.readProtection() ? 0x01 : 0x00;

  // compute the mac
  return MT;
}

error_code DS28E15_22_25::readSegment(int page, int segment,
                                      Segment & data) const {
  error_code result = writeCommandWithCrc(ReadMemory, (segment << 5) | page);
  if (result) {
    return result;
  }

  result = continueReadSegment(data);
  return result;
}

error_code DS28E15_22_25::continueReadSegment(Segment & data) const {
  return master->readBlock(data.data(), data.size());
}

error_code DS28E15_22_25::writeSegment(int page, int block,
                                       const Segment & data) {
  error_code result = writeCommandWithCrc(WriteMemory, (block << 5) | page);
  if (result) {
    return result;
  }

  result = continueWriteSegment(data);
  return result;
}

error_code DS28E15_22_25::continueWriteSegment(const Segment & data) {
  error_code result = writeDataWithCrc(*master, data.data(), data.size());
  if (result) {
    return result;
  }

  result = releaseSequence(*master, *sleep, eepromWriteDelayMs);
  return result;
}

SlaveSecretData
DS28E15_22_25::createSlaveSecretData(const Page & bindingPage,
                                     int bindingPageNum,
                                     const Scratchpad & partialSecret,
                                     const RomId & romId, const ManId & manId) {
  SlaveSecretData slaveSecretData;
  SlaveSecretData::iterator slaveSecretDataIt = slaveSecretData.begin();
  slaveSecretDataIt =
      copy(bindingPage.begin(), bindingPage.end(), slaveSecretDataIt);
  slaveSecretDataIt =
      copy(partialSecret.begin(), partialSecret.end(), slaveSecretDataIt);
  slaveSecretDataIt = copy(romId.begin(), romId.end(), slaveSecretDataIt);
  *slaveSecretDataIt = manId[1];
  *(++slaveSecretDataIt) = manId[0];
  *(++slaveSecretDataIt) = bindingPageNum;
  *(++slaveSecretDataIt) = 0x00;
  return slaveSecretData;
}

error_code DS28E15_22_25::doReadAllBlockProtection(BlockProtection * protection,
                                                   const size_t protectionLen,
                                                   Variant variant) const {
  error_code result = writeCommandWithCrc(ReadStatus, 0);
  if (!result) {
    if (variant == DS28E22 || variant == DS28E25) {
      // Need to read extra data on DS28E22 to get CRC16.
      uint_least8_t buf[DS28E25::memoryPages];
      result = readDataWithCrc(*master, buf, DS28E25::memoryPages);
      if (!result) {
        const int blocks = ((variant == DS28E22) ? DS28E22::memoryPages
                                                 : DS28E25::memoryPages) /
                           ds28e22_25_pagesPerBlock;
        for (size_t i = 0;
             i < std::min(protectionLen, static_cast<size_t>(blocks)); i++) {
          protection[i].setStatusByte(
              (buf[i * ds28e22_25_pagesPerBlock] & 0xF0) | // Upper nibble
              ((buf[i * ds28e22_25_pagesPerBlock] & 0x0F) /
               ds28e22_25_pagesPerBlock)); // Lower nibble
        }
      }
    } else // DS28E15
    {
      uint_least8_t buf[DS28E15::protectionBlocks];
      result = readDataWithCrc(*master, buf, DS28E15::protectionBlocks);
      if (!result) {
        for (size_t i = 0;
             i < std::min(protectionLen,
                          static_cast<size_t>(DS28E15::protectionBlocks));
             i++) {
          protection[i].setStatusByte(buf[i]);
        }
      }
    }
  }
  return result;
}

const error_category & DS28E15_22_25::errorCategory() {
  static class : public error_category {
  public:
    virtual const char * name() const { return "DS28E15_22_25"; }

    virtual std::string message(int condition) const {
      switch (condition) {
      case CrcError:
        return "CRC Error";

      case OperationFailure:
        return "Operation Failure";

      default:
        return defaultErrorMessage(condition);
      }
    }
  } instance;
  return instance;
}

error_code DS28E15_22_25::loadSecret(bool lock) {
  // Use worst-case low power timing if the device type is not known.
  return doLoadSecret(lock, true);
}

error_code DS28E15_22_25::computeSecret(int pageNum, bool lock) {
  // Use worst-case low power timing if the device type is not known.
  return doComputeSecret(pageNum, lock, true);
}

error_code DS28E15_22_25::readPersonality(Personality & personality) const {
  error_code result = writeCommandWithCrc(ReadStatus, 0xE0);
  if (!result) {
    array<uint_least8_t, 4> data;
    result = readDataWithCrc(*master, data.data(), data.size());
    if (!result) {
      personality.PB1 = data[0];
      personality.PB2 = data[1];
      personality.manId[0] = data[2];
      personality.manId[1] = data[3];
    }
  }
  return result;
}

error_code DS28EL15::writeScratchpad(const Scratchpad & data) {
  return doWriteScratchpad(data, DS28E15);
}

error_code DS28EL15::readScratchpad(Scratchpad & data) const {
  return doReadScratchpad(data, DS28E15);
}

error_code DS28EL15::readBlockProtection(int blockNum,
                                         BlockProtection & protection) const {
  return doReadBlockProtection(blockNum, protection, DS28E15);
}

error_code DS28EL15::writeAuthSegment(int pageNum, int segmentNum,
                                      const Segment & newData,
                                      const Hash & mac) {
  return doWriteAuthSegment(pageNum, segmentNum, newData, mac, DS28E15);
}

error_code DS28EL15::continueWriteAuthSegment(const Segment & newData,
                                              const Hash & mac) {
  return doContinueWriteAuthSegment(newData, mac, DS28E15);
}

error_code DS28EL15::readAllBlockProtection(
    array<BlockProtection, protectionBlocks> & protection) const {
  return doReadAllBlockProtection(protection.data(), protection.size(),
                                  DS28E15);
}

error_code DS28E15::loadSecret(bool lock) { return doLoadSecret(lock, false); }

error_code DS28E15::computeSecret(int pageNum, bool lock) {
  return doComputeSecret(pageNum, lock, false);
}

error_code DS28EL22::writeScratchpad(const Scratchpad & data) {
  return doWriteScratchpad(data, DS28E22);
}

error_code DS28EL22::readScratchpad(Scratchpad & data) const {
  return doReadScratchpad(data, DS28E22);
}

error_code DS28EL22::readBlockProtection(int blockNum,
                                         BlockProtection & protection) const {
  return doReadBlockProtection(blockNum, protection, DS28E22);
}

error_code DS28EL22::writeAuthSegment(int pageNum, int segmentNum,
                                      const Segment & newData,
                                      const Hash & mac) {
  return doWriteAuthSegment(pageNum, segmentNum, newData, mac, DS28E22);
}

error_code DS28EL22::continueWriteAuthSegment(const Segment & newData,
                                              const Hash & mac) {
  return doContinueWriteAuthSegment(newData, mac, DS28E22);
}

error_code DS28EL22::readAllBlockProtection(
    array<BlockProtection, protectionBlocks> & protection) const {
  return doReadAllBlockProtection(protection.data(), protection.size(),
                                  DS28E22);
}

error_code DS28E22::loadSecret(bool lock) { return doLoadSecret(lock, false); }

error_code DS28E22::computeSecret(int pageNum, bool lock) {
  return doComputeSecret(pageNum, lock, false);
}

error_code DS28EL25::writeScratchpad(const Scratchpad & data) {
  return doWriteScratchpad(data, DS28E25);
}

error_code DS28EL25::readScratchpad(Scratchpad & data) const {
  return doReadScratchpad(data, DS28E25);
}

error_code DS28EL25::readBlockProtection(int blockNum,
                                         BlockProtection & protection) const {
  return doReadBlockProtection(blockNum, protection, DS28E25);
}

error_code DS28EL25::writeAuthSegment(int pageNum, int segmentNum,
                                      const Segment & newData,
                                      const Hash & mac) {
  return doWriteAuthSegment(pageNum, segmentNum, newData, mac, DS28E25);
}

error_code DS28EL25::continueWriteAuthSegment(const Segment & newData,
                                              const Hash & mac) {
  return doContinueWriteAuthSegment(newData, mac, DS28E25);
}

error_code DS28EL25::readAllBlockProtection(
    array<BlockProtection, protectionBlocks> & protection) const {
  return doReadAllBlockProtection(protection.data(), protection.size(),
                                  DS28E25);
}

error_code DS28E25::loadSecret(bool lock) { return doLoadSecret(lock, false); }

error_code DS28E25::computeSecret(int pageNum, bool lock) {
  return doComputeSecret(pageNum, lock, false);
}

} // namespace MaximInterface
