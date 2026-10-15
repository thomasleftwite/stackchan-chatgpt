#pragma once
#include <cstddef>
#include <cstdint>

// Sample-count based decisions remain correct when network polling is delayed.
class SpeechGate {
 public:
  void reset(uint32_t rate, uint32_t silenceMs, uint32_t minMs,
             uint32_t maxMs, uint32_t idleMs, uint32_t threshold) {
    rate_ = rate; silence_ = silenceMs; min_ = minMs; max_ = maxMs;
    idle_ = idleMs; threshold_ = threshold;
    elapsed_ = utterance_ = voiced_ = quiet_ = 0; started_ = false;
  }
  enum class Result { Waiting, Recording, Complete, Empty };
  Result push(const int16_t* data, size_t n) {
    uint64_t sum = 0;
    int64_t mean = 0;
    for (size_t i = 0; i < n; ++i) mean += data[i];
    if (n == 0) return Result::Waiting;
    mean /= static_cast<int64_t>(n);
    for (size_t i = 0; i < n; ++i) {
      const int64_t x = data[i] - mean;
      sum += x * x;
    }
    const bool voice = sum / n >= uint64_t(threshold_) * threshold_;
    elapsed_ += n;
    if (voice || started_) utterance_ += n;
    if (voice) { voiced_ += n; quiet_ = 0; started_ = true; }
    else if (started_) quiet_ += n;
    if (started_ && utterance_ >= samples(max_))
      return voiced_ >= samples(min_) ? Result::Complete : Result::Empty;
    if (started_ && quiet_ >= samples(silence_)) {
      if (voiced_ >= samples(min_)) return Result::Complete;
      started_ = false; utterance_ = voiced_ = quiet_ = 0;
    }
    if (!started_ && elapsed_ >= samples(idle_)) return Result::Empty;
    return started_ ? Result::Recording : Result::Waiting;
  }
 private:
  uint64_t samples(uint32_t ms) const { return uint64_t(rate_) * ms / 1000; }
  uint32_t rate_ = 16000, silence_ = 700, min_ = 200, max_ = 15000;
  uint32_t idle_ = 15000, threshold_ = 500;
  uint64_t elapsed_ = 0, utterance_ = 0, voiced_ = 0, quiet_ = 0;
  bool started_ = false;
};

inline bool validTemplateHeader(uint8_t tag, int32_t frames, int32_t dimensions,
                                size_t bytes) {
  // SimpleVox VERSION1, 16 kHz / 512 samples / 256 hop / 12 MFCCs.
  return tag == 1 && frames >= 2 && frames <= 186 && dimensions == 12 &&
         bytes == 9 + size_t(frames) * size_t(dimensions) * 2;
}
