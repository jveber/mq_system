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

#pragma once

#include <memory>
#include <string>
#include <MaximInterface/Links/OneWireMaster.hpp>
#include <MaximInterface/Utilities/Export.h>
#include "MoveOnly.hpp"

namespace MaximInterface {

/// 1-Wire interface using the OneWireLinkLayer DLL from the Compact.NET API.
class OneWireLinkLayerMaster : public OneWireMaster, private MoveOnly {
public:
  enum ErrorValue {
    CommunicationError = 1,
    OpenPortError,
    PowerDeliveryError,
    AlreadyConnectedError
  };

  /// @param adapterName Adapter type name recogized by OneWireLinkLayer.
  MaximInterface_EXPORT OneWireLinkLayerMaster(const std::string & adapterName);
  MaximInterface_EXPORT ~OneWireLinkLayerMaster();

  MaximInterface_EXPORT
  OneWireLinkLayerMaster(OneWireLinkLayerMaster &&) noexcept;
  MaximInterface_EXPORT OneWireLinkLayerMaster &
  operator=(OneWireLinkLayerMaster &&) noexcept;

  /// Connect to an adapter on the specified COM port.
  MaximInterface_EXPORT error_code connect(const std::string & portName);
  
  /// Disconnect from the adapter on the current COM port.
  MaximInterface_EXPORT void disconnect();

  /// Check if currently connected to an adapter.
  /// @returns True if connected.
  MaximInterface_EXPORT bool connected() const;
  
  /// Get the adapter type name.
  MaximInterface_EXPORT std::string adapterName() const;
  
  /// Get the currently connected COM port name.
  MaximInterface_EXPORT std::string portName() const;

  MaximInterface_EXPORT virtual error_code reset() override;
  MaximInterface_EXPORT virtual error_code
  touchBitSetLevel(bool & sendRecvBit, Level afterLevel) override;
  MaximInterface_EXPORT virtual error_code
  writeByteSetLevel(uint_least8_t sendByte, Level afterLevel) override;
  MaximInterface_EXPORT virtual error_code
  readByteSetLevel(uint_least8_t & recvByte, Level afterLevel) override;
  MaximInterface_EXPORT virtual error_code
  writeBlock(const uint_least8_t * sendBuf, size_t sendLen) override;
  MaximInterface_EXPORT virtual error_code readBlock(uint_least8_t * recvBuf,
                                                     size_t recvLen) override;
  MaximInterface_EXPORT virtual error_code setSpeed(Speed newSpeed) override;
  MaximInterface_EXPORT virtual error_code setLevel(Level newLevel) override;

  MaximInterface_EXPORT static const error_category & errorCategory();

private:
  struct Data;
  std::unique_ptr<Data> data;
};

inline error_code make_error_code(OneWireLinkLayerMaster::ErrorValue e) {
  return error_code(e, OneWireLinkLayerMaster::errorCategory());
}

} // namespace MaximInterface