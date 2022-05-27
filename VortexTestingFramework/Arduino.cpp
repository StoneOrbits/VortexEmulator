#include "Arduino.h"

#ifdef LINUX_FRAMEWORK
#include "TestFrameworkLinux.h"
#else
#include "TestFramework.h"
#include <Windows.h>
#endif

#include <chrono>
#include <random>
#include <time.h>

SerialClass Serial;

#ifdef LINUX_FRAMEWORK
static uint64_t start;
#else
static LARGE_INTEGER start;
static LARGE_INTEGER tps; //tps = ticks per second
#endif

void init_arduino()
{
#ifdef LINUX_FRAMEWORK
  start = micros();
#else
  QueryPerformanceFrequency(&tps);
  QueryPerformanceCounter(&start);
#endif
}

void delay(size_t amt)
{
#ifdef LINUX_FRAMEWORK
  //sleep(amt);
#else
  Sleep(amt);
#endif
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

/// Convert seconds to milliseconds
#define SEC_TO_MS(sec) ((sec)*1000)
/// Convert seconds to microseconds
#define SEC_TO_US(sec) ((sec)*1000000)
/// Convert seconds to nanoseconds
#define SEC_TO_NS(sec) ((sec)*1000000000)

/// Convert nanoseconds to seconds
#define NS_TO_SEC(ns)   ((ns)/1000000000)
/// Convert nanoseconds to milliseconds
#define NS_TO_MS(ns)    ((ns)/1000000)
/// Convert nanoseconds to microseconds
#define NS_TO_US(ns)    ((ns)/1000)

unsigned long millis()
{
#ifdef LINUX_FRAMEWORK
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  uint64_t ms = SEC_TO_MS((uint64_t)ts.tv_sec) + NS_TO_MS((uint64_t)ts.tv_nsec);
  return (unsigned long)ms;
#else
  return GetTickCount();
#endif
}

unsigned long micros()
{
#ifdef LINUX_FRAMEWORK
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  uint64_t us = SEC_TO_US((uint64_t)ts.tv_sec) + NS_TO_US((uint64_t)ts.tv_nsec);
  return (unsigned long)us;
#else
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  // yes, this will overflow, that's how arduino micros() works *shrug*
  return (unsigned long)((now.QuadPart - start.QuadPart) * 1000000 / tps.QuadPart);
#endif
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
