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
#include "DS2413.hpp"

#define TRY MaximInterfaceCore_TRY
#define TRY_VALUE MaximInterfaceCore_TRY_VALUE

namespace MaximInterfaceDevices {

using namespace Core;

Result<DS2413::Status> DS2413::readStatus() const {
  return Result<DS2413::Status>(pioAccessRead());
}

Result<void> DS2413::writeOutputState(bool pioAState, bool pioBState) {
  uint_least8_t val = 0xFC;
  if (pioAState) {
    val |= 0x1;
  }
  if (pioBState) {
    val |= 0x2;
  }
  return pioAccessWrite(val);
}

Result<uint_least8_t> DS2413::pioAccessRead() const {
  TRY(selectRom(*master));
  TRY(master->writeByte(0xF5));
  uint_least8_t result;
  TRY_VALUE(result, master->readByte());
  return (result == ((result ^ 0xF0) >> 4)) ? makeResult(result)
                                            : CommunicationError;
}

Result<void> DS2413::pioAccessWrite(uint_least8_t val) {
  TRY(selectRom(*master));
  const uint_least8_t block[] = {0x5A, val,
                                 static_cast<uint_least8_t>(val ^ 0xFF)};
  TRY(master->writeBlock(block));
  TRY_VALUE(val, master->readByte());
  return (val == 0xAA) ? makeResult(none) : CommunicationError;
}

const error_category & DS2413::errorCategory() {
  static class : public error_category {
  public:
    virtual const char * name() const { return "DS2413"; }

    virtual std::string message(int condition) const {
      switch (condition) {
      case CommunicationError:
        return "Communication Error";
      }
      return defaultErrorMessage(condition);
    }
  } instance;
  return instance;
}

Result<void> writePioAOutputState(DS2413 & ds2413, bool pioAState) {
  DS2413::Status status;
  TRY_VALUE(status, ds2413.readStatus());
  if (pioAState != status[DS2413::PioAOutputState]) {
    TRY(ds2413.writeOutputState(pioAState, status[DS2413::PioBOutputState]));
  }
  return none;
}

Result<void> writePioBOutputState(DS2413 & ds2413, bool pioBState) {
  DS2413::Status status;
  TRY_VALUE(status, ds2413.readStatus());
  if (pioBState != status[DS2413::PioBOutputState]) {
    TRY(ds2413.writeOutputState(status[DS2413::PioAOutputState], pioBState));
  }
  return none;
}

} // namespace MaximInterfaceDevices
