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

#ifndef MaximInterfaceCore_span_hpp
#define MaximInterfaceCore_span_hpp

#include <stddef.h>
#include <iterator>
#include <vector>
#include "array.hpp"
#include "type_traits.hpp"

namespace MaximInterfaceCore {

/// Differentiates spans of static and dynamic extent.
static const size_t dynamic_extent = static_cast<size_t>(-1); // SIZE_MAX

/// @brief
/// Generic memory span class similar to gsl::span and the proposed std::span.
/// @note
/// Separate implementations are used for spans of static and dynamic extent.
template <typename T, size_t Extent = dynamic_extent> class span;

namespace detail {

// Implementation of common span functionality using CRTP.
template <template <typename, size_t = MaximInterfaceCore::dynamic_extent>
          class span,
          typename T, size_t Extent>
class span_base {
public:
  typedef T element_type;
  typedef typename remove_cv<element_type>::type value_type;
  typedef size_t index_type;
  typedef ptrdiff_t difference_type;
  typedef element_type * pointer;
  typedef const element_type * const_pointer;
  typedef element_type & reference;
  typedef const element_type & const_reference;
  typedef element_type * iterator;
  typedef const element_type * const_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  static const index_type extent = Extent;

protected:
  span_base(pointer data) : data_(data) {}
  ~span_base() {}

public:
  /// @name Iterators
  /// @{

  iterator begin() const { return data(); }

  const_iterator cbegin() const { return begin(); }

  iterator end() const { return begin() + size(); }

  const_iterator cend() const { return end(); }

  reverse_iterator rbegin() const { return reverse_iterator(end()); }

  const_reverse_iterator crbegin() const {
    return const_reverse_iterator(cend());
  }

  reverse_iterator rend() const { return reverse_iterator(begin()); }

  const_reverse_iterator crend() const {
    return const_reverse_iterator(cbegin());
  }

  /// @}

  /// @name Element access
  /// @{

  reference front() const { return operator[](0); }

  reference back() const { return operator[](size() - 1); }

  reference operator[](index_type idx) const { return data()[idx]; }

  pointer data() const { return data_; }

  /// @}

  /// @name Subviews
  /// @{

  template <index_type Count> span<element_type, Count> first() const {
    return subspan<0, Count>();
  }

  span<element_type> first(index_type Count) const { return subspan(0, Count); }

  span<element_type> last(index_type Count) const {
    return subspan(size() - Count, Count);
  }

  template <index_type Offset, index_type Count>
  span<element_type, Count> subspan() const {
    return span<element_type, Count>(data() + Offset, Count);
  }

  span<element_type> subspan(index_type Offset,
                             index_type Count = dynamic_extent) const {
    return span<element_type>(
        data() + Offset, Count == dynamic_extent ? size() - Offset : Count);
  }

  /// @}

private:
  index_type size() const {
    return static_cast<const span<T, Extent> &>(*this).size();
  }

  pointer data_;
};

template <template <typename, size_t> class span, typename T, size_t Extent>
const typename span_base<span, T, Extent>::index_type
    span_base<span, T, Extent>::extent;

template <typename T> class has_data {
  typedef char true_type[1];
  typedef char false_type[2];

  template <typename U, U> struct check;

  template <typename U>
  static true_type & test(check<typename U::pointer (U::*)(), &U::data> *);

  template <typename> static false_type & test(...);

public:
  static const bool value = (sizeof(test<T>(NULL)) == sizeof(true_type));
};

template <typename T> class has_data<const T> {
  typedef char true_type[1];
  typedef char false_type[2];

  template <typename U, U> struct check;

  template <typename U>
  static true_type &
  test(check<typename U::const_pointer (U::*)() const, &U::data> *);

  template <typename> static false_type & test(...);

public:
  static const bool value = (sizeof(test<T>(NULL)) == sizeof(true_type));
};

template <typename T> class has_size {
  typedef char true_type[1];
  typedef char false_type[2];

  template <typename U, U> struct check;

  template <typename U>
  static true_type &
  test(check<typename U::size_type (U::*)() const, &U::size> *);

  // Additionally support a static member function.
  template <typename U>
  static true_type & test(check<typename U::size_type (*)(), &U::size> *);

  template <typename> static false_type & test(...);

public:
  static const bool value = (sizeof(test<T>(NULL)) == sizeof(true_type));
};

template <typename T> struct is_array_class_helper : false_type {};

template <typename T, size_t N>
struct is_array_class_helper<array<T, N> > : true_type {};

template <typename T>
struct is_array_class : is_array_class_helper<typename remove_cv<T>::type> {};

template <typename T> struct is_span_helper : false_type {};

template <typename T, size_t Extent>
struct is_span_helper<span<T, Extent> > : true_type {};

template <typename T>
struct is_span : is_span_helper<typename remove_cv<T>::type> {};

template <typename T> struct is_vector_helper : false_type {};

template <typename T, typename Allocator>
struct is_vector_helper<std::vector<T, Allocator> > : true_type {};

template <typename T>
struct is_vector : is_vector_helper<typename remove_cv<T>::type> {};

// Used to disable the span container constructors.
template <typename T>
struct enable_if_span_convertible
    : enable_if<has_data<T>::value && has_size<T>::value &&
                !(is_array_class<T>::value || is_span<T>::value ||
                  is_vector<T>::value)> {};

} // namespace detail

// Implementation of the static extent span.
template <typename T, size_t Extent>
class span : public detail::span_base<MaximInterfaceCore::span, T, Extent> {
  typedef detail::span_base<MaximInterfaceCore::span, T, Extent> span_base;

public:
  using span_base::extent;
  using typename span_base::element_type;
  using typename span_base::index_type;
  using typename span_base::pointer;
  using typename span_base::value_type;

  span(pointer data, index_type) : span_base(data) {}

  span(pointer begin, pointer) : span_base(begin) {}

  span(element_type (&arr)[extent]) : span_base(arr) {}

  span(array<value_type, extent> & arr) : span_base(arr.data()) {}

  span(const array<value_type, extent> & arr) : span_base(arr.data()) {}

  template <typename U> span(const span<U, extent> & s) : span_base(s.data()) {}

  template <typename Allocator>
  span(std::vector<value_type, Allocator> & vec) : span_base(&vec.front()) {}

  template <typename Allocator>
  span(const std::vector<value_type, Allocator> & vec)
      : span_base(&vec.front()) {}

  template <typename Container>
  span(Container & cont,
       typename detail::enable_if_span_convertible<Container>::type * = NULL)
      : span_base(cont.data()) {}

  template <typename Container>
  span(const Container & cont,
       typename detail::enable_if_span_convertible<const Container>::type * =
           NULL)
      : span_base(cont.data()) {}

  /// @name Observers
  /// @{

  static index_type size() { return extent; }

  static index_type size_bytes() { return size() * sizeof(element_type); }

  static bool empty() { return size() == 0; }

  /// @}

  /// @name Subviews
  /// @{

  template <index_type Count> span<element_type, Count> last() const {
    return this->template subspan<extent - Count, Count>();
  }

  /// @}
};

// Implementation of the dynamic extent span.
template <typename T>
class span<T, dynamic_extent>
    : public detail::span_base<MaximInterfaceCore::span, T, dynamic_extent> {
  typedef detail::span_base<MaximInterfaceCore::span, T, dynamic_extent>
      span_base;

public:
  using typename span_base::element_type;
  using typename span_base::index_type;
  using typename span_base::pointer;
  using typename span_base::value_type;

  span() : span_base(NULL), size_(0) {}

  span(pointer data, index_type size) : span_base(data), size_(size) {}

  span(pointer begin, pointer end) : span_base(begin), size_(end - begin) {}

  template <size_t N> span(element_type (&arr)[N]) : span_base(arr), size_(N) {}

  template <size_t N>
  span(array<value_type, N> & arr) : span_base(arr.data()), size_(N) {}

  template <size_t N>
  span(const array<value_type, N> & arr) : span_base(arr.data()), size_(N) {}

  template <typename U, size_t N>
  span(const span<U, N> & s) : span_base(s.data()), size_(s.size()) {}

  template <typename Allocator>
  span(std::vector<value_type, Allocator> & vec)
      : span_base(vec.empty() ? NULL : &vec.front()), size_(vec.size()) {}

  template <typename Allocator>
  span(const std::vector<value_type, Allocator> & vec)
      : span_base(vec.empty() ? NULL : &vec.front()), size_(vec.size()) {}

  template <typename Container>
  span(Container & cont,
       typename detail::enable_if_span_convertible<Container>::type * = NULL)
      : span_base(cont.data()), size_(cont.size()) {}

  template <typename Container>
  span(const Container & cont,
       typename detail::enable_if_span_convertible<const Container>::type * =
           NULL)
      : span_base(cont.data()), size_(cont.size()) {}

  /// @name Observers
  /// @{

  index_type size() const { return size_; }

  index_type size_bytes() const { return size() * sizeof(element_type); }

  bool empty() const { return size() == 0; }

  /// @}

  /// @name Subviews
  /// @{

  template <index_type Count> span<element_type, Count> last() const {
    return span<element_type, Count>(this->data() + (size() - Count), Count);
  }

  /// @}

private:
  index_type size_;
};

template <typename T>
span<T> make_span(T * data, typename span<T>::index_type size) {
  return span<T>(data, size);
}

template <typename T> span<T> make_span(T * begin, T * end) {
  return span<T>(begin, end);
}

template <typename T, size_t N> span<T, N> make_span(T (&arr)[N]) {
  return span<T, N>(arr);
}

template <typename T, size_t N> span<T, N> make_span(array<T, N> & arr) {
  return arr;
}

template <typename T, size_t N>
span<const T, N> make_span(const array<T, N> & arr) {
  return arr;
}

template <typename Container>
span<typename Container::value_type> make_span(Container & cont) {
  return cont;
}

template <typename Container>
span<const typename Container::value_type> make_span(const Container & cont) {
  return cont;
}

} // namespace MaximInterfaceCore

#endif
