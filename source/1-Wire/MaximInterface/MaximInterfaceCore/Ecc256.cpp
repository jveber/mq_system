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

#include "Ecc256.hpp"

namespace MaximInterfaceCore {
namespace Ecc256 {

void copy(Point::const_span src, Point::span dst) {
  copy(src.x, dst.x);
  copy(src.y, dst.y);
}

bool equal(Point::const_span lhs, Point::const_span rhs) {
  return equal(lhs.x, rhs.x) && equal(lhs.y, rhs.y);
}

void copy(KeyPair::const_span src, KeyPair::span dst) {
  copy(src.privateKey, dst.privateKey);
  copy(src.publicKey, dst.publicKey);
}

bool equal(KeyPair::const_span lhs, KeyPair::const_span rhs) {
  return equal(lhs.privateKey, rhs.privateKey) &&
         equal(lhs.publicKey, rhs.publicKey);
}

void copy(Signature::const_span src, Signature::span dst) {
  copy(src.r, dst.r);
  copy(src.s, dst.s);
}

bool equal(Signature::const_span lhs, Signature::const_span rhs) {
  return equal(lhs.r, rhs.r) && equal(lhs.s, rhs.s);
}

PublicKey::span CertificateData::publicKey() {
  const PublicKey::span span = {
      make_span(result_).subspan<publicKeyIdx, Scalar::size>(),
      make_span(result_).subspan<publicKeyIdx + Scalar::size, Scalar::size>()};
  return span;
}

} // namespace Ecc256
} // namespace MaximInterfaceCore
