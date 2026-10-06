// oBraids - parameter table. MIT.
#include "wrapper/params.h"

#include <math.h>
#include <string.h>

namespace obraids {

// Ranges are the hardware's (braids/settings.cc, Settings::metadata_): AD
// values 0..15, BITS/RATE 0..6 with 6 = 16 bit / 96 kHz, DRFT/SIGN 0..4.
// Defaults are the hardware's kInitSettings where it has one (DECAY = 5,
// everything else off); TIMBRE and COLOR are panel pots and start at noon.
// CUTOFF / RESO are the wrapper's low-pass (obraids.cpp), open by default;
// "spare" is a knob slot that does nothing (the grid cannot leave a gap).
// PITCH is a scale-note index (scale.h): its table max is the chromatic top
// (60); the served chain_params carry the current scale's. 14 = C3 in C
// Phrygian.
//
// FORMAT IS PARSED by scripts/metadata.py: one row per line, in this order.
const ParamInfo kParams[P_COUNT] = {
  {"level",     PT_FLOAT,   0.0f,    1.0f,  0.5f},
  {"model",     PT_ENUM,    0.0f,   46.0f,  0.0f},
  {"timbre",    PT_FLOAT,   0.0f,    1.0f,  0.5f},
  {"color",     PT_FLOAT,   0.0f,    1.0f,  0.5f},
  {"cutoff",    PT_FLOAT,   0.0f,    1.0f,  1.0f},
  {"reso",      PT_FLOAT,   0.0f,    1.0f,  0.0f},
  {"spare",     PT_ENUM,    0.0f,    0.0f,  0.0f},
  {"pitch",     PT_INT,     0.0f,   60.0f, 14.0f},
  {"attack",    PT_INT,     0.0f,   15.0f,  0.0f},
  {"decay",     PT_INT,     0.0f,   15.0f,  5.0f},
  {"ad_fm",     PT_INT,     0.0f,   15.0f,  0.0f},
  {"ad_timbre", PT_INT,     0.0f,   15.0f,  0.0f},
  {"ad_color",  PT_INT,     0.0f,   15.0f,  0.0f},
  {"ad_vca",    PT_ENUM,    0.0f,    1.0f,  0.0f},
  {"coarse",    PT_INT,   -24.0f,   24.0f,  0.0f},
  {"fine",      PT_INT,  -100.0f,  100.0f,  0.0f},
  {"bits",      PT_ENUM,    0.0f,    6.0f,  6.0f},
  {"rate",      PT_ENUM,    0.0f,    6.0f,  6.0f},
  {"drift",     PT_ENUM,    0.0f,    4.0f,  0.0f},
  {"flat",      PT_ENUM,    0.0f,    1.0f,  0.0f},
  {"sign",      PT_ENUM,    0.0f,    4.0f,  0.0f},
  {"noteoff",   PT_ENUM,    0.0f,    2.0f,  0.0f},
  {"root",      PT_ENUM,    0.0f,   11.0f,  0.0f},
  {"scale",     PT_ENUM,    0.0f,   11.0f,  0.0f},
  {"steps",     PT_ENUM,    0.0f,    1.0f,  0.0f},
};

int FindParam(const char *key) {
  if (!key) return -1;
  for (int i = 0; i < P_COUNT; ++i) {
    if (!strcmp(key, kParams[i].key)) return i;
  }
  return -1;
}

bool SanitizeParam(int id, float *value) {
  if (id < 0 || id >= P_COUNT || !value || !isfinite(*value)) return false;
  const ParamInfo &p = kParams[id];
  float v = *value;
  if (p.type != PT_FLOAT) v = floorf(v + 0.5f);
  if (v < p.min) v = p.min;
  if (v > p.max) v = p.max;
  *value = v;
  return true;
}

}  // namespace obraids
