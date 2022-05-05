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

#ifndef MaximInterfaceDevices_DS2465_hpp
#define MaximInterfaceDevices_DS2465_hpp

#include <MaximInterfaceCore/array_span.hpp>
#include <MaximInterfaceCore/I2CMaster.hpp>
#include <MaximInterfaceCore/OneWireMaster.hpp>
#include <MaximInterfaceCore/Sleep.hpp>
#include "Config.hpp"

namespace MaximInterfaceDevices {

/// @brief Interface to the DS2465 1-Wire master and SHA-256 coprocessor.
/// @details
/// Const member functions should not change the settings of the DS2465 or
/// affect the state of the 1-Wire bus. Read pointer, scratchpad, MAC output
/// register, and command register on the DS2465 are considered mutable.
class DS2465 : public Core::OneWireMaster {
public:
  enum ErrorValue { HardwareError = 1, ArgumentOutOfRangeError };

  /// @brief 1-Wire port adjustment parameters.
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
  typedef Core::array_span<uint_least8_t, 4> Segment;

  /// Holds the contents of a device memory page.
  typedef Core::array_span<uint_least8_t, 32> Page;

  /// Data for Compute Write MAC operation.
  typedef Core::array_span<uint_least8_t, 20> WriteMacData;

  /// Data for the Compute Auth. MAC and Compute Slave Secret operations.
  typedef Core::array_span<uint_least8_t, 76> AuthenticationData;

  static const int memoryPages = 2;
  static const int segmentsPerPage = Page::size / Segment::size;

  /// Represents a DS2465 configuration.
  class Config {
  public:
    /// Default construct with power-on config.
    explicit Config(uint_least8_t readByte = optionAPU)
        : readByte_(readByte & 0xF) {}

    /// @name 1WS
    /// @brief 1-Wire Speed
    /// @{

    /// Get 1WS bit.
    bool get1WS() const { return (readByte_ & option1WS) == option1WS; }

    /// Set 1WS bit.
    Config & set1WS(bool new1WS) {
      if (new1WS) {
        readByte_ |= option1WS;
      } else {
        readByte_ &= ~option1WS;
      }
      return *this;
    }

    /// @}

    /// @name SPU
    /// @brief Strong Pullup
    /// @{

    /// Get SPU bit.
    bool getSPU() const { return (readByte_ & optionSPU) == optionSPU; }

    /// Set SPU bit.
    Config & setSPU(bool newSPU) {
      if (newSPU) {
        readByte_ |= optionSPU;
      } else {
        readByte_ &= ~optionSPU;
      }
      return *this;
    }

    /// @}

    /// @name PDN
    /// @brief 1-Wire Power Down
    /// @{

    /// Get PDN bit.
    bool getPDN() const { return (readByte_ & optionPDN) == optionPDN; }

    /// Set PDN bit.
    Config & setPDN(bool newPDN) {
      if (newPDN) {
        readByte_ |= optionPDN;
      } else {
        readByte_ &= ~optionPDN;
      }
      return *this;
    }

    /// @}

    /// @name APU
    /// @brief Active Pullup
    /// @{

    /// Get APU bit.
    bool getAPU() const { return (readByte_ & optionAPU) == optionAPU; }

    /// Set APU bit.
    Config & setAPU(bool newAPU) {
      if (newAPU) {
        readByte_ |= optionAPU;
      } else {
        readByte_ &= ~optionAPU;
      }
      return *this;
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

  DS2465(Core::Sleep & sleep, Core::I2CMaster & master,
         uint_least8_t address = 0x30)
      : sleep(&sleep), master(&master), address_(address & 0xFE) {}

  void setSleep(Core::Sleep & sleep) { this->sleep = &sleep; }

  void setMaster(Core::I2CMaster & master) { this->master = &master; }

  uint_least8_t address() const { return address_; }

  void setAddress(uint_least8_t address) { address_ = address & 0xFE; }

  /// Initialize hardware for use.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  initialize(Config config = Config());

  /// @brief Write a new configuration to the DS2465.
  /// @param[in] config New configuration to write.
  MaximInterfaceDevices_EXPORT Core::Result<void> writeConfig(Config config);

  /// @brief Write a new port configuration parameter to the DS2465.
  /// @param[in] param Parameter to adjust.
  /// @param[in] val
  /// New parameter value to set. Consult datasheet for value mappings.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writePortParameter(PortParameter param, int val);

  // 1-Wire Master Commands

  MaximInterfaceDevices_EXPORT virtual Core::Result<void> reset();

  MaximInterfaceDevices_EXPORT virtual Core::Result<bool>
  touchBitSetLevel(bool sendBit, Level afterLevel);

  MaximInterfaceDevices_EXPORT virtual Core::Result<uint_least8_t>
  readByteSetLevel(Level afterLevel);

  MaximInterfaceDevices_EXPORT virtual Core::Result<void>
  writeByteSetLevel(uint_least8_t sendByte, Level afterLevel);

  MaximInterfaceDevices_EXPORT virtual Core::Result<void>
  readBlock(Core::span<uint_least8_t> recvBuf);

  MaximInterfaceDevices_EXPORT virtual Core::Result<void>
  writeBlock(Core::span<const uint_least8_t> sendBuf);

  MaximInterfaceDevices_EXPORT virtual Core::Result<void>
  setSpeed(Speed newSpeed);

  /// @copydoc Core::OneWireMaster::setLevel
  /// @note
  /// The DS2465 only supports enabling strong pullup following a 1-Wire read or
  /// write operation.
  MaximInterfaceDevices_EXPORT virtual Core::Result<void>
  setLevel(Level newLevel);

  MaximInterfaceDevices_EXPORT virtual Core::Result<TripletData>
  triplet(bool sendBit);

  // DS2465 Coprocessor Commands

  /// @brief Read data from an EEPROM memory page.
  /// @param pageNum Page number to read from.
  /// @returns Data that was read.
  MaximInterfaceDevices_EXPORT Core::Result<Page::array>
  readPage(int pageNum) const;

  /// @brief Write data to an EEPROM memory page.
  /// @param pageNum Page number to copy to.
  /// @param data Data to write.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writePage(int pageNum, Page::const_span data);

  /// @brief Write data to an EEPROM memory segment.
  /// @param pageNum Page number to copy to.
  /// @param segmentNum Segment number to copy to.
  /// @param data Data to write.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeSegment(int pageNum, int segmentNum, Segment::const_span data);

  /// Write data to the secret EEPROM memory page.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeMasterSecret(Page::const_span masterSecret);

  /// @brief Compute Next Master Secret.
  /// @param data Combined data fields for computation.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  computeNextMasterSecret(AuthenticationData::const_span data);

  /// @brief Compute Next Master Secret with page swapping.
  /// @param data Combined data fields for computation.
  /// @param pageNum Page number to swap in.
  /// @param region Region of the page to swap in.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  computeNextMasterSecretWithSwap(AuthenticationData::const_span data,
                                  int pageNum, PageRegion region);

  /// @brief Compute Write MAC.
  /// @param data Combined data fields for computation.
  /// @returns Computed Write MAC.
  MaximInterfaceDevices_EXPORT Core::Result<Page::array>
  computeWriteMac(WriteMacData::const_span data) const;

  /// @brief Compute Write MAC.
  /// @param data Combined data fields for computation.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  computeAndTransmitWriteMac(WriteMacData::const_span data);

  /// @brief Compute Write MAC with page swapping.
  /// @param data Combined data fields for computation.
  /// @param pageNum Page number to swap in.
  /// @param segmentNum Segment number to swap in.
  /// @returns Computed Write MAC.
  MaximInterfaceDevices_EXPORT Core::Result<Page::array>
  computeWriteMacWithSwap(WriteMacData::const_span data, int pageNum,
                          int segmentNum) const;

  /// @brief Compute Write MAC with page swapping.
  /// @param data Combined data fields for computation.
  /// @param pageNum Page number to swap in.
  /// @param segmentNum Segment number to swap in.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  computeAndTransmitWriteMacWithSwap(WriteMacData::const_span data, int pageNum,
                                     int segmentNum);

  /// @brief Compute Slave Secret (S-Secret).
  /// @param data Combined data fields for computation.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  computeSlaveSecret(AuthenticationData::const_span data);

  /// @brief Compute Slave Secret (S-Secret) with page swapping.
  /// @param data Combined data fields for computation.
  /// @param pageNum Page number to swap in.
  /// @param region Region of the page to swap in.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  computeSlaveSecretWithSwap(AuthenticationData::const_span data, int pageNum,
                             PageRegion region);

  /// @brief Compute Authentication MAC.
  /// @param data Combined data fields for computation.
  /// @returns Computed Auth MAC.
  MaximInterfaceDevices_EXPORT Core::Result<Page::array>
  computeAuthMac(AuthenticationData::const_span data) const;

  /// @brief Compute Authentication MAC.
  /// @param data Combined data fields for computation.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  computeAndTransmitAuthMac(AuthenticationData::const_span data);

  /// @brief Compute Authentication MAC with page swapping.
  /// @param data Combined data fields for computation.
  /// @param pageNum Page number to swap in.
  /// @param region Region of the page to swap in.
  /// @returns Computed Auth MAC.
  MaximInterfaceDevices_EXPORT Core::Result<Page::array>
  computeAuthMacWithSwap(AuthenticationData::const_span data, int pageNum,
                         PageRegion region) const;

  /// @brief Compute Authentication MAC with page swapping.
  /// @param data Combined data fields for computation.
  /// @param pageNum Page number to swap in.
  /// @param region Region of the page to swap in.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  computeAndTransmitAuthMacWithSwap(AuthenticationData::const_span data,
                                    int pageNum, PageRegion region);

  MaximInterfaceDevices_EXPORT static const Core::error_category &
  errorCategory();

private:
  const Core::Sleep * sleep;
  Core::I2CMaster * master;
  uint_least8_t address_;
  Config curConfig;

  /// @brief Performs a soft reset on the DS2465.
  /// @note This is not a 1-Wire Reset.
  Core::Result<void> resetDevice();

  /// @brief
  /// Polls the DS2465 status waiting for the 1-Wire Busy bit (1WB) to be cleared.
  /// @returns Status byte or TimeoutError if poll limit reached.
  Core::Result<uint_least8_t> pollBusy() const;

  /// @brief Ensure that the desired 1-Wire level is set in the configuration.
  /// @param level Desired 1-Wire level.
  Core::Result<void> configureLevel(Level level);

  /// @note Const since only for internal use.
  Core::Result<void> writeMemory(uint_least8_t addr,
                                 Core::span<const uint_least8_t> buf) const;

  /// @brief Read memory from the DS2465.
  /// @param addr Address to begin reading from.
  /// @param[out] buf Buffer to hold read data.
  Core::Result<void> readMemory(uint_least8_t addr,
                                Core::span<uint_least8_t> buf) const;

  /// @brief Read memory from the DS2465 at the current pointer.
  /// @param[out] buf Buffer to hold read data.
  Core::Result<void> readMemory(Core::span<uint_least8_t> buf) const;

  /// Write the last computed MAC to the 1-Wire bus.
  Core::Result<void> writeMacBlock();

  Core::Result<void> doComputeWriteMac(WriteMacData::const_span data) const;

  Core::Result<void> doComputeWriteMacWithSwap(WriteMacData::const_span data,
                                               int pageNum,
                                               int segmentNum) const;

  Core::Result<void>
  doComputeAuthMac(AuthenticationData::const_span data) const;

  Core::Result<void>
  doComputeAuthMacWithSwap(AuthenticationData::const_span data, int pageNum,
                           PageRegion region) const;

  // Legacy implementations
  Core::Result<void> copyScratchpad(bool destSecret, int pageNum, bool notFull,
                                    int segmentNum);

  Core::Result<void> computeNextMasterSecret(bool swap, int pageNum,
                                             PageRegion region);

  Core::Result<void> computeWriteMac(bool regwrite, bool swap, int pageNum,
                                     int segmentNum) const;

  Core::Result<void> computeSlaveSecret(bool swap, int pageNum,
                                        PageRegion region);

  Core::Result<void> computeAuthMac(bool swap, int pageNum,
                                    PageRegion region) const;
};

} // namespace MaximInterfaceDevices
namespace MaximInterfaceCore {

template <>
struct is_error_code_enum<MaximInterfaceDevices::DS2465::ErrorValue>
    : true_type {};

} // namespace MaximInterfaceCore
namespace MaximInterfaceDevices {

inline Core::error_code make_error_code(DS2465::ErrorValue e) {
  return Core::error_code(e, DS2465::errorCategory());
}

} // namespace MaximInterfaceDevices

#endif
