#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <ctime>

#include "TestFrameworkLinux.h"
#include "Arduino.h"

#include "Log.h"

#include "VortexFramework.h"
#include "PatternBuilder.h"
#include "ModeBuilder.h"
#include "TimeControl.h"
#include "Colorset.h"
#include "Modes.h"
#include "Mode.h"

#include "patterns/Pattern.h"
#include "patterns/SingleLedPattern.h"

#include <ncurses.h>

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
  endwin();
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
  initscr();
  cbreak();
  noecho();
  nodelay(stdscr, TRUE);
  scrollok(stdscr, TRUE);
  refresh();

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
    int ch = getch();
    if (do_release && (millis() > do_release)) {
      do_release = 0;
      releaseButton();
    } else {
      if (ch != ERR) {
        switch (ch) {
        case 'a':
          pressButton();
          do_release = millis();
          break;
        case 's':
          pressButton();
          do_release = millis() + 1100;
          break;
        case 'f':
          pressButton();
          do_release = millis() + 3100;
          break;
        case 'q':
          cleanup();
          break;
        case 'd':
        default:
          if (m_buttonPressed) {
            releaseButton();
          } else {
            pressButton();
          }
        }
      }
    }
    refresh();

    VortexFramework::tick();
  }
}

void TestFramework::cleanup()
{
  DEBUG("Quitting...");
  m_keepGoing = false;
  m_isPaused = false;
}

void TestFramework::arduino_setup()
{
  // init the drop-in arduino library replacement
  init_arduino();
  if (!VortexFramework::init()) {
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
  string strMsg = file;
  if (strMsg.find_last_of('\\') != string::npos) {
    strMsg = strMsg.substr(strMsg.find_last_of('\\') + 1);
  }
  strMsg += ":";
  strMsg += to_string(line);
  strMsg += " ";
  strMsg += func;
  strMsg += "(): ";
  strMsg += msg;
  strMsg += "\r\n";
  va_list list2;
  va_copy(list2, vlst);
  vfprintf(stdout, strMsg.c_str(), vlst);
  vfprintf(g_pTestFramework->m_logHandle, strMsg.c_str(), list2);
}
