#include "TestFrameworkLinux.h"

int main(int argc, char *argv[])
{
  TestFramework framework;
  framework.init(argc, argv);
#ifndef WASM
  while (framework.stillRunning()) {
    framework.run();
  }
#endif
  return 0;
}
