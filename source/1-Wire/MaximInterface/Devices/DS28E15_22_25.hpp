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

#ifndef MaximInterface_DS28E15_22_25
#define MaximInterface_DS28E15_22_25

#include <MaximInterface/Links/OneWireMaster.hpp>
#include <MaximInterface/Links/SelectRom.hpp>
#include <MaximInterface/Links/Sleep.hpp>
#include <MaximInterface/Utilities/array.hpp>
#include <MaximInterface/Utilities/Export.h>
#include <MaximInterface/Utilities/ManId.hpp>
#include <MaximInterface/Utilities/Sha256.hpp>

namespace MaximInterface {

/// Interface to the DS28E15/22/25 series of authenticators
/// including low power variants.
class DS28E15_22_25 {
public:
  enum ErrorValue { CrcError = 1, OperationFailure };

  /// Holds the contents of a device memory segment.
  typedef array<uint_least8_t, 4> Segment;

  /// Holds the contents of a device memory page.
  typedef array<uint_least8_t, 32> Page;

  /// Number of segments per page.
  static const int segmentsPerPage = Page::csize / Segment::csize;

  /// Holds the contents of the device scratchpad.
  typedef array<uint_least8_t, 32> Scratchpad;

  /// Container for the device personality. 
  struct Personality {
    uint_least8_t PB1;
    uint_least8_t PB2;
    ManId manId;
    
    bool secretLocked() const { return PB2 & 0x01; }
  };

  /// Represents the status of a memory protection block.
  class BlockProtection {
  private:
    static const unsigned int readProtectionMask = 0x80,
                              writeProtectionMask = 0x40,
                              eepromEmulationMask = 0x20,
                              authProtectionMask = 0x10,
                              blockNumMask = 0x0F;
    uint_least8_t status;

  public:
    explicit BlockProtection(uint_least8_t status = 0x00) : status(status) {}
    MaximInterface_EXPORT BlockProtection(bool readProtection,
                                          bool writeProtection,
                                          bool eepromEmulation,
                                          bool authProtection,
                                          int blockNum);

    /// Get the byte representation used by the device.
    uint_least8_t statusByte() const { return status; }
    /// Set the byte representation used by the device.
    void setStatusByte(uint_least8_t status) { this->status = status; }

    /// Get the Block Number which is indexed from zero.
    int blockNum() const { return (status & blockNumMask); }
    /// Set the Block Number which is indexed from zero.
    MaximInterface_EXPORT void setBlockNum(int blockNum);

    /// Get the Read Protection status.
    /// @returns True if Read Protection is enabled.
    bool readProtection() const {
      return ((status & readProtectionMask) == readProtectionMask);
    }
    /// Set the Read Protection status.
    MaximInterface_EXPORT void setReadProtection(bool readProtection);

    /// Get the Write Protection status.
    /// @returns True if Write Protection is enabled.
    bool writeProtection() const {
      return ((status & writeProtectionMask) == writeProtectionMask);
    }
    /// Set the Write Protection status.
    MaximInterface_EXPORT void setWriteProtection(bool writeProtection);

    /// Get the EEPROM Emulation Mode status.
    /// @returns True if EEPROM Emulation Mode is enabled.
    bool eepromEmulation() const {
      return ((status & eepromEmulationMask) == eepromEmulationMask);
    }
    /// Set the EEPROM Emulation Mode status.
    MaximInterface_EXPORT void setEepromEmulation(bool eepromEmulation);

    /// Get the Authentication Protection status.
    /// @returns True if Authentication Protection is enabled.
    bool authProtection() const {
      return ((status & authProtectionMask) == authProtectionMask);
    }
    /// Set the Authentication Protection status.
    MaximInterface_EXPORT void setAuthProtection(bool authProtection);

    /// Check if no protection options are enabled.
    /// @returns True if no protection options are enabled.
    MaximInterface_EXPORT bool noProtection() const;
  };

  /// Format data to hash for an Authenticated Write to a memory segment.
  /// @param pageNum Page number for write operation.
  /// @param segmentNum Segment number within page for write operation.
  /// @param[in] newData New data to write to the segment.
  /// @param[in] oldData Existing data contained in the segment.
  /// @param[in] romId 1-Wire ROM ID of the device.
  /// @param[in] manId Manufacturer ID of the device.
  MaximInterface_EXPORT static Sha256::WriteMacData
  createSegmentWriteMacData(int pageNum, int segmentNum,
                            const Segment & newData, const Segment & oldData,
                            const RomId & romId, const ManId & manId);

  /// Format data to hash for an Authenticated Write to a memory protection block.
  /// @param[in] newProtection New protection status to write.
  /// @param[in] oldProtection Existing protection status in device.
  /// @param[in] romId 1-Wire ROM ID of the device.
  /// @param[in] manId Manufacturer ID of the device.
  MaximInterface_EXPORT static Sha256::WriteMacData
  createProtectionWriteMacData(const BlockProtection & newProtection,
                               const BlockProtection & oldProtection,
                               const RomId & romId, const ManId & manId);

  /// Format data to hash to compute the next secret from the existing secret.
  /// @param[in] bindingPage Binding data from a device memory page.
  /// @param bindingPageNum Number of the page where the binding data is from.
  /// @param[in] partialSecret Partial secret data from the device scratchpad.
  /// @param[in] romId 1-Wire ROM ID of the device.
  /// @param[in] manId Manufacturer ID of the device.
  MaximInterface_EXPORT static Sha256::SlaveSecretData
  createSlaveSecretData(const Page & bindingPage, int bindingPageNum,
                        const Scratchpad & partialSecret, const RomId & romId,
                        const ManId & manId);

  /// Format data to hash for device authentication.
  /// @param[in] pageData Data from a device memory page.
  /// @param pageNum Number of the page to use data from.
  /// @param[in] challenge Random challenge to prevent replay attacks.
  /// @param[in] romId 1-Wire ROM ID of the device.
  /// @param[in] manId Manufacturer ID of the device.
  MaximInterface_EXPORT static Sha256::AuthMacData
  createAuthMacData(const Page & pageData, int pageNum,
                    const Scratchpad & challenge, const RomId & romId,
                    const ManId & manId);

  /// Format datat to hash for device authentication using anonymous mode.
  /// @param[in] pageData Data from a device memory page.
  /// @param pageNum Number of the page to use data from.
  /// @param[in] challenge Random challenge to prevent replay attacks.
  /// @param[in] manId Manufacturer ID of the device.
  MaximInterface_EXPORT static Sha256::AuthMacData
  createAnonAuthMacData(const Page & pageData, int pageNum,
                        const Scratchpad & challenge, const ManId & manId);

  void setSleep(const Sleep & sleep) { this->sleep = &sleep; }
  void setMaster(OneWireMaster & master) { this->master = &master; }
  void setSelectRom(const SelectRom & selectRom) {
    this->selectRom = selectRom;
  }

  // Const member functions should not affect the state of the memory,
  // block protection, or secret on the device.

  /// Read memory segment using the Read Memory command on the device.
  /// @param pageNum Page number for read operation.
  /// @param segmentNum Segment number within page for read operation.
  /// @param[out] data Buffer to read data from the segment into.
  MaximInterface_EXPORT error_code readSegment(int pageNum, int segmentNum,
                                               Segment & data) const;

  /// Continue an in-progress readSegment operation.
  /// @note A CRC16 will encountered after reading the last segment of a page.
  /// @param[out] data Buffer to read data from the segment into.
  MaximInterface_EXPORT error_code continueReadSegment(Segment & data) const;

  /// Write memory segment using the Write Memory command.
  /// @note 1-Wire ROM selection should have already occurred.
  /// @param pageNum Page number for write operation.
  /// @param segmentNum Segment number within page for write operation.
  /// @param[in] data Data to write to the memory segment.
  MaximInterface_EXPORT error_code writeSegment(int pageNum, int segmentNum,
                                                const Segment & data);

  /// Continue an in-progress Write Memory command.
  /// @param[in] data Data to write to the memory segment.
  MaximInterface_EXPORT error_code continueWriteSegment(const Segment & data);

  /// Read memory page using the Read Memory command on the device.
  /// @param pageNum Page number for write operation.
  /// @param[out] rdbuf Buffer to read data from the page into.
  MaximInterface_EXPORT error_code readPage(int pageNum, Page & rdbuf) const;

  /// Continue an in-progress readPageOperation.
  /// @param[out] rdbuf Buffer to read data from the page into.
  MaximInterface_EXPORT error_code continueReadPage(Page & rdbuf) const;

  /// Perform a Compute Page MAC command on the device.
  /// Read back the MAC and verify the CRC16.
  /// @param pageNum Page number to use for the computation.
  /// @param anon True to compute in anonymous mode where ROM ID is not used.
  /// @param[out] mac The device computed MAC.
  MaximInterface_EXPORT error_code computeReadPageMac(int pageNum, bool anon,
                                                      Sha256::Hash & mac) const;

  /// Update the status of a memory protection block using the
  /// Write Page Protection command.
  /// @param[in] Desired protection status for the block.
  ///            It is not possible to disable existing protections.
  /// @param continuing True to continue a previous Write Page Protection command.
  ///                   False to begin a new command.
  MaximInterface_EXPORT error_code
  writeBlockProtection(const BlockProtection & protection);

  /// Update the status of a memory protection block using the
  /// Authenticated Write Page Protection command.
  /// @param[in] newProtection New protection status to write.
  /// @param[in] mac Write MAC computed for this operation.
  MaximInterface_EXPORT error_code writeAuthBlockProtection(
      const BlockProtection & newProtection, const Sha256::Hash & mac);

  /// Perform Load and Lock Secret command on the device.
  /// @note The secret should already be stored in the scratchpad on the device.
  /// @param lock Prevent further changes to the secret on the device after loading.
  MaximInterface_EXPORT error_code loadSecret(bool lock);

  /// Perform a Compute and Lock Secret command on the device.
  /// @param pageNum Page number to use as the binding data.
  /// @param lock Prevent further changes to the secret on the device after computing.
  MaximInterface_EXPORT error_code computeSecret(int pageNum, bool lock);

  /// Read the personality bytes using the Read Status command.
  /// @param[out] personality Receives personality read from device.
  MaximInterface_EXPORT error_code
  readPersonality(Personality & personality) const;

  MaximInterface_EXPORT static const error_category & errorCategory();

protected:
  enum Variant { DS28E15, DS28E22, DS28E25 };

  DS28E15_22_25(const Sleep & sleep, OneWireMaster & master,
                const SelectRom & selectRom)
      : selectRom(selectRom), master(&master), sleep(&sleep) {}

  error_code doWriteScratchpad(const Scratchpad & data, Variant variant);

  error_code doReadScratchpad(Scratchpad & data, Variant variant) const;

  error_code doReadBlockProtection(int blockNum, BlockProtection & protection,
                                   Variant variant) const;

  error_code doWriteAuthSegment(int pageNum, int segmentNum,
                                const Segment & newData,
                                const Sha256::Hash & mac, Variant variant);

  error_code doContinueWriteAuthSegment(const Segment & newData,
                                        const Sha256::Hash & mac,
                                        Variant variant);

  error_code doReadAllBlockProtection(BlockProtection * protection,
                                      size_t protectionLen,
                                      Variant variant) const;

  error_code doLoadSecret(bool lock, bool lowPower);

  error_code doComputeSecret(int pageNum, bool lock, bool lowPower);

private:
  enum Command {
    WriteMemory = 0x55,
    ReadMemory = 0xF0,
    LoadAndLockSecret = 0x33,
    ComputeAndLockSecret = 0x3C,
    ReadWriteScratchpad = 0x0F,
    ComputePageMac = 0xA5,
    ReadStatus = 0xAA,
    WriteBlockProtection = 0xC3,
    AuthWriteMemory = 0x5A,
    AuthWriteBlockProtection = 0xCC,
  };

  error_code doWriteAuthSegment(const Segment & newData,
                                const Sha256::Hash & mac, Variant variant,
                                bool continuing);

  error_code writeCommandWithCrc(
      Command command, uint_least8_t parameter,
      OneWireMaster::Level level = OneWireMaster::NormalLevel) const;

  SelectRom selectRom;
  OneWireMaster * master;
  const Sleep * sleep;
};

inline error_code make_error_code(DS28E15_22_25::ErrorValue e) {
  return error_code(e, DS28E15_22_25::errorCategory());
}

inline bool operator==(DS28E15_22_25::BlockProtection lhs,
                       DS28E15_22_25::BlockProtection rhs) {
  return lhs.statusByte() == rhs.statusByte();
}

inline bool operator!=(DS28E15_22_25::BlockProtection lhs,
                       DS28E15_22_25::BlockProtection rhs) {
  return !operator==(lhs, rhs);
}

/// Interface to the DS28EL15 (low power) authenticator.
class DS28EL15 : public DS28E15_22_25 {
public:
  // DS28E15_22_25 traits
  static const int memoryPages = 2;
  static const int protectionBlocks = 4;

  DS28EL15(const Sleep & sleep, OneWireMaster & master,
           const SelectRom & selectRom)
      : DS28E15_22_25(sleep, master, selectRom) {}

  /// Perform Write Scratchpad operation on the device.
  /// @param[in] data Data to write to the scratchpad.
  MaximInterface_EXPORT error_code writeScratchpad(const Scratchpad & data);

  /// Perform a Read Scratchpad operation on the device.
  /// @param[out] data Buffer to read data from the scratchpad into.
  MaximInterface_EXPORT error_code readScratchpad(Scratchpad & data) const;

  /// Read the status of a memory protection block using the Read Status command.
  /// @param blockNum Block number to to read status of.
  /// @param[out] protection Receives protection status read from device.
  MaximInterface_EXPORT error_code
  readBlockProtection(int blockNum, BlockProtection & protection) const;

  /// Write memory segment using the Authenticated Write Memory command.
  /// @param pageNum Page number for write operation.
  /// @param segmentNum Segment number within page for write operation.
  /// @param[in] newData New data to write to the segment.
  /// @param[in] mac Write MAC computed for this operation.
  MaximInterface_EXPORT error_code writeAuthSegment(int pageNum, int segmentNum,
                                                    const Segment & newData,
                                                    const Sha256::Hash & mac);

  /// Continue an in-progress Authenticated Write Memory command.
  /// @param[in] newData New data to write to the segment.
  /// @param[in] mac Write MAC computed for this operation.
  MaximInterface_EXPORT error_code
  continueWriteAuthSegment(const Segment & newData, const Sha256::Hash & mac);

  /// Read the status of all memory protection blocks using the Read Status command.
  /// @param[out] protection Receives protection statuses read from device.
  MaximInterface_EXPORT error_code readAllBlockProtection(
      array<BlockProtection, protectionBlocks> & protection) const;
};

/// Interface to the DS28E15 authenticator.
class DS28E15 : public DS28EL15 {
public:
  DS28E15(const Sleep & sleep, OneWireMaster & master,
          const SelectRom & selectRom)
      : DS28EL15(sleep, master, selectRom) {}

  /// Perform Load and Lock Secret command on the device.
  /// @note The secret should already be stored in the scratchpad on the device.
  /// @param lock Prevent further changes to the secret on the device after loading.
  MaximInterface_EXPORT error_code loadSecret(bool lock);

  /// Perform a Compute and Lock Secret command on the device.
  /// @param pageNum Page number to use as the binding data.
  /// @param lock Prevent further changes to the secret on the device after computing.
  MaximInterface_EXPORT error_code computeSecret(int pageNum, bool lock);
};

/// Interface to the DS28EL22 (low power) authenticator.
class DS28EL22 : public DS28E15_22_25 {
public:
  // DS28E15_22_25 traits
  static const int memoryPages = 8;
  static const int protectionBlocks = 4;

  DS28EL22(const Sleep & sleep, OneWireMaster & master,
           const SelectRom & selectRom)
      : DS28E15_22_25(sleep, master, selectRom) {}

  /// Perform Write Scratchpad operation on the device.
  /// @param[in] data Data to write to the scratchpad.
  MaximInterface_EXPORT error_code writeScratchpad(const Scratchpad & data);

  /// Perform a Read Scratchpad operation on the device.
  /// @param[out] data Buffer to read data from the scratchpad into.
  MaximInterface_EXPORT error_code readScratchpad(Scratchpad & data) const;

  /// Read the status of a memory protection block using the Read Status command.
  /// @param blockNum Block number to to read status of.
  /// @param[out] protection Receives protection status read from device.
  MaximInterface_EXPORT error_code
  readBlockProtection(int blockNum, BlockProtection & protection) const;

  /// Write memory segment using the Authenticated Write Memory command.
  /// @param pageNum Page number for write operation.
  /// @param segmentNum Segment number within page for write operation.
  /// @param[in] newData New data to write to the segment.
  /// @param[in] mac Write MAC computed for this operation.
  MaximInterface_EXPORT error_code writeAuthSegment(int pageNum, int segmentNum,
                                                    const Segment & newData,
                                                    const Sha256::Hash & mac);

  /// Continue an in-progress Authenticated Write Memory command.
  /// @param[in] newData New data to write to the segment.
  /// @param[in] mac Write MAC computed for this operation.
  MaximInterface_EXPORT error_code
  continueWriteAuthSegment(const Segment & newData, const Sha256::Hash & mac);

  /// Read the status of all memory protection blocks using the Read Status command.
  /// @param[out] protection Receives protection statuses read from device.
  MaximInterface_EXPORT error_code readAllBlockProtection(
      array<BlockProtection, protectionBlocks> & protection) const;
};

/// Interface to the DS28E22 authenticator.
class DS28E22 : public DS28EL22 {
public:
  DS28E22(const Sleep & sleep, OneWireMaster & master,
          const SelectRom & selectRom)
      : DS28EL22(sleep, master, selectRom) {}

  /// Perform Load and Lock Secret command on the device.
  /// @note The secret should already be stored in the scratchpad on the device.
  /// @param lock Prevent further changes to the secret on the device after loading.
  MaximInterface_EXPORT error_code loadSecret(bool lock);

  /// Perform a Compute and Lock Secret command on the device.
  /// @param pageNum Page number to use as the binding data.
  /// @param lock Prevent further changes to the secret on the device after computing.
  MaximInterface_EXPORT error_code computeSecret(int pageNum, bool lock);
};

/// Interface to the DS28EL25 (low power) authenticator.
class DS28EL25 : public DS28E15_22_25 {
public:
  // DS28E15_22_25 traits
  static const int memoryPages = 16;
  static const int protectionBlocks = 8;

  DS28EL25(const Sleep & sleep, OneWireMaster & master,
           const SelectRom & selectRom)
      : DS28E15_22_25(sleep, master, selectRom) {}

  /// Perform Write Scratchpad operation on the device.
  /// @param[in] data Data to write to the scratchpad.
  MaximInterface_EXPORT error_code writeScratchpad(const Scratchpad & data);

  /// Perform a Read Scratchpad operation on the device.
  /// @param[out] data Buffer to read data from the scratchpad into.
  MaximInterface_EXPORT error_code readScratchpad(Scratchpad & data) const;

  /// Read the status of a memory protection block using the Read Status command.
  /// @param blockNum Block number to to read status of.
  /// @param[out] protection Receives protection status read from device.
  MaximInterface_EXPORT error_code
  readBlockProtection(int blockNum, BlockProtection & protection) const;

  /// Write memory segment using the Authenticated Write Memory command.
  /// @param pageNum Page number for write operation.
  /// @param segmentNum Segment number within page for write operation.
  /// @param[in] newData New data to write to the segment.
  /// @param[in] mac Write MAC computed for this operation.
  MaximInterface_EXPORT error_code writeAuthSegment(int pageNum, int segmentNum,
                                                    const Segment & newData,
                                                    const Sha256::Hash & mac);

  /// Continue an in-progress Authenticated Write Memory command.
  /// @param[in] newData New data to write to the segment.
  /// @param[in] mac Write MAC computed for this operation.
  MaximInterface_EXPORT error_code
  continueWriteAuthSegment(const Segment & newData, const Sha256::Hash & mac);

  /// Read the status of all memory protection blocks using the Read Status command.
  /// @param[out] protection Receives protection statuses read from device.
  MaximInterface_EXPORT error_code readAllBlockProtection(
      array<BlockProtection, protectionBlocks> & protection) const;
};

/// Interface to the DS28E25 authenticator.
class DS28E25 : public DS28EL25 {
public:
  DS28E25(const Sleep & sleep, OneWireMaster & master,
          const SelectRom & selectRom)
      : DS28EL25(sleep, master, selectRom) {}

  /// Perform Load and Lock Secret command on the device.
  /// @note The secret should already be stored in the scratchpad on the device.
  /// @param lock Prevent further changes to the secret on the device after loading.
  MaximInterface_EXPORT error_code loadSecret(bool lock);

  /// Perform a Compute and Lock Secret command on the device.
  /// @param pageNum Page number to use as the binding data.
  /// @param lock Prevent further changes to the secret on the device after computing.
  MaximInterface_EXPORT error_code computeSecret(int pageNum, bool lock);
};

} // namespace MaximInterface

#endif
