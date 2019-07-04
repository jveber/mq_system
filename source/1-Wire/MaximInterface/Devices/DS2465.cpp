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

#include <MaximInterface/Utilities/Error.hpp>
#include "DS2465.hpp"

namespace MaximInterface {

using namespace Sha256;

enum MemoryAddress {
  Scratchpad = 0x00,
  CommandReg = 0x60,
  StatusReg = 0x61,
  ReadDataReg = 0x62,
  MacReadoutReg = 0x63,
  MemoryProtectionReg = 0x64,
  ConfigReg = 0x67,
  tRSTL_Reg = 0x68,
  tMSP_Reg = 0x69,
  tW0L_Reg = 0x6A,
  tREC0_Reg = 0x6B,
  RWPU_Reg = 0x6C,
  tW1L_Reg = 0x6D,
  UserMemoryPage0 = 0x80,
  UserMemoryPage1 = 0xA0
};

/// Delay required after writing an EEPROM segment.
static const int eepromSegmentWriteDelayMs = 10;
/// Delay required after writing an EEPROM page such as the secret memory.
static const int eepromPageWriteDelayMs = 8 * eepromSegmentWriteDelayMs;
/// Delay required for a SHA computation to complete.
static const int shaComputationDelayMs = 2;

/// DS2465 Commands
enum Command {
  DeviceResetCmd = 0xF0,
  WriteDeviceConfigCmd = 0xD2,
  OwResetCmd = 0xB4,
  OwWriteByteCmd = 0xA5,
  OwReadByteCmd = 0x96,
  OwSingleBitCmd = 0x87,
  OwTripletCmd = 0x78,
  OwTransmitBlockCmd = 0x69,
  OwReceiveBlockCmd = 0xE1,
  CopyScratchpadCmd = 0x5A,
  ComputeSlaveSecretCmd = 0x4B,
  ComputeSlaveAuthMacCmd = 0x3C,
  ComputeSlaveWriteMacCmd = 0x2D,
  ComputeNextMasterSecretCmd = 0x1E,
  SetProtectionCmd = 0x0F
};

/// DS2465 Status Bits
enum StatusBit {
  Status_1WB = 0x01,
  Status_PPD = 0x02,
  Status_SD = 0x04,
  Status_LL = 0x08,
  Status_RST = 0x10,
  Status_SBR = 0x20,
  Status_TSB = 0x40,
  Status_DIR = 0x80
};

static const size_t maxBlockSize = 63;

error_code DS2465::initialize(Config config) {
  // reset DS2465
  error_code result = resetDevice();
  if (!result) {
    // write the default configuration setup
    result = writeConfig(config);
  }
  return result;
}

error_code DS2465::computeNextMasterSecret(bool swap, int pageNum,
                                           PageRegion region) {
  error_code result = make_error_code(ArgumentOutOfRangeError);
  if (pageNum >= 0) {
    uint_least8_t command[] = {
        ComputeNextMasterSecretCmd,
        static_cast<uint_least8_t>(swap ? (0xC8 | (pageNum << 4) | region)
                                        : 0xBF)};
    result =
        writeMemory(CommandReg, command, sizeof(command) / sizeof(command[0]));
  }
  return result;
}

error_code DS2465::computeWriteMac(bool regwrite, bool swap, int pageNum,
                                   int segmentNum) const {
  error_code result = make_error_code(ArgumentOutOfRangeError);
  if (pageNum >= 0 && segmentNum >= 0) {
    uint_least8_t command[] = {
        ComputeSlaveWriteMacCmd,
        static_cast<uint_least8_t>((regwrite << 7) | (swap << 6) |
                                   (pageNum << 4) | segmentNum)};
    result =
        writeMemory(CommandReg, command, sizeof(command) / sizeof(command[0]));
    if (!result) {
      (*sleep)(shaComputationDelayMs);
    }
  }
  return result;
}

error_code DS2465::computeAuthMac(bool swap, int pageNum,
                                  PageRegion region) const {
  error_code result = make_error_code(ArgumentOutOfRangeError);
  if (pageNum >= 0) {
    uint_least8_t command[] = {
        ComputeSlaveAuthMacCmd,
        static_cast<uint_least8_t>(swap ? (0xC8 | (pageNum << 4) | region)
                                        : 0xBF)};
    result =
        writeMemory(CommandReg, command, sizeof(command) / sizeof(command[0]));
    if (!result) {
      (*sleep)(shaComputationDelayMs * 2);
    }
  }
  return result;
}

error_code DS2465::computeSlaveSecret(bool swap, int pageNum,
                                      PageRegion region) {
  error_code result = make_error_code(ArgumentOutOfRangeError);
  if (pageNum >= 0) {
    uint_least8_t command[] = {
        ComputeSlaveSecretCmd,
        static_cast<uint_least8_t>(swap ? (0xC8 | (pageNum << 4) | region)
                                        : 0xBF)};
    result =
        writeMemory(CommandReg, command, sizeof(command) / sizeof(command[0]));
    if (!result) {
      (*sleep)(shaComputationDelayMs * 2);
    }
  }
  return result;
}

error_code DS2465::readPage(int pageNum, Page & data) const {
  uint_least8_t addr;
  switch (pageNum) {
  case 0:
    addr = UserMemoryPage0;
    break;
  case 1:
    addr = UserMemoryPage1;
    break;
  default:
    return make_error_code(ArgumentOutOfRangeError);
  }
  return readMemory(addr, data.data(), data.size());
}

error_code DS2465::writePage(int pageNum, const Page & data) {
  error_code result = writeMemory(Scratchpad, data.data(), data.size());
  if (!result) {
    result = copyScratchpad(false, pageNum, false, 0);
  }
  if (!result) {
    (*sleep)(eepromPageWriteDelayMs);
  }
  return result;
}

error_code DS2465::writeSegment(int pageNum, int segmentNum,
                                const Segment & data) {
  error_code result = writeMemory(Scratchpad, data.data(), data.size());
  if (!result) {
    result = copyScratchpad(false, pageNum, true, segmentNum);
  }
  if (!result) {
    (*sleep)(eepromSegmentWriteDelayMs);
  }
  return result;
}

error_code DS2465::writeMasterSecret(const Hash & masterSecret) {
  error_code result =
      writeMemory(Scratchpad, masterSecret.data(), masterSecret.size());
  if (!result) {
    result = copyScratchpad(true, 0, false, 0);
  }
  if (!result) {
    (*sleep)(eepromPageWriteDelayMs);
  }
  return result;
}

error_code DS2465::copyScratchpad(bool destSecret, int pageNum, bool notFull,
                                  int segmentNum) {
  error_code result = make_error_code(ArgumentOutOfRangeError);
  if (pageNum >= 0 && segmentNum >= 0) {
    uint_least8_t command[] = {
        CopyScratchpadCmd,
        static_cast<uint_least8_t>(destSecret ? 0
                                              : (0x80 | (pageNum << 4) |
                                                 (notFull << 3) | segmentNum))};
    result =
        writeMemory(CommandReg, command, sizeof(command) / sizeof(command[0]));
  }
  return result;
}

error_code DS2465::configureLevel(Level level) {
  // Check if supported level
  if (!((level == NormalLevel) || (level == StrongLevel))) {
    return make_error_code(InvalidLevelError);
  }
  // Check if requested level already set
  if (curConfig.getSPU() == (level == StrongLevel)) {
    return error_code();
  }
  // Set the level
  Config newConfig = curConfig;
  newConfig.setSPU(level == StrongLevel);
  return writeConfig(newConfig);
}

error_code DS2465::setLevel(Level newLevel) {
  if (newLevel == StrongLevel) {
    return make_error_code(InvalidLevelError);
  }

  return configureLevel(newLevel);
}

error_code DS2465::setSpeed(Speed newSpeed) {
  // Check if supported speed
  if (!((newSpeed == OverdriveSpeed) || (newSpeed == StandardSpeed))) {
    return make_error_code(InvalidSpeedError);
  }
  // Check if requested speed is already set
  if (curConfig.get1WS() == (newSpeed == OverdriveSpeed)) {
    return error_code();
  }
  // Set the speed
  Config newConfig = curConfig;
  newConfig.set1WS(newSpeed == OverdriveSpeed);
  return writeConfig(newConfig);
}

error_code DS2465::triplet(TripletData & data) {
  // 1-Wire Triplet (Case B)
  //   S AD,0 [A] 1WT [A] SS [A] Sr AD,1 [A] [Status] A [Status] A\ P
  //                                         \--------/
  //                           Repeat until 1WB bit has changed to 0
  //  [] indicates from slave
  //  SS indicates byte containing search direction bit value in msbit

  const uint_least8_t command[] = {
      OwTripletCmd, static_cast<uint_least8_t>(data.writeBit ? 0x80 : 0x00)};
  error_code result =
      writeMemory(CommandReg, command, sizeof(command) / sizeof(command[0]));
  if (!result) {
    uint_least8_t status;
    result = pollBusy(&status);
    if (!result) {
      // check bit results in status byte
      data.readBit = ((status & Status_SBR) == Status_SBR);
      data.readBitComplement = ((status & Status_TSB) == Status_TSB);
      data.writeBit = ((status & Status_DIR) == Status_DIR);
    }
  }
  return result;
}

error_code DS2465::readBlock(uint_least8_t * recvBuf, size_t recvLen) {
  // 1-Wire Receive Block (Case A)
  //   S AD,0 [A] CommandReg [A] 1WRF [A] PR [A] P
  //  [] indicates from slave
  //  PR indicates byte containing parameter

  error_code result;
  while ((recvLen > 0) && !result) {
    const uint_least8_t command[] = {
        OwReceiveBlockCmd,
        static_cast<uint_least8_t>(std::min(recvLen, maxBlockSize))};
    result =
        writeMemory(CommandReg, command, sizeof(command) / sizeof(command[0]));
    if (!result) {
      result = pollBusy();
    }
    if (!result) {
      result = readMemory(Scratchpad, recvBuf, command[1]);
    }
    recvBuf += command[1];
    recvLen -= command[1];
  }
  return result;
}

error_code DS2465::writeBlock(const uint_least8_t * sendBuf, size_t sendLen) {
  error_code result;
  while ((sendLen > 0) && !result) {
    const uint_least8_t command[] = {
        OwTransmitBlockCmd,
        static_cast<uint_least8_t>(std::min(sendLen, maxBlockSize))};

    // prefill scratchpad with required data
    result = writeMemory(Scratchpad, sendBuf, command[1]);

    // 1-Wire Transmit Block (Case A)
    //   S AD,0 [A] CommandReg [A] 1WTB [A] PR [A] P
    //  [] indicates from slave
    //  PR indicates byte containing parameter
    if (!result) {
      result = writeMemory(CommandReg, command,
                           sizeof(command) / sizeof(command[0]));
    }
    if (!result) {
      result = pollBusy();
    }
    sendBuf += command[1];
    sendLen -= command[1];
  }
  return result;
}

error_code DS2465::writeMacBlock() const {
  // 1-Wire Transmit Block (Case A)
  //   S AD,0 [A] CommandReg [A] 1WTB [A] PR [A] P
  //  [] indicates from slave
  //  PR indicates byte containing parameter

  const uint_least8_t command[] = {OwTransmitBlockCmd, 0xFF};
  error_code result =
      writeMemory(CommandReg, command, sizeof(command) / sizeof(command[0]));
  if (!result) {
    result = pollBusy();
  }
  return result;
}

error_code DS2465::readByteSetLevel(uint_least8_t & recvByte,
                                    Level afterLevel) {
  // 1-Wire Read Bytes (Case C)
  //   S AD,0 [A] CommandReg [A] 1WRB [A] Sr AD,1 [A] [Status] A [Status] A
  //                                                  \--------/
  //                     Repeat until 1WB bit has changed to 0
  //   Sr AD,0 [A] SRP [A] E1 [A] Sr AD,1 [A] DD A\ P
  //
  //  [] indicates from slave
  //  DD data read

  error_code result = configureLevel(afterLevel);
  if (result) {
    return result;
  }

  uint_least8_t buf = OwReadByteCmd;
  result = writeMemory(CommandReg, &buf, 1);

  if (!result) {
    result = pollBusy();
  }

  if (!result) {
    result = readMemory(ReadDataReg, &buf, 1);
  }

  if (!result) {
    recvByte = buf;
  }

  return result;
}

error_code DS2465::writeByteSetLevel(uint_least8_t sendByte, Level afterLevel) {
  // 1-Wire Write Byte (Case B)
  //   S AD,0 [A] CommandReg [A] 1WWB [A] DD [A] Sr AD,1 [A] [Status] A [Status]
  //   A\ P
  //                                                           \--------/
  //                             Repeat until 1WB bit has changed to 0
  //  [] indicates from slave
  //  DD data to write

  error_code result = configureLevel(afterLevel);
  if (result) {
    return result;
  }

  const uint_least8_t command[] = {OwWriteByteCmd, sendByte};

  result =
      writeMemory(CommandReg, command, sizeof(command) / sizeof(command[0]));
  if (!result) {
    result = pollBusy();
  }

  return result;
}

error_code DS2465::touchBitSetLevel(bool & sendRecvBit, Level afterLevel) {
  // 1-Wire bit (Case B)
  //   S AD,0 [A] CommandReg [A] 1WSB [A] BB [A] Sr AD,1 [A] [Status] A [Status]
  //   A\ P
  //                                                          \--------/
  //                           Repeat until 1WB bit has changed to 0
  //  [] indicates from slave
  //  BB indicates byte containing bit value in msbit

  error_code result = configureLevel(afterLevel);
  if (result) {
    return result;
  }

  const uint_least8_t command[] = {
      OwSingleBitCmd, static_cast<uint_least8_t>(sendRecvBit ? 0x80 : 0x00)};
  uint_least8_t status;

  result =
      writeMemory(CommandReg, command, sizeof(command) / sizeof(command[0]));

  if (!result) {
    result = pollBusy(&status);
  }

  if (!result) {
    sendRecvBit = ((status & Status_SBR) == Status_SBR);
  }

  return result;
}

error_code DS2465::writeMemory(uint_least8_t addr, const uint_least8_t * buf,
                               size_t bufLen) const {
  // Write SRAM (Case A)
  //   S AD,0 [A] VSA [A] DD [A]  P
  //                      \-----/
  //                        Repeat for each data byte
  //  [] indicates from slave
  //  VSA valid SRAM memory address
  //  DD memory data to write

  error_code result = i2cMaster->start(i2cAddress_);
  if (result) {
    i2cMaster->stop();
    return result;
  }
  result = i2cMaster->writeByte(addr);
  if (result) {
    i2cMaster->stop();
    return result;
  }
  result = i2cMaster->writeBlock(buf, bufLen);
  if (result) {
    i2cMaster->stop();
    return result;
  }
  result = i2cMaster->stop();
  return result;
}

error_code DS2465::readMemory(uint_least8_t addr, uint_least8_t * buf,
                              size_t bufLen) const {
  // Read (Case A)
  //   S AD,0 [A] MA [A] Sr AD,1 [A] [DD] A [DD] A\ P
  //                                 \-----/
  //                                   Repeat for each data byte, NAK last byte
  //  [] indicates from slave
  //  MA memory address
  //  DD memory data read

  error_code result = i2cMaster->start(i2cAddress_);
  if (result) {
    i2cMaster->stop();
    return result;
  }
  result = i2cMaster->writeByte(addr);
  if (result) {
    i2cMaster->stop();
    return result;
  }
  result = readMemory(buf, bufLen);
  return result;
}

error_code DS2465::readMemory(uint_least8_t * buf, size_t bufLen) const {
  error_code result = i2cMaster->start(i2cAddress_ | 1);
  if (result) {
    i2cMaster->stop();
    return result;
  }
  result = i2cMaster->readBlock(I2CMaster::Nack, buf, bufLen);
  if (result) {
    i2cMaster->stop();
    return result;
  }
  result = i2cMaster->stop();
  return result;
}

error_code DS2465::writeConfig(Config config) {
  uint_least8_t configBuf = ((config.readByte() ^ 0xF) << 4) | config.readByte();
  error_code result = writeMemory(ConfigReg, &configBuf, 1);
  if (!result) {
    result = readMemory(ConfigReg, &configBuf, 1);
  }
  if (!result) {
    if (configBuf != config.readByte())
      result = make_error_code(HardwareError);
  }
  if (!result) {
    curConfig = config;
  }
  return result;
}

error_code DS2465::writePortParameter(PortParameter param, int val) {
  if (val < 0 || val > 15) {
    return make_error_code(ArgumentOutOfRangeError);
  }

  uint_least8_t addr = 0;
  switch (param) {
  case tRSTL_STD:
  case tRSTL_OD:
    addr = tRSTL_Reg;
    break;
  case tMSP_STD:
  case tMSP_OD:
    addr = tMSP_Reg;
    break;
  case tW0L_STD:
  case tW0L_OD:
    addr = tW0L_Reg;
    break;
  case tREC0:
    addr = tREC0_Reg;
    break;
  case RWPU:
    addr = RWPU_Reg;
    break;
  case tW1L_OD:
    addr = tW1L_Reg;
    break;
  }

  uint_least8_t data;
  error_code result = readMemory(addr, &data, 1);
  if (result) {
    return result;
  }

  uint_least8_t newData;
  if (param == tRSTL_OD || param == tMSP_OD || param == tW0L_OD) {
    newData = (data & 0x0F) | (val << 4);
  } else {
    newData = (data & 0xF0) | val;
  }

  if (newData != data) {
    result = writeMemory(addr, &newData, 1);
  }
  return result;
}

error_code DS2465::pollBusy(uint_least8_t * pStatus) const {
  const int pollLimit = 200;

  int pollCount = 0;
  uint_least8_t status;
  do {
    error_code result = readMemory(&status, 1);
    if (result) {
      return result;
    }
    if (pStatus != NULL) {
      *pStatus = status;
    }
    if (pollCount++ >= pollLimit) {
      return make_error_code(HardwareError);
    }
  } while (status & Status_1WB);

  return error_code();
}

error_code DS2465::reset() {
  // 1-Wire reset (Case B)
  //   S AD,0 [A] CommandReg  [A] 1WRS [A] Sr AD,1 [A] [Status] A [Status] A\ P
  //                                                  \--------/
  //                       Repeat until 1WB bit has changed to 0
  //  [] indicates from slave

  uint_least8_t buf = OwResetCmd;
  error_code result = writeMemory(CommandReg, &buf, 1);

  if (!result) {
    result = pollBusy(&buf);
  }

  if (!result) {
    if ((buf & Status_SD) == Status_SD) {
      result = make_error_code(ShortDetectedError);
    }
    else if ((buf & Status_PPD) != Status_PPD) {
      result = make_error_code(NoSlaveError);
    }
  }

  return result;
}

error_code DS2465::resetDevice() {
  // Device Reset
  //   S AD,0 [A] CommandReg [A] 1WMR [A] Sr AD,1 [A] [SS] A\ P
  //  [] indicates from slave
  //  SS status byte to read to verify state

  uint_least8_t buf = DeviceResetCmd;
  error_code result = writeMemory(CommandReg, &buf, 1);

  if (!result) {
    result = readMemory(&buf, 1);
  }

  if (!result) {
    if ((buf & 0xF7) != 0x10) {
      result = make_error_code(HardwareError);
    }
  }

  if (!result) {
    reset(); // do a command to get 1-Wire master reset out of holding state
  }

  return result;
}

error_code DS2465::computeNextMasterSecret(const SlaveSecretData & data) {
  error_code result = writeMemory(Scratchpad, data.data(), data.size());
  if (!result) {
    result = computeNextMasterSecret(false, 0, FullPage);
  }
  return result;
}

error_code DS2465::computeNextMasterSecretWithSwap(const SlaveSecretData & data,
                                                   int pageNum,
                                                   PageRegion region) {
  error_code result = writeMemory(Scratchpad, data.data(), data.size());
  if (!result) {
    result = computeNextMasterSecret(true, pageNum, region);
  }
  return result;
}

error_code DS2465::computeWriteMac(const WriteMacData & data) const {
  error_code result = writeMemory(Scratchpad, data.data(), data.size());
  if (!result) {
    result = computeWriteMac(false, false, 0, 0);
  }
  return result;
}

error_code DS2465::computeWriteMac(const WriteMacData & data,
                                   Hash & mac) const {
  error_code result = computeWriteMac(data);
  if (!result) {
    result = readMemory(mac.data(), mac.size());
  }
  return result;
}

error_code DS2465::computeAndTransmitWriteMac(const WriteMacData & data) const {
  error_code result = computeWriteMac(data);
  if (!result) {
    result = writeMacBlock();
  }
  return result;
}

error_code DS2465::computeWriteMacWithSwap(const WriteMacData & data,
                                           int pageNum, int segmentNum) const {
  error_code result = writeMemory(Scratchpad, data.data(), data.size());
  if (!result) {
    result = computeWriteMac(false, true, pageNum, segmentNum);
  }
  return result;
}

error_code DS2465::computeWriteMacWithSwap(const WriteMacData & data,
                                           int pageNum, int segmentNum,
                                           Hash & mac) const {
  error_code result = computeWriteMacWithSwap(data, pageNum, segmentNum);
  if (!result) {
    result = readMemory(mac.data(), mac.size());
  }
  return result;
}

error_code DS2465::computeAndTransmitWriteMacWithSwap(const WriteMacData & data,
                                                      int pageNum,
                                                      int segmentNum) const {
  error_code result = computeWriteMacWithSwap(data, pageNum, segmentNum);
  if (!result) {
    result = writeMacBlock();
  }
  return result;
}

error_code DS2465::computeSlaveSecret(const SlaveSecretData & data) {
  error_code result = writeMemory(Scratchpad, data.data(), data.size());
  if (!result) {
    result = computeSlaveSecret(false, 0, FullPage);
  }
  return result;
}

error_code DS2465::computeSlaveSecretWithSwap(const SlaveSecretData & data,
                                              int pageNum, PageRegion region) {
  error_code result = writeMemory(Scratchpad, data.data(), data.size());
  if (!result) {
    result = computeSlaveSecret(true, pageNum, region);
  }
  return result;
}

error_code DS2465::computeAuthMac(const AuthMacData & data) const {
  error_code result = writeMemory(Scratchpad, data.data(), data.size());
  if (!result) {
    result = computeAuthMac(false, 0, FullPage);
  }
  return result;
}

error_code DS2465::computeAuthMac(const AuthMacData & data, Hash & mac) const {
  error_code result = computeAuthMac(data);
  if (!result) {
    result = readMemory(mac.data(), mac.size());
  }
  return result;
}

error_code DS2465::computeAndTransmitAuthMac(const AuthMacData & data) const {
  error_code result = computeAuthMac(data);
  if (!result) {
    result = writeMacBlock();
  }
  return result;
}

error_code DS2465::computeAuthMacWithSwap(const AuthMacData & data, int pageNum,
                                          PageRegion region) const {
  error_code result = writeMemory(Scratchpad, data.data(), data.size());
  if (!result) {
    result = computeAuthMac(true, pageNum, region);
  }
  return result;
}

error_code DS2465::computeAuthMacWithSwap(const AuthMacData & data, int pageNum,
                                          PageRegion region, Hash & mac) const {
  error_code result = computeAuthMacWithSwap(data, pageNum, region);
  if (!result) {
    result = readMemory(mac.data(), mac.size());
  }
  return result;
}

error_code DS2465::computeAndTransmitAuthMacWithSwap(const AuthMacData & data,
                                                     int pageNum,
                                                     PageRegion region) const {
  error_code result = computeAuthMacWithSwap(data, pageNum, region);
  if (!result) {
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

      default:
        return defaultErrorMessage(condition);
      }
    }
  } instance;
  return instance;
}

} // namespace MaximInterface
