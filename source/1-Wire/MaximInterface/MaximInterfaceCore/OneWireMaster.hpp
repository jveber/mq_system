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

#ifndef MaximInterfaceCore_OneWireMaster_hpp
#define MaximInterfaceCore_OneWireMaster_hpp

#include <stdint.h>
#include "Config.hpp"
#include "Result.hpp"
#include "span.hpp"
#include "system_error.hpp"

namespace MaximInterfaceCore {

/// 1-Wire master interface.
class OneWireMaster {
public:
  /// Speed of the 1-Wire bus.
  enum Speed { StandardSpeed = 0x00, OverdriveSpeed = 0x01 };

  /// Level of the 1-Wire bus.
  enum Level { NormalLevel = 0x00, StrongLevel = 0x02 };

  /// Result of all 1-Wire commands.
  enum ErrorValue {
    NoSlaveError = 1, ///< Slave not detected, typically due to no presence pulse.
    ShortDetectedError,
    InvalidSpeedError,
    InvalidLevelError
  };

  /// Result of the 1-Wire triplet command.
  struct TripletData {
    bool readBit;
    bool readBitComplement;
    bool writeBit;
  };

  virtual ~OneWireMaster() {}

  /// @brief
  /// Reset all of the devices on the 1-Wire bus and check for a presence pulse.
  /// @returns
  /// NoSlaveError if reset was performed but no presence pulse was detected.
  virtual Result<void> reset() = 0;

  /// @brief
  /// Send and receive one bit of communication and set a new level on the
  /// 1-Wire bus.
  /// @param sendBit Bit to send on the 1-Wire bus.
  /// @param afterLevel Level to set the 1-Wire bus to after communication.
  /// @returns Bit received from the 1-Wire bus.
  virtual Result<bool> touchBitSetLevel(bool sendBit, Level afterLevel) = 0;

  /// @brief
  /// Send one byte of communication and set a new level on the 1-Wire bus.
  /// @param sendByte Byte to send on the 1-Wire bus.
  /// @param afterLevel Level to set the 1-Wire bus to after communication.
  MaximInterfaceCore_EXPORT virtual Result<void>
  writeByteSetLevel(uint_least8_t sendByte, Level afterLevel);

  /// @brief
  /// Receive one byte of communication and set a new level on the 1-Wire bus.
  /// @param afterLevel Level to set the 1-Wire bus to after communication.
  /// @returns Data received from the 1-Wire bus.
  MaximInterfaceCore_EXPORT virtual Result<uint_least8_t>
  readByteSetLevel(Level afterLevel);

  /// @brief Send a block of communication on the 1-Wire bus.
  /// @param[in] sendBuf Buffer to send on the 1-Wire bus.
  MaximInterfaceCore_EXPORT virtual Result<void>
  writeBlock(span<const uint_least8_t> sendBuf);

  /// @brief Receive a block of communication on the 1-Wire bus.
  /// @param[out] recvBuf Buffer to receive the data from the 1-Wire bus.
  MaximInterfaceCore_EXPORT virtual Result<void>
  readBlock(span<uint_least8_t> recvBuf);

  /// Set the 1-Wire bus communication speed.
  virtual Result<void> setSpeed(Speed newSpeed) = 0;

  /// Set the 1-Wire bus level.
  virtual Result<void> setLevel(Level newLevel) = 0;

  /// @brief 1-Wire Triplet operation.
  /// @details Perform one bit of a 1-Wire search. This command
  /// does two read bits and one write bit. The write bit is either
  /// the default direction (all devices have same bit) or in case
  /// of a discrepancy, the data.writeBit parameter is used.
  /// @param sendBit Bit to send in case both read bits are zero.
  MaximInterfaceCore_EXPORT virtual Result<TripletData> triplet(bool sendBit);

  /// @brief
  /// Send one bit of communication and set a new level on the 1-Wire bus.
  /// @param sendBit Bit to send on the 1-Wire bus.
  /// @param afterLevel Level to set the 1-Wire bus to after communication.
  Result<void> writeBitSetLevel(bool sendBit, Level afterLevel) {
    MaximInterfaceCore_TRY(touchBitSetLevel(sendBit, afterLevel));
    return none;
  }

  /// @brief
  /// Receive one bit of communication and set a new level on the 1-Wire bus.
  /// @param afterLevel Level to set the 1-Wire bus to after communication.
  /// @returns Received data from the 1-Wire bus.
  Result<bool> readBitSetLevel(Level afterLevel) {
    return touchBitSetLevel(1, afterLevel);
  }

  // Alternate forms of the read and write functions.

  Result<bool> touchBit(bool sendBit) {
    return touchBitSetLevel(sendBit, NormalLevel);
  }

  Result<void> writeBit(bool sendBit) {
    return writeBitSetLevel(sendBit, NormalLevel);
  }

  Result<bool> readBit() { return readBitSetLevel(NormalLevel); }

  Result<void> writeBitPower(bool sendBit) {
    return writeBitSetLevel(sendBit, StrongLevel);
  }

  Result<bool> readBitPower() { return readBitSetLevel(StrongLevel); }

  Result<void> writeByte(uint_least8_t sendByte) {
    return writeByteSetLevel(sendByte, NormalLevel);
  }

  Result<uint_least8_t> readByte() { return readByteSetLevel(NormalLevel); }

  Result<void> writeBytePower(uint_least8_t sendByte) {
    return writeByteSetLevel(sendByte, StrongLevel);
  }

  Result<uint_least8_t> readBytePower() {
    return readByteSetLevel(StrongLevel);
  }

  MaximInterfaceCore_EXPORT static const error_category & errorCategory();
};

template <> struct is_error_code_enum<OneWireMaster::ErrorValue> : true_type {};

inline error_code make_error_code(OneWireMaster::ErrorValue e) {
  return error_code(e, OneWireMaster::errorCategory());
}

inline error_condition make_error_condition(OneWireMaster::ErrorValue e) {
  return error_condition(e, OneWireMaster::errorCategory());
}

} // namespace MaximInterfaceCore

#endif
