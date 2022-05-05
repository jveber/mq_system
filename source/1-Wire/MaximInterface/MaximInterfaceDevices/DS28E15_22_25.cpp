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

#include <algorithm>
#include <MaximInterfaceCore/Crc.hpp>
#include <MaximInterfaceCore/Error.hpp>
#include "DS28E15_22_25.hpp"

#define TRY MaximInterfaceCore_TRY
#define TRY_VALUE MaximInterfaceCore_TRY_VALUE

namespace MaximInterfaceDevices {

using namespace Core;
using std::copy;

static const int shaComputationDelayMs = 3;
static const int eepromWriteDelayMs = 10;
static inline int secretEepromWriteDelayMs(bool lowPower) {
  return lowPower ? 200 : 100;
}

static const int ds28e22_25_pagesPerBlock = 2;

static Result<void>
writeDataWithCrc(OneWireMaster & master, span<const uint_least8_t> data,
                 OneWireMaster::Level level = OneWireMaster::NormalLevel,
                 uint_fast16_t crcStart = 0) {
  TRY(master.writeBlock(data));
  uint_least8_t response[2];
  TRY_VALUE(response[0], master.readByte());
  TRY_VALUE(response[1], master.readByteSetLevel(level));
  if (calculateCrc16(calculateCrc16(crcStart, data), response) != 0xB001) {
    return DS28E15_22_25::CrcError;
  }
  return none;
}

const int DS28E15_22_25::segmentsPerPage;

DS28E15_22_25::AuthenticationData &
DS28E15_22_25::AuthenticationData::setAnonymousRomId() {
  std::fill(romId().begin(), romId().end(), 0xFF);
  return *this;
}

Result<void>
DS28E15_22_25::writeCommandWithCrc(Command command, uint_least8_t parameter,
                                   OneWireMaster::Level level) const {
  Result<void> result = selectRom(*master);
  if (result) {
    const uint_least8_t data[] = {static_cast<uint_least8_t>(command),
                                  parameter};
    result = writeDataWithCrc(*master, data, level);
  }
  return result;
}

static Result<void> readDataWithCrc(OneWireMaster & master,
                                    span<uint_least8_t> data) {
  Result<void> result = master.readBlock(data);
  if (!result) {
    return result;
  }
  uint_least8_t response[2];
  result = master.readBlock(response);
  if (!result) {
    return result;
  }
  if (calculateCrc16(calculateCrc16(data), response) != 0xB001) {
    result = DS28E15_22_25::CrcError;
  }
  return result;
}

static Result<void> readCsByte(OneWireMaster & master) {
  const Result<uint_least8_t> response = master.readByte();
  if (!response) {
    return response.error();
  }
  return (response.value() == 0xAA) ? makeResult(none)
                                    : DS28E15_22_25::OperationFailure;
}

static Result<void> releaseSequence(OneWireMaster & master, Sleep & sleep,
                                    int delayTimeMs) {
  Result<void> result = master.writeBytePower(0xAA);
  if (!result) {
    return result;
  }
  sleep(delayTimeMs);
  result = master.setLevel(OneWireMaster::NormalLevel);
  if (!result) {
    return result;
  }
  return readCsByte(master);
}

DS28E15_22_25::BlockProtection &
DS28E15_22_25::BlockProtection::setBlockNum(int blockNum) {
  status &= ~blockNumMask;
  status |= (blockNum & blockNumMask);
  return *this;
}

bool DS28E15_22_25::BlockProtection::noProtection() const {
  return !readProtection() && !writeProtection() && !eepromEmulation() &&
         !authProtection();
}

DS28E15_22_25::BlockProtection &
DS28E15_22_25::BlockProtection::setReadProtection(bool readProtection) {
  if (readProtection) {
    status |= readProtectionMask;
  } else {
    status &= ~readProtectionMask;
  }
  return *this;
}

DS28E15_22_25::BlockProtection &
DS28E15_22_25::BlockProtection::setWriteProtection(bool writeProtection) {
  if (writeProtection) {
    status |= writeProtectionMask;
  } else {
    status &= ~writeProtectionMask;
  }
  return *this;
}

DS28E15_22_25::BlockProtection &
DS28E15_22_25::BlockProtection::setEepromEmulation(bool eepromEmulation) {
  if (eepromEmulation) {
    status |= eepromEmulationMask;
  } else {
    status &= ~eepromEmulationMask;
  }
  return *this;
}

DS28E15_22_25::BlockProtection &
DS28E15_22_25::BlockProtection::setAuthProtection(bool authProtection) {
  if (authProtection) {
    status |= authProtectionMask;
  } else {
    status &= ~authProtectionMask;
  }
  return *this;
}

DS28E15_22_25::ProtectionWriteMacData::ProtectionWriteMacData()
    : result_(), oldProtection_(), newProtection_() {
  setOldProtection(oldProtection_);
  setNewProtection(newProtection_);
}

DS28E15_22_25::ProtectionWriteMacData &
DS28E15_22_25::ProtectionWriteMacData::setOldProtection(
    BlockProtection oldProtection) {
  result_[oldProtectionIdx] = oldProtection.authProtection() ? 1 : 0;
  result_[oldProtectionIdx + 1] = oldProtection.eepromEmulation() ? 1 : 0;
  result_[oldProtectionIdx + 2] = oldProtection.writeProtection() ? 1 : 0;
  result_[oldProtectionIdx + 3] = oldProtection.readProtection() ? 1 : 0;
  oldProtection_ = oldProtection;
  return *this;
}

DS28E15_22_25::ProtectionWriteMacData &
DS28E15_22_25::ProtectionWriteMacData::setNewProtection(
    BlockProtection newProtection) {
  result_[blockNumIdx] = newProtection.blockNum();
  result_[newProtectionIdx] = newProtection.authProtection() ? 1 : 0;
  result_[newProtectionIdx + 1] = newProtection.eepromEmulation() ? 1 : 0;
  result_[newProtectionIdx + 2] = newProtection.writeProtection() ? 1 : 0;
  result_[newProtectionIdx + 3] = newProtection.readProtection() ? 1 : 0;
  newProtection_ = newProtection;
  return *this;
}

Result<void>
DS28E15_22_25::writeAuthBlockProtection(BlockProtection newProtection,
                                        Page::const_span mac) {
  Result<void> result =
      writeCommandWithCrc(AuthWriteBlockProtection, newProtection.statusByte(),
                          OneWireMaster::StrongLevel);
  if (!result) {
    return result;
  }

  sleep->invoke(shaComputationDelayMs);
  result = master->setLevel(OneWireMaster::NormalLevel);
  if (!result) {
    return result;
  }

  result = writeDataWithCrc(*master, mac);
  if (!result) {
    return result;
  }

  result = readCsByte(*master);
  if (!result) {
    return result;
  }

  result = releaseSequence(*master, *sleep, eepromWriteDelayMs);
  return result;
}

Result<void> DS28E15_22_25::writeBlockProtection(BlockProtection protection) {
  Result<void> result =
      writeCommandWithCrc(WriteBlockProtection, protection.statusByte());
  if (!result) {
    return result;
  }

  result = releaseSequence(*master, *sleep, eepromWriteDelayMs);
  return result;
}

Result<DS28E15_22_25::BlockProtection>
DS28E15_22_25::doReadBlockProtection(int blockNum, Variant variant) const {
  uint_least8_t buffer = blockNum;
  if (variant == DS28E22 || variant == DS28E25) {
    buffer *= ds28e22_25_pagesPerBlock;
  }
  TRY(writeCommandWithCrc(ReadStatus, buffer));
  TRY_VALUE(buffer, master->readByte());
  return BlockProtection(buffer);
}

Result<DS28E15_22_25::Page::array>
DS28E15_22_25::computeReadPageMac(int page_num, bool anon) const {
  Result<void> result =
      writeCommandWithCrc(ComputePageMac, (anon ? 0xE0 : 0x00) | page_num,
                          OneWireMaster::StrongLevel);
  if (!result) {
    return result.error();
  }

  sleep->invoke(shaComputationDelayMs * 2);
  result = master->setLevel(OneWireMaster::NormalLevel);
  if (!result) {
    return result.error();
  }

  result = readCsByte(*master);
  if (!result) {
    return result.error();
  }

  Page::array mac;
  result = readDataWithCrc(*master, mac);
  if (!result) {
    return result.error();
  }

  return mac;
}

Result<void> DS28E15_22_25::doComputeSecret(int page_num, bool lock,
                                            bool lowPower) {
  Result<void> result = writeCommandWithCrc(
      ComputeAndLockSecret, lock ? (0xE0 | page_num) : page_num);
  if (!result) {
    return result;
  }

  result = releaseSequence(*master, *sleep,
                           shaComputationDelayMs * 2 +
                               secretEepromWriteDelayMs(lowPower));
  return result;
}

Result<void> DS28E15_22_25::doWriteScratchpad(Page::const_span data,
                                              Variant variant) {
  const uint_least8_t parameter =
      (variant == DS28E22 || variant == DS28E25) ? 0x20 : 0x00;
  Result<void> result = writeCommandWithCrc(ReadWriteScratchpad, parameter);
  if (!result) {
    return result;
  }

  result = writeDataWithCrc(*master, data);
  return result;
}

Result<DS28E15_22_25::Page::array>
DS28E15_22_25::doReadScratchpad(Variant variant) const {
  const uint_least8_t parameter =
      (variant == DS28E22 || variant == DS28E25) ? 0x2F : 0x0F;
  Result<void> result = writeCommandWithCrc(ReadWriteScratchpad, parameter);
  if (!result) {
    return result.error();
  }

  Page::array data;
  result = readDataWithCrc(*master, data);
  if (!result) {
    return result.error();
  }

  return data;
}

Result<void> DS28E15_22_25::doLoadSecret(bool lock, bool lowPower) {
  Result<void> result =
      writeCommandWithCrc(LoadAndLockSecret, lock ? 0xE0 : 0x00);
  if (!result) {
    return result;
  }

  result = releaseSequence(*master, *sleep, secretEepromWriteDelayMs(lowPower));
  return result;
}

Result<DS28E15_22_25::Page::array> DS28E15_22_25::readPage(int page) const {
  const Result<void> result = writeCommandWithCrc(ReadMemory, page);
  if (!result) {
    return result.error();
  }

  return continueReadPage();
}

Result<DS28E15_22_25::Page::array> DS28E15_22_25::continueReadPage() const {
  Page::array rdbuf;
  TRY(readDataWithCrc(*master, rdbuf));
  return rdbuf;
}

Result<void> DS28E15_22_25::doWriteAuthSegment(Segment::const_span newData,
                                               Page::const_span mac,
                                               Variant variant,
                                               bool continuing) {
  // CRC gets calculated with CS byte when continuing on DS28E22 and DS28E25.
  const uint_fast16_t crcStart =
      ((variant == DS28E22 || variant == DS28E25) && continuing)
          ? calculateCrc16(0xAA)
          : 0;
  Result<void> result =
      writeDataWithCrc(*master, newData, OneWireMaster::StrongLevel, crcStart);
  if (!result) {
    return result;
  }

  sleep->invoke(shaComputationDelayMs);
  result = master->setLevel(OneWireMaster::NormalLevel);
  if (!result) {
    return result;
  }

  result = writeDataWithCrc(*master, mac);
  if (!result) {
    return result;
  }

  result = readCsByte(*master);
  if (!result) {
    return result;
  }

  result = releaseSequence(*master, *sleep, eepromWriteDelayMs);
  return result;
}

Result<void> DS28E15_22_25::doWriteAuthSegment(int pageNum, int segmentNum,
                                               Segment::const_span newData,
                                               Page::const_span mac,
                                               Variant variant) {
  Result<void> result =
      writeCommandWithCrc(AuthWriteMemory, (segmentNum << 5) | pageNum);
  if (!result) {
    return result;
  }

  result = doWriteAuthSegment(newData, mac, variant, false);
  return result;
}

Result<void> DS28E15_22_25::doContinueWriteAuthSegment(
    Segment::const_span newData, Page::const_span mac, Variant variant) {
  return doWriteAuthSegment(newData, mac, variant, true);
}

Result<DS28E15_22_25::Segment::array>
DS28E15_22_25::readSegment(int page, int segment) const {
  Result<void> result = writeCommandWithCrc(ReadMemory, (segment << 5) | page);
  if (!result) {
    return result.error();
  }

  return continueReadSegment();
}

Result<DS28E15_22_25::Segment::array>
DS28E15_22_25::continueReadSegment() const {
  Segment::array data;
  TRY(master->readBlock(data));
  return data;
}

Result<void> DS28E15_22_25::writeSegment(int page, int block,
                                         Segment::const_span data) {
  Result<void> result = writeCommandWithCrc(WriteMemory, (block << 5) | page);
  if (!result) {
    return result;
  }

  result = continueWriteSegment(data);
  return result;
}

Result<void> DS28E15_22_25::continueWriteSegment(Segment::const_span data) {
  Result<void> result = writeDataWithCrc(*master, data);
  if (!result) {
    return result;
  }

  result = releaseSequence(*master, *sleep, eepromWriteDelayMs);
  return result;
}

Result<void>
DS28E15_22_25::doReadAllBlockProtection(span<BlockProtection> protection,
                                        Variant variant) const {
  Result<void> result = writeCommandWithCrc(ReadStatus, 0);
  if (!result) {
    if (variant == DS28E22 || variant == DS28E25) {
      // Need to read extra data on DS28E22 to get CRC16.
      uint_least8_t buf[DS28E25::memoryPages];
      result = readDataWithCrc(*master, buf);
      if (!result) {
        const int blocks = ((variant == DS28E22) ? DS28E22::memoryPages
                                                 : DS28E25::memoryPages) /
                           ds28e22_25_pagesPerBlock;
        for (span<BlockProtection>::index_type i = 0;
             i < std::min<span<BlockProtection>::index_type>(protection.size(),
                                                             blocks);
             ++i) {
          protection[i].setStatusByte(
              (buf[i * ds28e22_25_pagesPerBlock] & 0xF0) | // Upper nibble
              ((buf[i * ds28e22_25_pagesPerBlock] & 0x0F) /
               ds28e22_25_pagesPerBlock)); // Lower nibble
        }
      }
    } else { // DS28E15
      uint_least8_t buf[DS28E15::protectionBlocks];
      result = readDataWithCrc(*master, buf);
      if (!result) {
        for (span<BlockProtection>::index_type i = 0;
             i < std::min<span<BlockProtection>::index_type>(
                     protection.size(), DS28E15::protectionBlocks);
             ++i) {
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
      }
      return defaultErrorMessage(condition);
    }
  } instance;
  return instance;
}

Result<void> DS28E15_22_25::loadSecret(bool lock) {
  // Use worst-case low power timing if the device type is not known.
  return doLoadSecret(lock, true);
}

Result<void> DS28E15_22_25::computeSecret(int pageNum, bool lock) {
  // Use worst-case low power timing if the device type is not known.
  return doComputeSecret(pageNum, lock, true);
}

Result<DS28E15_22_25::Personality> DS28E15_22_25::readPersonality() const {
  Result<void> result = writeCommandWithCrc(ReadStatus, 0xE0);
  if (!result) {
    return result.error();
  }

  uint_least8_t data[4];
  result = readDataWithCrc(*master, data);
  if (!result) {
    return result.error();
  }

  Personality personality;
  personality.PB1 = data[0];
  personality.PB2 = data[1];
  personality.manId[0] = data[2];
  personality.manId[1] = data[3];
  return personality;
}

const int DS28EL15::memoryPages;
const int DS28EL15::protectionBlocks;

Result<void> DS28EL15::writeScratchpad(Page::const_span data) {
  return doWriteScratchpad(data, DS28E15);
}

Result<DS28E15_22_25::Page::array> DS28EL15::readScratchpad() const {
  return doReadScratchpad(DS28E15);
}

Result<DS28E15_22_25::BlockProtection>
DS28EL15::readBlockProtection(int blockNum) const {
  return doReadBlockProtection(blockNum, DS28E15);
}

Result<void> DS28EL15::writeAuthSegment(int pageNum, int segmentNum,
                                        Segment::const_span newData,
                                        Page::const_span mac) {
  return doWriteAuthSegment(pageNum, segmentNum, newData, mac, DS28E15);
}

Result<void> DS28EL15::continueWriteAuthSegment(Segment::const_span newData,
                                                Page::const_span mac) {
  return doContinueWriteAuthSegment(newData, mac, DS28E15);
}

Result<array<DS28E15_22_25::BlockProtection, DS28E15::protectionBlocks> >
DS28EL15::readAllBlockProtection() const {
  array<BlockProtection, protectionBlocks> protection;
  TRY(doReadAllBlockProtection(protection, DS28E15));
  return protection;
}

Result<void> DS28E15::loadSecret(bool lock) {
  return doLoadSecret(lock, false);
}

Result<void> DS28E15::computeSecret(int pageNum, bool lock) {
  return doComputeSecret(pageNum, lock, false);
}

const int DS28EL22::memoryPages;
const int DS28EL22::protectionBlocks;

Result<void> DS28EL22::writeScratchpad(Page::const_span data) {
  return doWriteScratchpad(data, DS28E22);
}

Result<DS28E15_22_25::Page::array> DS28EL22::readScratchpad() const {
  return doReadScratchpad(DS28E22);
}

Result<DS28E15_22_25::BlockProtection>
DS28EL22::readBlockProtection(int blockNum) const {
  return doReadBlockProtection(blockNum, DS28E22);
}

Result<void> DS28EL22::writeAuthSegment(int pageNum, int segmentNum,
                                        Segment::const_span newData,
                                        Page::const_span mac) {
  return doWriteAuthSegment(pageNum, segmentNum, newData, mac, DS28E22);
}

Result<void> DS28EL22::continueWriteAuthSegment(Segment::const_span newData,
                                                Page::const_span mac) {
  return doContinueWriteAuthSegment(newData, mac, DS28E22);
}

Result<array<DS28E15_22_25::BlockProtection, DS28E22::protectionBlocks> >
DS28EL22::readAllBlockProtection() const {
  array<BlockProtection, protectionBlocks> protection;
  TRY(doReadAllBlockProtection(protection, DS28E22));
  return protection;
}

Result<void> DS28E22::loadSecret(bool lock) {
  return doLoadSecret(lock, false);
}

Result<void> DS28E22::computeSecret(int pageNum, bool lock) {
  return doComputeSecret(pageNum, lock, false);
}

const int DS28EL25::memoryPages;
const int DS28EL25::protectionBlocks;

Result<void> DS28EL25::writeScratchpad(Page::const_span data) {
  return doWriteScratchpad(data, DS28E25);
}

Result<DS28E15_22_25::Page::array> DS28EL25::readScratchpad() const {
  return doReadScratchpad(DS28E25);
}

Result<DS28E15_22_25::BlockProtection>
DS28EL25::readBlockProtection(int blockNum) const {
  return doReadBlockProtection(blockNum, DS28E25);
}

Result<void> DS28EL25::writeAuthSegment(int pageNum, int segmentNum,
                                        Segment::const_span newData,
                                        Page::const_span mac) {
  return doWriteAuthSegment(pageNum, segmentNum, newData, mac, DS28E25);
}

Result<void> DS28EL25::continueWriteAuthSegment(Segment::const_span newData,
                                                Page::const_span mac) {
  return doContinueWriteAuthSegment(newData, mac, DS28E25);
}

Result<array<DS28E15_22_25::BlockProtection, DS28E25::protectionBlocks> >
DS28EL25::readAllBlockProtection() const {
  array<BlockProtection, protectionBlocks> protection;
  TRY(doReadAllBlockProtection(protection, DS28E25));
  return protection;
}

Result<void> DS28E25::loadSecret(bool lock) {
  return doLoadSecret(lock, false);
}

Result<void> DS28E25::computeSecret(int pageNum, bool lock) {
  return doComputeSecret(pageNum, lock, false);
}

} // namespace MaximInterfaceDevices
