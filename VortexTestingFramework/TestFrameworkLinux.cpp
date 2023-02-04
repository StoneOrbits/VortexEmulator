#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <ctime>

#include <stdio.h>

#include <emscripten/html5.h>
#include <emscripten.h>

#include "TestFrameworkLinux.h"
#include "Arduino.h"

#include "Log/Log.h"

#include "VortexEngine.h"

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

EM_BOOL key_callback(int eventType, const EmscriptenKeyboardEvent *e, void *userData)
{
  if (eventType == EMSCRIPTEN_EVENT_KEYDOWN) {
    if (!strcmp(e->key, "a")) {
      g_pTestFramework->pressButton();
    }
  }
  if (eventType == EMSCRIPTEN_EVENT_KEYUP) {
    if (!strcmp(e->key, "a")) {
      g_pTestFramework->releaseButton();
    }
  }
  return 0;
}

void do_run(void *arg)
{
  TestFramework *tf = (TestFramework *)arg;
  tf->run();
}

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
  arduino_setup();
  m_initialized = true;

  //emscripten_set_keypress_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, 1, key_callback);
  emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, 1, key_callback);
  emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, 1, key_callback);

  emscripten_set_main_loop_arg(do_run, this, 1000, false);

  return true;
}

void TestFramework::run()
{
  if (!stillRunning()) {
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
}

void TestFramework::arduino_setup()
{
  // init the drop-in arduino library replacement
  init_arduino();
  if (!VortexEngine::init()) {
    // uhoh
  }
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
  printf("Press\r\n");
  m_buttonPressed = true;
}

void TestFramework::releaseButton()
{
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

void TestFramework::injectButtons()
{
}

bool TestFramework::stillRunning() const
{
  return (m_initialized && m_keepGoing);
}
