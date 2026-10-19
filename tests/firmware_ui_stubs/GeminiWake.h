#pragma once
inline bool fakeWakeAvailable = false, fakeWakeMatch = false;
inline unsigned fakeWakePolls = 0, fakeEnrollCommits = 0;
inline bool fakeEnrollAllowed=true, fakeEnrollMatch=true, fakeEnrollSave=true;
inline bool fakeEnrollPending=false, fakeEnrollVerified=false;
class GeminiWake {
 public:
  void begin() {}
  bool available() const { return fakeWakeAvailable; }
  bool canEnroll() const { return fakeEnrollAllowed; }
  bool prepare(const int16_t*,size_t count) { fakeEnrollPending=count>=6400; return fakeEnrollPending; }
  bool verify(const int16_t*,size_t count) { fakeEnrollVerified=fakeEnrollPending && fakeEnrollMatch && count>=6400; return fakeEnrollVerified; }
  bool commit() { if(!fakeEnrollPending || !fakeEnrollVerified || !fakeEnrollSave) return false;
    ++fakeEnrollCommits; fakeWakeAvailable=true; fakeEnrollPending=false; return true; }
  void cancel() { fakeEnrollPending=fakeEnrollVerified=false; }
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
