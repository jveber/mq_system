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

#ifndef MaximInterfaceDevices_DS2413_hpp
#define MaximInterfaceDevices_DS2413_hpp

#include <stdint.h>
#include <MaximInterfaceCore/FlagSet.hpp>
#include <MaximInterfaceCore/SelectRom.hpp>
#include "Config.hpp"

namespace MaximInterfaceDevices {

/// @brief DS2413 1-Wire Dual Channel Addressable Switch
/// @details The DS2413 is a dual-channel programmable I/O 1-Wire®
/// chip. The PIO outputs are configured as open-drain and provide up
/// to 20mA continuous sink capability and off-state operating voltage
/// up to 28V. Control and sensing of the PIO pins is performed with
/// a dedicated device-level command protocol. To provide a high level
/// of fault tolerance in the end application, the 1-Wire I/O and PIO
/// pins are all capable of withstanding continuous application of
/// voltages up to 28V max. Communication and operation of the DS2413
/// is performed with the single contact Maxim 1-Wire serial interface.
class DS2413 {
public:
  enum ErrorValue { CommunicationError = 1 };

  enum StatusFlags {
    PioAInputState = 0x1,
    PioAOutputState = 0x2,
    PioBInputState = 0x4,
    PioBOutputState = 0x8
  };
  typedef Core::FlagSet<StatusFlags, 4> Status;

  DS2413(Core::OneWireMaster & master, const Core::SelectRom & selectRom)
      : selectRom(selectRom), master(&master) {}

  void setMaster(Core::OneWireMaster & master) { this->master = &master; }

  void setSelectRom(const Core::SelectRom & selectRom) {
    this->selectRom = selectRom;
  }

  /// Read the input and output logic states for all PIO pins.
  MaximInterfaceDevices_EXPORT Core::Result<Status> readStatus() const;

  /// Write the output logic states for all PIO pins.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeOutputState(bool pioAState, bool pioBState);

  MaximInterfaceDevices_EXPORT static const Core::error_category &
  errorCategory();

private:
  Core::Result<uint_least8_t> pioAccessRead() const;
  Core::Result<void> pioAccessWrite(uint_least8_t val);

  Core::SelectRom selectRom;
  Core::OneWireMaster * master;
};

/// Write the output logic state for only PIOA.
MaximInterfaceDevices_EXPORT Core::Result<void>
writePioAOutputState(DS2413 & ds2413, bool pioAState);

/// Write the output logic state for only PIOB.
MaximInterfaceDevices_EXPORT Core::Result<void>
writePioBOutputState(DS2413 & ds2413, bool pioBState);

} // namespace MaximInterfaceDevices
namespace MaximInterfaceCore {

template <>
struct is_error_code_enum<MaximInterfaceDevices::DS2413::ErrorValue>
    : true_type {};

} // namespace MaximInterfaceCore
namespace MaximInterfaceDevices {

inline Core::error_code make_error_code(DS2413::ErrorValue e) {
  return Core::error_code(e, DS2413::errorCategory());
}

} // namespace MaximInterfaceDevices

#endif
