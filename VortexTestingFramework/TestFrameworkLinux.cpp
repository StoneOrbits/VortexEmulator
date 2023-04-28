#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <ctime>

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <fcntl.h>
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

#define RECORD_FILE "recorded_input.txt"

TestFramework *g_pTestFramework = nullptr;

using namespace std;

#define USAGE   "\n[a] click                                     " \
                "\n[s] long click                                " \
                "\n[d] enter menus                               " \
                "\n[f] toggle pressed                            " \
                "\n[w] wait                                      " \
                "\n[<digit>] repeat last command n times         " \
                "\n[q] quit                                      "

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
  // turn colored output off in the wasm version
  g_pTestFramework->setColoredOuptut(false);
}
#endif // ifdef WASM

TestFramework::TestFramework() :
  m_numLeds(0),
  m_initialized(false),
  m_buttonPressed(false),
  m_keepGoing(true),
  m_isPaused(true),
  m_curPattern(PATTERN_FIRST),
  m_curColorset(),
  m_coloredOutput(false),
  m_noTimestep(false),
  m_lockstep(false),
  m_inPlace(false),
  m_record(false)
{
}

TestFramework::~TestFramework()
{
}

// Define the long options
static struct option long_options[] = {
  {"color", no_argument, nullptr, 'c'},
  {"no-timestep", no_argument, nullptr, 't'},
  {"in-place", no_argument, nullptr, 'i'},
  {"record", no_argument, nullptr, 'r'},
  {nullptr, 0, nullptr, 0}
};

static void print_usage(const char* program_name) 
{
  fprintf(stderr, "Usage: %s [options]\n", program_name);
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -c, --color            Use console color codes to represent led colors\n");
  fprintf(stderr, "  -t, --no-timestep      Bypass the timestep and run as fast as possible\n");
  fprintf(stderr, "  -l, --lockstep         Only step once each time an input is received\n");
  fprintf(stderr, "  -i, --in-place         Print the output in-place (interactive mode)\n");
  fprintf(stderr, "  -r, --record           Record the inputs and dump to a file after (" RECORD_FILE ")\n");
  fprintf(stderr, "  -h, --help             Display this help message\n");
}

struct termios orig_term_attr = {0};

static void restore_terminal()
{
  tcsetattr(STDIN_FILENO, TCSANOW, &orig_term_attr);
}

void set_terminal_nonblocking() {
  struct termios term_attr = {0};

  // Get the current terminal attributes and store them
  tcgetattr(STDIN_FILENO, &term_attr);
  orig_term_attr = term_attr;

  // Set the terminal in non-canonical mode (raw mode)
  term_attr.c_lflag &= ~(ICANON | ECHO);

  // Set the minimum number of input bytes read at a time to 1
  term_attr.c_cc[VMIN] = 1;

  // Set the timeout for read to 0 (no waiting)
  term_attr.c_cc[VTIME] = 0;

  // Apply the new terminal attributes
  tcsetattr(STDIN_FILENO, TCSANOW, &term_attr);

  // Set the terminal to non-blocking mode
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

  // Register the restore_terminal function to be called at exit
  atexit(restore_terminal);
}

bool TestFramework::init(int argc, char *argv[])
{
  if (g_pTestFramework) {
    return false;
  }
  g_pTestFramework = this;

  int opt;
  int option_index = 0;
  while ((opt = getopt_long(argc, argv, "ctirh", long_options, &option_index)) != -1) {
    switch (opt) {
    case 'c':
      // if the user wants pretty colors
      m_coloredOutput = true;
      break;
    case 't':
      // if the user wants to bypass timestep
      m_noTimestep = true;
      break;
    case 'l':
      // if the user wants to step in lockstep with the engine
      m_lockstep = true;
      break;
    case 'i':
      // if the user wants to print in-place (on one line)
      m_inPlace = true;
      break;
    case 'r':
      // record the inputs and dump them to a file after
      m_record = true;
      break;
    case 'h':
      // print usage and exit
      print_usage(argv[0]);
      exit(EXIT_SUCCESS);
    default: // '?' for unrecognized options
      print_usage(argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  // do the arduino init/setup
  Vortex::init<TestFrameworkCallbacks>();

  // whether to tick instantly or not, and whether to record commands
  Vortex::setInstantTimestep(m_noTimestep);
  Vortex::enableCommandLog(m_record);
  Vortex::enableLockstep(m_lockstep);

  if (m_inPlace && !system("clear")) {
    printf("Failed to clear\n");
  }

  if (m_inPlace) {
    printf("Initialized!\n");
    printf("%s\n", USAGE);
  }

  set_terminal_nonblocking();

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
  if (m_inPlace) {
    printf("\n");
  }
  if (m_record) {
    // Open the file in write mode
    FILE *outputFile = fopen(RECORD_FILE, "w");
    if (outputFile) {
      // Print the recorded input to the file
      fprintf(outputFile, "%s", Vortex::getCommandLog().c_str());
      // Close the output file
      fclose(outputFile);
    }
    printf("Wrote recorded input to " RECORD_FILE "\n");
  }
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
  if (m_inPlace) {
    out += "\33[2K\033[8A\r";
  }
  if (m_coloredOutput) {
    for (uint32_t i = 0; i < m_numLeds; ++i) {
      out += "\x1B[0m|"; // opening |
      out += "\x1B[48;2;"; // colorcode start
      out += to_string(m_ledList[i].red) + ";"; // col red
      out += to_string(m_ledList[i].green) + ";"; // col green
      out += to_string(m_ledList[i].blue) + "m"; // col blue
      out += "  "; // colored space
      out += "\x1B[0m|"; // ending |
    }
  } else {
    for (uint32_t i = 0; i < m_numLeds; ++i) {
      char buf[128] = { 0 };
      snprintf(buf, sizeof(buf), "%06X", m_ledList[i].raw());
      out += buf;
    }
  }
  if (!m_inPlace) {
    out += "\n";
  } else {
    out += USAGE;
  }
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
