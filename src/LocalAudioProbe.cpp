#ifdef STACKCHAN_AUDIO_PROBE
#include <Arduino.h>
#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <math.h>
#include "AudioDevice.h"
#include "ProbeResample.h"

namespace {
constexpr size_t inputSamples = 16000 * 3, outputSamples = 24000 * 3;
AudioDevice audio;
int16_t *input = nullptr, *output = nullptr;
bool consumed = true;
char command[24] = {};
size_t commandLength = 0;
bool commandOverflow = false;

void display(const char* text) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(2, 2);
  M5.Display.print(text);
}
void idle() { display("Audio probe\nShort: 24k tone\nHold: record/play\nUSB local only"); }
void wipe() {
  audio.stop();
  if (input) memset(input, 0, inputSamples * sizeof(int16_t));
  if (output) memset(output, 0, outputSamples * sizeof(int16_t));
}
bool drain(uint32_t timeoutMs) {
  const auto start = millis();
  while (audio.playing() && millis() - start < timeoutMs) delay(1);
  return !audio.playing();
}
void tone() {
  if (!output) return;
  display("24k PCM tone\n1 kHz / 1 sec");
  for (size_t i = 0; i < 24000; ++i) {
    const float envelope = min(1.0f, min(i / 240.0f, (23999 - i) / 240.0f));
    output[i] = static_cast<int16_t>(2000 * envelope * sinf(2 * PI * 1000 * i / 24000));
  }
  const bool ok = audio.output() && audio.play(output, 24000) && drain(2500);
  wipe(); Serial.println(ok ? "PROBE TONE PASS rate=24000 samples=24000" : "PROBE TONE FAIL");
  idle();
}
void recordPlay() {
  if (!input || !output) return;
  display("Speak after beep\n3 sec recording");
  if (!audio.cue() || !audio.listen(16000)) {
    wipe(); Serial.println("PROBE CAPTURE FAIL"); idle(); return;
  }
  display("Recording 16k\nSpeak now (3s)");
  const auto start = millis();
  bool ok = true;
  for (size_t offset = 0; offset < inputSamples; offset += 320) {
    if (!audio.read(input + offset, 320)) { ok = false; break; }
  }
  const uint32_t captureMs = millis() - start;
  audio.stop();
  if (!ok) { wipe(); Serial.println("PROBE CAPTURE FAIL"); idle(); return; }
  uint64_t energy = 0;
  uint32_t peak = 0, clipped = 0;
  for (size_t i = 0; i < inputSamples; ++i) {
    const int32_t sample = input[i];
    const uint32_t magnitude = sample < 0 ? -sample : sample;
    energy += static_cast<int64_t>(sample) * sample;
    peak = max(peak, magnitude);
    if (magnitude >= 32760) ++clipped;
  }
  probeResample16to24(input, inputSamples, output);
  Serial.printf("PROBE CAPTURE DONE rate=16000 samples=48000 ms=%lu rms=%lu peak=%lu clipped=%lu\n",
    static_cast<unsigned long>(captureMs),
    static_cast<unsigned long>(sqrt(static_cast<double>(energy) / inputSamples)),
    static_cast<unsigned long>(peak), static_cast<unsigned long>(clipped));
  display("Playing 24k\n3 sec / mono");
  ok = audio.output() && audio.play(output, outputSamples) && drain(4500);
  wipe(); Serial.println(ok ? "PROBE PLAYBACK PASS rate=24000 samples=72000" : "PROBE PLAYBACK FAIL");
  M5.update(); consumed = true; idle();
}
void dispatch() {
  if (strcmp(command, "PROBE INFO") == 0) Serial.println("PROBE READY v=1 input=16000 output=24000 mono=1 bits=16");
  else if (strcmp(command, "PROBE TONE") == 0) tone();
  // Microphone capture requires a physical long press; no remote capture command.
}
}

void setup() {
  auto cfg = M5.config();
  cfg.internal_spk = false; cfg.internal_mic = false;
  cfg.external_speaker.atomic_echo = true; cfg.serial_baudrate = 115200;
  M5.begin(cfg); M5.Display.setTextSize(1); M5.Speaker.setVolume(100);
  input = static_cast<int16_t*>(heap_caps_calloc(inputSamples, sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  output = static_cast<int16_t*>(heap_caps_calloc(outputSamples, sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!input || !output) { display("PSRAM allocation\nfailed"); Serial.println("PROBE MEMORY FAIL"); return; }
  idle(); Serial.println("PROBE READY v=1 input=16000 output=24000 mono=1 bits=16");
}
void loop() {
  M5.update();
  if (!input || !output) { delay(20); return; }
  if (M5.BtnA.isReleased() && !M5.BtnA.wasReleased()) consumed = false;
  if (!consumed && M5.BtnA.pressedFor(2000)) {
    consumed = true;
    display("Release to record");
    while (M5.BtnA.isPressed()) { M5.update(); delay(10); }
    recordPlay();
  }
  if (M5.BtnA.wasReleased() && !consumed) { consumed = true; tone(); }
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\n') {
      command[commandLength] = 0;
      if (!commandOverflow) dispatch();
      commandLength = 0; commandOverflow = false;
    } else if (c != '\r') {
      if (commandLength < sizeof(command) - 1) command[commandLength++] = c;
      else commandOverflow = true;
    }
  }
  delay(2);
}
#endif
