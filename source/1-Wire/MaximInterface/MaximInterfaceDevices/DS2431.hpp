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

#ifndef MaximInterfaceDevices_DS2431_hpp
#define MaximInterfaceDevices_DS2431_hpp

#include <utility>
#include <MaximInterfaceCore/array_span.hpp>
#include <MaximInterfaceCore/SelectRom.hpp>
#include <MaximInterfaceCore/Sleep.hpp>
#include "Config.hpp"

namespace MaximInterfaceDevices {

/// @brief DS2431 1024-bit 1-Wire EEPROM
/// @details The DS2431 is a 1024-bit, 1-Wire® EEPROM chip organized
/// as four memory pages of 256 bits each. Data is written to an 8-byte
/// scratchpad, verified, and then copied to the EEPROM memory. As a
/// special feature, the four memory pages can individually be write
/// protected or put in EPROM-emulation mode, where bits can only be
/// changed from a 1 to a 0 state. The DS2431 communicates over the
/// single-conductor 1-Wire bus. The communication follows the standard
/// 1-Wire protocol. Each device has its own unalterable and unique
/// 64-bit ROM registration number that is factory lasered into the chip.
/// The registration number is used to address the device in a multidrop,
/// 1-Wire net environment.
class DS2431 {
public:
  enum ErrorValue { CrcError = 1, OperationFailure };

  typedef Core::array_span<uint_least8_t, 8> Scratchpad;

  DS2431(Core::Sleep & sleep, Core::OneWireMaster & master,
         const Core::SelectRom & selectRom)
      : selectRom(selectRom), master(&master), sleep(&sleep) {}

  void setSleep(Core::Sleep & sleep) { this->sleep = &sleep; }

  void setMaster(Core::OneWireMaster & master) { this->master = &master; }

  void setSelectRom(const Core::SelectRom & selectRom) {
    this->selectRom = selectRom;
  }

  /// @brief Reads block of data from EEPROM memory.
  /// @param[in] beginAddress EEPROM memory address to start reading from.
  /// @param[out] data EEPROM data read from the device.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  readMemory(uint_least8_t beginAddress, Core::span<uint_least8_t> data) const;

  /// @brief Writes 8 bytes to the scratchpad.
  /// @param[in] targetAddress
  /// EEPROM memory address that this data will be copied to.
  /// Must be on row boundary.
  /// @param[in] data Data to write to scratchpad.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeScratchpad(uint_least8_t targetAddress, Scratchpad::const_span data);

  /// @brief Reads contents of scratchpad.
  /// @returns E/S byte and scratchpad data.
  MaximInterfaceDevices_EXPORT
      Core::Result<std::pair<uint_least8_t, Scratchpad::array> >
      readScratchpad() const;

  /// @brief Copies contents of scratchpad to EEPROM.
  /// @param[in] targetAddress EEPROM memory address that scratchpad
  /// will be copied to. Must be on row boundary.
  /// @param[in] esByte E/S byte from preceding Read Scratchpad command.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  copyScratchpad(uint_least8_t targetAddress, uint_least8_t esByte);

  MaximInterfaceDevices_EXPORT static const Core::error_category &
  errorCategory();

private:
  Core::SelectRom selectRom;
  Core::OneWireMaster * master;
  const Core::Sleep * sleep;
};

/// @brief
/// Writes data to EEPROM using Write Scratchpad, Read Scratchpad,
/// and Copy Scratchpad commands.
/// @param device Device to write.
/// @param[in] targetAddress EEPROM memory address to start writing at.
/// @param[in] data Data to write to EEPROM.
MaximInterfaceDevices_EXPORT Core::Result<void>
writeMemory(DS2431 & device, uint_least8_t targetAddress,
            DS2431::Scratchpad::const_span data);

} // namespace MaximInterfaceDevices
namespace MaximInterfaceCore {

template <>
struct is_error_code_enum<MaximInterfaceDevices::DS2431::ErrorValue>
    : true_type {};

} // namespace MaximInterfaceCore
namespace MaximInterfaceDevices {

inline Core::error_code make_error_code(DS2431::ErrorValue e) {
  return Core::error_code(e, DS2431::errorCategory());
}

} // namespace MaximInterfaceDevices

#endif
