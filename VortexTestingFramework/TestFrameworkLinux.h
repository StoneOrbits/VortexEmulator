#pragma once

#include "VortexLib.h"

#include "Patterns/Patterns.h"
#include "Colors/ColorTypes.h"
#include "Colors/Colorset.h"
#include "Leds/LedTypes.h"

typedef uint32_t HDC;
typedef uint32_t HINSTANCE;
typedef uint32_t HWND;
typedef uint32_t WPARAM;
typedef uint32_t LPARAM;
typedef uint32_t HBRUSH;
typedef uint32_t DWORD;
typedef uint32_t LRESULT;
typedef uint32_t UINT;

typedef struct tagCRGB CRGB;

// paint callback type
typedef void (*paint_fn_t)(void *, HDC);

class TestFramework
{
public:
  TestFramework();
  ~TestFramework();

  // initialize the test framework
  bool init(int argc, char *argv[]);

  // run the test framework
  void run();
  void cleanup();

  // handlers for the arduino routines
  void show();

  // whether the button is pressed
  bool isButtonPressed() const;

  // whether the test framework is still running
  bool stillRunning() const;

  // setup the array of leds
  void installLeds(CRGB *leds, uint32_t count);

  static void printlog(const char *file, const char *func, int line, const char *msg, va_list list);

  void setColoredOutput(bool output) { m_outputType = OUTPUT_TYPE_COLOR; }
  void setHexOutput(bool output) { m_outputType = OUTPUT_TYPE_HEX; }
  void setNoTimestep(bool timestep) { m_noTimestep = timestep; }
  void setInPlace(bool inplace) { m_inPlace = inplace; }

private:
  class TestFrameworkCallbacks : public VortexCallbacks
  {
  public:
    TestFrameworkCallbacks() {}
    virtual ~TestFrameworkCallbacks() {}
    virtual long checkPinHook(uint32_t pin) override;
    virtual void ledsInit(void *cl, int count) override;
    virtual void ledsShow() override;
  private:
    // receive a message from client
  };

  // internal helper for updating terminal size
  void get_terminal_size();

  // these are in no particular order
  RGBColor *m_ledList;
  uint32_t m_numLeds;
  bool m_initialized;
  bool m_buttonPressed;
  bool m_keepGoing;
  volatile bool m_isPaused;
  PatternID m_curPattern;
  Colorset m_curColorset;
  enum OutputType {
    OUTPUT_TYPE_NONE,
    OUTPUT_TYPE_HEX,
    OUTPUT_TYPE_COLOR,
  };
  OutputType m_outputType;
  bool m_noTimestep;
  bool m_lockstep;
  bool m_inPlace;
  bool m_record;
  bool m_storage;
  bool m_sleepEnabled;
  bool m_lockEnabled;
  std::string m_storageFile;
  std::string m_patternIDStr;
  std::string m_colorsetStr;
  std::string m_argumentsStr;
  // to pipe stuff into the engine
  int m_pipe_fd[2];
  int m_saved_stdin;
  std::string m_inputBuffer;
};

extern TestFramework *g_pTestFramework;
