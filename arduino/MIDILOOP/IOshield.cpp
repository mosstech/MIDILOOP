// 64shield Library
// Controls MCP23017 16-bit digital I/O chips

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif
#include "IOshield.h"
#include <Wire.h>

uint8_t IODataArray[2] = {0};

#define IOAddress 0b0100000

IOshield::IOshield()
{
  // no constructor tasks yet
}

// Set device to default values
void IOshield::initialize()
{

  for (int j = 0; j < 7; j++) {

    IODataArray[0] = 255;
    IODataArray[1] = 255;

    WriteRegisters(j, 0x00, 2);

    IODataArray[0] = 0;
    IODataArray[1] = 0;

    for (int k = 2; k < 0x15; k+=2) {
      WriteRegisters(j, k, 2);
    }

  }

}


void IOshield::WriteRegisters(int port, int startregister, int quantity) {

  Wire.beginTransmission(IOAddress + port);
#if defined(ARDUINO) && ARDUINO >= 100
    Wire.write((byte)startregister);
    for (int i = 0; i < quantity; i++) {
                Wire.write((byte)IODataArray[i]);
    }
#else
    Wire.send((byte)startregister);
    for (int i = 0; i < quantity; i++) {
                Wire.send((byte)IODataArray[i]);
    }
#endif

  Wire.endTransmission();

}

void IOshield::ReadRegisters(int port, int startregister, int quantity) {

  Wire.beginTransmission(IOAddress + port);
#if defined(ARDUINO) && ARDUINO >= 100
        Wire.write((byte)startregister);
        Wire.endTransmission();
        Wire.requestFrom(IOAddress + port, quantity);
        for (int i = 0; i < quantity; i++) {
                IODataArray[i] = Wire.read();
        }
#else
        Wire.send((byte)startregister);
        Wire.endTransmission();
        Wire.requestFrom(IOAddress + port, quantity);
        for (int i = 0; i < quantity; i++) {
                IODataArray[i] = Wire.receive();
        }
#endif

}


void IOshield::WriteRegisterPin(int port, int regpin, int subregister, int level) {

  ReadRegisters(port, subregister, 1); 
  
  if (level == 0) {
    IODataArray[0] &= ~(1 << regpin);
  }
  else {
    IODataArray[0] |= (1 << regpin);
  }
  
  WriteRegisters(port, subregister, 1);
  
}

void IOshield::pinMode(int pin, int mode) {
  
  int port = pin >> 4;
  int subregister = (pin & 8) >> 3;

  int regpin = pin - ((port << 1) + subregister)*8;

  WriteRegisterPin(port, regpin, subregister, mode ^ 1);
  
}

void IOshield::pinPullup(int pin, int mode) {
  
  int port = pin >> 4;
  int subregister = (pin & 8) >> 3;

  int regpin = pin - ((port << 1) + subregister)*8;

  WriteRegisterPin(port, regpin, 0x0C + subregister, mode);
  
}


void IOshield::digitalWrite(int pin, int level) {
  
  int port = pin >> 4;
  int subregister = (pin & 8) >> 3;

  int regpin = pin - ((port << 1) + subregister)*8;

  WriteRegisterPin(port, regpin, 0x12 + subregister, level);
  
}

int IOshield::digitalRead(int pin) {

  int port = pin >> 4;
  int subregister = (pin & 8) >> 3;

  ReadRegisters(port, 0x12 + subregister, 1);

  int returnval = (IODataArray[0] >> (pin - ((port << 1) + subregister)*8)) & 1;

  return returnval;

}

void IOshield::portMode(int port, int value) {
  
  IODataArray[0] = value;
  IODataArray[1] = value>>8;
  
  WriteRegisters(port, 0x00, 2);
  
}

void IOshield::portWrite(int port, int value) {
  
  IODataArray[0] = value;
  IODataArray[1] = value>>8;
  
  WriteRegisters(port, 0x12, 2);
  
}

void IOshield::portPullup(int port, int value) {
  
  IODataArray[0] = value;
  IODataArray[1] = value>>8;
  
  WriteRegisters(port, 0x0C, 2);
  
}

int IOshield::portRead(int port) {

  ReadRegisters(port, 0x12, 2);

  int receivedval = IODataArray[0];
  receivedval |= IODataArray[1] << 8;

  return receivedval;  

}
