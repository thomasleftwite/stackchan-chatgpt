#pragma once
#include <Arduino.h>

class AudioDevice {
 public:
  bool listen(uint32_t rate);
  bool read(int16_t* dest, size_t samples);
  bool output();
  bool play(const int16_t* data, size_t samples);
  bool playing() const;
  void stop();
  bool cue();
 private:
  uint32_t rate_ = 0;
};
