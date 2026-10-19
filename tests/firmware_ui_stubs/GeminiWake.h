#pragma once
inline bool fakeWakeAvailable = false, fakeWakeMatch = false;
inline unsigned fakeWakePolls = 0;
class GeminiWake {
 public:
  void begin() {}
  bool available() const { return fakeWakeAvailable; }
  void stop(AudioDevice& audio) { if (running_) audio.stop(); running_ = false; }
  bool poll(AudioDevice& audio) {
    if (!fakeWakeAvailable) return false;
    ++fakeWakePolls;
    if (!running_) running_ = audio.listen(16000);
    if (fakeWakeMatch) { fakeWakeMatch = false; stop(audio); return true; }
    return false;
  }
 private:
  bool running_ = false;
};
