#pragma once

class IRsend {
public:
  IRsend() {}
  void send(uint32_t protocol = 0, uint32_t value = 0, uint32_t thing = 0) {}
};

typedef IRsend IRsendNEC;
