#pragma once
#include <stddef.h>
#include <stdint.h>

// Diagnostic playback only: 16k -> 24k linear interpolation, no gain change.
// Contract: nonempty even input count; output capacity >= count * 3 / 2.
inline void probeResample16to24(const int16_t* input, size_t count, int16_t* output) {
  for (size_t i = 0, j = 0; i < count; i += 2, j += 3) {
    const int32_t a = input[i], b = input[i + 1], c = input[i + 2 < count ? i + 2 : i + 1];
    output[j] = static_cast<int16_t>(a);
    output[j + 1] = static_cast<int16_t>((a + 2 * b) / 3);
    output[j + 2] = static_cast<int16_t>((2 * b + c) / 3);
  }
}
