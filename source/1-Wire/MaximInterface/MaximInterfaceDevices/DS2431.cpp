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
#include <MaximInterfaceCore/Error.hpp>
#include <MaximInterfaceCore/OneWireMaster.hpp>
#include "DS2431.hpp"

namespace MaximInterfaceDevices {

using namespace Core;

Result<void> writeMemory(DS2431 & device, uint_least8_t targetAddress,
                         DS2431::Scratchpad::const_span data) {
  Result<void> result = device.writeScratchpad(targetAddress, data);
  if (!result) {
    return result;
  }
  uint_least8_t esByte;
  if (const Result<std::pair<uint_least8_t, DS2431::Scratchpad::array> >
          scratchpad = device.readScratchpad()) {
    esByte = scratchpad.value().first;
  } else {
    return scratchpad.error();
  }
  result = device.copyScratchpad(targetAddress, esByte);
  return result;
}

Result<void> DS2431::readMemory(uint_least8_t beginAddress,
                                span<uint_least8_t> data) const {
  Result<void> result = selectRom(*master);
  if (!result) {
    return result;
  }
  const uint_least8_t sendBlock[] = {0xF0, beginAddress, 0x00};
  result = master->writeBlock(sendBlock);
  if (!result) {
    return result;
  }
  result = master->readBlock(data);
  return result;
}

Result<void> DS2431::writeScratchpad(uint_least8_t targetAddress,
                                     Scratchpad::const_span data) {
  Result<void> result = selectRom(*master);
  if (!result) {
    return result;
  }
  uint_least8_t block[3 + Scratchpad::size] = {0x0F, targetAddress, 0x00};
  std::copy(data.begin(), data.end(), block + 3);
  result = master->writeBlock(block);
  if (!result) {
    return result;
  }
  const uint_fast16_t calculatedCrc = calculateCrc16(block) ^ 0xFFFF;
  result = master->readBlock(make_span(block, 2));
  if (!result) {
    return result;
  }
  if (calculatedCrc !=
      ((static_cast<uint_fast16_t>(block[1]) << 8) | block[0])) {
    result = CrcError;
  }
  return result;
}

Result<std::pair<uint_least8_t, DS2431::Scratchpad::array> >
DS2431::readScratchpad() const {
  typedef array<uint_least8_t, 6 + Scratchpad::size> Block;

  Result<void> result = selectRom(*master);
  if (!result) {
    return result.error();
  }
  Block block = {0xAA};
  result = master->writeByte(block.front());
  if (!result) {
    return result.error();
  }
  result = master->readBlock(make_span(block).subspan(1));
  if (!result) {
    return result.error();
  }
  Block::const_iterator blockIt = block.end();
  uint_fast16_t receivedCrc = static_cast<uint_fast16_t>(*(--blockIt)) << 8;
  receivedCrc |= *(--blockIt);
  const uint_fast16_t expectedCrc =
      calculateCrc16(make_span(block.data(), block.size() - 2)) ^ 0xFFFF;
  if (expectedCrc != receivedCrc) {
    return CrcError;
  }
  std::pair<uint_least8_t, Scratchpad::array> data;
  Block::const_iterator blockItEnd = blockIt;
  blockIt -= data.second.size();
  std::copy(blockIt, blockItEnd, data.second.begin());
  data.first = *(--blockIt);
  return data;
}

Result<void> DS2431::copyScratchpad(uint_least8_t targetAddress,
                                    uint_least8_t esByte) {
  Result<void> result = selectRom(*master);
  if (!result) {
    return result;
  }
  uint_least8_t block[] = {0x55, targetAddress, 0x00};
  result = master->writeBlock(block);
  if (!result) {
    return result;
  }
  result = master->writeByteSetLevel(esByte, OneWireMaster::StrongLevel);
  if (!result) {
    return result;
  }
  sleep->invoke(10);
  result = master->setLevel(OneWireMaster::NormalLevel);
  if (!result) {
    return result;
  }
  MaximInterfaceCore_TRY_VALUE(block[0], master->readByte());
  if (block[0] != 0xAA) {
    result = OperationFailure;
  }
  return result;
}

const error_category & DS2431::errorCategory() {
  static class : public error_category {
  public:
    virtual const char * name() const { return "DS2431"; }

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

} // namespace MaximInterfaceDevices
