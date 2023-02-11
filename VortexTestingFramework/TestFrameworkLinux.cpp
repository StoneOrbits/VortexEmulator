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
  if (e->key[0] == ' ') {
    if (eventType == EMSCRIPTEN_EVENT_KEYDOWN) {
      Vortex::pressButton();
    } else if (eventType == EMSCRIPTEN_EVENT_KEYUP) {
      Vortex::releaseButton();
    }
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

  printf("Initialized!\n");
  printf("%s\n", USAGE);

  m_initialized = true;

#ifndef WASM
#else
  // NOTE: This call does not return and will instead automatically 
  // call the TestFramework::run() in a loop
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
  string out;
#ifdef WASM
  for (uint32_t i = 0; i < m_numLeds; ++i) {
    char buf[128] = { 0 };
    snprintf(buf, sizeof(buf), "#%06X|", m_ledList[i].raw());
    out += buf;
  }
  out += "\n";
#else
  out += "\33[2K\033[A\r";
  for (uint32_t i = 0; i < m_numLeds; ++i) {
    out += "\x1B[0m|"; // opening |
    out += "\x1B[48;2;"; // colorcode start
    out += to_string(m_ledList[i].red) + ";"; // col red
    out += to_string(m_ledList[i].green) + ";"; // col green
    out += to_string(m_ledList[i].blue) + "m"; // col blue
    out += "  "; // colored space
    out += "\x1B[0m|"; // ending |
  }
  out += USAGE;
#endif
  printf("%s", out.c_str());
  fflush(stdout);
}

bool TestFramework::isButtonPressed() const
{
  return Vortex::isButtonPressed();
}

bool TestFramework::stillRunning() const
{
  return (m_initialized && m_keepGoing);
}

void TestFramework::installLeds(CRGB *leds, uint32_t count)
{
  m_ledList = (RGBColor *)leds;
  m_numLeds = count;
}

long TestFramework::TestFrameworkCallbacks::checkPinHook(uint32_t pin)
{
  // TODO: check realtime key press? ncurses?
  return HIGH;
}

void TestFramework::TestFrameworkCallbacks::ledsInit(void *cl, int count)
{
  g_pTestFramework->installLeds((CRGB *)cl, count);
}

void TestFramework::TestFrameworkCallbacks::ledsShow()
{
  g_pTestFramework->show();
}
