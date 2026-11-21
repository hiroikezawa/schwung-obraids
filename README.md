# oBraids

**English** | [日本語](README.ja.md)

(Module ID: `obraids`)

A Sound Generator for Schwung / Ableton Move. It bundles the DSP code of Mutable
Instruments' official Braids and runs it **at the hardware's 96 kHz in 24-sample blocks**,
resampling to 44.1 kHz. The internal AD envelope and AD→FM / TIMBRE / COLOR / VCA are
the hardware firmware's own calculations, and the settings use **the same units as the
hardware's menu** (DECAY 5 means the same as DECAY 5 on the hardware).

**It has a built-in sequencer.** Press Play on Move and it starts playing on every 1/16
note in time with the tempo, with no clips or MIDI notes needed (an 8- or 16-step loop).

- **Choose the steps on the pads**: the bottom two pad rows switch each step on and off.
- **Record knob motion**: turn on the REC pad (top right) and turn a knob while touching
  it. The motion is recorded into the loop and played back every pass.
- **Generate a melody**: press the RANDOM pad next to REC and a random melody is written
  in as a pitch per step.
- **Pick only the models you use on the knob**: switch on just the ones you want out of
  the 47 models (MODEL), and the MODEL knob steps through only those (open the list with
  the MODELS pad).
- **Pitch stays in the scale**: PITCH (knob 8) only picks notes of the selected scale
  (C Phrygian by default).

---

## Building

This repository builds on its own. The Schwung plugin API header (`plugin_api_v1.h`,
MIT) is bundled in `third_party/schwung/` (unmodified).

```bash
scripts/build.sh arm64
```

→ `dist/obraids-module.tar.gz` (AArch64 `dsp.so` for Move + UI + licences)

Requirements: `aarch64-linux-gnu-g++`, `python3`. During the build `scripts/check_abi.py`
checks that the library is AArch64, needs glibc 2.34 or older, does not depend on
libstdc++, and exports only `move_plugin_init_v2`, and fails the build otherwise.

The full test suite (native ASan/UBSan engine tests, a dlopen smoke test of the real
`dsp.so`, UI tests and finally the arm64 build):

```bash
scripts/test.sh
```

The UI tests need Node.js and a checkout of Schwung itself (for its page engine and
drawing harness): `../schwung` next to this repository, or
`SCHWUNG_ROOT=/path/to/schwung`. If neither is found, only the UI tests are skipped
(on WSL the Windows `node.exe` works too).

## Installing on Move

Built packages are on the GitHub **Releases** page (`obraids-module.tar.gz`); you do not
need to build anything yourself.

Upload through Schwung Manager (`http://<Move's IP>:7700`):

```bash
# install a package downloaded from Releases
scripts/install.py http://move.local:7700 --package obraids-module.tar.gz
# install your own build (dist/)
scripts/install.py http://move.local:7700
```

(`--check` only tests the connection.) Then choose **oBraids** as the Synth of a chain slot.

**Restart Move after an update.** The old `dsp.so` stays loaded in the process, so
re-selecting the slot does not switch to the new version.

## Getting started

The basic flow is: put it in a slot → press Play → shape the sound with the knobs →
build a pattern on the pads → record knob motion with REC.

### 1. Make a sound

1. Open a slot on the Schwung screen (Shift+Vol+Track, or hold a track button) and
   choose **oBraids** as the Synth.
2. Open the slot's Synth (oBraids) to get the oBraids screen.
3. Press **Play** on Move. oBraids **plays by itself on every 1/16 note, in time with the
   transport** (no clips or MIDI notes needed). Nothing plays while stopped.
   MIDI notes from the Move track still play it as before.

### 2. Shape the sound (main page)

| Knobs | | | |
|---|---|---|---|
| 1 VOLUME | 2 TIMBRE | 3 COLOR | 4 CUTOFF |
| 5 RESO | 6 (spare) | 7 MODEL (47 models) | 8 PITCH |

- **MODEL** picks the kind of sound and **TIMBRE / COLOR** shape it (Braids' two pots).
- **CUTOFF / RESO** are a low-pass filter. Lowering CUTOFF muffles the sound; RESO adds
  character.
- **PITCH** picks a note of the scale (set the scale on the Scale page).
- Knob 6 does nothing (safe to touch).
- **Narrow it down to the models you use**: turning through all 47 is tedious, so open the
  list with the MODELS pad (left of RANDOM) and switch on only the models you use. The
  MODEL knob then only picks those. The setting is saved automatically and shared by
  oBraids in every slot (see "Choosing the models (MODELS list)" below).
- **Turn the jog wheel** to move through the pages AD (envelope and pitch) → Braids (bits,
  rate, ...) → Scale (ROOT, SCALE, STEPS) → My Presets (presets) → Module.
  The note length is **DECAY** on the AD page.

### 3. Build a pattern (pads)

While the screen is open the pads belong to oBraids (they do not reach Move).

```
                         MODELS  RANDOM  REC   ← right three pads of the top row
 P1  P2  P3  P4  P5  P6  P7  P8                ← second row from the top (presets)
  9  10  11  12  13  14  15  16                ← second row from the bottom
  1   2   3   4   5   6   7   8                ← bottom row
```

- The **bottom two rows** switch each step's hit on (bright) / off (dark). The step lit
  white is the one playing now. With 8 steps the second row from the bottom is dark
  (16 steps is set with STEPS on the Scale page).
- **RANDOM** writes a new melody into PITCH. Press it as often as you like until you get
  one you like. The range is centred on PITCH (knob 8). It works with REC off and while
  stopped. To undo, **hold Shift and touch knob 8** (clears the PITCH recording).
  See "RANDOM (melody generation)" below for details.
- **MODELS** opens the on/off list of models the MODEL knob can pick (below).
- The eight pads of the **second row from the top** are presets (see "5. Save and recall
  presets" below).

### 4. Record knob motion (REC)

1. Press **REC** (the top-right pad) to turn it on (solid red; dim red when off).
2. With Move playing, **touch and turn** the knob you want to record. While you touch it,
   its value is recorded into each step of the loop. Letting go ends the take.
3. A recorded knob plays its motion back on every pass from then on. The knob's own value
   does not change, and clearing the recording returns it to the knob's value.
4. To erase: **hold Shift and touch a knob** to clear that knob's recording.
   **Shift + touching knob 1 (VOLUME)** clears everything.
5. When you are done, press REC again to turn it off so a stray touch records nothing.

TIMBRE, COLOR, CUTOFF, RESO, PITCH, ATTACK, DECAY, AD>FM, AD>TIMBRE and AD>COLOR can be
recorded (VOLUME and MODEL are not).

### 5. Save and recall presets

**On the pads (eight, quickly):** the eight pads of the second row from the top are
presets 1–8.

- **Save**: hold Shift and tap a pad. It stores the whole current state.
- **Recall**: tap a pad. It switches at once, even while playing.
- Colours: off = empty, dim yellow = saved, white = the one last recalled (or saved).
  Tapping an empty pad only shows `EMPTY` on screen and changes nothing.
- A preset holds every knob value, the recorded knob motion, the melody made by RANDOM,
  the step on/off and ROOT / SCALE / STEPS (not the REC on/off or the MODELS list).
- The eight presets are stored independently of sets in
  `/data/UserData/schwung/obraids/presets.json`, so oBraids in any set and any slot sees
  the same eight. They survive reinstalling.

**With names (no limit on the number):** turn the jog wheel to the **My Presets** page
near the end.

- **Save As**: saves the current state under a name (a keyboard appears).
- **Preset** (the top row): click to open the preset list and load the one you pick. The
  name of the loaded preset is shown here too (`(none)` if none is loaded).
- **Save**: overwrites the loaded preset. **Delete**: deletes the loaded preset.

Besides every knob value, a preset also holds **the recorded knob motion, the melody made
by RANDOM and the step on/off**. It can be recalled from oBraids in any slot. No presets
are included from the start. The MODELS list on/off is a separate setting from presets.

The **Module** page next to it lets you swap the module and open help (a page shared by
every Schwung module).

### 6. When you are done

Close the screen with **Back**. The pads go back to Move. The sound, recordings and step
on/off are saved with the set (the slot's autosave) and in presets.

## Controls (reference)

The screen and knobs look and work like Schwung's standard knob screen (eight knobs per
page), with knob touch (for recording) and the pads added.

| Control | Action |
|---|---|
| Knobs 1–8 | Parameters of the current page (page table below) |
| Touch a knob (REC on, playing) | **Motion-records** that knob (below) |
| Shift + touch a knob | Clears that knob's recording; on VOLUME (knob 1) clears **everything** |
| Jog turn / click | Change page, select (same as Schwung's standard grid) |
| Back | Close the screen (pads go back to Move) |

Pages:

| Page | Knobs 1–8 |
|---|---|
| Main | VOLUME, TIMBRE, COLOR, CUTOFF, RESO, (spare), MODEL, PITCH |
| AD | ATTACK, DECAY, AD>FM, AD>TIMBRE, AD>COLOR, AD>VCA, COARSE, FINE |
| Braids | BITS, RATE, DRIFT, FLAT, SIGN, NOTE OFF |
| Scale | ROOT, SCALE, STEPS |
| My Presets | Recall, save and delete presets (shared by Schwung) |
| Module | Swap the module, help (shared by Schwung) |

Knobs 4 and 5 are a **low-pass filter** (CUTOFF / RESO), and knob 7 is **model selection
(MODEL)**. Knob 6 is a **spare that does nothing** (a dotted frame with `-`); turning or
touching it has no effect. Schwung's standard grid cannot leave an empty knob slot (it
packs knobs together), so this filler is what puts MODEL on knob 7 and PITCH on knob 8.
DECAY / AD>FM / AD>TIMBRE were moved to the AD page so they are not touched by accident
(v0.4.0).

The header shows the current model name, the loop position, `ARM` while REC is on and
`REC` while recording. Knobs with a recording use Schwung's "modulated" display (the
playing value moves).

## Pads

**While this screen is open, oBraids takes over the pads** (`host_pad_block`). Move's own
instruments and sequencer do not receive pad presses. Closing the screen gives the pads
back to Move with their colours restored (in Schwung a module can only take the pads
while its screen is shown).

```
top     .   .   .   .   .   MODELS  RANDOM  REC      ← REC is the top-right pad
        P1  P2  P3  P4  P5  P6      P7      P8       ← presets 1–8
        9  10  11  12  13  14      15      16        ← triggers 9–16
bottom  1   2   3   4   5   6       7       8        ← triggers 1–8
```

| Pad | Action |
|---|---|
| **REC** (top right) | Motion recording on/off. **On = solid red, off = dim red** |
| **RANDOM** (left of REC) | Generates a new melody and writes it into PITCH, step by step (see "RANDOM (melody generation)" below) |
| MODELS (further left) | Opens / closes the MODELS list (which models to use) |
| Second row from the top (8 pads) | Presets 1–8. Tap to recall, Shift + tap to save (see "5. Save and recall presets" above) |
| Bottom two rows (16 pads) | Each step's trigger on/off. On = bright, off = dark, the playing step = white. With 8 steps the upper row (9–16) is dark |

- Playback **plays automatically on every 1/16 note, synced to Move's transport** (it starts
  on Play, nothing plays while stopped, and the first step falls on the bar line).
  An off step stops the previous note and rests.
- The loop length is **STEPS** on the Scale page (8 / 16, default 8).
- MIDI notes coming from the Move track (clips, or the pads while the screen is closed)
  still play oBraids as before.

### Motion recording

- With REC on and Move playing, a knob's motion is recorded into the loop **while you
  touch it** (8 points per 1/16 note). Letting go ends it.
- Recordable: TIMBRE, COLOR, CUTOFF, RESO, PITCH, ATTACK, DECAY, AD>FM, AD>TIMBRE,
  AD>COLOR. VOLUME and MODEL (the model) are not recorded (MODEL always plays the model
  picked on the knob).
- Playback does not rewrite the knob's value (the recording just takes priority).
  Clearing it with Shift + touch returns to the knob's value.

### PITCH (knob 8)

- **PITCH is a scale note from the start.** Knob 8 only picks notes of ROOT + SCALE on the
  Scale page, within C1–C6 (5 octaves) (rather than being quantised afterwards, the knob
  itself moves through the scale). The default is C3 in C Phrygian.
- Changing ROOT / SCALE moves PITCH, and any recorded PITCH, to the nearest note of the new
  scale.
- SCALE: Phrygian (default), Minor, Dorian, Major, Mixolyd, Lydian, Locrian, HarmMin,
  PhrygDom, MinPent, In, Chromatic.

### RANDOM (melody generation)

RANDOM **builds a slightly different melody each time from the MIDI files (source
phrases) in `patterns/` and writes it into PITCH**. Six source phrases are included.
You can replace them with your own MIDI, which changes the melodies RANDOM makes
(see "Replacing the source phrases" below).

**What happens when you press it**

1. One source phrase is picked at random (the one used last time is less likely).
2. Its sequence of notes is varied by "50 % mutation": roughly half of the notes change
   — moving to a neighbouring scale note, jumping an octave, repeating the previous
   note, and so on — and the rest stay as in the source. The result differs every time
   and never repeats the previous melody.
3. No rests are made. Rests in the source phrase are filled with notes too, so every step
   gets a pitch.
4. The melody is written into the loop's steps as a PITCH recording.
   - With STEPS at 8 on the Scale page an 8-step melody is made; at 16 a 16-step melody
     (two 8-step variations in a row).
   - Any earlier PITCH recording is overwritten for STEPS steps (if you write at 8 and
     then switch to 16, steps 9–16 keep their earlier recording).
   - Whether a step plays is set by the bottom two pad rows. RANDOM does not change the
     step on/off.

**Range and scale**

- Every written note is a note of the scale set on the Scale page (ROOT, SCALE). Like
  knob 8's value it is written as "the nth note of the scale", so it never drifts later.
- The range is **based on the note of knob 8 (PITCH), from a fifth below to an octave and
  a half above** (-7 to +17 semitones, within C1–C6). Notes outside it are folded back by
  octaves. The melody keeps its shape and knob 8 sets the overall register (set knob 8
  before pressing).

**Undo / try again**

- Press it again to write a different melody.
- **Shift + touch knob 8** clears the PITCH recording and goes back to knob 8's note alone.
- With REC on, touch and turn knob 8 to rewrite just some of the steps by hand.

#### Replacing the source phrases

The source MIDI is embedded in `dsp.so` at build time (nothing is read from files on
Move). After replacing it you must **rebuild and reinstall**.

1. Put the MIDI files you want (Standard MIDI Files, `.mid`) in `patterns/`.
2. List the files to use in `patterns/patterns.txt`, one per line (1 to 16). The line
   order is the source phrase number. Everything after `#` is a comment.

   ```
   my_riff.mid          # 8 steps from the start of the file
   long_phrase.mid 16   # 8 steps from step 17 (the second column is the start)
   ```

   If `patterns.txt` is deleted, every `.mid` in `patterns/` is used in name order.
3. Build, install, and restart Move.

   ```bash
   scripts/build.sh arm64
   scripts/install.py http://move.local:7700
   ```

Which part of a MIDI file is used:

- Only **eight 1/16 notes from the start position** of each file (anything longer is
  ignored). To use another part of a long file, give the start position (in 1/16 notes)
  in the second column.
- Only **the pitches and their order** are used. Velocity and note length are ignored
  (oBraids hits every 1/16 at the same strength). Rests are filled with notes as above.
- Notes are snapped to the nearest 1/16. When a step holds several notes (a chord, for
  example) only the loudest one (the first, on a tie) is used. All tracks and channels
  are read together.
- Source phrases are first snapped to **C Phrygian** when loaded, varied, and finally
  fitted to the current ROOT and SCALE. MIDI written in anything other than C Phrygian
  changes slightly, so writing in Phrygian on C keeps the shape best.
- A file with no notes in the eight steps from its start position is not used. File
  names are read up to 31 bytes and up to 256 notes per file.

## Choosing the models (MODELS list)

You can decide which of the 47 models the MODEL knob can pick.

1. Press the **MODELS pad** (left of RANDOM) → the list opens (the cursor is on the model
   playing now; `<` at the right edge marks the current model).
2. Turn the jog wheel to move the cursor (it stops at the ends); click the jog to switch
   **ON / OFF** (■ = ON, □ = OFF).
3. **ALL ON** at the top switches everything on; **ALL OFF** switches off everything but
   the model playing now.
4. Close the list with **Back** or the MODELS pad.

- Changes are saved at once (`/data/UserData/schwung/obraids/settings.json`). They are
  shared by oBraids in every slot and survive reinstalling. They are not part of presets.
- The MODEL knob steps through the ON models only and stops at both ends.
- At least one always stays on ("ONE MUST STAY ON").

## Parameters

| Page | Knob | Name | Range | Hardware equivalent |
|---|---|---|---|---|
| Main | 1 | VOLUME | 0–100 (default 50) | Output level |
| Main | 2 | TIMBRE | 0–100 | TIMBRE pot (0–32767) |
| Main | 3 | COLOR | 0–100 | COLOR pot (0–32767) |
| Main | 4 | CUTOFF | 20 Hz–20 kHz (default 20 kHz = fully open) | Low-pass filter (not on the hardware) |
| Main | 5 | RESO | 0–100 (default 0) | Filter resonance (Q 0.7 to 10) |
| Main | 6 | (spare) | – | Does nothing |
| Main | 7 | MODEL | 47 models | WAVE menu (CSAW … QPSK, list below) |
| Main | 8 | PITCH | Scale notes, C1–C6 (default C3) | The pitch played |
| AD | 1 | ATTACK | 0–15 | `\ATT` |
| AD | 2 | DECAY | 0–15 (default 5) | `\DEC` |
| AD | 3 | AD>FM | 0–15 | `\FM ` |
| AD | 4 | AD>TIMBRE | 0–15 | `\TIM` |
| AD | 5 | AD>COLOR | 0–15 | `\COL` |
| AD | 6 | AD>VCA | OFF / ON | `\VCA` |
| AD | 7 | COARSE | −24…+24 semitones | Replaces the COARSE pot (also applies to MIDI notes) |
| AD | 8 | FINE | −100…+100 cents | FINE pot |
| Braids | 1 | BITS | 2BIT…16B | `BITS` |
| Braids | 2 | RATE | 4K…96K | `RATE` |
| Braids | 3 | DRIFT | OFF, 1–3, FULL | `DRFT` |
| Braids | 4 | FLAT | OFF / ON | `FLAT` |
| Braids | 5 | SIGN | OFF, 1–3, FULL | `SIGN` |
| Braids | 6 | NOTE OFF | AUTO / DRONE / GATE | Note-off handling (below) |
| Scale | 1 | ROOT | C…B | Root of PITCH's scale |
| Scale | 2 | SCALE | 12 scales | PITCH's scale |
| Scale | 3 | STEPS | 8 / 16 (default 8) | Loop length |

The low-pass filter is a 12 dB/oct filter after Braids' sound (the hardware Braids has no
filter, so this is oBraids' own). With CUTOFF fully open (the default) it passes almost
everything in the audible range, so untouched it is Braids' sound as is. CUTOFF and RESO
can be motion-recorded too.

Real AD envelope times (the hardware firmware's table `lut_env_portamento_increments` run
at the 4 kHz control rate; identical to the hardware):

| Value | 0 | 1 | 2 | 3 | 4 | **5** | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Segment length (ms) | 0.8 | 2.6 | 7.1 | 16.7 | 35.3 | **68.4** | 124 | 212 | 346 | 543 | 825 | 1219 | 1756 | 2475 | 3422 | 4648 |

**AD>FM = 10** gives `65535 × 10 >> 7 = 5119` (in 1/128 semitones) at the envelope peak,
i.e. **+40 semitones**. It is exactly the hardware's `pitch += ad_value * setting >> 7`.
AD>TIMBRE / AD>COLOR likewise add `ad_value * setting >> 5` to the pot values, as on the
hardware.

Model list (special characters on the hardware display replaced with ASCII):
CSAW, MORPH, SAWSQ, FOLD, BUZZ, SUBSQ, SUBSW, SYNSQ, SYNSW, SAWx3, SQRx3, TRIx3,
SINx3, RING, SWARM, COMB, TOY, ZLPF, ZPKF, ZBPF, ZHPF, VOSM, VOWL, VFOF, HARM, FM,
FBFM, WTFM, PLUK, BOWD, BLOW, FLUT, **BELL**, DRUM, KICK, CYMB, SNAR, WTBL, WMAP,
WLIN, WTx4, NOIS, TWNQ, CLKN, CLOU, PRTC, QPSK

### NOTE OFF

The hardware Braids has no gate, so the note-off handling can be chosen.

| Value | Behaviour |
|---|---|
| AUTO (default) | With AD>VCA = ON the AD envelope is the level itself, as on the hardware. With AD>VCA = OFF it gates with the key (with automatic playing, until the next 1/16) |
| DRONE | As on the hardware: ignores note-off and keeps sounding (stops on CC 120) |
| GATE | Always gates, whatever AD>VCA is |

## MIDI

MIDI notes correspond to Braids' TRIG input + V/OCT.

| Message | Action |
|---|---|
| Note on | `osc.Strike()` and an AD envelope trigger |
| Note off | Follows the NOTE OFF setting |
| Velocity | Not used (the hardware has no such input) |
| Pitch bend | ±2 semitones |
| CC 123 / CC 120 | All Notes Off / All Sound Off |
| MIDI clock | Used to sync automatic playing only when the host has no transport position |

## State saving

Presets, autosave and Shift+Copy snapshots all go through `state`, which saves every
setting, the motion recording and the trigger on/off:

```json
{"v":2,"level":0.5000,"model":32,...,"decay":5,...,"steps":0,"m":"pitch:<data>;...","g":65535}
```

Integer settings are saved as integers. v0.2 state (`"v":1`) can be read too (the old
`pitch` loads as COARSE and `length` as STEPS; the phrase is not carried over).

## About the sample rate

Braids is not just "a 96 kHz oscillator": every table in `resources.cc` (oscillator phase
increments and delays, SVF and resonator coefficients, bow/breath excitation lengths) is
generated for 96 kHz, and the AD envelope advances once every 24 samples (= 4 kHz).
Correcting only the increments for 44.1 kHz would get the pitch right but throw everything
else off, so it **runs as is at the hardware's 96 kHz in 24-sample blocks**, and the output
is brought to 44.1 kHz by a 147/320 polyphase FIR (64 taps, flat to 19 kHz, 70 dB or more
of attenuation above 25.5 kHz). Pitch, envelope times and each model's internal timing are
the same as the hardware by construction (tests: pitch error within 0.2 %, envelope times
compared against the hardware code).

## Level and headroom

Like the hardware, Braids' output is loud (CSAW is about −7 dBFS RMS at LEVEL 100, and some
models peak above 0 dBFS), so headroom is kept even at the default LEVEL 50. The last stage
is a soft limiter that acts from −0.9 dBFS, which the default level does not reach.

## Licence

- This module's own code: **MIT** (`LICENSE`)
- `src/braids/mi/`: Mutable Instruments Braids / stmlib, **MIT**,
  Copyright 2012 Emilie Gillet. See `THIRD_PARTY_LICENSES.md`, `patches/` and
  `scripts/fetch_braids.sh --verify` for the upstream commit and the differences.
- `src/phrase/` and `patterns/`: code and source MIDI taken from Phrase Gen (a MIDI FX for
  Schwung by the same author), used for the RANDOM pad's phrase generation.
- `third_party/schwung/`: Schwung's `plugin_api_v1.h`, **MIT**,
  Copyright 2025-2026 Charles Vestal (`third_party/schwung/LICENSE`).
- `dsp.so` links only libc / libm and contains no copyleft code. It does not conflict
  with Schwung (MIT) either.
- Name: upstream asks that derivatives not use the "Mutable Instruments" name and
  recommends not using the original module names. This module is named oBraids (the
  user's choice). If you publish or distribute it, consider renaming it (`name` in
  `module.json`).

## Files

```
schwung-obraids/
  module.json            chain_params (checked against the DSP's table at build time)
  ui_chain.js            only connects Schwung's functions to ui_core.mjs
  ui_core.mjs            drives Schwung's page engine, knob touch, pads, MODELS list
  ui_params.mjs          generated from module.json
  src/dsp_plugin.cpp     plugin API v2 adapter (static pool, RT-safe)
  src/wrapper/           MIDI, parameters, state saving, parameter table
  src/braids/            BraidsEngine (port of braids.cc RenderBlock), resampler
  src/braids/mi/         Mutable Instruments' official code (bundled, MIT)
  src/phrase/            Phrase Gen's engine (analysis, generation, sequencer, scales)
  src/wrapper/motion.*   1/16 clock and motion recording
  src/wrapper/scale.*    PITCH's scales (12, C1–C6)
  src/wrapper/random_phrase.*  RANDOM (makes the PITCH recording with Phrase Gen)
  patterns/              source MIDI for the phrases (embedded at build time)
  patches/               fixes to upstream Braids
  scripts/               build / test / install / fetch_braids / generator scripts
  tests/                 engine, dlopen smoke, UI
  third_party/schwung/   Schwung plugin API header (MIT, unmodified)
```
