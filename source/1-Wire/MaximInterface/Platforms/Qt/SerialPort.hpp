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

#include <QSerialPort>
#include <MaximInterface/Links/SerialPort.hpp>
#include <MaximInterface/Utilities/Export.h>

namespace MaximInterface {
namespace Qt {

class SerialPort : public MaximInterface::SerialPort {
public:
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
  readByte(uint_least8_t & data) override;
                                                     
  MaximInterface_EXPORT static const error_category & errorCategory();

private:
  QSerialPort port;
};

inline error_code make_error_code(QSerialPort::SerialPortError e) {
  return error_code(e, SerialPort::errorCategory());
}

} // namespace Qt
} // namespace MaximInterface