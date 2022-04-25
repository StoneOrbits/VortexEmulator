#pragma once
#include <Windows.h>
#include <stdio.h>

#include "VortexGloveset.h"

#define NUM_LEDS 10

// paint callback type
typedef void (*paint_fn_t)(void *, HDC);

#define Debug(msg, ...) \
    TestFramework::_printlog(__FILE__, __FUNCTION__, __LINE__, msg, __VA_ARGS__)

class TestFramework
{
public:
    TestFramework();

    // initialize the test framework
    bool init(HINSTANCE hInstance);
    // run the test framework
    void run();
    
    // windows message handlers
    void create(HWND hwnd);
    void command(WPARAM wParam, LPARAM lParam);
    void paintbg(HWND hwnd);
    void paint(HWND hwnd);
    void cleanup();
    
    // arduino setup/loop
    void arduino_setup();
    void arduino_loop();

    void installLeds(CRGB *leds, uint32_t count);
    void setBrightness(int brightness);
    void show();

    // whether the button is pressed
    bool isButtonPressed() const;

    // whether initialized
    bool initialized() const { return m_initialized; }
    // whether to exit
    bool keepGoing() const { return m_keepGoing; }

    // main window procedure
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static DWORD __stdcall run_loop(void *arg);

    static void _printlog(const char *file, const char *func, int line, const char *msg, ...);

private:
    static FILE *m_logHandle;

    // these are in no particular order
    VortexGloveset m_gloveSet;

    paint_fn_t m_paintCallback;
    void *m_paintArg;

    bool *m_pButtonDown;
    HWND m_hwndButton;

    HWND m_hwnd;
    WNDCLASS m_wc;

    const COLORREF bkcolor = RGB(40, 40, 40);
    HBRUSH bkbrush;
    
    int m_brightness;

    RECT m_ledPos[NUM_LEDS];

    CRGB *m_ledList;
    uint32_t m_numLeds;

    bool m_initialized;

    bool m_buttonPressed;

    HANDLE m_hLoopThread;
    bool m_keepGoing;
};

extern TestFramework *g_pTestFramework;
