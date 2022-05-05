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

#include "RomCommands.hpp"

namespace MaximInterfaceCore {

void skipCurrentFamily(SearchRomState & searchState) {
  // Set the last discrepancy to last family discrepancy.
  searchState.lastDiscrepancy = searchState.lastFamilyDiscrepancy;
  // Clear the last family discrepancy.
  searchState.lastFamilyDiscrepancy = 0;
  // Check for end of list.
  if (searchState.lastDiscrepancy == 0) {
    searchState.lastDevice = true;
  }
}

Result<void> verifyRom(OneWireMaster & master, RomId::const_span romId) {
  SearchRomState searchState(romId);
  Result<void> result = searchRom(master, searchState);
  if (!result) {
    return result;
  }
  // Check if same device found.
  if (!equal(romId, make_span(searchState.romId))) {
    result = OneWireMaster::NoSlaveError;
  }
  return result;
}

Result<RomId::array> readRom(OneWireMaster & master) {
  Result<void> result = master.reset();
  if (!result) {
    return result.error();
  }
  result = master.writeByte(0x33);
  if (!result) {
    return result.error();
  }
  RomId::array romId;
  result = master.readBlock(romId);
  if (!result) {
    return result.error();
  }
  if (!valid(romId)) {
    return OneWireMaster::NoSlaveError;
  }
  return romId;
}

Result<void> skipRom(OneWireMaster & master) {
  Result<void> result = master.reset();
  if (!result) {
    return result;
  }
  result = master.writeByte(0xCC);
  return result;
}

Result<void> matchRom(OneWireMaster & master, RomId::const_span romId) {
  Result<void> result = master.reset();
  if (!result) {
    return result;
  }
  result = master.writeByte(0x55);
  if (!result) {
    return result;
  }
  result = master.writeBlock(romId);
  return result;
}

Result<void> overdriveSkipRom(OneWireMaster & master) {
  Result<void> result = master.reset();
  if (!result) {
    return result;
  }
  result = master.writeByte(0x3C);
  if (!result) {
    return result;
  }
  result = master.setSpeed(OneWireMaster::OverdriveSpeed);
  return result;
}

Result<void> overdriveMatchRom(OneWireMaster & master,
                               RomId::const_span romId) {
  Result<void> result = master.reset();
  if (!result) {
    return result;
  }
  result = master.writeByte(0x69);
  if (!result) {
    return result;
  }
  result = master.setSpeed(OneWireMaster::OverdriveSpeed);
  if (!result) {
    return result;
  }
  result = master.writeBlock(romId);
  return result;
}

Result<void> resumeRom(OneWireMaster & master) {
  Result<void> result = master.reset();
  if (!result) {
    return result;
  }
  result = master.writeByte(0xA5);
  return result;
}

Result<void> searchRom(OneWireMaster & master, SearchRomState & searchState) {
  if (searchState.lastDevice) {
    searchState = SearchRomState();
  }

  Result<void> result = master.reset();
  if (!result) {
    return result;
  }
  result = master.writeByte(0xF0);
  if (!result) {
    return result;
  }

  SearchRomState newSearchState;
  newSearchState.lastFamilyDiscrepancy = searchState.lastFamilyDiscrepancy;
  for (int idBitNumber = 1; idBitNumber <= 64; ++idBitNumber) {
    const int idByteNumber = (idBitNumber - 1) / 8;
    const unsigned int idBitMask = 1 << ((idBitNumber - 1) % 8);

    OneWireMaster::TripletData tripletData;
    if (idBitNumber == searchState.lastDiscrepancy) {
      tripletData.writeBit = 1;
    } else if (idBitNumber > searchState.lastDiscrepancy) {
      tripletData.writeBit = 0;
    } else { // idBitNumber < searchState.lastDiscrepancy
      tripletData.writeBit =
          (searchState.romId[idByteNumber] & idBitMask) == idBitMask;
    }

    MaximInterfaceCore_TRY_VALUE(tripletData,
                                 master.triplet(tripletData.writeBit));
    if (tripletData.readBit && tripletData.readBitComplement) {
      return OneWireMaster::NoSlaveError;
    }

    if (tripletData.writeBit) {
      newSearchState.romId[idByteNumber] |= idBitMask;
    } else if (!tripletData.readBit && !tripletData.readBitComplement) {
      newSearchState.lastDiscrepancy = idBitNumber;
      if (idBitNumber <= 8) {
        newSearchState.lastFamilyDiscrepancy = idBitNumber;
      }
    }
  }

  if (valid(newSearchState.romId)) {
    if (newSearchState.lastDiscrepancy == 0) {
      newSearchState.lastDevice = true;
    }
    searchState = newSearchState;
  } else {
    result = OneWireMaster::NoSlaveError;
  }
  return result;
}

} // namespace MaximInterfaceCore
