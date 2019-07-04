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

#ifndef MaximInterface_Ecc256
#define MaximInterface_Ecc256

#include <stdint.h>
#include "array.hpp"
#include "Export.h"
#include "ManId.hpp"
#include "RomId.hpp"

namespace MaximInterface {
namespace Ecc256 {

typedef array<uint_least8_t, 32> Scalar;
struct Point {
  /// Total size of all elements in bytes.
  static const size_t size = 2 * Scalar::csize;
  
  Scalar x;
  Scalar y;
};

typedef Scalar PrivateKey;
typedef Point PublicKey;
struct KeyPair {
  /// Total size of all elements in bytes.
  static const size_t size = PrivateKey::csize + PublicKey::size;
  
  PrivateKey privateKey;
  PublicKey publicKey;
};

struct Signature {
  /// Total size of all elements in bytes.
  static const size_t size = 2 * Scalar::csize;
  
  Scalar r;
  Scalar s;
};

/// Data used to create a device key certificate for ECC-256 authenticators.
typedef array<uint_least8_t, 2 * Scalar::csize + RomId::csize + ManId::csize>
    CertificateData;

/// Formats data for creating a device key certificate from the public key,
/// ROM ID, and MAN ID.
MaximInterface_EXPORT CertificateData createCertificateData(
    const PublicKey & publicKey, const RomId & romId, const ManId & manId);

} // namespace Ecc256
} // namespace MaximInterface

#endif
