#pragma once

#include <Windows.h>
#include <Dbt.h>

#include <map>
#include <string>

// The window class
#define WC_VWINDOW      "VWINDOW"

// The VWindow is the main window, there's only really supposed to be one
// if you want child windows that is a separate class. This is also the base
// class of all other GUI objects
class VWindow
{
public:
  // typedef for callback params
  typedef void (*VWindowCallback)(void *arg, VWindow *window);
  typedef void (*VMenuCallback)(void *arg, uintptr_t hMenu);
  typedef void (*VDeviceCallback)(void *arg, DEV_BROADCAST_HDR *dbh, bool added);

  VWindow();
  VWindow(HINSTANCE hinstance, const std::string &title, 
    COLORREF backcol, uint32_t width, uint32_t height,
    void *callbackArg);
  virtual ~VWindow();

  virtual void init(HINSTANCE hinstance, const std::string &title, 
    COLORREF backcol, uint32_t width, uint32_t height,
    void *callbackArg);
  virtual void cleanup();

  virtual bool process(MSG &msg);

  // windows message handlers
  virtual void create();
  virtual void paint();
  virtual INT_PTR controlColor(WPARAM wParam, LPARAM lParam);
  virtual void command(WPARAM wParam, LPARAM lParam);
  virtual void pressButton(WPARAM wParam, LPARAM lParam);
  virtual void releaseButton(WPARAM wParam, LPARAM lParam);

  // add/get a child
  virtual bool addChild(uintptr_t menuID, VWindow *child, uint32_t *out_id = nullptr); 
  virtual VWindow *getChild(uintptr_t menuID);
  virtual VWindow *getChild(HWND hwnd);

  // add a menu callback
  virtual uint32_t addCallback(uintptr_t menuID, VMenuCallback callback);
  virtual VMenuCallback getCallback(uintptr_t menuID);

  // add a WM_USER + id callback
  virtual void installUserCallback(uint32_t id, VWindowCallback callback);

  // install a device change callback
  virtual void installDeviceCallback(VDeviceCallback callback);

  // redraw this window
  virtual void redraw();

  virtual void setTooltip(std::string text);

  virtual void setVisible(bool visible);
  virtual void setEnabled(bool enable);

  virtual void setBackColor(COLORREF backcol);
  virtual void setForeColor(COLORREF forecol);

  virtual void setBackEnabled(bool enable);
  virtual void setForeEnabled(bool enable);

  virtual bool isVisible() const;
  virtual bool isEnabled() const;
  virtual bool isBackEnabled() const;
  virtual bool isForeEnabled() const;

  HWND hwnd() const { return m_hwnd; }
  HMENU menu() const { return GetMenu(m_hwnd); }

protected:
  // window handle
  HWND m_hwnd;
  // tooltip handle
  HWND m_tooltipHwnd;

  // list of children
  std::map<uintptr_t, VWindow *> m_children;

  // list of callbacks for menu presses
  std::map<uintptr_t, VMenuCallback> m_menuCallbacks;

  // pointer to parent
  VWindow *m_pParent;

  // arg to pass to callback
  void *m_callbackArg;

  // device callback
  HDEVNOTIFY m_hDeviceNotify;
  VDeviceCallback m_deviceCallback;

  // user callback
  std::map<uint32_t, VWindowCallback> m_userCallbacks;

  // background/foreground color
  COLORREF m_backColor;
  COLORREF m_foreColor;

  // enable background/foreground color
  bool m_backEnabled;
  bool m_foreEnabled;

  // for counting menu ids
  static uint32_t nextMenuID;

private:
  static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  static void registerWindowClass(HINSTANCE hInstance, COLORREF backcol);
  static WNDCLASS m_wc;
};

