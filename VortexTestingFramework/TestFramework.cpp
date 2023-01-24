#include <Windows.h>
#include <CommCtrl.h>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <ctime>

#include "TestFramework.h"
#include "ArduinoSerial.h"
#include "Arduino.h"

#include "Log/Log.h"

#include "Patterns/PatternBuilder.h"
#include "Leds/Leds.h"
#include "Time/TimeControl.h"
#include "Colors/Colorset.h"
#include "Modes/ModeBuilder.h"
#include "Modes/Modes.h"
#include "Modes/Mode.h"

#include "VortexEngine.h"

#include "patterns/Pattern.h"
#include "patterns/single/SingleLedPattern.h"

#include "resource.h"

#pragma comment(lib, "Comctl32.lib")

// uncomment this to flip the colours to be fullbright
//#ifdef HSV_TO_RGB_GENERIC

TestFramework *g_pTestFramework = nullptr;

using namespace std;

#define LOG_TO_FILE 0

#define TrackBar_GetPos(hwnd) \
    (LONG)SendMessage((hwnd), TBM_GETPOS, 0, 0)

#define TrackBar_SetPos(hwnd, pos) \
    (LONG)SendMessage((hwnd), TBM_SETPOS, TRUE, pos)

#define CLICK_BUTTON_ID 10001
#define TICKRATE_SLIDER_ID 10002
#define TIME_OFFSET_SLIDER_ID 10003
#define LOAD_BUTTON_ID 10004

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
  m_hwndLoadButton(nullptr),
  m_gloveBMP(nullptr),
  m_hIcon(nullptr),
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
  m_redrawStrip(false),
  m_curSelectedLed(LED_FIRST)
{
}

TestFramework::~TestFramework()
{
#if LOG_TO_FILE == 1
  fclose(m_logHandle);
#endif
}

bool TestFramework::init(HINSTANCE hInstance)
{
  if (g_pTestFramework) {
    return false;
  }
  g_pTestFramework = this;

#if 0
  if (!m_consoleHandle) {
    AllocConsole();
    freopen_s(&m_consoleHandle, "CONOUT$", "w", stdout);
  }
#endif
#if LOG_TO_FILE == 1
  if (!m_logHandle) {
    time_t t = time(nullptr);
    tm tm;
    localtime_s(&tm, &t);
    ostringstream oss;
    oss << std::put_time(&tm, "%d-%m-%Y-%H-%M-%S");
    oss << "." << GetCurrentProcessId();
    string filename = "vortex-test-framework-log." + oss.str() + ".txt";
    int err = fopen_s(&m_logHandle, filename.c_str(), "w");
    if (err != 0 || !m_logHandle) {
      MessageBox(NULL, "Failed to open logfile", to_string(err).c_str(), 0);
      return false;
    }
  }
#endif

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
  m_hwnd = CreateWindow(m_wc.lpszClassName, "Vortex Emulator",
    WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
    (desktop.right / 2) - (width / 2), (desktop.bottom / 2) - (height / 2),
    width, height, nullptr, nullptr, hInstance, nullptr);
  if (!m_hwnd) {
    MessageBox(nullptr, "Failed to open window", "Error", 0);
    return 0;
  }

  // load the icon and background image
  m_hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
  // set the icon
  SendMessage(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)m_hIcon);
  m_gloveBMP = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(IDB_BITMAP1), IMAGE_BITMAP, 0, 0, 0);
  
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
    226, 312, 48, 24, hwnd, (HMENU)CLICK_BUTTON_ID, nullptr, nullptr);

  // sub-process the button to capture the press/release individually
  g_pTestFramework->m_oldButtonProc = (WNDPROC)SetWindowLongPtr(m_hwndClickButton, GWLP_WNDPROC,
    (LONG_PTR)TestFramework::button_subproc);

  m_hwndTickrateSlider = CreateWindow(TRACKBAR_CLASS, "Tickrate",
    WS_VISIBLE | WS_CHILD | WS_TABSTOP,
    14, 330, 120, 24, hwnd, (HMENU)TICKRATE_SLIDER_ID, nullptr, nullptr);

  //m_hwndTickOffsetSlider = CreateWindow(TRACKBAR_CLASS, "Time Offset",
  //  WS_VISIBLE | WS_CHILD | WS_TABSTOP | TBS_VERT,
  //  360, 30, 36, 160, hwnd, (HMENU)TIME_OFFSET_SLIDER_ID, nullptr, nullptr);

  //m_hwndLoadButton = CreateWindow(WC_BUTTON, "+",
  //  WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON | WS_TABSTOP,
  //  5, 5, 22, 22, hwnd, (HMENU)LOAD_BUTTON_ID, nullptr, nullptr);
}

void TestFramework::command(WPARAM wParam, LPARAM lParam)
{
  if (!m_initialized) {
    return;
  }

  // ignore commands for other things
  if (LOWORD(wParam) != LOAD_BUTTON_ID) {
    return;
  }

  // DO LOAD
  INFO_LOG("== LOADING FROM GLOVESET ==");

  ArduinoSerial sp;
  sp.connect("\\\\.\\COM8");

  if (sp.IsConnected()) {
    printf("We're connected\n");
  }

  uint8_t buf[8192] = {0};
  //printf("%s\n",incomingData);
  uint32_t dataLength = 0;
  int rv = 0;
  rv = sp.ReadData(&dataLength, sizeof(dataLength));
  printf("Read %d length: %u\n", rv, dataLength);
  rv = sp.ReadData(buf, dataLength);
  printf("read: %d\n", rv);
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

  // draw the glove
  HDC hdcGlove = CreateCompatibleDC(hdc);
  HBITMAP hbmpOld = (HBITMAP)SelectObject(hdcGlove, m_gloveBMP);
  // copy the glove into position
  BitBlt(hdc, 0, 30, 500, 250, hdcGlove, 0, 0, SRCCOPY);
  SelectObject(hdcGlove, hbmpOld);
  DeleteDC(hdcGlove);

  // the first led is 5,5 to 25,25
  for (uint32_t i = 0; i < m_numLeds; ++i) {
    // draw the LED ellipsed
#ifdef HSV_TO_RGB_GENERIC
    // implicitly convert rgb 'col' to hsv in argument, then back to rgb with different
    // algorithm than was originally used
    RGBColor trueCol = hsv_to_rgb_generic(m_ledList[i]);
#else
    RGBColor trueCol = m_ledList[i];
#endif
    HBRUSH oldbrush = (HBRUSH)SelectObject(hdc, getBrushCol(trueCol));
    Ellipse(hdc, m_ledPos[i].left, m_ledPos[i].top, m_ledPos[i].right, m_ledPos[i].bottom);
    SelectObject(hdc, oldbrush);

    // Draw the numbers above/below the LEDs
    RECT idRect = m_ledPos[i];
    // 1 if even, -1 if odd, this makes the evens go down and odds go up
    int signEven = 1 - (2 * (i % 2));
    // shift it up/down 30 with a static offset of 8
    idRect.top += (30 * signEven) + 8;
    idRect.bottom += (30 * signEven) + 8;
    char text[4] = { 0 };
    // The text is in reverse (LED_LAST - i) because that's the order of the enums
    // in LedTypes.h -- the actual hardware is reversed too and should be flipped in v2
    snprintf(text, sizeof(text), "%d", LED_LAST - i);
    //DrawText(hdc, text, -1, &idRect, DT_CENTER);
  }

  // Tip:
  //RECT tipRect = m_ledPos[1];
  //tipRect.top += 8;
  //tipRect.bottom += 10;
  //tipRect.left -= 44;
  //tipRect.right -= 38;
  //DrawText(hdc, "Tip", 3, &tipRect, DT_RIGHT);

  // Top:
  //RECT topRect = m_ledPos[0]//;
  //topRect.top += 8;
  //topRect.bottom += 10;
  //topRect.left -= 44;
  //topRect.right -= 38;
  ///DrawText(hdc, "Top", 3, &topRect, DT_RIGHT);

  // Tickspeed
  string tickspeedStr = "Tickrate: " + to_string(Time::getTickrate());
  RECT rateRect;
  rateRect.top = 310;
  rateRect.bottom = 350;
  rateRect.left = 20;
  rateRect.right = 200;
  DrawText(hdc, tickspeedStr.c_str(), -1, &rateRect, 0);

  if (Time::getTickrate() < 120) {
    RECT rateRect;
    rateRect.top = 356;
    rateRect.bottom = 375;
    rateRect.left = 16;
    rateRect.right = 600;
    DrawText(hdc, "Warning! Low tickrates cause unresponsiveness", -1, &rateRect, 0);
  }

  if (Time::getTickrate() > 600) {
    RECT rateRect;
    rateRect.top = 356;
    rateRect.bottom = 375;
    rateRect.left = 16;
    rateRect.right = 600;
    DrawText(hdc, "Warning! High tickrates are visually inaccurate", -1, &rateRect, 0);
  }

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

#ifdef ENABLE_PATTERN_STRIP
  // pattern strip
  if (m_redrawStrip) {
    m_redrawStrip = false;
    RECT stripRect = { 0, patternStripStart, width, patternStripEnd };
    FillRect(hdc, &stripRect, getBrushCol(0));
    for (uint32_t i = 0; i < m_patternStrip.size(); ++i) {
      RECT stripPos = { (LONG)i, patternStripStart + 2, (LONG)i + 1, patternStripEnd - 2 };
      RGBColor col = m_patternStrip[i];
      HSVColor hsvCol = col;
      uint32_t val = 255; //hsvCol.val;
      // if drawing a color with non full value
      if (!col.empty() && val < 255) {
        // fill black background
        FillRect(hdc, &stripPos, getBrushCol(0));
        // adjust the size of the bar based on value
        uint32_t offset = (uint32_t)(8 - ((val / 255.0) * 8));
        stripPos.top += offset;
        stripPos.bottom -= offset;
      }
#ifdef HSV_TO_RGB_GENERIC
      // implicitly convert rgb 'col' to hsv in argument, then back to rgb with different
      // algorithm than was originally used
      RGBColor trueCol = hsv_to_rgb_generic(col);
#else
      RGBColor trueCol = col;
#endif
      FillRect(hdc, &stripPos, getBrushCol(trueCol));
    }

    const uint32_t border_size = 2;
    for (uint32_t i = 0; i < MAX_COLOR_SLOTS; ++i) {
      RGBColor curCol = Modes::curMode()->getColorset()->get(i);
      HSVColor hsvCol = curCol;
      uint32_t offset = (uint32_t)(8 - ((hsvCol.val / 255.0) * 8));
      RECT colPos = { 50 + (LONG)(i * 40) , 265, 50 + (LONG)(i * 40) + 20, 285 };
      RECT bordPos = colPos;
      colPos.left += offset;
      colPos.top += offset;
      colPos.right -= offset;
      colPos.bottom -= offset;
      bordPos.left -= border_size;
      bordPos.top -= border_size;
      bordPos.bottom += border_size;
      bordPos.right += border_size;
      FillRect(hdc, &bordPos, getBrushCol(RGB_OFF));
      FillRect(hdc, &colPos, getBrushCol(curCol));
    }
  }
#endif

  EndPaint(hwnd, &ps);
}

void TestFramework::cleanup()
{
  VortexEngine::cleanup();
  m_keepGoing = false;
  m_isPaused = false;
  WaitForSingleObject(m_loopThread, 3000);
  DeleteObject(m_bkbrush);
  cleanup_arduino();
}

void TestFramework::arduino_setup()
{
  // init the drop-in arduino library replacement
  init_arduino();
  if (!VortexEngine::init()) {
    // uhoh
  }
}

void TestFramework::arduino_loop()
{
  // run the tick
  VortexEngine::tick();

  for (uint32_t i = 0; i < LED_COUNT; ++i) {
    m_lastLedColor[i] = m_ledList[i];
  }
}

void TestFramework::installLeds(CRGB *leds, uint32_t count)
{
  m_ledList = (RGBColor *)leds;
  m_numLeds = count;

  // initialize the positions of all the leds
  uint32_t base_left = 92;
  uint32_t base_top = 50;
  uint32_t diameter = 21;
  uint32_t dx = 24;
  uint32_t dy = 30;

  // quadrant 1 top
  for (uint32_t i = 0; i < 3; ++i) {
    m_ledPos[i].left = 150 + (i * 17);
    m_ledPos[i].top = 181 + (i * 17);
  }

  // quadrant 1 edge
  m_ledPos[3].left = m_ledPos[2].left + 25;
  m_ledPos[3].top = m_ledPos[2].top + 25;
  
  // quadrant 1 bot
  for (uint32_t i = 0; i < 3; ++i) {
    m_ledPos[6 - i].left = 332 - (i * 17);
    m_ledPos[6 - i].top = 181 + (i * 17);
  }

  // quadrant 2 bot
  for (uint32_t i = 0; i < 3; ++i) {
    m_ledPos[7 + i].left = 400 + (i * 17);
    m_ledPos[7 + i].top = 181 + (i * 17);
  }
  
  // quadrant 2 top
  for (uint32_t i = 0; i < 3; ++i) {
    m_ledPos[13 - i].left = 82 - (i * 17);
    m_ledPos[13 - i].top = 181 + (i * 17);
  }

  // quadrant 2 edge
  m_ledPos[10].left = m_ledPos[11].left - 25;
  m_ledPos[10].top = m_ledPos[11].top + 25;

  // quadrant 3 top
  for (uint32_t i = 0; i < 3; ++i) {
    m_ledPos[i + 14].left = 82 - (i * 17);
    m_ledPos[i + 14].top = 113 - (i * 17);
  }

  // quadrant 3 edge
  m_ledPos[17].left = m_ledPos[16].left - 25;
  m_ledPos[17].top = m_ledPos[16].top - 25;

  // quadrant 3 bot
  for (uint32_t i = 0; i < 3; ++i) {
    m_ledPos[20 - i].left = 400 + (i * 17);
    m_ledPos[20 - i].top = 113 - (i * 17);
  }

  // quadrant 4 bot
  for (uint32_t i = 0; i < 3; ++i) { 
    m_ledPos[21 + i].left = 332 - (i * 17);
    m_ledPos[21 + i].top = 113 - (i * 17);
  }

  // quadrant 4 top

  for (uint32_t i = 0; i < 3; ++i) {
    m_ledPos[27 - i].left = 150 + (i * 17);
    m_ledPos[27 - i].top = 113 - (i * 17);
  }

  // quadrant 4 edge
  m_ledPos[24].left = m_ledPos[25].left + 25;
  m_ledPos[24].top = m_ledPos[25].top - 25;

  for (uint32_t i = 0; i < LED_COUNT; ++i) {
    m_ledPos[i].right = m_ledPos[i].left + diameter;
    m_ledPos[i].bottom = m_ledPos[i].top + diameter;
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
    if (m_ledList[i] == m_lastLedColor[i]) {
      continue;
    }
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
  if (tickrate > 20) {
    tickrate *= (tickrate / 10);
  }
  if (tickrate < 1) {
    tickrate = 1;
  }
  // baseline allowable tickrate
  tickrate += 60;
  if (tickrate > 1000) {
    tickrate = 1000;
  }
  pause();
  Time::setTickrate(tickrate);
  unpause();
  RECT rateRect;
  rateRect.top = 310;
  rateRect.bottom = 375;
  rateRect.left = 10;
  rateRect.right = 600;
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

bool TestFramework::handlePatternChange(bool force)
{
#ifndef ENABLE_PATTERN_STRIP
  return true;
#endif
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
  if (!force) {
    if (curPattern == m_curPattern && *curColorset == m_curColorset) {
      return false;
    }
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
  // The hardware is not flipped so the 'real' led position is correct
  LedPos realPos = (LedPos)(m_curSelectedLed);
  if (isMultiLedPatternID(m_curPattern)) {
    if (!newMode->setMultiPat(m_curPattern, nullptr, &m_curColorset)) {
      delete newMode;
      return false;
    }
  } else {
    // bind the pattern and colorset to the mode
    if (!newMode->setSinglePat(LED_FIRST, m_curPattern, nullptr, &m_curColorset)) {
      delete newMode;
      return false;
    }
    // if it's single led pattern then we can only poll from slot 0
    realPos = (LedPos)(LED_FIRST);
  }
  // backup the current LED 0 color
  RGBColor curLed0Col = m_ledList[realPos];
  Time::startSimulation();
  newMode->init();
  m_patternStrip.clear();
  for (int i = 0; i < width; ++i) {
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
  RECT stripRect = { 0, patternStripStart, width, patternStripEnd };
  InvalidateRect(m_hwnd, &stripRect, TRUE);
  return true;
}

void TestFramework::handleWindowClick(int x, int y)
{
  for (uint32_t i = 0; i < m_numLeds; ++i) {
    if (x >= m_ledPos[i].left && y >= m_ledPos[i].top && 
        x <= m_ledPos[i].right && y <= m_ledPos[i].bottom) {
      // TODO: flip this when hardware switches
      selectLed((LedPos)(LED_LAST - i));
    }
  }
}

void TestFramework::selectLed(LedPos pos)
{
  DEBUG_LOGF("Selected LED %u", pos);
  m_curSelectedLed = pos;
  m_redrawStrip = true;
  handlePatternChange(true);
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
  // do the arduino init/setup
  framework->arduino_setup();
  TrackBar_SetPos(framework->m_hwndTickrateSlider, 30);
  //TrackBar_SetPos(m_hwndTickOffsetSlider, 0);
  // init tickrate and time offset to match the sliders
  framework->setTickrate();
  //setTickOffset();
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
  VortexEngine::cleanup();
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
  case WM_HSCROLL:
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
  case WM_LBUTTONDOWN:
    g_pTestFramework->handleWindowClick(LOWORD(lParam), HIWORD(lParam));
    break;
  case WM_COMMAND:
    g_pTestFramework->command(wParam, lParam);
    break;
  case WM_CLOSE:
    g_pTestFramework->cleanup();
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  default:
    break;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void TestFramework::printlog(const char *file, const char *func, int line, const char *msg, va_list list)
{
  if (!g_pTestFramework->m_consoleHandle) {
    return;
  }
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
#if LOG_TO_FILE == 1
  vfprintf(g_pTestFramework->m_logHandle, strMsg.c_str(), list);
#endif
}

std::string TestFramework::getWindowTitle()
{
  char text[2048] = {0};
  GetWindowText(m_hwnd, text, sizeof(text));
  return text;
}

void TestFramework::setWindowTitle(std::string title)
{
  SetWindowTextA(m_hwnd, title.c_str());
}


void TestFramework::setWindowPos(uint32_t x, uint32_t y)
{
  RECT pos;
  GetWindowRect(m_hwnd, &pos);
  SetWindowPos(m_hwnd, NULL, x, y, pos.right - pos.left, pos.bottom - pos.top, 0);
}
