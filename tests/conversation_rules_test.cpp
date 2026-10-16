#include "ConversationRules.h"
#include "ProbeResample.h"
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
  std::cout << passed << " tests passed\n";
}
