#include "TestFrameworkLinux.h"

int main(int argc, char *argv[])
{
  TestFramework framework;
  framework.init();
#ifndef WASM
  framework.run();
#endif
  return 0;
}
