// Exercise real button, recording and transport paths with hardware replaced.
#define STACKCHAN_GEMINI_DEVICE 1
#include "firmware_ui_stubs/Fakes.h"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4244 4267)
#endif
#include "../src/AudioDevice.cpp"
#include "../src/FaceUI.cpp"
#include "../src/LocalGemini.cpp"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <fstream>
#include <iostream>

namespace {
unsigned failures = 0, checks = 0;
void check(bool ok, const char* name) {
  ++checks; if (!ok) ++failures;
  std::cout << (ok ? "PASS " : "FAIL ") << name << '\n';
}
void resetRuntime() {
  std::free(input); std::free(output); input = output = nullptr;
  fakeTime = 1000; fakeGestureStart = fakeTime; fakeHeldForever = false;
  fakeDelayBudget = 1000000; fakeMicBegins = fakeRecordRequests = fakeBeeps = 0;
  fakeMicActive = false; fakePlaybackMs = fakePlaybackUntil = 0; fakeWsLoop = {};
  fakeButtonSchedule = [](uint32_t) { return false; };
  M5.BtnA = {}; M5.Display = {}; face.state = {};
  configured = pcOnline = true; pcSeen = fakeTime; WiFi.state = WL_CONNECTED; consumed = true;
  setup();
}
void advance(uint32_t ms) { const auto start = fakeTime; while (fakeTime - start < ms) loop(); }
void gesture(uint32_t ms) {
  const auto start = fakeTime;
  fakeButtonSchedule = [start,ms](uint32_t now) { return now - start < ms; };
  advance(ms + 40);
  fakeButtonSchedule = [](uint32_t) { return false; };
}
void awake() { face.state.wake(fakeTime); advance(360); }
}
int main() {
  resetRuntime(); fakeWakeAvailable = true; awake();
  check(fakeMicActive && fakeWakePolls > 0, "registered call enables local mic only after waking");
  const auto beforeSleep = fakeWakePolls;
  gesture(60); advance(100);
  check(!fakeMicActive && fakeWakePolls == beforeSleep, "sleep stops wake input and polling");
  awake(); pcOnline = false; loop();
  check(!fakeMicActive, "PC loss stops wake microphone");
  pcOnline = true; pcSeen = fakeTime; WiFi.state = 0; loop();
  check(!fakeMicActive, "WiFi loss keeps wake microphone off");
  WiFi.state = WL_CONNECTED; loop();
  check(fakeMicActive, "link recovery restarts local detection");
  fakeWakeMatch = true;
  const auto cues = fakeBeeps; loop();
  check(fakeBeeps == cues && face.state.mode == Mode::WakeWait && !fakeMicActive,
        "matched call handles preparation failure without a false start cue and returns ready");
  const Mode wakeBlocked[] = {Mode::Sleep, Mode::Waking, Mode::Preparing, Mode::Listening,
    Mode::Sending, Mode::Thinking, Mode::Receiving, Mode::Speaking, Mode::Cooldown, Mode::Notice, Mode::Error, Mode::CueWait};
  bool wakeQuiet = true;
  for (auto mode : wakeBlocked) {
    face.state.set(mode, fakeTime); const auto polls = fakeWakePolls; loop();
    wakeQuiet = wakeQuiet && polls == fakeWakePolls && !fakeMicActive;
  }
  check(wakeQuiet, "all non-wait modes stop wake microphone and never poll");
  face.state.rest(fakeTime); loop();
  fakeTime += deviceui::State::sleepAfterMs; pcSeen = fakeTime; loop();
  check(face.state.asleep && !fakeMicActive, "wake polling does not extend auto sleep timeout");
  fakeWakeAvailable = false; wakeInput.stop(audio);
  resetRuntime();
  check(face.state.mode == Mode::Sleep && !fakeMicActive && fakeMicBegins == 0, "boot sleeps with microphone stopped");
  gesture(60); advance(360);
  check(face.state.mode == Mode::WakeWait && !fakeMicActive && fakeMicBegins == 0, "click wakes without recording");
  gesture(60);
  check(face.state.mode == Mode::Sleep && !fakeMicActive, "click returns to sleep");
  gesture(2100); advance(360);
  check(face.state.mode == Mode::WakeWait && fakeMicBegins == 0, "one long gesture from sleep only wakes");
  gesture(2100);
  check(fakeMicBegins == 1 && fakeRecordRequests == 1 && !fakeMicActive && face.state.mode == Mode::WakeWait,
        "awake hold starts one manual recording; failure stops mic and returns ready");

  resetRuntime(); awake();
  fakeTime += deviceui::State::sleepAfterMs; pcSeen = fakeTime; loop();
  check(face.state.mode == Mode::Sleep && !fakeMicActive, "three minutes of idle automatically sleeps");
  deviceui::State rollover; const uint32_t nearWrap = 0xFFFFFF00u;
  rollover.wake(nearWrap); rollover.tick(nearWrap + 400u); rollover.tick(nearWrap + deviceui::State::sleepAfterMs);
  check(rollover.mode == Mode::Sleep, "auto sleep survives millis rollover");

  resetRuntime(); awake();
  const Mode blocked[] = {Mode::Sleep, Mode::Waking, Mode::Sending, Mode::Thinking,
    Mode::Receiving, Mode::Speaking, Mode::Cooldown, Mode::Notice, Mode::Error, Mode::Setup};
  bool gated = true;
  for (auto mode : blocked) {
    face.state.set(mode, fakeTime);
    gated = gated && !face.state.microphoneAllowed() && !recordQuestion() && fakeMicBegins == 0;
  }
  face.state.set(Mode::WakeWait, fakeTime);
  gated = gated && !face.state.microphoneAllowed() && face.state.microphoneAllowed(true);
  face.state.set(Mode::Listening, fakeTime); gated = gated && face.state.microphoneAllowed();
  face.state.sleep(fakeTime); gated = gated && !face.state.microphoneAllowed(true);
  check(gated, "sleep and transaction phases reject microphone entry; wake detection requires explicit permission");

  resetRuntime(); awake(); inputCount = 320; fakePlaybackMs = 250;
  unsigned iterations = 0; bool quiet = true, spoke = false;
  fakeWsLoop = [&]() {
    ++iterations; quiet = quiet && !fakeMicActive && fakeMicBegins == 0 && !face.state.microphoneAllowed();
    ready = true; acked = sent;
    if (inputEnded && !thinking) { thinking = true; face.set(Mode::Thinking); }
    if (thinking && iterations > 25) { outputStarted = outputEnded = true; reply.total = reply.received = 2400; }
    if (face.state.mode == Mode::Speaking) spoke = true;
    if (played) { complete = closed = true; }
  };
  // run(false) is the existing synthetic path; it never starts a microphone or calls a provider.
  run(false); fakeWsLoop = {};
  check(quiet && spoke && played && face.state.mode == Mode::WakeWait && fakeMicBegins == 0,
        "actual transport and playback keep microphone off through return to ready");

  resetRuntime(); awake(); fakePlaybackMs = 2000;
  const auto cancelStart = fakeTime;
  fakeButtonSchedule = [cancelStart](uint32_t now) { return now - cancelStart >= 80 && now - cancelStart < 140; };
  fakeWsLoop = []() { ready = true; acked = sent; };
  run(false); fakeWsLoop = {};
  advance(100);
  check(fault == 9 && face.state.mode == Mode::WakeWait && !fakeMicActive && fakeMicBegins == 0,
        "busy button cancels and its release does not put device to sleep or start recording");

  resetRuntime(); awake();
  face.set(Mode::Thinking); const auto frames = fakeFrames;
  for (unsigned i = 1; i < 50; ++i) face.tick(fakeTime + i);
  check(fakeFrames == frames, "unchanged animation is capped at twenty frames per second");
  face.tick(fakeTime + 50); check(fakeFrames == frames + 1, "animation advances without a blocking sleep");
  fakeCanvasFailure = true;
  check(!face.begin(), "sprite allocation failure is observable");
  face.set(Mode::Error, "safe fallback");
  check(M5.Display.text == "safe fallback", "allocation failure preserves a text fallback");
  fakeCanvasFailure = false; face.begin();

  const Mode modes[] = {Mode::Boot, Mode::Connecting, Mode::Sleep, Mode::Waking, Mode::WakeWait,
    Mode::Armed, Mode::Preparing, Mode::Listening, Mode::Sending, Mode::Thinking, Mode::Receiving,
    Mode::Speaking, Mode::Cooldown, Mode::Notice, Mode::Error, Mode::Enrolling, Mode::Verify, Mode::Setup, Mode::CueWait};
  std::ofstream preview("face-mode-preview.svg");
  preview << "<svg xmlns='http://www.w3.org/2000/svg' width='768' height='512' viewBox='0 0 768 512'>";
  unsigned index = 0;
  for (auto mode : modes) {
    fakeTime = 1550; face.state.asleep = mode == Mode::Sleep;
    face.set(mode, mode == Mode::WakeWait ? "Hold: ask" : ""); face.level(6); face.tick(fakeTime, true);
    const auto& svg = M5.Display.svg;
    preview << "<g transform='translate(" << (index % 6) * 128 << ',' << (index / 6) * 128 << ")'>"
            << svg.substr(svg.find('>') + 1, svg.rfind("</svg>") - svg.find('>') - 1) << "</g>";
    ++index;
  }
  preview << "</svg>"; preview.close();
  check(preview.good(), "all nineteen modes render from actual drawing code to an offline SVG preview");
  std::free(input); std::free(output); input = output = nullptr;
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
