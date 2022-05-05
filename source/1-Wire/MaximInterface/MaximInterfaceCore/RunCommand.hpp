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

#ifndef MaximInterfaceCore_RunCommand_hpp
#define MaximInterfaceCore_RunCommand_hpp

#include "Config.hpp"
#include "Function.hpp"
#include "SelectRom.hpp"
#include "span.hpp"
#include "system_error.hpp"

namespace MaximInterfaceCore {

class I2CMaster;
class OneWireMaster;
class Sleep;

/// @brief
/// Runs a command sequence by writing the command request, waiting the
/// specified amount of time, and reading back the command response.
/// @details
/// The parameters for this function are the command request, the delay time in
/// milliseconds, and a mutable buffer for the command response. The actual
/// buffer used for the command response is returned.
typedef Function<Result<span<uint_least8_t> >(span<const uint_least8_t>, int,
                                              span<uint_least8_t>)>
    RunCommand;

class RunCommandWithOneWireMaster {
public:
  typedef RunCommand::result_type result_type;

  enum ErrorValue { CrcError = 1, InvalidResponseError };
  MaximInterfaceCore_EXPORT static const error_category & errorCategory();

  RunCommandWithOneWireMaster(Sleep & sleep, OneWireMaster & master,
                              const SelectRom & selectRom)
      : selectRom(selectRom), master(&master), sleep(&sleep) {}

  void setSleep(Sleep & sleep) { this->sleep = &sleep; }

  void setMaster(OneWireMaster & master) { this->master = &master; }

  void setSelectRom(const SelectRom & selectRom) {
    this->selectRom = selectRom;
  }

  MaximInterfaceCore_EXPORT Result<span<uint_least8_t> >
  operator()(span<const uint_least8_t> request, int delayTime,
             span<uint_least8_t> response) const;

private:
  SelectRom selectRom;
  OneWireMaster * master;
  const Sleep * sleep;
};

template <>
struct is_error_code_enum<RunCommandWithOneWireMaster::ErrorValue>
    : true_type {};

inline error_code make_error_code(RunCommandWithOneWireMaster::ErrorValue e) {
  return error_code(e, RunCommandWithOneWireMaster::errorCategory());
}

class RunCommandWithI2CMaster {
public:
  typedef RunCommand::result_type result_type;

  enum ErrorValue { InvalidResponseError = 1 };
  MaximInterfaceCore_EXPORT static const error_category & errorCategory();

  RunCommandWithI2CMaster(Sleep & sleep, I2CMaster & master,
                          uint_least8_t address)
      : sleep(&sleep), master(&master), address_(address & 0xFE) {}

  void setSleep(Sleep & sleep) { this->sleep = &sleep; }

  void setMaster(I2CMaster & master) { this->master = &master; }

  uint_least8_t address() const { return address_; }

  void setAddress(uint_least8_t address) { address_ = address & 0xFE; }

  MaximInterfaceCore_EXPORT Result<span<uint_least8_t> >
  operator()(span<const uint_least8_t> request, int delayTime,
             span<uint_least8_t> response) const;

private:
  const Sleep * sleep;
  I2CMaster * master;
  uint_least8_t address_;
};

template <>
struct is_error_code_enum<RunCommandWithI2CMaster::ErrorValue> : true_type {};

inline error_code make_error_code(RunCommandWithI2CMaster::ErrorValue e) {
  return error_code(e, RunCommandWithI2CMaster::errorCategory());
}

} // namespace MaximInterfaceCore

#endif
