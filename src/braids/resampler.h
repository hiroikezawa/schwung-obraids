// oBraids - 96 kHz -> 44.1 kHz polyphase resampler for the Braids engine. MIT.
//
// Braids renders at its native 96 kHz (see braids_engine.h for why). 44100 /
// 96000 is exactly 147 / 320, so output sample n sits at input time
// n * 320 / 147 = q + r / 147 and r only ever takes 147 values: one
// precomputed 64-tap Kaiser-windowed sinc per r (scripts/gen_tables.py).
// Flat to 19 kHz, -6 dB at 22.05 kHz, >= 70 dB down from 25.5 kHz.
//
// The input side is pull-driven: the caller asks NeedsInput() and pushes one
// Braids block (24 samples) at a time until the next output can be formed.
#ifndef OBRAIDS_RESAMPLER_H_
#define OBRAIDS_RESAMPLER_H_

#include <stdint.h>
#include <string.h>

#include "obraids_tables.h"

namespace obraids {

class Resampler96To44 {
 public:
  static const uint32_t kRing = 256;  // > taps + one Braids block, power of 2

  void Reset() {
    memset(ring_, 0, sizeof(ring_));
    // Pretend 2 * HALF zeros were written, so the first output (centred on
    // q = HALF) has a full window of history. Latency: HALF input samples
    // (0.33 ms) plus at most one Braids block.
    written_ = 2 * OBRAIDS_RS_HALF;
    q_ = OBRAIDS_RS_HALF;
    r_ = 0;
  }

  // True while the next output needs input that has not been pushed yet.
  // Compared as a signed DIFFERENCE, so the counters wrapping after ~12 h of
  // 96 kHz input changes nothing.
  bool NeedsInput() const {
    return static_cast<int32_t>(written_ - q_) <= OBRAIDS_RS_HALF;
  }

  void Push(const int16_t *in, int n, float scale) {
    for (int i = 0; i < n; ++i) {
      ring_[written_ & (kRing - 1)] = static_cast<float>(in[i]) * scale;
      ++written_;
    }
  }

  // Only call when !NeedsInput().
  float Next() {
    const float *h = obraids_rs_table[r_];
    uint32_t base = q_ - OBRAIDS_RS_HALF + 1;
    float acc = 0.0f;
    for (int j = 0; j < OBRAIDS_RS_TAPS; ++j) {
      acc += h[j] * ring_[(base + j) & (kRing - 1)];
    }
    r_ += OBRAIDS_RS_DOWN;
    while (r_ >= OBRAIDS_RS_UP) {
      r_ -= OBRAIDS_RS_UP;
      ++q_;
    }
    return acc;
  }

 private:
  float ring_[kRing];
  uint32_t written_;  // input samples written (wraps harmlessly: unsigned)
  uint32_t q_;        // integer input time of the next output
  uint32_t r_;        // fractional part, in 1/147 of an input sample
};

}  // namespace obraids

#endif  // OBRAIDS_RESAMPLER_H_
