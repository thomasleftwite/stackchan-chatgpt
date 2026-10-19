#pragma once
#include <SPIFFS.h>
#include "LocalWake.h"
#include "AudioDevice.h"

// Local call detection and verified enrollment. Never send call audio to a server.
class GeminiWake {
 public:
  void begin() {
    String error;
    initialized_ = SPIFFS.begin(false) && config_.load(error, false) && wake_.begin(config_, false);
    available_ = initialized_ && config_.wakeEnabled && wake_.registered();
  }
  bool available() const { return available_; }
  bool canEnroll() const { return initialized_ && config_.wakeEnabled; }
  bool prepare(const int16_t* data,size_t count) { return canEnroll() && wake_.prepareCandidate(data,count); }
  bool collect(const int16_t* data,size_t count) { return canEnroll() && wake_.collectCandidate(data,count); }
  bool validate() { return canEnroll() && wake_.validateCandidates(); }
  bool commit() { const bool ok=wake_.commitCandidate(); if(ok) available_=true; return ok; }
  void cancel() { if(initialized_) wake_.cancelCandidate(); }
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
  bool available_ = false, running_ = false, initialized_ = false;
};
