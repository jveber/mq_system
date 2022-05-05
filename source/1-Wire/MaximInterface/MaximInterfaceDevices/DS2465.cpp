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

#include <MaximInterfaceCore/Error.hpp>
#include "DS2465.hpp"

#define TRY MaximInterfaceCore_TRY
#define TRY_VALUE MaximInterfaceCore_TRY_VALUE

namespace MaximInterfaceDevices {

using namespace Core;

// Delay required after writing an EEPROM segment.
static const int eepromSegmentWriteDelayMs = 10;

// Delay required after writing an EEPROM page such as the secret memory.
static const int eepromPageWriteDelayMs = 8 * eepromSegmentWriteDelayMs;

// Delay required for a SHA computation to complete.
static const int shaComputationDelayMs = 2;

static const uint_least8_t scratchpad = 0x00;
static const uint_least8_t commandReg = 0x60;

static const uint_least8_t owTransmitBlockCmd = 0x69;

// DS2465 status bits.
static const uint_least8_t status_1WB = 0x01;
static const uint_least8_t status_PPD = 0x02;
static const uint_least8_t status_SD = 0x04;
static const uint_least8_t status_SBR = 0x20;
static const uint_least8_t status_TSB = 0x40;
static const uint_least8_t status_DIR = 0x80;

static const int maxBlockSize = 63;

const int DS2465::memoryPages;
const int DS2465::segmentsPerPage;

Result<void> DS2465::initialize(Config config) {
  // reset DS2465
  Result<void> result = resetDevice();
  if (result) {
    // write the default configuration setup
    result = writeConfig(config);
  }
  return result;
}

Result<void> DS2465::computeNextMasterSecret(bool swap, int pageNum,
                                             PageRegion region) {
  Result<void> result = ArgumentOutOfRangeError;
  if (pageNum >= 0) {
    const uint_least8_t command[] = {
        0x1E, static_cast<uint_least8_t>(swap ? (0xC8 | (pageNum << 4) | region)
                                              : 0xBF)};
    result = writeMemory(commandReg, command);
  }
  return result;
}

Result<void> DS2465::computeWriteMac(bool regwrite, bool swap, int pageNum,
                                     int segmentNum) const {
  Result<void> result = ArgumentOutOfRangeError;
  if (pageNum >= 0 && segmentNum >= 0) {
    const uint_least8_t command[] = {
        0x2D, static_cast<uint_least8_t>((regwrite << 7) | (swap << 6) |
                                         (pageNum << 4) | segmentNum)};
    result = writeMemory(commandReg, command);
    if (result) {
      sleep->invoke(shaComputationDelayMs);
    }
  }
  return result;
}

Result<void> DS2465::computeAuthMac(bool swap, int pageNum,
                                    PageRegion region) const {
  Result<void> result = ArgumentOutOfRangeError;
  if (pageNum >= 0) {
    const uint_least8_t command[] = {
        0x3C, static_cast<uint_least8_t>(swap ? (0xC8 | (pageNum << 4) | region)
                                              : 0xBF)};
    result = writeMemory(commandReg, command);
    if (result) {
      sleep->invoke(shaComputationDelayMs * 2);
    }
  }
  return result;
}

Result<void> DS2465::computeSlaveSecret(bool swap, int pageNum,
                                        PageRegion region) {
  Result<void> result = ArgumentOutOfRangeError;
  if (pageNum >= 0) {
    const uint_least8_t command[] = {
        0x4B, static_cast<uint_least8_t>(swap ? (0xC8 | (pageNum << 4) | region)
                                              : 0xBF)};
    result = writeMemory(commandReg, command);
    if (result) {
      sleep->invoke(shaComputationDelayMs * 2);
    }
  }
  return result;
}

Result<DS2465::Page::array> DS2465::readPage(int pageNum) const {
  uint_least8_t addr;
  switch (pageNum) {
  case 0:
    addr = 0x80;
    break;
  case 1:
    addr = 0xA0;
    break;
  default:
    return ArgumentOutOfRangeError;
  }
  Page::array data;
  TRY(readMemory(addr, data));
  return data;
}

Result<void> DS2465::writePage(int pageNum, Page::const_span data) {
  Result<void> result = writeMemory(scratchpad, data);
  if (result) {
    result = copyScratchpad(false, pageNum, false, 0);
  }
  if (result) {
    sleep->invoke(eepromPageWriteDelayMs);
  }
  return result;
}

Result<void> DS2465::writeSegment(int pageNum, int segmentNum,
                                  Segment::const_span data) {
  Result<void> result = writeMemory(scratchpad, data);
  if (result) {
    result = copyScratchpad(false, pageNum, true, segmentNum);
  }
  if (result) {
    sleep->invoke(eepromSegmentWriteDelayMs);
  }
  return result;
}

Result<void> DS2465::writeMasterSecret(Page::const_span masterSecret) {
  Result<void> result = writeMemory(scratchpad, masterSecret);
  if (result) {
    result = copyScratchpad(true, 0, false, 0);
  }
  if (result) {
    sleep->invoke(eepromPageWriteDelayMs);
  }
  return result;
}

Result<void> DS2465::copyScratchpad(bool destSecret, int pageNum, bool notFull,
                                    int segmentNum) {
  Result<void> result = ArgumentOutOfRangeError;
  if (pageNum >= 0 && segmentNum >= 0) {
    const uint_least8_t command[] = {
        0x5A,
        static_cast<uint_least8_t>(destSecret ? 0
                                              : (0x80 | (pageNum << 4) |
                                                 (notFull << 3) | segmentNum))};
    result = writeMemory(commandReg, command);
  }
  return result;
}

Result<void> DS2465::configureLevel(Level level) {
  // Check if supported level
  if (!((level == NormalLevel) || (level == StrongLevel))) {
    return InvalidLevelError;
  }
  // Check if requested level already set
  if (curConfig.getSPU() == (level == StrongLevel)) {
    return none;
  }
  // Set the level
  return writeConfig(Config(curConfig).setSPU(level == StrongLevel));
}

Result<void> DS2465::setLevel(Level newLevel) {
  if (newLevel == StrongLevel) {
    return InvalidLevelError;
  }

  return configureLevel(newLevel);
}

Result<void> DS2465::setSpeed(Speed newSpeed) {
  // Check if supported speed
  if (!((newSpeed == OverdriveSpeed) || (newSpeed == StandardSpeed))) {
    return InvalidSpeedError;
  }
  // Check if requested speed is already set
  if (curConfig.get1WS() == (newSpeed == OverdriveSpeed)) {
    return none;
  }
  // Set the speed
  return writeConfig(Config(curConfig).set1WS(newSpeed == OverdriveSpeed));
}

Result<OneWireMaster::TripletData> DS2465::triplet(bool sendBit) {
  // 1-Wire Triplet (Case B)
  //   S AD,0 [A] 1WT [A] SS [A] Sr AD,1 [A] [Status] A [Status] A\ P
  //                                         \--------/
  //                           Repeat until 1WB bit has changed to 0
  //  [] indicates from slave
  //  SS indicates byte containing search direction bit value in msbit

  const uint_least8_t command[] = {
      0x78, static_cast<uint_least8_t>(sendBit ? 0x80 : 0x00)};
  TRY(writeMemory(commandReg, command));

  uint_least8_t status;
  TRY_VALUE(status, pollBusy());

  TripletData data;
  data.readBit = ((status & status_SBR) == status_SBR);
  data.readBitComplement = ((status & status_TSB) == status_TSB);
  data.writeBit = ((status & status_DIR) == status_DIR);
  return data;
}

Result<void> DS2465::readBlock(span<uint_least8_t> recvBuf) {
  // 1-Wire Receive Block (Case A)
  //   S AD,0 [A] CommandReg [A] 1WRF [A] PR [A] P
  //  [] indicates from slave
  //  PR indicates byte containing parameter

  span<uint_least8_t>::index_type recvIdx = 0;
  while (recvIdx < recvBuf.size()) {
    const uint_least8_t command[] = {
        0xE1,
        static_cast<uint_least8_t>(std::min<span<uint_least8_t>::index_type>(
            recvBuf.size() - recvIdx, maxBlockSize))};
    TRY(writeMemory(commandReg, command));
    TRY(pollBusy());
    TRY(readMemory(scratchpad, recvBuf.subspan(recvIdx, command[1])));
    recvIdx += command[1];
  }
  return none;
}

Result<void> DS2465::writeBlock(span<const uint_least8_t> sendBuf) {
  span<const uint_least8_t>::index_type sendIdx = 0;
  while (sendIdx < sendBuf.size()) {
    const uint_least8_t command[] = {
        owTransmitBlockCmd, static_cast<uint_least8_t>(
                                std::min<span<const uint_least8_t>::index_type>(
                                    sendBuf.size() - sendIdx, maxBlockSize))};

    // prefill scratchpad with required data
    TRY(writeMemory(scratchpad, sendBuf.subspan(sendIdx, command[1])));

    // 1-Wire Transmit Block (Case A)
    //   S AD,0 [A] CommandReg [A] 1WTB [A] PR [A] P
    //  [] indicates from slave
    //  PR indicates byte containing parameter
    TRY(writeMemory(commandReg, command));
    TRY(pollBusy());
    sendIdx += command[1];
  }
  return none;
}

Result<void> DS2465::writeMacBlock() {
  // 1-Wire Transmit Block (Case A)
  //   S AD,0 [A] CommandReg [A] 1WTB [A] PR [A] P
  //  [] indicates from slave
  //  PR indicates byte containing parameter

  const uint_least8_t command[] = {owTransmitBlockCmd, 0xFF};
  TRY(writeMemory(commandReg, command));
  TRY(pollBusy());
  return none;
}

Result<uint_least8_t> DS2465::readByteSetLevel(Level afterLevel) {
  // 1-Wire Read Bytes (Case C)
  //   S AD,0 [A] CommandReg [A] 1WRB [A] Sr AD,1 [A] [Status] A [Status] A
  //                                                  \--------/
  //                     Repeat until 1WB bit has changed to 0
  //   Sr AD,0 [A] SRP [A] E1 [A] Sr AD,1 [A] DD A\ P
  //
  //  [] indicates from slave
  //  DD data read

  TRY(configureLevel(afterLevel));

  uint_least8_t buf = 0x96;
  TRY(writeMemory(commandReg, make_span(&buf, 1)));
  TRY(pollBusy());
  TRY(readMemory(0x62, make_span(&buf, 1)));
  return buf;
}

Result<void> DS2465::writeByteSetLevel(uint_least8_t sendByte,
                                       Level afterLevel) {
  // 1-Wire Write Byte (Case B)
  //   S AD,0 [A] CommandReg [A] 1WWB [A] DD [A] Sr AD,1 [A] [Status] A [Status]
  //   A\ P
  //                                                           \--------/
  //                             Repeat until 1WB bit has changed to 0
  //  [] indicates from slave
  //  DD data to write

  TRY(configureLevel(afterLevel));

  const uint_least8_t command[] = {0xA5, sendByte};
  TRY(writeMemory(commandReg, command));
  TRY(pollBusy());
  return none;
}

Result<bool> DS2465::touchBitSetLevel(bool sendBit, Level afterLevel) {
  // 1-Wire bit (Case B)
  //   S AD,0 [A] CommandReg [A] 1WSB [A] BB [A] Sr AD,1 [A] [Status] A [Status]
  //   A\ P
  //                                                          \--------/
  //                           Repeat until 1WB bit has changed to 0
  //  [] indicates from slave
  //  BB indicates byte containing bit value in msbit

  TRY(configureLevel(afterLevel));

  const uint_least8_t command[] = {
      0x87, static_cast<uint_least8_t>(sendBit ? 0x80 : 0x00)};
  TRY(writeMemory(commandReg, command));

  uint_least8_t status;
  TRY_VALUE(status, pollBusy());

  return (status & status_SBR) == status_SBR;
}

Result<void> DS2465::writeMemory(uint_least8_t addr,
                                 span<const uint_least8_t> buf) const {
  // Write SRAM (Case A)
  //   S AD,0 [A] VSA [A] DD [A]  P
  //                      \-----/
  //                        Repeat for each data byte
  //  [] indicates from slave
  //  VSA valid SRAM memory address
  //  DD memory data to write

  Result<void> result = master->start(address_);
  if (!result) {
    master->stop();
    return result;
  }
  result = master->writeByte(addr);
  if (!result) {
    master->stop();
    return result;
  }
  result = master->writeBlock(buf);
  if (!result) {
    master->stop();
    return result;
  }
  result = master->stop();
  return result;
}

Result<void> DS2465::readMemory(uint_least8_t addr,
                                span<uint_least8_t> buf) const {
  // Read (Case A)
  //   S AD,0 [A] MA [A] Sr AD,1 [A] [DD] A [DD] A\ P
  //                                 \-----/
  //                                   Repeat for each data byte, NAK last byte
  //  [] indicates from slave
  //  MA memory address
  //  DD memory data read

  Result<void> result = master->start(address_);
  if (!result) {
    master->stop();
    return result;
  }
  result = master->writeByte(addr);
  if (!result) {
    master->stop();
    return result;
  }
  result = readMemory(buf);
  return result;
}

Result<void> DS2465::readMemory(span<uint_least8_t> buf) const {
  Result<void> result = master->start(address_ | 1);
  if (!result) {
    master->stop();
    return result;
  }
  result = master->readBlock(buf, I2CMaster::Nack);
  if (!result) {
    master->stop();
    return result;
  }
  result = master->stop();
  return result;
}

Result<void> DS2465::writeConfig(Config config) {
  const uint_least8_t configReg = 0x67;
  uint_least8_t configBuf =
      ((config.readByte() ^ 0xF) << 4) | config.readByte();
  Result<void> result = writeMemory(configReg, make_span(&configBuf, 1));
  if (result) {
    result = readMemory(configReg, make_span(&configBuf, 1));
  }
  if (result && configBuf != config.readByte()) {
    result = HardwareError;
  }
  if (result) {
    curConfig = config;
  }
  return result;
}

Result<void> DS2465::writePortParameter(PortParameter param, int val) {
  if (val < 0 || val > 15) {
    return ArgumentOutOfRangeError;
  }

  uint_least8_t addr = 0;
  switch (param) {
  case tRSTL_STD:
  case tRSTL_OD:
    addr = 0x68;
    break;
  case tMSP_STD:
  case tMSP_OD:
    addr = 0x69;
    break;
  case tW0L_STD:
  case tW0L_OD:
    addr = 0x6A;
    break;
  case tREC0:
    addr = 0x6B;
    break;
  case RWPU:
    addr = 0x6C;
    break;
  case tW1L_OD:
    addr = 0x6D;
    break;
  }

  uint_least8_t data;
  Result<void> result = readMemory(addr, make_span(&data, 1));
  if (!result) {
    return result;
  }

  uint_least8_t newData;
  if (param == tRSTL_OD || param == tMSP_OD || param == tW0L_OD) {
    newData = (data & 0x0F) | (val << 4);
  } else {
    newData = (data & 0xF0) | val;
  }

  if (newData != data) {
    result = writeMemory(addr, make_span(&newData, 1));
  }
  return result;
}

Result<uint_least8_t> DS2465::pollBusy() const {
  const int pollLimit = 200;

  int pollCount = 0;
  uint_least8_t status;
  do {
    const Result<void> result = readMemory(make_span(&status, 1));
    if (!result) {
      return result.error();
    }
    if (pollCount++ >= pollLimit) {
      return HardwareError;
    }
  } while ((status & status_1WB) == status_1WB);
  return status;
}

Result<void> DS2465::reset() {
  // 1-Wire reset (Case B)
  //   S AD,0 [A] CommandReg  [A] 1WRS [A] Sr AD,1 [A] [Status] A [Status] A\ P
  //                                                  \--------/
  //                       Repeat until 1WB bit has changed to 0
  //  [] indicates from slave

  uint_least8_t buf = 0xB4;
  TRY(writeMemory(commandReg, make_span(&buf, 1)));

  TRY_VALUE(buf, pollBusy());

  if ((buf & status_SD) == status_SD) {
    return ShortDetectedError;
  }
  if ((buf & status_PPD) != status_PPD) {
    return NoSlaveError;
  }

  return none;
}

Result<void> DS2465::resetDevice() {
  // Device Reset
  //   S AD,0 [A] CommandReg [A] 1WMR [A] Sr AD,1 [A] [SS] A\ P
  //  [] indicates from slave
  //  SS status byte to read to verify state

  uint_least8_t buf = 0xF0;
  Result<void> result = writeMemory(commandReg, make_span(&buf, 1));

  if (result) {
    result = readMemory(make_span(&buf, 1));
  }

  if (result) {
    if ((buf & 0xF7) != 0x10) {
      result = HardwareError;
    }
  }

  if (result) {
    reset(); // do a command to get 1-Wire master reset out of holding state
  }

  return result;
}

Result<void>
DS2465::computeNextMasterSecret(AuthenticationData::const_span data) {
  Result<void> result = writeMemory(scratchpad, data);
  if (result) {
    result = computeNextMasterSecret(false, 0, FullPage);
  }
  return result;
}

Result<void>
DS2465::computeNextMasterSecretWithSwap(AuthenticationData::const_span data,
                                        int pageNum, PageRegion region) {
  Result<void> result = writeMemory(scratchpad, data);
  if (result) {
    result = computeNextMasterSecret(true, pageNum, region);
  }
  return result;
}

Result<void> DS2465::doComputeWriteMac(WriteMacData::const_span data) const {
  Result<void> result = writeMemory(scratchpad, data);
  if (result) {
    result = computeWriteMac(false, false, 0, 0);
  }
  return result;
}

Result<DS2465::Page::array>
DS2465::computeWriteMac(WriteMacData::const_span data) const {
  Result<void> result = doComputeWriteMac(data);
  if (!result) {
    return result.error();
  }
  Page::array mac;
  result = readMemory(mac);
  if (!result) {
    return result.error();
  }
  return mac;
}

Result<void> DS2465::computeAndTransmitWriteMac(WriteMacData::const_span data) {
  Result<void> result = doComputeWriteMac(data);
  if (result) {
    result = writeMacBlock();
  }
  return result;
}

Result<void> DS2465::doComputeWriteMacWithSwap(WriteMacData::const_span data,
                                               int pageNum,
                                               int segmentNum) const {
  Result<void> result = writeMemory(scratchpad, data);
  if (result) {
    result = computeWriteMac(false, true, pageNum, segmentNum);
  }
  return result;
}

Result<DS2465::Page::array>
DS2465::computeWriteMacWithSwap(WriteMacData::const_span data, int pageNum,
                                int segmentNum) const {
  Result<void> result = doComputeWriteMacWithSwap(data, pageNum, segmentNum);
  if (!result) {
    return result.error();
  }
  Page::array mac;
  result = readMemory(mac);
  if (!result) {
    return result.error();
  }
  return mac;
}

Result<void>
DS2465::computeAndTransmitWriteMacWithSwap(WriteMacData::const_span data,
                                           int pageNum, int segmentNum) {
  Result<void> result = doComputeWriteMacWithSwap(data, pageNum, segmentNum);
  if (result) {
    result = writeMacBlock();
  }
  return result;
}

Result<void> DS2465::computeSlaveSecret(AuthenticationData::const_span data) {
  Result<void> result = writeMemory(scratchpad, data);
  if (result) {
    result = computeSlaveSecret(false, 0, FullPage);
  }
  return result;
}

Result<void>
DS2465::computeSlaveSecretWithSwap(AuthenticationData::const_span data,
                                   int pageNum, PageRegion region) {
  Result<void> result = writeMemory(scratchpad, data);
  if (result) {
    result = computeSlaveSecret(true, pageNum, region);
  }
  return result;
}

Result<void>
DS2465::doComputeAuthMac(AuthenticationData::const_span data) const {
  Result<void> result = writeMemory(scratchpad, data);
  if (result) {
    result = computeAuthMac(false, 0, FullPage);
  }
  return result;
}

Result<DS2465::Page::array>
DS2465::computeAuthMac(AuthenticationData::const_span data) const {
  Result<void> result = doComputeAuthMac(data);
  if (!result) {
    return result.error();
  }
  Page::array mac;
  result = readMemory(mac);
  if (!result) {
    return result.error();
  }
  return mac;
}

Result<void>
DS2465::computeAndTransmitAuthMac(AuthenticationData::const_span data) {
  Result<void> result = doComputeAuthMac(data);
  if (result) {
    result = writeMacBlock();
  }
  return result;
}

Result<void>
DS2465::doComputeAuthMacWithSwap(AuthenticationData::const_span data,
                                 int pageNum, PageRegion region) const {
  Result<void> result = writeMemory(scratchpad, data);
  if (result) {
    result = computeAuthMac(true, pageNum, region);
  }
  return result;
}

Result<DS2465::Page::array>
DS2465::computeAuthMacWithSwap(AuthenticationData::const_span data, int pageNum,
                               PageRegion region) const {
  Result<void> result = doComputeAuthMacWithSwap(data, pageNum, region);
  if (!result) {
    return result.error();
  }
  Page::array mac;
  result = readMemory(mac);
  if (!result) {
    return result.error();
  }
  return mac;
}

Result<void>
DS2465::computeAndTransmitAuthMacWithSwap(AuthenticationData::const_span data,
                                          int pageNum, PageRegion region) {
  Result<void> result = doComputeAuthMacWithSwap(data, pageNum, region);
  if (result) {
    result = writeMacBlock();
  }
  return result;
}

const error_category & DS2465::errorCategory() {
  static class : public error_category {
  public:
    virtual const char * name() const { return "DS2465"; }

    virtual std::string message(int condition) const {
      switch (condition) {
      case HardwareError:
        return "Hardware Error";

      case ArgumentOutOfRangeError:
        return "Argument Out of Range Error";
      }
      return defaultErrorMessage(condition);
    }
  } instance;
  return instance;
}

} // namespace MaximInterfaceDevices
