#pragma once

class IRrecv {
public:
  IRrecv(uint32_t pin) {
  }
  void enableIRIn() {}
  void disableIRIn() {}
  bool getResults() const {
    return true;
  }
};
