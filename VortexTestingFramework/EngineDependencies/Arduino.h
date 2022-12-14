#pragma once

// drop in replacement for the arduino framework

#include <inttypes.h>
#include <string.h>

#define HIGH 1
#define LOW 0

#define INPUT_PULLUP 1
#define OUTPUT 1
#define CHANGE 1

// uncommont this to enable IR comms simulation
//#define SIMULATE_IR_COMMS

// init this drop-in framework
void init_arduino();

void delay(size_t amt);
void delayMicroseconds(size_t us);

unsigned long analogRead(uint32_t pin);
unsigned long digitalRead(uint32_t pin);
void digitalWrite(uint32_t pin,  uint32_t val); 
unsigned long millis();
unsigned long micros();
unsigned long nanos();
// generate random number from low (inclusive) to high (exclusive)
unsigned long random(uint32_t low, uint32_t high);
unsigned long random(uint32_t high);
void randomSeed(uint32_t seed);
void pinMode(uint32_t pin, uint32_t mode);

void attachInterrupt(int interrupt, void (*func)(), int type);
void detachInterrupt(int interrupt);
int digitalPinToInterrupt(int pin);

#ifdef TEST_FRAMEWORK
// not actually from arduino but we use to simulate ir comms
void test_ir_mark(uint32_t duration);
void test_ir_space(uint32_t duration);

bool is_ir_server();

// install the IR callback for test framework
void installIRCallback(void (*func)(uint32_t));
#endif

class SerialClass
{
public:
    void begin(uint32_t i) {}
    void print(uint32_t i) {}
    void print(const char *s) {}
    void println(const char *s) {}
    uint32_t write(const uint8_t *buf, size_t len) { return 0; }
    operator bool() { return true; }
    int32_t available() { return 0; }
    size_t readBytes(char *buf, size_t amt) { return 0; }
    uint8_t read() { return 0; }
};

extern SerialClass Serial;
