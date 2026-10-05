// oBraids - engine tests (native, ASan/UBSan). MIT.
//
// What is claimed in the README is asserted here:
//   - pitch is exact after the 96k -> 44.1k resampler (coarse and fine too)
//   - AD envelope times are the hardware's (compared against the firmware's
//     own Envelope class, clocked the way braids.cc clocks it)
//   - AD->FM raises the pitch at the start of the note by the hardware amount
//   - every one of the 47 models renders finite, bounded audio and stops
//   - the NOTE OFF modes, CC 120 / 123, velocity-0 note-off, pitch bend
//   - the state blob restores exactly (DECAY 5 / FM 10 stored as 5 / 10)
//   - the auto 1/16: plays with the transport and only then, the 16 trigger
//     pads, PITCH is a note of the scale, touch motion records under a finger
//     and plays back as an override (never a knob write), a touch nobody
//     restates lets go, RANDOM writes a C Phrygian phrase into PITCH, a
//     root / scale change re-snaps it, and nothing is ever left sounding
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <vector>

#include "braids/envelope.h"
#include "wrapper/obraids.h"
#include "wrapper/scale.h"

using namespace obraids;

static int g_failures = 0;
#define CHECK(cond, ...)                                   \
  do {                                                     \
    if (!(cond)) {                                         \
      ++g_failures;                                        \
      printf("FAIL %s:%d: ", __FILE__, __LINE__);          \
      printf(__VA_ARGS__);                                 \
      printf("\n");                                        \
    }                                                      \
  } while (0)

static const float kFs = 44100.0f;
static OBraids g_synth;   // ~20 KB: static, like the plugin's pool

// Transport stopped: the phrase is silent, only MIDI plays the voice.
static std::vector<float> Render(OBraids &e, float seconds) {
  std::vector<float> out(static_cast<size_t>(seconds * kFs));
  size_t n = 0;
  while (n < out.size()) {
    size_t m = out.size() - n;
    if (m > 128) m = 128;
    e.Process(&out[n], m, -1.0, 120.0f, true);
    n += m;
  }
  return out;
}

// Transport running at `bpm` from beat `*beat`, which is advanced.
static std::vector<float> RenderPlaying(OBraids &e, float seconds, double *beat, float bpm = 120.0f) {
  std::vector<float> out(static_cast<size_t>(seconds * kFs));
  size_t n = 0;
  while (n < out.size()) {
    size_t m = out.size() - n;
    if (m > 128) m = 128;
    e.Process(&out[n], m, *beat, bpm, true);
    *beat += m / kFs * bpm / 60.0;
    n += m;
  }
  return out;
}

static float Peak(const std::vector<float> &x, size_t from = 0, size_t to = 0) {
  if (to == 0 || to > x.size()) to = x.size();
  float p = 0.0f;
  for (size_t i = from; i < to; ++i) p = fmaxf(p, fabsf(x[i]));
  return p;
}

static float MaxStep(const std::vector<float> &x, size_t from, size_t to) {
  float m = 0.0f;
  for (size_t i = from + 1; i < to && i < x.size(); ++i) m = fmaxf(m, fabsf(x[i] - x[i - 1]));
  return m;
}

// Frequency by rising zero crossings of the DC-removed signal, over [from, to).
static float Frequency(const std::vector<float> &x, size_t from, size_t to) {
  double mean = 0.0;
  for (size_t i = from; i < to; ++i) mean += x[i];
  mean /= static_cast<double>(to - from);
  int first = -1, last = -1, count = 0;
  for (size_t i = from + 1; i < to; ++i) {
    if (x[i - 1] - mean < 0.0 && x[i] - mean >= 0.0) {
      if (first < 0) first = static_cast<int>(i);
      last = static_cast<int>(i);
      ++count;
    }
  }
  if (count < 2) return 0.0f;
  return static_cast<float>((count - 1) * kFs / static_cast<float>(last - first));
}

static bool AllFinite(const std::vector<float> &x) {
  for (float v : x) if (!isfinite(v)) return false;
  return true;
}

static void Send(OBraids &e, uint8_t a, uint8_t b, uint8_t c) {
  const uint8_t msg[3] = { a, b, c };
  e.Midi(msg, 3);
}
static void NoteOn(OBraids &e, int note, int vel = 100) { Send(e, 0x90, note, vel); }
static void NoteOff(OBraids &e, int note) { Send(e, 0x80, note, 0); }
static void Set(OBraids &e, ParamId id, float v) { e.SetParam(id, v); }
static void Fresh(OBraids &e) { e.Init(12345u); }

// ---------------------------------------------------------------------------

static void TestPitch() {
  const int notes[] = { 45, 57, 69, 81 };
  for (int note : notes) {
    Fresh(g_synth);
    Set(g_synth, P_MODEL, 0);           // CSAW
    Set(g_synth, P_TIMBRE, 0.0f);
    NoteOn(g_synth, note);
    std::vector<float> x = Render(g_synth, 1.2f);
    const float want = 440.0f * powf(2.0f, (note - 69) / 12.0f);
    const float got = Frequency(x, 4410, x.size());
    CHECK(fabsf(got / want - 1.0f) < 0.002f, "note %d: %.3f Hz, want %.3f", note, got, want);
  }
  // Coarse and fine: +12 semitones and +50 cents.
  Fresh(g_synth);
  Set(g_synth, P_TIMBRE, 0.0f);
  Set(g_synth, P_COARSE, 12);
  Set(g_synth, P_FINE, 50);
  NoteOn(g_synth, 57);
  std::vector<float> x = Render(g_synth, 1.2f);
  float want = 440.0f * powf(2.0f, 0.5f / 12.0f);
  float got = Frequency(x, 4410, x.size());
  // FINE is quantised to 1/128 semitone like the hardware pot: 64/128.
  CHECK(fabsf(got / want - 1.0f) < 0.003f, "pitch+fine: %.3f Hz, want %.3f", got, want);
  // Pitch bend: full up = +2 semitones.
  Fresh(g_synth);
  Set(g_synth, P_TIMBRE, 0.0f);
  Send(g_synth, 0xE0, 127, 127);
  NoteOn(g_synth, 57);
  x = Render(g_synth, 1.2f);
  want = 220.0f * powf(2.0f, 2.0f / 12.0f);
  got = Frequency(x, 4410, x.size());
  CHECK(fabsf(got / want - 1.0f) < 0.003f, "bend: %.3f Hz, want %.3f", got, want);
}

// Blocks the firmware's own Envelope takes to fall below `fraction` of full
// scale, clocked once per 24-sample block as braids.cc does.
static int ReferenceBlocksBelow(int attack, int decay, float fraction) {
  static braids::Envelope env;
  memset(static_cast<void *>(&env), 0, sizeof(env));
  env.Init();
  env.Update(attack * 8, decay * 8);
  env.Trigger(braids::ENV_SEGMENT_ATTACK);
  bool peaked = false;
  for (int b = 1; b < 4000 * 20; ++b) {
    env.Update(attack * 8, decay * 8);
    const uint16_t v = env.Render();
    if (v > 60000) peaked = true;
    if (peaked && v < fraction * 65535.0f) return b;
  }
  return -1;
}

static void TestEnvelopeTimes() {
  const int decays[] = { 0, 5, 9 };
  for (int decay : decays) {
    Fresh(g_synth);
    Set(g_synth, P_MODEL, 3);          // FOLD: a steady, full-level waveform
    Set(g_synth, P_TIMBRE, 0.0f);
    Set(g_synth, P_AD_VCA, 1);
    Set(g_synth, P_ATTACK, 0);
    Set(g_synth, P_DECAY, decay);
    Set(g_synth, P_LEVEL, 1.0f);
    Render(g_synth, 0.1f);             // let LEVEL's smoothing settle first
    NoteOn(g_synth, 60);
    std::vector<float> x = Render(g_synth, 6.0f);
    const float peak = Peak(x);
    const float fraction = 0.05f;
    int last = -1;
    for (size_t i = 0; i < x.size(); ++i) if (fabsf(x[i]) > fraction * peak) last = static_cast<int>(i);
    const int blocks = ReferenceBlocksBelow(0, decay, fraction);
    const float want = blocks * 24.0f / 96000.0f;
    const float got = last / kFs;
    // Resampler latency is 0.33 ms; the waveform's own peak spacing adds
    // up to one period (~3.8 ms at C4).
    CHECK(blocks > 0 && fabsf(got - want) < 0.005f + 0.02f * want,
          "DECAY %d: sound ends at %.4f s, hardware envelope at %.4f s", decay, got, want);
  }
}

static void TestAdFm() {
  // DECAY 5, FM 10: the pitch starts 40 semitones up (ad_value * 10 >> 7 =
  // 5119/128 at the envelope's peak) and falls back to the note.
  Fresh(g_synth);
  Set(g_synth, P_MODEL, 3);
  Set(g_synth, P_TIMBRE, 0.0f);
  Set(g_synth, P_DECAY, 5);
  Set(g_synth, P_AD_FM, 10);
  NoteOn(g_synth, 36);
  std::vector<float> x = Render(g_synth, 1.5f);
  const float early = Frequency(x, 44, 441);              // 1 .. 10 ms
  const float late = Frequency(x, 44100, x.size());
  const float note = 440.0f * powf(2.0f, (36 - 69) / 12.0f);
  CHECK(fabsf(late / note - 1.0f) < 0.003f, "AD>FM: settles at %.2f Hz, want %.2f", late, note);
  CHECK(early > note * 6.0f, "AD>FM 10: early pitch %.1f Hz is not far above %.1f", early, note);
  CHECK(g_synth.GetParam(P_DECAY) == 5.0f && g_synth.GetParam(P_AD_FM) == 10.0f,
        "DECAY/FM read back in hardware units");
}

static void TestModels() {
  for (int model = 0; model <= 46; ++model) {
    Fresh(g_synth);
    Set(g_synth, P_MODEL, static_cast<float>(model));
    Set(g_synth, P_TIMBRE, 0.7f);
    Set(g_synth, P_COLOR, 0.3f);
    Set(g_synth, P_DECAY, 7);
    Set(g_synth, P_AD_TIMBRE, 6);
    Set(g_synth, P_AD_COLOR, 4);
    NoteOn(g_synth, 48);
    std::vector<float> x = Render(g_synth, 0.4f);
    NoteOff(g_synth, 48);
    std::vector<float> y = Render(g_synth, 0.2f);
    CHECK(AllFinite(x) && AllFinite(y), "model %d: non-finite output", model);
    CHECK(Peak(x) <= 1.0f, "model %d: peak %.3f", model, Peak(x));
    CHECK(Peak(x) > 0.01f, "model %d: silent (peak %.5f)", model, Peak(x));
    // AUTO note-off with AD->VCA off: the gate closes.
    CHECK(Peak(y, y.size() - 2205) < 1e-4f, "model %d: still sounding after note-off", model);
  }
}

static void TestNoteOffModes() {
  // AUTO + AD->VCA on: a note-off changes nothing, the AD envelope ends it.
  Fresh(g_synth);
  Set(g_synth, P_AD_VCA, 1);
  Set(g_synth, P_DECAY, 9);
  NoteOn(g_synth, 60);
  Render(g_synth, 0.02f);
  NoteOff(g_synth, 60);
  std::vector<float> x = Render(g_synth, 0.1f);
  CHECK(Peak(x, 2205, 4410) > 0.05f, "AUTO/VCA: a note-off cut the AD envelope");
  // GATE: even with AD->VCA on, the note-off closes the output.
  Fresh(g_synth);
  Set(g_synth, P_AD_VCA, 1);
  Set(g_synth, P_DECAY, 12);
  Set(g_synth, P_NOTEOFF, NOTEOFF_GATE);
  NoteOn(g_synth, 60);
  Render(g_synth, 0.05f);
  NoteOff(g_synth, 60);
  x = Render(g_synth, 0.1f);
  CHECK(Peak(x, 2205) < 1e-3f, "GATE: still sounding 50 ms after note-off: %.5f", Peak(x, 2205));
  // DRONE: the hardware -- keeps sounding after the note-off...
  Fresh(g_synth);
  Set(g_synth, P_NOTEOFF, NOTEOFF_DRONE);
  NoteOn(g_synth, 60);
  Render(g_synth, 0.05f);
  NoteOff(g_synth, 60);
  x = Render(g_synth, 0.3f);
  CHECK(Peak(x, x.size() - 2205) > 0.1f, "DRONE: silent after note-off");
  // ...but CC 120 silences it, without a click.
  Send(g_synth, 0xB0, 120, 0);
  x = Render(g_synth, 0.1f);
  CHECK(Peak(x, 2205) < 1e-4f, "DRONE: All Sound Off left it sounding");
  CHECK(MaxStep(x, 0, 2205) < 0.5f, "All Sound Off stepped by %.3f", MaxStep(x, 0, 2205));
}

static void TestMidiReleases() {
  // Velocity-0 note-on is a note-off.
  Fresh(g_synth);
  NoteOn(g_synth, 60);
  Render(g_synth, 0.05f);
  Send(g_synth, 0x90, 60, 0);
  std::vector<float> x = Render(g_synth, 0.1f);
  CHECK(Peak(x, 2205) < 1e-3f, "velocity-0 note-on did not release: %.5f", Peak(x, 2205));
  // CC 123 releases every held key.
  Fresh(g_synth);
  NoteOn(g_synth, 60);
  NoteOn(g_synth, 64);
  Render(g_synth, 0.05f);
  Send(g_synth, 0xB0, 123, 0);
  x = Render(g_synth, 0.1f);
  CHECK(Peak(x, 2205) < 1e-3f, "CC 123 did not release: %.5f", Peak(x, 2205));
  // Legato: releasing the newest key returns to the held one, still sounding.
  Fresh(g_synth);
  Set(g_synth, P_TIMBRE, 0.0f);
  NoteOn(g_synth, 45);
  NoteOn(g_synth, 57);
  Render(g_synth, 0.1f);
  NoteOff(g_synth, 57);
  x = Render(g_synth, 1.0f);
  const float got = Frequency(x, 4410, x.size());
  CHECK(fabsf(got / 110.0f - 1.0f) < 0.003f, "legato return: %.2f Hz, want 110", got);
  NoteOff(g_synth, 45);
  x = Render(g_synth, 0.1f);
  CHECK(Peak(x, 2205) < 1e-3f, "stuck after both keys released");
}

// ---------------------------------------------------------------------------

static char g_state_a[4096], g_state_b[4096];
static OBraids g_other;

static void TestStateRoundTrip() {
  Fresh(g_synth);
  Set(g_synth, P_MODEL, 32);        // BELL
  Set(g_synth, P_TIMBRE, 0.45f);
  Set(g_synth, P_COLOR, 0.62f);
  Set(g_synth, P_DECAY, 5);
  Set(g_synth, P_AD_FM, 10);
  const int n = g_synth.WriteState(g_state_a, sizeof(g_state_a));
  CHECK(n > 0, "WriteState");
  printf("  state: %s\n", g_state_a);
  CHECK(strstr(g_state_a, "\"decay\":5,") != NULL, "DECAY 5 not stored as the integer 5");
  CHECK(strstr(g_state_a, "\"ad_fm\":10,") != NULL, "FM 10 not stored as the integer 10");
  CHECK(strstr(g_state_a, "\"model\":32,") != NULL, "BELL not stored as model 32");
  g_other.Init(777u);
  CHECK(g_other.ReadState(g_state_a), "ReadState");
  g_other.WriteState(g_state_b, sizeof(g_state_b));
  CHECK(!strcmp(g_state_a, g_state_b), "round trip differs:\n  %s\n  %s", g_state_a, g_state_b);

  // Robustness: whitespace, missing keys keep their value, junk rejected.
  g_other.Init(777u);
  CHECK(g_other.ReadState(" { \"v\" : 1 , \"decay\" : 9 } "), "pretty JSON");
  CHECK(g_other.GetParam(P_DECAY) == 9.0f && g_other.GetParam(P_AD_FM) == 0.0f, "partial state");
  CHECK(!g_other.ReadState("{\"v\":3,\"decay\":3}"), "future version accepted");
  CHECK(g_other.GetParam(P_DECAY) == 9.0f, "rejected state still applied");
  CHECK(!g_other.ReadState("{\"decay\":3"), "truncated state accepted");
  g_other.ReadState("{\"decay\":99,\"model\":-4,\"timbre\":1e9}");
  CHECK(g_other.GetParam(P_DECAY) == 15.0f && g_other.GetParam(P_MODEL) == 0.0f &&
        g_other.GetParam(P_TIMBRE) == 1.0f, "out-of-range values not clamped");
  // A key inside a nested object never answers for the top level.
  g_other.Init(777u);
  g_other.ReadState("{\"x\":{\"decay\":1}}");
  CHECK(g_other.GetParam(P_DECAY) == 5.0f, "nested key leaked into DECAY");
}


// ---------------------------------------------------------------------------
// The auto 1/16, the pads and the motion.

static bool InPhrygian(int midi) {
  const int pc = midi % 12;
  return pc == 0 || pc == 1 || pc == 3 || pc == 5 || pc == 7 || pc == 8 || pc == 10;
}

static void TestAutoSixteenths() {
  // Silent while stopped, sounding while the transport runs (no MIDI at all).
  Fresh(g_synth);
  std::vector<float> x = Render(g_synth, 0.5f);
  CHECK(Peak(x) == 0.0f, "sounded with the transport stopped");
  double beat = 0.0;
  x = RenderPlaying(g_synth, 1.0f, &beat);
  CHECK(Peak(x) > 0.05f, "silent with the transport running");
  // PITCH is the note: G2 = 98 Hz (MIDI 43), which is in C Phrygian.
  Fresh(g_synth);
  Set(g_synth, P_TIMBRE, 0.0f);
  Set(g_synth, P_PITCH, static_cast<float>(PitchIndexOfMidi(43.0f, 0, 0)));
  beat = 0.0;
  x = RenderPlaying(g_synth, 2.0f, &beat);
  // Measured INSIDE one 1/16 (5512 samples at 120 BPM): every hit resets the
  // oscillator's phase (Braids' sync, as on the hardware), so a count across
  // hits would read low.
  const float got = Frequency(x, 8 * 5513 + 300, 9 * 5512 - 100);
  CHECK(fabsf(got / 98.0f - 1.0f) < 0.01f, "PITCH G2: %.2f Hz, want 98", got);
  // The 16 trigger pads: all off is silence (after the held step ends).
  for (int i = 0; i < 16; ++i) g_synth.ToggleGate(i);
  CHECK(g_synth.gates() == 0, "gates %04x", g_synth.gates());
  x = RenderPlaying(g_synth, 0.5f, &beat);
  CHECK(Peak(x, 8820) < 1e-3f, "all trigger pads off still sounds: %.5f", Peak(x, 8820));
  // One pad on: sound again.
  g_synth.ToggleGate(0);
  x = RenderPlaying(g_synth, 1.0f, &beat);
  CHECK(Peak(x) > 0.05f, "a single trigger pad does not play");
  // Stop: silent, nothing held.
  x = Render(g_synth, 0.2f);
  CHECK(Peak(x, 4410) < 1e-3f, "Stop left a note sounding: %.5f", Peak(x, 4410));
  CHECK(!g_synth.key_held(), "Stop left a key held");
}

static void TestHitsLandOnTheSixteenth() {
  // AD>VCA on, short decay: each hit is a burst; the burst starts on the
  // 1/16 (5512.5 samples at 120 BPM) within the resampler + one Braids block.
  Fresh(g_synth);
  Set(g_synth, P_AD_VCA, 1);
  Set(g_synth, P_DECAY, 3);
  Set(g_synth, P_MODEL, 3);
  Set(g_synth, P_TIMBRE, 0.0f);
  Set(g_synth, P_LEVEL, 1.0f);
  Render(g_synth, 0.1f);
  double beat = 0.0;
  std::vector<float> x = RenderPlaying(g_synth, 1.0f, &beat);
  int onsets = 0, bad = 0;
  for (int k = 1; k < 7; ++k) {
    const size_t at = static_cast<size_t>(k * 5512.5);
    // silent just before, loud within 2 ms after
    const float before = Peak(x, at - 200, at - 20);
    const float after = Peak(x, at, at + 88);
    ++onsets;
    if (!(before < 0.02f && after > 0.05f)) ++bad;
  }
  CHECK(bad == 0, "%d of %d hits not on their 1/16", bad, onsets);
}

static void TestTouchMotion() {
  Fresh(g_synth);
  double beat = 0.0;
  RenderPlaying(g_synth, 0.1f, &beat);
  // REC off: a touch records nothing.
  g_synth.Touch(P_TIMBRE, true);
  RenderPlaying(g_synth, 1.1f, &beat);
  g_synth.Touch(P_TIMBRE, false);
  CHECK(g_synth.motion_mask() == 0, "recorded with REC off");
  // REC on: one whole loop under the finger at TIMBRE 0.8, restated.
  g_synth.SetWrite(true);
  Set(g_synth, P_TIMBRE, 0.8f);
  for (int i = 0; i < 9; ++i) {
    g_synth.Touch(P_TIMBRE, true);
    RenderPlaying(g_synth, 0.13f, &beat);
    CHECK(i == 0 || (g_synth.motion_recording() & 2u), "not recording under the finger");
  }
  g_synth.Touch(P_TIMBRE, false);
  CHECK(g_synth.motion_mask() == 2u, "timbre lane %x", g_synth.motion_mask());
  // Knob elsewhere: playback overrides it, and never writes it.
  Set(g_synth, P_TIMBRE, 0.2f);
  RenderPlaying(g_synth, 0.2f, &beat);
  CHECK(fabsf(g_synth.Effective(P_TIMBRE) - 0.8f) < 0.01f, "playback %.3f", g_synth.Effective(P_TIMBRE));
  CHECK(g_synth.GetParam(P_TIMBRE) == 0.2f, "playback wrote the knob");
  // Stopped: the knob plays.
  Render(g_synth, 0.05f);
  CHECK(g_synth.Effective(P_TIMBRE) == 0.2f, "stopped still plays the lane");
  // A touch nobody restates lets go by itself (the screen went away).
  beat = 0.0;
  g_synth.Touch(P_COLOR, true);
  RenderPlaying(g_synth, 0.7f, &beat);
  CHECK(!(g_synth.motion_recording() & 4u), "a touch outlived its heartbeat");
  // VOLUME is never a lane; clear one lane, then all.
  g_synth.Touch(P_LEVEL, true);
  CHECK(!(g_synth.motion_mask() & ~6u), "VOLUME recorded");
  g_synth.ClearMotion(P_TIMBRE);
  CHECK(!(g_synth.motion_mask() & 2u), "clear timbre");
  g_synth.ClearMotion(-1);
  CHECK(g_synth.motion_mask() == 0, "clear all");
}

static void TestRandomAndScale() {
  Fresh(g_synth);
  for (int n = 0; n < 50; ++n) {
    g_synth.Randomize();
    CHECK(g_synth.motion_mask() & (1u << 6), "RANDOM left the PITCH lane empty");
    double beat = 0.0;
    std::vector<float> x;
    for (int st = 0; st < 8; ++st) {
      RenderPlaying(g_synth, 0.125f, &beat);
      const int midi = PitchMidi(static_cast<int>(g_synth.Effective(P_PITCH) + 0.5f), 0, 0);
      CHECK(InPhrygian(midi), "RANDOM note %d not in C Phrygian", midi);
      const int knob = PitchMidi(static_cast<int>(g_synth.GetParam(P_PITCH)), 0, 0);
      CHECK(midi >= knob - 7 && midi <= knob + 17, "RANDOM note %d outside the knob's window", midi);
    }
    Render(g_synth, 0.02f);
  }
  // A scale change re-snaps the knob and the lane: C Phrygian -> C Major.
  const int before = PitchMidi(static_cast<int>(g_synth.GetParam(P_PITCH)), 0, 0);
  Set(g_synth, P_SCALE, 3);
  const int after = PitchMidi(static_cast<int>(g_synth.GetParam(P_PITCH)), 0, 3);
  CHECK(abs(after - before) <= 1, "knob jumped %d -> %d on a scale change", before, after);
  double beat = 0.0;
  for (int st = 0; st < 8; ++st) {
    RenderPlaying(g_synth, 0.125f, &beat);
    const int midi = PitchMidi(static_cast<int>(g_synth.Effective(P_PITCH) + 0.5f), 0, 3);
    const int pc = midi % 12;
    CHECK(pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7 || pc == 9 || pc == 11,
          "lane note %d not in C Major after the change", midi);
  }
  // PITCH is clamped to the scale in force.
  Set(g_synth, P_SCALE, 9);                        // minor pentatonic: 26 notes
  Set(g_synth, P_PITCH, 60.0f);
  CHECK(g_synth.GetParam(P_PITCH) == static_cast<float>(PitchCount(0, 9) - 1), "PITCH past the scale");
}

static void TestSixteenStepLoop() {
  Fresh(g_synth);
  Set(g_synth, P_STEPS, 1);
  CHECK(g_synth.steps() == 16, "steps");
  double beat = 0.0;
  int maxstep = -1;
  for (int i = 0; i < 40; ++i) {
    RenderPlaying(g_synth, 0.06f, &beat);
    if (g_synth.motion_step() > maxstep) maxstep = g_synth.motion_step();
  }
  CHECK(maxstep == 15, "16-step loop reached step %d", maxstep + 1);
}

static char g_state_c[16384], g_state_d[16384];
static OBraids g_third;

static void TestMotionState() {
  Fresh(g_synth);
  g_synth.Randomize();
  g_synth.ToggleGate(3);
  Set(g_synth, P_STEPS, 1);
  g_synth.WriteState(g_state_c, sizeof(g_state_c));
  CHECK(strstr(g_state_c, "\"m\":\"pitch:") != NULL, "PITCH lane not saved");
  CHECK(strstr(g_state_c, "\"g\":65527}") != NULL, "gates not saved");
  CHECK(strlen(g_state_c) < 16384, "state too large for MAX_SYNTH_STATE_LEN");
  g_third.Init(1u);
  CHECK(g_third.ReadState(g_state_c), "ReadState");
  g_third.WriteState(g_state_d, sizeof(g_state_d));
  CHECK(!strcmp(g_state_c, g_state_d), "motion round trip differs");
  // 0.2's state: "pitch" was COARSE, "length" the steps; the rest ignored.
  g_third.Init(1u);
  CHECK(g_third.ReadState("{\"v\":1,\"model\":32,\"decay\":5,\"pitch\":12,\"length\":1,"
                          "\"seq_pitch\":3,\"pattern\":\"8|60.100.92\",\"motion\":[1,2]}"),
        "0.2 state refused");
  CHECK(g_third.GetParam(P_MODEL) == 32.0f && g_third.GetParam(P_COARSE) == 12.0f &&
        g_third.GetParam(P_STEPS) == 1.0f && g_third.GetParam(P_PITCH) == kParams[P_PITCH].def,
        "0.2 state mapping");
  // A corrupt lane refuses the motion only; the knobs still load.
  g_third.Init(1u);
  CHECK(g_third.ReadState("{\"v\":2,\"decay\":9,\"m\":\"pitch:!!\",\"g\":1}"), "partial");
  CHECK(g_third.GetParam(P_DECAY) == 9.0f && g_third.motion_mask() == 0, "corrupt lane applied");
}

static void TestNoStuckNotesFuzz() {
  Fresh(g_synth);
  double beat = 0.0;
  unsigned r = 7;
  float buf[128];
  for (int i = 0; i < 3000; ++i) {
    r = r * 1664525u + 1013904223u;
    switch ((r >> 24) % 9) {
      case 0: g_synth.Randomize(); break;
      case 1: g_synth.ToggleGate(static_cast<int>((r >> 8) % 16)); break;
      case 2: Set(g_synth, P_STEPS, static_cast<float>((r >> 9) & 1)); break;
      case 3: Set(g_synth, P_PITCH, static_cast<float>((r >> 10) % 36)); break;
      case 4: g_synth.SetWrite(((r >> 11) & 1) != 0); g_synth.Touch(P_PITCH, ((r >> 12) & 1) != 0); break;
      case 5: beat = (r >> 12) % 64 / 4.0; break;           // relocate / loop back
      case 6: for (int b = 0; b < 3; ++b) g_synth.Process(buf, 128, -1.0, 120.0f, true); break;
      case 7: Set(g_synth, P_SCALE, static_cast<float>((r >> 13) % 12)); break;
      default: break;
    }
    const int n = 1 + static_cast<int>((r >> 16) % 20);
    for (int b = 0; b < n; ++b) {
      g_synth.Process(buf, 128, beat, 120.0f, true);
      beat += 128.0 / kFs * 2.0;
      for (float v : buf) CHECK(isfinite(v), "fuzz: non-finite output");
    }
  }
  std::vector<float> x = Render(g_synth, 0.2f);
  CHECK(!g_synth.key_held(), "fuzz: a key left held after Stop");
  CHECK(Peak(x, 4410) < 1e-3f, "fuzz: sounding after Stop %.5f", Peak(x, 4410));
}

// ---------------------------------------------------------------------------

static double BlockMicros(OBraids &e, int blocks) {
  float buf[128];
  timespec a, b;
  clock_gettime(CLOCK_MONOTONIC, &a);
  for (int i = 0; i < blocks; ++i) e.Process(buf, 128, -1.0, 120.0f, true);
  clock_gettime(CLOCK_MONOTONIC, &b);
  return ((b.tv_sec - a.tv_sec) * 1e9 + (b.tv_nsec - a.tv_nsec)) / 1e3 / blocks;
}

static void ReportCost() {
  const int models[] = { 0, 14, 28, 33, 44 };
  for (int m : models) {
    Fresh(g_synth);
    Set(g_synth, P_MODEL, static_cast<float>(m));
    NoteOn(g_synth, 48);
    printf("  cost (this machine, sanitized): model %2d %.1f us/block\n", m,
           BlockMicros(g_synth, 2000));
  }
}

int main() {
  TestPitch();
  TestEnvelopeTimes();
  TestAdFm();
  TestModels();
  TestNoteOffModes();
  TestMidiReleases();
  TestStateRoundTrip();
  TestAutoSixteenths();
  TestHitsLandOnTheSixteenth();
  TestTouchMotion();
  TestRandomAndScale();
  TestSixteenStepLoop();
  TestMotionState();
  TestNoStuckNotesFuzz();
  ReportCost();
  if (g_failures) {
    printf("%d FAILED\n", g_failures);
    return 1;
  }
  printf("PASS: engine tests\n");
  return 0;
}
