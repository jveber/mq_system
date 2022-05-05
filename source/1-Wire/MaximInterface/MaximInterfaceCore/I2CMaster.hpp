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

#ifndef MaximInterfaceCore_I2CMaster_hpp
#define MaximInterfaceCore_I2CMaster_hpp

#include <stdint.h>
#include "Config.hpp"
#include "Result.hpp"
#include "span.hpp"
#include "system_error.hpp"

namespace MaximInterfaceCore {

/// I2C master interface.
class I2CMaster {
public:
  enum ErrorValue {
    NackError = 1 ///< Transaction stopped due to a NACK from the slave device.
  };

  enum DoAck { Ack, Nack };

  enum DoStop { Stop, StopOnError, NoStop };

  virtual ~I2CMaster() {}

  /// @brief Send start condition and address on the bus.
  /// @param address Address with R/W bit.
  virtual Result<void> start(uint_least8_t address) = 0;

  /// Send stop condition on the bus.
  virtual Result<void> stop() = 0;

  /// Write data byte to the bus.
  virtual Result<void> writeByte(uint_least8_t data) = 0;

  /// Write data block to the bus.
  MaximInterfaceCore_EXPORT virtual Result<void>
  writeBlock(span<const uint_least8_t> data);

  /// @brief Perform a complete write transaction on the bus.
  /// @param address Address in 8-bit format.
  /// @param data Data to write to the bus.
  /// @param doStop
  /// Determines whether to do a stop condition or set up a repeated start.
  MaximInterfaceCore_EXPORT virtual Result<void>
  writePacket(uint_least8_t address, span<const uint_least8_t> data,
              DoStop doStop);

  /// @brief Read data byte from the bus.
  /// @param doAck Determines whether an ACK or NACK is done after reading.
  /// @returns data Data read from the bus if successful.
  virtual Result<uint_least8_t> readByte(DoAck doAck) = 0;

  /// @brief Read data block from the bus.
  /// @param[out] data Data read from the bus if successful.
  /// @param doAck Determines whether an ACK or NACK is done after reading.
  MaximInterfaceCore_EXPORT virtual Result<void>
  readBlock(span<uint_least8_t> data, DoAck doAck);

  /// @brief Perform a complete read transaction on the bus.
  /// @param address Address in 8-bit format.
  /// @param[out] data Data read from the bus if successful.
  /// @param doStop
  /// Determines whether to do a stop condition or set up a repeated start.
  MaximInterfaceCore_EXPORT virtual Result<void>
  readPacket(uint_least8_t address, span<uint_least8_t> data, DoStop doStop);

  MaximInterfaceCore_EXPORT static const error_category & errorCategory();
};

template <> struct is_error_code_enum<I2CMaster::ErrorValue> : true_type {};

inline error_code make_error_code(I2CMaster::ErrorValue e) {
  return error_code(e, I2CMaster::errorCategory());
}

inline error_condition make_error_condition(I2CMaster::ErrorValue e) {
  return error_condition(e, I2CMaster::errorCategory());
}

} // namespace MaximInterfaceCore

#endif
