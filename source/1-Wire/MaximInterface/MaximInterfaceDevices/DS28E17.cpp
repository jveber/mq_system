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
#include <MaximInterfaceCore/Crc.hpp>
#include <MaximInterfaceCore/Error.hpp>
#include <MaximInterfaceCore/OneWireMaster.hpp>
#include "DS28E17.hpp"

#define TRY MaximInterfaceCore_TRY
#define TRY_VALUE MaximInterfaceCore_TRY_VALUE

namespace MaximInterfaceDevices {

using namespace Core;

Result<void> DS28E17::writeDataWithStop(uint_least8_t I2C_addr,
                                        span<const uint_least8_t> data) {
  return sendPacket(WriteDataWithStopCmd, &I2C_addr, data,
                    span<uint_least8_t>());
}

Result<void> DS28E17::writeDataNoStop(uint_least8_t I2C_addr,
                                      span<const uint_least8_t> data) {
  return sendPacket(WriteDataNoStopCmd, &I2C_addr, data, span<uint_least8_t>());
}

Result<void> DS28E17::writeDataOnly(span<const uint_least8_t> data) {
  return sendPacket(WriteDataOnlyCmd, NULL, data, span<uint_least8_t>());
}

Result<void> DS28E17::writeDataOnlyWithStop(span<const uint_least8_t> data) {
  return sendPacket(WriteDataOnlyWithStopCmd, NULL, data,
                    span<uint_least8_t>());
}

Result<void>
DS28E17::writeReadDataWithStop(uint_least8_t I2C_addr,
                               span<const uint_least8_t> write_data,
                               span<uint_least8_t> read_data) {
  return sendPacket(WriteReadDataWithStopCmd, &I2C_addr, write_data, read_data);
}

Result<void> DS28E17::readDataWithStop(uint_least8_t I2C_addr,
                                       span<uint_least8_t> data) {
  return sendPacket(ReadDataWithStopCmd, &I2C_addr, span<const uint_least8_t>(),
                    data);
}

Result<void> DS28E17::writeConfigReg(I2CSpeed speed) {
  Result<void> result = selectRom(*master);
  if (result) {
    // Send CMD and Data
    const uint_least8_t send_block[] = {WriteConfigurationCmd,
                                        static_cast<uint_least8_t>(speed)};
    result = master->writeBlock(send_block);
  }
  return result;
}

Result<DS28E17::I2CSpeed> DS28E17::readConfigReg() const {
  TRY(selectRom(*master));
  // Send CMD and receive Data
  TRY(master->writeByte(ReadConfigurationCmd));
  uint_least8_t config;
  TRY_VALUE(config, master->readByte());
  switch (config) {
  case Speed100kHz:
  case Speed400kHz:
  case Speed900kHz:
    return static_cast<I2CSpeed>(config);
  }
  return OutOfRangeError;
}

Result<void> DS28E17::enableSleepMode() {
  Result<void> result = selectRom(*master);
  if (result) {
    // Send CMD
    result = master->writeByte(EnableSleepModeCmd);
  }
  return result;
}

Result<uint_least8_t> DS28E17::readDeviceRevision() const {
  Result<void> result = selectRom(*master);
  if (!result) {
    return result.error();
  }
  result = master->writeByte(ReadDeviceRevisionCmd);
  if (!result) {
    return result.error();
  }
  return master->readByte();
}

Result<void> DS28E17::sendPacket(Command command,
                                 const uint_least8_t * I2C_addr,
                                 span<const uint_least8_t> write_data,
                                 span<uint_least8_t> read_data) {
  const int pollLimit = 10000;
  const span<const uint_least8_t>::index_type maxDataLen = 255;

  if ((!write_data.empty() && write_data.size() > maxDataLen) ||
      (!read_data.empty() && read_data.size() > maxDataLen)) {
    return OutOfRangeError;
  }

  Result<void> result = selectRom(*master);
  if (!result) {
    return result;
  }
  uint_fast16_t crc16 = calculateCrc16(command);
  result = master->writeByte(command);
  if (!result) {
    return result;
  }
  if (I2C_addr) {
    crc16 = calculateCrc16(crc16, *I2C_addr);
    result = master->writeByte(*I2C_addr);
    if (!result) {
      return result;
    }
  }
  if (!write_data.empty()) {
    crc16 = calculateCrc16(crc16, static_cast<uint_fast8_t>(write_data.size()));
    result = master->writeByte(static_cast<uint_least8_t>(write_data.size()));
    if (!result) {
      return result;
    }
    crc16 = calculateCrc16(crc16, write_data);
    result = master->writeBlock(write_data);
    if (!result) {
      return result;
    }
  }
  if (!read_data.empty()) {
    crc16 = calculateCrc16(crc16, static_cast<uint_fast8_t>(read_data.size()));
    result = master->writeByte(static_cast<uint_least8_t>(read_data.size()));
    if (!result) {
      return result;
    }
  }
  crc16 ^= 0xFFFF;
  const uint_least8_t crc16Bytes[] = {static_cast<uint_least8_t>(crc16),
                                      static_cast<uint_least8_t>(crc16 >> 8)};
  result = master->writeBlock(crc16Bytes);
  if (!result) {
    return result;
  }
  // Poll for Zero 1-Wire bit and return if an error occurs
  int poll_count = 0;
  bool recvBit;
  do {
    if (poll_count++ < pollLimit) {
      return TimeoutError;
    }
    TRY_VALUE(recvBit, master->readBit());
  } while (recvBit);
  uint_least8_t status;
  TRY_VALUE(status, master->readByte());
  if ((status & 0x1) == 0x1) {
    return InvalidCrc16Error;
  }
  if ((status & 0x2) == 0x2) {
    return AddressNackError;
  }
  if ((status & 0x8) == 0x8) {
    return InvalidStartError;
  }
  if (!write_data.empty()) {
    TRY_VALUE(status, master->readByte());
    if (status != 0) {
      return error_code(status, errorCategory());
    }
  }
  if (!read_data.empty()) {
    result = master->readBlock(read_data);
  }
  return result;
}

const error_category & DS28E17::errorCategory() {
  static class : public error_category {
  public:
    virtual const char * name() const { return "DS28E17"; }

    virtual std::string message(int condition) const {
      switch (condition) {
      case TimeoutError:
        return "Timeout Error";

      case OutOfRangeError:
        return "Out of Range Error";

      case InvalidCrc16Error:
        return "Invalid CRC16 Error";

      case AddressNackError:
        return "Address Nack Error";

      case InvalidStartError:
        return "Invalid Start Error";
      }
      if (condition >= 1 && condition <= 255) {
        return "Write Nack Error";
      }
      return defaultErrorMessage(condition);
    }
  } instance;
  return instance;
}

} // namespace MaximInterfaceDevices
