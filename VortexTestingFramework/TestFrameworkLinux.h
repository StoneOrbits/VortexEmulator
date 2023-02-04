#pragma once

#include "VortexEngine.h"

#include "Patterns/Patterns.h"
#include "Colors/ColorTypes.h"
#include "Colors/Colorset.h"
#include "Leds/LedTypes.h"

typedef uint32_t HDC;
typedef uint32_t HINSTANCE;
typedef uint32_t HWND;
typedef uint32_t WPARAM;
typedef uint32_t LPARAM;
typedef uint32_t HBRUSH;
typedef uint32_t DWORD;
typedef uint32_t LRESULT;
typedef uint32_t UINT;

typedef struct tagCRGB CRGB;

// paint callback type
typedef void (*paint_fn_t)(void *, HDC);

class TestFramework
{
public:
  TestFramework();
  ~TestFramework();

  // initialize the test framework
  bool init();
  // run the test framework
  void run();
  void cleanup();

  // arduino setup/loop
  void arduino_setup();
  void arduino_loop();

  // handlers for the arduino routines
  void show();

  // control the button
  void pressButton();
  void releaseButton();

  // whether the button is pressed
  bool isButtonPressed() const;

  // called by engine Buttons::check right after buttons are checked
  void injectButtons();

  bool stillRunning() const;

  static void printlog(const char *file, const char *func, int line, const char *msg, va_list list);

private:
  // these are in no particular order
  //HANDLE m_loopThread;
  //FILE *m_logHandle;
  //RECT m_ledPos[LED_COUNT];
  RGBColor *m_ledList;
  uint32_t m_numLeds;
  bool m_initialized;
  bool m_buttonPressed;
  bool m_keepGoing;
  volatile bool m_isPaused;
  PatternID m_curPattern;
  Colorset m_curColorset;
};

extern TestFramework *g_pTestFramework;
