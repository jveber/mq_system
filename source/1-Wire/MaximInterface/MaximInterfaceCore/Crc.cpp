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

#include "Crc.hpp"

namespace MaximInterfaceCore {

uint_fast8_t calculateCrc8(uint_fast8_t crc, uint_fast8_t data) {
  // See Application Note 27
  crc = (crc ^ data) & 0xFF;
  for (int i = 0; i < 8; ++i) {
    if (crc & 1) {
      crc = (crc >> 1) ^ 0x8C;
    } else {
      crc = (crc >> 1);
    }
  }
  return crc;
}

uint_fast8_t calculateCrc8(uint_fast8_t crc, span<const uint_least8_t> data) {
  for (span<const uint_least8_t>::index_type i = 0; i < data.size(); ++i) {
    crc = calculateCrc8(crc, data[i]);
  }
  return crc;
}

uint_fast16_t calculateCrc16(uint_fast16_t crc, uint_fast8_t data) {
  static const uint_least8_t oddparity[] = {0, 1, 1, 0, 1, 0, 0, 1,
                                            1, 0, 0, 1, 0, 1, 1, 0};

  uint_fast16_t data16 = (data ^ crc) & 0xFF;
  crc = (crc >> 8) & 0xFF;
  if (oddparity[data16 & 0xF] ^ oddparity[data16 >> 4]) {
    crc ^= 0xC001;
  }
  data16 <<= 6;
  crc ^= data16;
  data16 <<= 1;
  crc ^= data16;
  return crc;
}

uint_fast16_t calculateCrc16(uint_fast16_t crc,
                             span<const uint_least8_t> data) {
  for (span<const uint_least8_t>::index_type i = 0; i < data.size(); ++i) {
    crc = calculateCrc16(crc, data[i]);
  }
  return crc;
}

} // namespace MaximInterfaceCore
