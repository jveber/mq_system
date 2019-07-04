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

#include <MaximInterface/Links/OneWireMaster.hpp>
#include <MaximInterface/Utilities/Error.hpp>
#include "DS1920.hpp"

namespace MaximInterface {

enum DS1920_CMDS {
  WRITE_SCRATCHPAD = 0x4E,
  READ_SCRATCHPAD = 0xBE,
  COPY_SCRATCHPAD = 0x48,
  CONV_TEMPERATURE = 0x44,
  RECALL = 0xB8
};

error_code DS1920::writeScratchpad(uint_least8_t th, uint_least8_t tl) {
  error_code result = selectRom(*master);
  if (!result) {
    const uint_least8_t sendBlock[] = {WRITE_SCRATCHPAD, th, tl};
    result =
        master->writeBlock(sendBlock, sizeof(sendBlock) / sizeof(sendBlock[0]));
  }
  return result;
}

error_code DS1920::readScratchpad(Scratchpad & scratchpad) {
  error_code result = selectRom(*master);
  if (!result) {
    result = master->writeByte(READ_SCRATCHPAD);
    if (!result) {
      result = master->readBlock(scratchpad.data(), scratchpad.size());
      if (!result) {
        uint_least8_t receivedCrc;
        result = master->readByte(receivedCrc);
        if (!result && (receivedCrc !=
                        calculateCrc8(scratchpad.data(), scratchpad.size()))) {
          result = make_error_code(CrcError);
        }
      }
    }
  }
  return result;
}

error_code DS1920::copyScratchpad() {
  error_code result = selectRom(*master);
  if (!result) {
    result =
        master->writeByteSetLevel(COPY_SCRATCHPAD, OneWireMaster::StrongLevel);
    if (!result) {
      (*sleep)(10);
      result = master->setLevel(OneWireMaster::NormalLevel);
    }
  }
  return result;
}

error_code DS1920::convertTemperature() {
  error_code result = selectRom(*master);
  if (!result) {
    result =
        master->writeByteSetLevel(CONV_TEMPERATURE, OneWireMaster::StrongLevel);
    if (!result) {
      (*sleep)(750);
      result = master->setLevel(OneWireMaster::NormalLevel);
    }
  }
  return result;
}

error_code DS1920::recallEeprom() {
  error_code result = selectRom(*master);
  if (!result) {
    result = master->writeByte(RECALL);
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

      default:
        return defaultErrorMessage(condition);
      }
    }
  } instance;
  return instance;
}

error_code readTemperature(DS1920 & ds1920, int & temperature) {
  error_code result = ds1920.convertTemperature();
  if (result) {
    return result;
  }
  DS1920::Scratchpad scratchpad;
  result = ds1920.readScratchpad(scratchpad);
  if (result) {
    return result;
  }

  unsigned int tempData =
      (static_cast<unsigned int>(scratchpad[1]) << 8) | scratchpad[0];
  const unsigned int signMask = 0xFF00;
  if ((tempData & signMask) == signMask) {
    temperature = -0x100;
    tempData &= ~signMask;
  } else if ((tempData & signMask) == 0) {
    temperature = 0;
  } else {
    return make_error_code(DS1920::DataError);
  }
  temperature += static_cast<int>(tempData);
  return error_code();
}

} // namespace MaximInterface
