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

#include "VortexLib.h"

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

  // reload the pattern strip with the new patternID
  bool handlePatternChange(bool force = false);

  // lookup a brush by rgbcolor
  HBRUSH getBrushCol(RGBColor col);

  // whether initialized
  bool initialized() const { return m_initialized; }

  std::string getWindowTitle();
  void setWindowTitle(std::string title);
  
  void setWindowPos(uint32_t x, uint32_t y);

  // loop that runs arduino code
  static DWORD __stdcall main_loop_thread(void *arg);

private:
  // initializes the led positions for various devices
  void setupLedPositionsOrbit();
  void setupLedPositionsGlove();
  void setupLedPositionsHandle();
  void setupLedPositionsFinger();
  void setupLedPositionsChromadeck();
  void setupLedPositionsSpark();

  class TestFrameworkCallbacks : public VortexCallbacks
  {
  public:
    TestFrameworkCallbacks() {}
    virtual ~TestFrameworkCallbacks() {}
    virtual long checkPinHook(uint32_t pin) override;
    virtual void infraredWrite(bool mark, uint32_t amount) override;
    virtual bool serialCheck() override;
    virtual int32_t serialAvail() override;
    virtual size_t serialRead(char *buf, size_t amt) override;
    virtual uint32_t serialWrite(const uint8_t *buf, size_t len) override;
    virtual void ledsInit(void *cl, int count) override;
    virtual void ledsBrightness(int brightness) override;
    virtual void ledsShow() override;
  };

  // the callbacks instance that is returned by init
  TestFrameworkCallbacks *m_pCallbacks;

  static void buttonClickCallback(void *arg, VButton *window, VButton::ButtonEvent type) {
    ((TestFramework *)arg)->buttonClick(window, type);
  }
  static void longClickCallback(void *arg, VButton *window, VButton::ButtonEvent type) {
    if (type == VButton::ButtonEvent::BUTTON_EVENT_CLICK) {
      ((TestFramework *)arg)->longClick(window, 0);
    }
  }
  static void longClickCallback2(void *arg, VButton *window, VButton::ButtonEvent type) {
    if (type == VButton::ButtonEvent::BUTTON_EVENT_CLICK) {
      ((TestFramework *)arg)->longClick(window, 1);
    }
  }
  static void launchIRCallback(void *arg, VButton *window, VButton::ButtonEvent type) {
    ((TestFramework *)arg)->launchIR(window, type);
  }
  static void generatePatsCallback(void *arg, VButton *window, VButton::ButtonEvent type) {
    ((TestFramework *)arg)->genPats(window, type);
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
  void longClick(VButton *window, uint32_t buttonIndex);
  void launchIR(VButton *window, VButton::ButtonEvent type);
  void genPats(VButton *window, VButton::ButtonEvent type);
  void patternStripSelect(uint32_t x, uint32_t y, VSelectBox::SelectEvent sevent);
  void ledClick(VWindow *window);
  void setTickrate(uint32_t x, uint32_t y, VSelectBox::SelectEvent sevent);

  bool generatePatternBMP(const std::string &filename, uint32_t numStrips = 100);
  bool writeBMPtoFile(const std::string &filename, uint32_t bitmapWidth, uint32_t bitmapHeight, COLORREF *cols);

  static const uint32_t width = LED_COUNT == 28 ? 610 : 460;
  static const uint32_t height = 460;

  static const uint32_t patternStripHeight = 30;

  static const uint32_t tickrateSliderWidth = 24;
  static const uint32_t tickrateSliderHeight = 260;

  // how many times the length of the pattern strip is extended
  // in order to simulate the scrolling effect
  static const uint32_t patternStripExtensionMultiplier = 16;

  // new stuff
  VWindow m_window;
  VSelectBox m_orbitBox;
  VSelectBox m_gloveBox;
  VSelectBox m_handleBox;
  VSelectBox m_fingerBox;
  VSelectBox m_chromadeckBox;
  VSelectBox m_sparkBox;
  VSelectBox m_patternStrip;
  VSelectBox m_tickrateSlider;
  VButton m_button;
  VButton m_button2;
  VButton m_button3;
  VButton m_button4;
  VButton m_IRLaunchButton;
  VButton m_generatePats;
  VCircle m_leds[LED_COUNT];
  RECT m_ledPos[LED_COUNT];

  HANDLE m_pauseMutex;

  HINSTANCE m_hInst;
  FILE *m_consoleHandle;
  HBITMAP m_orbitBMP;
  HBITMAP m_gloveBMP;
  HBITMAP m_handleBMP;
  HBITMAP m_fingerBMP;
  HBITMAP m_chromadeckBMP;
  HBITMAP m_sparkBMP;
  HICON m_hIcon;
  HANDLE m_loopThread;

  volatile uint32_t m_tickrate;

  int m_brightness;

  RGBColor *m_ledList;
  uint32_t m_numLeds;
  RGBColor *m_lastLedColor;

  LedPos m_curSelectedLed;

  bool m_initialized;
  bool m_buttonPressed;
  bool m_buttonPressed2;

  bool m_keepGoing;

  volatile bool m_isPaused;

  Mode m_curMode;

  std::map<COLORREF, HBRUSH> m_brushmap;

  HACCEL m_accelTable;
};

extern TestFramework *g_pTestFramework;
#endif
