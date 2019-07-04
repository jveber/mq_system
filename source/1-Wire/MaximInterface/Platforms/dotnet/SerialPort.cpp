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

#include <msclr/auto_gcroot.h>
#include <msclr/marshal_cppstd.h>
#include <MaximInterface/Utilities/Error.hpp>
#include "ChangeSizeType.hpp"
#include "Sleep.hpp"
#include "SerialPort.hpp"

using msclr::interop::marshal_as;
using std::string;
using namespace System;
using System::Runtime::InteropServices::Marshal;

namespace MaximInterface {
namespace dotnet {

template <typename Type, typename Input> static bool isType(Input in) {
  return dynamic_cast<Type>(in) != nullptr;
}

template <typename Func>
static error_code executeTryCatchOperation(Func tryOperation) {
  return executeTryCatchOperation(tryOperation, [] {});
}

template <typename TryFunc, typename CatchFunc>
static error_code executeTryCatchOperation(TryFunc tryOperation,
                                           CatchFunc catchOperation) {
  error_code result;
  try {
    tryOperation();
  } catch (Exception ^ e) {
    catchOperation();
    if (isType<ArgumentException ^>(e)) {
      result = make_error_code(SerialPort::ArgumentError);
    } else if (isType<InvalidOperationException ^>(e)) {
      result = make_error_code(SerialPort::InvalidOperationError);
    } else if (isType<System::IO::IOException ^>(e)) {
      result = make_error_code(SerialPort::IOError);
    } else if (isType<UnauthorizedAccessException ^>(e)) {
      result = make_error_code(SerialPort::UnauthorizedAccessError);
    } else if (isType<TimeoutException ^>(e)) {
      result = make_error_code(SerialPort::TimeoutError);
    } else {
      throw;
    }
  }
  return result;
}

template <typename Func>
static error_code executeIfConnected(const SerialPort & serial,
                                     Func operation) {
  return serial.connected() ? operation()
                            : make_error_code(SerialPort::NotConnectedError);
}

struct SerialPort::Data {
  msclr::auto_gcroot<System::IO::Ports::SerialPort ^> port;
};

SerialPort::SerialPort() : data(std::make_unique<Data>()) {}

SerialPort::~SerialPort() = default;

SerialPort::SerialPort(SerialPort &&) noexcept = default;

SerialPort & SerialPort::operator=(SerialPort &&) noexcept = default;

error_code SerialPort::connect(const string & portName) {
  data->port = gcnew System::IO::Ports::SerialPort;
  return executeTryCatchOperation(
      [this, &portName] {
        data->port->PortName = marshal_as<String ^>(portName);
        data->port->DtrEnable = true;
        data->port->ReadTimeout = 1000;
        data->port->WriteTimeout = 1000;
        data->port->Open();
      },
      [this] { data->port.reset(); });
}

error_code SerialPort::disconnect() {
  data->port.reset();
  return error_code();
}

bool SerialPort::connected() const { return data->port.get() != nullptr; }

string SerialPort::portName() const {
  return connected() ? marshal_as<string>(data->port->PortName) : "";
}

error_code SerialPort::setBaudRate(int_least32_t baudRate) {
  return executeIfConnected(*this, [this, baudRate] {
    return executeTryCatchOperation(
        [this, baudRate] { data->port->BaudRate = baudRate; });
  });
}

error_code SerialPort::sendBreak() {
  return executeIfConnected(*this, [this] {
    return executeTryCatchOperation([this] {
      data->port->BreakState = true;
      sleep(1);
      data->port->BreakState = false;
    });
  });
}

error_code SerialPort::clearReadBuffer() {
  return executeIfConnected(*this, [this] {
    return executeTryCatchOperation([this] { data->port->ReadExisting(); });
  });
}

error_code SerialPort::writeByte(uint_least8_t data) {
  return writeBlock(&data, 1);
}

error_code SerialPort::writeBlock(const uint_least8_t * data, size_t dataLen) {
  return executeIfConnected(*this, [this, data, dataLen] {
    return executeTryCatchOperation([this, data, dataLen] {
      changeSizeType<int>(
          [this](const uint_least8_t * dataChunk, int dataChunkSize) {
            auto dataManaged = gcnew array<uint_least8_t>(dataChunkSize);
            Marshal::Copy(
                static_cast<IntPtr>(const_cast<uint_least8_t *>(dataChunk)),
                dataManaged, 0, dataChunkSize);
            this->data->port->Write(dataManaged, 0, dataChunkSize);
          },
          data, dataLen);
    });
  });
}

error_code SerialPort::readByte(uint_least8_t & data) {
  return executeIfConnected(*this, [this, &data] {
    return executeTryCatchOperation(
        [this, &data] { data = this->data->port->ReadByte(); });
  });
}

error_code SerialPort::readBlock(uint_least8_t * data, size_t dataLen) {
  return executeIfConnected(*this, [this, data, dataLen] {
    return executeTryCatchOperation([this, data, dataLen] {
      changeSizeType<int>(
          [this](uint_least8_t * dataChunk, int dataChunkSize) {
            auto dataManaged = gcnew array<uint_least8_t>(dataChunkSize);
            int bytesRead = 0;
            do {
              bytesRead += this->data->port->Read(dataManaged, bytesRead,
                                                  dataChunkSize - bytesRead);
            } while (bytesRead < dataChunkSize);
            Marshal::Copy(dataManaged, 0, static_cast<IntPtr>(dataChunk),
                          dataChunkSize);
          },
          data, dataLen);
    });
  });
}

const error_category & SerialPort::errorCategory() {
  static class : public error_category {
  public:
    virtual const char * name() const override { return "dotnet SerialPort"; }

    virtual string message(int condition) const override {
      switch (condition) {
      case NotConnectedError:
        return "Not Connected Error";

      case ArgumentError:
        return "Argument Error";

      case InvalidOperationError:
        return "Invalid Operation Error";

      case IOError:
        return "IO Error";

      case UnauthorizedAccessError:
        return "Unauthorized Access Error";

      default:
        return defaultErrorMessage(condition);
      }
    }
  } instance;
  return instance;
}

} // namespace dotnet
} // namespace MaximInterface