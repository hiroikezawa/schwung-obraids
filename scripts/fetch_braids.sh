#!/usr/bin/env bash
# Where src/braids/mi came from, as a command.
#
#   scripts/fetch_braids.sh --verify   download the pinned upstream commits,
#                                      apply patches/, and diff against the
#                                      vendored tree (exit 1 on any difference)
#   scripts/fetch_braids.sh --update   the same, but overwrite the vendored tree
set -euo pipefail
MODULE="$(cd "$(dirname "$0")/.." && pwd)"
EURORACK_COMMIT=08460a69a7e1f7a81c5a2abcc7189c9a6b7208d4
STMLIB_COMMIT=e3bd7c9cc00e4364166f9905c0509b6ffd0535ec
BRAIDS_FILES=(analog_oscillator.cc analog_oscillator.h digital_oscillator.cc
  digital_oscillator.h macro_oscillator.cc macro_oscillator.h envelope.h
  excitation.h parameter_interpolation.h resources.cc resources.h svf.h
  signature_waveshaper.h vco_jitter_source.h settings.h)
STMLIB_FILES=(stmlib.h utils/dsp.h utils/random.h utils/random.cc LICENSE)

mode="${1:---verify}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

git -C "$work" init -q eurorack
git -C "$work/eurorack" fetch -q --depth 1 https://github.com/pichenettes/eurorack "$EURORACK_COMMIT"
git -C "$work/eurorack" checkout -q FETCH_HEAD
git -C "$work" init -q stmlib
git -C "$work/stmlib" fetch -q --depth 1 https://github.com/pichenettes/stmlib "$STMLIB_COMMIT"
git -C "$work/stmlib" checkout -q FETCH_HEAD

out="$work/mi"
mkdir -p "$out/braids" "$out/stmlib/utils"
for f in "${BRAIDS_FILES[@]}"; do cp "$work/eurorack/braids/$f" "$out/braids/$f"; done
for f in "${STMLIB_FILES[@]}"; do cp "$work/stmlib/$f" "$out/stmlib/$f"; done
mv "$out/stmlib/LICENSE" "$out/LICENSE"
for p in "$MODULE"/patches/*.patch; do
  patch -s -d "$out" -p1 < "$p"
done

case "$mode" in
  --verify)
    if diff -r "$out" "$MODULE/src/braids/mi"; then
      echo "PASS: src/braids/mi is upstream $EURORACK_COMMIT / $STMLIB_COMMIT + patches/"
    else
      echo "FAIL: src/braids/mi differs from upstream + patches/" >&2
      exit 1
    fi ;;
  --update)
    rm -rf "$MODULE/src/braids/mi"
    cp -r "$out" "$MODULE/src/braids/mi"
    echo "updated src/braids/mi" ;;
  *) echo "usage: $0 [--verify|--update]" >&2; exit 2 ;;
esac
