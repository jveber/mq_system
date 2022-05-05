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

#ifndef MaximInterfaceCore_OneWireMasterDecorator_hpp
#define MaximInterfaceCore_OneWireMasterDecorator_hpp

#include "Config.hpp"
#include "OneWireMaster.hpp"

namespace MaximInterfaceCore {

class OneWireMasterDecorator : public OneWireMaster {
protected:
  explicit OneWireMasterDecorator(OneWireMaster & master) : master(&master) {}

public:
  void setMaster(OneWireMaster & master) { this->master = &master; }

  MaximInterfaceCore_EXPORT virtual Result<void> reset();

  MaximInterfaceCore_EXPORT virtual Result<bool>
  touchBitSetLevel(bool sendBit, Level afterLevel);

  MaximInterfaceCore_EXPORT virtual Result<void>
  writeByteSetLevel(uint_least8_t sendByte, Level afterLevel);

  MaximInterfaceCore_EXPORT virtual Result<uint_least8_t>
  readByteSetLevel(Level afterLevel);

  MaximInterfaceCore_EXPORT virtual Result<void>
  writeBlock(span<const uint_least8_t> sendBuf);

  MaximInterfaceCore_EXPORT virtual Result<void>
  readBlock(span<uint_least8_t> recvBuf);

  MaximInterfaceCore_EXPORT virtual Result<void> setSpeed(Speed newSpeed);

  MaximInterfaceCore_EXPORT virtual Result<void> setLevel(Level newLevel);

  MaximInterfaceCore_EXPORT virtual Result<TripletData> triplet(bool sendBit);

private:
  OneWireMaster * master;
};

} // namespace MaximInterfaceCore

#endif
