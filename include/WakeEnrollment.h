#pragma once
#include "UtteranceWindow.h"
#include <cstring>

// A bounded local call recording; its storage is owned and wiped by the caller.
class WakeEnrollmentCapture {
 public:
  static constexpr size_t frame = 320, capacity = 48000;
  enum class Result { Calibration, Waiting, Recording, Ready, Empty, Short, Long, Noisy, Clipped, Invalid };
  WakeEnrollmentCapture(int16_t* destination, size_t size) : data_(destination), size_(size) { ambient_.reset(); }
  Result push(const int16_t* samples, size_t n) {
    if (terminal()) return result_;
    if (!samples || n != frame || !data_ || size_ < capacity) return result_ = Result::Invalid;
    if (!ambient_.ready()) {
      ambient_.push(samples,n);
      if (ambient_.ready()) result_ = ambient_.usable() ? Result::Waiting : Result::Noisy;
      return result_;
    }
    int64_t mean=0; for(size_t i=0;i<n;++i) mean+=samples[i]; mean/=static_cast<int64_t>(n);
    uint64_t energy=0; size_t clips=0;
    for(size_t i=0;i<n;++i) { const int64_t v=samples[i]-mean; energy+=v*v;
      const int32_t magnitude=samples[i]<0 ? -int32_t(samples[i]) : samples[i]; if(magnitude>=32760) ++clips; }
    const bool voice=energy/n >= uint64_t(ambient_.threshold())*ambient_.threshold();
    waiting_+=n;
    if (!active_ && !voice) {
      if (used_>=1600) { memmove(data_,data_+n,(used_-n)*sizeof(int16_t)); used_-=n; }
      memcpy(data_+used_,samples,n*sizeof(int16_t)); used_+=n;
      if(waiting_>=80000) result_=Result::Empty;
      return result_;
    }
    active_=true;
    if (used_+n>capacity) return result_=Result::Long;
    memcpy(data_+used_,samples,n*sizeof(int16_t)); used_+=n; clipped_+=clips;
    if(voice) { voiced_+=n; quiet_=0; } else quiet_+=n;
    result_=Result::Recording;
    if(quiet_>=6400) {
      count_=used_>4800 ? used_-4800 : used_;
      result_=voiced_<6400 ? Result::Short : clipped_>count_/1000 ? Result::Clipped : Result::Ready;
    } else if(used_>=capacity) result_=Result::Long;
    return result_;
  }
  bool terminal() const { return result_!=Result::Calibration && result_!=Result::Waiting && result_!=Result::Recording; }
  Result result() const { return result_; }
  size_t samples() const { return result_==Result::Ready ? count_ : 0; }
  uint32_t noiseRms() const { return ambient_.noise(); }
  uint32_t voiceThreshold() const { return ambient_.threshold(); }
 private:
  int16_t* data_; size_t size_,used_=0,count_=0,voiced_=0,quiet_=0,waiting_=0,clipped_=0;
  AmbientGate ambient_; bool active_=false;
  Result result_=Result::Calibration;
};
