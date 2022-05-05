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

#include <MaximInterfaceCore/Error.hpp>
#include "DS9481P_300.hpp"

#define TRY MaximInterfaceCore_TRY

namespace MaximInterfaceDevices {

using namespace Core;

DS9481P_300::DS9481P_300(Sleep & sleep, SerialPort & serialPort)
    : serialPort(&serialPort), currentBus(OneWire), ds2480b(sleep, serialPort),
      oneWireMaster_(*this), ds9400(serialPort), i2cMaster_(*this) {}

Result<void> DS9481P_300::connect(const std::string & portName) {
  Result<void> result = serialPort->connect(portName);
  if (result) {
    result = selectOneWire();
    if (result) {
      currentBus = OneWire;
    } else {
      serialPort->disconnect();
    }
  }
  return result;
}

Result<void> DS9481P_300::disconnect() { return serialPort->disconnect(); }

Result<void> DS9481P_300::selectOneWire() {
  // Escape DS9400 mode.
  TRY(ds9400.escape());
  TRY(ds2480b.initialize());
  return none;
}

Result<void> DS9481P_300::selectBus(Bus newBus) {
  if (currentBus != newBus) {
    switch (currentBus) {
    case OneWire: // Next bus I2C.
      // Escape DS2480 Mode.
      TRY(ds2480b.escape());
      // Wait for awake notification.
      TRY(ds9400.waitAwake());
      break;

    case I2C: // Next bus OneWire.
      TRY(selectOneWire());
      break;
    }
    currentBus = newBus;
  }
  return none;
}

Result<void> DS9481P_300::OneWireMaster::reset() {
  TRY(parent->selectBus(OneWire));
  return OneWireMasterDecorator::reset();
}

Result<bool> DS9481P_300::OneWireMaster::touchBitSetLevel(bool sendBit,
                                                          Level afterLevel) {
  TRY(parent->selectBus(OneWire));
  return OneWireMasterDecorator::touchBitSetLevel(sendBit, afterLevel);
}

Result<void>
DS9481P_300::OneWireMaster::writeByteSetLevel(uint_least8_t sendByte,
                                              Level afterLevel) {
  TRY(parent->selectBus(OneWire));
  return OneWireMasterDecorator::writeByteSetLevel(sendByte, afterLevel);
}

Result<uint_least8_t>
DS9481P_300::OneWireMaster::readByteSetLevel(Level afterLevel) {
  TRY(parent->selectBus(OneWire));
  return OneWireMasterDecorator::readByteSetLevel(afterLevel);
}

Result<void>
DS9481P_300::OneWireMaster::writeBlock(span<const uint_least8_t> sendBuf) {
  TRY(parent->selectBus(OneWire));
  return OneWireMasterDecorator::writeBlock(sendBuf);
}

Result<void>
DS9481P_300::OneWireMaster::readBlock(span<uint_least8_t> recvBuf) {
  TRY(parent->selectBus(OneWire));
  return OneWireMasterDecorator::readBlock(recvBuf);
}

Result<void> DS9481P_300::OneWireMaster::setSpeed(Speed newSpeed) {
  TRY(parent->selectBus(OneWire));
  return OneWireMasterDecorator::setSpeed(newSpeed);
}

Result<void> DS9481P_300::OneWireMaster::setLevel(Level newLevel) {
  TRY(parent->selectBus(OneWire));
  return OneWireMasterDecorator::setLevel(newLevel);
}

Result<OneWireMaster::TripletData>
DS9481P_300::OneWireMaster::triplet(bool sendBit) {
  TRY(parent->selectBus(OneWire));
  return OneWireMasterDecorator::triplet(sendBit);
}

Result<void> DS9481P_300::I2CMaster::start(uint_least8_t address) {
  TRY(parent->selectBus(I2C));
  return I2CMasterDecorator::start(address);
}

Result<void> DS9481P_300::I2CMaster::stop() {
  TRY(parent->selectBus(I2C));
  return I2CMasterDecorator::stop();
}

Result<void> DS9481P_300::I2CMaster::writeByte(uint_least8_t data) {
  TRY(parent->selectBus(I2C));
  return I2CMasterDecorator::writeByte(data);
}

Result<void>
DS9481P_300::I2CMaster::writeBlock(span<const uint_least8_t> data) {
  TRY(parent->selectBus(I2C));
  return I2CMasterDecorator::writeBlock(data);
}

Result<void> DS9481P_300::I2CMaster::writePacket(uint_least8_t address,
                                                 span<const uint_least8_t> data,
                                                 DoStop doStop) {
  TRY(parent->selectBus(I2C));
  return I2CMasterDecorator::writePacket(address, data, doStop);
}

Result<uint_least8_t> DS9481P_300::I2CMaster::readByte(DoAck doAck) {
  TRY(parent->selectBus(I2C));
  return I2CMasterDecorator::readByte(doAck);
}

Result<void> DS9481P_300::I2CMaster::readBlock(span<uint_least8_t> data,
                                               DoAck doAck) {
  TRY(parent->selectBus(I2C));
  return I2CMasterDecorator::readBlock(data, doAck);
}

Result<void> DS9481P_300::I2CMaster::readPacket(uint_least8_t address,
                                                span<uint_least8_t> data,
                                                DoStop doStop) {
  TRY(parent->selectBus(I2C));
  return I2CMasterDecorator::readPacket(address, data, doStop);
}

Result<void> DS9481P_300::DS2480BWithEscape::escape() {
  return sendCommand(0xE5);
}

Result<void> DS9481P_300::DS9400WithEscape::escape() { return configure('O'); }

} // namespace MaximInterfaceDevices
