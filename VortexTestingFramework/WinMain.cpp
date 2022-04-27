#include <Windows.h>

#include "TestFramework.h"

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
  TestFramework framework;
  framework.init(hInstance);
  framework.run();
  return 0;
}
