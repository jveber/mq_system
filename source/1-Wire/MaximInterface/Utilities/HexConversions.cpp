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

#include <cstdio>
#include "HexConversions.hpp"

namespace MaximInterface {

static const unsigned int charsPerByte = 2;

std::string byteArrayToHexString(const uint_least8_t * byteArray,
                                 size_t byteArraySize) {
  std::string hexString;
  char hexBuf[charsPerByte + 1];
  for (size_t i = 0; i < byteArraySize; i++) {
    std::sprintf(hexBuf, "%2.2X", byteArray[i] & 0xFF);
    hexString.append(hexBuf, charsPerByte);
  }
  return hexString;
}

std::vector<uint_least8_t> hexStringToByteArray(const std::string & hexString) {
  std::vector<uint_least8_t> byteArray;
  byteArray.reserve(hexString.size() / charsPerByte + 1);
  for (std::string::size_type i = 0; i < hexString.size(); i += charsPerByte) {
    unsigned int byte;
    std::sscanf(hexString.substr(i, charsPerByte).c_str(), "%2x", &byte);
    byteArray.push_back(byte);
  }
  return byteArray;
}

} // namespace MaximInterface
