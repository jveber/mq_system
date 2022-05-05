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

#ifndef MaximInterfaceCore_Optional_hpp
#define MaximInterfaceCore_Optional_hpp

#include "None.hpp"
#include "SafeBool.hpp"

// Include for std::swap.
#include <algorithm>
#include <utility>

namespace MaximInterfaceCore {

/// @brief %Optional value container similar to std::optional.
/// @details
/// To prevent the need for aligned storage, this implementation imposes that
/// types must be DefaultConstructible, CopyConstructible, and CopyAssignable.
/// No exceptions are thrown when accessing a valueless Optional.
template <typename T> class Optional {
public:
  typedef T value_type;

  Optional() : value_(), hasValue_(false) {}

  Optional(None) : value_(), hasValue_(false) {}

  Optional(const T & value) : value_(value), hasValue_(true) {}

  template <typename U>
  explicit Optional(const Optional<U> & other)
      : value_(other.value()), hasValue_(other.hasValue()) {}

  Optional & operator=(None) {
    reset();
    return *this;
  }

  Optional & operator=(const T & value) {
    value_ = value;
    hasValue_ = true;
    return *this;
  }

  template <typename U> Optional & operator=(const Optional<U> & other) {
    assign(other);
    return *this;
  }

  Optional & operator=(const Optional & other) {
    assign(other);
    return *this;
  }

  bool hasValue() const { return hasValue_; }

  operator SafeBool() const { return makeSafeBool(hasValue()); }

  const T & value() const { return value_; }

  T & value() {
    return const_cast<T &>(static_cast<const Optional &>(*this).value());
  }

  const T & operator*() const { return value(); }

  T & operator*() {
    return const_cast<T &>(static_cast<const Optional &>(*this).operator*());
  }

  const T * operator->() const { return &value(); }

  T * operator->() {
    return const_cast<T *>(static_cast<const Optional &>(*this).operator->());
  }

  const T & valueOr(const T & defaultValue) const {
    return hasValue() ? value() : defaultValue;
  }

  void swap(Optional & other) {
    if (hasValue_ || other.hasValue_) {
      using std::swap;
      swap(value_, other.value_);
      swap(hasValue_, other.hasValue_);
    }
  }

  void reset() {
    if (hasValue_) {
      hasValue_ = false;
      value_ = T();
    }
  }

private:
  template <typename U> void assign(const Optional<U> & other) {
    if (hasValue_ || other.hasValue()) {
      value_ = other.value();
      hasValue_ = other.hasValue();
    }
  }

  T value_;
  bool hasValue_;
};

template <typename T> Optional<T> makeOptional(const T & value) {
  return value;
}

template <typename T> void swap(Optional<T> & lhs, Optional<T> & rhs) {
  lhs.swap(rhs);
}

template <typename T, typename U>
bool operator==(const Optional<T> & lhs, const Optional<U> & rhs) {
  if (lhs.hasValue() != rhs.hasValue()) {
    return false;
  }
  if (!lhs.hasValue()) {
    return true;
  }
  return lhs.value() == rhs.value();
}

template <typename T, typename U>
bool operator!=(const Optional<T> & lhs, const Optional<U> & rhs) {
  if (lhs.hasValue() != rhs.hasValue()) {
    return true;
  }
  if (!lhs.hasValue()) {
    return false;
  }
  return lhs.value() != rhs.value();
}

template <typename T, typename U>
bool operator<(const Optional<T> & lhs, const Optional<U> & rhs) {
  if (!rhs.hasValue()) {
    return false;
  }
  if (!lhs.hasValue()) {
    return true;
  }
  return lhs.value() < rhs.value();
}

template <typename T, typename U>
bool operator<=(const Optional<T> & lhs, const Optional<U> & rhs) {
  if (!lhs.hasValue()) {
    return true;
  }
  if (!rhs.hasValue()) {
    return false;
  }
  return lhs.value() <= rhs.value();
}

template <typename T, typename U>
bool operator>(const Optional<T> & lhs, const Optional<U> & rhs) {
  if (!lhs.hasValue()) {
    return false;
  }
  if (!rhs.hasValue()) {
    return true;
  }
  return lhs.value() > rhs.value();
}

template <typename T, typename U>
bool operator>=(const Optional<T> & lhs, const Optional<U> & rhs) {
  if (!rhs.hasValue()) {
    return true;
  }
  if (!lhs.hasValue()) {
    return false;
  }
  return lhs.value() >= rhs.value();
}

template <typename T> bool operator==(const Optional<T> & opt, None) {
  return !opt.hasValue();
}

template <typename T> bool operator==(None, const Optional<T> & opt) {
  return operator==(opt, none);
}

template <typename T> bool operator!=(const Optional<T> & opt, None) {
  return !operator==(opt, none);
}

template <typename T> bool operator!=(None, const Optional<T> & opt) {
  return operator!=(opt, none);
}

template <typename T> bool operator<(const Optional<T> &, None) {
  return false;
}

template <typename T> bool operator<(None, const Optional<T> & opt) {
  return opt.hasValue();
}

template <typename T> bool operator<=(const Optional<T> & opt, None) {
  return !operator>(opt, none);
}

template <typename T> bool operator<=(None, const Optional<T> & opt) {
  return !operator>(none, opt);
}

template <typename T> bool operator>(const Optional<T> & opt, None) {
  return operator<(none, opt);
}

template <typename T> bool operator>(None, const Optional<T> & opt) {
  return operator<(opt, none);
}

template <typename T> bool operator>=(const Optional<T> & opt, None) {
  return !operator<(opt, none);
}

template <typename T> bool operator>=(None, const Optional<T> & opt) {
  return !operator<(none, opt);
}

template <typename T, typename U>
bool operator==(const Optional<T> & opt, const U & value) {
  return opt.hasValue() ? opt.value() == value : false;
}

template <typename T, typename U>
bool operator==(const T & value, const Optional<U> & opt) {
  return operator==(opt, value);
}

template <typename T, typename U>
bool operator!=(const Optional<T> & opt, const U & value) {
  return opt.hasValue() ? opt.value() != value : true;
}

template <typename T, typename U>
bool operator!=(const T & value, const Optional<U> & opt) {
  return operator!=(opt, value);
}

template <typename T, typename U>
bool operator<(const Optional<T> & opt, const U & value) {
  return opt.hasValue() ? opt.value() < value : true;
}

template <typename T, typename U>
bool operator<(const T & value, const Optional<U> & opt) {
  return opt.hasValue() ? value < opt.value() : false;
}

template <typename T, typename U>
bool operator<=(const Optional<T> & opt, const U & value) {
  return opt.hasValue() ? opt.value() <= value : true;
}

template <typename T, typename U>
bool operator<=(const T & value, const Optional<U> & opt) {
  return opt.hasValue() ? value <= opt.value() : false;
}

template <typename T, typename U>
bool operator>(const Optional<T> & opt, const U & value) {
  return opt.hasValue() ? opt.value() > value : false;
}

template <typename T, typename U>
bool operator>(const T & value, const Optional<U> & opt) {
  return opt.hasValue() ? value > opt.value() : true;
}

template <typename T, typename U>
bool operator>=(const Optional<T> & opt, const U & value) {
  return opt.hasValue() ? opt.value() >= value : false;
}

template <typename T, typename U>
bool operator>=(const T & value, const Optional<U> & opt) {
  return opt.hasValue() ? value >= opt.value() : true;
}

} // namespace MaximInterfaceCore

#endif
