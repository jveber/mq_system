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

#ifndef MaximInterfaceDevices_DS2480B_hpp
#define MaximInterfaceDevices_DS2480B_hpp

#include <MaximInterfaceCore/OneWireMaster.hpp>
#include <MaximInterfaceCore/Sleep.hpp>
#include <MaximInterfaceCore/Uart.hpp>
#include "Config.hpp"

namespace MaximInterfaceDevices {

/// Serial to 1-Wire Line Driver
class DS2480B : public Core::OneWireMaster {
public:
  enum ErrorValue { HardwareError = 1 };

  DS2480B(Core::Sleep & sleep, Core::Uart & uart)
      : sleep(&sleep), uart(&uart) {}

  void setSleep(Core::Sleep & sleep) { this->sleep = &sleep; }

  void setUart(Core::Uart & uart) { this->uart = &uart; }

  MaximInterfaceDevices_EXPORT Core::Result<void> initialize();

  MaximInterfaceDevices_EXPORT virtual Core::Result<void> reset();

  MaximInterfaceDevices_EXPORT virtual Core::Result<bool>
  touchBitSetLevel(bool sendBit, Level afterLevel);

  MaximInterfaceDevices_EXPORT virtual Core::Result<void>
  writeByteSetLevel(uint_least8_t sendByte, Level afterLevel);

  MaximInterfaceDevices_EXPORT virtual Core::Result<uint_least8_t>
  readByteSetLevel(Level afterLevel);

  MaximInterfaceDevices_EXPORT virtual Core::Result<void>
  setSpeed(Speed newSpeed);

  MaximInterfaceDevices_EXPORT virtual Core::Result<void>
  setLevel(Level newLevel);

  MaximInterfaceDevices_EXPORT static const Core::error_category &
  errorCategory();

protected:
  MaximInterfaceDevices_EXPORT Core::Result<void>
  sendCommand(uint_least8_t command);

private:
  const Core::Sleep * sleep;
  Core::Uart * uart;

  Level level;
  uint_least8_t mode;
  uint_least8_t speed;
};

} // namespace MaximInterfaceDevices
namespace MaximInterfaceCore {

template <>
struct is_error_code_enum<MaximInterfaceDevices::DS2480B::ErrorValue>
    : true_type {};

} // namespace MaximInterfaceCore
namespace MaximInterfaceDevices {

inline Core::error_code make_error_code(DS2480B::ErrorValue e) {
  return Core::error_code(e, DS2480B::errorCategory());
}

} // namespace MaximInterfaceDevices

#endif
