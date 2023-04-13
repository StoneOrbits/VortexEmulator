#pragma once

#include "TestFramework.h"

#define NEO_GRB 1
#define NEO_RGB 1
#define NEO_KHZ800 1

class tinyNeoPixel
{
public:
  tinyNeoPixel()
  {
  }
  tinyNeoPixel(int a, int b, int c, void *d)
  {
#ifndef LINUX_FRAMEWORK
    g_pTestFramework->installLeds((CRGB *)d, a);
#endif
  }

  void show()
  {
#ifndef LINUX_FRAMEWORK
    g_pTestFramework->show();
#endif
  }
};
