#include <Windows.h>
#include <CommCtrl.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <ctime>

#include "TestFramework.h"
#include "IRSimulator.h"
#include "EditorPipe.h"
#include "VortexLib.h"

#include "Log/Log.h"

#include "Patterns/PatternBuilder.h"
#include "Menus/MenuList/Randomizer.h"
#include "Time/TimeControl.h"
#include "Colors/Colorset.h"
#include "VortexEngine.h"
#include "Menus/Menus.h"
#include "Modes/Mode.h"
#include "Leds/Leds.h"

#include "patterns/Pattern.h"

#include "resource.h"

#pragma comment(lib, "Comctl32.lib")

TestFramework *g_pTestFramework = nullptr;

using namespace std;

#define CLICK_BUTTON_ID 10001
#define TICKRATE_SLIDER_ID 15001
#define LED_CIRCLE_ID 20003
#define LAUNCH_IR_ID  30001
#define GEN_PATS_ID  30002
#define PATTERN_STRIP_ID  35001

#define BACK_COL        RGB(40, 40, 40)

TestFramework::TestFramework() :
  m_pCallbacks(nullptr),
  m_window(),
  m_orbitBox(),
  m_gloveBox(),
  m_handleBox(),
  m_fingerBox(),
  m_chromadeckBox(),
  m_sparkBox(),
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
  m_gloveBMP(nullptr),
  m_handleBMP(nullptr),
  m_fingerBMP(nullptr),
  m_chromadeckBMP(nullptr),
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
  m_window.init(m_hInst, "Vortex Emulator", BACK_COL, width, height, this, "VortexTestFramework");

  // load the icon and background image
  m_hIcon = LoadIcon(m_hInst, MAKEINTRESOURCE(IDI_ICON1));
  // set the icon
  SendMessage(m_window.hwnd(), WM_SETICON, ICON_BIG, (LPARAM)m_hIcon);

  switch (LED_COUNT) {
  case 28: // orbit
    m_orbitBMP = (HBITMAP)LoadImage(m_hInst, MAKEINTRESOURCE(IDB_BITMAP1), IMAGE_BITMAP, 0, 0, 0);
    m_orbitBox.init(m_hInst, m_window, "Orbit", BACK_COL, 500, 250, 66, 30, 0, 0, nullptr);
    m_orbitBox.setDoCapture(false);
    m_orbitBox.setDrawHLine(false);
    m_orbitBox.setDrawVLine(false);
    m_orbitBox.setDrawCircle(false);
    m_orbitBox.setBackground(m_orbitBMP);
    m_orbitBox.setEnabled(false);
    m_orbitBox.setVisible(true);
    m_button.init(m_hInst, m_window, "Click", BACK_COL, 48, 24, 270, 308, CLICK_BUTTON_ID, buttonClickCallback);
    m_button2.init(m_hInst, m_window, "Click2", BACK_COL, 48, 24, 270, 336, CLICK_BUTTON_ID + 1, buttonClickCallback);
    m_button3.init(m_hInst, m_window, "Long", BACK_COL, 52, 24, 328, 308, CLICK_BUTTON_ID + 2, longClickCallback);
    m_button4.init(m_hInst, m_window, "Long2", BACK_COL, 52, 24, 328, 336, CLICK_BUTTON_ID + 3, longClickCallback2);
    break;
  case 20: // chromadeck
    m_chromadeckBMP = (HBITMAP)LoadImage(m_hInst, MAKEINTRESOURCE(IDB_BITMAP5), IMAGE_BITMAP, 0, 0, 0);
    m_chromadeckBox.init(m_hInst, m_window, "Chromadeck", BACK_COL, 500, 250, 66, 30, 0, 0, nullptr);
    m_chromadeckBox.setDoCapture(false);
    m_chromadeckBox.setDrawHLine(false);
    m_chromadeckBox.setDrawVLine(false);
    m_chromadeckBox.setDrawCircle(false);
    m_chromadeckBox.setBackground(m_chromadeckBMP);
    m_chromadeckBox.setEnabled(false);
    m_chromadeckBox.setVisible(false);
    m_button.init(m_hInst, m_window, "<", BACK_COL, 36, 24, 164, 175, CLICK_BUTTON_ID, buttonClickCallback);
    m_button2.init(m_hInst, m_window, "o", BACK_COL, 44, 24, 204, 175, CLICK_BUTTON_ID + 1, buttonClickCallback);
    m_button3.init(m_hInst, m_window, ">", BACK_COL, 36, 24, 254, 175, CLICK_BUTTON_ID + 2, buttonClickCallback);
    break;
  case 10: // glove
    m_gloveBMP = (HBITMAP)LoadImage(m_hInst, MAKEINTRESOURCE(IDB_BITMAP2), IMAGE_BITMAP, 0, 0, 0);
    m_gloveBox.init(m_hInst, m_window, "Glove", BACK_COL, 250, 320, 86, 30, 0, 0, nullptr);
    m_gloveBox.setDoCapture(false);
    m_gloveBox.setDrawHLine(false);
    m_gloveBox.setDrawVLine(false);
    m_gloveBox.setDrawCircle(false);
    m_gloveBox.setBackground(m_gloveBMP);
    m_gloveBox.setEnabled(false);
    m_gloveBox.setVisible(true);
    m_button.init(m_hInst, m_window, "Click", BACK_COL, 48, 24, 198, 312, CLICK_BUTTON_ID, buttonClickCallback);
    m_button2.init(m_hInst, m_window, "Long", BACK_COL, 48, 24, 198, 346, CLICK_BUTTON_ID + 2, longClickCallback);
    break;
  case 6: // spark
    m_sparkBMP = (HBITMAP)LoadImage(m_hInst, MAKEINTRESOURCE(IDB_BITMAP6), IMAGE_BITMAP, 0, 0, 0);
    m_sparkBox.init(m_hInst, m_window, "Spark", BACK_COL, 200, 200, 30, 30, 0, 0, nullptr);
    m_sparkBox.setDoCapture(false);
    m_sparkBox.setDrawHLine(false);
    m_sparkBox.setDrawVLine(false);
    m_sparkBox.setDrawCircle(false);
    m_sparkBox.setBackground(m_sparkBMP);
    m_sparkBox.setEnabled(false);
    m_sparkBox.setVisible(false);
    m_button.init(m_hInst, m_window, "Click", BACK_COL, 48, 24, 198, 312, CLICK_BUTTON_ID, buttonClickCallback);
    m_button2.init(m_hInst, m_window, "Long", BACK_COL, 48, 24, 198, 346, CLICK_BUTTON_ID + 2, longClickCallback);
    break;
  case 3: // handle
    m_handleBMP = (HBITMAP)LoadImage(m_hInst, MAKEINTRESOURCE(IDB_BITMAP3), IMAGE_BITMAP, 0, 0, 0);
    m_handleBox.init(m_hInst, m_window, "Handle", BACK_COL, 285, 187, 87, 90, 0, 0, nullptr);
    m_handleBox.setDoCapture(false);
    m_handleBox.setDrawHLine(false);
    m_handleBox.setDrawVLine(false);
    m_handleBox.setDrawCircle(false);
    m_handleBox.setBackground(m_handleBMP);
    m_handleBox.setEnabled(false);
    m_handleBox.setVisible(true);
    m_button.init(m_hInst, m_window, "Click", BACK_COL, 48, 24, 198, 312, CLICK_BUTTON_ID, buttonClickCallback);
    m_button2.init(m_hInst, m_window, "Long", BACK_COL, 48, 24, 198, 336, CLICK_BUTTON_ID + 1, longClickCallback);
    break;
  case 2: // duo
    m_fingerBMP = (HBITMAP)LoadImage(m_hInst, MAKEINTRESOURCE(IDB_BITMAP4), IMAGE_BITMAP, 0, 0, 0);
    m_fingerBox.init(m_hInst, m_window, "Duo", BACK_COL, 250, 320, 86, 30, 0, 0, nullptr);
    m_fingerBox.setDoCapture(false);
    m_fingerBox.setDrawHLine(false);
    m_fingerBox.setDrawVLine(false);
    m_fingerBox.setDrawCircle(false);
    m_fingerBox.setBackground(m_fingerBMP);
    m_fingerBox.setEnabled(false);
    m_fingerBox.setVisible(true);
    m_button.init(m_hInst, m_window, "Click", BACK_COL, 48, 24, 198, 312, CLICK_BUTTON_ID, buttonClickCallback);
    m_button2.init(m_hInst, m_window, "Long", BACK_COL, 48, 24, 198, 346, CLICK_BUTTON_ID + 1, longClickCallback);
    break;
  }

  m_patternStrip.init(m_hInst, m_window, "Pattern Strip", BACK_COL, width, patternStripHeight, -2, 375, 2, PATTERN_STRIP_ID, patternStripSelectCallback);
  m_patternStrip.setDrawHLine(false);
  m_patternStrip.setDrawVLine(false);
  m_patternStrip.setDrawCircle(false);

  m_tickrateSlider.init(m_hInst, m_window, "Tickrate", BACK_COL, tickrateSliderWidth, tickrateSliderHeight, 30, 100, 1, TICKRATE_SLIDER_ID, setTickrateCallback);
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

  // the generate patterns button
  m_generatePats.init(m_hInst, m_window, "Make Art", BACK_COL, 80, 24,
    350 + ((LED_COUNT == 28) * 150), 310, GEN_PATS_ID, generatePatsCallback);

  // move the IR launch button over to the right on the orbit build
  m_IRLaunchButton.init(m_hInst, m_window, "Connect IR", BACK_COL, 80, 24,
    350 + ((LED_COUNT == 28) * 150), 340, LAUNCH_IR_ID, launchIRCallback);

  // TODO: storage enabled tickbox?
  Vortex::enableStorage(true);
  //Vortex::setStorageFilename(m_storageFile);
  //if (access(m_storageFile.c_str(), F_OK) == 0) {
  //  // load storage if the file exists
  //  Vortex::loadStorage();
  //}
  Vortex::setSleepEnabled(true);
  Vortex::setLockEnabled(true);

  // hardcoded switch optimizes to a single call based on engine led count
  switch (LED_COUNT) {
  case 28:
    setupLedPositionsOrbit();
    break;
  case 20:
    setupLedPositionsChromadeck();
    break;
  case 10:
    setupLedPositionsGlove();
    break;
  case 6:
    setupLedPositionsSpark();
    break;
  case 3:
    setupLedPositionsHandle();
    break;
  case 2:
    setupLedPositionsFinger();
    break;
  default:
    break;
  }

  // create an accelerator table for dispatching hotkeys as WM_COMMANDS
  // for specific menu IDs
  ACCEL accelerators[] = {
    // ctrl + shift + P   dumps patterns
    { FCONTROL | FSHIFT | FVIRTKEY, 'p', GEN_PATS_ID },
    // ctrl + shift + I   open the IR connection
    { FCONTROL | FSHIFT | FVIRTKEY, 'i', LAUNCH_IR_ID },
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
  m_loopThread = CreateThread(NULL, 0, TestFramework::main_loop_thread, this, 0, NULL);
  if (!m_loopThread) {
    // error
    return false;
  }

  m_initialized = true;

  return true;
}

void TestFramework::setupLedPositionsOrbit()
{
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
}

void TestFramework::setupLedPositionsGlove()
{
    // initialize the positions of all the leds
  uint32_t base_left = 92;
  uint32_t base_top = 50;
  uint32_t radius = 15;
  uint32_t dx = 24;
  uint32_t dy = 30;

  // thumb top/tip
  m_ledPos[0].left = 95;
  m_ledPos[0].top = 175;
  m_ledPos[1].top = m_ledPos[0].top - 20;
  m_ledPos[1].left = m_ledPos[0].left - 20;

  // index top/tip
  m_ledPos[2].left = 135;
  m_ledPos[2].top = 60;
  m_ledPos[3].top = m_ledPos[2].top - 30;
  m_ledPos[3].left = m_ledPos[2].left - 8;

  // middle top/tip
  m_ledPos[4].left = 195;
  m_ledPos[4].top = 40;
  m_ledPos[5].top = m_ledPos[4].top - 30;
  m_ledPos[5].left = m_ledPos[4].left;

  // ring top/tip
  m_ledPos[6].left = 254;
  m_ledPos[6].top = 60;
  m_ledPos[7].top = m_ledPos[6].top - 30;
  m_ledPos[7].left = m_ledPos[6].left + 8;

  // pinky top/tip
  m_ledPos[8].left = 300;
  m_ledPos[8].top = 95;
  m_ledPos[9].top = m_ledPos[8].top - 22;
  m_ledPos[9].left = m_ledPos[8].left + 16;

  for (uint32_t i = 0; i < LED_COUNT; ++i) {
    m_ledPos[i].right = m_ledPos[i].left + (radius * 2);
    m_ledPos[i].bottom = m_ledPos[i].top + (radius * 2);
  }

  for (uint32_t i = 0; i < LED_COUNT; ++i) {
    m_leds[i].init(m_hInst, m_window, to_string(0),
      BACK_COL, 30, 30, m_ledPos[i].left, m_ledPos[i].top, LED_CIRCLE_ID + i, ledClickCallback);
  }
}

void TestFramework::setupLedPositionsHandle()
{
  // initialize the positions of all the leds
  uint32_t base_left = 92;
  uint32_t base_top = 50;
  uint32_t radius = 18;
  uint32_t dx = 24;
  uint32_t dy = 30;

  // thumb top/tip
  m_ledPos[0].left = 165;
  m_ledPos[0].top = 95;
  m_ledPos[1].left = 112;
  m_ledPos[1].top = 178;
  m_ledPos[2].left = 186;
  m_ledPos[2].top = 230;

  for (uint32_t i = 0; i < LED_COUNT; ++i) {
    m_ledPos[i].right = m_ledPos[i].left + (radius * 2);
    m_ledPos[i].bottom = m_ledPos[i].top + (radius * 2);
  }

  for (uint32_t i = 0; i < LED_COUNT; ++i) {
    m_leds[i].init(m_hInst, m_window, to_string(0),
      BACK_COL, radius * 2, radius * 2, m_ledPos[i].left, m_ledPos[i].top, LED_CIRCLE_ID + i, ledClickCallback);
  }
}

void TestFramework::setupLedPositionsFinger()
{
  // initialize the positions of all the leds
  uint32_t base_left = 92;
  uint32_t base_top = 50;
  uint32_t radius = 15;
  uint32_t dx = 24;
  uint32_t dy = 30;

  // thumb top/tip
  m_ledPos[1].left = 196;
  m_ledPos[1].top = 38;
  m_ledPos[0].top = m_ledPos[1].top - 20;
  m_ledPos[0].left = m_ledPos[1].left;

  for (uint32_t i = 0; i < LED_COUNT; ++i) {
    m_ledPos[i].right = m_ledPos[i].left + (radius * 2);
    m_ledPos[i].bottom = m_ledPos[i].top + (radius * 2);
  }

  for (uint32_t i = 0; i < LED_COUNT; ++i) {
    m_leds[i].init(m_hInst, m_window, to_string(0),
      BACK_COL, 30, 30, m_ledPos[i].left, m_ledPos[i].top, LED_CIRCLE_ID + i, ledClickCallback);
  }
}

void TestFramework::setupLedPositionsChromadeck()
{
  // initialize the positions of all the leds
  uint32_t base_left = 92;
  uint32_t base_top = 50;
  uint32_t diameter = 21;
  uint32_t radius = 140;
  uint32_t dx = 24;
  uint32_t dy = 30;
  uint32_t centerX = 150;
  uint32_t centerY = 170;

  // all second points 
  for (uint32_t i = 0; i < LED_COUNT; ++i) {
#define M_PI (3.14159265358979323846)
#define M_2PI (2.0f * M_PI)
#define M_PI_O_2 (M_PI / 2.0f)
#define M_PI_O_4 (M_PI / 4.0f)
#define M_PI_O_8 (M_PI / 8.0f)
    float angle = i * (M_2PI / (LED_COUNT / 2));
    m_ledPos[i].left = centerX + radius * std::cos(angle - M_PI_O_2);
    m_ledPos[i].top = centerY + radius * std::sin(angle - M_PI_O_2);
    m_ledPos[i].right = m_ledPos[i].left + diameter;
    m_ledPos[i].bottom = m_ledPos[i].top + diameter;
    if (i == ((LED_COUNT / 2) - 1)) {
      radius -= 30;
    }
  }

  for (uint32_t i = 0; i < LED_COUNT; ++i) {
    // super lazy reposition of + 67 because I don't want to go
    // adjust all the values in the above statements
    m_leds[i].init(m_hInst, m_window, to_string(i),
      BACK_COL, 21, 21, m_ledPos[i].left + 67, m_ledPos[i].top, LED_CIRCLE_ID + i, ledClickCallback);
    m_leds[i].setTooltip(to_string(i));
  }
}

void TestFramework::setupLedPositionsSpark()
{
  // initialize the positions of all the leds
  uint32_t base_left = 92;
  uint32_t base_top = 50;
  uint32_t diameter = 21;
  uint32_t radius = 120;
  uint32_t dx = 24;
  uint32_t dy = 30;
  uint32_t centerX = 150;
  uint32_t centerY = 150;

  // all second points 
  for (uint32_t i = 0; i < LED_COUNT; ++i) {
#define M_PI (3.14159265358979323846)
#define M_2PI (2.0f * M_PI)
#define M_PI_O_2 (M_PI / 2.0f)
#define M_PI_O_4 (M_PI / 4.0f)
#define M_PI_O_8 (M_PI / 8.0f)
    float angle = i * (M_2PI / LED_COUNT);
    m_ledPos[i].left = centerX + radius * std::cos(-angle - M_PI_O_2);
    m_ledPos[i].top = centerY + radius * std::sin(-angle - M_PI_O_2);
    m_ledPos[i].right = m_ledPos[i].left + diameter;
    m_ledPos[i].bottom = m_ledPos[i].top + diameter;
  }

  for (uint32_t i = 0; i < LED_COUNT; ++i) {
    // super lazy reposition of + 67 because I don't want to go
    // adjust all the values in the above statements
    m_leds[i].init(m_hInst, m_window, to_string(i),
      BACK_COL, 21, 21, m_ledPos[i].left + 67, m_ledPos[i].top, LED_CIRCLE_ID + i, ledClickCallback);
    m_leds[i].setTooltip(to_string(i));
  }
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
  uint32_t buttonID = ((uint32_t)(uintptr_t)GetMenu(button->hwnd())) - CLICK_BUTTON_ID;
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

void TestFramework::longClick(VButton *window, uint32_t buttonIndex)
{
  Vortex::longClick(buttonIndex);
}

void TestFramework::launchIR(VButton *window, VButton::ButtonEvent type)
{
  if (!m_pCallbacks || type != VButton::ButtonEvent::BUTTON_EVENT_RELEASE) {
    return;
  }
  IRSimulator::startServer();
  m_IRLaunchButton.setEnabled(false);
  Vortex::openModeSharing();
}

void TestFramework::genPats(VButton *window, VButton::ButtonEvent type)
{
  if (type != VButton::ButtonEvent::BUTTON_EVENT_RELEASE) {
    return;
  }
  // Generate a unique filename
  string baseFilename = "patterns";
  string extension = ".bmp";
  string filename = baseFilename + extension;
  int counter = 1;
  while (ifstream(filename)) {
    filename = baseFilename + to_string(counter++) + extension;
  }
  DWORD dwWaitResult = WaitForSingleObject(m_pauseMutex, INFINITE);  // no time-out interval
  if (dwWaitResult != WAIT_OBJECT_0) {
    //  only run the tick if we acquire the pause mutex
    return;
  }
  bool success = generatePatternBMP(filename, 100);
  ReleaseMutex(m_pauseMutex);
  if (success) {
    // launch the file
    system(("start " + filename).c_str());
  }
}

bool TestFramework::generatePatternBMP(const string &filename, uint32_t numStrips)
{
  if (!Menus::checkInMenu()) {
    MessageBox(NULL, "Please enter the randomizer to generate art", "Error", 0);
    return false;
  }
  // The width of the bitmap is the same as the width of a single pattern strip
  uint32_t bitmapWidth = width * patternStripExtensionMultiplier;
  // The height of the bitmap is 100 times the height of a single pattern strip
  uint32_t bitmapHeight = patternStripHeight * numStrips;  // 100 pattern strips
  // the current mode of the randomizer menu
  Mode *menuMode = Vortex::getMenuDemoMode();
  if (!menuMode) {
    return false;
  }
  COLORREF *cols = new COLORREF[bitmapWidth * bitmapHeight];
  if (!cols) {
    return false;
  }
  // Clear and re-generate the pattern strip
  for (uint32_t i = 0; i < numStrips; ++i) {  // 100 pattern strips
    // reset the mode
    menuMode->init();
    // Begin the time simulation so we can tick forward
    Time::startSimulation();
    for (uint32_t x = 0; x < bitmapWidth; ++x) {
      // Run the current mode like normal
      menuMode->play();
      // Tick the virtual time forward so that next play()
      // the engine will think a tick has passed
      Time::tickSimulation();
      // Sample the color for the selected LED
      COLORREF col = Leds::getLed(m_curSelectedLed).raw();
      // Fill the entire column of the bitmap with this color
      for (uint32_t y = 0; y < patternStripHeight; ++y) {
        cols[((i * patternStripHeight + y) * bitmapWidth) + x] = col;
      }
    }
    // End the time simulation, this snaps the tickcount
    // back to where it was before starting the sim
    Time::endSimulation();
    // force randomizer to randomize lol
    ((Randomizer *)Menus::curMenu())->reRoll();
  }
  bool rv = writeBMPtoFile(filename, bitmapWidth, bitmapHeight, cols);
  delete[] cols;
  return rv;
}

// func to write bitmap to file
bool TestFramework::writeBMPtoFile(const string &filename, uint32_t bitmapWidth, uint32_t bitmapHeight, COLORREF *cols)
{
  // Create a bitmap out of the array of colors
  HBITMAP bitmap = CreateBitmap(bitmapWidth, bitmapHeight, 1, 32, cols);
  if (!bitmap) {
    return false;
  }
  BITMAPFILEHEADER fileHeader;
  BITMAPINFOHEADER infoHeader;
  // Configure the headers
  fileHeader.bfType = 0x4D42;  // BM
  fileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + bitmapWidth * bitmapHeight * 4;
  fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  infoHeader.biSize = sizeof(BITMAPINFOHEADER);
  infoHeader.biWidth = bitmapWidth;
  infoHeader.biHeight = bitmapHeight;
  infoHeader.biPlanes = 1;
  infoHeader.biBitCount = 32;  // Assuming the bitmap is 32-bit
  infoHeader.biCompression = BI_RGB;
  infoHeader.biSizeImage = bitmapWidth * bitmapHeight * 4;
  infoHeader.biXPelsPerMeter = 0;
  infoHeader.biYPelsPerMeter = 0;
  infoHeader.biClrUsed = 0;
  infoHeader.biClrImportant = 0;
  // Open the file
  ofstream bmpFile(filename, ios::out | ios::binary);
  if (!bmpFile) {
    cerr << "Could not open the .bmp file." << endl;
    return false;
  }
  // Write the headers
  bmpFile.write(reinterpret_cast<char *>(&fileHeader), sizeof(fileHeader));
  bmpFile.write(reinterpret_cast<char *>(&infoHeader), sizeof(infoHeader));
  // Get the device context
  HDC hdc = GetDC(NULL);
  // Prepare the structure for GetDIBits
  BITMAPINFO bmi;
  ZeroMemory(&bmi, sizeof(BITMAPINFO));
  bmi.bmiHeader = infoHeader;
  // Use GetDIBits to retrieve the bitmap data
  BYTE *pPixels = new BYTE[bitmapWidth * bitmapHeight * 4];
  GetDIBits(hdc, bitmap, 0, bitmapHeight, pPixels, &bmi, DIB_RGB_COLORS);
  // Write the bitmap data
  bmpFile.write(reinterpret_cast<char *>(pPixels), bitmapWidth * bitmapHeight * 4);
  // Clean up
  delete[] pPixels;
  ReleaseDC(NULL, hdc);
  bmpFile.close();
  return true;
}

void TestFramework::patternStripSelect(uint32_t x, uint32_t y, VSelectBox::SelectEvent sevent)
{
  // can't drag it yet
}

void TestFramework::ledClick(VWindow *window)
{
  uint32_t led;
  if (LED_COUNT == 10) {
    // the glove leds are reverse order
    led = LED_LAST - ((uint32_t)(uintptr_t)GetMenu(window->hwnd()) - LED_CIRCLE_ID);
  } else {
    led = ((uint32_t)(uintptr_t)GetMenu(window->hwnd()) - LED_CIRCLE_ID);
  }
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
  if (!m_initialized || !m_ledList || Time::isSimulation()) {
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
  if (!VortexEngine::curMode()) {
    return false;
  }
  // don't want to create a callback mechanism just for the test framework to be
  // notified of pattern changes, I'll just watch the patternID each tick
  Mode *targetMode = VortexEngine::curMode();
  if (!targetMode) {
    return false;
  }
  // this causes lag in editor when the tickrate is low
  // TODO: Find a way to detach this
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
  // backup the led colors
  RGBColor backupCols[LED_COUNT];
  memcpy(backupCols, m_ledList, sizeof(RGBColor) * LED_COUNT);
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
    // run the current mode like normal
    m_curMode.play();
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
  memcpy(m_ledList, backupCols, sizeof(RGBColor) * LED_COUNT);
  // redraw this led because it was written to generate pattern strip
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

DWORD __stdcall TestFramework::main_loop_thread(void *arg)
{
  TestFramework *framework = (TestFramework *)arg;
  // init the vortex engine
  framework->m_pCallbacks = Vortex::initEx<TestFrameworkCallbacks>();
  if (!framework->m_pCallbacks) {
    // failed to init, error
    return 0;
  }
  if (IRSimulator::isConnected()) {
    framework->m_IRLaunchButton.setEnabled(false);
    Vortex::openModeSharing();
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
    uint32_t newTickrate = framework->m_tickrate > 10 ? framework->m_tickrate : 10;
    if (newTickrate != Vortex::getTickrate()) {
      Vortex::setTickrate(newTickrate);
    }
    // scroll the background a little
    if (newTickrate > 10 && !Vortex::isSleeping()) {
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

string TestFramework::getWindowTitle()
{
  char text[2048] = {0};
  GetWindowText(m_window.hwnd(), text, sizeof(text));
  return text;
}

void TestFramework::setWindowTitle(string title)
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
  if (pin == 5) {
    // chromadeck button L
    return Vortex::isButtonPressed(0) ? 0 : 1;
  }
  if (pin == 6) {
    // chromadeck button M
    return Vortex::isButtonPressed(1) ? 0 : 1;
  }
  if (pin == 7) {
    // chromadeck button R
    return Vortex::isButtonPressed(2) ? 0 : 1;
  }
  if (pin == 20) {
    // orbit button 2
    return Vortex::isButtonPressed(1) ? 0 : 1;
  }
  return Vortex::isButtonPressed(0) ? 0 : 1;
#if 0
  // old code, this filtered by the LED_COUNT of the engine to avoid
  // pin collisions where one engine is using a pin that another engine
  // is using for it's main button... But that isn't an issue so better
  // to filter on pin number so that the button always work
  switch (LED_COUNT) {
  case 28: // orbit
    if (pin == 19 && Vortex::isButtonPressed(0)) {
      return 0;
    }
    if (pin == 20 && Vortex::isButtonPressed(1)) {
      return 0;
    }
    break;
  case 10: // glove
  case 3:  // handle
    if (pin == 1 && Vortex::isButtonPressed()) {
      return 0;
    }
    break;
  case 2:  // finger
    if (pin == 9 && Vortex::isButtonPressed()) {
      return 0;
    }
  default:
    break;
  }
  return 1;
#endif
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
