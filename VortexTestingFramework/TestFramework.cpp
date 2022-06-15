#include <Windows.h>
#include <CommCtrl.h>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <ctime>

#include "TestFramework.h"
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
#include "patterns/single/SingleLedPattern.h"

#pragma comment(lib, "Comctl32.lib")

TestFramework *g_pTestFramework = nullptr;

using namespace std;

#define TrackBar_GetPos(hwnd) \
    (LONG)SendMessage((hwnd), TBM_GETPOS, 0, 0)

#define TrackBar_SetPos(hwnd, pos) \
    (LONG)SendMessage((hwnd), TBM_SETPOS, TRUE, pos)

#define CLICK_BUTTON_ID 10001
#define TICKRATE_SLIDER_ID 10002
#define TIME_OFFSET_SLIDER_ID 10003

TestFramework::TestFramework() :
  m_loopThread(nullptr),
  m_bkbrush(nullptr),
  m_consoleHandle(NULL),
  m_logHandle(NULL),
  m_oldButtonProc(nullptr),
  m_oldSliderProc(nullptr),
  m_hwndClickButton(nullptr),
  m_hwndTickrateSlider(nullptr),
  m_hwndTickOffsetSlider(nullptr),
  m_hwnd(nullptr),
  m_wc(),
  m_brightness(255),
  m_ledPos(),
  m_ledList(nullptr),
  m_numLeds(0),
  m_initialized(false),
  m_buttonPressed(false),
  m_keepGoing(true),
  m_isPaused(false),
  m_pauseMutex(nullptr),
  m_curPattern(PATTERN_FIRST),
  m_curColorset(),
  m_patternStrip(),
  m_redrawStrip(false)
{
}

TestFramework::~TestFramework()
{
  fclose(m_logHandle);
}

bool TestFramework::init(HINSTANCE hInstance)
{
  if (g_pTestFramework) {
    return false;
  }
  g_pTestFramework = this;

  if (!m_consoleHandle) {
    AllocConsole();
    freopen_s(&m_consoleHandle, "CONOUT$", "w", stdout);
  }
  if (!m_logHandle) {
    time_t t = time(nullptr);
    tm tm;
    localtime_s(&tm, &t);
    ostringstream oss;
    oss << std::put_time(&tm, "%d-%m-%Y-%H-%M-%S");
    string filename = "vortex-test-framework-log." + oss.str() + ".txt";
    fopen_s(&m_logHandle, filename.c_str(), "w");
  }

  // create the pause mutex
  m_pauseMutex = CreateMutex(NULL, false, NULL);
  if (!m_pauseMutex) {
    //return false;
  }

  m_bkbrush = CreateSolidBrush(bkcolor);

  // class registration
  memset(&m_wc, 0, sizeof(m_wc));
  m_wc.lpfnWndProc = TestFramework::window_proc;
  m_wc.hInstance = hInstance;
  m_wc.lpszClassName = "Vortex Test Framework";
  m_wc.hbrBackground = m_bkbrush;
  RegisterClass(&m_wc);

  // get desktop rect so we can center the window
  RECT desktop;
  GetClientRect(GetDesktopWindow(), &desktop);

  // create the window
  m_hwnd = CreateWindow(m_wc.lpszClassName, "Vortex Test Framework",
    WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
    (desktop.right / 2) - 240, (desktop.bottom / 2) - 84,
    420, 310, nullptr, nullptr, hInstance, nullptr);
  if (!m_hwnd) {
    MessageBox(nullptr, "Failed to open window", "Error", 0);
    return 0;
  }
  return true;
}

void TestFramework::run()
{
  // launch the 'loop' thread
  m_loopThread = CreateThread(NULL, 0, TestFramework::arduino_loop_thread, this, 0, NULL);
  if (!m_loopThread) {
    // error
    return;
  }
  // main message loop
  MSG msg;
  ShowWindow(m_hwnd, SW_NORMAL);
  while (GetMessage(&msg, NULL, 0, 0)) {
    if (!IsDialogMessage(m_hwnd, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
}

void TestFramework::create(HWND hwnd)
{
  // create the server checkbox and ip textbox
  m_hwndClickButton = CreateWindow(WC_BUTTON, "Click",
    WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON | WS_TABSTOP,
    120, 160, 170, 32, hwnd, (HMENU)CLICK_BUTTON_ID, nullptr, nullptr);

  // sub-process the button to capture the press/release individually
  g_pTestFramework->m_oldButtonProc = (WNDPROC)SetWindowLongPtr(m_hwndClickButton, GWLP_WNDPROC,
    (LONG_PTR)TestFramework::button_subproc);

  m_hwndTickrateSlider = CreateWindow(TRACKBAR_CLASS, "Tickrate",
    WS_VISIBLE | WS_CHILD | WS_TABSTOP | TBS_VERT,
    20, 30, 36, 160, hwnd, (HMENU)TICKRATE_SLIDER_ID, nullptr, nullptr);

  //m_hwndTickOffsetSlider = CreateWindow(TRACKBAR_CLASS, "Time Offset",
  //  WS_VISIBLE | WS_CHILD | WS_TABSTOP | TBS_VERT,
  //  360, 30, 36, 160, hwnd, (HMENU)TIME_OFFSET_SLIDER_ID, nullptr, nullptr);

  // do the arduino init/setup
  arduino_setup();

  TrackBar_SetPos(m_hwndTickrateSlider, 20);
  //TrackBar_SetPos(m_hwndTickOffsetSlider, 0);

  // init tickrate and time offset to match the sliders
  setTickrate();
  //setTickOffset();
}

void TestFramework::command(WPARAM wParam, LPARAM lParam)
{
  if (!m_initialized) {
    return;
  }

  // ignore commands for other things
  if (LOWORD(wParam) != CLICK_BUTTON_ID) {
    return;
  }

}

void TestFramework::paint(HWND hwnd)
{
  if (!m_initialized) {
    return;
  }

  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd, &ps);

  // transparent background for text
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, RGB(200, 200, 200));

  // the first led is 5,5 to 25,25
  for (uint32_t i = 0; i < m_numLeds; ++i) {
    // draw the LED ellipsed
    HBRUSH oldbrush = (HBRUSH)SelectObject(hdc, getBrushCol(m_ledList[i]));
    Ellipse(hdc, m_ledPos[i].left, m_ledPos[i].top, m_ledPos[i].right, m_ledPos[i].bottom);
    SelectObject(hdc, oldbrush);

    // Draw the numbers above/below the LEDs
    RECT idRect = m_ledPos[i];
    // 1 if even, -1 if odd, this makes the evens go down and odds go up
    int signEven = 1 - (2 * (i % 2));
    // shift it up/down 30 with a static offset of 8
    idRect.top += (30 * signEven) + 8;
    idRect.bottom += (30 * signEven) + 8;
    char text[4] = {0};
    // The text is in reverse (LED_LAST - i) because that's the order of the enums
    // in LedConfig.h -- the actual hardware is reversed too and should be flipped in v2
    snprintf(text, sizeof(text), "%d", LED_LAST - i);
    DrawText(hdc, text, -1, &idRect, DT_CENTER);
  }

  // Tip:
  RECT tipRect = m_ledPos[1];
  tipRect.top += 8;
  tipRect.bottom += 10;
  tipRect.left -= 44;
  tipRect.right -= 38;
  DrawText(hdc, "Tip", 3, &tipRect, DT_RIGHT);

  // Top:
  RECT topRect = m_ledPos[0];
  topRect.top += 8;
  topRect.bottom += 10;
  topRect.left -= 44;
  topRect.right -= 38;
  DrawText(hdc, "Top", 3, &topRect, DT_RIGHT);

  // Tickspeed
  string tickspeedStr = "Tickrate: " + to_string(Time::getTickrate());
  RECT rateRect;
  rateRect.top = 200;
  rateRect.bottom = 240;
  rateRect.left = 20;
  rateRect.right = 200;
  DrawText(hdc, tickspeedStr.c_str(), -1, &rateRect, 0);

#if 0
  // Tick offset
  string tickoffsetStr = "Tick Offset: " + to_string(Time::getTickOffset((LedPos)1));
  RECT offsetRect;
  offsetRect.top = 200;
  offsetRect.bottom = 240;
  offsetRect.left = 280;
  offsetRect.right = 400;
  DrawText(hdc, tickoffsetStr.c_str(), -1, &offsetRect, DT_RIGHT);
#endif

  // pattern strip
  if (m_redrawStrip) {
    m_redrawStrip = false;
    RECT backPos = { 0, 229, 420, 251 };
    FillRect(hdc, &backPos, getBrushCol(0));
    for (uint32_t i = 0; i < m_patternStrip.size(); ++i) {
      RECT stripPos = { (LONG)i, 230, (LONG)i + 1, 250 };
      FillRect(hdc, &stripPos, getBrushCol(m_patternStrip[i]));
    }
  }

  EndPaint(hwnd, &ps);
}

void TestFramework::cleanup()
{
  m_keepGoing = false;
  m_isPaused = false;
  WaitForSingleObject(m_loopThread, 3000);
  DeleteObject(m_bkbrush);
}

void TestFramework::arduino_setup()
{
  // init the drop-in arduino library replacement
  init_arduino();
  if (!VortexFramework::init()) {
    // uhoh
  }
}

void TestFramework::arduino_loop()
{
  // run the tick
  VortexFramework::tick();
}

void TestFramework::installLeds(CRGB *leds, uint32_t count)
{
  m_ledList = (RGBColor *)leds;
  m_numLeds = count;

  // initialize the positions of all the leds
  uint32_t base_left = 92;
  uint32_t base_top = 50;
  uint32_t radius = 15;
  uint32_t dx = 24;
  uint32_t dy = 30;
  for (int i = 0; i < LED_COUNT; ++i) {
    // bottom to top, left to right is the true order of the hardware
    int even = i % 2; // whether i is even
    int odd = (even == 0); // whether i is odd
    int rDown = i - even; // i rounded down to even
    m_ledPos[i].left = base_left + (rDown * dx);
    m_ledPos[i].right = base_left + dy + (rDown * dx);
    m_ledPos[i].top = base_top + (odd * dy);
    m_ledPos[i].bottom = base_top + dy + (odd * dy);
    if (i == 0 || i == 1) {
      // offset the thumb
      m_ledPos[i].top += 10;
      m_ledPos[i].bottom += 10;
    }
  }

  m_initialized = true;;
}

void TestFramework::setBrightness(int brightness)
{
  m_brightness = brightness;
}

// when the glove framework calls 'FastLED.show'
void TestFramework::show()
{
  if (!m_initialized) {
    return;
  }
#if 0
  static uint64_t lastshow = 0;
  if (lastshow && ((millis() - lastshow) < 100)) {
    return;
  }
  lastshow = millis();
#endif

  // redraw the leds
  for (int i = 0; i < LED_COUNT; ++i) {
    InvalidateRect(m_hwnd, m_ledPos + i, FALSE);
  }
}

void TestFramework::pressButton()
{
  m_buttonPressed = true;
}

void TestFramework::releaseButton()
{
  m_buttonPressed = false;
}

bool TestFramework::isButtonPressed() const
{
  // spacebar also works
  return m_buttonPressed || (GetKeyState(VK_SPACE) & 0x100) != 0;
}

void TestFramework::setTickrate()
{
  uint32_t tickrate = TrackBar_GetPos(g_pTestFramework->m_hwndTickrateSlider);
  if (tickrate > 75) {
    tickrate *= (tickrate / 20);
  }
  if (tickrate < 1) {
    tickrate = 1;
  }
  if (tickrate > 1000000) {
    tickrate = 1000000;
  }
  pause();
  Time::setTickrate(tickrate);
  unpause();
  RECT rateRect;
  rateRect.top = 200;
  rateRect.bottom = 220;
  rateRect.left = 20;
  rateRect.right = 200;
  InvalidateRect(m_hwnd, &rateRect, TRUE);
  DEBUG_LOGF("Set tickrate: %u", tickrate);
}

void TestFramework::setTickOffset()
{
#if 0
  if (!Modes::curMode()) {
    return;
  }
  uint32_t offset = TrackBar_GetPos(g_pTestFramework->m_hwndTickOffsetSlider);
  // mom can we get a synchronization lock?
  // mom: We have a synchronization lock at home
  // the synchronization lock at home:
  pause();
  Time::setTickOffset(offset);
  Modes::curMode()->init();
  unpause();
  RECT offsetRect;
  offsetRect.top = 200;
  offsetRect.bottom = 220;
  offsetRect.left = 280;
  offsetRect.right = 400;
  InvalidateRect(m_hwnd, &offsetRect, TRUE);
  DEBUG_LOGF("Set time offset: %u", offset);
#endif
}

void TestFramework::pause()
{
  if (m_isPaused) {
    return;
  }
  if (WaitForSingleObject(m_pauseMutex, INFINITE) != WAIT_OBJECT_0) {
    DEBUG_LOG("Failed to pause");
    return;
  }
  m_isPaused = true;
}

void TestFramework::unpause()
{
  if (!m_isPaused) {
    return;
  }
  ReleaseMutex(m_pauseMutex);
  m_isPaused = false;
}

bool TestFramework::handlePatternChange()
{
  if (!Modes::curMode()) {
    return false;
  }
  // don't want to create a callback mechanism just for the test framework to be
  // notified of pattern changes, I'll just watch the patternID each tick
  PatternID curPattern = Modes::curMode()->getPatternID();
  Colorset *curColorset = (Colorset *)Modes::curMode()->getColorset();
  if (!curColorset) {
    return false;
  }
  // check to see if the pattern or colorset changed
  if (curPattern == m_curPattern && *curColorset == m_curColorset) {
    return false;
  }
  // update current pattern and colorset
  m_curPattern = curPattern;
  m_curColorset = *curColorset;
  // would use the mode builder but it's not quite suited for this
  // create the new mode object
  Mode *newMode = new Mode();
  if (!newMode) { 
    return false; 
  }
  LedPos targetPos = LED_FIRST;
  // TODO: The hardware is flipped so the 'real' led position is reversed
  LedPos realPos = (LedPos)(LED_LAST - targetPos);
  if (isMultiLedPatternID(m_curPattern)) {
    if (!newMode->bindMulti(m_curPattern, &m_curColorset)) {
      delete newMode;
      return false;
    }
  } else {
  // bind the pattern and colorset to the mode
    if (!newMode->bindSingle(m_curPattern, &m_curColorset, LED_FIRST)) {
      delete newMode;
      return false;
    }
  }
  // backup the current LED 0 color
  RGBColor curLed0Col = m_ledList[realPos];
  Time::startSimulation();
  newMode->init();
  m_patternStrip.clear();
  for (int i = 0; i < 420; ++i) {
    newMode->play();
    Time::tickSimulation();
    RGBColor c = m_ledList[realPos];
    m_patternStrip.push_back(c);
  }
  Time::endSimulation();
  // restore original color on Led0
  m_ledList[realPos] = curLed0Col;
  // idk why this sleep is necessary, bad synchronization
  Sleep(100);
  // clean up the temp mode object and the pattern/colorset it contains
  delete newMode;
  // redraw the pattern strip
  m_redrawStrip = true;
  RECT stripRect = { 0, 200, 420, 260 };
  InvalidateRect(m_hwnd, &stripRect, TRUE);
  return true;
}

HBRUSH TestFramework::getBrushCol(RGBColor rgbcol)
{
  HBRUSH br;
  COLORREF col = RGB(rgbcol.red, rgbcol.green, rgbcol.blue);
  if (m_brushmap.find(col) == m_brushmap.end()) {
    br = CreateSolidBrush(col);
    m_brushmap[col] = br;
  }
  br = m_brushmap[col];
  return br;
}

DWORD __stdcall TestFramework::arduino_loop_thread(void *arg)
{
  TestFramework *framework = (TestFramework *)arg;
  while (framework->m_initialized && framework->m_keepGoing) {
    DWORD dwWaitResult = WaitForSingleObject(framework->m_pauseMutex, INFINITE);  // no time-out interval
    if (dwWaitResult == WAIT_OBJECT_0) {
      framework->arduino_loop();
      // if pattern changes we need to reload the pattern strip
      framework->handlePatternChange();
      ReleaseMutex(framework->m_pauseMutex);
    }
  }
  // cleanup
  VortexFramework::cleanup();
  return 0;
}

LRESULT CALLBACK TestFramework::button_subproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg) {
  case WM_LBUTTONDOWN:
    g_pTestFramework->pressButton();
    break;
  case WM_LBUTTONUP:
    g_pTestFramework->releaseButton();
    break;
  default:
    break;
  }
  return CallWindowProcA(g_pTestFramework->m_oldButtonProc, hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK TestFramework::window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg) {
  case WM_VSCROLL:
    switch (LOWORD(wParam)) {
    case TB_THUMBTRACK:
    case TB_ENDTRACK:
      // lazy
      //if ((HWND)lParam == g_pTestFramework->m_hwndTickOffsetSlider) {
      //  g_pTestFramework->setTickOffset();
      //}
      if ((HWND)lParam == g_pTestFramework->m_hwndTickrateSlider) {
        g_pTestFramework->setTickrate();
      }
      break;
    }
    break;
  case WM_CTLCOLORSTATIC:
     return (INT_PTR)g_pTestFramework->m_bkbrush;
  case WM_CREATE:
    g_pTestFramework->create(hwnd);
    break;
  case WM_PAINT:
    g_pTestFramework->paint(hwnd);
    return 0;
  case WM_COMMAND:
    g_pTestFramework->command(wParam, lParam);
    break;
  case WM_DESTROY:
    g_pTestFramework->cleanup();
    PostQuitMessage(0);
    break;
  default:
    break;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void TestFramework::printlog(const char *file, const char *func, int line, const char *msg, va_list list)
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
  strMsg += "\n";
  vfprintf(g_pTestFramework->m_consoleHandle, strMsg.c_str(), list);
  vfprintf(g_pTestFramework->m_logHandle, strMsg.c_str(), list);
}
