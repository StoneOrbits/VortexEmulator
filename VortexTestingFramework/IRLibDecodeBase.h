#pragma once

class IRdecode {
public:
  IRdecode() {}
  void decode() {}
  void dumpResults(bool dump) { }
  uint32_t value;
  bool ignoreHeader;
};

typedef IRdecode IRdecodeNEC;
