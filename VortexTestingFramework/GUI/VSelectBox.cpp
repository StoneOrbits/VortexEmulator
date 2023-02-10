#include "VSelectBox.h"

// Windows includes
#include <CommCtrl.h>
#include <Windowsx.h>

// Vortex Engine includes
#include "Colors/ColorTypes.h"

using namespace std;

#define WC_HUE_SAT_BOX "VSelectBox"

WNDCLASS VSelectBox::m_wc = {0};

VSelectBox::VSelectBox() :
  VWindow(),
  m_borderSize(1),
  m_width(0),
  m_height(0),
  m_innerLeft(0),
  m_innerTop(0),
  m_innerRight(0),
  m_innerBottom(0),
  m_innerWidth(0),
  m_innerHeight(0),
  m_drawHLine(true),
  m_drawVLine(true),
  m_drawCircle(true),
  m_doCapture(true),
  m_pressed(false),
  m_xSelect(0),
  m_ySelect(0),
  m_callback(nullptr),
  m_bitmap(nullptr),
  m_backgroundOffsetX(0),
  m_backgroundOffsetY(0)
{
}

VSelectBox::VSelectBox(HINSTANCE hInstance, VWindow &parent, const string &title,
  COLORREF backcol, uint32_t width, uint32_t height, uint32_t x, uint32_t y,
  uint32_t borderSize, uintptr_t menuID, VSelectBoxCallback callback) :
  VSelectBox()
{
  init(hInstance, parent, title, backcol, width, height, x, y, borderSize, menuID, callback);
}

VSelectBox::~VSelectBox()
{
  cleanup();
}

void VSelectBox::init(HINSTANCE hInstance, VWindow &parent, const string &title,
  COLORREF backcol, uint32_t width, uint32_t height, uint32_t x, uint32_t y,
  uint32_t borderSize, uintptr_t menuID, VSelectBoxCallback callback)
{
  // store callback and menu id
  m_callback = callback;

  // register window class if it hasn't been registered yet
  registerWindowClass(hInstance, backcol);

  if (!menuID) {
    menuID = nextMenuID++;
  }

  if (!parent.addChild(menuID, this)) {
    return;
  }

  m_borderSize = borderSize;

  m_innerWidth = width;
  m_innerHeight = height;

  uint32_t borders = m_borderSize * 2;
  m_width = m_innerWidth + borders;
  m_height = m_innerHeight + borders;

  m_innerLeft = m_borderSize;
  m_innerTop = m_borderSize;
  // if the inner width/height is 1 (it's 1 pixel big) then it's the same 
  // pixel as the inner left/top. That helps to understand the -1 offset
  // because the inside size is 1x1 but the inner left == inner right and
  // the inner top == inner bottom because it's all the same 1x1 inner pixel
  m_innerRight = m_borderSize + m_innerWidth - 1;
  m_innerBottom = m_borderSize + m_innerHeight - 1;

  // create the window
  m_hwnd = CreateWindow(WC_HUE_SAT_BOX, title.c_str(),
    WS_CHILD | WS_OVERLAPPED | WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN,
    x, y, m_width, m_height, parent.hwnd(), (HMENU)menuID, nullptr, nullptr);
  if (!m_hwnd) {
    MessageBox(nullptr, "Failed to open window", "Error", 0);
    throw exception("idk");
  }

  // set 'this' in the user data area of the class so that the static callback
  // routine can access the object
  SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

  m_colorLabel.init(hInstance, parent, "", backcol, 100, 24, x + width + 6, y + (height / 4), 0, nullptr);
}

void VSelectBox::cleanup()
{
}

void VSelectBox::create()
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

void VSelectBox::paint()
{
  PAINTSTRUCT paintStruct;
  memset(&paintStruct, 0, sizeof(paintStruct));
  HDC hdc = BeginPaint(m_hwnd, &paintStruct);

  // create a backbuffer and select it
  HDC backbuffDC = CreateCompatibleDC(hdc);
  HBITMAP backbuffer = CreateCompatibleBitmap(hdc, m_width, m_height);
  SelectObject(backbuffDC, backbuffer);

  // copy the bitmap into the backbuffer
  HDC bmpDC = CreateCompatibleDC(hdc);
  HBITMAP hbmpOld = (HBITMAP)SelectObject(bmpDC, m_bitmap);
  // fill the entire space with black to create a border
  BitBlt(backbuffDC, 0, 0, m_width, m_height, bmpDC, 0, 0, BLACKNESS);
  // then overwrite the inner area with the intended background
  BitBlt(backbuffDC, m_innerLeft, m_innerTop, m_innerWidth, m_innerHeight, bmpDC, m_backgroundOffsetX, m_backgroundOffsetY, SRCCOPY);
  DeleteDC(bmpDC);

  // draw the lines and circle
  uint32_t selectorSize = 5;
  if (m_drawHLine) {
    SelectObject(backbuffDC, GetStockObject(WHITE_PEN));
    if (m_drawCircle) {
      if (m_xSelect > selectorSize) {
        MoveToEx(backbuffDC, m_innerLeft, m_innerTop + m_ySelect, NULL);
        LineTo(backbuffDC, m_innerLeft + (m_xSelect - selectorSize), m_innerTop + m_ySelect);
      }
      MoveToEx(backbuffDC, m_innerLeft + (m_xSelect + selectorSize), m_innerTop + m_ySelect, NULL);
      LineTo(backbuffDC, m_innerLeft + m_innerWidth, m_innerTop + m_ySelect);
    } else {
      MoveToEx(backbuffDC, m_innerLeft, m_innerTop + m_ySelect, NULL);
      LineTo(backbuffDC, m_innerLeft + m_innerWidth, m_innerTop + m_ySelect);
    }
  }
  if (m_drawVLine) {
    SelectObject(backbuffDC, GetStockObject(WHITE_PEN));
    if (m_drawCircle) {
      if (m_ySelect > selectorSize) {
        MoveToEx(backbuffDC, m_innerLeft + m_xSelect, m_innerTop, NULL);
        LineTo(backbuffDC, m_innerLeft + m_xSelect, m_innerTop + (m_ySelect - selectorSize));
      }
      MoveToEx(backbuffDC, m_innerLeft + m_xSelect, m_innerTop + (m_ySelect + selectorSize), NULL);
      LineTo(backbuffDC, m_innerLeft + m_xSelect, m_innerTop + m_innerHeight);
    } else {
      MoveToEx(backbuffDC, m_innerLeft + m_xSelect, m_innerTop, NULL);
      LineTo(backbuffDC, m_innerLeft + m_xSelect, m_innerTop + m_innerHeight);
    }
  }
  if (m_drawCircle) {
    SelectObject(backbuffDC, GetStockObject(WHITE_PEN));
    SelectObject(backbuffDC, GetStockObject(HOLLOW_BRUSH));
    Ellipse(backbuffDC, m_innerLeft + (m_xSelect - selectorSize),
      m_innerTop + (m_ySelect - selectorSize),
      m_innerLeft + (m_xSelect + selectorSize) + 1,
      m_innerTop + (m_ySelect + selectorSize) + 1);
    SelectObject(backbuffDC, GetStockObject(BLACK_PEN));
    Ellipse(backbuffDC, m_innerLeft + (m_xSelect - selectorSize) + 1,
      m_innerTop + (m_ySelect - selectorSize) + 1,
      m_innerLeft + (m_xSelect + selectorSize),
      m_innerTop + (m_ySelect + selectorSize));
    SelectObject(backbuffDC, GetStockObject(WHITE_PEN));
    Ellipse(backbuffDC, m_innerLeft + (m_xSelect - selectorSize) + 2,
      m_innerTop + (m_ySelect - selectorSize) + 2,
      m_innerLeft + (m_xSelect + selectorSize) - 1,
      m_innerTop + (m_ySelect + selectorSize) - 1);
  }

  BitBlt(hdc, 0, 0, m_width, m_height, backbuffDC, 0, 0, SRCCOPY);

  DeleteObject(backbuffer);
  DeleteDC(backbuffDC);

  EndPaint(m_hwnd, &paintStruct);
}

void VSelectBox::command(WPARAM wParam, LPARAM lParam)
{
}

void VSelectBox::pressButton(WPARAM wParam, LPARAM lParam)
{
  if (m_doCapture) {
    // Get the window client area.
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    // Convert the client area to screen coordinates.
    POINT pt = { rc.left, rc.top };
    POINT pt2 = { rc.right, rc.bottom };
    ClientToScreen(m_hwnd, &pt);
    ClientToScreen(m_hwnd, &pt2);
    SetRect(&rc, pt.x, pt.y, pt2.x, pt2.y);
    // enable mouse capture on the window so we receive mouse move
    SetCapture(m_hwnd);
    // Confine the cursor.
    ClipCursor(&rc);
  }
  m_pressed = true;
  SelectEvent sevent = SELECT_PRESS;
  if (wParam & MK_CONTROL) {
    sevent = SELECT_CTRL_PRESS;
  }
  if (wParam & MK_SHIFT) {
    sevent = SELECT_SHIFT_PRESS;
  }
  doCallback(sevent);
}

void VSelectBox::releaseButton(WPARAM wParam, LPARAM lParam)
{
  if (!m_pressed) {
    return;
  }
  if (m_doCapture) {
    ClipCursor(NULL);
    ReleaseCapture();
  }
  m_pressed = false;
  doCallback(SELECT_RELEASE);
}

void VSelectBox::mouseMove()
{
  if (!m_pressed) {
    return;
  }
  doCallback(SELECT_DRAG);
}

void VSelectBox::doCallback(SelectEvent sevent)
{
  if (!m_callback) {
    return;
  }
  POINT pos;
  GetCursorPos(&pos);
  ScreenToClient(m_hwnd, &pos);
  if (pos.x < (LONG)m_innerLeft) pos.x = m_innerLeft;
  if (pos.x > (LONG)m_innerRight) pos.x = m_innerRight;
  if (pos.y < (LONG)m_innerTop) pos.y = m_innerTop;
  if (pos.y > (LONG)m_innerBottom) pos.y = m_innerBottom;
  uint32_t innerX = pos.x - m_innerLeft;
  uint32_t innerY = pos.y - m_innerTop;
  setSelection(innerX, innerY);
  m_callback(m_callbackArg, innerX, innerY, sevent);
}

void VSelectBox::setBackground(HBITMAP hBitmap)
{
  if (m_bitmap) {
    DeleteBitmap(m_bitmap);
  }
  m_bitmap = hBitmap;
}

void VSelectBox::setBackgroundOffset(int32_t x, int32_t y)
{
  m_backgroundOffsetX = x;
  m_backgroundOffsetY = y;
}

void VSelectBox::addBackgroundOffset(int32_t x, int32_t y, bool wrap)
{
  m_backgroundOffsetX += x;
  m_backgroundOffsetY += y;
  if (wrap) {
    m_backgroundOffsetX %= m_width;
    m_backgroundOffsetY %= m_height;
  }
}

void VSelectBox::setSelection(uint32_t x, uint32_t y)
{
  m_xSelect = x;
  m_ySelect = y;
  // redraw?
  redraw();
}

LRESULT CALLBACK VSelectBox::window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  VSelectBox *pColorSelect = (VSelectBox *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  if (!pColorSelect) {
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
  switch (uMsg) {
  case WM_VSCROLL:
    break;
  case WM_LBUTTONDOWN:
    pColorSelect->pressButton(wParam, lParam);
    break;
  case WM_LBUTTONUP:
    pColorSelect->releaseButton(wParam, lParam);
    break;
  case WM_MOUSEMOVE:
    pColorSelect->mouseMove();
    break;
  case WM_CTLCOLORSTATIC:
    return (INT_PTR)pColorSelect->m_wc.hbrBackground;
  case WM_ERASEBKGND:
    return 1;
  case WM_CREATE:
    pColorSelect->create();
    break;
  case WM_PAINT:
    pColorSelect->paint();
    return 0;
  case WM_COMMAND:
    pColorSelect->command(wParam, lParam);
    break;
  case WM_DESTROY:
    pColorSelect->cleanup();
    break;
  default:
    break;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void VSelectBox::registerWindowClass(HINSTANCE hInstance, COLORREF backcol)
{
  if (m_wc.lpfnWndProc == VSelectBox::window_proc) {
    // alredy registered
    return;
  }
  // class registration
  m_wc.lpfnWndProc = VSelectBox::window_proc;
  m_wc.hInstance = hInstance;
  m_wc.hbrBackground = CreateSolidBrush(backcol);
  m_wc.style = CS_GLOBALCLASS | CS_HREDRAW | CS_VREDRAW;
  m_wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  m_wc.lpszClassName = WC_HUE_SAT_BOX;
  RegisterClass(&m_wc);
}