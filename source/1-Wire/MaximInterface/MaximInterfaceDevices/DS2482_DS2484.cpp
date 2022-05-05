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
#include "DS2482_DS2484.hpp"

#define TRY MaximInterfaceCore_TRY
#define TRY_VALUE MaximInterfaceCore_TRY_VALUE

namespace MaximInterfaceDevices {

using namespace Core;

// Device status bits.
static const uint_least8_t status_1WB = 0x01;
static const uint_least8_t status_PPD = 0x02;
static const uint_least8_t status_SD = 0x04;
static const uint_least8_t status_SBR = 0x20;
static const uint_least8_t status_TSB = 0x40;
static const uint_least8_t status_DIR = 0x80;

Result<void> DS2482_DS2484::initialize(Config config) {
  TRY(resetDevice());
  // Write the default configuration setup.
  TRY(writeConfig(config));
  return none;
}

Result<void> DS2482_DS2484::resetDevice() {
  // Device Reset
  //   S AD,0 [A] DRST [A] Sr AD,1 [A] [SS] A\ P
  //  [] indicates from slave
  //  SS status byte to read to verify state

  TRY(sendCommand(0xF0));

  uint_least8_t buf;
  TRY_VALUE(buf, readRegister());

  if ((buf & 0xF7) != 0x10) {
    return HardwareError;
  }

  // Do a command to get 1-Wire master reset out of holding state.
  reset();

  return none;
}

Result<OneWireMaster::TripletData> DS2482_DS2484::triplet(bool sendBit) {
  // 1-Wire Triplet (Case B)
  //   S AD,0 [A] 1WT [A] SS [A] Sr AD,1 [A] [Status] A [Status] A\ P
  //                                         \--------/
  //                           Repeat until 1WB bit has changed to 0
  //  [] indicates from slave
  //  SS indicates byte containing search direction bit value in msbit

  TRY(sendCommand(0x78, sendBit ? 0x80 : 0x00));

  uint_least8_t status;
  TRY_VALUE(status, pollBusy());

  TripletData data;
  data.readBit = ((status & status_SBR) == status_SBR);
  data.readBitComplement = ((status & status_TSB) == status_TSB);
  data.writeBit = ((status & status_DIR) == status_DIR);
  return data;
}

Result<void> DS2482_DS2484::reset() {
  // 1-Wire reset (Case B)
  //   S AD,0 [A] 1WRS [A] Sr AD,1 [A] [Status] A [Status] A\ P
  //                                   \--------/
  //                       Repeat until 1WB bit has changed to 0
  //  [] indicates from slave

  TRY(sendCommand(0xB4));

  uint_least8_t status;
  TRY_VALUE(status, pollBusy());

  if ((status & status_SD) == status_SD) {
    return ShortDetectedError;
  }
  if ((status & status_PPD) != status_PPD) {
    return NoSlaveError;
  }

  return none;
}

Result<bool> DS2482_DS2484::touchBitSetLevel(bool sendBit, Level afterLevel) {
  // 1-Wire bit (Case B)
  //   S AD,0 [A] 1WSB [A] BB [A] Sr AD,1 [A] [Status] A [Status] A\ P
  //                                          \--------/
  //                           Repeat until 1WB bit has changed to 0
  //  [] indicates from slave
  //  BB indicates byte containing bit value in msbit

  TRY(configureLevel(afterLevel));
  TRY(sendCommand(0x87, sendBit ? 0x80 : 0x00));
  uint_least8_t status;
  TRY_VALUE(status, pollBusy());
  return (status & status_SBR) == status_SBR;
}

Result<void> DS2482_DS2484::writeByteSetLevel(uint_least8_t sendByte,
                                              Level afterLevel) {
  // 1-Wire Write Byte (Case B)
  //   S AD,0 [A] 1WWB [A] DD [A] Sr AD,1 [A] [Status] A [Status] A\ P
  //                                          \--------/
  //                             Repeat until 1WB bit has changed to 0
  //  [] indicates from slave
  //  DD data to write

  TRY(configureLevel(afterLevel));
  TRY(sendCommand(0xA5, sendByte));
  TRY(pollBusy());
  return none;
}

Result<uint_least8_t> DS2482_DS2484::readByteSetLevel(Level afterLevel) {
  // 1-Wire Read Bytes (Case C)
  //   S AD,0 [A] 1WRB [A] Sr AD,1 [A] [Status] A [Status] A
  //                                   \--------/
  //                     Repeat until 1WB bit has changed to 0
  //   Sr AD,0 [A] SRP [A] E1 [A] Sr AD,1 [A] DD A\ P
  //
  //  [] indicates from slave
  //  DD data read

  TRY(configureLevel(afterLevel));
  TRY(sendCommand(0x96));
  TRY(pollBusy());
  return readRegister(0xE1);
}

Result<void> DS2482_DS2484::setSpeed(Speed newSpeed) {
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

Result<void> DS2482_DS2484::setLevel(Level newLevel) {
  if (newLevel == StrongLevel) {
    return InvalidLevelError;
  }

  return configureLevel(newLevel);
}

Result<void> DS2482_DS2484::writeConfig(Config config) {
  uint_least8_t configBuf =
      ((config.readByte() ^ 0xF) << 4) | config.readByte();
  TRY(sendCommand(0xD2, configBuf));

  TRY_VALUE(configBuf, readRegister(0xC3));

  if (configBuf != config.readByte()) {
    return HardwareError;
  }

  curConfig = config;
  return none;
}

Result<uint_least8_t> DS2482_DS2484::readRegister(uint_least8_t reg) const {
  TRY(sendCommand(0xE1, reg));
  return readRegister();
}

Result<uint_least8_t> DS2482_DS2484::readRegister() const {
  uint_least8_t buf;
  TRY(master->readPacket(address_, make_span(&buf, 1), I2CMaster::Stop));
  return buf;
}

Result<uint_least8_t> DS2482_DS2484::pollBusy() {
  const int pollLimit = 200;

  int pollCount = 0;
  uint_least8_t status;
  do {
    TRY_VALUE(status, readRegister());
    if (pollCount++ >= pollLimit) {
      return HardwareError;
    }
  } while ((status & status_1WB) == status_1WB);
  return status;
}

Result<void> DS2482_DS2484::configureLevel(Level level) {
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

Result<void> DS2482_DS2484::sendCommand(uint_least8_t cmd) const {
  return master->writePacket(address_, make_span(&cmd, 1), I2CMaster::Stop);
}

Result<void> DS2482_DS2484::sendCommand(uint_least8_t cmd,
                                        uint_least8_t param) const {
  const uint_least8_t buf[] = {cmd, param};
  return master->writePacket(address_, buf, I2CMaster::Stop);
}

const error_category & DS2482_DS2484::errorCategory() {
  static class : public error_category {
  public:
    virtual const char * name() const { return "DS2482_DS2484"; }

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

Result<void> DS2482_800::selectChannel(int channel) {
  // Channel Select (Case A)
  //   S AD,0 [A] CHSL [A] CC [A] Sr AD,1 [A] [RR] A\ P
  //  [] indicates from slave
  //  CC channel value
  //  RR channel read back

  uint_least8_t ch;
  uint_least8_t ch_read;
  switch (channel) {
  case 0:
    ch = 0xF0;
    ch_read = 0xB8;
    break;

  case 1:
    ch = 0xE1;
    ch_read = 0xB1;
    break;

  case 2:
    ch = 0xD2;
    ch_read = 0xAA;
    break;

  case 3:
    ch = 0xC3;
    ch_read = 0xA3;
    break;

  case 4:
    ch = 0xB4;
    ch_read = 0x9C;
    break;

  case 5:
    ch = 0xA5;
    ch_read = 0x95;
    break;

  case 6:
    ch = 0x96;
    ch_read = 0x8E;
    break;

  case 7:
    ch = 0x87;
    ch_read = 0x87;
    break;

  default:
    return ArgumentOutOfRangeError;
  };

  TRY(sendCommand(0xC3, ch));
  TRY_VALUE(ch, readRegister());
  // check for failure due to incorrect read back of channel
  if (ch != ch_read) {
    return HardwareError;
  }

  return none;
}

Result<void> DS2484::adjustPort(PortParameter param, int val) {
  if (val < 0 || val > 15) {
    return ArgumentOutOfRangeError;
  }

  TRY(sendCommand(0xC3, (param << 4) | val));

  uint_least8_t portConfig = val + 1;
  for (int reads = -1; reads < param; ++reads) {
    TRY_VALUE(portConfig, readRegister());
  }
  if (val != portConfig) {
    return HardwareError;
  }

  return none;
}

} // namespace MaximInterfaceDevices
