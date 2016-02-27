// 64Shield Library
// Controls MCP23017 16-bit digital I/O chips

#ifndef IOshield_h
#define IOshield_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

extern uint8_t IODataArray[2];

class IOshield
{
        public:
                      IOshield();
                void pinMode(int pin, int mode);
                void pinPullup(int pin, int mode);
                void digitalWrite(int pin, int level);
                int digitalRead(int pin);
                void portMode(int port, int value);
                void portPullup(int port, int value);
                void portWrite(int port, int value);
                int portRead(int port);
                void initialize();
        private:
                void WriteRegisters(int port, int startregister, int quantity);
                void ReadRegisters(int port, int startregister, int quantity);
                void WriteRegisterPin(int port, int regpin, int subregister, int level);
};

#endif
