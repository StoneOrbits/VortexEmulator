#pragma once

#include "VWindow.h"

class VButton : public VWindow
{
public:
  // various press/release events
  enum ButtonEvent {
    BUTTON_EVENT_CLICK,
    BUTTON_EVENT_PRESS,
    BUTTON_EVENT_RELEASE,
  };
  // callback for a button
  typedef void (*VButtonCallback)(void *arg, VButton *button, ButtonEvent type);

  VButton();
  VButton(HINSTANCE hinstance, VWindow &parent, const std::string &title, 
    COLORREF backcol, uint32_t width, uint32_t height, uint32_t x, uint32_t y,
    uintptr_t menuID, VButtonCallback callback);
  virtual ~VButton();

  virtual void init(HINSTANCE hinstance, VWindow &parent, const std::string &title, 
    COLORREF backcol, uint32_t width, uint32_t height, uint32_t x, uint32_t y,
    uintptr_t menuID, VButtonCallback callback);
  virtual void cleanup();

  // windows message handlers
  virtual void create() override;
  virtual void paint() override;
  virtual void command(WPARAM wParam, LPARAM lParam) override;
  virtual void pressButton(WPARAM wParam, LPARAM lParam) override;
  virtual void releaseButton(WPARAM wParam, LPARAM lParam) override;

private:
  static LRESULT CALLBACK button_subproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  VButtonCallback m_callback;
  WNDPROC m_oldButtonProc;
  bool m_pressed;
};

