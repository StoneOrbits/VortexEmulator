#pragma once

// drop in replacement for the arduino framework

#include <inttypes.h>

#define HIGH 1
#define LOW 0

#define INPUT_PULLUP 1

// init this drop-in framework
void init_arduino();

unsigned long analogRead(uint32_t pin);
unsigned long digitalRead(uint32_t pin);
unsigned long millis();
unsigned long micros();
unsigned long random(uint32_t low = 0, uint32_t high = 0);
void randomSeed(uint32_t seed);
void pinMode(uint32_t pin, uint32_t mode);

class SerialClass
{
public:
    void begin(uint32_t i) {}
    void print(uint32_t i) {}
    void print(const char *s) {}
};

extern SerialClass Serial;
