#include "ConversationRules.h"
#include "ProbeResample.h"
#include "LivePcm.h"
#include "UtteranceWindow.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
using Result = SpeechGate::Result;
using Frame = std::array<int16_t, 320>;
const Frame quiet{};
Frame voice() {
  Frame f{};
  for (size_t i = 0; i < f.size(); ++i) f[i] = i % 2 ? 1200 : -1200;
  return f;
}
void require(bool condition) {
  if (!condition) throw std::runtime_error("unexpected decision");
}
Result feed(SpeechGate& gate, const Frame& f, unsigned frames) {
  Result result = Result::Waiting;
  while (frames--) result = gate.push(f.data(), f.size());
  return result;
}
SpeechGate gate(uint32_t maxMs = 3000, uint32_t idleMs = 5000) {
  SpeechGate g;
  g.reset(16000, 400, 200, maxMs, idleMs, 500);
  return g;
}
}

int main() {
  unsigned passed = 0;
  auto test = [&](const char* name, auto check) {
    try { check(); ++passed; std::cout << "PASS " << name << '\n'; }
    catch (const std::exception& e) {
      std::cerr << "FAIL " << name << ": " << e.what() << '\n'; std::exit(1);
    }
  };
  test("silence waits for configured idle timeout", [] {
    auto g = gate();
    require(feed(g, quiet, 249) == Result::Waiting);
    require(feed(g, quiet, 1) == Result::Empty);
  });
  test("speech duration excludes leading silence", [] {
    auto g = gate(); feed(g, quiet, 100);
    require(feed(g, voice(), 149) == Result::Recording);
    require(feed(g, voice(), 1) == Result::Complete);
  });
  test("minimum speech then trailing silence completes", [] {
    auto g = gate(); require(feed(g, voice(), 10) == Result::Recording);
    require(feed(g, quiet, 19) == Result::Recording);
    require(feed(g, quiet, 1) == Result::Complete);
  });
  test("short noise is discarded before valid speech", [] {
    auto g = gate(); feed(g, voice(), 9);
    require(feed(g, quiet, 20) == Result::Waiting);
    feed(g, voice(), 10);
    require(feed(g, quiet, 20) == Result::Complete);
  });
  test("new voice resets trailing silence", [] {
    auto g = gate(); feed(g, voice(), 10); feed(g, quiet, 19);
    feed(g, voice(), 1);
    require(feed(g, quiet, 19) == Result::Recording);
    require(feed(g, quiet, 1) == Result::Complete);
  });
  test("DC offset is not speech", [] {
    auto g = gate(); Frame dc{}; dc.fill(30000);
    require(feed(g, dc, 250) == Result::Empty);
  });
  test("full scale samples do not overflow energy", [] {
    auto g = gate(); Frame f{};
    for (size_t i = 0; i < f.size(); ++i) f[i] = i % 2 ? INT16_MAX : INT16_MIN;
    feed(g, f, 10); require(feed(g, quiet, 20) == Result::Complete);
  });
  test("zero length input does not consume timeout", [] {
    auto g = gate(); require(g.push(nullptr, 0) == Result::Waiting);
    require(feed(g, quiet, 249) == Result::Waiting);
  });
  test("reset discards preceding utterance", [] {
    auto g = gate(); feed(g, voice(), 10);
    g.reset(16000, 400, 200, 3000, 5000, 500);
    require(feed(g, quiet, 20) == Result::Waiting);
  });
  test("24 kHz uses sample count rather than frame count", [] {
    auto g = gate(); g.reset(24000, 400, 200, 3000, 5000, 500);
    feed(g, voice(), 15);
    require(feed(g, quiet, 29) == Result::Recording);
    require(feed(g, quiet, 1) == Result::Complete);
  });
  test("minimum and maximum valid template sizes", [] {
    require(validTemplateHeader(1, 2, 12, 57));
    require(validTemplateHeader(1, 186, 12, 4473));
  });
  test("invalid template version and signed lengths", [] {
    require(!validTemplateHeader(0, 2, 12, 57));
    require(!validTemplateHeader(1, -1, 12, 9));
    require(!validTemplateHeader(1, INT32_MAX, 12, 9));
  });
  test("template dimension and frame limits", [] {
    require(!validTemplateHeader(1, 1, 12, 33));
    require(!validTemplateHeader(1, 187, 12, 4497));
    require(!validTemplateHeader(1, 2, 13, 61));
  });
  test("truncated or extended template rejected", [] {
    require(!validTemplateHeader(1, 2, 12, 56));
    require(!validTemplateHeader(1, 2, 12, 58));
  });
  test("diagnostic resampler preserves DC and output boundaries", [] {
    std::array<int16_t, 320> input{}; input.fill(1234);
    std::array<int16_t, 482> output{}; output.front() = 999; output.back() = -999;
    probeResample16to24(input.data(), input.size(), output.data() + 1);
    require(output.front() == 999 && output.back() == -999);
    for (size_t i = 1; i <= 480; ++i) require(output[i] == 1234);
  });
  test("diagnostic resampler handles full scale without overflow", [] {
    const int16_t input[] = {INT16_MIN, INT16_MIN, INT16_MAX, INT16_MAX};
    int16_t output[6]{};
    probeResample16to24(input, 4, output);
    require(output[0] == INT16_MIN && output[1] == INT16_MIN);
    require(output[3] == INT16_MAX && output[5] == INT16_MAX);
    require(output[2] < 0);
  });
  test("diagnostic resampler preserves one-second 1kHz tone duration and pitch", [] {
    std::array<int16_t, 16000> input{};
    std::array<int16_t, 24000> output{};
    for (size_t i = 0; i < input.size(); ++i)
      input[i] = static_cast<int16_t>(2000 * std::sin(2 * 3.141592653589793 * 1000 * i / 16000));
    probeResample16to24(input.data(), input.size(), output.data());
    unsigned crossings = 0;
    for (size_t i = 1; i < output.size(); ++i)
      if (output[i - 1] <= 0 && output[i] > 0) ++crossings;
    require(crossings >= 999 && crossings <= 1001);
  });
  test("live output accepts a partial final frame and exact total", [] {
    livepcm::Output state; require(state.begin(7, 481, 2));
    std::array<uint8_t, 972> packet{}; livepcm::write32(packet.data(), 7);
    packet[8] = 0xe0; packet[9] = 1;
    require(state.accept(packet.data(), packet.size())); require(!state.complete());
    livepcm::write32(packet.data() + 4, 1); packet[8] = 1; packet[9] = 0;
    require(state.accept(packet.data(), 14)); require(state.complete());
    require(!state.accept(packet.data(), 14));
  });
  test("live output rejects stale reordered truncated and excessive audio", [] {
    livepcm::Output state; require(!state.begin(0, 480, 1)); require(!state.begin(1, 240001, 501));
    require(!state.begin(1, 480, 2)); require(state.begin(1, 480, 1));
    std::array<uint8_t, 972> packet{}; livepcm::write32(packet.data(), 2); packet[8] = 0xe0; packet[9] = 1;
    require(!state.accept(packet.data(), packet.size())); livepcm::write32(packet.data(), 1);
    livepcm::write32(packet.data() + 4, 1); require(!state.accept(packet.data(), packet.size()));
    livepcm::write32(packet.data() + 4, 0); require(!state.accept(packet.data(), packet.size() - 2));
    packet[10] = 1; require(!state.accept(packet.data(), packet.size())); packet[10] = 0;
    require(state.accept(packet.data(), packet.size())); require(state.complete());
  });
  test("utterance silence exits locally at five seconds and stays terminal", [] {
    UtteranceWindow w; w.reset();
    for (unsigned i = 0; i < 249; ++i) require(w.push(quiet.data(), quiet.size()) == UtteranceWindow::Result::Waiting);
    require(w.push(quiet.data(), quiet.size()) == UtteranceWindow::Result::Empty); require(w.samples() == 0);
    auto f = voice(); require(w.push(f.data(), f.size()) == UtteranceWindow::Result::Empty);
  });
  test("utterance trims leading silence with pre-roll and ends on trailing silence", [] {
    UtteranceWindow w; w.reset(); auto f = voice();
    for (unsigned i = 0; i < 40; ++i) w.push(quiet.data(), quiet.size());
    for (unsigned i = 0; i < 30; ++i) w.push(f.data(), f.size());
    for (unsigned i = 0; i < 34; ++i) require(w.push(quiet.data(), quiet.size()) == UtteranceWindow::Result::Recording);
    require(w.push(quiet.data(), quiet.size()) == UtteranceWindow::Result::Complete);
    require(w.start() == 9600 && w.samples() == 24000 && w.processed() == 33600 && !w.limited());
  });
  test("utterance rejects short noise and starts a fresh window for speech", [] {
    UtteranceWindow w; w.reset(); auto f = voice();
    for (unsigned i = 0; i < 4; ++i) w.push(f.data(), f.size());
    for (unsigned i = 0; i < 50; ++i) w.push(quiet.data(), quiet.size());
    for (unsigned i = 0; i < 10; ++i) w.push(f.data(), f.size());
    for (unsigned i = 0; i < 35; ++i) w.push(quiet.data(), quiet.size());
    require(w.terminal() && w.start() == 14080 && w.samples() == 17600);
  });
  test("utterance sustained voice respects API bound including pre-roll", [] {
    for (unsigned lead : {0u, 40u, 249u}) {
      UtteranceWindow w; w.reset(); auto f = voice();
      for (unsigned i = 0; i < lead; ++i) w.push(quiet.data(), quiet.size());
      for (unsigned i = 0; i < 240; ++i) w.push(f.data(), f.size());
      require(w.limited() && w.samples() <= 80000 && w.start() + w.samples() <= UtteranceWindow::capacity);
      require(w.samples() == (lead == 0 ? 76800u : 80000u));
    }
  });
  test("utterance malformed input fails closed and reset clears old speech", [] {
    UtteranceWindow w; w.reset(); auto f = voice();
    require(w.push(f.data(), 319) == UtteranceWindow::Result::Empty); require(w.samples() == 0);
    w.reset(); for (unsigned i = 0; i < 20; ++i) w.push(f.data(), f.size());
    w.reset(); require(w.start() == 0 && w.samples() == 0 && !w.terminal());
    require(w.push(nullptr, 320) == UtteranceWindow::Result::Empty);
  });
  test("ambient calibration ignores startup transients and rejects steady room noise", [] {
    AmbientGate ambient; ambient.reset(); auto loud = voice(); Frame room{};
    for (size_t i = 0; i < room.size(); ++i) room[i] = i % 2 ? 400 : -400;
    for (unsigned i = 0; i < 20; ++i) require(ambient.push(loud.data(), loud.size()));
    require(!ambient.ready());
    for (unsigned i = 0; i < 30; ++i) ambient.push(room.data(), room.size());
    require(ambient.usable() && ambient.noise() == 400 && ambient.threshold() == 1200);
    UtteranceWindow window; window.reset(ambient.threshold());
    for (unsigned i = 0; i < 250; ++i) window.push(room.data(), room.size());
    require(window.terminal() && window.samples() == 0);
  });
  test("ambient threshold has a floor, removes negative DC and rejects excessive noise", [] {
    AmbientGate ambient; ambient.reset(); Frame dc{}; dc.fill(-1000);
    for (unsigned i = 0; i < 50; ++i) ambient.push(dc.data(), dc.size());
    require(ambient.noise() == 0 && ambient.threshold() == 750 && ambient.usable());
    ambient.reset(); auto f = voice();
    for (unsigned i = 0; i < 50; ++i) ambient.push(f.data(), f.size());
    require(ambient.ready() && !ambient.usable());
  });
  test("calibrated utterance accepts speech above its measured noise threshold", [] {
    UtteranceWindow window; window.reset(1200); auto f = voice();
    for (auto& value : f) value *= 2;
    for (unsigned i = 0; i < 20; ++i) window.push(f.data(), f.size());
    for (unsigned i = 0; i < 35; ++i) window.push(quiet.data(), quiet.size());
    require(window.terminal() && window.samples() == 17600);
  });
  std::cout << passed << " tests passed\n";
}
