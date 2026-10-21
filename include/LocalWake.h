#pragma once
#include <Arduino.h>
#include <memory>
#include <array>
#include <simplevox.h>
#include "ConversationRules.h"
#include "Config.h"
#include "WakeEnrollment.h"

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
  void resetDetection();
  bool detect(const int16_t* samples);
  const char* detectionStatus() const { return detectionStatus_; }
  uint32_t inputRms() const { return inputRms_; }
  uint32_t detectionThreshold() const { return detectionThreshold_; }
  uint32_t lastDistance() const { return lastDistance_; }
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
  std::unique_ptr<WakeEnrollmentCapture> detection_;
  uint32_t inputRms_=0, detectionThreshold_=0, lastDistance_=UINT32_MAX;
  const char* detectionStatus_="Wake preparing";
  unsigned resultFrames_=0;
};
