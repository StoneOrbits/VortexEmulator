#pragma once
#ifdef LINUX_FRAMEWORK

// redirect to linux version
#include "TestFrameworkLinux.h"

#else // entire file

#include <Windows.h>
#include <stdio.h>

#include "GUI/VWindow.h"
#include "GUI/VButton.h"
#include "GUI/VSelectBox.h"
#include "GUI/VCircle.h"

#include "Colors/ColorTypes.h"
#include "Colors/Colorset.h"
#include "Leds/LedTypes.h"
#include "Patterns/Patterns.h"
#include "Modes/Mode.h"

#include <string>
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
  void cleanup();

  // run the test framework
  void run();

  // handlers for the arduino routines
  void installLeds(CRGB *leds, uint32_t count);
  void setBrightness(int brightness);
  void show();

  // control the button
  void pressButton();
  void releaseButton();

  // whether the button is pressed
  bool isButtonPressed() const;

  // pause and unpause the main arduino loop
  void pause();
  void unpause();

  // reload the pattern strip with the new patternID
  bool handlePatternChange(bool force = false);
  void handleWindowClick(int x, int y);
  void selectLed(LedPos led);

  // lookup a brush by rgbcolor
  HBRUSH getBrushCol(RGBColor col);

  // whether initialized
  bool initialized() const { return m_initialized; }

  std::string getWindowTitle();
  void setWindowTitle(std::string title);
  
  void setWindowPos(uint32_t x, uint32_t y);

  // loop that runs arduino code
  static DWORD __stdcall arduino_loop_thread(void *arg);

  // main window procedure
  static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
  static void buttonClickCallback(void *arg, VButton *window, VButton::ButtonEvent type) {
    ((TestFramework *)arg)->buttonClick(window, type);
  }
  static void launchIRCallback(void *arg, VButton *window, VButton::ButtonEvent type) {
    ((TestFramework *)arg)->launchIR(window, type);
  }
  static void patternStripSelectCallback(void *arg, uint32_t x, uint32_t y, VSelectBox::SelectEvent sevent) {
    ((TestFramework *)arg)->patternStripSelect(x, y, sevent);
  }
  static void ledClickCallback(void *arg, VWindow *window) {
    ((TestFramework *)arg)->ledClick(window);
  }
  static void setTickrateCallback(void *arg, uint32_t x, uint32_t y, VSelectBox::SelectEvent sevent) {
    ((TestFramework *)arg)->setTickrate(x, y, sevent);
  }

  void buttonClick(VButton *window, VButton::ButtonEvent type);
  void launchIR(VButton *window, VButton::ButtonEvent type);
  void patternStripSelect(uint32_t x, uint32_t y, VSelectBox::SelectEvent sevent);
  void ledClick(VWindow *window);
  void setTickrate(uint32_t x, uint32_t y, VSelectBox::SelectEvent sevent);

  static const uint32_t width = 460;
  static const uint32_t height = 460;

  static const uint32_t patternStripHeight = 30;

  static const uint32_t tickrateSliderWidth = 24;
  static const uint32_t tickrateSliderHeight = 260;

  // how many times the length of the pattern strip is extended
  // in order to simulate the scrolling effect
  static const uint32_t patternStripExtensionMultiplier = 10;

  // new stuff
  VWindow m_window;
  VSelectBox m_gloveBox;
  VSelectBox m_patternStrip;
  VSelectBox m_tickrateSlider;
  VButton m_button;
  VButton m_IRLaunchButton;
  VCircle m_leds[LED_COUNT];

  RECT m_ledPos[LED_COUNT];

  HANDLE m_pauseMutex;

  HINSTANCE m_hInst;
  FILE *m_consoleHandle;
  HBITMAP m_gloveBMP;
  HICON m_hIcon;
  HANDLE m_loopThread;

  int m_brightness;

  RGBColor *m_ledList;
  uint32_t m_numLeds;
  RGBColor *m_lastLedColor;

  LedPos m_curSelectedLed;

  bool m_initialized;
  bool m_buttonPressed;
  bool m_keepGoing;

  volatile bool m_isPaused;

  Mode m_curMode;

  std::map<COLORREF, HBRUSH> m_brushmap;

  HACCEL m_accelTable;
};

extern TestFramework *g_pTestFramework;
#endif
