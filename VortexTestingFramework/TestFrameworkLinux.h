#pragma once
#include <stdio.h>

#include "VortexFramework.h"
#include "ColorTypes.h"
#include "LedConfig.h"
#include "Patterns.h"
#include "Colorset.h"

#include <vector>

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

  // initialize the test framework
  bool init();
  // run the test framework
  void run();

  // windows message handlers
  void create(HWND hwnd);
  void command(WPARAM wParam, LPARAM lParam);
  void paint(HWND hwnd);
  void cleanup();

  // arduino setup/loop
  void arduino_setup();
  void arduino_loop();

  // handlers for the arduino routines
  void installLeds(CRGB *leds, uint32_t count);
  void setBrightness(int brightness);
  void show();

  // control the button
  void pressButton();
  void releaseButton();

  // whether the button is pressed
  bool isButtonPressed() const;

  // change the tick rate based on slider (ticks per second)
  void setTickrate();
  // change the time offset based on the slider
  void setTickOffset();

  // whether initialized
  bool initialized() const { return m_initialized; }

  // pause and unpause the main arduino loop
  void pause();
  void unpause();

  // reload the pattern strip with the new patternID
  void handlePatternChange();
    
  // lookup a brush by rgbcolor
  HBRUSH getBrushCol(RGBColor col);

  // loop that runs arduino code
  static DWORD arduino_loop_thread(void *arg);

  // button subproc
  static LRESULT button_subproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  static LRESULT slider_subproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  // main window procedure
  static LRESULT window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  static void printlog(const char *file, const char *func, int line, const char *msg, va_list list);

private:
  // these are in no particular order
  //HANDLE m_loopThread;

  FILE *m_logHandle;

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
