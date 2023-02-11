#pragma once

#include "VWindow.h"
#include "VLabel.h"

class RGBColor;

#define WC_VCIRCLE "VCIRCLE"

class VCircle : public VWindow
{
public:
  VCircle();
  VCircle(HINSTANCE hinstance, VWindow &parent, const std::string &title, 
    COLORREF backcol, uint32_t width, uint32_t height, uint32_t x, uint32_t y,
    uintptr_t menuID, VWindowCallback callback);
  virtual ~VCircle();

  virtual void init(HINSTANCE hinstance, VWindow &parent, const std::string &title, 
    COLORREF backcol, uint32_t width, uint32_t height, uint32_t x, uint32_t y,
    uintptr_t menuID, VWindowCallback callback);
  virtual void cleanup();

  // windows message handlers
  virtual void create() override;
  virtual void paint() override;
  virtual void command(WPARAM wParam, LPARAM lParam) override;
  virtual void pressButton(WPARAM wParam, LPARAM lParam) override;
  virtual void releaseButton(WPARAM wParam, LPARAM lParam) override;

  // set a background via callback to convert x/y to rgb
  //void setBackground(VFillCallback fillCallback);
  void setColor(uint32_t color, bool redraw = true);
  uint32_t getColor() const;

private:
  static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  static void registerWindowClass(HINSTANCE hInstance, COLORREF backcol);
  static WNDCLASS m_wc;

  HBITMAP m_background;

  uint32_t m_width;
  uint32_t m_height;

  DWORD m_col;

  VWindowCallback m_callback;
};

