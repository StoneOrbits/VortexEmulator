#include "FastLED.h"
#ifdef LINUX_FRAMEWORK
#include "TestFrameworkLinux.h"
#else
#include "TestFramework.h"
#endif

// global instance
FastLEDClass FastLED;

// called when the user calls FastLED.addLeds
void FastLEDClass::init(CRGB *cl, int count)
{
#ifndef LINUX_FRAMEWORK
  g_pTestFramework->installLeds(cl, count);
#endif
}

// called when user calls FastLED.setBrightness
void FastLEDClass::setBrightness(int brightness)
{
#ifndef LINUX_FRAMEWORK
  g_pTestFramework->setBrightness(brightness);
#endif
}

// called when user calls FastLED.show
void FastLEDClass::show(uint32_t brightness)
{
  setBrightness(brightness);
  g_pTestFramework->show();
}

