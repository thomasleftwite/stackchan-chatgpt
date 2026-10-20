#ifdef STACKCHAN_GEMINI_DEVICE
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <atomic>
#include "AudioDevice.h"
#include "LivePcm.h"
#include "UtteranceWindow.h"
#include "FaceUI.h"
#include "GeminiWake.h"
#include "WakeEnrollment.h"
#ifdef STACKCHAN_WIRELESS
#include "WirelessLink.h"
#endif

namespace {
constexpr size_t maxInput = UtteranceWindow::maxInput, captureCapacity = UtteranceWindow::capacity, maxOutput = 240000;
size_t inputCount = 0;
AudioDevice audio;
FaceUI face;
GeminiWake wakeInput;
using deviceui::Mode;
WebSocketsClient ws;
int16_t *input = nullptr, *output = nullptr;
char host[16]{}, token[65]{}, auth[96]{}, command[160]{};
unsigned port = 0, fault = 0;
size_t commandLength = 0, sent = 0, acked = 0;
bool overflow = false, configured = false, consumed = true, failed = false, stopped = true;
bool ready = false, inputEnded = false, thinking = false, outputStarted = false;
bool credit = false, outputEnded = false, played = false, complete = false, closed = false;
uint32_t generation = 0, pressedAt = 0;
uint32_t pcSeen = 0;
bool pcOnline = false;
bool enrollmentActive = false;
#ifdef STACKCHAN_WIRELESS
WirelessLink wireless;
bool wirelessMode=true;
#endif
void pollCommands();
livepcm::Output reply;
char lastResult[160] = "GEM RESULT NONE", lastCapture[160] = "GEM CAPTURE NONE";
char lastEnrollmentCal[128] = "GEM ENROLL CAL NONE";
char lastEnrollmentResult[64] = "GEM ENROLL result=NONE";
char lastEnrollmentStep[48] = "GEM ENROLL STEP none";
char lastEnrollmentAudio[128] = "GEM ENROLL AUDIO NONE";
char lastSpeechTiming[96] = "GEM CAPTURE TIMING NONE";
std::atomic<size_t> captured{0};
std::atomic<bool> capturing{false}, captureFault{false};
std::atomic<bool> settling{false};
std::atomic<uint32_t> settlingStarted{0};
std::atomic<size_t> preSpeechSamples{0};
size_t queued = 0; // Written only by setup before capture, then by the mic callback.
void released(void*, void* data, size_t length) {
  const size_t count = captured.load(std::memory_order_relaxed);
  if (length != UtteranceWindow::frame || data != input + count || count + length > captureCapacity) {
    captureFault.store(true); return;
  }
  // Stamp completed settling frames in the producer. A delayed UI update at
  // 700ms must not discard speech that has already arrived after the gate.
  if(settling.load(std::memory_order_acquire) && millis()-settlingStarted.load()<700)
    preSpeechSamples.store(count+length,std::memory_order_release);
  // This callback runs in the mic task: queue the successor before returning,
  // so the driver carries its DMA tail across the two requests without a gap.
  if (capturing.load() && queued < captureCapacity) {
    if (!M5.Mic.record(input + queued, UtteranceWindow::frame, 16000, false)) {
      // end() can win the request lock after the main task requests shutdown.
      // That expected rejection must not turn a completed utterance into FAIL.
      if (capturing.load()) captureFault.store(true);
    }
    else queued += UtteranceWindow::frame;
  }
  captured.store(count + length, std::memory_order_release);
}
bool pcReady() { return configured && pcOnline && millis() - pcSeen < 20000; }
void updateUi() {
#ifdef STACKCHAN_WIRELESS
  if(wirelessMode && wireless.ready() && !wireless.paired()) return;
#endif
  face.wakeAvailable(wakeInput.available());
  face.link(!configured ? deviceui::Link::Unconfigured : WiFi.status() != WL_CONNECTED ? deviceui::Link::WifiOffline :
            !pcReady() ? deviceui::Link::PcOffline : deviceui::Link::Ready);
  const bool wasAsleep = face.state.asleep;
  face.tick(millis());
  if (!wasAsleep && face.state.asleep) audio.stop();
  if (face.state.mode != Mode::WakeWait || !face.state.microphoneAllowed(true) ||
      !pcReady() || WiFi.status() != WL_CONNECTED || M5.BtnA.isPressed() || !consumed) wakeInput.stop(audio);
}
void waitUi(uint32_t duration) {
  const auto started = millis();
  while (millis() - started < duration) { M5.update(); updateUi(); delay(10); }
}
void idle() { face.state.rest(millis()); face.set(face.state.mode, wakeInput.available() && !face.state.asleep ? "Say call / hold to ask" : ""); updateUi(); }
bool canRecord() {
  if (!configured || WiFi.status() != WL_CONNECTED || !pcReady()) {
    Serial.printf("GEM START BLOCKED reason=%s\n", !configured ? "setup" : WiFi.status() != WL_CONNECTED ? "wifi" : "pc");
    face.set(Mode::Error, !configured ? "Run PC tool / USB" : WiFi.status() != WL_CONNECTED ? "WiFi disconnected" : "Start PC service");
    waitUi(1500); idle(); return false;
  }
  return true;
}
void wipe() { capturing.store(false); audio.stop(); M5.Mic.setBufferReleaseCallback(nullptr, nullptr); inputCount = 0;
  if (input) memset(input, 0, captureCapacity * 2); if (output) memset(output, 0, maxOutput * 2); }
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
      doc["outputRate"].as<unsigned>() == 24000 && doc["maxInputSamples"].as<unsigned>() >= maxInput &&
      doc["maxOutputSamples"].as<unsigned>() <= maxOutput) { ready = true; return; }
  if (!ready || !doc["generation"].is<uint32_t>() || doc["generation"].as<uint32_t>() != generation) { bad(3); return; }
  if (kind == "input_ack" && !inputEnded && sent == acked + 1 && doc["sequence"].is<uint32_t>() && doc["sequence"].as<uint32_t>() == acked) { ++acked; return; }
  if (kind == "thinking" && inputEnded && !thinking) { thinking = true; face.set(Mode::Thinking); return; }
  if (kind == "output_start" && thinking && !outputStarted && doc["sampleRate"].as<unsigned>() == 24000 &&
      doc["samples"].is<uint32_t>() && doc["frames"].is<uint32_t>() &&
      reply.begin(generation, doc["samples"].as<uint32_t>(), doc["frames"].as<uint32_t>())) { outputStarted = true; face.set(Mode::Receiving); return; }
  if (kind == "output_end" && outputStarted && !outputEnded && !credit && reply.complete() &&
      doc["samples"].is<uint32_t>() && doc["samples"].as<uint32_t>() == reply.total) { outputEnded = true; return; }
  if (kind == "complete" && played && !complete) { complete = true; return; }
  bad(3);
}
bool transport() {
  if (!inputCount || inputCount > maxInput || inputCount % 320) return false;
  ready = failed = inputEnded = thinking = outputStarted = credit = outputEnded = played = complete = closed = false;
  stopped = false; sent = acked = fault = 0; reply = {};
  if (++generation == 0) ++generation;
  ws.onEvent(event);
#ifdef STACKCHAN_WIRELESS
  if(wirelessMode) ws.beginSSL(host,port,"/live",static_cast<const char*>(nullptr),"");
  else
#endif
  ws.begin(host, port, "/live", "");
  ws.setReconnectInterval(500);
  snprintf(auth, sizeof(auth), "Authorization: Bearer %s", token); ws.setExtraHeaders(auth);
  const auto started = millis(); uint32_t nextSend = started, playbackStarted = 0;
  bool playbackQueued = false;
  while (!failed && !closed && millis() - started < 85000) {
    ws.loop();
    if (failed || closed) break;
    if (ready && !inputEnded && sent == acked && sent < inputCount / 320 && static_cast<int32_t>(millis() - nextSend) >= 0) {
      uint8_t packet[652]; livepcm::write32(packet, generation); livepcm::write32(packet + 4, sent);
      packet[8] = 0x40; packet[9] = 1; packet[10] = packet[11] = 0;
      memcpy(packet + 12, input + sent * 320, 640); ++sent;
      if (!ws.sendBIN(packet, sizeof(packet))) bad(6);
      nextSend = millis() + 20;
    }
    if (!inputEnded && acked == inputCount / 320) { inputEnded = true; if (!control("input_end")) bad(6); memset(input, 0, inputCount * 2); }
    if (outputStarted && !outputEnded && !credit && !reply.complete()) { credit = true; if (!control("next", reply.sequence)) bad(6); }
    if (outputEnded && !playbackQueued) {
      playbackQueued = true; playbackStarted = millis();
      if (!audio.output() || !audio.play(output, reply.total)) bad(7);
      else face.set(Mode::Speaking);
    }
    if (playbackQueued && !played && !failed) {
      if (millis() - playbackStarted > 12000) bad(8);
      else if (!audio.playing()) { audio.stop(); face.set(Mode::Cooldown); played = true; if (!control("played", reply.total)) bad(6); }
    }
    M5.update(); if (M5.BtnA.wasPressed()) bad(9);
    updateUi(); delay(1);
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
void stopSpeechCapture() {
  capturing.store(false); audio.stop(); settling.store(false); M5.Mic.setBufferReleaseCallback(nullptr,nullptr);
}
bool startSpeechCapture(bool afterCue=false,uint32_t cueEnded=0) {
  if(!audio.listen(16000)) return false;
  captured.store(0); captureFault.store(false); preSpeechSamples.store(0);
  settlingStarted.store(cueEnded); settling.store(afterCue); queued=UtteranceWindow::frame; capturing.store(true);
  M5.Mic.setBufferReleaseCallback(nullptr,released);
  return M5.Mic.record(input,UtteranceWindow::frame,16000,false);
}
bool captureCancelled() {
  M5.update();
  // Enrollment already rejects commands other than INFO/PING/STOP. A normal
  // question leaves USB commands queued to avoid recursive PROBE dispatch.
  if(enrollmentActive) pollCommands();
  updateUi(); return M5.BtnA.wasPressed();
}
struct SpeechPreparation {
  AmbientGate ambient;
  size_t offset=0;
  uint32_t ms=0,gap=0,started=0;
};
const char* prepareSpeechCapture(SpeechPreparation& prep) {
  strlcpy(lastSpeechTiming,"GEM CAPTURE TIMING NONE",sizeof(lastSpeechTiming));
  prep.ambient.reset(); const auto preparing=millis();
  if(!startSpeechCapture()) { stopSpeechCapture(); return "IO"; }
  size_t processed=0; const char* failure=nullptr;
  while(!prep.ambient.ready() && millis()-preparing<3000) {
    if(captureCancelled()) { failure="CANCEL"; break; }
    if(captureFault.load()) { failure="IO"; break; }
    const size_t available=captured.load(std::memory_order_acquire);
    while(processed+UtteranceWindow::frame<=available && !prep.ambient.ready()) {
      prep.ambient.push(input+processed,UtteranceWindow::frame); processed+=UtteranceWindow::frame;
    }
    delay(2);
  }
  prep.ms=millis()-preparing; stopSpeechCapture();
  // Preparation audio never reaches the call model or API input.
  memset(input,0,captureCapacity*2);
  if(failure) return failure;
  if(!prep.ambient.ready()) return "IO";
  if(!prep.ambient.usable()) return "NOISY";
  face.set(Mode::CueWait,"Beep then wait 0.7s");
  if(!audio.cue()) { stopSpeechCapture(); return "IO"; }
  const auto cueEnded=millis();
  if(!startSpeechCapture(true,cueEnded)) { stopSpeechCapture(); return "IO"; }
  prep.started=millis();
  // Re-start/warm the shared codec inside the user's 700ms pause. No second
  // ambient calibration follows the cue. Completed pre-gate frames are dropped.
  while(millis()-cueEnded<700) {
    if(captureCancelled()) { stopSpeechCapture(); return "CANCEL"; }
    if(captureFault.load()) { stopSpeechCapture(); return "IO"; }
    delay(2);
  }
  prep.gap=millis()-cueEnded;
  settling.store(false);
  prep.offset=preSpeechSamples.load(std::memory_order_acquire);
  // Require at least the existing 400ms microphone warmup by the gate. A failed
  // restart must fail closed instead of unexpectedly accepting speech much later.
  if(prep.offset<6400 || captureFault.load()) { stopSpeechCapture(); return "IO"; }
  snprintf(lastSpeechTiming,sizeof(lastSpeechTiming),"GEM CAPTURE TIMING prep=%lu gap=%lu",
    static_cast<unsigned long>(prep.ms),static_cast<unsigned long>(prep.gap));
  Serial.println(lastSpeechTiming); return "READY";
}
bool recordQuestion() {
  strlcpy(lastCapture, "GEM CAPTURE FAIL", sizeof(lastCapture));
  if (!face.state.mayStart()) return false;
  face.set(Mode::Preparing, "Please stay quiet");
  if (!face.state.microphoneAllowed()) return false;
  SpeechPreparation prep;const char* prepared=prepareSpeechCapture(prep);
  if(strcmp(prepared,"READY")) {
    const bool cancelled=!strcmp(prepared,"CANCEL"), noisy=!strcmp(prepared,"NOISY");
    strlcpy(lastCapture,cancelled ? "GEM CAPTURE CANCEL" : noisy ? "GEM CAPTURE NOISY" : "GEM CAPTURE FAIL",sizeof(lastCapture));
    Serial.println(lastCapture);face.set(Mode::Notice,cancelled ? "Cancelled" : noisy ? "Too noisy" : "Microphone failed");return false;
  }
  UtteranceWindow window;window.reset(prep.ambient.threshold());
  Serial.printf("GEM VAD CALIBRATED rms=%lu threshold=%lu\n",static_cast<unsigned long>(prep.ambient.noise()),static_cast<unsigned long>(prep.ambient.threshold()));
  face.set(Mode::Listening,"Speak now (max 5s)");
  const auto started=prep.started;size_t processed=prep.offset;bool cancelled=false;
  while(!captureFault.load() && !window.terminal() && millis()-started<12500) {
    if(captureCancelled()) { cancelled=true;break; }
    const size_t available=captured.load(std::memory_order_acquire);
    while(processed+UtteranceWindow::frame<=available && !window.terminal()) {
      window.push(input+processed,UtteranceWindow::frame);
      uint64_t frameEnergy=0;
      for(size_t i=0;i<UtteranceWindow::frame;++i) {
        const int32_t value=input[processed+i];frameEnergy+=static_cast<int64_t>(value)*value;
      }
      face.level(static_cast<unsigned>(sqrt(static_cast<double>(frameEnergy)/UtteranceWindow::frame))/400);
      processed+=UtteranceWindow::frame;
    }
    delay(2);
  }
  stopSpeechCapture();
  const size_t completed=captured.load(std::memory_order_acquire);
  if(cancelled) { strlcpy(lastCapture,"GEM CAPTURE CANCEL",sizeof(lastCapture));Serial.println(lastCapture);face.set(Mode::Notice,"Cancelled");return false; }
  if(captureFault.load() || !window.terminal()) { Serial.println(lastCapture);return false; }
  if(!window.samples()) {
    snprintf(lastCapture,sizeof(lastCapture),"GEM CAPTURE EMPTY ms=%lu captured=%u",static_cast<unsigned long>(millis()-started),static_cast<unsigned>(completed));
    Serial.println(lastCapture);face.set(Mode::Notice,"No speech");return false;
  }
  inputCount = window.samples();
  const size_t start = window.start() + prep.offset; // Exclude the post-cue settling input.
  if (inputCount > maxInput || start + inputCount > completed) { Serial.println("GEM CAPTURE FAIL"); return false; }
  memmove(input, input + start, inputCount * 2);
  memset(input + inputCount, 0, (captureCapacity - inputCount) * 2);
  uint64_t energy = 0; uint32_t peak = 0, clipped = 0;
  for (size_t i = 0; i < inputCount; ++i) { const int32_t v = input[i]; const uint32_t m = v < 0 ? -v : v;
    energy += static_cast<int64_t>(v) * v; peak = max(peak, m); if (m >= 32760) ++clipped; }
  snprintf(lastCapture, sizeof(lastCapture), "GEM CAPTURE DONE samples=%u ms=%lu rms=%lu peak=%lu clipped=%lu captured=%u end=%s",
    static_cast<unsigned>(inputCount), static_cast<unsigned long>(millis() - started),
    static_cast<unsigned long>(sqrt(static_cast<double>(energy) / inputCount)), static_cast<unsigned long>(peak),
    static_cast<unsigned long>(clipped), static_cast<unsigned>(completed), window.limited() ? "limit" : "silence");
  Serial.println(lastCapture); return true;
}
void run(bool microphone) {
  wakeInput.stop(audio);
  if (!canRecord()) return;
  wipe();
  if (microphone) {
    if (!recordQuestion()) { wipe();
      if (face.state.mode == Mode::Preparing || face.state.mode == Mode::Listening) face.set(Mode::Error, "Microphone failed");
      waitUi(1500); M5.update(); consumed = true; idle(); return; }
  } else {
    // Exercise the VAD and variable upload path without starting the microphone.
    UtteranceWindow window; window.reset();
    for (size_t offset = 0; offset < captureCapacity && !window.terminal(); offset += 320) {
      for (size_t i = 0; i < 320; ++i) input[offset + i] = offset >= 12800 && offset < 22400 ? static_cast<int16_t>(2000 * sinf(2 * PI * 1000 * (offset + i) / 16000)) : 0;
      window.push(input + offset, 320);
    }
    inputCount = window.samples();
    if (!window.terminal() || inputCount != 24000 || window.start() != 9600) { wipe(); Serial.println("GEM VADTEST FAIL"); idle(); return; }
    memmove(input, input + window.start(), inputCount * 2);
    Serial.printf("GEM VADTEST PASS samples=%u\n", static_cast<unsigned>(inputCount));
  }
  face.set(Mode::Sending); const bool ok = transport();
  wipe();
#ifdef STACKCHAN_WIRELESS
  if(wirelessMode && !ok) wireless.offline();
#endif
  if (ok) { face.set(Mode::Cooldown); waitUi(700); }
  else { face.set(fault == 9 ? Mode::Notice : Mode::Error, fault == 9 ? "Cancelled" : "Check PC service"); waitUi(1500); }
  M5.update(); consumed = true; idle();
}
bool enrollmentChoice(const char* note, Mode mode=Mode::Setup) {
  face.set(mode,note); const auto start=millis(); uint32_t hold=0; bool down=false;
  while(millis()-start<30000) {
    M5.update(); pollCommands(); updateUi();
    if(M5.BtnA.wasPressed()) { down=true; hold=millis(); }
    if(down && M5.BtnA.isPressed() && millis()-hold>=2000) return false;
    if(down && M5.BtnA.wasReleased()) return true;
    delay(5);
  }
  return false;
}
const char* enrollmentCapture(Mode mode, size_t& count, unsigned sample=1) {
  strlcpy(lastEnrollmentCal,"GEM ENROLL CAL NONE",sizeof(lastEnrollmentCal));
  strlcpy(lastEnrollmentAudio,"GEM ENROLL AUDIO NONE",sizeof(lastEnrollmentAudio));
  char note[32];snprintf(note,sizeof(note),"%u/3 Stay quiet",sample);
  count=0;wipe();face.set(Mode::Preparing,note);
  SpeechPreparation prep;const char* prepared=prepareSpeechCapture(prep);
  snprintf(lastEnrollmentCal,sizeof(lastEnrollmentCal),"GEM ENROLL CAL rms=%lu threshold=%lu ms=%lu gap=%lu",
    static_cast<unsigned long>(prep.ambient.noise()),static_cast<unsigned long>(prep.ambient.threshold()),
    static_cast<unsigned long>(prep.ms),static_cast<unsigned long>(prep.gap));
  Serial.println(lastEnrollmentCal);
  if(strcmp(prepared,"READY")) {
    strlcpy(lastEnrollmentAudio,"GEM ENROLL AUDIO samples=0 voiced=0 ms=0",sizeof(lastEnrollmentAudio));
    Serial.println(lastEnrollmentAudio);return prepared;
  }
  WakeEnrollmentCapture window(input,captureCapacity,prep.ambient);
  snprintf(note,sizeof(note),"%u/3 Say same call",sample);face.set(mode,note);
  size_t processed=prep.offset;bool cancelled=false;
  while(!captureFault.load() && !window.terminal() && millis()-prep.started<10000) {
    if(captureCancelled()) { cancelled=true;break; }
    const size_t available=captured.load(std::memory_order_acquire);
    while(processed+UtteranceWindow::frame<=available && !window.terminal()) {
      // The discarded restart input provides >=6400 samples of headroom, so
      // compaction cannot overwrite unfinished or future DMA input.
      window.push(input+processed,UtteranceWindow::frame);processed+=UtteranceWindow::frame;
    }
    delay(2);
  }
  stopSpeechCapture();count=window.samples();
  snprintf(lastEnrollmentAudio,sizeof(lastEnrollmentAudio),"GEM ENROLL AUDIO samples=%lu voiced=%lu ms=%lu",
    static_cast<unsigned long>(window.recordedSamples()),static_cast<unsigned long>(window.voicedSamples()),
    static_cast<unsigned long>(millis()-prep.started));
  Serial.println(lastEnrollmentAudio);
  if(cancelled)return "CANCEL";
  if(captureFault.load())return "IO";
  using R=WakeEnrollmentCapture::Result;
  switch(window.result()) {
    case R::Ready:return "READY";
    case R::Empty:return "NO_SPEECH";
    case R::Short:return "SHORT";
    case R::Long:return "LONG";
    case R::Noisy:return "NOISY";
    case R::Clipped:return "CLIPPED";
    default:return "TIMEOUT";
  }
}
void enrollmentStep(const char* step) {
  snprintf(lastEnrollmentStep,sizeof(lastEnrollmentStep),"GEM ENROLL STEP %s",step);
  Serial.println(lastEnrollmentStep);
}
void enrollmentResult(const char* result) {
  snprintf(lastEnrollmentResult,sizeof(lastEnrollmentResult),"GEM ENROLL result=%s",result);
  Serial.println(lastEnrollmentResult);
}
const char* enrollmentNotice(const char* result) {
  return !strcmp(result,"SAVED") ? "Call saved" : !strcmp(result,"SHORT") ? "Call too short" :
    !strcmp(result,"LONG") ? "Call too long" : !strcmp(result,"NO_SPEECH") ? "No call heard" :
    !strcmp(result,"NOISY") ? "Room too noisy" : !strcmp(result,"CLIPPED") ? "Voice too loud" :
    !strcmp(result,"MISMATCH") ? "Calls differ / retry" : !strcmp(result,"SAVE_FAIL") ? "Save failed (kept)" :
    !strcmp(result,"UNAVAILABLE") ? "Setup unavailable" : !strcmp(result,"CANCEL") ? "Cancelled / kept old" : "Try again / kept old";
}
void enrollCall() {
  wakeInput.stop(audio);
  enrollmentActive=true; face.state.wake(millis());
  const char* result="CANCEL"; size_t count=0;
  while(true) {
    wipe(); wakeInput.cancel();
    strlcpy(lastEnrollmentCal,"GEM ENROLL CAL NONE",sizeof(lastEnrollmentCal));
    strlcpy(lastEnrollmentResult,"GEM ENROLL result=NONE",sizeof(lastEnrollmentResult));
    strlcpy(lastEnrollmentAudio,"GEM ENROLL AUDIO NONE",sizeof(lastEnrollmentAudio));
    result="CANCEL";
    if(!wakeInput.canEnroll()) { result="UNAVAILABLE"; break; }
    for(unsigned sample=1;sample<=3;++sample) {
      char step[16], note[32]; snprintf(step,sizeof(step),"record%u",sample); enrollmentStep(step);
      snprintf(note,sizeof(note),"%u/3 Click: record",sample);
      if(!enrollmentChoice(note)) { result="CANCEL"; break; }
      result=enrollmentCapture(Mode::Enrolling,count,sample);
      if(strcmp(result,"READY")) break;
      const bool collected=sample==1 ? wakeInput.prepare(input,count) : wakeInput.collect(input,count);
      wipe();
      if(!collected) { result="IO"; break; }
    }
    if(!strcmp(result,"READY")) {
      enrollmentStep("check"); face.set(Mode::Setup,"Checking 3 calls");
      if(!wakeInput.validate()) result="MISMATCH";
      else {
        enrollmentStep("save"); face.set(Mode::Setup,"Saving call");
        result=wakeInput.commit() ? "SAVED" : "SAVE_FAIL";
      }
    }
    wipe(); wakeInput.cancel();
    if(!strcmp(result,"SAVED") || !strcmp(result,"CANCEL")) break;
    enrollmentResult(result); face.set(Mode::Notice,enrollmentNotice(result)); waitUi(3000);
    enrollmentStep("retry");
    if(!enrollmentChoice("Click: retry / hold:no")) break;
  }
  wipe(); wakeInput.cancel(); enrollmentActive=false; consumed=true;
  enrollmentResult(result); face.set(Mode::Notice,enrollmentNotice(result)); waitUi(1800);
  face.state.sleep(millis()); idle();
}
// Preserve Wi-Fi provisioning in the normal Gemini firmware, not only probes.
// Never erase stored credentials or expose network passwords on USB.
void wifiDiagnostic(WiFiEvent_t event) {
  // Fixed event names only: no SSID, password, MAC address, or payload.
  if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) Serial.println("GEM WIFI AP_JOIN");
  else if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) Serial.println("GEM WIFI AP_LEAVE");
  else if (event == ARDUINO_EVENT_WIFI_AP_STOP) Serial.println("GEM WIFI AP_STOP");
}
void connectWifi(bool forcePortal) {
  audio.stop();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFiManager manager;
  manager.setDebugOutput(false);
  manager.setRestorePersistent(false);
  manager.setConnectTimeout(15);
  manager.setConfigPortalTimeout(0); // Standard WiFiManager: no portal timeout.
  manager.setBreakAfterConfig(false); // Keep the portal open after a failed save/connect.
  char password[] = "12345678"; // User-selected setup AP password.
  face.set(Mode::Connecting, "Connecting WiFi");
  manager.setAPCallback([&](WiFiManager*) {
    // Use the whole small display so the AP/password/IP remain readable.
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(2, 2);
    M5.Display.print("WiFi setup\nStackchan-Setup\nPW: ");
    M5.Display.print(password);
    M5.Display.print("\n192.168.4.1\nNo timeout");
    Serial.println("GEM WIFI SETUP");
  });
  if (forcePortal) manager.startConfigPortal("Stackchan-Setup", password);
  else manager.autoConnect("Stackchan-Setup", password);
  manager.stopConfigPortal();
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  memset(password, 0, sizeof(password));
  Serial.println(WiFi.status() == WL_CONNECTED ? "GEM WIFI CONNECTED" : "GEM WIFI FAIL");
}
void configure() {
  char candidateHost[16]{}, candidateToken[65]{}, extra; unsigned candidatePort;
  if (sscanf(command, "GEM CONFIG %15s %u %64s %c", candidateHost, &candidatePort, candidateToken, &extra) != 3 || !candidatePort || candidatePort > 65535 || strlen(candidateToken) != 64) return;
  IPAddress ip; if (!ip.fromString(candidateHost) || !(ip[0] == 10 || (ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31) || (ip[0] == 192 && ip[1] == 168))) return;
  for (size_t i = 0; i < 64; ++i) if (!isxdigit(static_cast<unsigned char>(candidateToken[i]))) return;
  strlcpy(host, candidateHost, sizeof(host)); strlcpy(token, candidateToken, sizeof(token)); port = candidatePort; memset(candidateToken, 0, sizeof(candidateToken));
#ifdef STACKCHAN_WIRELESS
  wirelessMode=false; // Explicit USB diagnostic mode; normal operation uses TLS.
#endif
  Serial.println("GEM CONFIG OK"); face.set(Mode::Connecting); WiFi.persistent(false); WiFi.mode(WIFI_STA); WiFi.begin();
  const auto started = millis(); while (WiFi.status() != WL_CONNECTED && millis() - started < 15000) { updateUi(); delay(20); }
  configured = WiFi.status() == WL_CONNECTED; pcOnline = configured; pcSeen = millis();
  Serial.println(configured ? "GEM WIFI CONNECTED" : "GEM WIFI FAIL"); idle();
}
void dispatch() {
  if(enrollmentActive && strcmp(command,"GEM PING") && strcmp(command,"GEM INFO") && strcmp(command,"GEM STOP")) {
    Serial.println("GEM LOCAL BUSY"); return;
  }
  if (strcmp(command, "GEM PING") == 0 || strcmp(command, "GEM INFO") == 0) {
    if (strcmp(command, "GEM PING") == 0 && configured) {
      const bool wasOnline = pcReady(); pcSeen = millis(); pcOnline = true;
      if (!wasOnline) updateUi();
    }
    Serial.println("GEM READY v=2 input=16000 output=24000 mono=1 bits=16");
    Serial.println("GEM BUILD enrollment=7");
    Serial.printf("GEM WAKE available=%u\n",wakeInput.available() ? 1 : 0);
#ifdef STACKCHAN_WIRELESS
    Serial.println(wirelessMode ? "GEM LINK TLS" : "GEM LINK USB");
    Serial.println(wireless.paired() ? "GEM PAIR READY" : "GEM PAIR REQUIRED");
#endif
    Serial.printf("GEM STATE configured=%u wifi=%u pc=%u\n", configured ? 1 : 0, WiFi.status() == WL_CONNECTED ? 1 : 0, pcReady() ? 1 : 0);
    Serial.printf("GEM UI stage=3 mode=%u asleep=%u mic=%u\n", static_cast<unsigned>(face.state.mode), face.state.asleep ? 1 : 0, M5.Mic.isRunning() ? 1 : 0);
    Serial.println(lastCapture); Serial.println(lastResult);
    Serial.println(lastEnrollmentCal); Serial.println(lastEnrollmentResult);
    Serial.println(lastEnrollmentStep); Serial.println(lastEnrollmentAudio); Serial.println(lastSpeechTiming);
  }
  else if (strncmp(command, "GEM CONFIG ", 11) == 0) configure();
  else if (strcmp(command, "GEM PROBE") == 0) run(false);
  else if (strcmp(command, "GEM STOP") == 0) { pcOnline = false; configured = false; memset(token, 0, sizeof(token)); memset(auth, 0, sizeof(auth)); Serial.println("GEM PC OFFLINE"); if(enrollmentActive) updateUi(); else idle(); }
}
void pollCommands() {
  while (Serial.available()) { const char c = Serial.read();
    if (c == '\n') { command[commandLength] = 0; if (!overflow) dispatch(); memset(command, 0, sizeof(command)); commandLength = 0; overflow = false; }
    else if (c != '\r') { if (commandLength < sizeof(command) - 1) command[commandLength++] = c; else overflow = true; }
  }

}

}
void setup() {
  auto cfg = M5.config(); cfg.internal_spk = false; cfg.internal_mic = false; cfg.external_speaker.atomic_echo = true; cfg.serial_baudrate = 115200;
  M5.begin(cfg); audio.stop(); M5.Display.setTextSize(1); M5.Speaker.setVolume(100);
  face.begin(); face.set(Mode::Boot);
  input = static_cast<int16_t*>(heap_caps_calloc(captureCapacity, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  output = static_cast<int16_t*>(heap_caps_calloc(maxOutput, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!input || !output) { face.set(Mode::Error, "PSRAM failed"); Serial.println("GEM MEMORY FAIL"); return; }
  M5.update();
  WiFi.onEvent(wifiDiagnostic);
  connectWifi(M5.BtnA.isPressed());
#ifdef STACKCHAN_WIRELESS
  if(!wireless.begin()) Serial.println("GEM PAIR FAIL");
#endif
  wakeInput.begin();
  Serial.printf("GEM WAKE available=%u\n", wakeInput.available() ? 1 : 0);
  face.state.sleep(millis()); idle(); Serial.println("GEM READY v=2 input=16000 output=24000 mono=1 bits=16");
}
void loop() {
#ifdef STACKCHAN_WIRELESS
  if(wirelessMode && wireless.ready()) {
    const bool wasPaired=wireless.paired();
    const bool online=wireless.online();
    configured=true;pcOnline=online;
    if(online) pcSeen=millis();
    const String gateway=WiFi.gatewayIP().toString();strlcpy(host,gateway.c_str(),sizeof(host));
    port=WirelessLink::port;strlcpy(token,wireless.key(),sizeof(token));
    if(!wasPaired && wireless.paired()) {face.state.sleep(millis());idle();}
    if(!wireless.paired()) {
      wireless.showCode();pollCommands();delay(50);return;
    }
  }
#endif
  M5.update(); updateUi(); if (!input || !output) { delay(20); return; }
  if (pcOnline && !pcReady()) { pcOnline = false; Serial.println("GEM PC OFFLINE"); updateUi(); }
  if (M5.BtnA.wasPressed()) { pressedAt = millis(); consumed = false; }
  if (!consumed && M5.BtnA.isPressed() && millis() - pressedAt >= 2000) {
    consumed = true;
    // A held button waking the device cannot also start a recording.
    if (face.state.asleep) {
      face.set(Mode::Setup,"Hold 5s: learn call");
      while(M5.BtnA.isPressed() && millis()-pressedAt<5000) { M5.update(); updateUi(); delay(10); }
      if(M5.BtnA.isPressed()) {
        face.set(Mode::Setup,"Release: learn call"); const auto wait=millis();
        while(M5.BtnA.isPressed() && millis()-wait<10000) {
          M5.update(); updateUi();
#ifdef STACKCHAN_WIRELESS
          if(millis()-pressedAt>=10000) {
            const auto release=millis();
            while(M5.BtnA.isPressed() && millis()-release<5000) {M5.update();delay(10);}
            if(M5.BtnA.isPressed()) {face.state.sleep(millis());idle();return;}
            if(wireless.resetPairing()) {wirelessMode=true;wireless.showCode();}
            return;
          }
#endif
          delay(10);
        }
        if(!M5.BtnA.isPressed()) enrollCall(); else { face.state.sleep(millis()); idle(); }
      } else { face.state.wake(millis()); face.tick(millis(),true); }
      return;
    }
    if (!face.state.mayStart() || !canRecord()) return;
    face.set(Mode::Armed, "Release to record"); const auto releaseWait = millis();
    while (M5.BtnA.isPressed() && millis() - releaseWait < 10000) { M5.update(); updateUi(); delay(10); }
    if (M5.BtnA.isPressed()) { Serial.println("GEM START BLOCKED reason=button"); face.set(Mode::Notice, "Release and retry"); waitUi(1500); idle(); return; }
    run(true);
  }
  if (M5.BtnA.wasReleased() && !consumed) {
    consumed = true; audio.stop();
    if (face.state.asleep) face.state.wake(millis());
    else face.state.sleep(millis());
    face.tick(millis(), true);
  }
  pollCommands();
  if (face.state.mode == Mode::WakeWait && face.state.microphoneAllowed(true) &&
      pcReady() && WiFi.status() == WL_CONNECTED && consumed && !M5.BtnA.isPressed()) {
    if (wakeInput.poll(audio)) { Serial.println("GEM WAKE MATCH"); run(true); }
  } else wakeInput.stop(audio);
  delay(2);
}
#ifdef STACKCHAN_WIRELESS
const char* stackchanTlsPsk() { return wireless.key(); }
#endif
#endif
