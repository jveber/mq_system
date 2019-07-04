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

#ifndef MaximInterface_DS2465
#define MaximInterface_DS2465

#include <MaximInterface/Links/I2CMaster.hpp>
#include <MaximInterface/Links/OneWireMaster.hpp>
#include <MaximInterface/Links/Sleep.hpp>
#include <MaximInterface/Utilities/Export.h>
#include <MaximInterface/Utilities/Sha256.hpp>

namespace MaximInterface {

/// Interface to the DS2465 1-Wire master and SHA-256 coprocessor.
class DS2465 : public OneWireMaster {
public:
  enum ErrorValue { HardwareError = 1, ArgumentOutOfRangeError };

  /// 1-Wire port adjustment parameters.
  /// @note See datasheet page 13.
  enum PortParameter {
    tRSTL_STD,
    tRSTL_OD,
    tMSP_STD,
    tMSP_OD,
    tW0L_STD,
    tW0L_OD,
    tREC0,
    RWPU,
    tW1L_OD
  };

  /// Page region to use for swapping.
  enum PageRegion { FullPage = 0x03, FirstHalf = 0x01, SecondHalf = 0x02 };

  /// Holds the contents of a device memory segment.
  typedef array<uint_least8_t, 4> Segment;

  /// Holds the contents of a device memory page.
  typedef array<uint_least8_t, 32> Page;

  static const int memoryPages = 2;
  static const int segmentsPerPage = Page::csize / Segment::csize;

  /// Represents a DS2465 configuration.
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

    /// Byte representation that is read from the DS2465.
    uint_least8_t readByte() const { return readByte_; }

  private:
    static const unsigned int option1WS = 0x8;
    static const unsigned int optionSPU = 0x4;
    static const unsigned int optionPDN = 0x2;
    static const unsigned int optionAPU = 0x1;

    uint_least8_t readByte_;
  };

  // Const member functions should not change the settings of the DS2465 or
  // affect the state of the 1-Wire bus. Read pointer, scratchpad, MAC output
  // register, and command register on the DS2465 are considered mutable.

  DS2465(const Sleep & sleep, I2CMaster & i2cMaster,
         uint_least8_t i2cAddress = 0x30)
      : sleep(&sleep), i2cMaster(&i2cMaster), i2cAddress_(i2cAddress & 0xFE) {}

  void setSleep(const Sleep & sleep) { this->sleep = &sleep; }
  void setI2CMaster(I2CMaster & i2cMaster) { this->i2cMaster = &i2cMaster; }
  uint_least8_t i2cAddress() const { return i2cAddress_; }
  void setI2CAddress(uint_least8_t i2cAddress) {
    this->i2cAddress_ = (i2cAddress & 0xFE);
  }

  /// Initialize hardware for use.
  MaximInterface_EXPORT error_code initialize(Config config = Config());

  /// Write a new configuration to the DS2465.
  /// @param[in] config New configuration to write.
  MaximInterface_EXPORT error_code writeConfig(Config config);

  /// Write a new port configuration parameter to the DS2465.
  /// @param[in] param Parameter to adjust.
  /// @param[in] val New parameter value to set. Consult datasheet for value
  /// mappings.
  MaximInterface_EXPORT error_code writePortParameter(PortParameter param,
                                                      int val);

  // 1-Wire Master Commands

  MaximInterface_EXPORT virtual error_code reset();
  MaximInterface_EXPORT virtual error_code touchBitSetLevel(bool & sendRecvBit,
                                                            Level afterLevel);
  MaximInterface_EXPORT virtual error_code
  readByteSetLevel(uint_least8_t & recvByte, Level afterLevel);
  MaximInterface_EXPORT virtual error_code
  writeByteSetLevel(uint_least8_t sendByte, Level afterLevel);
  MaximInterface_EXPORT virtual error_code readBlock(uint_least8_t * recvBuf,
                                                     size_t recvLen);
  MaximInterface_EXPORT virtual error_code
  writeBlock(const uint_least8_t * sendBuf, size_t sendLen);
  MaximInterface_EXPORT virtual error_code setSpeed(Speed newSpeed);
  /// @note The DS2465 only supports enabling strong pullup following a 1-Wire
  /// read or write operation.
  MaximInterface_EXPORT virtual error_code setLevel(Level newLevel);
  MaximInterface_EXPORT virtual error_code triplet(TripletData & data);

  // DS2465 Coprocessor Commands
  
  /// Read data from an EEPROM memory page.
  /// @param pageNum Page number to read from.
  MaximInterface_EXPORT error_code readPage(int pageNum, Page & data) const;

  /// Write data to an EEPROM memory page.
  /// @param pageNum Page number to copy to.
  MaximInterface_EXPORT error_code writePage(int pageNum, const Page & data);

  /// Write data to an EEPROM memory segment.
  /// @param pageNum Page number to copy to.
  /// @param segmentNum Segment number to copy to.
  MaximInterface_EXPORT error_code writeSegment(int pageNum, int segmentNum,
                                                const Segment & data);

  /// Write data to the secret EEPROM memory page.
  MaximInterface_EXPORT error_code
  writeMasterSecret(const Sha256::Hash & masterSecret);

  /// Compute Next Master Secret.
  /// @param data Combined data fields for computation.
  MaximInterface_EXPORT error_code
  computeNextMasterSecret(const Sha256::SlaveSecretData & data);

  /// Compute Next Master Secret with page swapping.
  /// @param data Combined data fields for computation.
  /// @param pageNum Page number to swap in.
  /// @param region Region of the page to swap in.
  MaximInterface_EXPORT error_code computeNextMasterSecretWithSwap(
      const Sha256::SlaveSecretData & data, int pageNum, PageRegion region);

  /// Compute Write MAC.
  /// @param data Combined data fields for computation.
  /// @param[out] mac Computed Write MAC.
  MaximInterface_EXPORT error_code
  computeWriteMac(const Sha256::WriteMacData & data, Sha256::Hash & mac) const;

  /// Compute Write MAC.
  /// @param data Combined data fields for computation.
  MaximInterface_EXPORT error_code
  computeAndTransmitWriteMac(const Sha256::WriteMacData & data) const;

  /// Compute Write MAC with page swapping.
  /// @param data Combined data fields for computation.
  /// @param pageNum Page number to swap in.
  /// @param segmentNum Segment number to swap in.
  /// @param[out] mac Computed Write MAC.
  MaximInterface_EXPORT error_code
  computeWriteMacWithSwap(const Sha256::WriteMacData & data, int pageNum,
                          int segmentNum, Sha256::Hash & mac) const;

  /// Compute Write MAC with page swapping.
  /// @param data Combined data fields for computation.
  /// @param pageNum Page number to swap in.
  /// @param segmentNum Segment number to swap in.
  MaximInterface_EXPORT error_code computeAndTransmitWriteMacWithSwap(
      const Sha256::WriteMacData & data, int pageNum, int segmentNum) const;

  /// Compute Slave Secret (S-Secret).
  /// @param data Combined data fields for computation.
  MaximInterface_EXPORT error_code
  computeSlaveSecret(const Sha256::SlaveSecretData & data);

  /// Compute Slave Secret (S-Secret) with page swapping.
  /// @param data Combined data fields for computation.
  /// @param pageNum Page number to swap in.
  /// @param region Region of the page to swap in.
  MaximInterface_EXPORT error_code computeSlaveSecretWithSwap(
      const Sha256::SlaveSecretData & data, int pageNum, PageRegion region);

  /// Compute Authentication MAC.
  /// @param data Combined data fields for computation.
  /// @param[out] mac Computed Auth MAC.
  MaximInterface_EXPORT error_code
  computeAuthMac(const Sha256::AuthMacData & data, Sha256::Hash & mac) const;

  /// Compute Authentication MAC.
  /// @param data Combined data fields for computation.
  MaximInterface_EXPORT error_code
  computeAndTransmitAuthMac(const Sha256::AuthMacData & data) const;

  /// Compute Authentication MAC with page swapping.
  /// @param data Combined data fields for computation.
  /// @param pageNum Page number to swap in.
  /// @param region Region of the page to swap in.
  /// @param[out] mac Computed Auth MAC.
  MaximInterface_EXPORT error_code
  computeAuthMacWithSwap(const Sha256::AuthMacData & data, int pageNum,
                         PageRegion region, Sha256::Hash & mac) const;

  /// Compute Authentication MAC with page swapping.
  /// @param data Combined data fields for computation.
  /// @param pageNum Page number to swap in.
  /// @param region Region of the page to swap in.
  MaximInterface_EXPORT error_code computeAndTransmitAuthMacWithSwap(
      const Sha256::AuthMacData & data, int pageNum, PageRegion region) const;

  MaximInterface_EXPORT static const error_category & errorCategory();

private:
  const Sleep * sleep;
  I2CMaster * i2cMaster;
  uint_least8_t i2cAddress_;
  Config curConfig;

  /// Performs a soft reset on the DS2465.
  /// @note This is not a 1-Wire Reset.
  error_code resetDevice();

  /// Polls the DS2465 status waiting for the 1-Wire Busy bit (1WB) to be cleared.
  /// @param[out] pStatus Optionally retrive the status byte when 1WB cleared.
  /// @returns Success or TimeoutError if poll limit reached.
  error_code pollBusy(uint_least8_t * pStatus = NULL) const;

  /// Ensure that the desired 1-Wire level is set in the configuration.
  /// @param level Desired 1-Wire level.
  error_code configureLevel(Level level);

  /// Const since only for internal use.
  error_code writeMemory(uint_least8_t addr, const uint_least8_t * buf,
                         size_t bufLen) const;

  /// Read memory from the DS2465.
  /// @param addr Address to begin reading from.
  /// @param[out] buf Buffer to hold read data.
  /// @param bufLen Length of buffer, buf, and number of bytes to read.
  error_code readMemory(uint_least8_t addr, uint_least8_t * buf,
                        size_t bufLen) const;

  /// Read memory from the DS2465 at the current pointer.
  /// @param[out] buf Buffer to hold read data.
  /// @param bufLen Length of buffer, buf, and number of bytes to read.
  error_code readMemory(uint_least8_t * buf, size_t bufLen) const;

  /// Write the last computed MAC to the 1-Wire bus.
  error_code writeMacBlock() const;

  error_code computeWriteMac(const Sha256::WriteMacData & data) const;

  error_code computeWriteMacWithSwap(const Sha256::WriteMacData & data,
                                     int pageNum, int segmentNum) const;

  error_code computeAuthMac(const Sha256::AuthMacData & data) const;

  error_code computeAuthMacWithSwap(const Sha256::AuthMacData & data,
                                    int pageNum, PageRegion region) const;

  // Legacy implementations
  error_code copyScratchpad(bool destSecret, int pageNum, bool notFull,
                            int segmentNum);

  error_code computeNextMasterSecret(bool swap, int pageNum, PageRegion region);

  error_code computeWriteMac(bool regwrite, bool swap, int pageNum,
                             int segmentNum) const;

  error_code computeSlaveSecret(bool swap, int pageNum, PageRegion region);

  error_code computeAuthMac(bool swap, int pageNum, PageRegion region) const;
};

inline error_code make_error_code(DS2465::ErrorValue e) {
  return error_code(e, DS2465::errorCategory());
}

} // namespace MaximInterface

#endif
