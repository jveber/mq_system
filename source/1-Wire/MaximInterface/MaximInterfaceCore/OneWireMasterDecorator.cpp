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

#include "OneWireMasterDecorator.hpp"

namespace MaximInterfaceCore {

Result<void> OneWireMasterDecorator::reset() { return master->reset(); }

Result<bool> OneWireMasterDecorator::touchBitSetLevel(bool sendBit,
                                                      Level afterLevel) {
  return master->touchBitSetLevel(sendBit, afterLevel);
}

Result<void> OneWireMasterDecorator::writeByteSetLevel(uint_least8_t sendByte,
                                                       Level afterLevel) {
  return master->writeByteSetLevel(sendByte, afterLevel);
}

Result<uint_least8_t>
OneWireMasterDecorator::readByteSetLevel(Level afterLevel) {
  return master->readByteSetLevel(afterLevel);
}

Result<void>
OneWireMasterDecorator::writeBlock(span<const uint_least8_t> sendBuf) {
  return master->writeBlock(sendBuf);
}

Result<void> OneWireMasterDecorator::readBlock(span<uint_least8_t> recvBuf) {
  return master->readBlock(recvBuf);
}

Result<void> OneWireMasterDecorator::setSpeed(Speed newSpeed) {
  return master->setSpeed(newSpeed);
}

Result<void> OneWireMasterDecorator::setLevel(Level newLevel) {
  return master->setLevel(newLevel);
}

Result<OneWireMaster::TripletData>
OneWireMasterDecorator::triplet(bool sendBit) {
  return master->triplet(sendBit);
}

} // namespace MaximInterfaceCore
