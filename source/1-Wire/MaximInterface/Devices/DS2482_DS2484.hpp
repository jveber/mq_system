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

#ifndef MaximInterface_DS2482_DS2484
#define MaximInterface_DS2482_DS2484

#include <MaximInterface/Links/OneWireMaster.hpp>
#include <MaximInterface/Links/I2CMaster.hpp>
#include <MaximInterface/Utilities/Export.h>

namespace MaximInterface {

/// Interface to the DS2484, DS2482-100, DS2482-101, DS2482-800 1-Wire masters.
class DS2482_DS2484 : public OneWireMaster {
public:
  enum ErrorValue { HardwareError = 1, ArgumentOutOfRangeError };

  /// Represents a device configuration.
  class Config {
  public:
    /// Default construct with power-on config.
    explicit Config(uint_least8_t readByte = optionAPU)
        : readByte_(readByte & 0xF) {}

    /// @{
    /// 1-Wire Speed
    bool get1WS() const { return (readByte_ & option1WS) == option1WS; }
    void set1WS(bool new1WS) {
      if (new1WS) {
        readByte_ |= option1WS;
      } else {
        readByte_ &= ~option1WS;
      }
    }
    /// @}

    /// @{
    /// Strong Pullup
    bool getSPU() const { return (readByte_ & optionSPU) == optionSPU; }
    void setSPU(bool newSPU) {
      if (newSPU) {
        readByte_ |= optionSPU;
      } else {
        readByte_ &= ~optionSPU;
      }
    }
    /// @}

    /// @{
    /// 1-Wire Power Down
    bool getPDN() const { return (readByte_ & optionPDN) == optionPDN; }
    void setPDN(bool newPDN) {
      if (newPDN) {
        readByte_ |= optionPDN;
      } else {
        readByte_ &= ~optionPDN;
      }
    }
    /// @}

    /// @{
    /// Active Pullup
    bool getAPU() const { return (readByte_ & optionAPU) == optionAPU; }
    void setAPU(bool newAPU) {
      if (newAPU) {
        readByte_ |= optionAPU;
      } else {
        readByte_ &= ~optionAPU;
      }
    }
    /// @}

    /// Byte representation that is read from the device.
    uint_least8_t readByte() const { return readByte_; }

  private:
    static const unsigned int option1WS = 0x8;
    static const unsigned int optionSPU = 0x4;
    static const unsigned int optionPDN = 0x2;
    static const unsigned int optionAPU = 0x1;

    uint_least8_t readByte_;
  };

  void setI2CMaster(I2CMaster & i2cMaster) { this->i2cMaster = &i2cMaster; }
  uint_least8_t i2cAddress() const { return i2cAddress_; }
  void setI2CAddress(uint_least8_t i2cAddress) {
    this->i2cAddress_ = i2cAddress;
  }

  /// Initialize hardware for use.
  MaximInterface_EXPORT error_code initialize(Config config = Config());

  /// Write a new configuration to the device.
  /// @param[in] config New configuration to write.
  MaximInterface_EXPORT error_code writeConfig(Config config);

  /// @note Perform a 1-Wire triplet using the device command.
  MaximInterface_EXPORT virtual error_code triplet(TripletData & data);

  MaximInterface_EXPORT virtual error_code reset();
  MaximInterface_EXPORT virtual error_code touchBitSetLevel(bool & sendRecvBit,
                                                            Level afterLevel);
  MaximInterface_EXPORT virtual error_code
  readByteSetLevel(uint_least8_t & recvByte, Level afterLevel);
  MaximInterface_EXPORT virtual error_code
  writeByteSetLevel(uint_least8_t sendByte, Level afterLevel);
  MaximInterface_EXPORT virtual error_code setSpeed(Speed newSpeed);
  MaximInterface_EXPORT virtual error_code setLevel(Level newLevel);

  MaximInterface_EXPORT static const error_category & errorCategory();

protected:
  DS2482_DS2484(I2CMaster & i2cMaster, uint_least8_t i2cAddress)
      : i2cMaster(&i2cMaster), i2cAddress_(i2cAddress) {}

  /// @note Allow marking const since not public.
  error_code sendCommand(uint_least8_t cmd) const;

  /// @note Allow marking const since not public.
  error_code sendCommand(uint_least8_t cmd, uint_least8_t param) const;

  /// Reads a register from the device.
  /// @param reg Register to read from.
  /// @param[out] buf Buffer to hold read data.
  error_code readRegister(uint_least8_t reg, uint_least8_t & buf) const;

  /// Reads the current register from the device.
  /// @param[out] buf Buffer to hold read data.
  error_code readRegister(uint_least8_t & buf) const;

private:
  /// Performs a soft reset on the device.
  /// @note This is not a 1-Wire Reset.
  error_code resetDevice();

  /// Polls the device status waiting for the 1-Wire Busy bit (1WB) to be cleared.
  /// @param[out] pStatus Optionally retrieve the status byte when 1WB cleared.
  /// @returns Success or TimeoutError if poll limit reached.
  error_code pollBusy(uint_least8_t * pStatus = NULL);

  /// Ensure that the desired 1-Wire level is set in the configuration.
  /// @param level Desired 1-Wire level.
  error_code configureLevel(Level level);

  I2CMaster * i2cMaster;
  uint_least8_t i2cAddress_;
  Config curConfig;
};

inline error_code make_error_code(DS2482_DS2484::ErrorValue e) {
  return error_code(e, DS2482_DS2484::errorCategory());
}

class DS2482_100 : public DS2482_DS2484 {
public:
  DS2482_100(I2CMaster & i2c_bus, uint_least8_t adrs)
      : DS2482_DS2484(i2c_bus, adrs) {}
};

/// DS2482-800 I2C to 1-Wire Master
class DS2482_800 : public DS2482_DS2484 {
public:
  DS2482_800(I2CMaster & i2c_bus, uint_least8_t adrs)
      : DS2482_DS2484(i2c_bus, adrs) {}

  /// Select the active 1-Wire channel.
  /// @param channel Channel number to select from 0 to 7.
  MaximInterface_EXPORT error_code selectChannel(int channel);
};

/// DS2484 I2C to 1-Wire Master
class DS2484 : public DS2482_DS2484 {
public:
  /// 1-Wire port adjustment parameters.
  /// @note See datasheet page 13.
  enum PortParameter {
    tRSTL = 0,
    tRSTL_OD,
    tMSP,
    tMSP_OD,
    tW0L,
    tW0L_OD,
    tREC0,   // OD N/A
    RWPU = 8 // OD N/A
  };

  explicit DS2484(I2CMaster & i2c_bus, uint_least8_t adrs = 0x30)
      : DS2482_DS2484(i2c_bus, adrs) {}

  /// Adjust 1-Wire port parameters.
  /// @param param Parameter to adjust.
  /// @param val New parameter value to set. Consult datasheet for value mappings.
  MaximInterface_EXPORT error_code adjustPort(PortParameter param, int val);
};

} // namespace MaximInterface

#endif
