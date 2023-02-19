#include "EditorPipe.h"
#include "TestFramework.h"

HANDLE EditorPipe::hPipe = nullptr;
bool EditorPipe::m_serialConnected = false;

bool EditorPipe::init()
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
    // this will happen if two test frameworks are launched
    // maybe there's a better way around it idk
    return false;
  }
  // try to find editor window
  HWND hwnd = FindWindow("VWINDOW", NULL);
  if (hwnd != NULL) {
    // send it a message to tell it the test framework is here
    PostMessage(hwnd, WM_USER + 1, 0, 0);
  }
  return true;
}

BOOL CALLBACK EditorPipe::findEditorWindow(HWND hwnd, LPARAM lParam)
{
  char text[256] = {0};
  if (!GetWindowText(hwnd, text, sizeof(text) - 1) || !text[0]) {
    return true;
  }
  // try to find the word editor in the window so we don't send the message
  // to the color picker or something
  if (!strstr(text, "Editor")) {
    return true;
  }
  // success
  *(HWND *)lParam = hwnd;
  return false;
}

bool EditorPipe::connect()
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
  HWND editor_hwnd = NULL;
  EnumWindows(findEditorWindow, (LPARAM)&editor_hwnd);
  HWND hwnd = FindWindow("VWINDOW", NULL);
  if (hwnd != NULL) {
    // send it a message to tell it the test framework is here
    PostMessage(hwnd, WM_USER + 1, 0, 0);
  }
  m_serialConnected = true;
  return true;
}

int32_t EditorPipe::available()
{
  DWORD amount = 0;
  if (!PeekNamedPipe(hPipe, 0, 0, 0, &amount, 0)) {
    return 0;
  }
  return (int32_t)amount;
}

size_t EditorPipe::read(char *buf, size_t amt)
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

uint32_t EditorPipe::write(const uint8_t *buf, size_t len)
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
