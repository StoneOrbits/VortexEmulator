#pragma once
#include <Windows.h>
#include <stdio.h>

#include "VortexFramework.h"
#include "ColorTypes.h"
#include "LedConfig.h"
#include "Patterns.h"
#include "Colorset.h"

#include <vector>
#include <map>

// paint callback type
typedef void (*paint_fn_t)(void *, HDC);

class TestFramework
{
public:
  TestFramework();
  ~TestFramework();

  // initialize the test framework
  bool init(HINSTANCE hInstance);
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
  static DWORD __stdcall arduino_loop_thread(void *arg);

  // button subproc
  static LRESULT CALLBACK button_subproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK slider_subproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  // main window procedure
  static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  static void printlog(const char *file, const char *func, int line, const char *msg, va_list list);

private:
  const COLORREF bkcolor = RGB(40, 40, 40);

  // these are in no particular order
  HANDLE m_loopThread;

  HBRUSH m_bkbrush;
  FILE *m_consoleHandle;
  FILE *m_logHandle;
  WNDPROC m_oldButtonProc;
  WNDPROC m_oldSliderProc;

  HWND m_hwndClickButton;
  HWND m_hwndTickrateSlider;
  HWND m_hwndTickOffsetSlider;

  HWND m_hwnd;
  WNDCLASS m_wc;

  int m_brightness;

  RECT m_ledPos[LED_COUNT];

  RGBColor *m_ledList;
  uint32_t m_numLeds;

  bool m_initialized;

  bool m_buttonPressed;

  bool m_keepGoing;

  volatile bool m_isPaused;

  HANDLE m_pauseMutex;

  PatternID m_curPattern;
  Colorset m_curColorset;

  // one color per pixel of strip
  std::vector<RGBColor> m_patternStrip;

  bool m_redrawStrip;

  std::map<COLORREF, HBRUSH> m_brushmap;
};

extern TestFramework *g_pTestFramework;
