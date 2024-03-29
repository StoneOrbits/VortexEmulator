#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <ctime>
#include <map>

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

#include "TestFrameworkLinux.h"

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
#include "Patterns/Single/SingleLedPattern.h"

#define RECORD_FILE "recorded_input.txt"

TestFramework *g_pTestFramework = nullptr;

using namespace std;

// the usage for the input strings
const char *input_usage[] = {
  "\n   c         standard short click",
  "\n   l         standard long click",
  "\n   m         open menus length click",
  "\n   a         enter adv menu length click (enter adv menu from menus)",
  "\n   d         delete length click (delete color in col select)",
  "\n   s         enter sleep length click (enter sleep at main modes)",
  "\n   f         force sleep length click (force sleep anywhere)",
  "\n   t         toggle button pressed (only way to wake after sleep)",
  "\n   r         rapid button click (ex: r15)",
  "\n   w         wait 1 tick",
  "\n   <digits>  repeat command n times (only single digits in -i mode)",
  "\n   q         quit",
};

#define NUM_USAGE (sizeof(input_usage) / sizeof(input_usage[0]))

// the usage for the input strings but in a brief format
const char *input_usage_brief[NUM_USAGE] = {
  "\n   c         short",
  "\n   l         long",
  "\n   m         menus",
  "\n   a         adv menu",
  "\n   d         delete col",
  "\n   s         sleep",
  "\n   f         force sleep",
  "\n   t         toggle",
  "\n   r         rapid",
  "\n   w         wait",
  "\n   <digits>  repeat",
  "\n   q         quit",
};


// 1000 spaces, used for efficiently printing any number of spaces up to 1000 by
// offsetting from the start of this string with the SPACES() macro below
#define SPACESTR "                                                                                                                                                     " \
                 "                                                                                                                                                     " \
                 "                                                                                                                                                     " \
                 "                                                                                                                                                     " \
                 "                                                                                                                                                     " \
                 "                                                                                                                                                     " \
                 "                                                                                                                                                     " \
                 "                                                                                                                                                     " \
                 "                                                                                                                                                     " \
                 "                                                                                                                                                     "

// 1000 hyphens for a long line
#define LINESTR  "-----------------------------------------------------------------------------------------------------------------------------------------------------" \
                 "-----------------------------------------------------------------------------------------------------------------------------------------------------" \
                 "-----------------------------------------------------------------------------------------------------------------------------------------------------" \
                 "-----------------------------------------------------------------------------------------------------------------------------------------------------" \
                 "-----------------------------------------------------------------------------------------------------------------------------------------------------" \
                 "-----------------------------------------------------------------------------------------------------------------------------------------------------" \
                 "-----------------------------------------------------------------------------------------------------------------------------------------------------" \
                 "-----------------------------------------------------------------------------------------------------------------------------------------------------" \
                 "-----------------------------------------------------------------------------------------------------------------------------------------------------"

// some amount of spaces, if you pass in an amount greater than 1000 you will die
#define SPACES(amt) SPACESTR + (sizeof(SPACESTR) - (amt))
// some length of line, if you pass in an amount greater than 1000 you will die
#define LINE(amt) LINESTR + (sizeof(LINESTR) - (amt))

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
  m_outputType(OUTPUT_TYPE_NONE),
  m_noTimestep(false),
  m_lockstep(false),
  m_inPlace(false),
  m_record(false),
  m_storage(false),
  m_sleepEnabled(true),
  m_lockEnabled(true),
  m_storageFile("FlashStorage.flash"),
  m_patternIDStr(),
  m_colorsetStr(),
  m_argumentsStr(),
  m_pipe_fd{-1, -1},
  m_saved_stdin(),
  m_inputBuffer()
{
}

TestFramework::~TestFramework()
{
}

static struct option long_options[] = {
  {"hex", no_argument, nullptr, 'x'},
  {"color", no_argument, nullptr, 'c'},
  {"no-timestep", no_argument, nullptr, 't'},
  {"lockstep", no_argument, nullptr, 'l'},
  {"in-place", no_argument, nullptr, 'i'},
  {"record", no_argument, nullptr, 'r'},
  {"autowake", no_argument, nullptr, 'a'},
  {"nolock", no_argument, nullptr, 'n'},
  {"storage", optional_argument, nullptr, 's'},
  {"pattern", required_argument, nullptr, 'P'},
  {"colorset", required_argument, nullptr, 'C'},
  {"arguments", required_argument, nullptr, 'A'},
  {"help", no_argument, nullptr, 'h'},
  {nullptr, 0, nullptr, 0}
};

// all the available colors that can be used to make colorsets
std::map<std::string, int> color_map = {
    {"black",   0x000000},
    {"white",   0xFFFFFF},
    {"red",     0xFF0000},
    {"lime",    0x00FF00},
    {"blue",    0x0000FF},
    {"yellow",  0xFFFF00},
    {"cyan",    0x00FFFF},
    {"magenta", 0xFF00FF},
    {"silver",  0xC0C0C0},
    {"gray",    0x808080},
    {"maroon",  0x800000},
    {"olive",   0x808000},
    {"green",   0x008000},
    {"purple",  0x800080},
    {"teal",    0x008080},
    {"navy",    0x000080},
    {"orange",  0xFFA500},
    {"pink",    0xFFC0CB},
    {"brown",   0xA52A2A}
    //...add more as needed
};

static void print_usage(const char* program_name) 
{
  fprintf(stderr, "Usage: %s [options] < input commands\n", program_name);
  fprintf(stderr, "Output Selection (at least one required):\n");
  fprintf(stderr, "  -x, --hex                Use hex values to represent led colors\n");
  fprintf(stderr, "  -c, --color              Use console color codes to represent led colors\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Engine Control Flags (optional):\n");
  fprintf(stderr, "  -t, --no-timestep        Bypass the timestep and run as fast as possible\n");
  fprintf(stderr, "  -l, --lockstep           Only step once each time an input is received\n");
  fprintf(stderr, "  -i, --in-place           Print the output in-place (interactive mode)\n");
  fprintf(stderr, "  -r, --record             Record the inputs and dump to a file after (" RECORD_FILE ")\n");
  fprintf(stderr, "  -a, --autowake           Automatically and instantly wake on sleep (disable sleep)\n");
  fprintf(stderr, "  -n, --nolock             Automatically unlock upon locking the chip (disable lock)\n");
  fprintf(stderr, "  -s, --storage [file]     Persistent storage to file (default file: FlashStorage.flash)\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Initial Pattern Options (optional):\n");
  fprintf(stderr, "  -P, --pattern <id>       Preset the pattern ID on the first mode\n");
  fprintf(stderr, "  -C, --colorset c1,c2...  Preset the colorset on the first mode (csv list of hex codes or color names)\n");
  fprintf(stderr, "  -A, --arguments a1,a2... Preset the arguments on the first mode (csv list of arguments)\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Other Options:\n");
  fprintf(stderr, "  -h, --help               Display this help message\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Input Commands (pass to stdin):");
  for (uint32_t i = 0; i < NUM_USAGE; ++i) {
    fprintf(stderr, "%s", input_usage[i]);
  }
  fprintf(stderr, "\n");
  fprintf(stderr, "Example Usage:\n");
  fprintf(stderr, "   ./vortex -ci\n");
  fprintf(stderr, "   ./vortex -ci -P42 -Ccyan,purple\n");
  fprintf(stderr, "   ./vortex -ct -P0 -Cred,green -A1,2 <<< w10q\n");
}

struct termios orig_term_attr = {0};
struct winsize terminal_size = {0};

static void restore_terminal()
{
  tcsetattr(STDIN_FILENO, TCSANOW, &orig_term_attr);
}

void set_terminal_nonblocking()
{
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

void TestFramework::get_terminal_size()
{
  struct winsize new_terminal_size = { 0 };
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &new_terminal_size);
  if (terminal_size.ws_col != new_terminal_size.ws_col) {
    terminal_size = new_terminal_size;
    if (m_inPlace && !system("clear")) {
      // failure but don't care
    }
  }
}

bool TestFramework::init(int argc, char *argv[])
{
  if (g_pTestFramework) {
    return false;
  }
  g_pTestFramework = this;

  int opt = -1;
  int option_index = 0;
  while ((opt = getopt_long(argc, argv, "xctliransP:C:A:h", long_options, &option_index)) != -1) {
    switch (opt) {
    case 'x':
      // if the user wants pretty colors or hex codes
      m_outputType = OUTPUT_TYPE_HEX;
      break;
    case 'c':
      // if the user wants pretty colors
      m_outputType = OUTPUT_TYPE_COLOR;
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
    case 'a':
      // autuowake prevents sleep
      m_sleepEnabled = false;
      break;
    case 'n':
      // disable the lock
      m_lockEnabled = false;
      break;
    case 's':
      // enable persistent storage to file
      m_storage = true;
      if (optarg) {
        m_storageFile = optarg;
      }
      break;
    case 'P':
      // preset the pattern ID on the first mode
      m_patternIDStr = optarg;
      break;
    case 'C':
      // preset the colorset on the first mode
      m_colorsetStr = optarg;
      break;
    case 'A':
      // preset the arguments on the first mode
      m_argumentsStr = optarg;
      break;
    case 'h':
      // print usage and exit
      print_usage(argv[0]);
      exit(EXIT_SUCCESS);
    default: // '?' for unrecognized options
      printf("Unknown arg: -%c\n", opt);
      exit(EXIT_FAILURE);
    }
  }

  switch (m_outputType) {
  case OUTPUT_TYPE_NONE:
    print_usage(argv[0]);
    exit(EXIT_SUCCESS);
    break;
  case OUTPUT_TYPE_COLOR:
    setColoredOutput(true);
    break;
  case OUTPUT_TYPE_HEX:
    setHexOutput(true);
    break;
  }

  // do the vortex init/setup
  Vortex::init<TestFrameworkCallbacks>();

  // configure the vortex engine as the parameters dictate
  Vortex::setInstantTimestep(m_noTimestep);
  Vortex::enableCommandLog(m_record);
  Vortex::enableLockstep(m_lockstep);
  Vortex::enableStorage(m_storage);
  if (m_storage) {
    Vortex::setStorageFilename(m_storageFile);
    if (access(m_storageFile.c_str(), F_OK) == 0) {
      // load storage if the file exists
      Vortex::loadStorage();
    }
  }
  Vortex::setSleepEnabled(m_sleepEnabled);
  Vortex::setLockEnabled(m_lockEnabled);

  if (m_patternIDStr.length() > 0) {
    PatternID id = (PatternID)strtoul(m_patternIDStr.c_str(), nullptr, 10);
    // TODO: add arg for the led position
    Vortex::setPatternAt(LED_ALL, id);
  }
  if (m_colorsetStr.length() > 0) {
    stringstream ss(m_colorsetStr);
    string color;
    Colorset set;
    while (getline(ss, color, ',')) {
      // iterate letters and lowercase them
      transform(color.begin(), color.end(), color.begin(), [](unsigned char c){ return tolower(c); });
      if (color_map.count(color) > 0) {
        set.addColor(color_map[color]);
      } else {
        set.addColor(strtoul(color.c_str(), nullptr, 16));
      }
    }
    // TODO: add arg for the led position
    Vortex::setColorset(LED_ALL, set);
  }
  if (m_argumentsStr.length() > 0) {
    stringstream ss(m_argumentsStr);
    string arg;
    PatternArgs args;
    while (getline(ss, arg, ',')) {
      args.args[args.numArgs++] = strtoul(arg.c_str(), nullptr, 10);
    }
    // TODO: add arg for the led position
    Vortex::setPatternArgs(LED_ALL, args);
  }
  if (m_inPlace && !system("clear")) {
    printf("Failed to clear\n");
  }

  set_terminal_nonblocking();
  get_terminal_size();

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
  get_terminal_size();
  uint32_t wid = terminal_size.ws_col;// & 0xFFFFFFFC;
  uint32_t odd = (wid) % 2;
  uint32_t halfwid = wid / 2;
  uint32_t midWid = (halfwid - ((2 + (m_outputType == OUTPUT_TYPE_HEX)) * LED_COUNT)) - 1;
  if (m_inPlace) {
    // this resets the cursor back to the beginning of the line and moves it up 12 lines
    out += "\33[2K\033[17A\r";
    // this is the top border line
    out += "+";
    out += LINE((wid + odd) - 2);
    out += "+\n|";
    // this is the left inner line
    out += LINE(midWid + odd);
    out += "=";
  }
  if (m_outputType == OUTPUT_TYPE_COLOR) {
    // the color strip itself
    for (uint32_t i = 0; i < m_numLeds; ++i) {
      out += "\x1B[0m["; // opening |
      out += "\x1B[48;2;"; // colorcode start
      out += to_string(m_ledList[i].red) + ";"; // col red
      out += to_string(m_ledList[i].green) + ";"; // col green
      out += to_string(m_ledList[i].blue) + "m"; // col blue
      out += "  "; // colored space
      out += "\x1B[0m]"; // ending |
    }
  } else if (m_outputType == OUTPUT_TYPE_HEX) {
    // otherwise this just prints out the raw hex code if not in color mode
    for (uint32_t i = 0; i < m_numLeds; ++i) {
      char buf[128] = { 0 };
      snprintf(buf, sizeof(buf), "%06X", m_ledList[i].raw());
      out += buf;
    }
  } else { // OUTPUT_TYPE_NONE
    // do nothing
  }
  if (!m_inPlace) {
    out += "\n";
  } else {
    // the right inner line
    out += "=";
    out += LINE((midWid - 1) + odd);
    // the end of middle line, fold and start of 3rd line
    out += "|\n+";
    out += LINE((wid + odd) - 2);
    // space line after the box and before usage
    out += "+\n";
    out += SPACES(wid + odd);
    // the usage message
    for (uint32_t i = 0; i < NUM_USAGE; ++i) {
      const char *brief = (wid < 70) ? input_usage_brief[i] : input_usage[i];
      out += brief;
      out += SPACES((wid + odd + 1) - strlen(brief));
    }
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
  if (pin == 20) {
    // orbit button 2
    return Vortex::isButtonPressed(1) ? 0 : 1;
  }
  return Vortex::isButtonPressed(0) ? 0 : 1;
}

void TestFramework::TestFrameworkCallbacks::ledsInit(void *cl, int count)
{
  g_pTestFramework->installLeds((CRGB *)cl, count);
}

void TestFramework::TestFrameworkCallbacks::ledsShow()
{
  g_pTestFramework->show();
}
