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

#ifndef MaximInterfaceDevices_DS18B20_hpp
#define MaximInterfaceDevices_DS18B20_hpp

#include <MaximInterfaceCore/array.hpp>
#include <MaximInterfaceCore/SelectRom.hpp>
#include <MaximInterfaceCore/Sleep.hpp>
#include "Config.hpp"

namespace MaximInterfaceDevices {

/// @brief DS18B20 Programmable Resolution 1-Wire Digital Thermometer
/// @details The DS18B20 digital thermometer provides 9-bit to 12-bit
/// Celsius temperature measurements and has an alarm function with
/// nonvolatile user-programmable upper and lower trigger points. The
/// DS18B20 communicates over a 1-Wire bus that by definition requires
/// only one data line (and ground) for communication with a central
/// microprocessor. In addition, the DS18B20 can derive power directly
/// from the data line ("parasite power"), eliminating the need for an
/// external power supply.
class DS18B20 {
public:
  enum ErrorValue { CrcError = 1, DataError };

  static const uint_least8_t nineBitResolution = 0x1F;
  static const uint_least8_t tenBitResolution = 0x3F;
  static const uint_least8_t elevenBitResolution = 0x5F;
  static const uint_least8_t twelveBitResolution = 0x7F;

  /// Holds the contents of the device scratchpad.
  typedef Core::array<uint_least8_t, 8> Scratchpad;

  DS18B20(Core::Sleep & sleep, Core::OneWireMaster & master,
          const Core::SelectRom & selectRom)
      : selectRom(selectRom), master(&master), sleep(&sleep), resolution(0) {}

  void setSleep(Core::Sleep & sleep) { this->sleep = &sleep; }

  void setMaster(Core::OneWireMaster & master) { this->master = &master; }

  void setSelectRom(const Core::SelectRom & selectRom) {
    this->selectRom = selectRom;
  }

  /// Initializes the device for first time use.
  MaximInterfaceDevices_EXPORT Core::Result<void> initialize() const;

  /// @brief Write Scratchpad Command
  /// @details If the result of a temperature measurement is higher
  /// than TH or lower than TL, an alarm flag inside the device is
  /// set. This flag is updated with every temperature measurement.
  /// As long as the alarm flag is set, the DS1920 will respond to
  /// the alarm search command.
  /// @param[in] th 8-bit upper temperature threshold, MSB indicates sign.
  /// @param[in] tl 8-bit lower temperature threshold, LSB indicates sign.
  /// @param[in] res Resolution of the DS18B20.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeScratchpad(uint_least8_t th, uint_least8_t tl, uint_least8_t res);

  /// @brief Read Scratchpad Command
  /// @returns scratchpad Contents of scratchpad.
  MaximInterfaceDevices_EXPORT Core::Result<Scratchpad> readScratchpad() const;

  /// @brief Copy Scratchpad Command
  /// @details This command copies from the scratchpad into the
  /// EEPROM of the DS18B20, storing the temperature trigger bytes
  /// and resolution in nonvolatile memory.
  MaximInterfaceDevices_EXPORT Core::Result<void> copyScratchpad();

  /// @brief Read Power Supply command
  /// @details This command determines if the DS18B20 is parasite
  /// powered or has a local supply
  /// @returns
  /// True if the device is powered by a local power supply, or false if the
  /// device is parasitically powered.
  MaximInterfaceDevices_EXPORT Core::Result<bool> readPowerSupply() const;

  /// @brief Convert Temperature Command
  /// @details This command begins a temperature conversion.
  MaximInterfaceDevices_EXPORT Core::Result<void> convertTemperature();

  /// @brief Recall Command
  /// @details This command recalls the temperature trigger values
  /// and resolution stored in EEPROM to the scratchpad.
  MaximInterfaceDevices_EXPORT Core::Result<void> recallEeprom();

  MaximInterfaceDevices_EXPORT static const Core::error_category &
  errorCategory();

private:
  Core::SelectRom selectRom;
  Core::OneWireMaster * master;
  const Core::Sleep * sleep;
  mutable uint_least8_t resolution;
};

/// @brief Reads the current temperature as an integer value with decimal.
/// @param ds18b20 Device to read.
/// @returns Temperature in degrees Celsius multiplied by 16.
MaximInterfaceDevices_EXPORT Core::Result<int>
readTemperature(DS18B20 & ds18b20);

} // namespace MaximInterfaceDevices
namespace MaximInterfaceCore {

template <>
struct is_error_code_enum<MaximInterfaceDevices::DS18B20::ErrorValue>
    : true_type {};

} // namespace MaximInterfaceCore
namespace MaximInterfaceDevices {

inline Core::error_code make_error_code(DS18B20::ErrorValue e) {
  return Core::error_code(e, DS18B20::errorCategory());
}

} // namespace MaximInterfaceDevices

#endif
