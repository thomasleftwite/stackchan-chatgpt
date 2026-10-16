#pragma once
#include <cstddef>
#include <cstdint>

namespace livepcm {
inline uint32_t read32(const uint8_t* p) {
  uint32_t v = 0; for (unsigned i = 0; i < 4; ++i) v |= uint32_t(p[i]) << (8 * i); return v;
}
inline void write32(uint8_t* p, uint32_t v) {
  for (unsigned i = 0; i < 4; ++i) p[i] = uint8_t(v >> (8 * i));
}
struct Output {
  uint32_t generation = 0, total = 0, sequence = 0, received = 0;
  bool begin(uint32_t gen, uint32_t samples, uint32_t frames) {
    if (!gen || !samples || samples > 240000 || frames != (samples + 479) / 480) return false;
    generation = gen; total = samples; sequence = received = 0; return true;
  }
  bool accept(const uint8_t* packet, size_t length) {
    if (!total || received >= total || length < 14 || length > 972) return false;
    const uint32_t count = total - received < 480 ? total - received : 480;
    if (read32(packet) != generation || read32(packet + 4) != sequence ||
        (uint32_t(packet[8]) | uint32_t(packet[9]) << 8) != count || packet[10] || packet[11] || length != 12 + count * 2) return false;
    received += count; ++sequence; return true;
  }
  bool complete() const { return total && received == total; }
};
}
