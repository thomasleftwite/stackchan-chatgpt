#pragma once
#include <SPIFFS.h>
#include "LocalWake.h"
#include "AudioDevice.h"
#include <M5Unified.h>
#include <atomic>

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
    // end() joins the producer before its callback or ring storage is changed.
    capturing_.store(false); audio.stop();
    M5.Mic.setBufferReleaseCallback(nullptr,nullptr);
    running_ = false; wake_.resetDetection();
    memset(frames_, 0, sizeof(frames_));
  }
  bool poll(AudioDevice& audio) {
    if (!available_) return false;
    if(retrying_ && static_cast<int32_t>(millis()-retryAt_)<0) return false;
    retrying_=false;
    if (!running_) {
      wake_.resetDetection();
      if (!audio.listen(16000)) { retry(audio,"Mic start retry"); return false; }
      produced_.store(0); consumed_.store(0); captureFault_.store(false); capturing_.store(true);
      M5.Mic.setBufferReleaseCallback(this,released);
      running_ = true;
      lastFrameAt_=millis();
      if(!M5.Mic.record(frames_[0],320,16000,false)) { retry(audio,"Mic read retry"); return false; }
    }
    if(captureFault_.load()) { retry(audio,"Wake buffer retry"); return false; }
    const auto next=consumed_.load(std::memory_order_relaxed);
    if(next==produced_.load(std::memory_order_acquire)) {
      if(millis()-lastFrameAt_>1000) retry(audio,"Mic stalled retry");
      return false;
    }
    lastFrameAt_=millis();
    int16_t frame[320]; memcpy(frame,frames_[next%slots],sizeof(frame));
    memset(frames_[next%slots],0,sizeof(frame)); consumed_.store(next+1,std::memory_order_release);
    const bool matched = wake_.detect(frame);
    memset(frame,0,sizeof(frame));
    if (matched) { stop(audio); lastStatus_="Call matched"; }
    return matched;
  }
  void diagnostics(char* state,size_t stateSize,char* metrics,size_t metricsSize) const {
    const char* status=!available_ ? "Call unavailable" : retrying_ ? lastStatus_ :
        running_ ? wake_.detectionStatus() : "Wake paused";
    snprintf(state,stateSize,"%s",status);
    const auto distance=wake_.lastDistance();
    if(distance==UINT32_MAX) snprintf(metrics,metricsSize,"V%lu T%lu D--",static_cast<unsigned long>(wake_.inputRms()),static_cast<unsigned long>(wake_.detectionThreshold()));
    else if(distance>999) snprintf(metrics,metricsSize,"V%lu T%lu D>999",static_cast<unsigned long>(wake_.inputRms()),static_cast<unsigned long>(wake_.detectionThreshold()));
    else snprintf(metrics,metricsSize,"V%lu T%lu D%lu",static_cast<unsigned long>(wake_.inputRms()),static_cast<unsigned long>(wake_.detectionThreshold()),static_cast<unsigned long>(distance));
  }
 private:
  static constexpr uint32_t slots=32;
  static void released(void* args,void* data,size_t length) {
    auto* self=static_cast<GeminiWake*>(args);
    const auto count=self->produced_.load(std::memory_order_relaxed);
    if(length!=320 || data!=self->frames_[count%slots]) { self->captureFault_.store(true); return; }
    const auto next=count+1;
    self->produced_.store(next,std::memory_order_release);
    if(!self->capturing_.load()) return;
    if(next-self->consumed_.load(std::memory_order_acquire)>=slots) { self->captureFault_.store(true); return; }
    if(!M5.Mic.record(self->frames_[next%slots],320,16000,false) && self->capturing_.load()) self->captureFault_.store(true);
  }
  void retry(AudioDevice& audio,const char* reason) {
    stop(audio); lastStatus_=reason; retrying_=true; retryAt_=millis()+2000;
  }
  Config config_;
  LocalWake wake_;
  int16_t frames_[slots][320]{};
  std::atomic<uint32_t> produced_{0},consumed_{0};
  std::atomic<bool> capturing_{false},captureFault_{false};
  const char* lastStatus_="Wake preparing";
  uint32_t retryAt_=0,lastFrameAt_=0;
  bool retrying_=false;
  bool available_ = false, running_ = false, initialized_ = false;
};
