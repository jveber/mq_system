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

#ifndef MaximInterfaceCore_Algorithm_hpp
#define MaximInterfaceCore_Algorithm_hpp

#include <algorithm>
#include "span.hpp"
#include "type_traits.hpp"

/// Alternative to std::max when a constant expression is required.
#define MaximInterfaceCore_MAX(a, b) ((a) < (b) ? (b) : (a))

/// Alternative to std::min when a constant expression is required.
#define MaximInterfaceCore_MIN(a, b) ((b) < (a) ? (b) : (a))

namespace MaximInterfaceCore {

/// Deep copy between static extent spans of the same size.
template <typename T, size_t Extent, typename U>
typename enable_if<Extent != dynamic_extent>::type copy(span<T, Extent> src,
                                                        span<U, Extent> dst) {
  std::copy(src.begin(), src.end(), dst.begin());
}

/// Deep equality between static extent spans of the same size.
template <typename T, size_t Extent, typename U>
typename enable_if<Extent != dynamic_extent, bool>::type
equal(span<T, Extent> lhs, span<U, Extent> rhs) {
  return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

} // namespace MaximInterfaceCore

#endif
