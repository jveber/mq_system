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

#ifndef MaximInterface_mxc_OneWireMaster
#define MaximInterface_mxc_OneWireMaster

#include <MaximInterface/Links/OneWireMaster.hpp>
#include <owm.h>

namespace MaximInterface {
namespace mxc {

/// MCU peripheral 1-Wire master (OWM)
class OneWireMaster : public MaximInterface::OneWireMaster {
public:
  explicit OneWireMaster(mxc_owm_regs_t & owm)
      : owm(&owm), extStrongPup(false) {}

  void set_mxc_owm(mxc_owm_regs_t & owm) { this->owm = &owm; }

  /// Initialize the hardware interface for use.
  error_code initialize(const owm_cfg_t & owmCfg,
                        const sys_cfg_owm_t & sysCfgOwm);
                        
  /// Shutdown the hardware interface after use.
  error_code shutdown();

  virtual error_code reset();
  virtual error_code touchBitSetLevel(bool & sendRecvBit, Level afterLevel);
  virtual error_code writeByteSetLevel(uint_least8_t sendByte,
                                       Level afterLevel);
  virtual error_code readByteSetLevel(uint_least8_t & recvByte,
                                      Level afterLevel);
  virtual error_code setSpeed(Speed newSpeed);
  virtual error_code setLevel(Level newLevel);

private:
  mxc_owm_regs_t * owm;
  bool extStrongPup;
};

} // namespace mxc
} // namespace MaximInterface

#endif /* MaximInterface_mxc_OneWireMaster */
