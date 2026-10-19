#pragma once
#include <SPIFFS.h>
#include "LocalWake.h"
#include "AudioDevice.h"

// Read-only reuse of one template; never register or send wake audio.
class GeminiWake {
 public:
  void begin() {
    String error;
    available_ = SPIFFS.begin(false) && config_.load(error, false) &&
                 config_.wakeEnabled && wake_.begin(config_, false) && wake_.registered();
  }
  bool available() const { return available_; }
  void stop(AudioDevice& audio) {
    if (!running_) return;
    audio.stop(); wake_.reset(); running_ = false;
    memset(frame_, 0, sizeof(frame_));
  }
  bool poll(AudioDevice& audio) {
    if (!available_) return false;
    if (!running_) {
      wake_.reset();
      if (!audio.listen(16000)) { available_ = false; return false; }
      running_ = true;
    }
    if (!audio.read(frame_, 320)) { stop(audio); available_ = false; return false; }
    String ignored;
    const bool matched = wake_.push(frame_, false, ignored);
    memset(frame_, 0, sizeof(frame_));
    if (matched) stop(audio);
    return matched;
  }
 private:
  Config config_;
  LocalWake wake_;
  int16_t frame_[320]{};
  bool available_ = false, running_ = false;
};
