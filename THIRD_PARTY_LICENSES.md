# Third-party code in oBraids

Everything the module ships is MIT-licensed. Nothing copyleft is compiled in or
linked (`dsp.so` needs only `libc` and `libm`; `scripts/check_abi.py` fails the
build otherwise), so there is no conflict with Schwung, whose source is MIT.

## Mutable Instruments Braids and stmlib — MIT

| | |
|---|---|
| Upstream | https://github.com/pichenettes/eurorack (`braids/`) at `08460a69a7e1f7a81c5a2abcc7189c9a6b7208d4` |
| | https://github.com/pichenettes/stmlib at `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec` (the submodule commit eurorack pins) |
| Copyright | Copyright 2012 Emilie Gillet |
| Licence | MIT (the eurorack README: "Code (STM32F projects): MIT license"; every vendored file carries the MIT notice) |
| Where | `src/braids/mi/` |

Vendored files: `braids/{analog_oscillator,digital_oscillator,macro_oscillator}.{h,cc}`,
`braids/resources.{h,cc}`, `braids/{envelope,excitation,parameter_interpolation,settings,signature_waveshaper,svf,vco_jitter_source}.h`,
`stmlib/stmlib.h`, `stmlib/utils/{dsp.h,random.h,random.cc}`, and stmlib's
`LICENSE`. Only DSP: no drivers, UI, storage, bootloader or calibration code.

They are unmodified except for the patches in `patches/`, each of which is
also marked in the source with an `oBraids patch` comment:

- `0001-wlin-wave-line-bounds.patch` — upstream reads one byte past
  `wave_line[64]` in the WLIN model when TIMBRE is near maximum. Clamped.

`scripts/fetch_braids.sh --verify` re-downloads both commits and checks that the
vendored tree is exactly upstream plus these patches.

### The MIT notice (as it appears in the vendored files)

```
Copyright 2012 Emilie Gillet.

Author: Emilie Gillet (emilie.o.gillet@gmail.com)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

See http://creativecommons.org/licenses/MIT/ for more information.
```

### Names and trademarks

The eurorack README asks that derivative works not use the name
"Mutable Instruments" (a registered trademark) and recommends not keeping a
module's original name. This build is named **oBraids** at its user's
request, for personal use. If you publish a build, consider renaming it
(`name` in `module.json`).

## Phrase Gen — this repository's own module

`src/phrase/` (the loader, analyser, generator, scale quantizer and sequencer)
and `patterns/` (the six source MIDI files) are copied from
Phrase Gen (a Schwung MIDI FX by the same author). One
change: `pg_sequencer.c` swaps a queued pattern on the LONGER of the old and
new lengths (marked `oBraids:`), so an 8 -> 16 step change starts on step 1.
Since 0.3.0 only the loader, analyser and generator are used, by the RANDOM
pad (`src/wrapper/random_phrase.cpp`); the sequencer is compiled but unused.

## Schwung — MIT

`third_party/schwung/host/plugin_api_v1.h` is Schwung's plugin API header
(Copyright 2025-2026 Charles Vestal, MIT), vendored unmodified; its licence is
`third_party/schwung/LICENSE`.
