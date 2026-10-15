#include "AudioDevice.h"
#include <M5Unified.h>

bool AudioDevice::listen(uint32_t rate) {
  stop();
  auto c = M5.Mic.config(); c.sample_rate = rate;
  // Atomic Voice Base needs more input gain for the configured speech threshold.
  c.magnification = 4;
  M5.Mic.config(c); rate_ = rate;
  if (!M5.Mic.begin()) { rate_ = 0; return false; }
  return true;
}
bool AudioDevice::read(int16_t* dest, size_t n) {
  if (!rate_ || !M5.Mic.record(dest, n, rate_)) return false;
  const uint32_t start = millis();
  // record() queues DMA work; do not consume or overwrite the destination early.
  while (M5.Mic.isRecording()) {
    if (millis() - start > 1000) { M5.Mic.end(); rate_ = 0; return false; }
    delay(1);
  }
  return true;
}
bool AudioDevice::output() { stop(); return M5.Speaker.begin(); }
bool AudioDevice::play(const int16_t* data, size_t samples) {
  return M5.Speaker.playRaw(data, samples, 24000, false, 1, 0);
}
bool AudioDevice::playing() const { return M5.Speaker.isPlaying(); }
void AudioDevice::stop() { M5.Mic.end(); M5.Speaker.stop(); M5.Speaker.end(); rate_ = 0; }
bool AudioDevice::cue() {
  if (!output()) return false;
  if (!M5.Speaker.tone(1200, 500)) { stop(); return false; }
  const auto start = millis();
  while (playing() && millis() - start < 1000) delay(1);
  const bool complete = !playing();
  stop(); return complete;
}
