#pragma once
#include "Arduino.h"
#include <algorithm>
inline bool failMicBegin=false,failMicRecord=false;
inline unsigned micBegins=0,micRequests=0;
struct DetectionMic {
  struct Config { uint32_t sample_rate=16000; unsigned magnification=4; } cfg;
  Config config() const { return cfg; }
  void config(Config c) { cfg=c; }
  bool running=false;
  int16_t* pending=nullptr;
  size_t count=0;
  void* args=nullptr;
  void (*callback)(void*,void*,size_t)=nullptr;
  bool begin() { ++micBegins; return running=!failMicBegin; }
  void end() { running=false;pending=nullptr; }
  void setBufferReleaseCallback(void* a,void (*cb)(void*,void*,size_t)) { args=a;callback=cb; }
  bool record(int16_t* out,size_t n,uint32_t,bool=false) {
    ++micRequests;if(failMicRecord || !running || pending) return false;
    pending=out;count=n;return true;
  }
  unsigned isRecording() const { return pending?1:0; }
  void finish(const int16_t* data) {
    if(!pending) return;
    auto* released=pending;pending=nullptr;std::copy(data,data+count,released);
    if(callback) callback(args,released,count);
  }
};
struct DetectionSpeaker {
  bool begin() { return true; } void stop() {} void end() {}
  bool isPlaying() const { return false; }
  bool playRaw(const int16_t*,size_t,unsigned,bool,int,int) { return true; }
  bool tone(int,int) { return true; }
};
inline struct DetectionM5 { DetectionMic Mic; DetectionSpeaker Speaker; } M5;
