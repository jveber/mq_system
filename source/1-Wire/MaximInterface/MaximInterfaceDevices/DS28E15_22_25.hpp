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

#ifndef MaximInterfaceDevices_DS28E15_22_25_hpp
#define MaximInterfaceDevices_DS28E15_22_25_hpp

#include <stdint.h>
#include <MaximInterfaceCore/Algorithm.hpp>
#include <MaximInterfaceCore/array_span.hpp>
#include <MaximInterfaceCore/ManId.hpp>
#include <MaximInterfaceCore/OneWireMaster.hpp>
#include <MaximInterfaceCore/RomId.hpp>
#include <MaximInterfaceCore/SelectRom.hpp>
#include <MaximInterfaceCore/Sleep.hpp>
#include "Config.hpp"

namespace MaximInterfaceDevices {

/// @brief
/// Interface to the DS28E15/22/25 series of authenticators
/// including low power variants.
/// @details
/// Const member functions should not affect the state of the memory,
/// block protection, or secret on the device.
class DS28E15_22_25 {
public:
  enum ErrorValue { CrcError = 1, OperationFailure };

  /// Holds the contents of a device memory segment.
  typedef Core::array_span<uint_least8_t, 4> Segment;

  /// Holds the contents of a device memory page.
  typedef Core::array_span<uint_least8_t, 32> Page;

  /// Number of segments per page.
  static const int segmentsPerPage = Page::size / Segment::size;

  /// Container for the device personality.
  struct Personality {
    uint_least8_t PB1;
    uint_least8_t PB2;
    Core::ManId::array manId;

    bool secretLocked() const { return PB2 & 0x01; }
  };

  // Represents the status of a memory protection block.
  class BlockProtection;

  // Format data to hash for an Authenticated Write to a memory segment.
  class SegmentWriteMacData;

  // Format data to hash for an Authenticated Write to a memory protection block.
  class ProtectionWriteMacData;

  // Format data to hash for device authentication or computing the next secret
  // from the existing secret.
  class AuthenticationData;

  void setSleep(Core::Sleep & sleep) { this->sleep = &sleep; }

  void setMaster(Core::OneWireMaster & master) { this->master = &master; }

  void setSelectRom(const Core::SelectRom & selectRom) {
    this->selectRom = selectRom;
  }

  /// @brief Read memory segment using the Read Memory command on the device.
  /// @param pageNum Page number for read operation.
  /// @param segmentNum Segment number within page for read operation.
  /// @returns Buffer to read data from the segment into.
  MaximInterfaceDevices_EXPORT Core::Result<Segment::array>
  readSegment(int pageNum, int segmentNum) const;

  /// @brief Continue an in-progress readSegment operation.
  /// @note A CRC16 will encountered after reading the last segment of a page.
  /// @returns Buffer to read data from the segment into.
  MaximInterfaceDevices_EXPORT Core::Result<Segment::array>
  continueReadSegment() const;

  /// @brief Write memory segment using the Write Memory command.
  /// @note 1-Wire ROM selection should have already occurred.
  /// @param pageNum Page number for write operation.
  /// @param segmentNum Segment number within page for write operation.
  /// @param[in] data Data to write to the memory segment.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeSegment(int pageNum, int segmentNum, Segment::const_span data);

  /// @brief Continue an in-progress Write Memory command.
  /// @param[in] data Data to write to the memory segment.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  continueWriteSegment(Segment::const_span data);

  /// @brief Read memory page using the Read Memory command on the device.
  /// @param pageNum Page number for write operation.
  /// @returns Buffer to read data from the page into.
  MaximInterfaceDevices_EXPORT Core::Result<Page::array>
  readPage(int pageNum) const;

  /// @brief Continue an in-progress readPageOperation.
  /// @returns Buffer to read data from the page into.
  MaximInterfaceDevices_EXPORT Core::Result<Page::array>
  continueReadPage() const;

  /// @brief
  /// Perform a Compute Page MAC command on the device.
  /// Read back the MAC and verify the CRC16.
  /// @param pageNum Page number to use for the computation.
  /// @param anon True to compute in anonymous mode where ROM ID is not used.
  /// @returns The device computed MAC.
  MaximInterfaceDevices_EXPORT Core::Result<Page::array>
  computeReadPageMac(int pageNum, bool anon) const;

  /// @brief
  /// Update the status of a memory protection block using the
  /// Write Page Protection command.
  /// @param protection
  /// Desired protection status for the block.
  /// It is not possible to disable existing protections.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeBlockProtection(BlockProtection protection);

  /// @brief
  /// Update the status of a memory protection block using the
  /// Authenticated Write Page Protection command.
  /// @param newProtection New protection status to write.
  /// @param[in] mac Write MAC computed for this operation.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeAuthBlockProtection(BlockProtection newProtection, Page::const_span mac);

  /// @brief Perform Load and Lock Secret command on the device.
  /// @note The secret should already be stored in the scratchpad on the device.
  /// @param lock
  /// Prevent further changes to the secret on the device after loading.
  MaximInterfaceDevices_EXPORT Core::Result<void> loadSecret(bool lock);

  /// @brief Perform a Compute and Lock Secret command on the device.
  /// @param pageNum Page number to use as the binding data.
  /// @param lock
  /// Prevent further changes to the secret on the device after computing.
  MaximInterfaceDevices_EXPORT Core::Result<void> computeSecret(int pageNum,
                                                                bool lock);

  /// @brief Read the personality bytes using the Read Status command.
  /// @returns Receives personality read from device.
  MaximInterfaceDevices_EXPORT Core::Result<Personality>
  readPersonality() const;

  MaximInterfaceDevices_EXPORT static const Core::error_category &
  errorCategory();

protected:
  enum Variant { DS28E15, DS28E22, DS28E25 };

  DS28E15_22_25(Core::Sleep & sleep, Core::OneWireMaster & master,
                const Core::SelectRom & selectRom)
      : selectRom(selectRom), master(&master), sleep(&sleep) {}

  ~DS28E15_22_25() {}

  Core::Result<void> doWriteScratchpad(Page::const_span data, Variant variant);

  Core::Result<Page::array> doReadScratchpad(Variant variant) const;

  Core::Result<BlockProtection> doReadBlockProtection(int blockNum,
                                                      Variant variant) const;

  Core::Result<void> doWriteAuthSegment(int pageNum, int segmentNum,
                                        Segment::const_span newData,
                                        Page::const_span mac, Variant variant);

  Core::Result<void> doContinueWriteAuthSegment(Segment::const_span newData,
                                                Page::const_span mac,
                                                Variant variant);

  Core::Result<void>
  doReadAllBlockProtection(Core::span<BlockProtection> protection,
                           Variant variant) const;

  Core::Result<void> doLoadSecret(bool lock, bool lowPower);

  Core::Result<void> doComputeSecret(int pageNum, bool lock, bool lowPower);

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

  Core::Result<void> doWriteAuthSegment(Segment::const_span newData,
                                        Page::const_span mac, Variant variant,
                                        bool continuing);

  Core::Result<void>
  writeCommandWithCrc(Command command, uint_least8_t parameter,
                      Core::OneWireMaster::Level level =
                          Core::OneWireMaster::NormalLevel) const;

  Core::SelectRom selectRom;
  Core::OneWireMaster * master;
  Core::Sleep * sleep;
};

} // namespace MaximInterfaceDevices
namespace MaximInterfaceCore {

template <>
struct is_error_code_enum<MaximInterfaceDevices::DS28E15_22_25::ErrorValue>
    : true_type {};

} // namespace MaximInterfaceCore
namespace MaximInterfaceDevices {

inline Core::error_code make_error_code(DS28E15_22_25::ErrorValue e) {
  return Core::error_code(e, DS28E15_22_25::errorCategory());
}

/// Interface to the DS28EL15 (low power) authenticator.
class DS28EL15 : public DS28E15_22_25 {
public:
  // DS28E15_22_25 traits
  static const int memoryPages = 2;
  static const int protectionBlocks = 4;

  DS28EL15(Core::Sleep & sleep, Core::OneWireMaster & master,
           const Core::SelectRom & selectRom)
      : DS28E15_22_25(sleep, master, selectRom) {}

  /// @brief Perform Write Scratchpad operation on the device.
  /// @param[in] data Data to write to the scratchpad.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeScratchpad(Page::const_span data);

  /// @brief Perform a Read Scratchpad operation on the device.
  /// @returns Buffer to read data from the scratchpad into.
  MaximInterfaceDevices_EXPORT Core::Result<Page::array> readScratchpad() const;

  /// @brief
  /// Read the status of a memory protection block using the Read Status command.
  /// @param blockNum Block number to to read status of.
  /// @returns Receives protection status read from device.
  MaximInterfaceDevices_EXPORT Core::Result<BlockProtection>
  readBlockProtection(int blockNum) const;

  /// @brief Write memory segment using the Authenticated Write Memory command.
  /// @param pageNum Page number for write operation.
  /// @param segmentNum Segment number within page for write operation.
  /// @param[in] newData New data to write to the segment.
  /// @param[in] mac Write MAC computed for this operation.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeAuthSegment(int pageNum, int segmentNum, Segment::const_span newData,
                   Page::const_span mac);

  /// @brief Continue an in-progress Authenticated Write Memory command.
  /// @param[in] newData New data to write to the segment.
  /// @param[in] mac Write MAC computed for this operation.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  continueWriteAuthSegment(Segment::const_span newData, Page::const_span mac);

  /// @brief
  /// Read the status of all memory protection blocks using the Read Status command.
  /// @returns Receives protection statuses read from device.
  MaximInterfaceDevices_EXPORT
      Core::Result<Core::array<BlockProtection, protectionBlocks> >
      readAllBlockProtection() const;
};

/// Interface to the DS28E15 authenticator.
class DS28E15 : public DS28EL15 {
public:
  DS28E15(Core::Sleep & sleep, Core::OneWireMaster & master,
          const Core::SelectRom & selectRom)
      : DS28EL15(sleep, master, selectRom) {}

  /// @brief Perform Load and Lock Secret command on the device.
  /// @note The secret should already be stored in the scratchpad on the device.
  /// @param lock
  /// Prevent further changes to the secret on the device after loading.
  MaximInterfaceDevices_EXPORT Core::Result<void> loadSecret(bool lock);

  /// @brief Perform a Compute and Lock Secret command on the device.
  /// @param pageNum Page number to use as the binding data.
  /// @param lock
  /// Prevent further changes to the secret on the device after computing.
  MaximInterfaceDevices_EXPORT Core::Result<void> computeSecret(int pageNum,
                                                                bool lock);
};

/// Interface to the DS28EL22 (low power) authenticator.
class DS28EL22 : public DS28E15_22_25 {
public:
  // DS28E15_22_25 traits
  static const int memoryPages = 8;
  static const int protectionBlocks = 4;

  DS28EL22(Core::Sleep & sleep, Core::OneWireMaster & master,
           const Core::SelectRom & selectRom)
      : DS28E15_22_25(sleep, master, selectRom) {}

  /// @brief Perform Write Scratchpad operation on the device.
  /// @param[in] data Data to write to the scratchpad.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeScratchpad(Page::const_span data);

  /// @brief Perform a Read Scratchpad operation on the device.
  /// @returns Buffer to read data from the scratchpad into.
  MaximInterfaceDevices_EXPORT Core::Result<Page::array> readScratchpad() const;

  /// @brief
  /// Read the status of a memory protection block using the Read Status command.
  /// @param blockNum Block number to to read status of.
  /// @returns Receives protection status read from device.
  MaximInterfaceDevices_EXPORT Core::Result<BlockProtection>
  readBlockProtection(int blockNum) const;

  /// @brief Write memory segment using the Authenticated Write Memory command.
  /// @param pageNum Page number for write operation.
  /// @param segmentNum Segment number within page for write operation.
  /// @param[in] newData New data to write to the segment.
  /// @param[in] mac Write MAC computed for this operation.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeAuthSegment(int pageNum, int segmentNum, Segment::const_span newData,
                   Page::const_span mac);

  /// @brief Continue an in-progress Authenticated Write Memory command.
  /// @param[in] newData New data to write to the segment.
  /// @param[in] mac Write MAC computed for this operation.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  continueWriteAuthSegment(Segment::const_span newData, Page::const_span mac);

  /// @brief
  /// Read the status of all memory protection blocks using the Read Status command.
  /// @returns Receives protection statuses read from device.
  MaximInterfaceDevices_EXPORT
      Core::Result<Core::array<BlockProtection, protectionBlocks> >
      readAllBlockProtection() const;
};

/// Interface to the DS28E22 authenticator.
class DS28E22 : public DS28EL22 {
public:
  DS28E22(Core::Sleep & sleep, Core::OneWireMaster & master,
          const Core::SelectRom & selectRom)
      : DS28EL22(sleep, master, selectRom) {}

  /// @brief Perform Load and Lock Secret command on the device.
  /// @note The secret should already be stored in the scratchpad on the device.
  /// @param lock
  /// Prevent further changes to the secret on the device after loading.
  MaximInterfaceDevices_EXPORT Core::Result<void> loadSecret(bool lock);

  /// @brief Perform a Compute and Lock Secret command on the device.
  /// @param pageNum Page number to use as the binding data.
  /// @param lock
  /// Prevent further changes to the secret on the device after computing.
  MaximInterfaceDevices_EXPORT Core::Result<void> computeSecret(int pageNum,
                                                                bool lock);
};

/// Interface to the DS28EL25 (low power) authenticator.
class DS28EL25 : public DS28E15_22_25 {
public:
  // DS28E15_22_25 traits
  static const int memoryPages = 16;
  static const int protectionBlocks = 8;

  DS28EL25(Core::Sleep & sleep, Core::OneWireMaster & master,
           const Core::SelectRom & selectRom)
      : DS28E15_22_25(sleep, master, selectRom) {}

  /// @brief Perform Write Scratchpad operation on the device.
  /// @param[in] data Data to write to the scratchpad.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeScratchpad(Page::const_span data);

  /// @brief Perform a Read Scratchpad operation on the device.
  /// @returns Buffer to read data from the scratchpad into.
  MaximInterfaceDevices_EXPORT Core::Result<Page::array> readScratchpad() const;

  /// @brief
  /// Read the status of a memory protection block using the Read Status command.
  /// @param blockNum Block number to to read status of.
  /// @returns Receives protection status read from device.
  MaximInterfaceDevices_EXPORT Core::Result<BlockProtection>
  readBlockProtection(int blockNum) const;

  /// Write memory segment using the Authenticated Write Memory command.
  /// @param pageNum Page number for write operation.
  /// @param segmentNum Segment number within page for write operation.
  /// @param[in] newData New data to write to the segment.
  /// @param[in] mac Write MAC computed for this operation.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  writeAuthSegment(int pageNum, int segmentNum, Segment::const_span newData,
                   Page::const_span mac);

  /// @brief Continue an in-progress Authenticated Write Memory command.
  /// @param[in] newData New data to write to the segment.
  /// @param[in] mac Write MAC computed for this operation.
  MaximInterfaceDevices_EXPORT Core::Result<void>
  continueWriteAuthSegment(Segment::const_span newData, Page::const_span mac);

  /// @brief
  /// Read the status of all memory protection blocks using the Read Status command.
  /// @returns Receives protection statuses read from device.
  MaximInterfaceDevices_EXPORT
      Core::Result<Core::array<BlockProtection, protectionBlocks> >
      readAllBlockProtection() const;
};

/// Interface to the DS28E25 authenticator.
class DS28E25 : public DS28EL25 {
public:
  DS28E25(Core::Sleep & sleep, Core::OneWireMaster & master,
          const Core::SelectRom & selectRom)
      : DS28EL25(sleep, master, selectRom) {}

  /// @brief Perform Load and Lock Secret command on the device.
  /// @note The secret should already be stored in the scratchpad on the device.
  /// @param lock Prevent further changes to the secret on the device after loading.
  MaximInterfaceDevices_EXPORT Core::Result<void> loadSecret(bool lock);

  /// @brief Perform a Compute and Lock Secret command on the device.
  /// @param pageNum Page number to use as the binding data.
  /// @param lock
  /// Prevent further changes to the secret on the device after computing.
  MaximInterfaceDevices_EXPORT Core::Result<void> computeSecret(int pageNum,
                                                                bool lock);
};

/// Represents the status of a memory protection block.
class DS28E15_22_25::BlockProtection {
public:
  explicit BlockProtection(uint_least8_t status = 0x00) : status(status) {}

  /// Get the byte representation used by the device.
  uint_least8_t statusByte() const { return status; }

  /// Set the byte representation used by the device.
  BlockProtection & setStatusByte(uint_least8_t status) {
    this->status = status;
    return *this;
  }

  /// Get the Block Number which is indexed from zero.
  int blockNum() const { return (status & blockNumMask); }

  /// Set the Block Number which is indexed from zero.
  MaximInterfaceDevices_EXPORT BlockProtection & setBlockNum(int blockNum);

  /// @brief Get the Read Protection status.
  /// @returns True if Read Protection is enabled.
  bool readProtection() const {
    return ((status & readProtectionMask) == readProtectionMask);
  }

  /// Set the Read Protection status.
  MaximInterfaceDevices_EXPORT BlockProtection &
  setReadProtection(bool readProtection);

  /// @brief Get the Write Protection status.
  /// @returns True if Write Protection is enabled.
  bool writeProtection() const {
    return ((status & writeProtectionMask) == writeProtectionMask);
  }

  /// Set the Write Protection status.
  MaximInterfaceDevices_EXPORT BlockProtection &
  setWriteProtection(bool writeProtection);

  /// @brief Get the EEPROM Emulation Mode status.
  /// @returns True if EEPROM Emulation Mode is enabled.
  bool eepromEmulation() const {
    return ((status & eepromEmulationMask) == eepromEmulationMask);
  }

  /// Set the EEPROM Emulation Mode status.
  MaximInterfaceDevices_EXPORT BlockProtection &
  setEepromEmulation(bool eepromEmulation);

  /// @brief Get the Authentication Protection status.
  /// @returns True if Authentication Protection is enabled.
  bool authProtection() const {
    return ((status & authProtectionMask) == authProtectionMask);
  }

  /// Set the Authentication Protection status.
  MaximInterfaceDevices_EXPORT BlockProtection &
  setAuthProtection(bool authProtection);

  /// @brief Check if no protection options are enabled.
  /// @returns True if no protection options are enabled.
  MaximInterfaceDevices_EXPORT bool noProtection() const;

private:
  static const unsigned int readProtectionMask = 0x80,
                            writeProtectionMask = 0x40,
                            eepromEmulationMask = 0x20,
                            authProtectionMask = 0x10,
                            blockNumMask = 0x0F;
  uint_least8_t status;
};

/// Format data to hash for an Authenticated Write to a memory segment.
class DS28E15_22_25::SegmentWriteMacData {
public:
  typedef Core::array_span<uint_least8_t, 20> Result;

  SegmentWriteMacData() : result_() {}

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
    return const_cast<SegmentWriteMacData &>(*this).romId();
  }

  /// Set ROM ID.
  SegmentWriteMacData & setRomId(Core::RomId::const_span romId) {
    copy(romId, this->romId());
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
    return const_cast<SegmentWriteMacData &>(*this).manId();
  }

  /// Set MAN ID.
  SegmentWriteMacData & setManId(Core::ManId::const_span manId) {
    copy(manId, this->manId());
    return *this;
  }

  /// @}

  /// @name Page number
  /// @brief Page number for write operation.
  /// @{

  /// Get page number.
  int pageNum() const { return result_[pageNumIdx]; }

  /// Set page number.
  SegmentWriteMacData & setPageNum(int pageNum) {
    result_[pageNumIdx] = pageNum;
    return *this;
  }

  /// @}

  /// @name Segment number
  /// @brief Segment number within page for write operation.
  /// @{

  /// Get segment number.
  int segmentNum() const { return result_[segmentNumIdx]; }

  /// Set segment number.
  SegmentWriteMacData & setSegmentNum(int segmentNum) {
    result_[segmentNumIdx] = segmentNum;
    return *this;
  }

  /// @}

  /// @name Old data
  /// @brief Existing data contained in the segment.
  /// @{

  /// Get mutable old data.
  Segment::span oldData() {
    return make_span(result_).subspan<oldDataIdx, Segment::size>();
  }

  /// Get immutable old data.
  Segment::const_span oldData() const {
    return const_cast<SegmentWriteMacData &>(*this).oldData();
  }

  /// Set old data.
  SegmentWriteMacData & setOldData(Segment::const_span oldData) {
    copy(oldData, this->oldData());
    return *this;
  }

  /// @}

  /// @name New data
  /// @brief New data to write to the segment.
  /// @{

  /// Get mutable new data.
  Segment::span newData() {
    return make_span(result_).subspan<newDataIdx, Segment::size>();
  }

  /// Get immutable new data.
  Segment::const_span newData() const {
    return const_cast<SegmentWriteMacData &>(*this).newData();
  }

  /// Set new data.
  SegmentWriteMacData & setNewData(Segment::const_span newData) {
    copy(newData, this->newData());
    return *this;
  }

  /// @}

private:
  static const size_t romIdIdx = 0;
  static const size_t manIdIdx = romIdIdx + Core::RomId::size;
  static const size_t pageNumIdx = manIdIdx + Core::ManId::size;
  static const size_t segmentNumIdx = pageNumIdx + 1;
  static const size_t oldDataIdx = segmentNumIdx + 1;
  static const size_t newDataIdx = oldDataIdx + Segment::size;

  Result::array result_;
};

/// Format data to hash for an Authenticated Write to a memory protection block.
class DS28E15_22_25::ProtectionWriteMacData {
public:
  typedef Core::array_span<uint_least8_t, 20> Result;

  MaximInterfaceDevices_EXPORT ProtectionWriteMacData();

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
    return const_cast<ProtectionWriteMacData &>(*this).romId();
  }

  /// Set ROM ID.
  ProtectionWriteMacData & setRomId(Core::RomId::const_span romId) {
    copy(romId, this->romId());
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
    return const_cast<ProtectionWriteMacData &>(*this).manId();
  }

  /// Set MAN ID.
  ProtectionWriteMacData & setManId(Core::ManId::const_span manId) {
    copy(manId, this->manId());
    return *this;
  }

  /// @}

  /// @name Old protection
  /// @brief Existing protection status in device.
  /// @{

  /// Get old protection.
  BlockProtection oldProtection() const { return oldProtection_; }

  /// Set old protection.
  MaximInterfaceDevices_EXPORT ProtectionWriteMacData &
  setOldProtection(BlockProtection oldProtection);

  /// @}

  /// @name New protection
  /// @brief New protection status to write.
  /// @{

  /// Get new protection.
  BlockProtection newProtection() const { return newProtection_; }

  /// Set new protection.
  MaximInterfaceDevices_EXPORT ProtectionWriteMacData &
  setNewProtection(BlockProtection newProtection);

  /// @}

private:
  static const size_t romIdIdx = 0;
  static const size_t manIdIdx = romIdIdx + Core::RomId::size;
  static const size_t blockNumIdx = manIdIdx + Core::ManId::size;
  static const size_t oldProtectionIdx = blockNumIdx + 2;
  static const size_t newProtectionIdx = oldProtectionIdx + 4;

  Result::array result_;
  BlockProtection oldProtection_;
  BlockProtection newProtection_;
};

/// @brief
/// Format data to hash for device authentication or computing the next secret
/// from the existing secret.
class DS28E15_22_25::AuthenticationData {
public:
  typedef Core::array_span<uint_least8_t, 76> Result;

  AuthenticationData() : result_() {}

  /// Formatted data result.
  Result::const_span result() const { return result_; }

  /// @name Page
  /// @brief Data from a device memory page.
  /// @{

  /// Get mutable page.
  Page::span page() {
    return make_span(result_).subspan<pageIdx, Page::size>();
  }

  /// Get immutable page.
  Page::const_span page() const {
    return const_cast<AuthenticationData &>(*this).page();
  }

  /// Set page.
  AuthenticationData & setPage(Page::const_span page) {
    copy(page, this->page());
    return *this;
  }

  /// @}

  /// @name Scratchpad
  /// @brief
  /// Data from device scratchpad used as a random challenge in device
  /// authentication and a partial secret in secret computation.
  /// @{

  /// Get mutable scratchpad.
  Page::span scratchpad() {
    return make_span(result_).subspan<scratchpadIdx, Page::size>();
  }

  /// Get immutable scratchpad.
  Page::const_span scratchpad() const {
    return const_cast<AuthenticationData &>(*this).scratchpad();
  }

  /// Set scratchpad.
  AuthenticationData & setScratchpad(Page::const_span scratchpad) {
    copy(scratchpad, this->scratchpad());
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
    return const_cast<AuthenticationData &>(*this).romId();
  }

  /// Set ROM ID.
  AuthenticationData & setRomId(Core::RomId::const_span romId) {
    copy(romId, this->romId());
    return *this;
  }

  /// Set ROM ID for use in anonymous mode.
  MaximInterfaceDevices_EXPORT AuthenticationData & setAnonymousRomId();

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
    return const_cast<AuthenticationData &>(*this).manId();
  }

  /// Set MAN ID.
  AuthenticationData & setManId(Core::ManId::const_span manId) {
    copy(manId, this->manId());
    return *this;
  }

  /// @}

  /// @name Page number
  /// @brief Number of the page to use data from.
  /// @{

  /// Get page number.
  int pageNum() const { return result_[pageNumIdx]; }

  /// Set page number.
  AuthenticationData & setPageNum(int pageNum) {
    result_[pageNumIdx] = pageNum;
    return *this;
  }

  /// @}

private:
  static const size_t pageIdx = 0;
  static const size_t scratchpadIdx = pageIdx + Page::size;
  static const size_t romIdIdx = scratchpadIdx + Page::size;
  static const size_t manIdIdx = romIdIdx + Core::RomId::size;
  static const size_t pageNumIdx = manIdIdx + Core::ManId::size;

  Result::array result_;
};

} // namespace MaximInterfaceDevices

#endif
