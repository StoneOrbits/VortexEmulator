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
#include "Colors/ColorTypes.h"
#include "Colors/Colorset.h"
#include "Buttons/Button.h"
#include "Time/Timings.h"
#include "Menus/Menus.h"
#include "Modes/Modes.h"
#include "Modes/Mode.h"

#include "Patterns/Pattern.h"
#include "Patterns/single/SingleLedPattern.h"

TestFramework *g_pTestFramework = nullptr;

using namespace std;

#define USAGE   "a = short press | s = med press | d = enter ringmenu | f = toggle pressed | q = quit"

#ifdef WASM // Web assembly glue
#include <emscripten/html5.h>
#include <emscripten.h>

static EM_BOOL key_callback(int eventType, const EmscriptenKeyboardEvent *e, void *userData)
{
  switch (e->key[0]) {
  case 'a':
    if (eventType == EMSCRIPTEN_EVENT_KEYDOWN) {
      Vortex::shortClick();
    }
    break;
  case 's':
    if (eventType == EMSCRIPTEN_EVENT_KEYDOWN) {
      Vortex::longClick();
    }
    break;
  case 'd':
    if (eventType == EMSCRIPTEN_EVENT_KEYDOWN) {
      Vortex::menuEnterClick();
    }
    break;
  case 'f':
    if (eventType == EMSCRIPTEN_EVENT_KEYDOWN) {
      Vortex::pressButton();
    } else if (eventType == EMSCRIPTEN_EVENT_KEYUP) {
      Vortex::releaseButton();
    }
    break;
  case 'q':
    if (eventType == EMSCRIPTEN_EVENT_KEYDOWN) {
      Vortex::quitClick();
    }
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
  emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, 1, key_callback);
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
}

class TestFrameworkCallbacks : public VortexCallbacks
{
public:
  TestFrameworkCallbacks() :
    VortexCallbacks(),
    m_leds(nullptr),
    m_count(0)
  {
  }
  virtual ~TestFrameworkCallbacks() {}

  // called when the LED strip is initialized
  virtual void ledsInit(void *cl, int count) override
  {
    //g_pTestFramework->installLeds((RGBColor *)cl, count);
    m_leds = (RGBColor *)cl;
    m_count = count;
  }
  // called when the brightness is changed
  virtual void ledsBrightness(int brightness) override
  {
    //g_pTestFramework->setBrightness(brightness);
  }
  // called when the leds are shown
  virtual void ledsShow() override
  {
    string out;
#ifdef WASM
    for (uint32_t i = 0; i < m_count; ++i) {
      char buf[128] = {0};
      snprintf(buf, sizeof(buf), "#%06X|", m_leds[i].raw());
      out += buf;
    }
    out += "\n";
#else
    out += "\33[2K\033[A\r";
    for (uint32_t i = 0; i < m_count; ++i) {
      out += "\x1B[0m|"; // opening |
      out += "\x1B[48;2;"; // colorcode start
      out += to_string(m_leds[i].red) + ";"; // col red
      out += to_string(m_leds[i].green) + ";"; // col green
      out += to_string(m_leds[i].blue) + "m"; // col blue
      out += "  "; // colored space
      out += "\x1B[0m|"; // ending |
    }
    out += USAGE;
#endif
    printf("%s", out.c_str());
    fflush(stdout);
  }

private:
  // receive a message from client
  RGBColor *m_leds;
  uint32_t m_count;
};

bool TestFramework::init()
{
  if (g_pTestFramework) {
    return false;
  }
  g_pTestFramework = this;

#ifndef WASM
  printf("Initializing...\n");
#endif

  // do the arduino init/setup
  Vortex::init<TestFrameworkCallbacks>();
  m_initialized = true;

  printf("Initialized!\n");
  printf("%s\n", USAGE);

#ifndef WASM
#else
  wasm_init();
#endif

  return true;
}

void TestFramework::run()
{
  if (!stillRunning()) {
    return;
  }
  if (!Vortex::tick()) {
    cleanup();
  }
}

void TestFramework::cleanup()
{
  DEBUG_LOG("Quitting...");
  m_keepGoing = false;
  m_isPaused = false;
  Vortex::cleanup();
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

bool TestFramework::isButtonPressed() const
{
  return Vortex::isButtonPressed();
}

bool TestFramework::stillRunning() const
{
  return (m_initialized && m_keepGoing);
}
