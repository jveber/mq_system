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

#ifndef MaximInterfaceDevices_DS28E16_hpp
#define MaximInterfaceDevices_DS28E16_hpp

#include <stdint.h>
#include <MaximInterfaceCore/Algorithm.hpp>
#include <MaximInterfaceCore/array_span.hpp>
#include <MaximInterfaceCore/FlagSet.hpp>
#include <MaximInterfaceCore/RomId.hpp>
#include <MaximInterfaceCore/RunCommand.hpp>
#include <MaximInterfaceCore/system_error.hpp>
#include "Config.hpp"

namespace MaximInterfaceDevices {

class DS28E16 {
public:
  /// Device command results.
  enum ErrorValue {
    InvalidOperationError = 0x55,
    InvalidParameterError = 0x77,
    InvalidSequenceError = 0x33,
    InternalError = 0x22,
    DeviceDisabledError = 0x88,
    AuthenticationError = 0x100,
    InvalidResponseError ///< Command response does not match expected format.
  };

  /// @name Device memory pages
  /// @{

  static const int decrementCounterPage = 2;
  static const int masterSecretPage = 3;

  /// @}

  static const int memoryPages = 4;

  /// Holds a device memory page.
  typedef Core::array_span<uint_least8_t, 16> Page;

  /// Holds a Challenge, Partial Secret, or HMAC.
  typedef Core::array_span<uint_least8_t, 32> DoublePage;

  /// Holds a password used to disable the device.
  typedef Core::array_span<uint_least8_t, 2> DisableDevicePassword;

  // Format page authentication input data.
  class PageAuthenticationData;

  // Format compute secret input data.
  class ComputeSecretData;

  /// Page protection types.
  enum PageProtectionType {
    RP = 0x01, ///< Read protection.
    WP = 0x02, ///< Write protection.
    DC = 0x08  ///< Decrement counter.
  };
  typedef Core::FlagSet<PageProtectionType, 4> PageProtection;

  struct Status {
    typedef Core::array_span<PageProtection, memoryPages> PageProtectionList;

    PageProtectionList::array pageProtection;
    uint_least8_t manId;
    uint_least8_t deviceVersion;
  };

  explicit DS28E16(const Core::RunCommand & runCommand)
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

  /// @brief Reads the current status of the device.
  /// @returns Status that was read.
  MaximInterfaceDevices_EXPORT Core::Result<Status> readStatus() const;

  /// @brief Set the protection settings of a page.
  /// @param pageNum Number of page to write.
  /// @param protection Protection to write.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  setPageProtection(int pageNum, const PageProtection & protection);

  /// @brief Compute and read page authentication with HMAC.
  /// @param pageNum Number of page to authenticate.
  /// @param anonymous True to disable use of ROM ID in computation.
  /// @param challenge Random challenge used to prevent replay attacks.
  /// @returns Computed page HMAC.
  MaximInterfaceDevices_EXPORT Core::Result<DoublePage::array>
  computeAndReadPageAuthentication(int pageNum, bool anonymous,
                                   DoublePage::const_span challenge) const;

  /// Decrement the decrement-only counter.
  MaximInterfaceDevices_EXPORT Core::Result<void> decrementCounter();

  /// Set password that will be subsequently used to disable the device.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  setDisableDevicePassword(DisableDevicePassword::const_span password);

  /// @brief Lock-out all disable functionality for the device.
  /// @note Only allowed prior to setting password.
  MaximInterfaceDevices_EXPORT Core::Result<void> lockOutDisableDevice();

  /// Permanently disable the device.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  disableDevice(DisableDevicePassword::const_span password);

  /// @brief
  /// Compute a derivative secret for authentication from the Master Secret.
  /// @param bindingDataPageNum Page number for Binding Data.
  /// @param constantBindingData
  /// Use constant Binding Data instead of Binding Data from the selected page.
  /// @param anonymous True to disable use of ROM ID in computation.
  /// @param partialSecret Partial secret to use in computation.
  /// @note
  /// This command should be executed prior to the
  /// Compute and Read Page Authentication command.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  computeSecret(int bindingDataPageNum, bool constantBindingData,
                bool anonymous, DoublePage::const_span partialSecret);

  MaximInterfaceDevices_EXPORT static const Core::error_category &
  errorCategory();

protected:
  MaximInterfaceDevices_EXPORT Core::Result<Core::span<uint_least8_t> >
  runCommand(Core::span<const uint_least8_t> request, int delayTime,
             Core::span<uint_least8_t> response) const;

  MaximInterfaceDevices_EXPORT Core::Result<void>
  runCommand(Core::span<const uint_least8_t> request, int delayTime);

private:
  enum DisableDeviceOperation {
    SetDisableDevicePassword = 0x0F,
    LockOutDisableDevice = 0x05,
    DisableDevice = 0x00
  };

  Core::Result<void> disableDevice(DisableDeviceOperation operation,
                                   DisableDevicePassword::const_span password);

  Core::RunCommand doRunCommand;
};

} // namespace MaximInterfaceDevices
namespace MaximInterfaceCore {

template <>
struct is_error_code_enum<MaximInterfaceDevices::DS28E16::ErrorValue>
    : true_type {};

} // namespace MaximInterfaceCore
namespace MaximInterfaceDevices {

inline Core::error_code make_error_code(DS28E16::ErrorValue e) {
  return Core::error_code(e, DS28E16::errorCategory());
}

/// Format page authentication input data.
class DS28E16::PageAuthenticationData {
public:
  typedef Core::array_span<uint_least8_t, Core::RomId::size + 2 * Page::size +
                                              DoublePage::size + 3>
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

  /// @name Challenge.
  /// @brief Random challenge used to prevent replay attacks.
  /// @{

  /// Get mutable Challenge.
  DoublePage::span challenge() {
    return make_span(result_).subspan<challengeIdx, DoublePage::size>();
  }

  /// Get immutable Challenge.
  DoublePage::const_span challenge() const {
    return const_cast<PageAuthenticationData &>(*this).challenge();
  }

  /// Set Challenge.
  PageAuthenticationData & setChallenge(DoublePage::const_span challenge) {
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
  uint_least8_t & manId() { return result_[manIdIdx]; }

  /// Get immutable MAN ID.
  uint_least8_t manId() const {
    return const_cast<PageAuthenticationData &>(*this).manId();
  }

  /// Set MAN ID.
  PageAuthenticationData & setManId(uint_least8_t manId) {
    this->manId() = manId;
    return *this;
  }

  /// @}

private:
  static const size_t romIdIdx = 0;
  static const size_t pageIdx = romIdIdx + Core::RomId::size;
  static const size_t challengeIdx = pageIdx + 2 * Page::size;
  static const size_t pageNumIdx = challengeIdx + DoublePage::size;
  static const size_t manIdIdx = pageNumIdx + 1;

  Result::array result_;
};

/// Format compute secret input data.
class DS28E16::ComputeSecretData {
public:
  typedef PageAuthenticationData::Result Result;

  ComputeSecretData() : data() {
    data.setPageNum(0x80 | constantBindingDataMask);
  }

  /// Formatted data result.
  Result::const_span result() const { return data.result(); }

  /// @name ROM ID
  /// @brief 1-Wire ROM ID of the device.
  /// @{

  /// Get mutable ROM ID.
  Core::RomId::span romId() { return data.romId(); }

  /// Get immutable ROM ID.
  Core::RomId::const_span romId() const { return data.romId(); }

  /// Set ROM ID.
  ComputeSecretData & setRomId(Core::RomId::const_span romId) {
    data.setRomId(romId);
    return *this;
  }

  /// @}

  /// @name Binding Data
  /// @brief Binding Data contained in the selected page.
  /// @{

  /// Get mutable Binding Data.
  Page::span bindingData() { return data.page(); }

  /// Get immutable Binding Data.
  Page::const_span bindingData() const { return data.page(); }

  /// Set Binding Data.
  ComputeSecretData & setBindingData(Page::const_span bindingData) {
    data.setPage(bindingData);
    return *this;
  }

  /// @}

  /// @name Constant Binding Data
  /// @brief
  /// Use constant Binding Data instead of Binding Data from the selected page.
  /// @{

  /// Get Constant Binding Data.
  bool constantBindingData() const {
    return (data.pageNum() & constantBindingDataMask) ==
           constantBindingDataMask;
  }

  /// Set Constant Binding Data.
  MaximInterfaceDevices_EXPORT ComputeSecretData &
  setConstantBindingData(bool constantBindingData);

  /// @}

  /// @name Partial Secret
  /// @brief Partial Secret used for customization.
  /// @{

  /// Get mutable Partial Secret.
  DoublePage::span partialSecret() { return data.challenge(); }

  /// Get immutable Partial Secret.
  DoublePage::const_span partialSecret() const { return data.challenge(); }

  /// Set Partial Secret.
  ComputeSecretData & setPartialSecret(DoublePage::const_span partialSecret) {
    data.setChallenge(partialSecret);
    return *this;
  }

  /// @}

  /// @name Binding Data page number
  /// @{

  /// Get Binding Data page number.
  int bindingDataPageNum() const {
    return data.pageNum() & bindingDataPageNumMask;
  }

  /// Set Binding Data page number.
  ComputeSecretData & setBindingDataPageNum(int bindingDataPageNum) {
    data.setPageNum((bindingDataPageNum & bindingDataPageNumMask) |
                    (data.pageNum() & ~bindingDataPageNumMask));
    return *this;
  }

  /// @}

  /// @name MAN ID
  /// @brief Manufacturer ID of the device.
  /// @{

  /// Get mutable MAN ID.
  uint_least8_t & manId() { return data.manId(); }

  /// Get immutable MAN ID.
  uint_least8_t manId() const { return data.manId(); }

  /// Set MAN ID.
  ComputeSecretData & setManId(uint_least8_t manId) {
    data.setManId(manId);
    return *this;
  }

  /// @}

private:
  static const unsigned int bindingDataPageNumMask = 0x03;
  static const unsigned int constantBindingDataMask = 0x04;

  PageAuthenticationData data;
};

} // namespace MaximInterfaceDevices

#endif
