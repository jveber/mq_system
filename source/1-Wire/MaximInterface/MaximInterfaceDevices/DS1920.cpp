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
#include <MaximInterfaceCore/OneWireMaster.hpp>
#include "DS1920.hpp"

namespace MaximInterfaceDevices {

using namespace Core;

Result<void> DS1920::writeScratchpad(uint_least8_t th, uint_least8_t tl) {
  Result<void> result = selectRom(*master);
  if (result) {
    const uint_least8_t sendBlock[] = {0x4E, th, tl};
    result = master->writeBlock(sendBlock);
  }
  return result;
}

Result<DS1920::Scratchpad> DS1920::readScratchpad() const {
  Result<void> result = selectRom(*master);
  if (!result) {
    return result.error();
  }
  result = master->writeByte(0xBE);
  if (!result) {
    return result.error();
  }
  Scratchpad scratchpad;
  result = master->readBlock(scratchpad);
  if (!result) {
    return result.error();
  }
  uint_least8_t receivedCrc;
  MaximInterfaceCore_TRY_VALUE(receivedCrc, master->readByte());
  if (receivedCrc != calculateCrc8(scratchpad)) {
    return CrcError;
  }
  return scratchpad;
}

Result<void> DS1920::copyScratchpad() {
  Result<void> result = selectRom(*master);
  if (result) {
    result = master->writeByteSetLevel(0x48, OneWireMaster::StrongLevel);
    if (result) {
      sleep->invoke(10);
      result = master->setLevel(OneWireMaster::NormalLevel);
    }
  }
  return result;
}

Result<void> DS1920::convertTemperature() {
  Result<void> result = selectRom(*master);
  if (result) {
    result = master->writeByteSetLevel(0x44, OneWireMaster::StrongLevel);
    if (result) {
      sleep->invoke(750);
      result = master->setLevel(OneWireMaster::NormalLevel);
    }
  }
  return result;
}

Result<void> DS1920::recallEeprom() {
  Result<void> result = selectRom(*master);
  if (result) {
    result = master->writeByte(0xB8);
  }
  return result;
}

const error_category & DS1920::errorCategory() {
  static class : public error_category {
  public:
    virtual const char * name() const { return "DS1920"; }

    virtual std::string message(int condition) const {
      switch (condition) {
      case CrcError:
        return "CRC Error";

      case DataError:
        return "Data Error";
      }
      return defaultErrorMessage(condition);
    }
  } instance;
  return instance;
}

Result<int> readTemperature(DS1920 & ds1920) {
  MaximInterfaceCore_TRY(ds1920.convertTemperature());
  DS1920::Scratchpad scratchpad;
  MaximInterfaceCore_TRY_VALUE(scratchpad, ds1920.readScratchpad());

  unsigned int tempData =
      (static_cast<unsigned int>(scratchpad[1]) << 8) | scratchpad[0];
  const unsigned int signMask = 0xFF00;
  int temperature;
  if ((tempData & signMask) == signMask) {
    temperature = -0x100;
    tempData &= ~signMask;
  } else if ((tempData & signMask) == 0) {
    temperature = 0;
  } else {
    return DS1920::DataError;
  }
  temperature += static_cast<int>(tempData);
  return temperature;
}

} // namespace MaximInterfaceDevices
