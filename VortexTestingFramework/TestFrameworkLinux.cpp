#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <ctime>

#include <stdio.h>

#include "TestFrameworkLinux.h"
#include "Arduino.h"

#include "Log/Log.h"

#include "VortexLib.h"

#include "Patterns/PatternBuilder.h"
#include "Time/TimeControl.h"
#include "Colors/Colorset.h"
#include "Buttons/Button.h"
#include "Time/Timings.h"
#include "Menus/Menus.h"
#include "Modes/Modes.h"
#include "Modes/Mode.h"

#include "Patterns/Pattern.h"
#include "Patterns/single/SingleLedPattern.h"

TestFramework *g_pTestFramework = nullptr;

FILE *m_logHandle = nullptr;

using namespace std;

#ifdef WASM // Web assembly glue
#include <emscripten/html5.h>
#include <emscripten.h>

#include <queue>

static queue<char> keyQueue;

static EM_BOOL key_callback(int eventType, const EmscriptenKeyboardEvent *e, void *userData)
{
    switch (e->key[0]) {
    case 'a':
    case 's':
    case 'd':
    case 'q':
      keyQueue.push(e->key[0]);
      break;
    default:
      break;
  }
  return 0;
}

static void do_run()
{
  g_pTestFramework->run();
}

static void wasm_init()
{
  emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, 1, key_callback);
  //emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, 1, key_callback);
  emscripten_set_main_loop(do_run, 0, true);
}
#endif // ifdef WASM

TestFramework::TestFramework() :
  m_ledList(nullptr),
  m_numLeds(0),
  m_initialized(false),
  m_buttonPressed(false),
  m_keepGoing(true),
  m_isPaused(true),
  m_curPattern(PATTERN_FIRST),
  m_curColorset()
{
}

TestFramework::~TestFramework()
{
  fclose(m_logHandle);
}

class TestFrameworkCallbacks : public VortexCallbacks
{
public:
  TestFrameworkCallbacks()
  {
  }
  virtual ~TestFrameworkCallbacks() {}

  // called when engine reads digital pins, use this to feed button presses to the engine
  virtual long checkPinHook(uint32_t pin) override
  {
    return g_pTestFramework->isButtonPressed();
  }
  virtual bool injectButtonsHook(VortexButtonEvent &buttonEvent) override
  {
    int ch = 0;
#ifndef WASM
    ch = getchar();
#else
    if (!keyQueue.size()) {
      return false;
    }
    ch = keyQueue.front();
    keyQueue.pop();
#endif
    if (ch == 0) {
      return false;
    }
    switch (ch) {
    case 'a':
      printf("short click\n");
      buttonEvent.m_newRelease = true;
      buttonEvent.m_shortClick = true;
      buttonEvent.m_pressTime = Time::getCurtime();
      buttonEvent.m_holdDuration = 200;
      break;
    case 's':
      printf("long click\n");
      buttonEvent.m_newRelease = true;
      buttonEvent.m_longClick = true;
      buttonEvent.m_pressTime = Time::getCurtime();
      buttonEvent.m_holdDuration = SHORT_CLICK_THRESHOLD_TICKS + 1;
      break;
    case 'd':
      printf("menu enter click\n");
      buttonEvent.m_longClick = true;
      buttonEvent.m_isPressed = true;
      buttonEvent.m_holdDuration = MENU_TRIGGER_THRESHOLD_TICKS + 1;
      break;
    case 'q':
      g_pTestFramework->cleanup();
      break;
    case 'f':
      printf("toggle\n");
      if (g_pTestFramework->isButtonPressed()) {
        g_pTestFramework->releaseButton();
      } else {
        g_pTestFramework->pressButton();
      }
      break;
    default:
      // do nothing
      break;
    }
    return false;
  }
  // called when the LED strip is initialized
  virtual void ledsInit(void *cl, int count) override
  {
    //g_pTestFramework->installLeds((CRGB *)cl, count);
  }
  // called when the brightness is changed
  virtual void ledsBrightness(int brightness) override
  {
    //g_pTestFramework->setBrightness(brightness);
  }
  // called when the leds are shown
  virtual void ledsShow() override
  {
    //g_pTestFramework->show();
  }

private:
  // receive a message from client
};

bool TestFramework::init()
{
  if (g_pTestFramework) {
    return false;
  }
  g_pTestFramework = this;

  if (!m_logHandle) {
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);
    ostringstream oss;
    char buf[256];
    strftime(buf, sizeof(buf), "%d-%m-%Y-%H-%M-%S", tmp);
    string timestr = buf;
    string filename = "vortex-test-framework-log." + timestr + ".txt";
    m_logHandle = fopen(filename.c_str(), "w");
  }

  printf("Initialized\r\n");
  printf("  a = short press\r\n");
  printf("  s = med press\r\n");
  printf("  d = variable press\r\n");

  // do the arduino init/setup
  Vortex::init<TestFrameworkCallbacks>();
  m_initialized = true;

#ifdef WASM
  wasm_init();
#endif

  return true;
}

void TestFramework::run()
{
  if (!stillRunning()) {
    printf("Cleaning up...\n");
    VortexEngine::cleanup();
    return;
  }
  VortexEngine::tick();
}

void TestFramework::cleanup()
{
  DEBUG_LOG("Quitting...");
  m_keepGoing = false;
  m_isPaused = false;
#ifdef WASM
  emscripten_force_exit(0);
#endif
}

// when the glove framework calls 'FastLED.show'
void TestFramework::show()
{
  if (!m_initialized) {
    return;
  }
}

void TestFramework::pressButton()
{
  if (m_buttonPressed) {
    return;
  }
  printf("Press\r\n");
  m_buttonPressed = true;
}

void TestFramework::releaseButton()
{
  if (!m_buttonPressed) {
    return;
  }
  printf("Release\r\n");
  m_buttonPressed = false;
}

bool TestFramework::isButtonPressed() const
{
  return m_buttonPressed;
}

void TestFramework::printlog(const char *file, const char *func, int line, const char *msg, va_list vlst)
{
  string strMsg;
  if (file) {
    strMsg = file;
    if (strMsg.find_last_of('\\') != string::npos) {
      strMsg = strMsg.substr(strMsg.find_last_of('\\') + 1);
    }
    strMsg += ":";
    strMsg += to_string(line);
  }
  if (func) {
    strMsg += " ";
    strMsg += func;
    strMsg += "(): ";
  }
  strMsg += msg;
  strMsg += "\r\n";
  va_list list2;
  va_copy(list2, vlst);
  vfprintf(stdout, strMsg.c_str(), vlst);
  vfprintf(m_logHandle, strMsg.c_str(), list2);
}

bool TestFramework::stillRunning() const
{
  return (m_initialized && m_keepGoing);
}
