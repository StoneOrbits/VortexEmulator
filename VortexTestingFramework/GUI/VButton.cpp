#include "VButton.h"

// Windows includes
#include <CommCtrl.h>

using namespace std;

VButton::VButton() :
  VWindow(),
  m_callback(nullptr),
  m_oldButtonProc(nullptr),
  m_pressed(false)
{
}

VButton::VButton(HINSTANCE hInstance, VWindow &parent, const string &title,
  COLORREF backcol, uint32_t width, uint32_t height, uint32_t x, uint32_t y,
  uintptr_t menuID, VButtonCallback callback) :
  VButton()
{
  init(hInstance, parent, title, backcol, width, height, x, y, menuID, callback);
}

VButton::~VButton()
{
  cleanup();
}

void VButton::init(HINSTANCE hInstance, VWindow &parent, const string &title,
  COLORREF backcol, uint32_t width, uint32_t height, uint32_t x, uint32_t y,
  uintptr_t menuID, VButtonCallback callback)
{
  // store callback and menu id
  m_callback = callback;
  m_backColor = backcol;
  m_foreColor = RGB(0xD0, 0xD0, 0xD0);

  if (!menuID) {
    menuID = nextMenuID++;
  }

  if (!parent.addChild(menuID, this)) {
    return;
  }

  // create the window
  m_hwnd = CreateWindow(WC_BUTTON, title.c_str(),
    WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON | WS_TABSTOP,
    x, y, width, height, parent.hwnd(), (HMENU)menuID, nullptr, nullptr);
  if (!m_hwnd) {
    MessageBox(nullptr, "Failed to open window", "Error", 0);
    throw exception("idk");
  }

  // set 'this' in the user data area of the class so that the static callback
  // routine can access the object
  SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

  // sub-process the button to capture the press/release individually
  m_oldButtonProc = (WNDPROC)SetWindowLongPtr(m_hwnd, GWLP_WNDPROC,
    (LONG_PTR)VButton::button_subproc);
}

void VButton::cleanup()
{
}

void VButton::create()
{
}

void VButton::paint()
{
}

void VButton::command(WPARAM wParam, LPARAM lParam)
{
  if (!m_callback) {
    return;
  }
  m_callback(m_callbackArg, this, BUTTON_EVENT_CLICK);
}

void VButton::pressButton(WPARAM wParam, LPARAM lParam)
{
  if (m_pressed) {
    return;
  }
  m_callback(m_callbackArg, this, BUTTON_EVENT_PRESS);
  m_pressed = true;
}

void VButton::releaseButton(WPARAM wParam, LPARAM lParam)
{
  if (!m_pressed) {
    return;
  }
  m_callback(m_callbackArg, this, BUTTON_EVENT_RELEASE);
  m_pressed = false;
}

LRESULT CALLBACK VButton::button_subproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  VButton *pButton = (VButton *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
  if (!pButton) {
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
  }
  switch (uMsg) {
  case WM_LBUTTONDOWN:
    pButton->pressButton(wParam, lParam);
    break;
  case WM_LBUTTONUP:
    pButton->releaseButton(wParam, lParam);
    break;
  default:
    break;
  }
  return CallWindowProc(pButton->m_oldButtonProc, hWnd, uMsg, wParam, lParam);
}
