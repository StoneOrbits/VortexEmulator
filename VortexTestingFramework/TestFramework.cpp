#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <Windows.h>
#include <CommCtrl.h>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <ctime>

#include "TestFramework.h"
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

#include "Menus/MenuList/EditorConnection.h"
#include "Menus/MenuList/PatternSelect.h"
#include "Menus/MenuList/ColorSelect.h"
#include "Menus/MenuList/Randomizer.h"

#include "VortexEngine.h"

#include "patterns/Pattern.h"
#include "patterns/single/SingleLedPattern.h"

#include "resource.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "ws2_32.lib")

TestFramework *g_pTestFramework = nullptr;

using namespace std;

#define CLICK_BUTTON_ID 10001
#define LED_CIRCLE_ID 20003
#define LAUNCH_IR_ID  30001

#define BACK_COL        RGB(40, 40, 40)

#define DEFAULT_PORT "33456"

class TestFrameworkCallbacks : public VortexCallbacks
{
public:
  TestFrameworkCallbacks() :
    sock(-1),
    client_sock(-1),
    is_server(false),
    hServerMutex(nullptr),
    hPipe(nullptr),
    m_serialConnected(false)
  {
    WSAData wsaData;
    // Initialize Winsock
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0) {
      printf("WSAStartup failed with error: %d\n", res);
      return;
    }
    // use a mutex to detect if server is running yet
    hServerMutex = CreateMutex(NULL, TRUE, "VortexServerMutex");
    if (!hServerMutex || GetLastError() == ERROR_ALREADY_EXISTS) {
      // otherwise try to connect client to server
      init_network_client();
    }
    // create a global pipe
    hPipe = CreateNamedPipe(
      "\\\\.\\pipe\\vortextestframework",
      PIPE_ACCESS_DUPLEX,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT,
      1,
      4096,
      4096,
      0,
      NULL);
    if (hPipe == INVALID_HANDLE_VALUE) {
      // only throw a message if this isn't the network client openign
      if (!hServerMutex) {
        std::string error = "Failed to open pipe";
        error += std::to_string(GetLastError());
        MessageBox(NULL, error.c_str(), "", 0);
      }
    }
    // try to find editor window
    HWND hwnd = FindWindow("VWINDOW", NULL);
    if (hwnd != NULL) {
      // send it a message to tell it the test framework is here
      PostMessage(hwnd, WM_USER + 1, 0, 0);
    }
  }
  virtual ~TestFrameworkCallbacks() {}
  
  void initServer()
  {
    init_server();
  }

  // called when engine reads digital pins, use this to feed button presses to the engine
  virtual long checkPinHook(uint32_t pin) override
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
  virtual void infraredWrite(bool mark, uint32_t amount) override
  {
    send_network_message(amount);
  }
  // called when engine checks for Serial, use this to indicate serial is connected
  virtual bool serialCheck() override
  {
    if (m_serialConnected) {
      return true;
    }
    // create a global pipe
    if (!ConnectNamedPipe(hPipe, NULL)) {
      int err = GetLastError();
      if (err != ERROR_PIPE_CONNECTED && err != ERROR_PIPE_LISTENING) {
        return false;
      }
    }
    m_serialConnected = true;
    return true;
  }

  // called when engine begins serial, use this to do any initialization of the connection
  virtual void serialBegin(uint32_t baud) override
  {
    // nothing to do here
  }
  // called when engine checks for data on serial, use this to tell the engine data is ready
  virtual int32_t serialAvail()
  {
    DWORD amount = 0;
    if (!PeekNamedPipe(hPipe, 0, 0, 0, &amount, 0)) {
      return 0;
    }
    return (int32_t)amount;
  }
  // called when engine reads from serial, use this to deliver data to the vortex engine
  virtual size_t serialRead(char *buf, size_t amt) override
  {
    DWORD total = 0;
    DWORD numRead = 0;
    do {
      if (!ReadFile(hPipe, buf + total, amt - total, &numRead, NULL)) {
        int err = GetLastError();
        if (err == ERROR_PIPE_NOT_CONNECTED) {
          printf("Fail\n");
        }
        break;
      }
      total += numRead;
    } while (total < amt);
    return total;
  }
  // called when engine writes to serial, use this to read data from the vortex engine
  virtual uint32_t serialWrite(const uint8_t *buf, size_t len)
  {
    DWORD total = 0;
    DWORD written = 0;
    do {
      if (!WriteFile(hPipe, buf + total, len - total, &written, NULL)) {
        break;
      }
      total += written;
    } while (total < len);
    return total;
  }
  // called when the LED strip is initialized
  virtual void ledsInit(void *cl, int count) override
  {
    g_pTestFramework->installLeds((CRGB *)cl, count);
  }
  // called when the brightness is changed
  virtual void ledsBrightness(int brightness) override
  {
    g_pTestFramework->setBrightness(brightness);
  }
  // called when the leds are shown
  virtual void ledsShow() override
  {
    g_pTestFramework->show();
  }

private:
  friend class TestFramework;

  // receive a message from client
  bool receive_message(uint32_t &out_message)
  {
    SOCKET target_sock = sock;
    if (is_server) {
      target_sock = client_sock;
    }
    if (recv(target_sock, (char *)&out_message, sizeof(out_message), 0) <= 0) {
      printf("Recv failed with error: %d\n", WSAGetLastError());
      return false;
    }
    return true;
  }
  bool send_network_message(uint32_t message)
  {
    SOCKET target_sock = sock;
    if (is_server) {
      target_sock = client_sock;
    }
    //static uint32_t counter = 0;
    //printf("Sending[%u]: %u\n", counter++, message);
    if (send(target_sock, (char *)&message, sizeof(message), 0) == SOCKET_ERROR) {
      // most likely server closed
      printf("send failed with error: %d\n", WSAGetLastError());
      return false;
    }
    return true;
  }
  // wait for a connection from client
  bool accept_connection()
  {
    // Wait for a client connection and accept it
    client_sock = accept(sock, NULL, NULL);
    if (client_sock == INVALID_SOCKET) {
      printf("accept failed with error: %d\n", WSAGetLastError());
      return 1;
    }
    printf("Received connection!\n");
    return true;
  }
  static DWORD __stdcall listen_connection(void *arg)
  {
    TestFrameworkCallbacks *pthis = (TestFrameworkCallbacks *)arg;
    // block till a client connects and clear the output files
    if (!pthis->accept_connection()) {
      return 0;
    }
    printf("Accepted connection\n");

    pthis->is_connected = true;

    // idk when another one launches the first ones strip unpaints
    PostMessage(NULL, WM_PAINT, NULL, NULL);

    // each message received will get passed into the logging system
    while (1) {
      uint32_t message = 0;
      if (!pthis->receive_message(message)) {
        break;
      }
      bool is_mark = (message & (1 << 31)) != 0;
      message &= ~(1 << 31);
      Vortex::IRDeliver(message);
      //printf("Received %s: %u\n", is_mark ? "mark" : "space", message);
    }

    printf("Connection closed\n");
    return 0;
  }

  // initialize the server
  bool init_server()
  {
    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    struct addrinfo *result = NULL;
    int res = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (res != 0) {
      printf("getaddrinfo failed with error: %d\n", res);
      WSACleanup();
      return false;
    }
    // Create a SOCKET for connecting to server
    sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock == INVALID_SOCKET) {
      printf("socket failed with error: %ld\n", WSAGetLastError());
      freeaddrinfo(result);
      return false;
    }
    // Setup the TCP listening socket
    res = bind(sock, result->ai_addr, (int)result->ai_addrlen);
    freeaddrinfo(result);
    if (res == SOCKET_ERROR) {
      printf("bind failed with error: %d\n", WSAGetLastError());
      closesocket(sock);
      return false;
    }
    // start listening
    if (listen(sock, SOMAXCONN) == SOCKET_ERROR) {
      printf("listen failed with error: %d\n", WSAGetLastError());
      return false;
    }
    CreateThread(NULL, 0, listen_connection, this, 0, NULL);
    printf("Success listening on *:8080\n");
    is_server = true;
    g_pTestFramework->setWindowTitle(g_pTestFramework->getWindowTitle() + " Receiver");
    g_pTestFramework->setWindowPos(900, 350);

    // launch another instance of the test framwork to act as the sender
    if (is_server) {
      char filename[2048] = { 0 };
      GetModuleFileName(GetModuleHandle(NULL), filename, sizeof(filename));
      PROCESS_INFORMATION procInfo;
      memset(&procInfo, 0, sizeof(procInfo));
      STARTUPINFO startInfo;
      memset(&startInfo, 0, sizeof(startInfo));
      CreateProcess(filename, NULL, NULL, NULL, false, 0, NULL, NULL, &startInfo, &procInfo);
    }
    return true;
  }

  // initialize the network client for server mode, thanks msdn for the code
  bool init_network_client()
  {
    struct addrinfo *addrs = NULL;
    struct addrinfo *ptr = NULL;
    struct addrinfo hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // allows ipv4 or ipv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    int res = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &addrs);
    if (res != 0) {
      printf("Could not resolve addr info\n");
      return false;
    }
    //info("Attempting to connect to %s", config.server_ip.c_str());
    // try connecting to all the addrs
    for (ptr = addrs; ptr != NULL; ptr = ptr->ai_next) {
      sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
      if (sock == INVALID_SOCKET) {
        printf("Error creating socket: %d\n", GetLastError());
        freeaddrinfo(addrs);
        return false;
      }
      if (connect(sock, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
        // try again
        closesocket(sock);
        sock = INVALID_SOCKET;
        continue;
      }
      // Success!
      break;
    }
    freeaddrinfo(addrs);
    if (sock == INVALID_SOCKET) {
      return false;
    }
    // turn on non-blocking for the socket so the module cannot
    // get stuck in the send() call if the server is closed
    u_long iMode = 1; // 1 = non-blocking mode
    res = ioctlsocket(sock, FIONBIO, &iMode);
    if (res != NO_ERROR) {
      printf("Failed to ioctrl on socket\n");
      closesocket(sock);
      return false;
    }
    printf("Success initializing network client\n");
    g_pTestFramework->setWindowTitle(g_pTestFramework->getWindowTitle() + " Sender");
    g_pTestFramework->setWindowPos(450, 350);
    is_connected = true;
    //info("Connected to server %s", config.server_ip.c_str());
    return true;
  }

  // network data (ir comms)
  SOCKET sock;
  SOCKET client_sock;
  bool is_server;
  bool is_connected;
  HANDLE hServerMutex;

  // pipe data (serial comms)
  HANDLE hPipe;
  bool m_serialConnected;
};

TestFrameworkCallbacks *g_pCallbacks = nullptr;

TestFramework::TestFramework() :
  m_window(),
  m_gloveBox(),
  m_patternStrip(),
  m_tickrateSlider(),
  m_button(),
  m_IRLaunchButton(),
  m_leds(),
  m_ledPos(),
  m_pauseMutex(nullptr),
  m_hInst(nullptr),
  m_consoleHandle(nullptr),
  m_gloveBMP(nullptr),
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
  g_pTestFramework = this;

  m_hInst = hInstance;

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
  m_window.init(m_hInst, "Vortex Orbit Emulator", BACK_COL, width, height, this);

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

  // launch the 'loop' thread
  m_loopThread = CreateThread(NULL, 0, TestFramework::arduino_loop_thread, this, 0, NULL);
  if (!m_loopThread) {
    // error
    return false;
  }

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
  if (!g_pCallbacks) {
    return;
  }
  g_pCallbacks->initServer();
  m_IRLaunchButton.setEnabled(false);
  Menus::openMenu(MENU_MODE_SHARING);
}

void TestFramework::patternStripSelect(uint32_t x, uint32_t y, VSelectBox::SelectEvent sevent)
{
  // can't drag it yet
}

void TestFramework::ledClick(VWindow *window)
{
  uint32_t led = LED_LAST - ((uint32_t)GetMenu(window->hwnd()) - LED_CIRCLE_ID);
  printf("Clicked led %u\n", led);
  m_curSelectedLed = (LedPos)led;
  handlePatternChange(true);
}

DWORD __stdcall TestFramework::do_tickrate(void *arg)
{
  // wait for a tick to finish
  while (!g_pTestFramework->pause());
  printf("Set tickrate to: %u\n", (uintptr_t)arg);
  // resume
  g_pTestFramework->unpause();
  return 0;
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

  m_initialized = true;
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
  // update the colors with the colors in the led list
  for (LedPos i = LED_FIRST; i < LED_COUNT; ++i) {
    uint32_t raw = m_ledList[i].raw();
    if (m_leds[i].getColor() != raw) {
      m_leds[i].setColor(raw);
    }
  }
}

bool TestFramework::pause()
{
  if (WaitForSingleObject(m_pauseMutex, 100) != WAIT_OBJECT_0) {
    DEBUG_LOG("Failed to pause");
    return false;
  }
}

void TestFramework::unpause()
{
  ReleaseMutex(m_pauseMutex);
  m_isPaused = false;
}

bool TestFramework::handlePatternChange(bool force)
{
  if (!Modes::curMode()) {
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
  LedPos realPos = (LedPos)(LED_LAST - m_curSelectedLed);
  if (isMultiLedPatternID(m_curMode.getPatternID())) {
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
  targetPat->bind(LED_FIRST);
  // clear and re-generate the pattern strip
  for (int x = 0; x < patternStripWidth; ++x) {
    // run the pattern like normal
    targetPat->play();
    // tick the virtual time forward so that next play()
    // the engine will think a tick has passed
    Time::tickSimulation();
    // sample the color for the selected LED
    COLORREF col = Leds::getLed(LED_FIRST).raw();
    // fill the entire column of the bitmap with this color
    for (int y = 0; y < patternStripHeight; ++y) {
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
  m_ledList[LED_FIRST] = backupCol;
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
  Vortex::init<TestFrameworkCallbacks>();
  g_pCallbacks = (TestFrameworkCallbacks *)Vortex::vcallbacks();
  if (g_pCallbacks->is_connected) {
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
