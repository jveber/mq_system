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

/// @file ROM Commands for enumerating and selecting 1-Wire devices.

#ifndef MaximInterface_RomCommands
#define MaximInterface_RomCommands

#include <stdint.h>
#include <MaximInterface/Utilities/Export.h>
#include <MaximInterface/Utilities/RomId.hpp>
#include "OneWireMaster.hpp"

namespace MaximInterface {

/// State used by Search ROM command.
struct SearchRomState {
  RomId romId;
  int_least8_t lastDiscrepancy;
  int_least8_t lastFamilyDiscrepancy;
  bool lastDevice;

  SearchRomState()
      : romId(), lastDiscrepancy(0), lastFamilyDiscrepancy(0),
        lastDevice(false) {}
    
  explicit SearchRomState(const RomId & romId)
      : romId(romId), lastDiscrepancy(64), lastFamilyDiscrepancy(0),
        lastDevice(false) {}
      
  explicit SearchRomState(RomId::value_type familyCode)
      : romId(), lastDiscrepancy(64), lastFamilyDiscrepancy(0),
        lastDevice(false) {
    setFamilyCode(romId, familyCode);
  }
};

/// Set the search state to skip the current device family on the next
/// Search ROM command.
MaximInterface_EXPORT void skipCurrentFamily(SearchRomState & searchState);

/// Verify that the device with the specified ROM ID is present.
MaximInterface_EXPORT error_code verifyRom(OneWireMaster & master,
                                           const RomId & romId);

/// Use Read ROM command to read ROM ID from device on bus.
/// @note Only use this command with a single drop bus, data
///       collisions will occur if more than 1 device on bus.
/// @param[out] romId ROM ID read from device.
MaximInterface_EXPORT error_code readRom(OneWireMaster & master, RomId & romId);

/// Issue Skip ROM command on bus.
/// @note Only use this command with a single drop bus, data
///       collisions will occur if more than 1 device on bus.
MaximInterface_EXPORT error_code skipRom(OneWireMaster & master);

/// Use the Match ROM command to select the device by its known ID.
/// @note This command causes all devices supporting Overdrive
///       mode to switch to Overdrive timing.
/// @param[in] romId ROM ID of device to select.
MaximInterface_EXPORT error_code matchRom(OneWireMaster & master,
                                          const RomId & romId);

/// Issue Overdrive Skip ROM command on bus.
/// @note This command causes all devices supporting Overdrive
///       mode to switch to Overdrive timing.
/// @note Only use this command with a single drop bus, data
///       collisions will occur if more than 1 device on bus.
MaximInterface_EXPORT error_code overdriveSkipRom(OneWireMaster & master);

/// Use the Overdrive Match ROM command to select the device by its known ID.
/// @param[in] romId ROM ID of device to select.
MaximInterface_EXPORT error_code overdriveMatchRom(OneWireMaster & master,
                                                   const RomId & romId);

/// Perform a Resume ROM command on bus.
/// @details Resumes communication with the last device selected
///          though a Match ROM or Search ROM operation.
MaximInterface_EXPORT error_code resumeRom(OneWireMaster & master);

/// Find device on the 1-Wire bus.
/// @details
/// This command uses the Search ROM command to enumerate all 1-Wire devices in
/// sequence. Begin with a new search state and continue using the same search
/// state until the last device flag is set which indicates that all devices
/// have been discovered.
MaximInterface_EXPORT error_code searchRom(OneWireMaster & master,
                                           SearchRomState & searchState);

} // namespace MaximInterface

#endif
