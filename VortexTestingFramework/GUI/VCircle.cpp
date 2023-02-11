#include "VCircle.h"

// Windows includes
#include <CommCtrl.h>
#include <Windowsx.h>

// Vortex Engine includes
#include "Colors/ColorTypes.h"

#pragma comment(lib, "Msimg32.lib")

using namespace std;

WNDCLASS VCircle::m_wc = {0};

VCircle::VCircle() :
  VWindow(),
  m_background(nullptr),
  m_width(0),
  m_height(0),
  m_col(),
  m_callback(nullptr)
{
}

VCircle::VCircle(HINSTANCE hInstance, VWindow &parent, const string &title,
  COLORREF backcol, uint32_t width, uint32_t height, uint32_t x, uint32_t y,
  uintptr_t menuID, VWindowCallback callback) :
  VCircle()
{
  init(hInstance, parent, title, backcol, width, height, x, y, menuID, callback);
}

VCircle::~VCircle()
{
  cleanup();
}

void VCircle::init(HINSTANCE hInstance, VWindow &parent, const string &title,
  COLORREF backcol, uint32_t width, uint32_t height, uint32_t x, uint32_t y,
  uintptr_t menuID, VWindowCallback callback)
{
  // store callback and menu id
  m_callback = callback;
  m_width = width;
  m_height = height;

  // register window class if it hasn't been registered yet
  registerWindowClass(hInstance, backcol);

  if (!menuID) {
    menuID = nextMenuID++;
  }

  if (!parent.addChild(menuID, this)) {
    return;
  }

  // create the window
  m_hwnd = CreateWindow(WC_VCIRCLE, title.c_str(),
    WS_CHILD | WS_OVERLAPPED | WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN,
    x, y, m_width, m_height, parent.hwnd(), (HMENU)menuID, nullptr, nullptr);
  if (!m_hwnd) {
    MessageBox(nullptr, "Failed to open window", "Error", 0);
    throw exception("idk");
  }

  // set 'this' in the user data area of the class so that the static callback
  // routine can access the object
  SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

  // adjust the window region so it's only a circle
  HRGN hRegion = CreateEllipticRgn(1, 1, m_width-1, m_height - 1);
  SetWindowRgn(m_hwnd, hRegion, true);
  DeleteObject(hRegion);
}

void VCircle::cleanup()
{
}

void VCircle::create()
{
}

static HBRUSH getBrushCol(DWORD rgbcol)
{
  static std::map<COLORREF, HBRUSH> m_brushmap;
  HBRUSH br;
  COLORREF col = RGB((rgbcol >> 16) & 0xFF, (rgbcol >> 8) & 0xFF, rgbcol & 0xFF);
  if (m_brushmap.find(col) == m_brushmap.end()) {
    br = CreateSolidBrush(col);
    m_brushmap[col] = br;
  }
  br = m_brushmap[col];
  return br;
}

void VCircle::paint()
{
  PAINTSTRUCT paintStruct;
  memset(&paintStruct, 0, sizeof(paintStruct));
  HDC hdc = BeginPaint(m_hwnd, &paintStruct);

  // create a backbuffer and select it
  HDC backbuffDC = CreateCompatibleDC(hdc);
  HBITMAP backbuffer = CreateCompatibleBitmap(hdc, m_width, m_height);
  SelectObject(backbuffDC, backbuffer);

  // draw to the back buffer
  SelectObject(backbuffDC, getBrushCol(m_col));
  Rectangle(backbuffDC, 0, 0, m_width, m_height);

  // TODO: make the edges not look so bad

  // copy in the back buffer
  BitBlt(hdc, 0, 0, m_width, m_height, backbuffDC, 0, 0, SRCCOPY);

  DeleteObject(backbuffer);
  DeleteDC(backbuffDC);

  EndPaint(m_hwnd, &paintStruct);
}

void VCircle::command(WPARAM wParam, LPARAM lParam)
{
}

void VCircle::pressButton(WPARAM wParam, LPARAM lParam)
{
  if (!m_callback) {
    return;
  }
  m_callback(m_callbackArg, this);
}

void VCircle::releaseButton(WPARAM wParam, LPARAM lParam)
{
}

void VCircle::setColor(uint32_t col, bool doredraw)
{
  m_col = col;
  if (doredraw) {
    redraw();
  }
}

uint32_t VCircle::getColor() const
{
  return m_col;
}

LRESULT CALLBACK VCircle::window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  VCircle *pCircle = (VCircle *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  if (!pCircle) {
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
  switch (uMsg) {
  case WM_VSCROLL:
    break;
  case WM_LBUTTONDOWN:
    pCircle->pressButton(wParam, lParam);
    break;
  case WM_LBUTTONUP:
    pCircle->releaseButton(wParam, lParam);
    break;
  case WM_CTLCOLORSTATIC:
    return (INT_PTR)pCircle->m_wc.hbrBackground;
  case WM_ERASEBKGND:
    return 1;
  case WM_CREATE:
    pCircle->create();
    break;
  case WM_PAINT:
    pCircle->paint();
    return 0;
  case WM_COMMAND:
    pCircle->command(wParam, lParam);
    break;
  case WM_DESTROY:
    pCircle->cleanup();
    break;
  default:
    break;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void VCircle::registerWindowClass(HINSTANCE hInstance, COLORREF backcol)
{
  if (m_wc.lpfnWndProc == VCircle::window_proc) {
    // alredy registered
    return;
  }
  // class registration
  m_wc.lpfnWndProc = VCircle::window_proc;
  m_wc.hInstance = hInstance;
  m_wc.hbrBackground = CreateSolidBrush(backcol);
  m_wc.style = CS_GLOBALCLASS | CS_HREDRAW | CS_VREDRAW;
  m_wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  m_wc.lpszClassName = WC_VCIRCLE;
  RegisterClass(&m_wc);
}