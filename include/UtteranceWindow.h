#pragma once
#include "ConversationRules.h"
#include <algorithm>
#include <array>
#include <cmath>

class AmbientGate {
 public:
  static constexpr unsigned warmupFrames = 20, noiseFrames = 30;
  void reset() { count_ = 0; levels_.fill(0); }
  bool push(const int16_t* samples, size_t n) {
    if (!samples || n != 320 || ready()) return false;
    if (count_ >= warmupFrames) {
      int64_t mean = 0; for (size_t i = 0; i < n; ++i) mean += samples[i]; mean /= static_cast<int64_t>(n);
      uint64_t sum = 0; for (size_t i = 0; i < n; ++i) { const int64_t v = samples[i] - mean; sum += v * v; }
      levels_[count_ - warmupFrames] = static_cast<uint32_t>(std::sqrt(static_cast<double>(sum / n)));
    }
    ++count_; if (ready()) std::sort(levels_.begin(), levels_.end()); return true;
  }
  bool ready() const { return count_ == warmupFrames + noiseFrames; }
  uint32_t noise() const {
    if (!ready()) return 0;
    return levels_[26]; // 90th percentile.
  }
  uint32_t threshold() const { return std::max(uint32_t(750), noise() * 3); }
  bool usable() const { return ready() && threshold() <= 3000; }
 private:
  unsigned count_ = 0;
  std::array<uint32_t, noiseFrames> levels_{};
};

// Owns decisions only; audio stays in the caller's bounded linear capture buffer.
class UtteranceWindow {
 public:
  static constexpr uint32_t rate = 16000, frame = 320, preRoll = 3200;
  static constexpr uint32_t maxInput = 80000, capacity = 176000;
  enum class Result { Waiting, Recording, Complete, Empty, Limit };
  void reset(uint32_t threshold = 500) {
    gate_.reset(rate, 700, 200, 4800, 5000, threshold);
    processed_ = start_ = speechStart_ = 0; active_ = false; result_ = Result::Waiting;
  }
  Result push(const int16_t* samples, size_t n) {
    if (terminal()) return result_;
    if (!samples || n != frame || processed_ + n > capacity) return result_ = Result::Empty;
    const auto decision = gate_.push(samples, n);
    if (!active_ && decision == SpeechGate::Result::Recording) {
      start_ = processed_ > preRoll ? processed_ - preRoll : 0; speechStart_ = processed_; active_ = true;
    }
    processed_ += frame;
    if (decision == SpeechGate::Result::Empty) return result_ = Result::Empty;
    if (decision == SpeechGate::Result::Waiting) { active_ = false; return result_ = Result::Waiting; }
    // The pre-roll is included in the API's five-second bound.
    if (active_ && (processed_ - start_ >= maxInput || processed_ - speechStart_ >= 76800)) return result_ = Result::Limit;
    if (decision == SpeechGate::Result::Complete) return result_ = Result::Complete;
    return result_ = Result::Recording;
  }
  bool terminal() const { return result_ == Result::Complete || result_ == Result::Empty || result_ == Result::Limit; }
  uint32_t start() const { return start_; }
  uint32_t samples() const { return active_ && result_ != Result::Empty ? processed_ - start_ : 0; }
  uint32_t processed() const { return processed_; }
  bool limited() const { return result_ == Result::Limit; }
 private:
  SpeechGate gate_;
  uint32_t processed_ = 0, start_ = 0, speechStart_ = 0;
  bool active_ = false;
  Result result_ = Result::Waiting;
};
