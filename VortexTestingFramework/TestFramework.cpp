#include <Windows.h>
#include <CommCtrl.h>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <ctime>

#include "TestFramework.h"
#include "IRSimulator.h"
#include "EditorPipe.h"
#include "VortexLib.h"
#include "EngineDependencies/Arduino.h"

#include "Log/Log.h"

#include "Patterns/PatternBuilder.h"
#include "Leds/Leds.h"
#include "Time/TimeControl.h"
#include "Colors/Colorset.h"
#include "Menus/Menus.h"
#include "Menus/Menu.h"
#include "Modes/Modes.h"
#include "Modes/Mode.h"

#include "patterns/Pattern.h"
#include "patterns/single/SingleLedPattern.h"

#include "resource.h"

#pragma comment(lib, "Comctl32.lib")

TestFramework *g_pTestFramework = nullptr;

using namespace std;

#define CLICK_BUTTON_ID 10001
#define LED_CIRCLE_ID 20003
#define LAUNCH_IR_ID  30001

#define BACK_COL        RGB(40, 40, 40)

TestFramework::TestFramework() :
  m_pCallbacks(nullptr),
  m_window(),
  m_orbitBox(),
  m_patternStrip(),
  m_tickrateSlider(),
  m_button(),
  m_IRLaunchButton(),
  m_leds(),
  m_ledPos(),
  m_pauseMutex(nullptr),
  m_hInst(nullptr),
  m_consoleHandle(nullptr),
  m_orbitBMP(nullptr),
  m_hIcon(nullptr),
  m_loopThread(nullptr),
  m_tickrate(150),
  m_brightness(255),
  m_ledList(nullptr),
  m_numLeds(0),
  m_lastLedColor(nullptr),
  m_curSelectedLed(LED_FIRST),
  m_initialized(false),
  m_buttonPressed(false),
  m_buttonPressed2(false),
  m_keepGoing(true),
  m_isPaused(false),
  m_curMode(),
  m_brushmap(),
  m_accelTable()
{
}

TestFramework::~TestFramework()
{
  if (m_lastLedColor) {
    delete[] m_lastLedColor;
  }
}

bool TestFramework::init(HINSTANCE hInstance)
{
  if (g_pTestFramework) {
    return false;
  }
  m_hInst = hInstance;

  g_pTestFramework = this;

#ifdef _DEBUG
  if (!m_consoleHandle) {
    AllocConsole();
    freopen_s(&m_consoleHandle, "CONOUT$", "w", stdout);
  }
  DeleteFile("VortexEditor.dat");
#endif

  // create the pause mutex
  m_pauseMutex = CreateMutex(NULL, false, NULL);
  if (!m_pauseMutex) {
    //return false;
  }

  // load the main window
  m_window.init(m_hInst, "Vortex Orbit Emulator", BACK_COL, width, height, this, "VortexTestFramework");

  // load the icon and background image
  m_hIcon = LoadIcon(m_hInst, MAKEINTRESOURCE(IDI_ICON1));
  // set the icon
  SendMessage(m_window.hwnd(), WM_SETICON, ICON_BIG, (LPARAM)m_hIcon);
  m_orbitBMP = (HBITMAP)LoadImage(m_hInst, MAKEINTRESOURCE(IDB_BITMAP1), IMAGE_BITMAP, 0, 0, 0);

  m_orbitBox.init(m_hInst, m_window, "Orbit", BACK_COL, 500, 250, 66, 30, 0, 0, nullptr);
  m_orbitBox.setDoCapture(false);
  m_orbitBox.setDrawHLine(false);
  m_orbitBox.setDrawVLine(false);
  m_orbitBox.setDrawCircle(false);
  m_orbitBox.setBackground(m_orbitBMP);
  // disable the glove so it doesn't steal clicks from the leds
  m_orbitBox.setEnabled(false);

  m_patternStrip.init(m_hInst, m_window, "Pattern Strip", BACK_COL, width, patternStripHeight, -2, 375, 2, 11234, patternStripSelectCallback); 
  m_patternStrip.setDrawHLine(false);
  m_patternStrip.setDrawVLine(false);
  m_patternStrip.setDrawCircle(false);

  m_tickrateSlider.init(m_hInst, m_window, "Tickrate", BACK_COL, tickrateSliderWidth, tickrateSliderHeight, 30, 100, 1, 123433, setTickrateCallback);
  m_tickrateSlider.setDrawCircle(false);
  m_tickrateSlider.setDrawVLine(false);
  m_tickrateSlider.setSelection(0, 30);

  COLORREF *cols = new COLORREF[tickrateSliderWidth * tickrateSliderHeight];
  if (!cols) {
    return false;
  }
  // clear and re-generate the pattern strip
  for (int x = 0; x < tickrateSliderWidth; ++x) {
    // fill the entire column of the bitmap with this color
    for (int y = 0; y < tickrateSliderHeight; ++y) {
      cols[(y * tickrateSliderWidth) + x] = RGB(30, 30, 30);
    }
  }
  // create a bitmap out of the array of colors
  HBITMAP bitmap = CreateBitmap(tickrateSliderWidth, tickrateSliderHeight, 1, 32, cols);
  m_tickrateSlider.setBackground(bitmap);
  m_tickrateSlider.setSelection(0, 240);
  Vortex::setTickrate(150);

  m_button.init(m_hInst, m_window, "Click", BACK_COL, 48, 24, 290, 308, CLICK_BUTTON_ID, buttonClickCallback);
  m_button2.init(m_hInst, m_window, "Click2", BACK_COL, 48, 24, 290, 336, CLICK_BUTTON_ID + 1, buttonClickCallback);
  m_IRLaunchButton.init(m_hInst, m_window, "Connect IR", BACK_COL, 80, 24, 350, 340, LAUNCH_IR_ID, launchIRCallback);
  m_IRLaunchButton.setVisible(false);

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

  for (uint32_t i = 0; i < LED_COUNT; ++i) {
    // super lazy reposition of + 67 because I don't want to go 
    // adjust all the values in the above statements
    m_leds[i].init(m_hInst, m_window, to_string(0),
      BACK_COL, 21, 21, m_ledPos[i].left + 67, m_ledPos[i].top, LED_CIRCLE_ID + i, ledClickCallback);
  }
    
  // create an accelerator table for dispatching hotkeys as WM_COMMANDS
  // for specific menu IDs
  ACCEL accelerators[] = {
    // ctrl + shift + I   open the IR connection
    { FCONTROL | FSHIFT | FVIRTKEY, 'I', LAUNCH_IR_ID },
  };
  m_accelTable = CreateAcceleratorTable(accelerators, sizeof(accelerators) / sizeof(accelerators[0]));
  if (!m_accelTable) {
    // error!
  }

  if (!IRSimulator::init()) {
    printf("IRSim failed to init\n");
  }

  if (!EditorPipe::init()) {
    if (!IRSimulator::isConnected()) {
      printf("Pipe failed to init\n");
    }
  }

  // launch the 'loop' thread
  m_loopThread = CreateThread(NULL, 0, TestFramework::arduino_loop_thread, this, 0, NULL);
  if (!m_loopThread) {
    // error
    return false;
  }

  m_initialized = true;

  return true;
}

void TestFramework::cleanup()
{
  // turn off the loops and unpause
  m_keepGoing = false;
  m_isPaused = false;
  // wait for the loop to finish, 3 seconds I guess
  WaitForSingleObject(m_loopThread, 3000);
}

void TestFramework::buttonClick(VButton *button, VButton::ButtonEvent type)
{
  uint32_t buttonID = ((uint32_t)GetMenu(button->hwnd())) - CLICK_BUTTON_ID;
  switch (type) {
  case VButton::ButtonEvent::BUTTON_EVENT_CLICK:
    break;
  case VButton::ButtonEvent::BUTTON_EVENT_PRESS:
    Vortex::pressButton(buttonID);
    break;
  case VButton::ButtonEvent::BUTTON_EVENT_RELEASE:
    Vortex::releaseButton(buttonID);
    break;
  default:
    break;
  }
}

void TestFramework::launchIR(VButton *window, VButton::ButtonEvent type)
{
  if (!m_pCallbacks) {
    return;
  }
  IRSimulator::startServer();
  m_IRLaunchButton.setEnabled(false);
  Menus::openMenu(MENU_MODE_SHARING);
}

void TestFramework::patternStripSelect(uint32_t x, uint32_t y, VSelectBox::SelectEvent sevent)
{
  // can't drag it yet
}

void TestFramework::ledClick(VWindow *window)
{
  uint32_t led = ((uint32_t)GetMenu(window->hwnd()) - LED_CIRCLE_ID);
  printf("Clicked led %u\n", led);
  m_curSelectedLed = (LedPos)led;
  handlePatternChange(true);
}

void TestFramework::setTickrate(uint32_t x, uint32_t y, VSelectBox::SelectEvent sevent)
{
  // tickrate is up to 1000 but it needs to be flipped because y+ is down
  m_tickrate = 1000 - (uint32_t)((float)y / tickrateSliderHeight * 1000.0);
}

void TestFramework::run()
{
  // main message loop
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    if (GetForegroundWindow() == m_window.hwnd()) {
      if (TranslateAccelerator(m_window.hwnd(), m_accelTable, &msg)) {
        continue;
      }
    }
    // pass message to main window otherwise process it
    if (!m_window.process(msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
}

void TestFramework::installLeds(CRGB *leds, uint32_t count)
{
  m_ledList = (RGBColor *)leds;
  m_numLeds = count;

  m_lastLedColor = new RGBColor[count];
  if (!m_lastLedColor) {
    return;
  }
}

void TestFramework::setBrightness(int brightness)
{
  m_brightness = brightness;
}

// when the glove framework calls 'FastLED.show'
void TestFramework::show()
{
  if (!m_initialized || !m_ledList) {
    return;
  }
  // update the colors with the colors in the led list
  for (LedPos i = LED_FIRST; i < LED_COUNT; ++i) {
    uint32_t raw = m_ledList[i].raw();
    if (m_leds[i].getColor() != raw) {
      m_leds[i].setColor(raw);
    }
  }
}

bool TestFramework::handlePatternChange(bool force)
{
  if (!Modes::curMode() || !m_ledList) {
    return false;
  }
  // don't want to create a callback mechanism just for the test framework to be
  // notified of pattern changes, I'll just watch the patternID each tick
  Mode *targetMode = Modes::curMode();
  if (!targetMode) {
    return false;
  }
  // cant do this it causes too much lag in the editor
  Menu *curMenu = Menus::curMenu();
  Mode *menuMode = Vortex::getMenuDemoMode();
  if (menuMode) {
    targetMode = menuMode;
  }

  // check to see if the mode changed
  if (!force && m_curMode.equals(targetMode)) {
    return false;
  }
  m_patternStrip.setBackgroundOffset(0, 0);
  // update current mode
  m_curMode = *targetMode;
  m_curMode.init();
  // the realpos is used to target the actual index of pattern to run
  LedPos realPos = (LedPos)(m_curSelectedLed);
  if (isMultiLedPatternID(m_curMode.getPatternID(realPos))) {
    // if it's multi led then the real pos is just the first
    realPos = (LedPos)(LED_FIRST);
  }
  // grab the target pattern object that will run
  Pattern *targetPat = m_curMode.getPattern(realPos);
  if (!targetPat) {
    return false;
  }
  // backup the selected led
  RGBColor backupCol = m_ledList[LED_FIRST];
  // begin the time simulation so we can tick forward
  Time::startSimulation();
  // the actual strip is twice the width of the window to allow scrolling
  uint32_t patternStripWidth = width * patternStripExtensionMultiplier;
  COLORREF *cols = new COLORREF[patternStripWidth * patternStripHeight];
  if (!cols) {
    return false;
  }
  // clear and re-generate the pattern strip
  for (uint32_t x = 0; x < patternStripWidth; ++x) {
    // run the pattern like normal
    targetPat->play();
    // tick the virtual time forward so that next play()
    // the engine will think a tick has passed
    Time::tickSimulation();
    // sample the color for the selected LED
    COLORREF col = Leds::getLed(m_curSelectedLed).raw();
    // fill the entire column of the bitmap with this color
    for (uint32_t y = 0; y < patternStripHeight; ++y) {
      cols[(y * patternStripWidth) + x] = col;
    }
  }
  // create a bitmap out of the array of colors
  HBITMAP bitmap = CreateBitmap(patternStripWidth, patternStripHeight, 1, 32, cols);
  delete[] cols;
  // end the time simulation, this snaps the tickcount
  // back to where it was before starting the sim
  Time::endSimulation();
  // restore original color on the target led
  m_ledList[m_curSelectedLed] = backupCol;
  // redraw this led because it was written to to generate pattern strip
  m_leds[m_curSelectedLed].redraw();
  // update the background of the pattern strip
  m_patternStrip.setBackground(bitmap);
  m_patternStrip.redraw();
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
  // init the vortex engine
  framework->m_pCallbacks = Vortex::init<TestFrameworkCallbacks>();
  if (!framework->m_pCallbacks) {
    // failed to init, error
    return 0;
  }
  if (IRSimulator::isConnected()) {
    framework->m_IRLaunchButton.setEnabled(false);
    Menus::openMenu(MENU_MODE_SHARING);
  }
  // init tickrate and time offset to match the sliders
  while (framework->m_initialized && framework->m_keepGoing) {
    DWORD dwWaitResult = WaitForSingleObject(framework->m_pauseMutex, INFINITE);  // no time-out interval
    if (dwWaitResult != WAIT_OBJECT_0) {
      //  only run the tick if we acquire the pause mutex
      continue;
    }
    // run the tick
    Vortex::tick();
    // backup the colors
    if (framework->m_lastLedColor && framework->m_ledList) {
      memcpy(framework->m_lastLedColor, framework->m_ledList, sizeof(RGBColor) * LED_COUNT);
    }
    // handle any tickrate changes
    uintptr_t newTickrate = framework->m_tickrate > 10 ? framework->m_tickrate : 10;
    if (newTickrate != Vortex::getTickrate()) {
      Vortex::setTickrate(newTickrate);
    }
    // scroll the background a little
    if (newTickrate > 10) {
      framework->m_patternStrip.addBackgroundOffset(1, 0, patternStripExtensionMultiplier - 1);
    }
    // if pattern changes we need to reload the pattern strip
    framework->handlePatternChange();
    ReleaseMutex(framework->m_pauseMutex);
  }
  // cleanup
  Vortex::cleanup();
  return 0;
}

std::string TestFramework::getWindowTitle()
{
  char text[2048] = {0};
  GetWindowText(m_window.hwnd(), text, sizeof(text));
  return text;
}

void TestFramework::setWindowTitle(std::string title)
{
  SetWindowTextA(m_window.hwnd(), title.c_str());
}

void TestFramework::setWindowPos(uint32_t x, uint32_t y)
{
  RECT pos;
  GetWindowRect(m_window.hwnd(), &pos);
  SetWindowPos(m_window.hwnd(), NULL, x, y, pos.right - pos.left, pos.bottom - pos.top, 0);
}

// ========================================================
//  Test Framework Engine Callbacks

// called when engine reads digital pins, use this to feed button presses to the engine
long TestFramework::TestFrameworkCallbacks::checkPinHook(uint32_t pin)
{
  if (pin == 19) {
    // get button state
    if (Vortex::isButtonPressed(0)) {
      return LOW;
    }
    return HIGH;
  }
  if (pin == 20) {
    // get button state
    if (Vortex::isButtonPressed(1)) {
      return LOW;
    }
    return HIGH;
  }
  return HIGH;
}

// called when engine writes to ir, use this to read data from the vortex engine
// the data received will be in timings of milliseconds
// NOTE: to send data to IR use Vortex::IRDeliver at any time
void TestFramework::TestFrameworkCallbacks::infraredWrite(bool mark, uint32_t amount)
{
  IRSimulator::send_message(amount);
}

// called when engine checks for Serial, use this to indicate serial is connected
bool TestFramework::TestFrameworkCallbacks::serialCheck()
{
  if (EditorPipe::isConnected()) {
    return true;
  }
  return EditorPipe::connect();
}

// called when engine checks for data on serial, use this to tell the engine data is ready
int32_t TestFramework::TestFrameworkCallbacks::serialAvail()
{
  return EditorPipe::available();
}

// called when engine reads from serial, use this to deliver data to the vortex engine
size_t TestFramework::TestFrameworkCallbacks::serialRead(char *buf, size_t amt)
{
  return EditorPipe::read(buf, amt);
}

// called when engine writes to serial, use this to read data from the vortex engine
uint32_t TestFramework::TestFrameworkCallbacks::serialWrite(const uint8_t *buf, size_t len)
{
  return EditorPipe::write(buf, len);
}

// called when the LED strip is initialized
void TestFramework::TestFrameworkCallbacks::ledsInit(void *cl, int count)
{
  g_pTestFramework->installLeds((CRGB *)cl, count);
}

// called when the brightness is changed
void TestFramework::TestFrameworkCallbacks::ledsBrightness(int brightness)
{
  g_pTestFramework->setBrightness(brightness);
}

// called when the leds are shown
void TestFramework::TestFrameworkCallbacks::ledsShow()
{
  g_pTestFramework->show();
}
