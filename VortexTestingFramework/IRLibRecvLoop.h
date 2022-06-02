#pragma once

#include <inttypes.h>

class IRrecvLoop {
public:
  IRrecvLoop(uint32_t pin) { }
  void disableIRIn() {}
  void enableIRIn() {}

  bool getResults() { return true; }
};
