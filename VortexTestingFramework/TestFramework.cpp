#include <Windows.h>
#include <CommCtrl.h>

#include <string>
#include <map>

#include "TestFramework.h"

#include "VortexGloveset.h"

TestFramework *g_pTestFramework = nullptr;
FILE *TestFramework::m_logHandle = nullptr;

using namespace std;

#define CLICK_BUTTON_ID 10001

TestFramework::TestFramework() :
    m_gloveSet(),
    m_paintCallback(nullptr),
    m_paintArg(nullptr),
    m_pButtonDown(nullptr),
    m_hwndButton(NULL),
    m_hwnd(NULL),
    m_wc(),
    bkbrush(),
    m_brightness(255),
    m_ledPos(),
    m_ledList(nullptr),
    m_numLeds(0),
    m_initialized(false),
    m_buttonPressed(false),
    m_hLoopThread(NULL),
    m_keepGoing(true)
{
}

bool TestFramework::init(HINSTANCE hInstance)
{
    if (g_pTestFramework) {
        return false;
    }
    g_pTestFramework = this;

    bkbrush = CreateSolidBrush(bkcolor);

    // class registration
    memset(&m_wc, 0, sizeof(m_wc));
    m_wc.lpfnWndProc = TestFramework::window_proc;
    m_wc.hInstance = hInstance;
    m_wc.lpszClassName = "Vortex Test Framework";
    RegisterClass(&m_wc);

    // get desktop rect so we can center the window
    RECT desktop;
    GetClientRect(GetDesktopWindow(), &desktop);

    // create the window
    m_hwnd = CreateWindow(m_wc.lpszClassName, "Vortex Test Framework",
        WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        (desktop.right/2) - 240, (desktop.bottom/2) - 84,
        420, 269, NULL, NULL, hInstance, NULL);
    if (!m_hwnd) {
        MessageBox(NULL, "Failed to open window", "Error", 0);
        return 0;
    }

    if (!m_logHandle) {
        AllocConsole();
        freopen_s(&m_logHandle, "CONOUT$", "w", stdout);
    }

    return true;
}
#include <chrono>

void TestFramework::run()
{
    MSG msg;
    ShowWindow(m_hwnd, SW_NORMAL);
    while (1) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            typedef std::chrono::high_resolution_clock hiresclock;
            static auto timer = hiresclock::now();
            auto milisec = (hiresclock::now() - timer).count() / 1000000;
            if (milisec > 50) {
                timer = hiresclock::now();
                if (m_initialized && m_keepGoing) {
                    g_pTestFramework->arduino_loop();
                }
                //... draw
            }
        }
    }

    //// main message loop
    //while (GetMessage(&msg, NULL, 0, 0)) {
    //    if (!IsDialogMessage(m_hwnd, &msg)) {
    //        TranslateMessage(&msg);
    //        DispatchMessage(&msg);
    //    }
    //}
}

void TestFramework::create(HWND hwnd)
{
    // create the server checkbox and ip textbox
    //m_hwndButton = CreateWindow(WC_BUTTON, "Click",
    //    WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON | WS_TABSTOP,
    //    120, 160, 170, 32, hwnd, (HMENU)CLICK_BUTTON_ID, NULL, NULL);

    // do the arduino init/setup
    arduino_setup();

#if 0
    m_hLoopThread = CreateThread(NULL, 0, TestFramework::run_loop, this, 0, NULL);
    if (!m_hLoopThread) {
        // error?
        return;
    }
#endif
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

    // button control
    switch (HIWORD(wParam)) {
    case BN_PUSHED:
        m_buttonPressed = true;
        break;
    case BN_UNPUSHED:
        m_buttonPressed = false;
        break;
    }
}

void TestFramework::paintbg(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
   
    // paint the background
    FillRect(hdc, &ps.rcPaint, (HBRUSH)bkbrush);

    EndPaint(hwnd, &ps);
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
    for (int i = 0; i < m_numLeds; ++i) {
        COLORREF col = RGB(m_ledList[i].bRed, m_ledList[i].bGreen, m_ledList[i].bBlue);
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
    WaitForSingleObject(m_hLoopThread, 3000);
    TerminateThread(m_hLoopThread, 0);
    DeleteObject(bkbrush);
}

void TestFramework::arduino_setup()
{
    if (!m_gloveSet.init()) {
        // uhoh
    }
}

void TestFramework::arduino_loop()
{
    static int i = 0;
    //Debug("Tick: %u", i++);
    // run the tick
    m_gloveSet.tick();
}

void TestFramework::installLeds(CRGB *leds, uint32_t count)
{
    m_ledList = leds;
    m_numLeds = count;

    // initialize the positions of all the leds
    uint32_t base_left = 70;
    uint32_t base_top = 50;
    uint32_t radius = 15;
    uint32_t d = radius * 2;
    for (int i = 0; i < NUM_LEDS; ++i) {
        m_ledPos[i].left = base_left + (i - (i%2)) * d;
        m_ledPos[i].right = base_left + d + (i - (i%2)) * d;
        m_ledPos[i].top = base_top + ((i % 2) * d);
        m_ledPos[i].bottom = base_top + d + ((i % 2) * d);
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
    ledArea.top = m_ledPos[0].top;
    ledArea.right = m_ledPos[LED_COUNT - 1].right;
    ledArea.bottom = m_ledPos[LED_COUNT - 1].bottom;
    InvalidateRect(m_hwnd, &ledArea, FALSE);
}

bool TestFramework::isButtonPressed() const
{
    return (GetKeyState(VK_SPACE) & 0x100) != 0;
}

LRESULT CALLBACK TestFramework::window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_ERASEBKGND:
        g_pTestFramework->paintbg(hwnd);
        break;
    case WM_CREATE:
        //SetTimer(hwnd, 1, 10, NULL);
        g_pTestFramework->create(hwnd);
        break;
    //case WM_TIMER:
    //    InvalidateRect(hwnd, NULL, FALSE);
    //    break;
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

DWORD __stdcall TestFramework::run_loop(void *arg)
{
    TestFramework *obj = (TestFramework *)arg;
    while (obj->keepGoing()) {
        obj->arduino_loop();
    }
    return 0;
}

void TestFramework::_printlog(const char *file, const char *func, int line, const char *msg, ...)
{
    va_list list;
    va_start(list, msg);
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
    vfprintf(m_logHandle, strMsg.c_str(), list);
    va_end(list);
}
