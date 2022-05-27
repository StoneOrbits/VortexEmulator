#include "TestFrameworkLinux.h"

int main(int argc, char *argv[])
{
  TestFramework framework;
  framework.init();
  framework.run();
  return 0;
}
