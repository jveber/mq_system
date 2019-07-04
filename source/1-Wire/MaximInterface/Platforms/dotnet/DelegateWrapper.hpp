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

#include <msclr/marshal_cppstd.h>
#include <vcclr.h>

namespace MaximInterface {
namespace detail {

template <typename DelegateT> class DelegateWrapperBase {
public:
  using Delegate = DelegateT;

protected:
  DelegateWrapperBase(Delegate ^ target) : target_(target) {}
  ~DelegateWrapperBase() = default;

public:
  Delegate ^ target() const { return static_cast<Delegate ^>(target_); }
  void setTarget(Delegate ^ target) { target_ = target; }
  void swap(DelegateWrapperBase & other) { target_.swap(other.target_); }
  explicit operator bool() const { return target() != nullptr; }

private:
  gcroot<Delegate ^> target_;
};

template <typename DelegateT>
inline void swap(DelegateWrapperBase<DelegateT> & lhs,
                 DelegateWrapperBase<DelegateT> & rhs) {
  lhs.swap(rhs);
}

template <typename R, typename... Args>
class DelegateWrapperImpl
    : public DelegateWrapperBase<System::Func<Args..., R> > {
protected:
  DelegateWrapperImpl(Delegate ^ target) : DelegateWrapperBase(target) {}

public:
  R operator()(Args... args) { return target()(args...); }
};

template <typename... Args>
class DelegateWrapperImpl<void, Args...>
    : public DelegateWrapperBase<System::Action<Args...> > {
protected:
  DelegateWrapperImpl(Delegate ^ target) : DelegateWrapperBase(target) {}

public:
  void operator()(Args... args) const { target()(args...); }
};

} // namespace detail

/// @{
/// Functor wrapper for .NET delegates.
template <typename> class DelegateWrapper;

template <typename R, typename... Args>
class DelegateWrapper<R(Args...)>
    : public detail::DelegateWrapperImpl<R, Args...> {
public:
  DelegateWrapper(Delegate ^ target = nullptr) : DelegateWrapperImpl(target) {}
};

template <typename R, typename... Args>
class DelegateWrapper<R __clrcall(Args...)>
    : public detail::DelegateWrapperImpl<R, Args...> {
public:
  DelegateWrapper(Delegate ^ target = nullptr) : DelegateWrapperImpl(target) {}
};
/// @}

/// Wrapper for using a .NET delegate as a MaximInterface::WriteMessage.
class WriteMessageDelegateWrapper
    : public DelegateWrapper<void(System::String ^)> {
public:
  WriteMessageDelegateWrapper(Delegate ^ target = nullptr)
      : DelegateWrapper(target) {}

  void operator()(const std::string & arg) const {
    target()(msclr::interop::marshal_as<System::String ^>(arg));
  }
};

} // namespace MaximInterface