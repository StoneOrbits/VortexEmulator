#include "Arduino.h"

#include "TestFramework.h"

#include <Windows.h>
#include <chrono>
#include <random>
#include <time.h>

SerialClass Serial;

static LARGE_INTEGER start;
static LARGE_INTEGER tps; //tps = ticks per second

void init_arduino()
{
  QueryPerformanceFrequency(&tps);
  QueryPerformanceCounter(&start);
}

// used for seeding randomSeed()
unsigned long analogRead(uint32_t pin)
{
  return rand();
}

// used to read button input
unsigned long digitalRead(uint32_t pin)
{
  if (pin == 1) {
    // get button state
    if (g_pTestFramework->isButtonPressed()) {
      return LOW;
    }
    return HIGH;
  }
  return HIGH;
}

unsigned long millis()
{
  return GetTickCount();
}

unsigned long micros()
{
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  // yes, this will overflow, that's how arduino micros() works *shrug*
  return (unsigned long)((now.QuadPart - start.QuadPart) * 1000000 / tps.QuadPart);
}

unsigned long random(uint32_t low, uint32_t high)
{
  return low + (rand() % high);
}

void randomSeed(uint32_t seed)
{
  srand(seed);
}

void pinMode(uint32_t pin, uint32_t mode)
{
  // ???
}
