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

#ifndef MaximInterfaceMbed_Uart_hpp
#define MaximInterfaceMbed_Uart_hpp

#include <MaximInterfaceCore/Uart.hpp>
#include <mbed-os/drivers/Serial.h>

namespace MaximInterfaceMbed {

/// Wrapper for mbed::Serial.
class Uart : public MaximInterfaceCore::Uart {
public:
  enum ErrorValue {
    HardwareError = 1 ///< Write operation aborted due to timeout.
  };

  explicit Uart(mbed::Serial & serial) : serial(&serial) {}

  void setSerial(mbed::Serial & serial) { this->serial = &serial; }

  virtual MaximInterfaceCore::Result<void> setBaudRate(int_least32_t baudRate);

  virtual MaximInterfaceCore::Result<void> sendBreak();

  virtual MaximInterfaceCore::Result<void> clearReadBuffer();

  virtual MaximInterfaceCore::Result<void> writeByte(uint_least8_t data);

  virtual MaximInterfaceCore::Result<uint_least8_t> readByte();

  static const MaximInterfaceCore::error_category & errorCategory();

private:
  mbed::Serial * serial;
};

} // namespace MaximInterfaceMbed
namespace MaximInterfaceCore {

template <>
struct is_error_code_enum<MaximInterfaceMbed::Uart::ErrorValue> : true_type {};

} // namespace MaximInterfaceCore
namespace MaximInterfaceMbed {

inline MaximInterfaceCore::error_code make_error_code(Uart::ErrorValue e) {
  return MaximInterfaceCore::error_code(e, Uart::errorCategory());
}

} // namespace MaximInterfaceMbed

#endif
