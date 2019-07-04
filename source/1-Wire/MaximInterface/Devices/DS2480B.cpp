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

#include <MaximInterface/Utilities/Error.hpp>
#include "DS2480B.hpp"

// Mode Commands
#define MODE_DATA 0xE1
#define MODE_COMMAND 0xE3
#define MODE_STOP_PULSE 0xF1

// Return byte value
#define RB_CHIPID_MASK 0x1C
#define RB_RESET_MASK 0x03
#define RB_1WIRESHORT 0x00
#define RB_PRESENCE 0x01
#define RB_ALARMPRESENCE 0x02
#define RB_NOPRESENCE 0x03

#define RB_BIT_MASK 0x03
#define RB_BIT_ONE 0x03
#define RB_BIT_ZERO 0x00

// Masks for all bit ranges
#define CMD_MASK 0x80
#define FUNCTSEL_MASK 0x60
#define BITPOL_MASK 0x10
#define SPEEDSEL_MASK 0x0C
#define MODSEL_MASK 0x02
#define PARMSEL_MASK 0x70
#define PARMSET_MASK 0x0E

// Command or config bit
#define CMD_COMM 0x81
#define CMD_CONFIG 0x01

// Function select bits
#define FUNCTSEL_BIT 0x00
#define FUNCTSEL_SEARCHON 0x30
#define FUNCTSEL_SEARCHOFF 0x20
#define FUNCTSEL_RESET 0x40
#define FUNCTSEL_CHMOD 0x60

// Bit polarity/Pulse voltage bits
#define BITPOL_ONE 0x10
#define BITPOL_ZERO 0x00
#define BITPOL_5V 0x00
#define BITPOL_12V 0x10

// One Wire speed bits
#define SPEEDSEL_STD 0x00
#define SPEEDSEL_FLEX 0x04
#define SPEEDSEL_OD 0x08
#define SPEEDSEL_PULSE 0x0C

// Data/Command mode select bits
#define MODSEL_DATA 0x00
#define MODSEL_COMMAND 0x02

// 5V Follow Pulse select bits
#define PRIME5V_TRUE 0x02
#define PRIME5V_FALSE 0x00

// Parameter select bits
#define PARMSEL_PARMREAD 0x00
#define PARMSEL_SLEW 0x10
#define PARMSEL_12VPULSE 0x20
#define PARMSEL_5VPULSE 0x30
#define PARMSEL_WRITE1LOW 0x40
#define PARMSEL_SAMPLEOFFSET 0x50
#define PARMSEL_ACTIVEPULLUPTIME 0x60
#define PARMSEL_BAUDRATE 0x70

// Pull down slew rate.
#define PARMSET_Slew15Vus 0x00
#define PARMSET_Slew2p2Vus 0x02
#define PARMSET_Slew1p65Vus 0x04
#define PARMSET_Slew1p37Vus 0x06
#define PARMSET_Slew1p1Vus 0x08
#define PARMSET_Slew0p83Vus 0x0A
#define PARMSET_Slew0p7Vus 0x0C
#define PARMSET_Slew0p55Vus 0x0E

// 12V programming pulse time table
#define PARMSET_32us 0x00
#define PARMSET_64us 0x02
#define PARMSET_128us 0x04
#define PARMSET_256us 0x06
#define PARMSET_512us 0x08
#define PARMSET_1024us 0x0A
#define PARMSET_2048us 0x0C
#define PARMSET_infinite 0x0E

// 5V strong pull up pulse time table
#define PARMSET_16p4ms 0x00
#define PARMSET_65p5ms 0x02
#define PARMSET_131ms 0x04
#define PARMSET_262ms 0x06
#define PARMSET_524ms 0x08
#define PARMSET_1p05s 0x0A
#define PARMSET_dynamic 0x0C
#define PARMSET_infinite 0x0E

// Write 1 low time
#define PARMSET_Write8us 0x00
#define PARMSET_Write9us 0x02
#define PARMSET_Write10us 0x04
#define PARMSET_Write11us 0x06
#define PARMSET_Write12us 0x08
#define PARMSET_Write13us 0x0A
#define PARMSET_Write14us 0x0C
#define PARMSET_Write15us 0x0E

// Data sample offset and Write 0 recovery time
#define PARMSET_SampOff3us 0x00
#define PARMSET_SampOff4us 0x02
#define PARMSET_SampOff5us 0x04
#define PARMSET_SampOff6us 0x06
#define PARMSET_SampOff7us 0x08
#define PARMSET_SampOff8us 0x0A
#define PARMSET_SampOff9us 0x0C
#define PARMSET_SampOff10us 0x0E

// Active pull up on time
#define PARMSET_PullUp0p0us 0x00
#define PARMSET_PullUp0p5us 0x02
#define PARMSET_PullUp1p0us 0x04
#define PARMSET_PullUp1p5us 0x06
#define PARMSET_PullUp2p0us 0x08
#define PARMSET_PullUp2p5us 0x0A
#define PARMSET_PullUp3p0us 0x0C
#define PARMSET_PullUp3p5us 0x0E

// DS2480B program voltage available
#define DS2480BPROG_MASK 0x20

namespace MaximInterface {

error_code DS2480B::initialize() {
  // reset modes
  level = NormalLevel;
  baud = Baud9600bps;
  mode = MODSEL_COMMAND;
  speed = SPEEDSEL_FLEX;

  // set the baud rate to 9600
  error_code result = setComBaud(baud);
  if (result) {
    return result;
  }

  // send a break to reset the DS2480B
  result = breakCom();
  if (result) {
    return result;
  }

  // delay to let line settle
  (*sleep)(2);

  // flush the buffers
  result = uart->clearReadBuffer();
  if (result) {
    return result;
  }

  // send the timing byte
  uint_least8_t packet[5];
  packet[0] = 0xC1;
  result = uart->writeBlock(packet, 1);
  if (result) {
    return result;
  }

  // delay to let line settle
  (*sleep)(2);

  // set the FLEX configuration parameters
  // default PDSRC = 1.37Vus
  int packetLen = 0;
  packet[packetLen++] = CMD_CONFIG | PARMSEL_SLEW | PARMSET_Slew1p37Vus;
  // default W1LT = 10us
  packet[packetLen++] = CMD_CONFIG | PARMSEL_WRITE1LOW | PARMSET_Write10us;
  // default DSO/WORT = 8us
  packet[packetLen++] = CMD_CONFIG | PARMSEL_SAMPLEOFFSET | PARMSET_SampOff8us;

  // construct the command to read the baud rate (to test command block)
  packet[packetLen++] = CMD_CONFIG | PARMSEL_PARMREAD | (PARMSEL_BAUDRATE >> 3);

  // also do 1 bit operation (to test 1-Wire block)
  packet[packetLen++] = CMD_COMM | FUNCTSEL_BIT | baud | BITPOL_ONE;

  // flush the buffers
  result = uart->clearReadBuffer();
  if (result) {
    return result;
  }

  // send the packet
  result = uart->writeBlock(packet, packetLen);
  if (result) {
    return result;
  }

  // read back the response
  result = uart->readBlock(packet, sizeof(packet) / sizeof(packet[0]));
  if (result) {
    return result;
  }

  // look at the baud rate and bit operation
  // to see if the response makes sense
  if (!(((packet[3] & 0xF1) == 0x00) && ((packet[3] & 0x0E) == baud) &&
        ((packet[4] & 0xF0) == 0x90) && ((packet[4] & 0x0C) == baud))) {
    result = make_error_code(HardwareError);
  }

  return result;
}

error_code DS2480B::reset() {
  uint_least8_t packet[2];
  int packetLen = 0;

  // check for correct mode
  if (mode != MODSEL_COMMAND) {
    mode = MODSEL_COMMAND;
    packet[packetLen++] = MODE_COMMAND;
  }

  // construct the command
  packet[packetLen++] = (CMD_COMM | FUNCTSEL_RESET | speed);

  // flush the buffers
  error_code result = uart->clearReadBuffer();
  if (result) {
    return result;
  }

  // send the packet
  result = uart->writeBlock(packet, packetLen);
  if (result) {
    return result;
  }

  // read back the 1 byte response
  result = uart->readBlock(packet, 1);
  if (result) {
    return result;
  }

  // make sure this byte looks like a reset byte
  if ((packet[0] & RB_RESET_MASK) == RB_1WIRESHORT) {
    result = make_error_code(ShortDetectedError);
  }
  else if ((packet[0] & RB_RESET_MASK) == RB_NOPRESENCE) {
    result = make_error_code(NoSlaveError);
  }

  return result;
}

error_code DS2480B::touchBitSetLevel(bool & sendRecvBit, Level afterLevel) {
  uint_least8_t packet[2];
  int packetLen = 0;

  // check for correct mode
  if (mode != MODSEL_COMMAND) {
    mode = MODSEL_COMMAND;
    packet[packetLen++] = MODE_COMMAND;
  }

  // construct the command
  packet[packetLen++] = (sendRecvBit ? BITPOL_ONE : BITPOL_ZERO) | CMD_COMM |
                        FUNCTSEL_BIT | speed;

  // flush the buffers
  error_code result = uart->clearReadBuffer();
  if (result) {
    return result;
  }

  // send the packet
  result = uart->writeBlock(packet, packetLen);
  if (result) {
    return result;
  }

  // read back the response
  result = uart->readBlock(packet, 1);
  if (result) {
    return result;
  }

  // interpret the response
  if ((packet[0] & 0xE0) == 0x80) {
    sendRecvBit = ((packet[0] & RB_BIT_MASK) == RB_BIT_ONE);
    result = setLevel(afterLevel);
  } else {
    result = make_error_code(HardwareError);
  }

  return result;
}

error_code DS2480B::writeByteSetLevel(uint_least8_t sendByte,
                                      Level afterLevel) {
  uint_least8_t packet[3];
  int packetLen = 0;

  // check for correct mode
  if (mode != MODSEL_DATA) {
    mode = MODSEL_DATA;
    packet[packetLen++] = MODE_DATA;
  }

  // add the byte to send
  packet[packetLen++] = sendByte;

  // check for duplication of data that looks like COMMAND mode
  if (sendByte == MODE_COMMAND) {
    packet[packetLen++] = sendByte;
  }

  // flush the buffers
  error_code result = uart->clearReadBuffer();
  if (result) {
    return result;
  }

  // send the packet
  result = uart->writeBlock(packet, packetLen);
  if (result) {
    return result;
  }

  // read back the 1 byte response
  result = uart->readBlock(packet, 1);
  if (result) {
    return result;
  }

  if (packet[0] == sendByte) {
    result = setLevel(afterLevel);
  } else {
    result = make_error_code(HardwareError);
  }

  return result;
}

error_code DS2480B::readByteSetLevel(uint_least8_t & recvByte,
                                     Level afterLevel) {
  uint_least8_t packet[2];
  int packetLen = 0;

  // check for correct mode
  if (mode != MODSEL_DATA) {
    mode = MODSEL_DATA;
    packet[packetLen++] = MODE_DATA;
  }

  // add the byte to send
  packet[packetLen++] = 0xFF;

  // flush the buffers
  error_code result = uart->clearReadBuffer();
  if (result) {
    return result;
  }

  // send the packet
  result = uart->writeBlock(packet, packetLen);
  if (result) {
    return result;
  }

  // read back the 1 byte response
  result = uart->readBlock(packet, 1);
  if (result) {
    return result;
  }

  recvByte = packet[0];
  result = setLevel(afterLevel);

  return result;
}

error_code DS2480B::setSpeed(Speed newSpeed) {
  error_code result;
  bool setSpeed = false;

  // check if supported speed
  switch (newSpeed) {
  case OverdriveSpeed:
    // check if change from current mode
    if (speed != SPEEDSEL_OD) {
      result = changeBaud(Baud115200bps);
      if (!result) {
        speed = SPEEDSEL_OD;
        setSpeed = true;
      }
    }
    break;

  case StandardSpeed:
    // check if change from current mode
    if (speed != SPEEDSEL_STD) {
      result = changeBaud(Baud9600bps);
      if (!result) {
        speed = SPEEDSEL_STD;
        setSpeed = true;
      }
    }
    break;

  default:
    result = make_error_code(InvalidSpeedError);
    break;
  }

  // if baud rate is set correctly then change DS2480 speed
  if (setSpeed) {
    uint_least8_t packet[2];
    int packetLen = 0;

    // check if correct mode
    if (mode != MODSEL_COMMAND) {
      mode = MODSEL_COMMAND;
      packet[packetLen++] = MODE_COMMAND;
    }

    // proceed to set the DS2480 communication speed
    packet[packetLen++] = CMD_COMM | FUNCTSEL_SEARCHOFF | speed;

    // send the packet
    result = uart->writeBlock(packet, packetLen);
  }

  return result;
}

error_code DS2480B::setLevel(Level newLevel) {
  error_code result;
  // check if need to change level
  if (newLevel != level) {
    uint_least8_t packet[4];
    int packetLen = 0;

    // check for correct mode
    if (mode != MODSEL_COMMAND) {
      mode = MODSEL_COMMAND;
      packet[packetLen++] = MODE_COMMAND;
    }

    switch (newLevel) {
    case NormalLevel:
      // stop pulse command
      packet[packetLen++] = MODE_STOP_PULSE;

      // add the command to begin the pulse WITHOUT prime
      packet[packetLen++] = CMD_COMM | FUNCTSEL_CHMOD | SPEEDSEL_PULSE |
                            BITPOL_5V | PRIME5V_FALSE;

      // stop pulse command
      packet[packetLen++] = MODE_STOP_PULSE;

      // flush the buffers
      result = uart->clearReadBuffer();
      if (result) {
        return result;
      }

      // send the packet
      result = uart->writeBlock(packet, packetLen);
      if (result) {
        return result;
      }

      // read back the 1 byte response
      result = uart->readBlock(packet, 2);
      if (result) {
        return result;
      }

      // check response byte
      if (((packet[0] & 0xE0) == 0xE0) && ((packet[1] & 0xE0) == 0xE0)) {
        level = NormalLevel;
      } else {
        result = make_error_code(HardwareError);
      }
      break;

    case StrongLevel:
      // set the SPUD time value
      packet[packetLen++] = CMD_CONFIG | PARMSEL_5VPULSE | PARMSET_infinite;
      // add the command to begin the pulse
      packet[packetLen++] =
          CMD_COMM | FUNCTSEL_CHMOD | SPEEDSEL_PULSE | BITPOL_5V;

      // flush the buffers
      result = uart->clearReadBuffer();
      if (result) {
        return result;
      }

      // send the packet
      result = uart->writeBlock(packet, packetLen);
      if (result) {
        return result;
      }

      // read back the 1 byte response from setting time limit
      result = uart->readBlock(packet, 1);
      if (result) {
        return result;
      }

      // check response byte
      if ((packet[0] & 0x81) == 0) {
        level = newLevel;
      } else {
        result = make_error_code(HardwareError);
      }
      break;

    default:
      result = make_error_code(InvalidLevelError);
      break;
    }
  }
  return result;
}

error_code DS2480B::sendCommand(uint_least8_t command) {
  uint_least8_t packet[2];
  int packetLen = 0;
  
  // check for correct mode
  if (mode != MODSEL_COMMAND) {
    mode = MODSEL_COMMAND;
    packet[packetLen++] = MODE_COMMAND;
  }

  // add command
  packet[packetLen++] = command;

  // send the packet
  return uart->writeBlock(packet, packetLen);
}

error_code DS2480B::changeBaud(BaudRate newBaud) {
  error_code result;

  //see if different then current baud rate
  if (baud != newBaud) {
    uint_least8_t packet[2];
    int packetLen = 0;

    // build the command packet
    // check for correct mode
    if (mode != MODSEL_COMMAND) {
      mode = MODSEL_COMMAND;
      packet[packetLen++] = MODE_COMMAND;
    }
    // build the command
    const uint_least8_t baudByte = CMD_CONFIG | PARMSEL_BAUDRATE | newBaud;
    packet[packetLen++] = baudByte;

    // flush the buffers
    result = uart->clearReadBuffer();
    if (result) {
      return result;
    }

    // send the packet
    result = uart->writeBlock(packet, packetLen);
    if (result) {
      return result;
    }

    // make sure buffer is flushed
    (*sleep)(5);

    // change our baud rate
    result = setComBaud(newBaud);
    if (result) {
      return result;
    }
    baud = newBaud;

    // wait for things to settle
    (*sleep)(5);

    // build a command packet to read back baud rate
    packet[0] = CMD_CONFIG | PARMSEL_PARMREAD | (PARMSEL_BAUDRATE >> 3);

    // flush the buffers
    result = uart->clearReadBuffer();
    if (result) {
      return result;
    }

    // send the packet
    result = uart->writeBlock(packet, 1);
    if (result) {
      return result;
    }

    // read back the 1 byte response
    result = uart->readBlock(packet, 1);
    if (result) {
      return result;
    }

    // verify correct baud
    if (!((packet[0] & 0x0E) == (baudByte & 0x0E))) {
      result = make_error_code(HardwareError);
    }
  }

  return result;
}

error_code DS2480B::setComBaud(BaudRate newBaud) {
  switch (newBaud) {
  case Baud115200bps:
    return uart->setBaudRate(115200);

  case Baud57600bps:
    return uart->setBaudRate(57600);

  case Baud19200bps:
    return uart->setBaudRate(19200);

  case Baud9600bps:
  default:
    return uart->setBaudRate(9600);
  }
}

error_code DS2480B::breakCom() {
  // Switch to lower baud rate to ensure break is longer than 2 ms.
  error_code result = uart->setBaudRate(4800);
  if (result) {
    return result;
  }
  result = uart->sendBreak();
  if (result) {
    return result;
  }
  result = setComBaud(baud);
  return result;
}

const error_category & DS2480B::errorCategory() {
  static class : public error_category {
  public:
    virtual const char * name() const { return "DS2480B"; }

    virtual std::string message(int condition) const {
      switch (condition) {
      case HardwareError:
        return "Hardware Error";

      default:
        return defaultErrorMessage(condition);
      }
    }
  } instance;
  return instance;
}

} // namespace MaximInterface
