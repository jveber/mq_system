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

#ifndef MaximInterfaceCore_Uart_hpp
#define MaximInterfaceCore_Uart_hpp

#include <stdint.h>
#include "Config.hpp"
#include "Result.hpp"
#include "span.hpp"
#include "system_error.hpp"

namespace MaximInterfaceCore {

/// Universal asynchronous receiver-transmitter interface.
class Uart {
public:
  enum ErrorValue {
    TimeoutError = 1, ///< Read operation aborted due to timeout.
    OverrunError      ///< Received data lost due to read buffer overrun.
  };

  virtual ~Uart() {}

  /// Set the baud rate of the port in Hz.
  virtual Result<void> setBaudRate(int_least32_t baudRate) = 0;

  /// Generate a break condition on the port for a small amount of time.
  virtual Result<void> sendBreak() = 0;

  /// Clear all received data that was buffered.
  virtual Result<void> clearReadBuffer() = 0;

  /// Writes a byte of data to the port.
  virtual Result<void> writeByte(uint_least8_t data) = 0;

  /// Writes a block of data to the port.
  MaximInterfaceCore_EXPORT virtual Result<void>
  writeBlock(span<const uint_least8_t> data);

  /// @brief
  /// Reads a byte of data from the port. Block until data is received or a
  /// timeout is reached.
  /// @returns Data read from the port if successful.
  virtual Result<uint_least8_t> readByte() = 0;

  /// @brief
  /// Read a block of data from the port. Block until data is received or a
  /// timeout is reached.
  /// @param[out] data Data read from the port if successful.
  MaximInterfaceCore_EXPORT virtual Result<void>
  readBlock(span<uint_least8_t> data);

  MaximInterfaceCore_EXPORT static const error_category & errorCategory();
};

template <> struct is_error_code_enum<Uart::ErrorValue> : true_type {};

inline error_code make_error_code(Uart::ErrorValue e) {
  return error_code(e, Uart::errorCategory());
}

inline error_condition make_error_condition(Uart::ErrorValue e) {
  return error_condition(e, Uart::errorCategory());
}

} // namespace MaximInterfaceCore

#endif
