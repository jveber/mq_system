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

#include <mxc_device.h>
#include "Error.hpp"
#include "I2CMaster.hpp"

namespace MaximInterface {
namespace mxc {

static void verifyTransactionStarted(mxc_i2cm_regs_t & i2cm) {
  if (!(i2cm.trans & MXC_F_I2CM_TRANS_TX_IN_PROGRESS)) {
    i2cm.trans |= MXC_F_I2CM_TRANS_TX_START;
  }
}

error_code I2CMaster::initialize(const sys_cfg_i2cm_t & sysCfg,
                                 i2cm_speed_t speed) {
  return makeErrorCode(I2CM_Init(i2cm, &sysCfg, speed));
}

error_code I2CMaster::shutdown() { return makeErrorCode(I2CM_Shutdown(i2cm)); }

error_code I2CMaster::writeByte(uint_least8_t data, bool start) {
  const uint16_t fifoData =
      static_cast<uint16_t>(data & 0xFF) |
      (start ? MXC_S_I2CM_TRANS_TAG_START : MXC_S_I2CM_TRANS_TAG_TXDATA_ACK);
  const error_code result =
      makeErrorCode(I2CM_WriteTxFifo(i2cm, i2cmFifo, fifoData));
  if (result) {
    I2CM_Recover(i2cm);
  } else {
    verifyTransactionStarted(*i2cm);
  }
  return result;
}

error_code I2CMaster::start(uint_least8_t address) {
  return writeByte(address, true);
}

error_code I2CMaster::stop() {
  error_code result = makeErrorCode(
      I2CM_WriteTxFifo(i2cm, i2cmFifo, MXC_S_I2CM_TRANS_TAG_STOP));
  if (!result) {
    result = makeErrorCode(I2CM_TxInProgress(i2cm));
  }
  return result;
}

error_code I2CMaster::writeByte(uint_least8_t data) {
  return writeByte(data, false);
}

error_code I2CMaster::readByte(AckStatus status, uint_least8_t & data) {
  uint16_t fifoData = (status == Nack) ? MXC_S_I2CM_TRANS_TAG_RXDATA_NACK
                                       : MXC_S_I2CM_TRANS_TAG_RXDATA_COUNT;
  error_code result = makeErrorCode(I2CM_WriteTxFifo(i2cm, i2cmFifo, fifoData));
  if (!result) {
    verifyTransactionStarted(*i2cm);
    do {
      int timeout = 0x5000;
      while ((i2cm->bb & MXC_F_I2CM_BB_RX_FIFO_CNT) == 0) {
        if ((--timeout <= 0) || (i2cm->trans & MXC_F_I2CM_TRANS_TX_TIMEOUT)) {
          result = makeErrorCode(E_TIME_OUT);
          goto exit;
        }
        if (i2cm->trans &
            (MXC_F_I2CM_TRANS_TX_LOST_ARBITR | MXC_F_I2CM_TRANS_TX_NACKED)) {
          result = make_error_code(NackError);
          goto exit;
        }
      }
      fifoData = i2cmFifo->rx;
    } while (fifoData & MXC_S_I2CM_RSTLS_TAG_EMPTY);
    data = fifoData & 0xFF;
  }

exit:
  if (result) {
    I2CM_Recover(i2cm);
  }
  return result;
}

} // namespace mxc
} // namespace MaximInterface
