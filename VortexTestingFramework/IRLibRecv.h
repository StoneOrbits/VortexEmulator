#pragma once

class IRrecv {
public:
  IRrecv(uint32_t pin) {
  }
  void enableIRIn() {}
  bool getResults() const {
    return true;
  }
};
