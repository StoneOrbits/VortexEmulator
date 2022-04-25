#include "Arduino.h"

#include "TestFramework.h"

#include <Windows.h>
#include <random>
#include <time.h>

SerialClass Serial;

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
