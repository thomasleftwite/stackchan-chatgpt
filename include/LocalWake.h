#pragma once
#include <Arduino.h>
#include <memory>
#include <simplevox.h>
#include "ConversationRules.h"
#include "Config.h"

class LocalWake {
 public:
  bool begin(const Config& config, bool recoverBackup = true);
  bool registered() const { return bool(reference_); }
  void reset();
  // 320 samples, 16 kHz. All buffers remain local.
  bool push(const int16_t* samples, bool registration, String& message);
 private:
  bool validFile(const String& path);
  bool save(const simplevox::MfccFeature& feature);
  const Config* config_ = nullptr;
  simplevox::MfccEngine engine_;
  std::unique_ptr<simplevox::MfccFeature> reference_;
  int16_t* raw_ = nullptr;
  size_t used_ = 0;
  SpeechGate gate_;
};
