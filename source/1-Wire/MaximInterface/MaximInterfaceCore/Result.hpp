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

#ifndef MaximInterfaceCore_Result_hpp
#define MaximInterfaceCore_Result_hpp

#include "None.hpp"
#include "Optional.hpp"
#include "SafeBool.hpp"
#include "system_error.hpp"

// Include for std::swap.
#include <algorithm>
#include <utility>

namespace MaximInterfaceCore {

/// @brief Expected value container that holds either a value or an error.
/// @details
/// Result can be used to multiplex errors and values returned from functions
/// that may fail as an alternative to exceptions.
/// @tparam T
/// Must be void or meet the requirements for DefaultConstructible,
/// CopyConstructible, and CopyAssignable.
template <typename T> class Result {
public:
  typedef T value_type;

  Result() : value_(), error_(), hasValue_(false) {}

  Result(const T & value) : value_(value), error_(), hasValue_(true) {}

  Result(const error_code & error)
      : value_(), error_(error), hasValue_(false) {}

  template <typename ErrorCodeEnum>
  Result(ErrorCodeEnum error,
         typename enable_if<is_error_code_enum<ErrorCodeEnum>::value>::type * =
             NULL)
      : value_(), error_(error), hasValue_(false) {}

  template <typename U>
  explicit Result(const Result<U> & other)
      : value_(other.value()), error_(other.error()),
        hasValue_(other.hasValue()) {}

  Result & operator=(const T & value) {
    value_ = value;
    error_.clear();
    hasValue_ = true;
    return *this;
  }

  Result & operator=(const error_code & error) {
    error_ = error;
    clearValue();
    return *this;
  }

  template <typename ErrorCodeEnum>
  typename enable_if<is_error_code_enum<ErrorCodeEnum>::value, Result &>::type
  operator=(ErrorCodeEnum error) {
    error_ = error;
    clearValue();
    return *this;
  }

  template <typename U> Result & operator=(const Result<U> & other) {
    assign(other);
    return *this;
  }

  Result & operator=(const Result & other) {
    assign(other);
    return *this;
  }

  bool hasValue() const { return hasValue_; }

  operator SafeBool() const { return makeSafeBool(hasValue()); }

  const T & value() const { return value_; }

  T & value() {
    return const_cast<T &>(static_cast<const Result &>(*this).value());
  }

  const error_code & error() const { return error_; }

  error_code & error() {
    return const_cast<error_code &>(static_cast<const Result &>(*this).error());
  }

  void swap(Result & other) {
    using std::swap;
    if (hasValue_ || other.hasValue_) {
      swap(value_, other.value_);
      swap(hasValue_, other.hasValue_);
    }
    swap(error_, other.error_);
  }

private:
  void clearValue() {
    if (hasValue_) {
      hasValue_ = false;
      value_ = T();
    }
  }

  template <typename U> void assign(const Result<U> & other) {
    if (hasValue_ || other.hasValue()) {
      value_ = other.value();
      hasValue_ = other.hasValue();
    }
    error_ = other.error();
  }

  T value_;
  error_code error_;
  bool hasValue_;
};

template <typename T> Result<T> makeResult(const T & value) { return value; }

template <typename T> void swap(Result<T> & lhs, Result<T> & rhs) {
  lhs.swap(rhs);
}

template <typename T, typename U>
bool operator==(const Result<T> & lhs, const Result<U> & rhs) {
  if (lhs.hasValue() != rhs.hasValue()) {
    return false;
  }
  if (!lhs.hasValue()) {
    return lhs.error() == rhs.error();
  }
  return lhs.value() == rhs.value();
}

template <typename T, typename U>
bool operator!=(const Result<T> & lhs, const Result<U> & rhs) {
  if (lhs.hasValue() != rhs.hasValue()) {
    return true;
  }
  if (!lhs.hasValue()) {
    return lhs.error() != rhs.error();
  }
  return lhs.value() != rhs.value();
}

template <typename T, typename U>
typename enable_if<!is_error_code_enum<U>::value, bool>::type
operator==(const Result<T> & exp, const U & value) {
  return exp.hasValue() ? exp.value() == value : false;
}

template <typename T, typename U>
typename enable_if<!is_error_code_enum<T>::value, bool>::type
operator==(const T & value, const Result<U> & exp) {
  return operator==(exp, value);
}

template <typename T, typename U>
typename enable_if<!is_error_code_enum<U>::value, bool>::type
operator!=(const Result<T> & exp, const U & value) {
  return exp.hasValue() ? exp.value() != value : true;
}

template <typename T, typename U>
typename enable_if<!is_error_code_enum<T>::value, bool>::type
operator!=(const T & value, const Result<U> & exp) {
  return operator!=(exp, value);
}

template <typename T>
bool operator==(const Result<T> & exp, const error_code & error) {
  return exp.hasValue() ? false : exp.error() == error;
}

template <typename T>
bool operator==(const error_code & error, const Result<T> & exp) {
  return operator==(exp, error);
}

template <typename T>
bool operator!=(const Result<T> & exp, const error_code & error) {
  return exp.hasValue() ? true : exp.error() != error;
}

template <typename T>
bool operator!=(const error_code & error, const Result<T> & exp) {
  return operator!=(exp, error);
}

// Specialization for void.
template <> class Result<void> {
public:
  typedef void value_type;

  Result() : error_(), hasValue_(false) {}

  Result(None) : error_(), hasValue_(true) {}

  Result(const error_code & error) : error_(error), hasValue_(false) {}

  template <typename ErrorCodeEnum>
  Result(ErrorCodeEnum error,
         typename enable_if<is_error_code_enum<ErrorCodeEnum>::value>::type * =
             NULL)
      : error_(error), hasValue_(false) {}

  bool hasValue() const { return hasValue_; }

  operator SafeBool() const { return makeSafeBool(hasValue()); }

  void value() const {}

  const error_code & error() const { return error_; }

  error_code & error() {
    return const_cast<error_code &>(static_cast<const Result &>(*this).error());
  }

  void swap(Result & other) {
    using std::swap;
    swap(hasValue_, other.hasValue_);
    swap(error_, other.error_);
  }

private:
  error_code error_;
  bool hasValue_;
};

inline Result<void> makeResult(None) { return none; }

template <>
inline bool operator==(const Result<void> & lhs, const Result<void> & rhs) {
  if (lhs.hasValue() != rhs.hasValue()) {
    return false;
  }
  if (!lhs.hasValue()) {
    return lhs.error() == rhs.error();
  }
  return true;
}

template <>
inline bool operator!=(const Result<void> & lhs, const Result<void> & rhs) {
  if (lhs.hasValue() != rhs.hasValue()) {
    return true;
  }
  if (!lhs.hasValue()) {
    return lhs.error() != rhs.error();
  }
  return false;
}

template <> inline bool operator==(const Result<void> & exp, const None &) {
  return exp.hasValue();
}

template <> inline bool operator==(const None &, const Result<void> & exp) {
  return operator==(exp, none);
}

template <> inline bool operator!=(const Result<void> & exp, const None &) {
  return !operator==(exp, none);
}

template <> inline bool operator!=(const None &, const Result<void> & exp) {
  return operator!=(exp, none);
}

namespace detail {

template <typename T> Optional<error_code> TRY_helper(const Result<T> & expr) {
  Optional<error_code> error;
  if (!expr) {
    error = expr.error();
  }
  return error;
}

template <typename T, typename U>
Optional<error_code> TRY_VALUE_helper(T & var, const Result<U> & expr) {
  Optional<error_code> error;
  if (expr) {
    var = expr.value();
  } else {
    error = expr.error();
  }
  return error;
}

} // namespace detail
} // namespace MaximInterfaceCore

// clang-format off

/// @brief
/// Evaluates an expression returning a Result type by continuing execution if
/// successful or returning the error to the calling function if unsuccessful.
#define MaximInterfaceCore_TRY(expr)                                           \
  if (const ::MaximInterfaceCore::Optional< ::MaximInterfaceCore::error_code>  \
          MaximInterfaceCore_TRY_error =                                       \
              ::MaximInterfaceCore::detail::TRY_helper(expr))                  \
    return MaximInterfaceCore_TRY_error.value()

/// @brief
/// Evaluates an expression returning a non-void Result type by assigning the
/// value to the specified variable if successful or returning the error to the
/// calling function if unsuccessful.
#define MaximInterfaceCore_TRY_VALUE(var, expr)                                \
  if (const ::MaximInterfaceCore::Optional< ::MaximInterfaceCore::error_code>  \
          MaximInterfaceCore_TRY_VALUE_error =                                 \
              ::MaximInterfaceCore::detail::TRY_VALUE_helper(var, expr))       \
    return MaximInterfaceCore_TRY_VALUE_error.value()

#endif
