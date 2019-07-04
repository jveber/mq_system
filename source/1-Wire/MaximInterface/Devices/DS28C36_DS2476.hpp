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

#ifndef MaximInterface_DS28C36_DS2476
#define MaximInterface_DS28C36_DS2476

#include <stddef.h>
#include <stdint.h>
#include <vector>
#include <MaximInterface/Links/I2CMaster.hpp>
#include <MaximInterface/Links/Sleep.hpp>
#include <MaximInterface/Utilities/array.hpp>
#include <MaximInterface/Utilities/Ecc256.hpp>
#include <MaximInterface/Utilities/Export.h>
#include <MaximInterface/Utilities/RomId.hpp>
#include <MaximInterface/Utilities/ManId.hpp>
#include <MaximInterface/Utilities/Sha256.hpp>
#include <MaximInterface/Utilities/system_error.hpp>
#include <MaximInterface/Utilities/FlagSet.hpp>

namespace MaximInterface {

/// Interface to the DS28C36 authenticator.
class DS28C36 {
public:
  /// Device command results.
  enum ErrorValue {
    ProtectionError = 0x55,
    InvalidParameterError = 0x77,
    InvalidSequenceError = 0x33,
    InvalidEcdsaInputOrResultError = 0x22,
    AuthenticationError = 0x100,
    InvalidResponseError = 0x101 ///< Response does not match expected format.
  };

  /// Device memory pages.
  enum PageNum {
    UserData0 = 0,
    UserData1,
    UserData2,
    UserData3,
    UserData4,
    UserData5,
    UserData6,
    UserData7,
    UserData8,
    UserData9,
    UserData10,
    UserData11,
    UserData12,
    UserData13,
    UserData14,
    UserData15,
    PublicKeyAX,
    PublicKeyAY,
    PublicKeyBX,
    PublicKeyBY,
    PublicKeyCX,
    PublicKeyCY,
    PrivateKeyA,
    PrivateKeyB,
    PrivateKeyC,
    SecretA,
    SecretB,
    DecrementCounter,
    RomOptions,
    GpioControl,
    PublicKeySX,
    PublicKeySY
  };

  /// Available keys for ECDSA operations.
  enum KeyNum { KeyNumA = 0, KeyNumB = 1, KeyNumC = 2, KeyNumS = 3 };

  /// Available secrets for HMAC operations.
  enum SecretNum { SecretNumA = 0, SecretNumB = 1, SecretNumS = 2 };

  /// Data hash type when verifying an ECDSA signature.
  enum HashType {
    HashInBuffer = 0, ///< Hash is loaded in the buffer.
    DataInBuffer = 1, ///< Compute hash from data loaded in the buffer.
    THASH = 2         ///< Use THASH from Compute Multiblock Hash command.
  };

  /// Available PIO states when verifying an ECDSA signature.
  enum PioState { Unchanged, Conducting, HighImpedance };

  /// Holds a device memory page.
  typedef array<uint_least8_t, 32> Page;

  /// Holds page authentication input data.
  typedef array<uint_least8_t, 75> PageAuthenticationData;

  /// Page protection types.
  enum PageProtectionType {
    RP = 0x01,   ///< Read protection.
    WP = 0x02,   ///< Write protection.
    EM = 0x04,   ///< EPROM emulation mode.
    APH = 0x08,  ///< Authentication write protection HMAC.
    EPH = 0x10,  ///< Encryption and authenticated write protection HMAC.
    AUTH = 0x20, ///< Public Key C is set to authority public key.
    ECH = 0x40,  ///< Encrypted read and write using shared key from ECDH.
    ECW = 0x80   ///< Authentication write protection ECDSA.
  };
  typedef FlagSet<PageProtectionType, 8> PageProtection;

  /// Holds an encrypted device memory page.
  struct EncryptedPage {
    typedef array<uint_least8_t, 8> Challenge;

    /// Total size of all elements in bytes.
    static const size_t size = Challenge::csize + Page::csize;

    Challenge challenge;
    Page data;
  };

  /// Number of memory pages on the device.
  static const int memoryPages = 32;

  DS28C36(const Sleep & sleep, I2CMaster & master, uint_least8_t address = 0x36)
      : sleep_(&sleep), master(&master), address_(address & 0xFE) {}

  void setSleep(const Sleep & sleep) { this->sleep_ = &sleep; }
  void setMaster(I2CMaster & master) { this->master = &master; }
  uint_least8_t address() const { return address_; }
  void setAddress(uint_least8_t address) { this->address_ = (address & 0xFE); }

  /// Write memory with no protection.
  /// @param pageNum Number of page to write.
  /// @param page Data to write.
  MaximInterface_EXPORT error_code writeMemory(int pageNum, const Page & page);

  /// Read memory with no protection.
  /// @param pageNum Number of page to read.
  /// @param[out] page Data that was read.
  MaximInterface_EXPORT error_code readMemory(int pageNum, Page & page);

  /// Write the temporary buffer.
  /// @param data Data to write.
  /// @param dataSize Size of data to write.
  MaximInterface_EXPORT error_code writeBuffer(const uint_least8_t * data,
                                               size_t dataSize);

  /// Read the temporary buffer.
  /// @param[out] data Data that was read.
  MaximInterface_EXPORT error_code
  readBuffer(std::vector<uint_least8_t> & data);

  /// Read the protection settings of a page.
  /// @param pageNum Number of page to read.
  /// @param[out] protection Protection that was read.
  MaximInterface_EXPORT error_code
  readPageProtection(int pageNum, PageProtection & protection);

  /// Set the protection settings of a page.
  /// @param pageNum Number of page to write.
  /// @param protection Protection to write.
  MaximInterface_EXPORT error_code
  setPageProtection(int pageNum, const PageProtection & protection);

  /// Decrement the decrement-only counter.
  MaximInterface_EXPORT error_code decrementCounter();

  /// Read a block of random data from the RNG.
  /// @param[out] data Buffer to hold random data from RNG.
  /// @param dataSize Number of bytes to read from 1 to 64 and length of data.
  MaximInterface_EXPORT error_code readRng(uint_least8_t * data,
                                           size_t dataSize);

  /// Read memory with encryption.
  /// @param pageNum Number of page to read from.
  /// @param secretNum Secret to use for encryption.
  /// @param[out] page Data that was read.
  MaximInterface_EXPORT error_code encryptedReadMemory(int pageNum,
                                                       SecretNum secretNum,
                                                       EncryptedPage & page);

  /// Compute and read page authentication with ECDSA.
  /// @param pageNum Number of page to authenticate.
  /// @param keyNum Private key to use for authentication.
  /// Key S cannot be used with this command.
  /// @param[out] signature Computed page signature.
  MaximInterface_EXPORT error_code computeAndReadEcdsaPageAuthentication(
      int pageNum, KeyNum keyNum, Ecc256::Signature & signature);

  /// Compute and read page authentication with HMAC.
  /// @param pageNum Number of page to authenticate.
  /// @param secretNum Secret to use for authentication.
  /// @param[out] hmac Computed page HMAC.
  MaximInterface_EXPORT error_code computeAndReadHmacPageAuthentication(
      int pageNum, SecretNum secretNum, Sha256::Hash & hmac);

  /// Write with SHA2 authentication.
  /// @param pageNum Number of page to write.
  /// @param secretNum Secret to use for authentication.
  /// @param page Data to write.
  MaximInterface_EXPORT error_code authenticatedSha2WriteMemory(
      int pageNum, SecretNum secretNum, const Page & page);

  /// Compute SHA2 secret and optionally lock.
  /// @param pageNum Number of page to use in computation.
  /// @param msecretNum Master secret to use in computation.
  /// @param dsecretNum Destination secret to receive the computation result.
  /// @param writeProtectEnable
  /// True to lock the destination secret against further writes.
  MaximInterface_EXPORT error_code
  computeAndLockSha2Secret(int pageNum, SecretNum msecretNum,
                           SecretNum dsecretNum, bool writeProtectEnable);

  /// Generate a new ECDSA key pair.
  /// @param keyNum Key to generate. Key S cannot be used with this command.
  /// @param writeProtectEnable True to lock the key against further writes.
  MaximInterface_EXPORT error_code
  generateEcc256KeyPair(KeyNum keyNum, bool writeProtectEnable);

  /// Compute a hash over multiple blocks.
  /// @param firstBlock True if this is the first block being hashed.
  /// @param lastBlock True if this is the last block being hashed.
  /// @param data Data block to hash.
  /// @param dataSize Size of data to hash.
  /// Should be 64 bytes unless this is the last block.
  MaximInterface_EXPORT error_code
  computeMultiblockHash(bool firstBlock, bool lastBlock,
                        const uint_least8_t * data, size_t dataSize);

  /// Verify ECDSA signature.
  /// @param keyNum Public key to use for verification.
  /// @param hashType Source of the data hash input.
  /// @param signature Signature to verify.
  /// @param pioa New state of PIOA if verification successful.
  /// @param piob New state of PIOB if verification successful.
  MaximInterface_EXPORT error_code verifyEcdsaSignature(
      KeyNum keyNum, HashType hashType, const Ecc256::Signature & signature,
      PioState pioa = Unchanged, PioState piob = Unchanged);

  /// Authenticate a public key for authenticated writes or encrypted reads with ECDH.
  /// @param authWrites True to select authentication for writes.
  /// @param ecdh True to select ECDH key exchange.
  /// @param keyNum Private key to use for ECDH key exchange.
  /// Key A or B can be selected.
  /// @param csOffset Certificate customization field ending offset in buffer.
  /// @param signature Signature to use for authentication of public key S.
  MaximInterface_EXPORT error_code
  authenticateEcdsaPublicKey(bool authWrites, bool ecdh, KeyNum keyNum,
                             int csOffset, const Ecc256::Signature & signature);

  /// Write with ECDSA authentication.
  /// @param pageNum Number of page to write.
  /// @param page Data to write.
  MaximInterface_EXPORT error_code
  authenticatedEcdsaWriteMemory(int pageNum, const Page & page);

  /// Create data used by the Compute and Read Page Authentication command to
  /// generate a signature.
  MaximInterface_EXPORT static PageAuthenticationData
  createPageAuthenticationData(const RomId & romId, const Page & page,
                               const Page & challenge, int pageNum,
                               const ManId & manId);

  MaximInterface_EXPORT static const error_category & errorCategory();

protected:
  // Timing constants.
  static const int generateEcdsaSignatureTimeMs = 50;
  static const int generateEccKeyPairTimeMs = 100;
  static const int verifyEsdsaSignatureOrComputeEcdhTimeMs = 150;
  static const int sha256ComputationTimeMs = 3;
  static const int readMemoryTimeMs = /*1*/ 2;
  static const int writeMemoryTimeMs = 15;

  error_code writeCommand(uint_least8_t command,
                          const uint_least8_t * parameters,
                          size_t parametersSize);

  error_code writeCommand(uint_least8_t command) {
    return writeCommand(command, NULL, 0);
  }

  error_code readVariableLengthResponse(uint_least8_t * response,
                                        size_t & responseSize);

  error_code readFixedLengthResponse(uint_least8_t * response,
                                     size_t responseSize);

  void sleep(int ms) const { (*sleep_)(ms); }

private:
  enum AuthType {
    HmacWithSecretA = 0,
    HmacWithSecretB = 1,
    HmacWithSecretS = 2,
    EcdsaWithKeyA = 3,
    EcdsaWithKeyB = 4,
    EcdsaWithKeyC = 5
  };

  const Sleep * sleep_;
  I2CMaster * master;
  uint_least8_t address_;

  error_code computeAndReadPageAuthentication(int pageNum, AuthType authType);
};

/// Read the device ROM ID and MAN ID using the Read Memory command on the
/// ROM Options page.
/// @param[out] romId
/// Read ROM ID valid when operation is successful. May be set to NULL.
/// @param[out] manId
/// Read MAN ID valid when operation is successful. May be set to NULL.
MaximInterface_EXPORT error_code readRomIdAndManId(DS28C36 & ds28c36,
                                                   RomId * romId,
                                                   ManId * manId);

/// Hash arbitrary length data with successive Compute Multiblock Hash commands.
/// @param data Data to hash.
/// @param dataSize Size of data to hash.
MaximInterface_EXPORT error_code computeMultiblockHash(
    DS28C36 & ds28c36, const uint_least8_t * data, const size_t dataSize);

/// Verify ECDSA signature.
/// @param publicKey Public key to use for verification.
/// @param data Data to verify.
/// @param dataSize Size of data to verify.
/// @param signature Signature to verify.
/// @param pioa New state of PIOA if verification successful.
/// @param piob New state of PIOB if verification successful.
MaximInterface_EXPORT error_code verifyEcdsaSignature(
    DS28C36 & ds28c36, DS28C36::KeyNum publicKey, const uint_least8_t * data,
    size_t dataSize, const Ecc256::Signature & signature,
    DS28C36::PioState pioa = DS28C36::Unchanged,
    DS28C36::PioState piob = DS28C36::Unchanged);

/// Verify ECDSA signature.
/// @param publicKey
/// Public key to use for verification which is loaded into Public Key S.
/// @param data Data to verify.
/// @param dataSize Size of data to verify.
/// @param signature Signature to verify.
/// @param pioa New state of PIOA if verification successful.
/// @param piob New state of PIOB if verification successful.
MaximInterface_EXPORT error_code
verifyEcdsaSignature(DS28C36 & ds28c36, const Ecc256::PublicKey & publicKey,
                     const uint_least8_t * data, size_t dataSize,
                     const Ecc256::Signature & signature,
                     DS28C36::PioState pioa = DS28C36::Unchanged,
                     DS28C36::PioState piob = DS28C36::Unchanged);

inline error_code make_error_code(DS28C36::ErrorValue e) {
  return error_code(e, DS28C36::errorCategory());
}

/// Interface to the DS2476 coprocessor.
class DS2476 : public DS28C36 {
public:
  DS2476(const Sleep & sleep, I2CMaster & master, uint_least8_t address = 0x76)
      : DS28C36(sleep, master, address) {}

  /// Generate ECDSA signature.
  /// @param keyNum Private key to use to create signature.
  /// Key S cannot be used with this command.
  /// @param[out] signature Computed signature.
  MaximInterface_EXPORT error_code
  generateEcdsaSignature(KeyNum keyNum, Ecc256::Signature & signature);

  /// Compute unique SHA2 secret.
  /// @param msecretNum Master secret to use in computation.
  MaximInterface_EXPORT error_code
  computeSha2UniqueSecret(SecretNum msecretNum);

  /// Compute SHA2 HMAC.
  /// @param[out] hmac Computed HMAC.
  MaximInterface_EXPORT error_code computeSha2Hmac(Sha256::Hash & hmac);
};

/// Enable coprocessor functionality on the DS2476 by writing to the
/// ROM Options page.
MaximInterface_EXPORT error_code enableCoprocessor(DS2476 & ds2476);

} // namespace MaximInterface

#endif
