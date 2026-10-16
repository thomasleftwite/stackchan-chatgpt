#ifdef STACKCHAN_GEMINI_DEVICE
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <math.h>
#include "AudioDevice.h"
#include "LivePcm.h"

namespace {
constexpr size_t inputCount = 48000, maxOutput = 240000;
AudioDevice audio;
WebSocketsClient ws;
int16_t *input = nullptr, *output = nullptr;
char host[16]{}, token[65]{}, auth[96]{}, command[160]{};
unsigned port = 0, fault = 0;
size_t commandLength = 0, sent = 0, acked = 0;
bool overflow = false, configured = false, consumed = true, failed = false, stopped = true;
bool ready = false, inputEnded = false, thinking = false, outputStarted = false;
bool credit = false, outputEnded = false, played = false, complete = false, closed = false;
uint32_t generation = 0, pressedAt = 0;
livepcm::Output reply;
char lastResult[160] = "GEM RESULT NONE", lastCapture[160] = "GEM CAPTURE NONE";
void display(const char* text) { M5.Display.fillScreen(TFT_BLACK); M5.Display.setCursor(2, 2); M5.Display.print(text); }
void idle() { display(configured ? "Gemini PC ready\nShort: local beep\nHold: ask (3 sec)\nWait for Release" : "Gemini PC v2\nRun PC device tool\nUSB config only"); }
void wipe() { audio.stop(); if (input) memset(input, 0, inputCount * 2); if (output) memset(output, 0, maxOutput * 2); }
void bad(unsigned code) { failed = true; if (!fault) fault = code; }
bool control(const char* type, uint32_t value = 0) {
  char json[128];
  snprintf(json, sizeof(json), "{\"type\":\"%s\",\"generation\":%lu,\"sequence\":%lu,\"samples\":%lu}",
    type, static_cast<unsigned long>(generation), static_cast<unsigned long>(value), static_cast<unsigned long>(value));
  return ws.sendTXT(json);
}
void event(WStype_t type, uint8_t* data, size_t length) {
  if (type == WStype_DISCONNECTED) { closed = true; if (!stopped && !complete) bad(1); return; }
  if (type == WStype_ERROR) { if (!stopped) bad(2); return; }
  if (type == WStype_BIN) {
    const auto offset = reply.received;
    if (!credit || !outputStarted || outputEnded || !reply.accept(data, length)) { bad(4); return; }
    credit = false; memcpy(output + offset, data + 12, length - 12); return;
  }
  if (type != WStype_TEXT) {
    if (type == WStype_FRAGMENT_TEXT_START || type == WStype_FRAGMENT_BIN_START || type == WStype_FRAGMENT || type == WStype_FRAGMENT_FIN) bad(5);
    return;
  }
  JsonDocument doc;
  if (length > 1024 || deserializeJson(doc, data, length)) { bad(3); return; }
  const String kind = doc["type"].as<String>();
  if (kind == "ready" && !ready && doc["mode"].as<String>() == "gemini-live" &&
      doc["protocolVersion"].as<unsigned>() == 2 && doc["inputRate"].as<unsigned>() == 16000 &&
      doc["outputRate"].as<unsigned>() == 24000 && doc["maxInputSamples"].as<unsigned>() >= inputCount &&
      doc["maxOutputSamples"].as<unsigned>() <= maxOutput) { ready = true; return; }
  if (!ready || !doc["generation"].is<uint32_t>() || doc["generation"].as<uint32_t>() != generation) { bad(3); return; }
  if (kind == "input_ack" && !inputEnded && sent == acked + 1 && doc["sequence"].is<uint32_t>() && doc["sequence"].as<uint32_t>() == acked) { ++acked; return; }
  if (kind == "thinking" && inputEnded && !thinking) { thinking = true; display("Gemini thinking\nPlease wait"); return; }
  if (kind == "output_start" && thinking && !outputStarted && doc["sampleRate"].as<unsigned>() == 24000 &&
      doc["samples"].is<uint32_t>() && doc["frames"].is<uint32_t>() &&
      reply.begin(generation, doc["samples"].as<uint32_t>(), doc["frames"].as<uint32_t>())) { outputStarted = true; return; }
  if (kind == "output_end" && outputStarted && !outputEnded && !credit && reply.complete() &&
      doc["samples"].is<uint32_t>() && doc["samples"].as<uint32_t>() == reply.total) { outputEnded = true; return; }
  if (kind == "complete" && played && !complete) { complete = true; return; }
  bad(3);
}
bool transport() {
  ready = failed = inputEnded = thinking = outputStarted = credit = outputEnded = played = complete = closed = false;
  stopped = false; sent = acked = fault = 0; reply = {};
  if (++generation == 0) ++generation;
  ws.onEvent(event); ws.begin(host, port, "/live", ""); ws.setReconnectInterval(500);
  snprintf(auth, sizeof(auth), "Authorization: Bearer %s", token); ws.setExtraHeaders(auth);
  const auto started = millis(); uint32_t nextSend = started, playbackStarted = 0;
  bool playbackQueued = false;
  while (!failed && !closed && millis() - started < 85000) {
    ws.loop();
    if (failed || closed) break;
    if (ready && !inputEnded && sent == acked && sent < 150 && static_cast<int32_t>(millis() - nextSend) >= 0) {
      uint8_t packet[652]; livepcm::write32(packet, generation); livepcm::write32(packet + 4, sent);
      packet[8] = 0x40; packet[9] = 1; packet[10] = packet[11] = 0;
      memcpy(packet + 12, input + sent * 320, 640); ++sent;
      if (!ws.sendBIN(packet, sizeof(packet))) bad(6);
      nextSend = millis() + 20;
    }
    if (!inputEnded && acked == 150) { inputEnded = true; if (!control("input_end")) bad(6); memset(input, 0, inputCount * 2); }
    if (outputStarted && !outputEnded && !credit && !reply.complete()) { credit = true; if (!control("next", reply.sequence)) bad(6); }
    if (outputEnded && !playbackQueued) {
      display("Gemini reply\nPlaying 24kHz"); playbackQueued = true; playbackStarted = millis();
      if (!audio.output() || !audio.play(output, reply.total)) bad(7);
    }
    if (playbackQueued && !played && !failed) {
      if (millis() - playbackStarted > 12000) bad(8);
      else if (!audio.playing()) { audio.stop(); played = true; if (!control("played", reply.total)) bad(6); }
    }
    M5.update(); if (M5.BtnA.wasPressed()) bad(9);
    delay(1);
  }
  const bool ok = !failed && complete && closed && played;
  stopped = true; ws.disconnect();
  if (!ok && !fault) fault = 10;
  snprintf(lastResult, sizeof(lastResult), "GEM RESULT %s gen=%lu input=%u output=%lu frames=%lu ms=%lu played=%u",
    ok ? "PASS" : "FAIL", static_cast<unsigned long>(generation), static_cast<unsigned>(acked * 320),
    static_cast<unsigned long>(reply.received), static_cast<unsigned long>(reply.sequence),
    static_cast<unsigned long>(millis() - started), played ? 1 : 0);
  Serial.println(lastResult); if (!ok) Serial.printf("GEM FAULT code=%u\n", fault);
  return ok;
}
void run(bool microphone) {
  if (!configured || WiFi.status() != WL_CONNECTED) { Serial.println("GEM WIFI FAIL"); return; }
  wipe();
  if (microphone) {
    display("Speak after beep\nQuestion: 3 sec");
    if (!audio.cue() || !audio.listen(16000)) { wipe(); Serial.println("GEM CAPTURE FAIL"); idle(); return; }
    display("Recording 16kHz\nSpeak now (3 sec)"); const auto started = millis();
    bool captured = M5.Mic.record(input, inputCount, 16000, false);
    while (captured && M5.Mic.isRecording() && millis() - started < 4500) delay(1);
    captured = captured && !M5.Mic.isRecording(); audio.stop();
    if (!captured) { wipe(); Serial.println("GEM CAPTURE FAIL"); idle(); return; }
    uint64_t energy = 0; uint32_t peak = 0, clipped = 0;
    for (size_t i = 0; i < inputCount; ++i) { const int32_t v = input[i]; const uint32_t m = v < 0 ? -v : v;
      energy += static_cast<int64_t>(v) * v; peak = max(peak, m); if (m >= 32760) ++clipped; }
    snprintf(lastCapture, sizeof(lastCapture), "GEM CAPTURE DONE samples=48000 ms=%lu rms=%lu peak=%lu clipped=%lu",
      static_cast<unsigned long>(millis() - started), static_cast<unsigned long>(sqrt(static_cast<double>(energy) / inputCount)),
      static_cast<unsigned long>(peak), static_cast<unsigned long>(clipped)); Serial.println(lastCapture);
  } else for (size_t i = 0; i < inputCount; ++i) input[i] = static_cast<int16_t>(2000 * sinf(2 * PI * 1000 * i / 16000));
  display("Sending question\nPC -> Gemini"); const bool ok = transport();
  wipe(); display(ok ? "Reply complete" : "Connection failed\nCheck PC service"); delay(1500); M5.update(); consumed = true; idle();
}
void configure() {
  char candidateHost[16]{}, candidateToken[65]{}, extra; unsigned candidatePort;
  if (sscanf(command, "GEM CONFIG %15s %u %64s %c", candidateHost, &candidatePort, candidateToken, &extra) != 3 || !candidatePort || candidatePort > 65535 || strlen(candidateToken) != 64) return;
  IPAddress ip; if (!ip.fromString(candidateHost) || !(ip[0] == 10 || (ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31) || (ip[0] == 192 && ip[1] == 168))) return;
  for (size_t i = 0; i < 64; ++i) if (!isxdigit(static_cast<unsigned char>(candidateToken[i]))) return;
  strlcpy(host, candidateHost, sizeof(host)); strlcpy(token, candidateToken, sizeof(token)); port = candidatePort; memset(candidateToken, 0, sizeof(candidateToken));
  Serial.println("GEM CONFIG OK"); display("Connecting WiFi\nSaved settings"); WiFi.persistent(false); WiFi.mode(WIFI_STA); WiFi.begin();
  const auto started = millis(); while (WiFi.status() != WL_CONNECTED && millis() - started < 15000) delay(20);
  configured = WiFi.status() == WL_CONNECTED; Serial.println(configured ? "GEM WIFI CONNECTED" : "GEM WIFI FAIL"); idle();
}
void dispatch() {
  if (strcmp(command, "GEM INFO") == 0) { Serial.println("GEM READY v=2 input=16000 output=24000 mono=1 bits=16"); Serial.println(lastCapture); Serial.println(lastResult); }
  else if (strncmp(command, "GEM CONFIG ", 11) == 0) configure();
  else if (strcmp(command, "GEM PROBE") == 0) run(false);
}
}
void setup() {
  auto cfg = M5.config(); cfg.internal_spk = false; cfg.internal_mic = false; cfg.external_speaker.atomic_echo = true; cfg.serial_baudrate = 115200;
  M5.begin(cfg); M5.Display.setTextSize(1); M5.Speaker.setVolume(100);
  input = static_cast<int16_t*>(heap_caps_calloc(inputCount, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  output = static_cast<int16_t*>(heap_caps_calloc(maxOutput, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!input || !output) { display("PSRAM failed"); Serial.println("GEM MEMORY FAIL"); return; }
  idle(); Serial.println("GEM READY v=2 input=16000 output=24000 mono=1 bits=16");
}
void loop() {
  M5.update(); if (!input || !output) { delay(20); return; }
  if (M5.BtnA.wasPressed()) { pressedAt = millis(); consumed = false; }
  if (!consumed && M5.BtnA.isPressed() && millis() - pressedAt >= 2000) {
    consumed = true; display("Release to record"); while (M5.BtnA.isPressed()) { M5.update(); delay(10); } run(true);
  }
  if (M5.BtnA.wasReleased() && !consumed) { consumed = true; audio.cue(); idle(); }
  while (Serial.available()) { const char c = Serial.read();
    if (c == '\n') { command[commandLength] = 0; if (!overflow) dispatch(); memset(command, 0, sizeof(command)); commandLength = 0; overflow = false; }
    else if (c != '\r') { if (commandLength < sizeof(command) - 1) command[commandLength++] = c; else overflow = true; }
  }
  delay(2);
}
#endif
