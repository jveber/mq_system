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

/// @file
/// @brief
/// Error handling constructs similar to std::error_code, std::error_condition,
/// and std::error_category.

#ifndef MaximInterfaceCore_system_error_hpp
#define MaximInterfaceCore_system_error_hpp

#include <stddef.h>
#include <stdexcept>
#include <string>
#include "Config.hpp"
#include "SafeBool.hpp"
#include "type_traits.hpp"
#include "Uncopyable.hpp"

namespace MaximInterfaceCore {

class error_condition;
class error_code;

/// Error category interface.
class error_category : private Uncopyable {
public:
  virtual ~error_category() {}

  virtual const char * name() const = 0;

  MaximInterfaceCore_EXPORT virtual error_condition
  default_error_condition(int code) const;

  MaximInterfaceCore_EXPORT virtual bool
  equivalent(int code, const error_condition & condition) const;

  MaximInterfaceCore_EXPORT virtual bool equivalent(const error_code & code,
                                                    int condition) const;

  virtual std::string message(int condition) const = 0;
};

inline bool operator==(const error_category & lhs, const error_category & rhs) {
  return &lhs == &rhs;
}

inline bool operator!=(const error_category & lhs, const error_category & rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator<(const error_category & lhs, const error_category & rhs) {
  return &lhs < &rhs;
}

/// Default error category.
MaximInterfaceCore_EXPORT const error_category & system_category();

/// Checks if an enum type is implicitly convertible to an error_condition.
template <typename T> struct is_error_condition_enum : false_type {};

/// @brief
/// Used for classifying groups of error_code into higher-level error conditions
/// through the error_category::equivalent methods.
class error_condition {
public:
  error_condition() : value_(0), category_(&system_category()) {}

  error_condition(int value, const error_category & category)
      : value_(value), category_(&category) {}

  template <typename ErrorConditionEnum>
  error_condition(
      ErrorConditionEnum e,
      typename enable_if<
          is_error_condition_enum<ErrorConditionEnum>::value>::type * = NULL) {
    *this = make_error_condition(e);
  }

  template <typename ErrorConditionEnum>
  typename enable_if<is_error_condition_enum<ErrorConditionEnum>::value,
                     error_condition &>::type
  operator=(ErrorConditionEnum e) {
    *this = make_error_condition(e);
    return *this;
  }

  void assign(int value, const error_category & category) {
    value_ = value;
    category_ = &category;
  }

  void clear() {
    value_ = 0;
    category_ = &system_category();
  }

  int value() const { return value_; }

  const error_category & category() const { return *category_; }

  std::string message() const { return category().message(value()); }

  operator SafeBool() const { return makeSafeBool(value() != 0); }

private:
  int value_;
  const error_category * category_;
};

inline bool operator==(const error_condition & lhs,
                       const error_condition & rhs) {
  return (lhs.value() == rhs.value()) && (lhs.category() == rhs.category());
}

inline bool operator!=(const error_condition & lhs,
                       const error_condition & rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator<(const error_condition & lhs,
                      const error_condition & rhs) {
  return (lhs.category() < rhs.category()) ||
         ((lhs.category() == rhs.category()) && (lhs.value() < rhs.value()));
}

/// Checks if an enum type is implicitly convertible to an error_code.
template <typename T> struct is_error_code_enum : false_type {};

/// @brief Holds a raw error code produced by a subsystem.
/// @details
/// An error_code is composed of a pair of values: a category of errors, usually
/// one per subsystem, and a number representing a specific error value within
/// that category. While not required, zero is typically used as the success
/// value so that the boolean conversion can be used as a failure test.
class error_code {
public:
  error_code() : value_(0), category_(&system_category()) {}

  error_code(int value, const error_category & category)
      : value_(value), category_(&category) {}

  template <typename ErrorCodeEnum>
  error_code(
      ErrorCodeEnum e,
      typename enable_if<is_error_code_enum<ErrorCodeEnum>::value>::type * =
          NULL) {
    *this = make_error_code(e);
  }

  template <typename ErrorCodeEnum>
  typename enable_if<is_error_code_enum<ErrorCodeEnum>::value,
                     error_code &>::type
  operator=(ErrorCodeEnum e) {
    *this = make_error_code(e);
    return *this;
  }

  void assign(int value, const error_category & category) {
    value_ = value;
    category_ = &category;
  }

  void clear() {
    value_ = 0;
    category_ = &system_category();
  }

  int value() const { return value_; }

  const error_category & category() const { return *category_; }

  error_condition default_error_condition() const {
    return category().default_error_condition(value());
  }

  std::string message() const { return category().message(value()); }

  operator SafeBool() const { return makeSafeBool(value() != 0); }

private:
  int value_;
  const error_category * category_;
};

inline bool operator==(const error_code & lhs, const error_code & rhs) {
  return (lhs.value() == rhs.value()) && (lhs.category() == rhs.category());
}

inline bool operator!=(const error_code & lhs, const error_code & rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator<(const error_code & lhs, const error_code & rhs) {
  return (lhs.category() < rhs.category()) ||
         ((lhs.category() == rhs.category()) && (lhs.value() < rhs.value()));
}

template <typename CharT, typename Traits>
std::basic_ostream<CharT, Traits> &
operator<<(std::basic_ostream<CharT, Traits> & os, const error_code & ec) {
  os << ec.category().name() << ':' << ec.value();
  return os;
}

inline bool operator==(const error_code & lhs, const error_condition & rhs) {
  return lhs.category().equivalent(lhs.value(), rhs) ||
         rhs.category().equivalent(lhs, rhs.value());
}

inline bool operator!=(const error_code & lhs, const error_condition & rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator==(const error_condition & lhs, const error_code & rhs) {
  return operator==(rhs, lhs);
}

inline bool operator!=(const error_condition & lhs, const error_code & rhs) {
  return !operator==(lhs, rhs);
}

/// Wrapper for throwing error_code as an exception.
class system_error : public std::runtime_error {
public:
  MaximInterfaceCore_EXPORT system_error(const error_code & ec);

  MaximInterfaceCore_EXPORT system_error(const error_code & ec,
                                         const std::string & what_arg);

  MaximInterfaceCore_EXPORT system_error(const error_code & ec,
                                         const char * what_arg);

  MaximInterfaceCore_EXPORT system_error(int ev, const error_category & ecat);

  MaximInterfaceCore_EXPORT system_error(int ev, const error_category & ecat,
                                         const std::string & what_arg);

  MaximInterfaceCore_EXPORT system_error(int ev, const error_category & ecat,
                                         const char * what_arg);

  const error_code & code() const { return code_; }

private:
  error_code code_;
};

} // namespace MaximInterfaceCore

#endif
