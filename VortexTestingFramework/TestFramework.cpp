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
#include "Modes/Modes.h"

#include "patterns/Pattern.h"
#include "patterns/single/SingleLedPattern.h"

#include "resource.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "ws2_32.lib")

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
  m_curMode(),
  m_patternStrip(),
  m_redrawStrip(true),
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
  m_hwnd = CreateWindow(m_wc.lpszClassName, "Vortex Glove Emulator",
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
    198, 312, 48, 24, hwnd, (HMENU)CLICK_BUTTON_ID, nullptr, nullptr);

  // sub-process the button to capture the press/release individually
  g_pTestFramework->m_oldButtonProc = (WNDPROC)SetWindowLongPtr(m_hwndClickButton, GWLP_WNDPROC,
    (LONG_PTR)TestFramework::button_subproc);

  m_hwndTickrateSlider = CreateWindow(TRACKBAR_CLASS, "Tickrate",
    WS_VISIBLE | WS_CHILD | WS_TABSTOP,
    14, 330, 120, 24, hwnd, (HMENU)TICKRATE_SLIDER_ID, nullptr, nullptr);

  // launch the 'loop' thread
  m_loopThread = CreateThread(NULL, 0, TestFramework::arduino_loop_thread, this, 0, NULL);
  if (!m_loopThread) {
    // error
    return;
  }

  //m_hwndTickOffsetSlider = CreateWindow(TRACKBAR_CLASS, "Time Offset",
  //  WS_VISIBLE | WS_CHILD | WS_TABSTOP | TBS_VERT,
  //  360, 30, 36, 160, hwnd, (HMENU)TIME_OFFSET_SLIDER_ID, nullptr, nullptr);

  //m_hwndLoadButton = CreateWindow(WC_BUTTON, "+",
  //  WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON | WS_TABSTOP,
  //  5, 5, 22, 22, hwnd, (HMENU)LOAD_BUTTON_ID, nullptr, nullptr);
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
  BitBlt(hdc, 86, 30, 250, 320, hdcGlove, 0, 0, SRCCOPY);
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
  }
#if 0
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
#endif
#endif

  EndPaint(hwnd, &ps);
}

void TestFramework::cleanup()
{
  // turn off the loops and unpause
  m_keepGoing = false;
  m_isPaused = false;
  // wait for the loop to finish, 3 seconds I guess
  WaitForSingleObject(m_loopThread, 3000);
  // delete the thing
  DeleteObject(m_bkbrush);
}

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
      std::string error = "Failed to open pipe";
      error += std::to_string(GetLastError());
      MessageBox(NULL, error.c_str(), "", 0);
    }
    // try to find editor window
    HWND hwnd = FindWindow("VWINDOW", NULL);
    if (hwnd != NULL) {
      // send it a message to tell it the test framework is here
      PostMessage(hwnd, WM_USER + 1, 0, 0);
    }
#ifdef SIMULATE_IR_COMMS
    WSAData wsaData;
    // Initialize Winsock
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0) {
      printf("WSAStartup failed with error: %d\n", res);
      return;
    }
    // use a mutex to detect if server is running yet
    hServerMutex = CreateMutex(NULL, TRUE, "VortexServerMutex");
    if (hServerMutex && GetLastError() != ERROR_ALREADY_EXISTS) {
      // initialize the listen server
      init_server();
    } else {
      // otherwise try to connect client to server
      init_network_client();
    }
#endif
  }
  virtual ~TestFrameworkCallbacks() {}

  // called when engine reads digital pins, use this to feed button presses to the engine
  virtual long checkPinHook(uint32_t pin) override
  {
    if (pin == 1) {
      // get button state
      if (g_pTestFramework->isButtonPressed()) {
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

    // idk when another one launches the first ones strip unpaints
    g_pTestFramework->redrawStrip();
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
    CreateThread(NULL, 0, listen_connection, NULL, 0, NULL);
    printf("Success listening on *:8080\n");
    is_server = true;
    g_pTestFramework->setWindowTitle(g_pTestFramework->getWindowTitle() + " Receiver");
    g_pTestFramework->setWindowPos(250, 650);

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
    g_pTestFramework->setWindowPos(1300, 650);
    //info("Connected to server %s", config.server_ip.c_str());
    return true;
  }

  // network data (ir comms)
  SOCKET sock;
  SOCKET client_sock;
  bool is_server;
  HANDLE hServerMutex;

  // pipe data (serial comms)
  HANDLE hPipe;
  bool m_serialConnected;
};

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
  Mode *targetMode = Modes::curMode();
  if (!targetMode) {
    return false;
  }
#if 0
  // cant do this it causes too much lag in the editor
  Menu *curMenu = Menus::curMenu();
  if (curMenu && curMenu->curMode()) {
    targetMode = curMenu->curMode();
  }
#endif
  // check to see if the mode changed
  if (!force && m_curMode.equals(targetMode)) {
    return false;
  }
  // update current mode
  m_curMode = *targetMode;
  m_curMode.init();
  // the realpos is used to target the actual index of pattern to run
  LedPos realPos = (LedPos)(m_curSelectedLed);
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
  RGBColor backupCol = m_ledList[m_curSelectedLed];
  // begin the time simulation so we can tick forward
  Time::startSimulation();
  // clear and re-generate the pattern strip
  m_patternStrip.clear();
  for (int i = 0; i < width; ++i) {
    // run the pattern each simulated tick
    targetPat->play();
    m_patternStrip.push_back(Leds::getLed(m_curSelectedLed));
    Time::tickSimulation();
  }
  // end the time simulation, this snaps the tickcount
  // back to where it was before starting the sim
  Time::endSimulation();
  // restore original color on the target led
  m_ledList[m_curSelectedLed] = backupCol;
  // idk why this sleep is necessary, bad synchronization
  //Sleep(100);
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
  // init the vortex engine
  Vortex::init<TestFrameworkCallbacks>();
  TrackBar_SetPos(framework->m_hwndTickrateSlider, 30);
  //TrackBar_SetPos(m_hwndTickOffsetSlider, 0);
  // init tickrate and time offset to match the sliders
  framework->setTickrate();
  //setTickOffset();
  while (framework->m_initialized && framework->m_keepGoing) {
    DWORD dwWaitResult = WaitForSingleObject(framework->m_pauseMutex, INFINITE);  // no time-out interval
    if (dwWaitResult == WAIT_OBJECT_0) {
      // run the tick
      Vortex::tick();
      // backup the colors
      memcpy(framework->m_lastLedColor, framework->m_ledList, sizeof(RGBColor) * LED_COUNT);
      // if pattern changes we need to reload the pattern strip
      framework->handlePatternChange();
      ReleaseMutex(framework->m_pauseMutex);
    }
  }
  // cleanup
  Vortex::cleanup();
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
  case WM_ERASEBKGND:
    if (hwnd != g_pTestFramework->m_hwnd) {
      return 1;
    }
  case WM_LBUTTONDOWN:
    g_pTestFramework->handleWindowClick(LOWORD(lParam), HIWORD(lParam));
    break;
  case WM_COMMAND:
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
