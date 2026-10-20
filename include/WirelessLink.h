#pragma once
#include <Arduino.h>

// The hotspot password is the user-selected trust boundary. No app pairing.
class WirelessLink {
public:
  static constexpr unsigned port=48443;
  bool begin();
  bool online();
  bool ready() const { return initialized_; }
  void offline() { online_=false; }
private:
  bool initialized_=false, online_=false;
  uint32_t checked_=0;
};
