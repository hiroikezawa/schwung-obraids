// oBraids - parameter table. MIT.
//
// METADATA ONLY: the values live in BraidsEngine (BraidsSettings). This table
// names each key, its range and its default. module.json's chain_params is
// cross-checked against it at build time (scripts/metadata.py), so the
// host's idea of a key can never drift from what the DSP does with it.
#ifndef OBRAIDS_PARAMS_H_
#define OBRAIDS_PARAMS_H_

#include <stdint.h>

namespace obraids {

enum ParamType { PT_FLOAT = 0, PT_INT = 1, PT_ENUM = 2 };

struct ParamInfo {
  const char *key;   // set_param / get_param key, and the key in the state blob
  uint8_t type;
  float min, max, def;
};

// In the knob grid's page order (module.json's chain_params must match):
//   main    LEVEL MODEL TIMBRE COLOR CUTOFF RESO (spare) PITCH
//   AD      ATTACK DECAY AD>FM AD>TIMBRE AD>COLOR AD>VCA COARSE FINE
//   Braids  BITS RATE DRIFT FLAT SIGN NOTE-OFF
//   Scale   ROOT SCALE STEPS
// Braids' own settings are in the hardware's own units (DECAY 5 = 5).
enum ParamId {
  P_LEVEL, P_MODEL, P_TIMBRE, P_COLOR, P_CUTOFF, P_RESO, P_SPARE, P_PITCH,
  P_ATTACK, P_DECAY, P_AD_FM, P_AD_TIMBRE, P_AD_COLOR, P_AD_VCA, P_COARSE, P_FINE,
  P_BITS, P_RATE, P_DRIFT, P_FLAT, P_SIGN, P_NOTEOFF,
  P_ROOT, P_SCALE, P_STEPS,
  P_COUNT
};

extern const ParamInfo kParams[P_COUNT];

int FindParam(const char *key);

// Clamp to range; integer types are also rounded. Returns false for NaN/Inf.
bool SanitizeParam(int id, float *value);

// Braids has no gate; how a note-off is handled is a wrapper setting.
// See BraidsEngine::GateTarget().
enum BraidsNoteOff { NOTEOFF_AUTO = 0, NOTEOFF_DRONE = 1, NOTEOFF_GATE = 2 };

}  // namespace obraids

#endif  // OBRAIDS_PARAMS_H_
