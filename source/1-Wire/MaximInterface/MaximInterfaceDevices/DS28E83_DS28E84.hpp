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

#ifndef MaximInterfaceDevices_DS28E83_DS28E84_hpp
#define MaximInterfaceDevices_DS28E83_DS28E84_hpp

#include <stdint.h>
#include <utility>
#include <MaximInterfaceCore/Algorithm.hpp>
#include <MaximInterfaceCore/array_span.hpp>
#include <MaximInterfaceCore/Ecc256.hpp>
#include <MaximInterfaceCore/FlagSet.hpp>
#include <MaximInterfaceCore/ManId.hpp>
#include <MaximInterfaceCore/Optional.hpp>
#include <MaximInterfaceCore/RomId.hpp>
#include <MaximInterfaceCore/RunCommand.hpp>
#include <MaximInterfaceCore/system_error.hpp>
#include "Config.hpp"

namespace MaximInterfaceDevices {

class DS28E83_DS28E84 {
public:
  /// Device command results.
  enum ErrorValue {
    InternalError = 0x22,
    InvalidSequenceError = 0x33,
    InvalidOperationError = 0x55,
    InvalidParameterError = 0x77,
    DeviceDisabledError = 0x88,
    AuthenticationError = 0x100,
    InvalidResponseError ///< Command response does not match expected format.
  };

  /// @name Device memory pages
  /// @{

  static const int publicKeyAxPage = 28;
  static const int publicKeyAyPage = 29;
  static const int publicKeyBxPage = 30;
  static const int publicKeyByPage = 31;
  static const int authorityPublicKeyAxPage = 32;
  static const int authorityPublicKeyAyPage = 33;
  static const int authorityPublicKeyBxPage = 34;
  static const int authorityPublicKeyByPage = 35;
  static const int privateKeyAPage = 36;
  static const int privateKeyBPage = 37;
  static const int secretAPage = 38;
  static const int secretBPage = 39;
  static const int romOptionsPage = 40;
  static const int gpioControlPage = 41;
  static const int publicKeySxPage = 42;
  static const int publicKeySyPage = 43;

  /// @}

  /// Key or secret to use for operation.
  enum KeySecret { KeySecretA = 0, KeySecretB = 1, KeySecretS = 2 };

  /// Available PIO states when verifying an ECDSA signature.
  enum GpioState { Unchanged, Conducting, HighImpedance };

  /// Holds a device memory page.
  typedef Core::array_span<uint_least8_t, 32> Page;

  /// Challenge for an encrypted device memory page.
  typedef Core::array_span<uint_least8_t, 8> EncryptionChallenge;

  // Format page authentication input data.
  class PageAuthenticationData;

  // Format authenticated write input data.
  class WriteAuthenticationData;

  // Format compute secret input data.
  class ComputeSecretData;

  // Format decryption HMAC input data.
  class DecryptionHmacData;

  // Format encryption HMAC input data.
  class EncryptionHmacData;

  // Access fields in the ROM Options page.
  class RomOptions;

  // Access fields in the GPIO Control page.
  class GpioControl;

  /// Page protection types.
  enum BlockProtectionType {
    RP = 0x01,  ///< Read protection.
    WP = 0x02,  ///< Write protection.
    EM = 0x04,  ///< EPROM emulation mode.
    APH = 0x08, ///< Authentication Write Protection HMAC
    EPH = 0x10, ///< Encryption and Authentication Write Protection HMAC
    ECH = 0x40, ///< Encryption and write using shared key from ECDH
    ECW = 0x80  ///< Authentication Write Protection ECDSA
  };
  typedef Core::FlagSet<BlockProtectionType, 8> BlockProtection;

protected:
  explicit DS28E83_DS28E84(const Core::RunCommand & runCommand)
      : doRunCommand(runCommand) {}

  ~DS28E83_DS28E84() {}

public:
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

  /// @brief Read memory with encryption.
  /// @param pageNum Number of page to read from.
  /// @param secret Secret to use for encryption.
  /// @returns Encryption challenge and encrypted page data that was read.
  MaximInterfaceDevices_EXPORT
      Core::Result<std::pair<EncryptionChallenge::array, Page::array> >
      encryptedReadMemory(int pageNum, KeySecret secret) const;

  /// @brief Read the protection settings of a block.
  /// @param blockNum Number of block to read.
  /// @returns Secret/Key and protection set on the block.
  MaximInterfaceDevices_EXPORT
      Core::Result<std::pair<Core::Optional<KeySecret>, BlockProtection> >
      readBlockProtection(int blockNum) const;

  /// @brief Set the protection settings of a block.
  /// @param blockNum Number of block to write.
  /// @param keySecret Secret/Key A or B.
  /// @param protection Protection to write.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  setBlockProtection(int blockNum, KeySecret keySecret,
                     const BlockProtection & protection);

  /// @brief Compute and read page authentication with ECDSA.
  /// @param pageNum Number of page to authenticate.
  /// @param key
  /// Private key to use for authentication.
  /// Key S cannot be used with this command.
  /// @param challenge Random challenge used to prevent replay attacks.
  /// @returns Computed page signature.
  MaximInterfaceDevices_EXPORT Core::Result<Core::Ecc256::Signature::array>
  computeAndReadEcdsaPageAuthentication(int pageNum, KeySecret key,
                                        Page::const_span challenge) const;

  /// @brief Compute and read page authentication with HMAC.
  /// @param pageNum Number of page to authenticate.
  /// @param secret
  /// Secret to use for authentication.
  /// Secret S cannot be used with this command.
  /// @param challenge Random challenge used to prevent replay attacks.
  /// @returns Computed page HMAC.
  MaximInterfaceDevices_EXPORT Core::Result<Page::array>
  computeAndReadSha256PageAuthentication(int pageNum, KeySecret secret,
                                         Page::const_span challenge) const;

  /// @brief Compute a hash over multiple blocks.
  /// @param firstBlock True if this is the first block being hashed.
  /// @param lastBlock True if this is the last block being hashed.
  /// @param data
  /// Data block to hash. Should be 64 bytes unless this is the last block.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  computeMultiblockHash(bool firstBlock, bool lastBlock,
                        Core::span<const uint_least8_t> data);

  /// @brief Verify ECDSA signature with data input.
  /// @param key Public key to use for verification.
  /// @param authorityKey Use the authority key instead of the standard key.
  /// @param gpioState New state of the GPIO pin if verification successful.
  /// @param signature Signature to verify.
  /// @param data Data to verify with length from 1 to 64.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  verifyEcdsaSignature(KeySecret key, bool authorityKey, GpioState gpioState,
                       Core::Ecc256::Signature::const_span signature,
                       Core::span<const uint_least8_t> data);

  /// @brief Verify ECDSA signature with hash input.
  /// @param key Public key to use for verification.
  /// @param authorityKey Use the authority key instead of the standard key.
  /// @param gpioState New state of the GPIO pin if verification successful.
  /// @param signature Signature to verify.
  /// @param hash Hash of data to verify.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  verifyEcdsaSignature(KeySecret key, bool authorityKey, GpioState gpioState,
                       Core::Ecc256::Signature::const_span signature,
                       Page::const_span hash);

  /// @brief
  /// Verify ECDSA signature with THASH input from Compute Multiblock Hash.
  /// @param key Public key to use for verification.
  /// @param authorityKey Use the authority key instead of the standard key.
  /// @param gpioState New state of the GPIO pin if verification successful.
  /// @param signature Signature to verify.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  verifyEcdsaSignature(KeySecret key, bool authorityKey, GpioState gpioState,
                       Core::Ecc256::Signature::const_span signature);

  /// @brief Authenticate a public key for authenticated writes.
  /// @param key
  /// Authority key to use for authentication. Key A or B can be selected.
  /// @param cert Certificate to use for authentication of Public Key S.
  /// @param certCustomization
  /// Certificate customization with length from 1 to 32.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  authenticateEcdsaPublicKey(KeySecret key,
                             Core::Ecc256::Signature::const_span cert,
                             Core::span<const uint_least8_t> certCustomization);

  /// @brief
  /// Authenticate a public key for ECDH and optionally authenticated writes.
  /// @param key
  /// Keys to use for authentication and ECDH key exchange.
  /// Key A or B can be selected.
  /// @param authWrites True to select authentication for writes.
  /// @param cert Certificate to use for authentication of Public Key S.
  /// @param certCustomization
  /// Certificate customization with length from 1 to 32.
  /// @param ecdhCustomization ECDH customization with length from 1 to 48.
  /// @note The maximum total customization length is 60 bytes.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  authenticateEcdsaPublicKey(KeySecret key, bool authWrites,
                             Core::Ecc256::Signature::const_span cert,
                             Core::span<const uint_least8_t> certCustomization,
                             Core::span<const uint_least8_t> ecdhCustomization);

  /// @brief Write with ECDSA authentication.
  /// @param pageNum Number of page to write.
  /// @param useKeyS
  /// Use Public Key S instead of the authority key set in the block protection.
  /// @param newPageData Data to write.
  /// @param signature Signature to use for authentication of page data.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  authenticatedEcdsaWriteMemory(int pageNum, bool useKeyS,
                                Page::const_span newPageData,
                                Core::Ecc256::Signature::const_span signature);

  /// @brief Write with ECDSA authentication and encryption.
  /// @param pageNum Number of page to write.
  /// @param useKeyS
  /// Use Public Key S instead of the authority key set in the block protection.
  /// @param newPageData Encrypted data to write.
  /// @param signature Signature to use for authentication of page data.
  /// @param challenge Challenge to use for decryption of page data.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  authenticatedEcdsaWriteMemory(int pageNum, bool useKeyS,
                                Page::const_span newPageData,
                                Core::Ecc256::Signature::const_span signature,
                                EncryptionChallenge::const_span challenge);

  /// @brief Write with SHA-256 HMAC authentication.
  /// @param pageNum Number of page to write.
  /// @param useSecretS
  /// Use Secret S instead of the secret set in the block protection.
  /// @param newPageData Data to write.
  /// @param hmac HMAC to use for authentication of page data.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  authenticatedSha256WriteMemory(int pageNum, bool useSecretS,
                                 Page::const_span newPageData,
                                 Page::const_span hmac);

  /// @brief Write with SHA-256 HMAC authentication and encryption.
  /// @param pageNum Number of page to write.
  /// @param useSecretS
  /// Use Secret S instead of the secret set in the block protection.
  /// @param newPageData Data to write.
  /// @param hmac HMAC to use for authentication of page data.
  /// @param challenge Challenge to use for decryption of page data.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  authenticatedSha256WriteMemory(int pageNum, bool useSecretS,
                                 Page::const_span newPageData,
                                 Page::const_span hmac,
                                 EncryptionChallenge::const_span challenge);

  /// @brief Compute a derivative SHA-256 secret from an existing secret.
  /// @param pageNum Number of page to use in computation.
  /// @param masterSecret Master secret to use in computation.
  /// @param destinationSecret
  /// Destination secret to receive the computation result.
  /// @param partialSecret Partial secret to use in computation.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  computeAndWriteSha256Secret(int pageNum, KeySecret masterSecret,
                              KeySecret destinationSecret,
                              Page::const_span partialSecret);

  /// @brief Generate a new ECDSA key pair.
  /// @param key Key to generate. Key S cannot be used with this command.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  generateEcc256KeyPair(KeySecret key);

  /// @brief Read a block of random data from the RNG.
  /// @param[out] data Random data from RNG with length from 1 to 64.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  readRng(Core::span<uint_least8_t> data) const;

  /// Run entropy health test on the RNG.
  MaximInterfaceDevices_EXPORT Core::Result<void> entropyHealthTest() const;

  MaximInterfaceDevices_EXPORT static const Core::error_category &
  errorCategory();

protected:
  MaximInterfaceDevices_EXPORT Core::Result<Core::span<uint_least8_t> >
  runCommand(Core::span<const uint_least8_t> request, int delayTime,
             Core::span<uint_least8_t> response) const;

  MaximInterfaceDevices_EXPORT Core::Result<void>
  runCommand(Core::span<const uint_least8_t> request, int delayTime) const;

private:
  enum HashType { HashInput, DataInput, THASH };

  Core::Result<void>
  verifyEcdsaSignature(KeySecret key, bool authorityKey, HashType hashType,
                       GpioState gpioState,
                       Core::Ecc256::Signature::const_span signature,
                       Core::span<const uint_least8_t> buffer);

  Core::Result<void> authenticateEcdsaPublicKey(
      KeySecret key, bool authWrites, Core::Ecc256::Signature::const_span cert,
      Core::span<const uint_least8_t> certCustomization,
      const Core::span<const uint_least8_t> * ecdhCustomization);

  Core::Result<void> authenticatedEcdsaWriteMemory(
      int pageNum, bool useKeyS, Page::const_span newPageData,
      Core::Ecc256::Signature::const_span signature,
      const EncryptionChallenge::const_span * challenge);

  Core::Result<void> authenticatedSha256WriteMemory(
      int pageNum, bool useSecretS, Page::const_span newPageData,
      Page::const_span hmac, const EncryptionChallenge::const_span * challenge);

  Core::RunCommand doRunCommand;
};

} // namespace MaximInterfaceDevices
namespace MaximInterfaceCore {

template <>
struct is_error_code_enum<MaximInterfaceDevices::DS28E83_DS28E84::ErrorValue>
    : true_type {};

} // namespace MaximInterfaceCore
namespace MaximInterfaceDevices {

inline Core::error_code make_error_code(DS28E83_DS28E84::ErrorValue e) {
  return Core::error_code(e, DS28E83_DS28E84::errorCategory());
}

class DS28E83 : public DS28E83_DS28E84 {
public:
  static const int memoryPages = 44;
  static const int protectionBlocks = 9;

  explicit DS28E83(const Core::RunCommand & runCommand)
      : DS28E83_DS28E84(runCommand) {}
};

class DS28E84 : public DS28E83_DS28E84 {
public:
  /// @name Device memory pages
  /// @{

  static const int publicKeySxBackupPage = 104;
  static const int publicKeySyBackupPage = 105;
  static const int decrementCounterPage = 106;

  /// @}

  static const int memoryPages = 107;
  static const int protectionBlocks = 24;

  enum StateOperation { Restore, Backup };

  explicit DS28E84(const Core::RunCommand & runCommand)
      : DS28E83_DS28E84(runCommand) {}

  /// Decrement the decrement-only counter.
  MaximInterfaceDevices_EXPORT Core::Result<void> decrementCounter();

  /// Back up or restore the state of the device to non-volatile memory.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  deviceStateControl(StateOperation operation);
};

/// @brief
/// Hash arbitrary length data with successive Compute Multiblock Hash commands.
/// @param device Device for computation.
/// @param data Data to hash.
MaximInterfaceDevices_EXPORT Core::Result<void>
computeMultiblockHash(DS28E83_DS28E84 & device,
                      Core::span<const uint_least8_t> data);

/// Format page authentication input data.
class DS28E83_DS28E84::PageAuthenticationData {
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

/// Format authenticated write input data.
class DS28E83_DS28E84::WriteAuthenticationData {
public:
  typedef PageAuthenticationData::Result Result;

  WriteAuthenticationData() : data() { setPageNum(0); }

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
  WriteAuthenticationData & setRomId(Core::RomId::const_span romId) {
    data.setRomId(romId);
    return *this;
  }

  /// Set ROM ID for use in anonymous mode.
  WriteAuthenticationData & setAnonymousRomId() {
    data.setAnonymousRomId();
    return *this;
  }

  /// @}

  /// @name Old page
  /// @brief Existing data contained in the page.
  /// @{

  /// Get mutable old page.
  Page::span oldPage() { return data.page(); }

  /// Get immutable old page.
  Page::const_span oldPage() const { return data.page(); }

  /// Set old page.
  WriteAuthenticationData & setOldPage(Page::const_span oldPage) {
    data.setPage(oldPage);
    return *this;
  }

  /// @}

  /// @name New page
  /// @brief New data to write to the page.
  /// @{

  /// Get mutable new page.
  Page::span newPage() { return data.challenge(); }

  /// Get immutable new page.
  Page::const_span newPage() const { return data.challenge(); }

  /// Set new page.
  WriteAuthenticationData & setNewPage(Page::const_span newPage) {
    data.setChallenge(newPage);
    return *this;
  }

  /// @}

  /// @name Page number
  /// @brief Page number for write operation.
  /// @{

  /// Get page number.
  int pageNum() const { return data.pageNum() & 0x7F; }

  /// Set page number.
  WriteAuthenticationData & setPageNum(int pageNum) {
    data.setPageNum(pageNum | 0x80);
    return *this;
  }

  /// @}

  /// @name MAN ID
  /// @brief Manufacturer ID of the device.
  /// @{

  /// Get mutable MAN ID.
  Core::ManId::span manId() { return data.manId(); }

  /// Get immutable MAN ID.
  Core::ManId::const_span manId() const { return data.manId(); }

  /// Set MAN ID.
  WriteAuthenticationData & setManId(Core::ManId::const_span manId) {
    data.setManId(manId);
    return *this;
  }

  /// @}

private:
  PageAuthenticationData data;
};

/// Format compute secret input data.
class DS28E83_DS28E84::ComputeSecretData {
public:
  typedef PageAuthenticationData::Result Result;

  MaximInterfaceDevices_EXPORT ComputeSecretData();

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

  /// @name Partial Secret
  /// @brief Partial Secret used for customization.
  /// @{

  /// Get mutable Partial Secret.
  Page::span partialSecret() { return data.challenge(); }

  /// Get immutable Partial Secret.
  Page::const_span partialSecret() const { return data.challenge(); }

  /// Set Partial Secret.
  ComputeSecretData & setPartialSecret(Page::const_span partialSecret) {
    data.setChallenge(partialSecret);
    return *this;
  }

  /// @}

  /// @name Page number
  /// @brief Page number for Binding Data.
  /// @{

  /// Get page number.
  int pageNum() const { return data.pageNum() & 0x3F; }

  /// Set page number.
  ComputeSecretData & setPageNum(int pageNum) {
    data.setPageNum(pageNum | 0xC0);
    return *this;
  }

  /// @}

  /// @name MAN ID
  /// @brief Manufacturer ID of the device.
  /// @{

  /// Get immutable MAN ID.
  Core::ManId::const_span manId() const { return data.manId(); }

  /// Set MAN ID.
  MaximInterfaceDevices_EXPORT ComputeSecretData &
  setManId(Core::ManId::const_span manId);

  /// @}

private:
  PageAuthenticationData data;
};

/// Format decryption HMAC input data.
class DS28E83_DS28E84::DecryptionHmacData {
public:
  typedef Core::array_span<uint_least8_t, EncryptionChallenge::size +
                                              Core::RomId::size + 1 +
                                              Core::ManId::size>
      Result;

  DecryptionHmacData() : result_() {}

  /// Formatted data result.
  Result::const_span result() const { return result_; }

  /// @name Encryption Challenge
  /// @brief Random challenge used to prevent replay attacks.
  /// @{

  /// Get mutable Encryption Challenge.
  EncryptionChallenge::span encryptionChallenge() {
    return make_span(result_)
        .subspan<encryptionChallengeIdx, EncryptionChallenge::size>();
  }

  /// Get immutable Encryption Challenge.
  EncryptionChallenge::const_span encryptionChallenge() const {
    return const_cast<DecryptionHmacData &>(*this).encryptionChallenge();
  }

  /// Set Encryption Challenge.
  DecryptionHmacData &
  setEncryptionChallenge(EncryptionChallenge::const_span encryptionChallenge) {
    copy(encryptionChallenge, this->encryptionChallenge());
    return *this;
  }

  /// @}

  /// @name ROM ID
  /// @brief 1-Wire ROM ID of the device.
  /// @{

  /// Get mutable ROM ID.
  Core::RomId::span romId() {
    return make_span(result_).subspan<romIdIdx, Core::RomId::size>();
  }

  /// Get immutable ROM ID.
  Core::RomId::const_span romId() const {
    return const_cast<DecryptionHmacData &>(*this).romId();
  }

  /// Set ROM ID.
  DecryptionHmacData & setRomId(Core::RomId::const_span romId) {
    copy(romId, this->romId());
    return *this;
  }

  /// Set ROM ID for use in anonymous mode.
  MaximInterfaceDevices_EXPORT DecryptionHmacData & setAnonymousRomId();

  /// @}

  /// @name Page number
  /// @brief Number of the page to use data from.
  /// @{

  /// Get page number.
  int pageNum() const { return result_[pageNumIdx]; }

  /// Set page number.
  DecryptionHmacData & setPageNum(int pageNum) {
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
    return const_cast<DecryptionHmacData &>(*this).manId();
  }

  /// Set MAN ID.
  DecryptionHmacData & setManId(Core::ManId::const_span manId) {
    copy(manId, this->manId());
    return *this;
  }

  /// @}

private:
  static const size_t encryptionChallengeIdx = 0;
  static const size_t romIdIdx =
      encryptionChallengeIdx + EncryptionChallenge::size;
  static const size_t pageNumIdx = romIdIdx + Core::RomId::size;
  static const size_t manIdIdx = pageNumIdx + 1;

  Result::array result_;
};

/// Format encryption HMAC input data.
class DS28E83_DS28E84::EncryptionHmacData {
public:
  typedef DecryptionHmacData::Result Result;

  EncryptionHmacData() : data() { setPageNum(0); }

  /// Formatted data result.
  Result::const_span result() const { return data.result(); }

  /// @name Encryption Challenge
  /// @brief Random challenge used to prevent replay attacks.
  /// @{

  /// Get mutable Encryption Challenge.
  EncryptionChallenge::span encryptionChallenge() {
    return data.encryptionChallenge();
  }

  /// Get immutable Encryption Challenge.
  EncryptionChallenge::const_span encryptionChallenge() const {
    return data.encryptionChallenge();
  }

  /// Set Encryption Challenge.
  EncryptionHmacData &
  setEncryptionChallenge(EncryptionChallenge::const_span encryptionChallenge) {
    data.setEncryptionChallenge(encryptionChallenge);
    return *this;
  }

  /// @}

  /// @name ROM ID
  /// @brief 1-Wire ROM ID of the device.
  /// @{

  /// Get mutable ROM ID.
  Core::RomId::span romId() { return data.romId(); }

  /// Get immutable ROM ID.
  Core::RomId::const_span romId() const { return data.romId(); }

  /// Set ROM ID.
  EncryptionHmacData & setRomId(Core::RomId::const_span romId) {
    data.setRomId(romId);
    return *this;
  }

  /// Set ROM ID for use in anonymous mode.
  EncryptionHmacData & setAnonymousRomId() {
    data.setAnonymousRomId();
    return *this;
  }

  /// @}

  /// @name Page number
  /// @brief Number of the page to use data from.
  /// @{

  /// Get page number.
  int pageNum() const { return data.pageNum() & 0x7F; }

  /// Set page number.
  EncryptionHmacData & setPageNum(int pageNum) {
    data.setPageNum(pageNum | 0x80);
    return *this;
  }

  /// @}

  /// @name MAN ID
  /// @brief Manufacturer ID of the device.
  /// @{

  /// Get mutable MAN ID.
  Core::ManId::span manId() { return data.manId(); }

  /// Get immutable MAN ID.
  Core::ManId::const_span manId() const { return data.manId(); }

  /// Set MAN ID.
  EncryptionHmacData & setManId(Core::ManId::const_span manId) {
    data.setManId(manId);
    return *this;
  }

  /// @}

private:
  DecryptionHmacData data;
};

/// Access fields in the ROM Options page.
class DS28E83_DS28E84::RomOptions {
public:
  explicit RomOptions(Page::span page) : page(page) {}

  bool anonymous() const { return page[anonymousIdx] == anonymousValue; }

  void setAnonymous(bool anonymous) {
    page[anonymousIdx] = (anonymous ? anonymousValue : 0);
  }

  Core::ManId::const_span manId() const {
    return page.subspan<22, Core::ManId::size>();
  }

  Core::RomId::const_span romId() const {
    return page.subspan<24, Core::RomId::size>();
  }

private:
  static const Page::span::index_type anonymousIdx = 1;
  static const Page::span::value_type anonymousValue = 0xAA;

  Page::span page;
};

/// Access fields in the GPIO Control page.
class DS28E83_DS28E84::GpioControl {
public:
  explicit GpioControl(Page::span page) : page(page) {}

  bool conducting() const { return page[conductingIdx] == conductingValue; }

  void setConducting(bool conducting) {
    page[conductingIdx] = (conducting ? conductingValue : 0x55);
  }

  bool level() const { return page[2] == 0x55; }

private:
  static const Page::span::index_type conductingIdx = 0;
  static const Page::span::value_type conductingValue = 0xAA;

  Page::span page;
};

} // namespace MaximInterfaceDevices

#endif
