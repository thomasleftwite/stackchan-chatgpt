#pragma once
#include <Arduino.h>
#include <memory>
#include <array>
#include <simplevox.h>
#include "ConversationRules.h"
#include "Config.h"

class LocalWake {
 public:
  bool begin(const Config& config, bool recoverBackup = true);
  bool registered() const { return bool(reference_); }
  void reset();
  bool prepareCandidate(const int16_t* samples, size_t count);
  bool collectCandidate(const int16_t* samples, size_t count);
  bool validateCandidates();
  bool commitCandidate();
  void cancelCandidate();
  // 320 samples, 16 kHz. All buffers remain local.
  bool push(const int16_t* samples, bool registration, String& message);
 private:
  bool validFile(const String& path);
  bool save(const simplevox::MfccFeature& feature);
  bool sameFile(const String& path, const simplevox::MfccFeature& feature);
  std::unique_ptr<simplevox::MfccFeature> createCandidate(const int16_t* samples, size_t count);
  const Config* config_ = nullptr;
  simplevox::MfccEngine engine_;
  std::unique_ptr<simplevox::MfccFeature> reference_;
  std::unique_ptr<simplevox::MfccFeature> pending_;
  std::array<std::unique_ptr<simplevox::MfccFeature>,2> repeats_;
  unsigned candidateCount_ = 0;
  bool verified_ = false;
  int16_t* raw_ = nullptr;
  size_t used_ = 0;
  SpeechGate gate_;
};
