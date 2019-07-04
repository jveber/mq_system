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

#include "Error.hpp"
#include "OneWireMaster.hpp"

namespace MaximInterface {
namespace mxc {

error_code OneWireMaster::initialize(const owm_cfg_t & owmCfg,
                                     const sys_cfg_owm_t & sysCfgOwm) {
  extStrongPup = (owmCfg.ext_pu_mode != OWM_EXT_PU_UNUSED);
  return makeErrorCode(OWM_Init(owm, &owmCfg, &sysCfgOwm));
}

error_code OneWireMaster::shutdown() {
  return makeErrorCode(OWM_Shutdown(owm));
}

error_code OneWireMaster::reset() {
  return OWM_Reset(owm) ? error_code() : make_error_code(NoSlaveError);
}

error_code OneWireMaster::touchBitSetLevel(bool & sendRecvBit,
                                           Level afterLevel) {
  sendRecvBit = OWM_TouchBit(owm, sendRecvBit);
  return setLevel(afterLevel);
}

error_code OneWireMaster::writeByteSetLevel(uint_least8_t sendByte,
                                            Level afterLevel) {
  error_code result = makeErrorCode(OWM_WriteByte(owm, sendByte));
  if (!result) {
    result = setLevel(afterLevel);
  }
  return result;
}

error_code OneWireMaster::readByteSetLevel(uint_least8_t & recvByte,
                                           Level afterLevel) {
  recvByte = OWM_ReadByte(owm);
  return setLevel(afterLevel);
}

error_code OneWireMaster::setSpeed(Speed newSpeed) {
  error_code result;
  if (newSpeed == OverdriveSpeed) {
    OWM_SetOverdrive(owm, 1);
  } else if (newSpeed == StandardSpeed) {
    OWM_SetOverdrive(owm, 0);
  } else {
    result = make_error_code(InvalidSpeedError);
  }
  return result;
}

error_code OneWireMaster::setLevel(Level newLevel) {
  error_code result;
  if (extStrongPup) {
    if (newLevel == StrongLevel) {
      OWM_SetExtPullup(owm, 1);
    } else if (newLevel == NormalLevel) {
      OWM_SetExtPullup(owm, 0);
    } else {
      result = make_error_code(InvalidLevelError);
    }
  } else if (newLevel != NormalLevel) {
    result = make_error_code(InvalidLevelError);
  }
  return result;
}

} // namespace mxc
} // namespace MaximInterface
