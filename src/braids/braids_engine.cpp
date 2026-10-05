// oBraids - Braids engine. MIT.
//
// RenderHardwareBlock() is braids.cc's RenderBlock() (pichenettes/eurorack,
// MIT, Copyright 2012 Emilie Gillet) line for line, with each hardware input
// replaced by what stands in for it here. The expressions and their integer
// types are kept exactly as the firmware writes them -- the unsigned
// arithmetic in "ad_value * setting >> 5" is part of the sound.
#include "braids/braids_engine.h"

#include <math.h>
#include <string.h>

#include "braids/resources.h"
#include "stmlib/utils/dsp.h"
#include "wrapper/params.h"

namespace obraids {

using braids::ENV_SEGMENT_ATTACK;
using braids::ENV_SEGMENT_DEAD;

static_assert(braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META == 46,
              "model's range (0..46) is the WAVE menu: CSAW .. QPSK");

namespace {

// braids.cc
const size_t kBlockSize = 24;
const uint16_t bit_reduction_masks[] = {
    0xc000, 0xe000, 0xf000, 0xf800, 0xff00, 0xfff0, 0xffff };
const uint16_t decimation_factors[] = { 24, 12, 6, 4, 3, 2, 1 };

// The signature waveshaper is seeded from the STM32's unique ID on the
// hardware, so every module has its own. One fixed seed here: a preset then
// sounds the same on every Move.
const uint32_t kSignatureSeed = 0x2A5F18C3u;

// Wrapper output stage, 44.1 kHz.
const float kGateAttackStep = 1.0f / 44.1f;   // 1 ms linear open
const float kGateReleaseCoef = 0.00377f;      // tau 6 ms
const float kGateSilenceCoef = 0.00753f;      // tau 3 ms (All Sound Off)
const float kLevelCoef = 0.0015f;             // ~15 ms level smoothing
const float kDcR = 0.99887f;                  // DC blocker, ~8 Hz

// TIMBRE / COLOR pots. On the hardware the pot and CV are summed and read
// as a 12-bit code; settings.adc_to_parameter() with the factory scale
// (32768) and offset (0) turns full travel into 0..32767.
inline int32_t PotToParameter(float pot) {
  int32_t v = static_cast<int32_t>(lroundf(pot * 32767.0f));
  return v < 0 ? 0 : (v > 32767 ? 32767 : v);
}

}  // namespace

void BraidsEngine::Init() {
  // The firmware's objects are globals, i.e. zero-initialised before Init();
  // several Init()s rely on that (Envelope leaves segment_/value_ alone).
  memset(static_cast<void *>(this), 0, sizeof(*this));
  for (int id = 0; id < P_COUNT; ++id) SetParam(id, kParams[id].def);
  osc_.Init();
  envelope_.Init();
  envelope_.Trigger(ENV_SEGMENT_DEAD);
  jitter_.Init();
  ws_.Init(kSignatureSeed);
  resampler_.Reset();
  note_pitch_ = 60 << 7;
  level_ = s_.level;
  forced_off_ = true;       // silent until the first note, whatever note_off says
}

void BraidsEngine::SetParam(int id, float v) {
  const int i = static_cast<int>(v);
  switch (id) {
    case P_MODEL: s_.shape = static_cast<uint8_t>(i); break;
    case P_TIMBRE: s_.timbre = v; break;
    case P_COLOR: s_.color = v; break;
    case P_ATTACK: s_.ad_attack = static_cast<uint8_t>(i); break;
    case P_DECAY: s_.ad_decay = static_cast<uint8_t>(i); break;
    case P_AD_FM: s_.ad_fm = static_cast<uint8_t>(i); break;
    case P_AD_TIMBRE: s_.ad_timbre = static_cast<uint8_t>(i); break;
    case P_AD_COLOR: s_.ad_color = static_cast<uint8_t>(i); break;
    case P_AD_VCA: s_.ad_vca = static_cast<uint8_t>(i); break;
    case P_COARSE: s_.pitch = static_cast<int8_t>(i); break;
    case P_FINE: s_.fine = static_cast<int8_t>(i); break;
    case P_BITS: s_.resolution = static_cast<uint8_t>(i); break;
    case P_RATE: s_.sample_rate = static_cast<uint8_t>(i); break;
    case P_DRIFT: s_.vco_drift = static_cast<uint8_t>(i); break;
    case P_FLAT: s_.vco_flatten = static_cast<uint8_t>(i); break;
    case P_SIGN: s_.signature = static_cast<uint8_t>(i); break;
    case P_LEVEL: s_.level = v; break;
    case P_NOTEOFF: s_.note_off = static_cast<uint8_t>(i); break;
    default: break;
  }
}

float BraidsEngine::GetParam(int id) const {
  switch (id) {
    case P_MODEL: return s_.shape;
    case P_TIMBRE: return s_.timbre;
    case P_COLOR: return s_.color;
    case P_ATTACK: return s_.ad_attack;
    case P_DECAY: return s_.ad_decay;
    case P_AD_FM: return s_.ad_fm;
    case P_AD_TIMBRE: return s_.ad_timbre;
    case P_AD_COLOR: return s_.ad_color;
    case P_AD_VCA: return s_.ad_vca;
    case P_COARSE: return s_.pitch;
    case P_FINE: return s_.fine;
    case P_BITS: return s_.resolution;
    case P_RATE: return s_.sample_rate;
    case P_DRIFT: return s_.vco_drift;
    case P_FLAT: return s_.vco_flatten;
    case P_SIGN: return s_.signature;
    case P_LEVEL: return s_.level;
    case P_NOTEOFF: return s_.note_off;
    default: return 0.0f;
  }
}

// A note-on is what a trigger at the TRIG input is on the hardware: the
// oscillator is struck and the AD envelope restarts from where it is. The
// pitch is what the V/OCT input would carry. Braids has no velocity input,
// so velocity is not used.
void BraidsEngine::NoteOn(uint8_t note, uint8_t velocity) {
  (void)velocity;
  notes_.Push(note);
  note_pitch_ = static_cast<int32_t>(note) << 7;
  trigger_flag_ = true;
  sync_pending_ = true;
  forced_off_ = false;
}

// Releasing the newest note with others still held returns to the previous
// one WITHOUT a trigger -- the pitch CV moves, the gate does not.
void BraidsEngine::NoteOff(uint8_t note) {
  if (!notes_.Remove(note)) return;
  if (!notes_.empty()) note_pitch_ = static_cast<int32_t>(notes_.top()) << 7;
}

void BraidsEngine::AllNotesOff() { notes_.Clear(); }

void BraidsEngine::Silence() {
  notes_.Clear();
  forced_off_ = true;
}

void BraidsEngine::SetPitchBend(float semitones) {
  bend_ = static_cast<int32_t>(lroundf(semitones * 128.0f));
}

float BraidsEngine::GateTarget() const {
  if (forced_off_) return 0.0f;
  // AUTO: with AD->VCA on, the AD envelope IS the amplitude, exactly as on
  // the hardware, and a note-off changes nothing. With it off the hardware
  // would drone forever, so the gate follows the keys instead.
  const bool gated = s_.note_off == NOTEOFF_GATE ||
                     (s_.note_off == NOTEOFF_AUTO && !s_.ad_vca);
  if (!gated) return 1.0f;
  return notes_.empty() ? 0.0f : 1.0f;
}

void BraidsEngine::RenderHardwareBlock() {
  envelope_.Update(s_.ad_attack * 8, s_.ad_decay * 8);
  uint32_t ad_value = envelope_.Render();

  // META off, no easter egg: the shape is the WAVE setting.
  osc_.set_shape(static_cast<braids::MacroOscillatorShape>(s_.shape));

  // Set timbre and color: CV + internal modulation. (There is no CV input,
  // so "CV" is the pot alone.)
  uint16_t parameters[2];
  for (uint16_t i = 0; i < 2; ++i) {
    int32_t value = PotToParameter(i == 0 ? s_.timbre : s_.color);
    uint8_t ad_mod_setting = i == 0 ? s_.ad_timbre : s_.ad_color;
    value += ad_value * ad_mod_setting >> 5;
    CONSTRAIN(value, 0, 32767);
    parameters[i] = value;
  }
  osc_.set_parameters(parameters[0], parameters[1]);

  // quantizer.Process(settings.adc_to_pitch(adc.channel(2)), ...) with the
  // quantizer off is the V/OCT pitch itself: here the MIDI note, the coarse
  // transpose and the pitch bend. The FM CV input is absent (adc_to_fm = 0).
  int32_t pitch = note_pitch_ + static_cast<int32_t>(s_.pitch) * 128 + bend_;

  pitch += jitter_.Render(s_.vco_drift);
  // internal_adc.value() >> 8: the FINE pot, +-128 (+-1 semitone).
  pitch += static_cast<int32_t>(s_.fine) * 128 / 100;
  pitch += ad_value * s_.ad_fm >> 7;

  if (pitch > 16383) {
    pitch = 16383;
  } else if (pitch < 0) {
    pitch = 0;
  }

  if (s_.vco_flatten) {
    pitch = stmlib::Interpolate88(braids::lut_vco_detune, pitch << 2);
  }
  // settings.pitch_transposition() is 0 at OCTV 0 / RANG EXT.; the coarse
  // transpose is applied above, where the hardware's COARSE pot sits.
  osc_.set_pitch(pitch);

  if (trigger_flag_) {
    osc_.Strike();
    envelope_.Trigger(ENV_SEGMENT_ATTACK);
    trigger_flag_ = false;
  }

  // The sync buffer carries the trigger input's edges (sample-accurate on
  // the hardware; the block's first sample here). The firmware drops them
  // whenever any AD modulation is in use.
  memset(sync_, 0, kBlockSize);
  if (sync_pending_) {
    sync_[0] = 1;
    sync_pending_ = false;
  }
  if (s_.ad_vca != 0 || s_.ad_timbre != 0 || s_.ad_color != 0 ||
      s_.ad_fm != 0) {
    memset(sync_, 0, kBlockSize);
  }

  osc_.Render(sync_, block_, kBlockSize);

  // Copy to DAC buffer with sample rate and bit reduction applied.
  int16_t held_sample = 0;
  size_t decimation_factor = decimation_factors[s_.sample_rate];
  uint16_t bit_mask = bit_reduction_masks[s_.resolution];
  int32_t gain = s_.ad_vca ? ad_value : 65535;
  uint16_t signature = s_.signature * s_.signature * 4095;
  for (size_t i = 0; i < kBlockSize; ++i) {
    if ((i % decimation_factor) == 0) {
      held_sample = block_[i] & bit_mask;
    }
    int16_t sample = held_sample * gain_lp_ >> 16;
    gain_lp_ += (gain - gain_lp_) >> 4;
    int16_t warped = ws_.Transform(sample);
    block_[i] = stmlib::Mix(sample, warped, signature);
  }
}

void BraidsEngine::Render(float *out, size_t frames) {
  // dac.Write(-sample + 32768): the hardware's output is inverted.
  const float kScale = -1.0f / 32768.0f;
  const float target_level = s_.level;
  for (size_t n = 0; n < frames; ++n) {
    while (resampler_.NeedsInput()) {
      RenderHardwareBlock();
      resampler_.Push(block_, kBlockSize, kScale);
    }
    const float x = resampler_.Next();
    const float y = x - dc_x1_ + kDcR * dc_y1_;
    dc_x1_ = x;
    dc_y1_ = y;

    const float target = GateTarget();
    if (target > gate_) {
      gate_ += kGateAttackStep;
      if (gate_ > target) gate_ = target;
    } else if (target < gate_) {
      gate_ += (target - gate_) * (forced_off_ ? kGateSilenceCoef : kGateReleaseCoef);
      if (gate_ < 1e-5f) gate_ = 0.0f;
    }
    level_ += (target_level - level_) * kLevelCoef;
    out[n] = y * gate_ * level_;
  }
  if (fabsf(dc_y1_) < 1e-20f) dc_y1_ = 0.0f;
}

}  // namespace obraids
