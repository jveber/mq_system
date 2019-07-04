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

#include <stdexcept>
#include <msclr/auto_gcroot.h>
#include <msclr/marshal_cppstd.h>
#include <MaximInterface/Utilities/Error.hpp>
#include "ChangeSizeType.hpp"
#include "OneWireLinkLayerMaster.hpp"

using DalSemi::OneWire::Adapter::AdapterException;
using System::IntPtr;
using System::Runtime::InteropServices::Marshal;
using System::String;
using msclr::interop::marshal_as;
using std::string;

namespace MaximInterface {

struct OneWireLinkLayerMaster::Data {
  msclr::auto_gcroot<DalSemi::OneWire::Adapter::PortAdapter ^> adapter;
  bool connected = false;
};

OneWireLinkLayerMaster::OneWireLinkLayerMaster(const string & adapterName)
    : data(std::make_unique<Data>()) {
  try {
    data->adapter = DalSemi::OneWire::AccessProvider::GetAdapter(
        marshal_as<String ^>(adapterName));
  } catch (AdapterException ^ e) {
    throw std::invalid_argument(marshal_as<string>(e->Message));
  }
}

OneWireLinkLayerMaster::~OneWireLinkLayerMaster() = default;

OneWireLinkLayerMaster::OneWireLinkLayerMaster(
    OneWireLinkLayerMaster && rhs) noexcept = default;

OneWireLinkLayerMaster & OneWireLinkLayerMaster::
operator=(OneWireLinkLayerMaster && rhs) noexcept = default;

error_code OneWireLinkLayerMaster::connect(const string & portName) {
  if (connected()) {
    return make_error_code(AlreadyConnectedError);
  }

  error_code result;
  try {
    auto adapterResult =
        data->adapter->OpenPort(marshal_as<String ^>(portName));
    if (adapterResult) {
      adapterResult = data->adapter->BeginExclusive(true);
      if (adapterResult) {
        data->connected = true;
      } else {
        data->adapter->FreePort();
        result = make_error_code(OpenPortError);
      }
    } else {
      result = make_error_code(OpenPortError);
    }
  } catch (AdapterException ^) {
    result = make_error_code(CommunicationError);
  }
  return result;
}

void OneWireLinkLayerMaster::disconnect() {
  data->adapter->EndExclusive();
  data->adapter->FreePort();
  data->connected = false;
}

bool OneWireLinkLayerMaster::connected() const { return data->connected; }

string OneWireLinkLayerMaster::adapterName() const {
  return marshal_as<string>(data->adapter->AdapterName);
}

string OneWireLinkLayerMaster::portName() const {
  return connected() ? marshal_as<string>(data->adapter->PortName) : "";
}

error_code OneWireLinkLayerMaster::reset() {
  using DalSemi::OneWire::Adapter::OWResetResult;

  error_code result;
  try {
    switch (data->adapter->Reset()) {
    case OWResetResult::RESET_SHORT:
      result = make_error_code(ShortDetectedError);
      break;

    case OWResetResult::RESET_NOPRESENCE:
      result = make_error_code(NoSlaveError);
      break;
    }
  } catch (AdapterException ^) {
    result = make_error_code(CommunicationError);
  }
  return result;
}

error_code OneWireLinkLayerMaster::touchBitSetLevel(bool & sendRecvBit,
                                                    Level afterLevel) {
  error_code result;
  try {
    switch (afterLevel) {
    case StrongLevel: {
      auto adapterResult = data->adapter->StartPowerDelivery(
          DalSemi::OneWire::Adapter::OWPowerStart::CONDITION_AFTER_BIT);
      if (!adapterResult) {
        result = make_error_code(PowerDeliveryError);
      }
    } break;

    case NormalLevel:
      break;

    default:
      result = make_error_code(InvalidLevelError);
      break;
    }
    if (!result) {
      if (sendRecvBit) {
        sendRecvBit = data->adapter->GetBit();
      } else {
        data->adapter->PutBit(0);
      }
    }
  } catch (AdapterException ^) {
    result = make_error_code(CommunicationError);
  }
  return result;
}

error_code OneWireLinkLayerMaster::writeByteSetLevel(uint_least8_t sendByte,
                                                     Level afterLevel) {
  error_code result;
  try {
    switch (afterLevel) {
    case StrongLevel: {
      auto adapterResult = data->adapter->StartPowerDelivery(
          DalSemi::OneWire::Adapter::OWPowerStart::CONDITION_AFTER_BYTE);
      if (!adapterResult) {
        result = make_error_code(PowerDeliveryError);
      }
    } break;

    case NormalLevel:
      break;

    default:
      result = make_error_code(InvalidLevelError);
      break;
    }
    if (!result) {
      data->adapter->PutByte(sendByte);
    }
  } catch (AdapterException ^) {
    result = make_error_code(CommunicationError);
  }
  return result;
}

error_code OneWireLinkLayerMaster::readByteSetLevel(uint_least8_t & recvByte,
                                                    Level afterLevel) {
  error_code result;
  try {
    switch (afterLevel) {
    case StrongLevel: {
      auto adapterResult = data->adapter->StartPowerDelivery(
          DalSemi::OneWire::Adapter::OWPowerStart::CONDITION_AFTER_BYTE);
      if (!adapterResult) {
        result = make_error_code(PowerDeliveryError);
      }
    } break;

    case NormalLevel:
      break;

    default:
      result = make_error_code(InvalidLevelError);
      break;
    }
    if (!result) {
      recvByte = data->adapter->GetByte();
    }
  } catch (AdapterException ^) {
    result = make_error_code(CommunicationError);
  }
  return result;
}

error_code OneWireLinkLayerMaster::writeBlock(const uint_least8_t * sendBuf,
                                              size_t sendLen) {
  error_code result;
  try {
    changeSizeType<int>(
        [this](const uint_least8_t * data, int dataSize) {
          auto dataManaged = gcnew array<uint_least8_t>(dataSize);
          Marshal::Copy(static_cast<IntPtr>(const_cast<uint_least8_t *>(data)),
                        dataManaged, 0, dataSize);
          this->data->adapter->DataBlock(dataManaged, 0, dataSize);
        },
        sendBuf, sendLen);
  } catch (AdapterException ^) {
    result = make_error_code(CommunicationError);
  }
  return result;
}

error_code OneWireLinkLayerMaster::readBlock(uint_least8_t * recvBuf,
                                             size_t recvLen) {
  error_code result;
  try {
    changeSizeType<int>(
        [this](uint_least8_t * data, int dataSize) {
          auto dataManaged = gcnew array<uint_least8_t>(dataSize);
          this->data->adapter->GetBlock(dataManaged, dataSize);
          Marshal::Copy(dataManaged, 0, static_cast<IntPtr>(data), dataSize);
        },
        recvBuf, recvLen);
  } catch (AdapterException ^) {
    result = make_error_code(CommunicationError);
  }
  return result;
}

error_code OneWireLinkLayerMaster::setSpeed(Speed newSpeed) {
  using DalSemi::OneWire::Adapter::OWSpeed;

  error_code result;
  try {
    switch (newSpeed) {
    case OverdriveSpeed:
      data->adapter->Speed = OWSpeed::SPEED_OVERDRIVE;
      break;

    case StandardSpeed:
      data->adapter->Speed = OWSpeed::SPEED_REGULAR;
      break;

    default:
      result = make_error_code(InvalidSpeedError);
      break;
    }
  } catch (AdapterException ^) {
    result = make_error_code(CommunicationError);
  }
  return result;
}

error_code OneWireLinkLayerMaster::setLevel(Level newLevel) {
  error_code result;
  try {
    switch (newLevel) {
    case StrongLevel: {
      auto setResult = data->adapter->StartPowerDelivery(
          DalSemi::OneWire::Adapter::OWPowerStart::CONDITION_NOW);
      if (!setResult) {
        result = make_error_code(PowerDeliveryError);
      }
    } break;

    case NormalLevel:
      data->adapter->SetPowerNormal();
      break;

    default:
      result = make_error_code(InvalidLevelError);
      break;
    }
  } catch (AdapterException ^) {
    result = make_error_code(CommunicationError);
  }
  return result;
}

const error_category & OneWireLinkLayerMaster::errorCategory() {
  static class : public error_category {
  public:
    virtual const char * name() const { return "OneWireLinkLayerMaster"; }

    virtual string message(int condition) const {
      switch (condition) {
      case CommunicationError:
        return "Communication Error";

      case OpenPortError:
        return "Open Port Error";

      case PowerDeliveryError:
        return "Power Delivery Error";

      case AlreadyConnectedError:
        return "Already Connected Error";

      default:
        return defaultErrorMessage(condition);
      }
    }
  } instance;
  return instance;
}

} // namespace MaximInterface