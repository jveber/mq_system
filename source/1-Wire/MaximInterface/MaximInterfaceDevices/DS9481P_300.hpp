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

#ifndef MaximInterfaceDevices_DS9481P_300_hpp
#define MaximInterfaceDevices_DS9481P_300_hpp

#include <MaximInterfaceCore/I2CMasterDecorator.hpp>
#include <MaximInterfaceCore/OneWireMasterDecorator.hpp>
#include <MaximInterfaceCore/SerialPort.hpp>
#include <MaximInterfaceCore/Sleep.hpp>
#include "DS2480B.hpp"
#include "DS9400.hpp"
#include "Config.hpp"

namespace MaximInterfaceDevices {

/// DS9481P-300 USB to 1-Wire and I2C adapter.
class DS9481P_300 {
public:
  MaximInterfaceDevices_EXPORT DS9481P_300(Core::Sleep & sleep,
                                           Core::SerialPort & serialPort);

  void setSleep(Core::Sleep & sleep) { ds2480b.setSleep(sleep); }

  void setSerialPort(Core::SerialPort & serialPort) {
    this->serialPort = &serialPort;
    ds2480b.setUart(serialPort);
    ds9400.setUart(serialPort);
  }

  MaximInterfaceDevices_EXPORT Core::Result<void>
  connect(const std::string & portName);

  MaximInterfaceDevices_EXPORT Core::Result<void> disconnect();

  /// Access the 1-Wire master when connected to an adapter.
  Core::OneWireMaster & oneWireMaster() { return oneWireMaster_; }

  /// Access the I2C master when connected to an adapter.
  Core::I2CMaster & i2cMaster() { return i2cMaster_; }

private:
  class OneWireMaster : public Core::OneWireMasterDecorator {
  public:
    explicit OneWireMaster(DS9481P_300 & parent)
        : OneWireMasterDecorator(parent.ds2480b), parent(&parent) {}

    MaximInterfaceDevices_EXPORT virtual Core::Result<void> reset();

    MaximInterfaceDevices_EXPORT virtual Core::Result<bool>
    touchBitSetLevel(bool sendBit, Level afterLevel);

    MaximInterfaceDevices_EXPORT virtual Core::Result<void>
    writeByteSetLevel(uint_least8_t sendByte, Level afterLevel);

    MaximInterfaceDevices_EXPORT virtual Core::Result<uint_least8_t>
    readByteSetLevel(Level afterLevel);

    MaximInterfaceDevices_EXPORT virtual Core::Result<void>
    writeBlock(Core::span<const uint_least8_t> sendBuf);

    MaximInterfaceDevices_EXPORT virtual Core::Result<void>
    readBlock(Core::span<uint_least8_t> recvBuf);

    MaximInterfaceDevices_EXPORT virtual Core::Result<void>
    setSpeed(Speed newSpeed);

    MaximInterfaceDevices_EXPORT virtual Core::Result<void>
    setLevel(Level newLevel);

    MaximInterfaceDevices_EXPORT virtual Core::Result<TripletData>
    triplet(bool sendBit);

  private:
    DS9481P_300 * parent;
  };

  class I2CMaster : public Core::I2CMasterDecorator {
  public:
    explicit I2CMaster(DS9481P_300 & parent)
        : I2CMasterDecorator(parent.ds9400), parent(&parent) {}

    MaximInterfaceDevices_EXPORT virtual Core::Result<void>
    start(uint_least8_t address);

    MaximInterfaceDevices_EXPORT virtual Core::Result<void> stop();

    MaximInterfaceDevices_EXPORT virtual Core::Result<void>
    writeByte(uint_least8_t data);

    MaximInterfaceDevices_EXPORT virtual Core::Result<void>
    writeBlock(Core::span<const uint_least8_t> data);

    MaximInterfaceDevices_EXPORT virtual Core::Result<void>
    writePacket(uint_least8_t address, Core::span<const uint_least8_t> data,
                DoStop doStop);

    MaximInterfaceDevices_EXPORT virtual Core::Result<uint_least8_t>
    readByte(DoAck doAck);

    MaximInterfaceDevices_EXPORT virtual Core::Result<void>
    readBlock(Core::span<uint_least8_t> data, DoAck doAck);

    MaximInterfaceDevices_EXPORT virtual Core::Result<void>
    readPacket(uint_least8_t address, Core::span<uint_least8_t> data,
               DoStop doStop);

  private:
    DS9481P_300 * parent;
  };

  class DS2480BWithEscape : public DS2480B {
  public:
    DS2480BWithEscape(Core::Sleep & sleep, Core::Uart & uart)
        : DS2480B(sleep, uart) {}

    Core::Result<void> escape();
  };

  class DS9400WithEscape : public DS9400 {
  public:
    explicit DS9400WithEscape(Core::Uart & uart) : DS9400(uart) {}

    Core::Result<void> escape();
  };

  enum Bus { OneWire, I2C };

  Core::SerialPort * serialPort;
  Bus currentBus;
  DS2480BWithEscape ds2480b;
  OneWireMaster oneWireMaster_;
  DS9400WithEscape ds9400;
  I2CMaster i2cMaster_;

  Core::Result<void> selectOneWire();
  Core::Result<void> selectBus(Bus newBus);

  friend class OneWireMaster;
  friend class I2CMaster;
};

} // namespace MaximInterfaceDevices

#endif
