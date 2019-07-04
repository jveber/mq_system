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

#ifndef MaximInterface_RomId
#define MaximInterface_RomId

#include "array.hpp"
#include "crc.hpp"

namespace MaximInterface {

/// Standard container for a 1-Wire ROM ID.
typedef array<uint_least8_t, 8> RomId;

/// @{
/// Access the Family Code byte.
inline RomId::value_type familyCode(const RomId & romId) {
  return romId.front();
}
inline void setFamilyCode(RomId & romId, RomId::value_type familyCode) {
  romId.front() = familyCode;
}
/// @}

/// @{
/// Access the CRC8 byte.
inline RomId::value_type crc8(const RomId & romId) { return romId.back(); }
inline void setCrc8(RomId & romId, RomId::value_type crc8) {
  romId.back() = crc8;
}
/// @}

/// Check if the ROM ID is valid (Family Code and CRC8 are both valid).
/// @returns True if the ROM ID is valid.
inline bool valid(const RomId & romId) {
  return calculateCrc8(romId.data(), romId.size() - 1) == crc8(romId);
}

} // namespace MaximInterface

#endif
