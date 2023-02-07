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

using namespace std;

#ifdef WASM // Web assembly glue
#include <emscripten/html5.h>
#include <emscripten.h>

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
    // LOW = 0 = pressed
    // HIGH = 1 = unpressed
    return 1;
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
  return false;
}

bool TestFramework::stillRunning() const
{
  return (m_initialized && m_keepGoing);
}
