// oBraids - Braids engine. MIT.
//
// The DSP is Mutable Instruments' own Braids code (src/braids/mi/, vendored
// unmodified from pichenettes/eurorack). This class is the part of the
// firmware's braids.cc that is NOT hardware: RenderBlock()'s AD envelope,
// AD->FM / AD->TIMBRE / AD->COLOR / AD->VCA, bit and rate reduction, the
// VCA smoothing and the signature waveshaper, with the ADC, DAC, gate input,
// display and EEPROM replaced by MIDI and plain fields.
//
// SAMPLE RATE. Braids is not merely "a 96 kHz oscillator": every table in
// resources.cc is generated for it -- oscillator phase increments and delays,
// SVF and resonator coefficients, the bowing/blowing excitation lengths --
// and the AD envelope advances once per 24-sample block, so its times are
// block counts at 96000/24 = 4000 Hz. Rescaling increments for 44.1 kHz
// would fix pitch and leave every one of those wrong. So the engine runs
// EXACTLY as the hardware does -- 96 kHz, 24-sample blocks -- and a polyphase
// FIR takes the result to 44.1 kHz (resampler.h). Pitch, envelope times and
// every model's internal timing are then the hardware's by construction.
#ifndef OBRAIDS_BRAIDS_ENGINE_H_
#define OBRAIDS_BRAIDS_ENGINE_H_

#include <stddef.h>
#include <stdint.h>
// signature_waveshaper.h calls abs() without including anything that
// declares it; the firmware's braids.cc has <algorithm> in front of it.
#include <stdlib.h>

#include "braids/envelope.h"
#include "braids/macro_oscillator.h"
#include "braids/signature_waveshaper.h"
#include "braids/vco_jitter_source.h"
#include "braids/resampler.h"
#include "wrapper/note_stack.h"

namespace obraids {

// Braids' own settings, in the hardware's own units -- nothing is
// normalised. ad_decay = 5 means what DECAY 5 means on the module's menu.
struct BraidsSettings {
  uint8_t shape;        // WAVE: 0..46 (MACRO_OSC_SHAPE_CSAW .. _DIGITAL_MODULATION)
  float timbre;         // TIMBRE pot, 0..1 (-> 0..32767)
  float color;          // COLOR pot, 0..1 (-> 0..32767)
  uint8_t ad_attack;    // \ATT 0..15
  uint8_t ad_decay;     // \DEC 0..15
  uint8_t ad_fm;        // \FM  0..15
  uint8_t ad_timbre;    // \TIM 0..15
  uint8_t ad_color;     // \COL 0..15
  uint8_t ad_vca;       // \VCA 0/1
  int8_t pitch;         // coarse transpose, semitones -24..24
  int8_t fine;          // FINE pot, cents -100..100 (= the pot's +-1 semitone)
  uint8_t resolution;   // BITS 0..6 (6 = 16 bit)
  uint8_t sample_rate;  // RATE 0..6 (6 = 96 kHz)
  uint8_t vco_drift;    // DRFT 0..4
  uint8_t vco_flatten;  // FLAT 0/1
  uint8_t signature;    // SIGN 0..4
  // Wrapper-level (not Braids settings):
  float level;          // output level 0..1
  uint8_t note_off;     // BraidsNoteOff
};

class BraidsEngine {
 public:
  void Init();

  const BraidsSettings &settings() const { return s_; }
  // id is a ParamId; value already sanitized.
  void SetParam(int id, float value);
  float GetParam(int id) const;

  void NoteOn(uint8_t note, uint8_t velocity);
  void NoteOff(uint8_t note);
  // Releases every held note (the gate follows note_off, as for a note-off).
  void AllNotesOff();
  // All Sound Off: closes the output gate whatever note_off says, until the
  // next note-on. A 3 ms ramp, never a jump.
  void Silence();
  void SetPitchBend(float semitones);

  // Mono, 44.1 kHz.
  void Render(float *out, size_t frames);

  bool key_held() const { return !notes_.empty(); }

 private:
  void RenderHardwareBlock();  // one braids.cc RenderBlock(): 24 samples @ 96k
  float GateTarget() const;

  BraidsSettings s_;

  braids::MacroOscillator osc_;
  braids::Envelope envelope_;
  braids::VcoJitterSource jitter_;
  braids::SignatureWaveshaper ws_;
  uint16_t gain_lp_;         // braids.cc: static uint16_t gain_lp
  bool trigger_flag_;        // braids.cc: trigger_flag (set by a note-on)
  bool sync_pending_;        // a trigger edge for the sync buffer

  int16_t block_[24];
  uint8_t sync_[24];
  Resampler96To44 resampler_;

  NoteStack notes_;
  int32_t note_pitch_;       // 1/128 semitone, MIDI numbering (69 << 7 = A440)
  int32_t bend_;             // 1/128 semitone

  float gate_;               // wrapper output gate, 0..1
  float level_;              // smoothed s_.level
  bool forced_off_;          // Silence()/Init(): gate shut until a note
  float dc_x1_, dc_y1_;      // DC blocker
};

}  // namespace obraids

#endif  // OBRAIDS_BRAIDS_ENGINE_H_
