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
#include <mbed-os/drivers/Timer.h>
#include "Uart.hpp"

namespace MaximInterfaceMbed {

using namespace MaximInterfaceCore;

static const int timeout_ms = 10000;

Result<void> Uart::setBaudRate(int_least32_t baudRate) {
  serial->baud(baudRate);
  return none;
}

Result<void> Uart::sendBreak() {
  serial->send_break();
  return none;
}

Result<void> Uart::clearReadBuffer() {
  while (serial->readable()) {
    serial->getc();
  }
  return none;
}

Result<void> Uart::writeByte(uint_least8_t data) {
  if (!serial->writeable()) {
    mbed::Timer timer;
    timer.start();
    while (!serial->writeable()) {
      if (timer.read_ms() >= timeout_ms) {
        return HardwareError;
      }
    }
  }
  serial->putc(data);
  return none;
}

Result<uint_least8_t> Uart::readByte() {
  if (!serial->readable()) {
    mbed::Timer timer;
    timer.start();
    while (!serial->readable()) {
      if (timer.read_ms() >= timeout_ms) {
        return TimeoutError;
      }
    }
  }
  return serial->getc();
}

const error_category & Uart::errorCategory() {
  static class : public error_category {
  public:
    virtual const char * name() const { return "Mbed UART"; }

    virtual std::string message(int condition) const {
      switch (condition) {
      case HardwareError:
        return "Hardware Error";
      }
      return defaultErrorMessage(condition);
    }
  } instance;
  return instance;
}

} // namespace MaximInterfaceMbed
