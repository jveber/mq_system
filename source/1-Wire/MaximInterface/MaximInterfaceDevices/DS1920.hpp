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

#ifndef MaximInterfaceDevices_DS1920_hpp
#define MaximInterfaceDevices_DS1920_hpp

#include <MaximInterfaceCore/array.hpp>
#include <MaximInterfaceCore/SelectRom.hpp>
#include <MaximInterfaceCore/Sleep.hpp>
#include "Config.hpp"

namespace MaximInterfaceDevices {

/// @brief DS1920 1-Wire Temperature iButton
/// @details The iButton® temperature logger (DS1920) provides
/// direct-to-digital 9-bit temperature readings over a range of
/// -55°C to +100°C in 0.5° increments. The iButton communicates with
/// a processor using the 1-Wire® protocol through a hardware port
/// interface. The port interface provides both the physical link and
/// handles the communication protocols that enable the processor to
/// access iButton resources with simple commands. Two bytes of
/// EEPROM can be used either to set alarm triggers or for storing
/// user data.
class DS1920 {
public:
  enum ErrorValue { CrcError = 1, DataError };

  /// Holds the contents of the device scratchpad.
  typedef Core::array<uint_least8_t, 8> Scratchpad;

  DS1920(Core::Sleep & sleep, Core::OneWireMaster & master,
         const Core::SelectRom & selectRom)
      : selectRom(selectRom), master(&master), sleep(&sleep) {}

  void setSleep(Core::Sleep & sleep) { this->sleep = &sleep; }

  void setMaster(Core::OneWireMaster & master) { this->master = &master; }

  void setSelectRom(const Core::SelectRom & selectRom) {
    this->selectRom = selectRom;
  }

  /// @brief Write Scratchpad Command
  /// @details If the result of a temperature measurement is higher
  /// than TH or lower than TL, an alarm flag inside the device is
  /// set. This flag is updated with every temperature measurement.
  /// As long as the alarm flag is set, the DS1920 will respond to
  /// the alarm search command.
  /// @param[in] th 8-bit upper temperature threshold, MSB indicates sign.
  /// @param[in] tl 8-bit lower temperature threshold, MSB indicates sign.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeScratchpad(uint_least8_t th, uint_least8_t tl);

  /// @brief Read Scratchpad Command
  /// @returns Contents of scratchpad.
  MaximInterfaceDevices_EXPORT Core::Result<Scratchpad> readScratchpad() const;

  /// @brief Copy Scratchpad Command
  /// @details This command copies from the scratchpad into the
  /// EEPROM of the DS1920, storing the temperature trigger bytes
  /// in nonvolatile memory.
  MaximInterfaceDevices_EXPORT Core::Result<void> copyScratchpad();

  /// @brief Convert Temperature Command
  /// @details This command begins a temperature conversion.
  MaximInterfaceDevices_EXPORT Core::Result<void> convertTemperature();

  /// @brief Recall Command
  /// @details This command recalls the temperature trigger values
  /// stored in EEPROM to the scratchpad.
  MaximInterfaceDevices_EXPORT Core::Result<void> recallEeprom();

  MaximInterfaceDevices_EXPORT static const Core::error_category &
  errorCategory();

private:
  Core::SelectRom selectRom;
  Core::OneWireMaster * master;
  const Core::Sleep * sleep;
};

/// @brief Reads the current temperature as an integer value.
/// @param ds1920 Device to read.
/// @returns Temperature in degrees Celsius multiplied by 2.
MaximInterfaceDevices_EXPORT Core::Result<int> readTemperature(DS1920 & ds1920);

} // namespace MaximInterfaceDevices
namespace MaximInterfaceCore {

template <>
struct is_error_code_enum<MaximInterfaceDevices::DS1920::ErrorValue>
    : true_type {};

} // namespace MaximInterfaceCore
namespace MaximInterfaceDevices {

inline Core::error_code make_error_code(DS1920::ErrorValue e) {
  return Core::error_code(e, DS1920::errorCategory());
}

} // namespace MaximInterfaceDevices

#endif
