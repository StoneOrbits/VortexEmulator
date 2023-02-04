#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <ctime>

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

using namespace std;

TestFramework::TestFramework() :
  m_logHandle(NULL),
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
  return true;
}

unsigned long do_release = 0;
void TestFramework::run()
{
  while (m_initialized && m_keepGoing) {
    VortexEngine::tick();
    g_pButton->m_longClick = false;
    g_pButton->m_isPressed = false;
    g_pButton->m_shortClick = false;
    g_pButton->m_holdDuration = 0;
  }

  VortexEngine::cleanup();
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
  vfprintf(g_pTestFramework->m_logHandle, strMsg.c_str(), list2);
}

void TestFramework::injectButtons()
{
  int ch = getchar();
  if (ch == 0) {
    return;
  }
  switch (ch) {
  case 'a':
    printf("short click\n");
    g_pButton->m_newRelease = true;
    g_pButton->m_shortClick = true;
    g_pButton->m_pressTime = Time::getCurtime();
    g_pButton->m_holdDuration = 200;
    break;
  case 's':
    printf("long click\n");
    g_pButton->m_newRelease = true;
    g_pButton->m_longClick = true;
    g_pButton->m_pressTime = Time::getCurtime();
    g_pButton->m_holdDuration = SHORT_CLICK_THRESHOLD_TICKS + 1;
    break;
  case 'd':
    printf("menu enter click\n");
    g_pButton->m_longClick = true;
    g_pButton->m_isPressed = true;
    g_pButton->m_holdDuration = MENU_TRIGGER_THRESHOLD_TICKS + 1;
    break;
  case 'q':
    cleanup();
    break;
  case 'f':
    printf("toggle\n");
    if (m_buttonPressed) {
      releaseButton();
    } else {
      pressButton();
    }
    break;
  default:
    // do nothing
    break;
  }
}
