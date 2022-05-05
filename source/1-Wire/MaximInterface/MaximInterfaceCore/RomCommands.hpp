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

/// @file
/// @brief ROM Commands for enumerating and selecting 1-Wire devices.

#ifndef MaximInterfaceCore_RomCommands_hpp
#define MaximInterfaceCore_RomCommands_hpp

#include <stdint.h>
#include "Algorithm.hpp"
#include "Config.hpp"
#include "OneWireMaster.hpp"
#include "RomId.hpp"

namespace MaximInterfaceCore {

/// State used by Search ROM command.
struct SearchRomState {
  RomId::array romId;
  int_least8_t lastDiscrepancy;
  int_least8_t lastFamilyDiscrepancy;
  bool lastDevice;

  SearchRomState()
      : romId(), lastDiscrepancy(0), lastFamilyDiscrepancy(0),
        lastDevice(false) {}

  explicit SearchRomState(RomId::const_span romId)
      : lastDiscrepancy(64), lastFamilyDiscrepancy(0), lastDevice(false) {
    copy(romId, make_span(this->romId));
  }

  explicit SearchRomState(RomId::element familyCode)
      : romId(), lastDiscrepancy(64), lastFamilyDiscrepancy(0),
        lastDevice(false) {
    romId.front() = familyCode;
  }
};

/// @brief
/// Set the search state to skip the current device family on the next
/// Search ROM command.
MaximInterfaceCore_EXPORT void skipCurrentFamily(SearchRomState & searchState);

/// Verify that the device with the specified ROM ID is present.
MaximInterfaceCore_EXPORT Result<void> verifyRom(OneWireMaster & master,
                                                 RomId::const_span romId);

/// @brief Use Read ROM command to read ROM ID from device on bus.
/// @note
/// Only use this command with a single-drop bus.
/// Data collisions will occur if there is more than one device on the bus.
/// @param master 1-Wire master for operation.
/// @returns ROM ID read from device.
MaximInterfaceCore_EXPORT Result<RomId::array> readRom(OneWireMaster & master);

/// @brief Issue Skip ROM command on bus.
/// @note
/// Only use this command with a single-drop bus.
/// Data collisions will occur if there is more than one device on the bus.
MaximInterfaceCore_EXPORT Result<void> skipRom(OneWireMaster & master);

/// @brief Use the Match ROM command to select the device by its known ID.
/// @note
/// This command causes all devices supporting Overdrive mode to switch to
/// Overdrive timing.
/// @param master 1-Wire master for operation.
/// @param[in] romId ROM ID of device to select.
MaximInterfaceCore_EXPORT Result<void> matchRom(OneWireMaster & master,
                                                RomId::const_span romId);

/// @brief Issue Overdrive Skip ROM command on bus.
/// @note
/// This command causes all devices supporting Overdrive mode to switch to
/// Overdrive timing.
/// @note
/// Only use this command with a single-drop bus.
/// Data collisions will occur if there is more than one device on the bus.
MaximInterfaceCore_EXPORT Result<void> overdriveSkipRom(OneWireMaster & master);

/// @brief
/// Use the Overdrive Match ROM command to select the device by its known ID.
/// @param master 1-Wire master for operation.
/// @param[in] romId ROM ID of device to select.
MaximInterfaceCore_EXPORT Result<void>
overdriveMatchRom(OneWireMaster & master, RomId::const_span romId);

/// @brief Perform a Resume ROM command on bus.
/// @details
/// Resumes communication with the last device selected through a Match ROM or
/// Search ROM operation.
MaximInterfaceCore_EXPORT Result<void> resumeRom(OneWireMaster & master);

/// @brief Find device on the 1-Wire bus.
/// @details
/// This command uses the Search ROM command to enumerate all 1-Wire devices in
/// sequence. Begin with a new search state and continue using the same search
/// state until the last device flag is set which indicates that all devices
/// have been discovered.
MaximInterfaceCore_EXPORT Result<void> searchRom(OneWireMaster & master,
                                                 SearchRomState & searchState);

} // namespace MaximInterfaceCore

#endif
