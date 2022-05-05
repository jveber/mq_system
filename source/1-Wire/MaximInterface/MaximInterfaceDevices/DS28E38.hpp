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

#ifndef MaximInterfaceDevices_DS28E38_hpp
#define MaximInterfaceDevices_DS28E38_hpp

#include <stdint.h>
#include <MaximInterfaceCore/Algorithm.hpp>
#include <MaximInterfaceCore/array_span.hpp>
#include <MaximInterfaceCore/Ecc256.hpp>
#include <MaximInterfaceCore/FlagSet.hpp>
#include <MaximInterfaceCore/ManId.hpp>
#include <MaximInterfaceCore/RomId.hpp>
#include <MaximInterfaceCore/RunCommand.hpp>
#include <MaximInterfaceCore/system_error.hpp>
#include "Config.hpp"

namespace MaximInterfaceDevices {

class DS28E38 {
public:
  /// Device command results.
  enum ErrorValue {
    InvalidOperationError = 0x55,
    InvalidParameterError = 0x77,
    InvalidSequenceError = 0x33,
    InternalError = 0x22,
    DeviceDisabledError = 0x88,
    InvalidResponseError =
        0x100 ///< Command response does not match expected format.
  };

  /// @name Device memory pages
  /// @{

  static const int decrementCounterPage = 3;
  static const int publicKeyXPage = 4;
  static const int publicKeyYPage = 5;
  static const int privateKeyPage = 6;

  /// @}

  static const int memoryPages = 7;

  /// Holds a device memory page.
  typedef Core::array_span<uint_least8_t, 32> Page;

  // Format page authentication input data.
  class PageAuthenticationData;

  /// Page protection types.
  enum PageProtectionType {
    RP = 0x01, ///< Read protection.
    WP = 0x02, ///< Write protection.
    EM = 0x04, ///< EPROM emulation mode.
    DC = 0x08, ///< Decrement counter.
    PF = 0x10  ///< PUF used as private key.
  };
  typedef Core::FlagSet<PageProtectionType, 5> PageProtection;

  struct Status {
    enum EntropyHealthTestStatus {
      TestNotPerformed = 0xFF,
      EntropyHealthy = 0xAA,
      EntropyNotHealthy = 0xDD
    };

    typedef Core::array_span<PageProtection, memoryPages> PageProtectionList;
    typedef Core::array_span<uint_least8_t, 2> RomVersion;

    PageProtectionList::array pageProtection;
    Core::ManId::array manId;
    RomVersion::array romVersion;
    EntropyHealthTestStatus entropyHealthTestStatus;
  };

  explicit DS28E38(const Core::RunCommand & runCommand)
      : doRunCommand(runCommand) {}

  void setRunCommand(const Core::RunCommand & runCommand) {
    doRunCommand = runCommand;
  }

  /// @brief Write memory with no protection.
  /// @param pageNum Number of page to write.
  /// @param page Data to write.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeMemory(int pageNum, Page::const_span page);

  /// @brief Read memory with no protection.
  /// @param pageNum Number of page to read.
  /// @returns Data that was read.
  MaximInterfaceDevices_EXPORT Core::Result<Page::array>
  readMemory(int pageNum) const;

  /// @brief
  /// Reads the current status of the device and optionally performs an
  /// entropy health test.
  /// @param entropyHealthTest True to perform an entropy health test.
  /// @returns status Status that was read.
  MaximInterfaceDevices_EXPORT Core::Result<Status>
  readStatus(bool entropyHealthTest) const;

  /// @brief Set the protection settings of a page.
  /// @param pageNum Number of page to write.
  /// @param protection Protection to write.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  setPageProtection(int pageNum, const PageProtection & protection);

  /// @brief Compute and read page authentication with ECDSA.
  /// @param pageNum Number of page to authenticate.
  /// @param anonymous True to disable use of ROM ID in computation.
  /// @param challenge Random challenge used to prevent replay attacks.
  /// @returns Computed page signature.
  MaximInterfaceDevices_EXPORT Core::Result<Core::Ecc256::Signature::array>
  computeAndReadPageAuthentication(int pageNum, bool anonymous,
                                   Page::const_span challenge) const;

  /// Decrement the decrement-only counter.
  MaximInterfaceDevices_EXPORT Core::Result<void> decrementCounter();

  /// Permanently disable the device.
  MaximInterfaceDevices_EXPORT Core::Result<void> disableDevice();

  /// @brief Generate a new ECDSA public key from an existing private key.
  /// @param privateKeyPuf True if PUF is used as the private key.
  /// @param writeProtectEnable True to lock the key against further writes.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  generateEcc256KeyPair(bool privateKeyPuf, bool writeProtectEnable);

  /// @brief Read a block of random data from the RNG.
  /// @param[out] data Random data from RNG with length from 1 to 64.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  readRng(Core::span<uint_least8_t> data) const;

  MaximInterfaceDevices_EXPORT static const Core::error_category &
  errorCategory();

protected:
  MaximInterfaceDevices_EXPORT Core::Result<Core::span<uint_least8_t> >
  runCommand(Core::span<const uint_least8_t> request, int delayTime,
             Core::span<uint_least8_t> response) const;

  MaximInterfaceDevices_EXPORT Core::Result<void>
  runCommand(Core::span<const uint_least8_t> request, int delayTime);

private:
  Core::RunCommand doRunCommand;
};

} // namespace MaximInterfaceDevices
namespace MaximInterfaceCore {

template <>
struct is_error_code_enum<MaximInterfaceDevices::DS28E38::ErrorValue>
    : true_type {};

} // namespace MaximInterfaceCore
namespace MaximInterfaceDevices {

inline Core::error_code make_error_code(DS28E38::ErrorValue e) {
  return Core::error_code(e, DS28E38::errorCategory());
}

/// Format page authentication input data.
class DS28E38::PageAuthenticationData {
public:
  typedef Core::array_span<uint_least8_t, Core::RomId::size + 2 * Page::size +
                                              1 + Core::ManId::size>
      Result;

  PageAuthenticationData() : result_() {}

  /// Formatted data result.
  Result::const_span result() const { return result_; }

  /// @name ROM ID
  /// @brief 1-Wire ROM ID of the device.
  /// @{

  /// Get mutable ROM ID.
  Core::RomId::span romId() {
    return make_span(result_).subspan<romIdIdx, Core::RomId::size>();
  }

  /// Get immutable ROM ID.
  Core::RomId::const_span romId() const {
    return const_cast<PageAuthenticationData &>(*this).romId();
  }

  /// Set ROM ID.
  PageAuthenticationData & setRomId(Core::RomId::const_span romId) {
    copy(romId, this->romId());
    return *this;
  }

  /// Set ROM ID for use in anonymous mode.
  MaximInterfaceDevices_EXPORT PageAuthenticationData & setAnonymousRomId();

  /// @}

  /// @name Page
  /// @brief Data from a device memory page.
  /// @{

  /// Get mutable page.
  Page::span page() {
    return make_span(result_).subspan<pageIdx, Page::size>();
  }

  /// Get immutable page.
  Page::const_span page() const {
    return const_cast<PageAuthenticationData &>(*this).page();
  }

  /// Set page.
  PageAuthenticationData & setPage(Page::const_span page) {
    copy(page, this->page());
    return *this;
  }

  /// @}

  /// @name Challenge
  /// @brief Random challenge used to prevent replay attacks.
  /// @{

  /// Get mutable Challenge.
  Page::span challenge() {
    return make_span(result_).subspan<challengeIdx, Page::size>();
  }

  /// Get immutable Challenge.
  Page::const_span challenge() const {
    return const_cast<PageAuthenticationData &>(*this).challenge();
  }

  /// Set Challenge.
  PageAuthenticationData & setChallenge(Page::const_span challenge) {
    copy(challenge, this->challenge());
    return *this;
  }

  /// @}

  /// @name Page number
  /// @brief Number of the page to use data from.
  /// @{

  /// Get page number.
  int pageNum() const { return result_[pageNumIdx]; }

  /// Set page number.
  PageAuthenticationData & setPageNum(int pageNum) {
    result_[pageNumIdx] = pageNum;
    return *this;
  }

  /// @}

  /// @name MAN ID
  /// @brief Manufacturer ID of the device.
  /// @{

  /// Get mutable MAN ID.
  Core::ManId::span manId() {
    return make_span(result_).subspan<manIdIdx, Core::ManId::size>();
  }

  /// Get immutable MAN ID.
  Core::ManId::const_span manId() const {
    return const_cast<PageAuthenticationData &>(*this).manId();
  }

  /// Set MAN ID.
  PageAuthenticationData & setManId(Core::ManId::const_span manId) {
    copy(manId, this->manId());
    return *this;
  }

  /// @}

private:
  static const size_t romIdIdx = 0;
  static const size_t pageIdx = romIdIdx + Core::RomId::size;
  static const size_t challengeIdx = pageIdx + Page::size;
  static const size_t pageNumIdx = challengeIdx + Page::size;
  static const size_t manIdIdx = pageNumIdx + 1;

  Result::array result_;
};

} // namespace MaximInterfaceDevices

#endif
