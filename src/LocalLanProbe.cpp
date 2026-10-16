#ifdef STACKCHAN_LAN_PROBE
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <math.h>
#include "AudioDevice.h"
#include "ProbeResample.h"

namespace {
constexpr size_t frames = 150, inputCount = frames * 320, outputCount = frames * 480;
AudioDevice audio;
WebSocketsClient ws;
int16_t *input = nullptr, *output = nullptr;
char host[16] = {}, token[65] = {}, auth[96] = {}, command[160] = {};
unsigned port = 0;
size_t commandLength = 0, sent = 0, received = 0;
bool overflow = false, configured = false, consumed = true;
bool ready = false, failed = false, stopped = false;
uint32_t generation = 0, pressedAt = 0;
unsigned fault = 0;
char lastResult[160] = "LAN RESULT NONE";
char lastCapture[160] = "LAN CAPTURE NONE";

void display(const char* text) {
  M5.Display.fillScreen(TFT_BLACK); M5.Display.setCursor(2, 2); M5.Display.print(text);
}
void idle() { display(configured ? "LAN PCM probe\nShort: tone test\nHold: record/play\nWait for Release" : "LAN PCM probe\nRun PC LAN tool\nUSB config only"); }
void wipe() {
  audio.stop();
  if (input) memset(input, 0, inputCount * 2);
  if (output) memset(output, 0, outputCount * 2);
}
void put32(uint8_t* p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = v >> (8 * i); }
uint32_t get32(const uint8_t* p) {
  uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(p[i]) << (8 * i); return v;
}
void event(WStype_t type, uint8_t* data, size_t length) {
  if (type == WStype_DISCONNECTED || type == WStype_ERROR) {
    if (!stopped) { failed = true; if (!fault) fault = type == WStype_ERROR ? 2 : 1; }
    return;
  }
  if (type == WStype_TEXT) {
    JsonDocument doc;
    if (length > 1024 || deserializeJson(doc, data, length) || ready ||
        doc["type"].as<String>() != "ready" || doc["protocolVersion"].as<unsigned>() != 1 || doc["mode"].as<String>() != "pcm-loopback" ||
        doc["inputRate"].as<unsigned>() != 16000 || doc["outputRate"].as<unsigned>() != 24000 || doc["maxFrames"].as<unsigned>() != frames) {
      failed = true; fault = 3; return;
    }
    ready = true;
  }
  if (type == WStype_BIN) {
    if (length != 972) { failed = true; fault = 4; return; }
    if (!ready || received >= frames || sent != received + 1) { failed = true; fault = 5; return; }
    if (get32(data) != generation || get32(data + 4) != received ||
        data[8] != 0xe0 || data[9] != 0x01 || data[10] != 0 || data[11] != 0) {
      failed = true; fault = 6; return;
    }
    if (memcmp(output + received * 480, data + 12, 960) != 0) { failed = true; fault = 7; return; }
    // Play only data actually received over the network after comparison.
    memcpy(output + received * 480, data + 12, 960);
    ++received;
  }
}
bool transport() {
  ready = failed = stopped = false; sent = received = 0;
  fault = 0;
  if (++generation == 0) ++generation;
  ws.onEvent(event);
  ws.begin(host, port, "/live", "");
  // WebSocketsClient appends its own CRLF; another one ends HTTP headers early.
  snprintf(auth, sizeof(auth), "Authorization: Bearer %s", token);
  ws.setExtraHeaders(auth);
  const auto started = millis();
  uint32_t nextSend = started;
  while (!failed && millis() - started < 20000 && received < frames) {
    ws.loop();
    if (ready && sent == received && sent < frames && static_cast<int32_t>(millis() - nextSend) >= 0) {
      uint8_t packet[652];
      put32(packet, generation); put32(packet + 4, sent);
      packet[8] = 0x40; packet[9] = 0x01; packet[10] = packet[11] = 0;
      memcpy(packet + 12, input + sent * 320, 640);
      ++sent;
      if (!ws.sendBIN(packet, sizeof(packet))) failed = true;
      nextSend = millis() + 20;
    }
    delay(1);
  }
  const bool ok = !failed && received == frames;
  stopped = true;
  if (ok) ws.sendTXT("{\"type\":\"stop\"}");
  ws.disconnect();
  snprintf(lastResult, sizeof(lastResult), "LAN RESULT %s gen=%lu frames=%u input=%u output=%u ms=%lu",
    ok ? "PASS" : "FAIL", static_cast<unsigned long>(generation), static_cast<unsigned>(received),
    static_cast<unsigned>(sent * 320), static_cast<unsigned>(received * 480),
    static_cast<unsigned long>(millis() - started));
  Serial.println(lastResult);
  if (!ok) Serial.printf("LAN FAULT code=%u\n", fault);
  return ok;
}
void run(bool microphone) {
  if (!configured || WiFi.status() != WL_CONNECTED) { Serial.println("LAN WIFI FAIL"); return; }
  wipe();
  if (microphone) {
    display("Speak after beep\n3 sec recording");
    if (!audio.cue() || !audio.listen(16000)) { wipe(); Serial.println("LAN CAPTURE FAIL"); return; }
    display("Recording 16k\nSpeak now (3s)");
    const auto started = millis();
    // One queued DMA destination avoids discarding the tail of each DMA block
    // between short capture requests. Networking starts only after capture.
    bool captured = M5.Mic.record(input, inputCount, 16000, false);
    while (captured && M5.Mic.isRecording() && millis() - started < 4500) delay(1);
    captured = captured && !M5.Mic.isRecording();
    audio.stop();
    if (!captured) { wipe(); Serial.println("LAN CAPTURE FAIL"); idle(); return; }
    uint64_t energy = 0; uint32_t peak = 0, clipped = 0;
    for (size_t i = 0; i < inputCount; ++i) {
      const int32_t v = input[i]; const uint32_t magnitude = v < 0 ? -v : v;
      energy += static_cast<int64_t>(v) * v; peak = max(peak, magnitude);
      if (magnitude >= 32760) ++clipped;
    }
    snprintf(lastCapture, sizeof(lastCapture), "LAN CAPTURE DONE samples=48000 ms=%lu rms=%lu peak=%lu clipped=%lu",
      static_cast<unsigned long>(millis() - started),
      static_cast<unsigned long>(sqrt(static_cast<double>(energy) / inputCount)),
      static_cast<unsigned long>(peak), static_cast<unsigned long>(clipped));
    Serial.println(lastCapture);
  } else {
    for (size_t i = 0; i < inputCount; ++i)
      input[i] = static_cast<int16_t>(2000 * sinf(2 * PI * 1000 * i / 16000));
  }
  // Independent expected result; frame boundaries match this diagnostic server.
  for (size_t i = 0; i < frames; ++i) probeResample16to24(input + i * 320, 320, output + i * 480);
  display("LAN 16k -> PC\nPC 24k -> device");
  if (transport()) {
    display("Playing 24k\nReceived from PC");
    bool ok = audio.output() && audio.play(output, outputCount);
    const auto started = millis();
    while (ok && audio.playing() && millis() - started < 4500) delay(1);
    ok = ok && !audio.playing();
    Serial.println(ok ? "LAN PLAYBACK PASS samples=72000 rate=24000" : "LAN PLAYBACK FAIL");
  }
  wipe(); M5.update(); consumed = true; idle();
}
void configure() {
  char candidateHost[16], candidateToken[65], extra;
  unsigned candidatePort;
  if (sscanf(command, "LAN CONFIG %15s %u %64s %c", candidateHost, &candidatePort, candidateToken, &extra) != 3 ||
      candidatePort == 0 || candidatePort > 65535 || strlen(candidateToken) != 64) return;
  IPAddress ip;
  if (!ip.fromString(candidateHost) || !(ip[0] == 10 || (ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31) ||
      (ip[0] == 192 && ip[1] == 168))) return;
  for (size_t i = 0; i < 64; ++i) if (!isxdigit(static_cast<unsigned char>(candidateToken[i]))) return;
  strlcpy(host, candidateHost, sizeof(host)); strlcpy(token, candidateToken, sizeof(token)); port = candidatePort;
  memset(candidateToken, 0, sizeof(candidateToken));
  Serial.println("LAN CONFIG OK");
  display("Connecting WiFi\nSaved settings");
  WiFi.persistent(false); WiFi.mode(WIFI_STA); WiFi.begin();
  const auto started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 15000) delay(20);
  configured = WiFi.status() == WL_CONNECTED;
  Serial.println(configured ? "LAN WIFI CONNECTED" : "LAN WIFI FAIL"); idle();
}
void dispatch() {
  if (strcmp(command, "LAN INFO") == 0) {
    Serial.println("LAN READY v=1 input=16000 output=24000 mono=1 bits=16");
    Serial.println(lastCapture); Serial.println(lastResult);
  } else if (strncmp(command, "LAN CONFIG ", 11) == 0) configure();
  else if (strcmp(command, "LAN SYNTH") == 0) run(false);
  // Microphone capture is only initiated by the physical button.
}
}
void setup() {
  auto cfg = M5.config(); cfg.internal_spk = false; cfg.internal_mic = false;
  cfg.external_speaker.atomic_echo = true; cfg.serial_baudrate = 115200;
  M5.begin(cfg); M5.Display.setTextSize(1); M5.Speaker.setVolume(100);
  input = static_cast<int16_t*>(heap_caps_calloc(inputCount, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  output = static_cast<int16_t*>(heap_caps_calloc(outputCount, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!input || !output) { display("PSRAM failed"); Serial.println("LAN MEMORY FAIL"); return; }
  idle(); Serial.println("LAN READY v=1 input=16000 output=24000 mono=1 bits=16");
}
void loop() {
  M5.update();
  if (!input || !output) { delay(20); return; }
  if (M5.BtnA.wasPressed()) { pressedAt = millis(); consumed = false; }
  if (!consumed && M5.BtnA.isPressed() && millis() - pressedAt >= 2000) {
    consumed = true; display("Release to record");
    while (M5.BtnA.isPressed()) { M5.update(); delay(10); }
    run(true);
  }
  if (M5.BtnA.wasReleased() && !consumed) { consumed = true; run(false); }
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\n') {
      command[commandLength] = 0; if (!overflow) dispatch();
      memset(command, 0, sizeof(command)); commandLength = 0; overflow = false;
    } else if (c != '\r') {
      if (commandLength < sizeof(command) - 1) command[commandLength++] = c;
      else overflow = true;
    }
  }
  delay(2);
}
#endif
