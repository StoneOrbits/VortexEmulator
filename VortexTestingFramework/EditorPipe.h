#pragma once

#include <windows.h>
#include <inttypes.h>

// This is a named pipe to the editor so that the editor
// can connect with the test framework seamlessly

class EditorPipe
{
public:
  static bool init();
  static bool connect();
  static int32_t available();
  static size_t read(char *buf, size_t amt);
  static uint32_t write(const uint8_t *buf, size_t len);

  static bool isConnected() { return m_serialConnected; }

private:
  // callback to find the editor window
  static BOOL CALLBACK findEditorWindow(HWND hwnd, LPARAM lParam);

  static HANDLE hPipe;
  static bool m_serialConnected;
};
