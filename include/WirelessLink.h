#pragma once
#include <Arduino.h>

// Persistent device credential. No credential ever appears in USB diagnostics.
class WirelessLink {
public:
  static constexpr unsigned port=48443;
  bool begin();
  bool online();
  bool paired() const { return paired_; }
  bool ready() const { return key_[0]!=0; }
  const char* key() const { return key_; }
  void showCode();
  bool resetPairing();
  void offline() { online_=false; }
private:
  char key_[33]{};
  bool paired_=false, online_=false, codeShown_=false;
  uint32_t checked_=0;
};
