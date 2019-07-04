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

#ifndef MaximInterface_crc
#define MaximInterface_crc

#include <stddef.h>
#include <stdint.h>
#include <MaximInterface/Utilities/Export.h>

namespace MaximInterface {

/// Perform a CRC8 calculation.
/// @param data Data to pass though the CRC generator.
/// @param crc Beginning state of the CRC generator.
/// @returns The calculated CRC8.
MaximInterface_EXPORT uint_fast8_t calculateCrc8(uint_fast8_t data,
                                                 uint_fast8_t crc = 0);

/// Perform a CRC8 calculation with variable length data.
/// @param[in] data Data array to pass through the CRC generator.
/// @param dataLen Length of the data array to process.
/// @param crc Beginning state of the CRC generator.
/// @returns The calculated CRC8.
MaximInterface_EXPORT uint_fast8_t calculateCrc8(const uint_least8_t * data,
                                                 size_t dataLen,
                                                 uint_fast8_t crc = 0);

/// Perform a CRC16 calculation.
/// @param data Data to pass though the CRC generator.
/// @param crc Beginning state of the CRC generator.
/// @returns The calculated CRC16.
MaximInterface_EXPORT uint_fast16_t calculateCrc16(uint_fast8_t data,
                                                   uint_fast16_t crc = 0);

/// Perform a CRC16 calculation with variable length data.
/// @param[in] data Data array to pass through the CRC generator.
/// @param data_offset Offset of the data array to begin processing.
/// @param data_len Length of the data array to process.
/// @param crc Beginning state of the CRC generator.
/// @returns The calculated CRC16.
MaximInterface_EXPORT uint_fast16_t calculateCrc16(const uint_least8_t * data,
                                                   size_t dataLen,
                                                   uint_fast16_t crc = 0);

} // namespace MaximInterface

#endif
