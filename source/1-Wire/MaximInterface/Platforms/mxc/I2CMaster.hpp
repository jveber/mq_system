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

#ifndef MaximInterface_mxc_I2CMaster
#define MaximInterface_mxc_I2CMaster

#include <MaximInterface/Links/I2CMaster.hpp>
#include <i2cm.h>

namespace MaximInterface {
namespace mxc {

class I2CMaster : public MaximInterface::I2CMaster {
public:
  I2CMaster(mxc_i2cm_regs_t & i2cm, mxc_i2cm_fifo_regs_t & i2cmFifo)
      : i2cm(&i2cm), i2cmFifo(&i2cmFifo) {}

  void set_mxc_i2cm(mxc_i2cm_regs_t & i2cm, mxc_i2cm_fifo_regs_t & i2cmFifo) {
    this->i2cm = &i2cm;
    this->i2cmFifo = &i2cmFifo;
  }

  /// Initialize the hardware interface for use.
  error_code initialize(const sys_cfg_i2cm_t & sysCfg, i2cm_speed_t speed);
  
  /// Shutdown the hardware interface after use.
  error_code shutdown();

  virtual error_code start(uint_least8_t address);
  virtual error_code stop();
  virtual error_code writeByte(uint_least8_t data);
  virtual error_code readByte(AckStatus status, uint_least8_t & data);

private:
  error_code writeByte(uint_least8_t data, bool start);

  mxc_i2cm_regs_t * i2cm;
  mxc_i2cm_fifo_regs_t * i2cmFifo;
};

} // namespace mxc
} // namespace MaximInterface

#endif
