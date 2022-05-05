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

#include "I2CMasterDecorator.hpp"

namespace MaximInterfaceCore {

Result<void> I2CMasterDecorator::start(uint_least8_t address) {
  return master->start(address);
}

Result<void> I2CMasterDecorator::stop() { return master->stop(); }

Result<void> I2CMasterDecorator::writeByte(uint_least8_t data) {
  return master->writeByte(data);
}

Result<void> I2CMasterDecorator::writeBlock(span<const uint_least8_t> data) {
  return master->writeBlock(data);
}

Result<void> I2CMasterDecorator::writePacket(uint_least8_t address,
                                             span<const uint_least8_t> data,
                                             DoStop doStop) {
  return master->writePacket(address, data, doStop);
}

Result<uint_least8_t> I2CMasterDecorator::readByte(DoAck doAck) {
  return master->readByte(doAck);
}

Result<void> I2CMasterDecorator::readBlock(span<uint_least8_t> data,
                                           DoAck doAck) {
  return master->readBlock(data, doAck);
}

Result<void> I2CMasterDecorator::readPacket(uint_least8_t address,
                                            span<uint_least8_t> data,
                                            DoStop doStop) {
  return master->readPacket(address, data, doStop);
}

} // namespace MaximInterfaceCore
