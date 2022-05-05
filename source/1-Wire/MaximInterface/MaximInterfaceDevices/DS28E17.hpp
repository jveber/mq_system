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

#ifndef MaximInterfaceDevices_DS28E17_hpp
#define MaximInterfaceDevices_DS28E17_hpp

#include <stdint.h>
#include <MaximInterfaceCore/SelectRom.hpp>
#include <MaximInterfaceCore/span.hpp>
#include "Config.hpp"

namespace MaximInterfaceDevices {

/// @brief DS28E17 1-Wire®-to-I2C Master Bridge
/// @details The DS28E17 is a 1-Wire slave to I2C master bridge
/// device that interfaces directly to I2C slaves at standard
/// (100kHz max) or fast (400kHz max). Data transfers serially by
/// means of the 1-Wire® protocol, which requires only a single data
/// lead and a ground return. Every DS28E17 is guaranteed to have a
/// unique 64-bit ROM registration number that serves as a node
/// address in the 1-Wire network. Multiple DS28E17 devices can
/// coexist with other devices in the 1-Wire network and be accessed
/// individually without affecting other devices. The DS28E17 allows
/// using complex I2C devices such as display controllers, ADCs, DACs,
/// I2C sensors, etc. in a 1-Wire environment. Each self-timed DS28E17
/// provides 1-Wire access for a single I2C interface.
class DS28E17 {
public:
  // Values from 1-255 represent the Write Status byte.
  enum ErrorValue {
    TimeoutError = 256,
    OutOfRangeError,
    InvalidCrc16Error,
    AddressNackError,
    InvalidStartError
  };

  enum I2CSpeed { Speed100kHz, Speed400kHz, Speed900kHz };

  DS28E17(Core::OneWireMaster & master, const Core::SelectRom & selectRom)
      : selectRom(selectRom), master(&master) {}

  void setMaster(Core::OneWireMaster & master) { this->master = &master; }

  void setSelectRom(const Core::SelectRom & selectRom) {
    this->selectRom = selectRom;
  }

  /// @brief Write Data With Stop command.
  /// @details Output on I2C: S, Address + Write, Write Data [1-255], P
  /// @param[in] I2C_addr
  /// I2C slave address. The least significant bit of the I2C
  /// address is automatically cleared by the command.
  /// @param[in] data I2C data to write with length 1-255.
  /// @returns
  /// Values from 1-255 in the DS28E17 category represent the Write Status
  /// indicating which write byte did not acknowledge.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeDataWithStop(uint_least8_t I2C_addr,
                    Core::span<const uint_least8_t> data);

  /// @brief Write Data No Stop command.
  /// @details Output on I2C: S, Address + Write, Write Data [1-255]
  /// @param[in] I2C_addr
  /// I2C slave address. The least significant bit of the I2C address
  /// is automatically cleared by the command.
  /// @param[in] data I2C data to write with length 1-255.
  /// @returns
  /// Values from 1-255 in the DS28E17 category represent the Write Status
  /// indicating which write byte did not acknowledge.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeDataNoStop(uint_least8_t I2C_addr, Core::span<const uint_least8_t> data);

  /// @brief Write Data Only command.
  /// @details Output on I2C: Write Data [1-255]
  /// @param[in] data I2C data to write with length 1-255.
  /// @returns
  /// Values from 1-255 in the DS28E17 category represent the Write Status
  /// indicating which write byte did not acknowledge.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeDataOnly(Core::span<const uint_least8_t> data);

  /// @brief Write Data Only With Stop command.
  /// @details Output on I2C: Write Data [1-255], P
  /// @param[in] data I2C data to write with length 1-255.
  /// @returns
  /// Values from 1-255 in the DS28E17 category represent the Write Status
  /// indicating which write byte did not acknowledge.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeDataOnlyWithStop(Core::span<const uint_least8_t> data);

  /// @brief Write, Read Data With Stop command.
  /// @details Output on I2C:
  /// S, Slave Address + Write, Write Data [1-255],
  /// Sr, Address + Read, Read Data [1-255], P (NACK last read byte)
  /// @param[in] I2C_addr
  /// I2C slave address. The least significant bit of the I2C address
  /// is automatically cleared and set by the command.
  /// @param[in] write_data I2C data to write with length 1-255.
  /// @param[out] read_data I2C data that was read with length 1-255.
  /// @returns
  /// Values from 1-255 in the DS28E17 category represent the Write Status
  /// indicating which write byte did not acknowledge.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeReadDataWithStop(uint_least8_t I2C_addr,
                        Core::span<const uint_least8_t> write_data,
                        Core::span<uint_least8_t> read_data);

  /// @brief Read Data With Stop command.
  /// @details Output on I2C:
  /// S, Slave Address + Read, Read Data [1-255], P (NACK last read byte)
  /// @param[in]  I2C_addr
  /// I2C slave address. The least significant bit of the I2C address
  /// is automatically set by the command.
  /// @param[out] data I2C data that was read with length 1-255.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  readDataWithStop(uint_least8_t I2C_addr, Core::span<uint_least8_t> data);

  /// Write to Configuration Register of DS28E17.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeConfigReg(I2CSpeed speed);

  /// @brief Read the Configuration Register of DS28E17.
  /// @returns speed Speed read from configuration register.
  MaximInterfaceDevices_EXPORT Core::Result<I2CSpeed> readConfigReg() const;

  /// @brief Put the device into a low current mode.
  /// @details All 1-Wire communication is ignored until woken up. Immediately
  /// after the command, the device monitors the WAKEUP input pin and exits
  /// sleep mode on a rising edge.
  MaximInterfaceDevices_EXPORT Core::Result<void> enableSleepMode();

  /// @brief Read the Device Revision of DS28E17.
  /// @details The upper nibble is the major revision,
  /// and the lower nibble is the minor revision.
  /// @returns Device Revision.
  MaximInterfaceDevices_EXPORT Core::Result<uint_least8_t>
  readDeviceRevision() const;

  MaximInterfaceDevices_EXPORT static const Core::error_category &
  errorCategory();

private:
  enum Command {
    WriteDataWithStopCmd = 0x4B,
    WriteDataNoStopCmd = 0x5A,
    WriteDataOnlyCmd = 0x69,
    WriteDataOnlyWithStopCmd = 0x78,
    ReadDataWithStopCmd = 0x87,
    WriteReadDataWithStopCmd = 0x2D,
    WriteConfigurationCmd = 0xD2,
    ReadConfigurationCmd = 0xE1,
    EnableSleepModeCmd = 0x1E,
    ReadDeviceRevisionCmd = 0xC3
  };

  Core::Result<void> sendPacket(Command command, const uint_least8_t * I2C_addr,
                                Core::span<const uint_least8_t> write_data,
                                Core::span<uint_least8_t> read_data);

  Core::SelectRom selectRom;
  Core::OneWireMaster * master;
};

} // namespace MaximInterfaceDevices
namespace MaximInterfaceCore {

template <>
struct is_error_code_enum<MaximInterfaceDevices::DS28E17::ErrorValue>
    : true_type {};

} // namespace MaximInterfaceCore
namespace MaximInterfaceDevices {

inline Core::error_code make_error_code(DS28E17::ErrorValue e) {
  return Core::error_code(e, DS28E17::errorCategory());
}

} // namespace MaximInterfaceDevices

#endif
