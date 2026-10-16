#pragma once
#include <M5Unified.h>
#include "DeviceMode.h"

class FaceUI {
 public:
  deviceui::State state;
  bool begin();
  void set(deviceui::Mode mode, const char* note = "");
  void link(deviceui::Link value);
  void level(unsigned value) { level_ = value > 10 ? 10 : value; }
  void tick(uint32_t now, bool force = false);
 private:
  M5Canvas frame_{&M5.Display};
  deviceui::Link link_ = deviceui::Link::Unconfigured;
  deviceui::Mode drawn_ = deviceui::Mode::Boot;
  char note_[32]{};
  bool available_ = false, dirty_ = true;
  unsigned level_ = 0;
  uint32_t lastFrame_ = 0;
};
