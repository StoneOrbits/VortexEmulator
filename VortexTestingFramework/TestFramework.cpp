#include <Windows.h>
#include <CommCtrl.h>

#include <string>
#include <map>

#include "TestFramework.h"
#include "Arduino.h"

#include "Log.h"

#include "VortexGloveset.h"
#include "TimeControl.h"

#pragma comment(lib, "Comctl32.lib")

TestFramework *g_pTestFramework = nullptr;

using namespace std;

#define TrackBar_GetPos(hwnd) \
    (LONG)SendMessage((hwnd), TBM_GETPOS, 0, 0)

#define CLICK_BUTTON_ID 10001
#define TICKRATE_SLIDER_ID 10002
#define TIME_OFFSET_SLIDER_ID 10003

TestFramework::TestFramework() :
  m_loopThread(nullptr),
  m_bkbrush(nullptr),
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
  m_keepGoing(true)
{
}

bool TestFramework::init(HINSTANCE hInstance)
{
  if (g_pTestFramework) {
    return false;
  }
  g_pTestFramework = this;
  
  if (!m_logHandle) {
    AllocConsole();
    freopen_s(&m_logHandle, "CONOUT$", "w", stdout);
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
    420, 269, nullptr, nullptr, hInstance, nullptr);
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
  g_pTestFramework->m_oldButtonProc = (WNDPROC)SetWindowLong(m_hwndClickButton, GWL_WNDPROC,
    (LONG_PTR)TestFramework::button_subproc);

  m_hwndTickrateSlider = CreateWindow(TRACKBAR_CLASS, "Tickrate",
    WS_VISIBLE | WS_CHILD | WS_TABSTOP | TBS_VERT,
    20, 30, 36, 160, hwnd, (HMENU)TICKRATE_SLIDER_ID, nullptr, nullptr);

  m_hwndTickOffsetSlider = CreateWindow(TRACKBAR_CLASS, "Time Offset",
    WS_VISIBLE | WS_CHILD | WS_TABSTOP | TBS_VERT,
    360, 30, 36, 160, hwnd, (HMENU)TIME_OFFSET_SLIDER_ID, nullptr, nullptr);

  // do the arduino init/setup
  arduino_setup();

  // init tickrate and time offset to match the sliders
  setTickrate();
  setTickOffset();
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

  static map<COLORREF, HBRUSH> brushmap;

  // the first led is 5,5 to 25,25
  HBRUSH br;
  for (uint32_t i = 0; i < m_numLeds; ++i) {
    COLORREF col = RGB(m_ledList[i].red, m_ledList[i].green, m_ledList[i].blue);
    if (brushmap.find(col) == brushmap.end()) {
      br = CreateSolidBrush(col);
      brushmap[col] = br;
    }
    br = brushmap[col];
    HBRUSH oldbrush = (HBRUSH)SelectObject(hdc, br);
    Ellipse(hdc, m_ledPos[i].left, m_ledPos[i].top, m_ledPos[i].right, m_ledPos[i].bottom);
    SelectObject(hdc, oldbrush);
  }

  EndPaint(hwnd, &ps);
}

void TestFramework::cleanup()
{
  m_keepGoing = false;
  WaitForSingleObject(m_loopThread, 3000);
  DeleteObject(m_bkbrush);
}

void TestFramework::arduino_setup()
{
  // init the drop-in arduino library replacement
  init_arduino();
  if (!VortexGloveset::init()) {
    // uhoh
  }
}

void TestFramework::arduino_loop()
{
  // run the tick
  VortexGloveset::tick();
}

void TestFramework::installLeds(CRGB *leds, uint32_t count)
{
  m_ledList = (RGBColor *)leds;
  m_numLeds = count;

  // initialize the positions of all the leds
  uint32_t base_left = 70;
  uint32_t base_top = 50;
  uint32_t radius = 15;
  uint32_t d = radius * 2;
  for (int i = 0; i < LED_COUNT; ++i) {
    m_ledPos[i].left = base_left + (i - (i % 2)) * d;
    m_ledPos[i].right = base_left + d + (i - (i % 2)) * d;
    m_ledPos[i].top = base_top + ((i % 2) * d);
    m_ledPos[i].bottom = base_top + d + ((i % 2) * d);
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
  RECT ledArea;
  ledArea.left = m_ledPos[0].left;
  ledArea.top = m_ledPos[2].top;
  ledArea.right = m_ledPos[LED_COUNT - 1].right;
  ledArea.bottom = m_ledPos[1].bottom;
  InvalidateRect(m_hwnd, &ledArea, FALSE);
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
  if (tickrate > 50) {
    tickrate *= (tickrate / 10);
  }
  if (tickrate < 1) {
    tickrate = 1;
  }
  if (tickrate > 1000000) {
    tickrate = 1000000;
  }
  Time::setTickrate(tickrate);
  DEBUG("Set tickrate: %u", tickrate);
}

void TestFramework::setTickOffset()
{
  uint32_t offset = TrackBar_GetPos(g_pTestFramework->m_hwndTickOffsetSlider);
  Time::setTickOffset(offset);
  DEBUG("Set time offset: %u", offset);
}

DWORD __stdcall TestFramework::arduino_loop_thread(void *arg)
{
  TestFramework *framework = (TestFramework *)arg;
  while (framework->m_initialized && framework->m_keepGoing) {
    framework->arduino_loop();
  }
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
      if ((HWND)lParam == g_pTestFramework->m_hwndTickOffsetSlider) {
        g_pTestFramework->setTickOffset();
      }
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
  strMsg += "\n";
  vfprintf(g_pTestFramework->m_logHandle, strMsg.c_str(), list);
}
