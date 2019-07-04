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

#include <QSerialPortInfo>
#include <MaximInterface/Utilities/Error.hpp>
#include "SerialPort.hpp"
#include "Sleep.hpp"

using std::string;

namespace MaximInterface {
namespace Qt {

static_assert(QSerialPort::NoError == 0, "Remapping required for error_code.");

error_code SerialPort::connect(const string & portName) {
  port.setPort(QSerialPortInfo(QString::fromStdString(portName)));
  auto result = port.open(QSerialPort::ReadWrite)
                    ? error_code()
                    : make_error_code(port.error());
  if (result) {
    return result;
  }
  result = port.setDataTerminalReady(true) ? error_code()
                                           : make_error_code(port.error());
  if (result) {
    port.close();
  }
  return result;
}

error_code SerialPort::disconnect() {
  port.close();
  return error_code();
}

bool SerialPort::connected() const { return port.isOpen(); }

string SerialPort::portName() const { return port.portName().toStdString(); }

error_code SerialPort::setBaudRate(int_least32_t baudRate) {
  return port.setBaudRate(baudRate) ? error_code()
                                    : make_error_code(port.error());
}

error_code SerialPort::sendBreak() {
  auto result =
      port.setBreakEnabled(true) ? error_code() : make_error_code(port.error());
  if (!result) {
    sleep(1);
    result = port.setBreakEnabled(false) ? error_code()
                                         : make_error_code(port.error());
  }
  return result;
}

error_code SerialPort::clearReadBuffer() {
  port.readAll();
  return error_code();
}

error_code SerialPort::writeByte(uint_least8_t data) {
  auto result =
      port.putChar(data) ? error_code() : make_error_code(port.error());
  if (!result) {
    result = port.waitForBytesWritten(1000) ? error_code()
                                            : make_error_code(port.error());
  }
  return result;
}

error_code SerialPort::readByte(uint_least8_t & data) {
  if (port.atEnd()) {
    const auto result = port.waitForReadyRead(1000)
                            ? error_code()
                            : make_error_code(port.error());
    if (result) {
      return result;
    }
  }
  return port.getChar(reinterpret_cast<char *>(&data))
             ? error_code()
             : make_error_code(port.error());
}

const error_category & SerialPort::errorCategory() {
  static class : public error_category {
  public:
    virtual const char * name() const override { return "Qt SerialPort"; }

    virtual string message(int condition) const override {
      switch (condition) {
      case QSerialPort::DeviceNotFoundError:
        return "Device Not Found";

      case QSerialPort::PermissionError:
        return "Permission Error";

      case QSerialPort::OpenError:
        return "Open Error";

      case QSerialPort::NotOpenError:
        return "Not Open Error";

      case QSerialPort::WriteError:
        return "Write Error";

      case QSerialPort::ReadError:
        return "Read Error";

      case QSerialPort::ResourceError:
        return "Resource Error";

      case QSerialPort::UnsupportedOperationError:
        return "Unsupported Operation Error";

      case QSerialPort::TimeoutError:
        return "Timeout Error";

      default:
        return defaultErrorMessage(condition);
      }
    }

    // Make QSerialPort::TimeoutError equivalent to Uart::TimeoutError.
    virtual bool equivalent(int code,
                            const error_condition & condition) const override {
      return (code == QSerialPort::TimeoutError)
                 ? (condition == make_error_condition(Uart::TimeoutError))
                 : error_category::equivalent(code, condition);
    }
  } instance;
  return instance;
}

} // namespace Qt
} // namespace MaximInterface