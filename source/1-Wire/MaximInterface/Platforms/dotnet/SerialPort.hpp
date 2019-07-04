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

#pragma once

#include <memory>
#include <string>
#include <MaximInterface/Links/SerialPort.hpp>
#include <MaximInterface/Utilities/Export.h>
#include "MoveOnly.hpp"

namespace MaximInterface {
namespace dotnet {

/// Serial port interface using .NET type System::IO::Ports::SerialPort.
class SerialPort : public MaximInterface::SerialPort, private MoveOnly {
public:
  enum ErrorValue {
    NotConnectedError = 1,
    ArgumentError,
    InvalidOperationError,
    IOError,
    UnauthorizedAccessError
  };

  MaximInterface_EXPORT SerialPort();
  MaximInterface_EXPORT ~SerialPort();

  MaximInterface_EXPORT SerialPort(SerialPort &&) noexcept;
  MaximInterface_EXPORT SerialPort & operator=(SerialPort &&) noexcept;

  MaximInterface_EXPORT virtual error_code
  connect(const std::string & portName) override;
  MaximInterface_EXPORT virtual error_code disconnect() override;
  MaximInterface_EXPORT virtual bool connected() const override;
  MaximInterface_EXPORT virtual std::string portName() const override;

  MaximInterface_EXPORT virtual error_code
  setBaudRate(int_least32_t baudRate) override;
  MaximInterface_EXPORT virtual error_code sendBreak() override;
  MaximInterface_EXPORT virtual error_code clearReadBuffer() override;
  MaximInterface_EXPORT virtual error_code
  writeByte(uint_least8_t data) override;
  MaximInterface_EXPORT virtual error_code
  writeBlock(const uint_least8_t * data, size_t dataLen) override;
  MaximInterface_EXPORT virtual error_code
  readByte(uint_least8_t & data) override;
  MaximInterface_EXPORT virtual error_code readBlock(uint_least8_t * data,
                                                     size_t dataLen) override;

  MaximInterface_EXPORT static const error_category & errorCategory();

private:
  struct Data;
  std::unique_ptr<Data> data;
};

inline error_code make_error_code(SerialPort::ErrorValue e) {
  return error_code(e, SerialPort::errorCategory());
}

} // namespace dotnet
} // namespace MaximInterface