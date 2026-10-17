#!/usr/bin/env python3
"""dlopen the real dsp.so through the v2 ABI, the way the chain host does."""
import ctypes as c
import json
import pathlib
import sys

LIB = sys.argv[1]
BLOCK = 128
KEEPALIVE = 6            # the DC keepalive a silent block carries (LSB)

BPMFN = c.CFUNCTYPE(c.c_float)
BEATFN = c.CFUNCTYPE(c.c_double)


class Host(c.Structure):
    _fields_ = [("api_version", c.c_uint32), ("sample_rate", c.c_int),
                ("frames_per_block", c.c_int), ("mapped_memory", c.c_void_p),
                ("audio_out_offset", c.c_int), ("audio_in_offset", c.c_int),
                ("log", c.c_void_p), ("midi_send_internal", c.c_void_p),
                ("midi_send_external", c.c_void_p), ("get_clock_status", c.c_void_p),
                ("mod_emit_value", c.c_void_p), ("mod_clear_source", c.c_void_p),
                ("mod_host_ctx", c.c_void_p), ("get_bpm", BPMFN),
                ("midi_inject_to_move", c.c_void_p), ("slot_recv_channel", c.c_void_p),
                ("get_beat_position", BEATFN), ("reserved", c.c_void_p * 8)]


assert Host.get_beat_position.offset == 112 and Host.reserved.offset == 120


class API(c.Structure):
    _fields_ = [("version", c.c_uint32)] + [(k, c.c_void_p) for k in
               ("create", "destroy", "midi", "set", "get", "error", "render")]


transport = {"beat": -1.0, "bpm": 120.0}
bpm_cb = BPMFN(lambda: transport["bpm"])
beat_cb = BEATFN(lambda: transport["beat"])

lib = c.CDLL(LIB)
lib.move_plugin_init_v2.restype = c.POINTER(API)
lib.move_plugin_init_v2.argtypes = [c.c_void_p]
bad = Host(api_version=1, sample_rate=48000, frames_per_block=BLOCK)
assert not lib.move_plugin_init_v2(c.byref(bad)), "must refuse a 48 kHz host"
host = Host(api_version=1, sample_rate=44100, frames_per_block=BLOCK,
            get_bpm=bpm_cb, get_beat_position=beat_cb)
api = lib.move_plugin_init_v2(c.byref(host)).contents
assert api.version == 2

create = c.CFUNCTYPE(c.c_void_p, c.c_char_p, c.c_char_p)(api.create)
destroy = c.CFUNCTYPE(None, c.c_void_p)(api.destroy)
midi_fn = c.CFUNCTYPE(None, c.c_void_p, c.POINTER(c.c_uint8), c.c_int, c.c_int)(api.midi)
set_param = c.CFUNCTYPE(None, c.c_void_p, c.c_char_p, c.c_char_p)(api.set)
get_param = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_char_p, c.c_char_p, c.c_int)(api.get)
render = c.CFUNCTYPE(None, c.c_void_p, c.POINTER(c.c_int16), c.c_int)(api.render)

buf = (c.c_int16 * (BLOCK * 2))()


def read(p, key, size=16384):
    out = c.create_string_buffer(size)
    n = get_param(p, key.encode(), out, size)
    return None if n < 0 else out.value.decode()


def write(p, key, value):
    set_param(p, key.encode(), str(value).encode())


def midi(p, *msg):
    arr = (c.c_uint8 * len(msg))(*msg)
    midi_fn(p, arr, len(msg), 0)


def blocks(p, n):
    """Render n blocks, advancing the transport if it runs; returns the peak."""
    peak = 0
    for _ in range(n):
        render(p, buf, BLOCK)
        assert all(buf[2 * i] == buf[2 * i + 1] for i in range(BLOCK)), "mono to both sides"
        peak = max(peak, max(abs(v) for v in buf))
        if transport["beat"] >= 0:
            transport["beat"] += BLOCK / 44100 * transport["bpm"] / 60
    return peak


manifest = json.loads((pathlib.Path(__file__).resolve().parent.parent / "module.json").read_text())
params = manifest["capabilities"]["chain_params"]

p = create(b".", None)
assert p

# ---- contract reads ----------------------------------------------------------
assert json.loads(read(p, "chain_params")) == params, "served chain_params == module.json (C Phrygian)"
out = c.create_string_buffer(64)
assert get_param(p, b"ui_hierarchy", out, 64) == 0 and out.value == b"", "ui_hierarchy must be served empty"
h = json.loads(read(p, "grid_hierarchy"))
(pathlib.Path(LIB).parent / "grid_hierarchy.json").write_text(read(p, "grid_hierarchy"))   # for test_ui.mjs
assert h["levels"]["root"]["knobs"] == ["level", "timbre", "color", "cutoff", "reso", "spare", "model", "pitch"]
assert read(p, "no_such_key") is None
for prm in params:
    got = read(p, prm["key"])
    want = prm["options"].index(prm["default"]) if prm["type"] == "enum" else prm["default"]
    assert abs(float(got) - float(want)) < 1e-4, (prm["key"], got, want)
names = read(p, "pitch_names").split(",")
assert len(names) == 36 and names[0] == "C1" and names[14] == "C3" and names[-1] == "C6", names
assert read(p, "motion_status") == "0,-1,0,8,0", read(p, "motion_status")
assert read(p, "gate_status") == "1" * 16

# Hardware units in, hardware units out.
write(p, "decay", 5)
write(p, "ad_fm", 10)
write(p, "model", 32)
assert read(p, "decay") == "5" and read(p, "ad_fm") == "10" and read(p, "model") == "32"
write(p, "decay", "nan")            # refused, not applied
write(p, "decay", "7junk")
assert read(p, "decay") == "5"
write(p, "pitch", 99)               # clamped to the scale's top note
assert read(p, "pitch") == "35"
write(p, "model", 0)
write(p, "ad_fm", 0)
write(p, "pitch", 14)

# ---- the auto 1/16 ------------------------------------------------------------
assert blocks(p, 20) <= KEEPALIVE, "silent while the transport is stopped"
transport["beat"] = 0.0
assert blocks(p, 100) > 1000, "plays every 1/16 while the transport runs"
st = read(p, "motion_status").split(",")
assert 0 <= int(st[1]) < 8, st
for i in range(16):
    write(p, "gate_toggle", i)       # every trigger pad off
assert read(p, "gate_status") == "0" * 16
blocks(p, 60)                        # the held note is let go at the next 1/16
assert blocks(p, 40) <= KEEPALIVE, "all 16 trigger pads off = silence"
for i in range(16):
    write(p, "gate_toggle", i)
assert blocks(p, 60) > 1000

# ---- the low-pass, and the spare knob ---------------------------------------------
open_peak = blocks(p, 60)
write(p, "cutoff", 0.0)              # 20 Hz: next to nothing gets through
blocks(p, 20)
assert blocks(p, 60) < open_peak / 8, "CUTOFF closes the low-pass"
write(p, "cutoff", 1.0)
blocks(p, 20)
assert blocks(p, 60) > open_peak / 2
write(p, "spare", 5)                 # the dummy: stays what it is
assert read(p, "spare") == "0"

# ---- RANDOM, and touch recording ----------------------------------------------
write(p, "randomize", 1)
assert int(read(p, "motion_status").split(",")[0]) & (1 << 5), "RANDOM fills the PITCH lane"
write(p, "write", 1)
write(p, "timbre", 0.9)
for _ in range(9):                   # one whole 8-step loop, restated like the UI
    write(p, "touch", "timbre,1")
    blocks(p, 40)
assert int(read(p, "motion_status").split(",")[2]) & (1 << 0), "TIMBRE records under the finger"
write(p, "touch", "timbre,0")
write(p, "timbre", 0.1)
blocks(p, 5)
assert abs(float(read(p, "eff_timbre")) - 0.9) < 0.01, read(p, "eff_timbre")
assert read(p, "timbre") == "0.1000", "playback never writes the knob"
write(p, "motion_clear", "timbre")
blocks(p, 2)
assert read(p, "eff_timbre") == "0.1000"

# MODEL is never recorded: a touch on it writes nothing, and an old state's
# MODEL lane is dropped while its other lanes are kept.
write(p, "touch", "model,1")
blocks(p, 40)
assert not (int(read(p, "motion_status").split(",")[0]) & ~((1 << 10) - 1)), "no lane beyond the ten"
write(p, "touch", "model,0")
r = create(b".", None)
lane = "AA" * 128
write(r, "state", '{"v":2,"model":5,"m":"model:%s;timbre:%s","g":65535}' % (lane, lane))
assert read(r, "motion_status").split(",")[0] == "1", read(r, "motion_status")   # timbre only
assert '"m":"timbre:' in read(r, "state")
destroy(r)

# ---- state ---------------------------------------------------------------------
write(p, "gate_toggle", 3)
write(p, "steps", 1)
state = read(p, "state")
assert '"decay":5,' in state and '"m":"pitch:' in state and '"g":65527' in state, state[:200]
q = create(b".", None)
assert q and q != p
write(q, "state", state)
assert read(q, "state") == state, "state round trip"
assert read(q, "motion_status").split(",")[3] == "16"
# 0.2's state: "pitch" was the coarse transpose and "length" the steps.
write(q, "state", '{"v":1,"model":32,"pitch":12,"length":1,"seq_pitch":3}')
assert read(q, "coarse") == "12" and read(q, "steps") == "1" and read(q, "model") == "32"

# ---- stop: silence ---------------------------------------------------------------
transport["beat"] = -1.0
blocks(p, 40)
assert blocks(p, 10) <= KEEPALIVE, "silent after Stop"

# ---- the pool: six instances, and a freed one is reused clean -------------------
extra = [create(b".", None) for _ in range(4)]
assert all(extra) and create(b".", None) is None, "pool must hold exactly six"
write(extra[0], "model", 7)
destroy(extra[0])
again = create(b".", None)
assert again == extra[0]
assert read(again, "model") == "0", "a reused slot must start clean"
for x in [again] + extra[1:] + [q, p]:
    destroy(x)

print("PASS: dlopen smoke (v2 ABI, grid contract, auto 1/16 + gates, RANDOM, touch motion, state, pool)")
