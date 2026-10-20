# SPDX-License-Identifier: GPL-3.0-only
# BIGORNA: MIDI LEARN tested on the FM-1 from the computer, over USB-MIDI, with a TELEMETRY build (build.py --telemetry):
# the panel's actions as F0 7D 5A a b c F7 (ui.c), a report back as F0 7D 5B; CCs sent as a controller would (the
# M-VAVE SMC Mixer in MIDI mode: absolute CCs 30.. on channel 1).
#   .venv/bin/python tools/learn_test.py
import re, sys, time
import mido

PG = {0: "MAIN", 1: "EDIT", 2: "FX"}


def port(names):
    n = [x for x in names if re.search(r"fm-1|bigorna", x, re.I)]
    if not n:
        raise SystemExit(f"no FM-1 port among {names}")
    return n[0]


out = mido.open_output(port(mido.get_output_names()))
inp = mido.open_input(port(mido.get_input_names()))
fails = 0


def cmd(a, b=0, c=0, wait=0.12):
    out.send(mido.Message("sysex", data=[0x7D, 0x5A, a, b, c]))
    time.sleep(wait)


def report():
    for _ in inp.iter_pending():
        pass
    cmd(6, wait=0)
    t0 = time.time()
    while time.time() - t0 < 2.0:
        for m in inp.iter_pending():
            if m.type == "sysex" and len(m.data) >= 9 and m.data[0] == 0x7D and m.data[1] == 0x5B:
                d = list(m.data)
                st = {"learn": d[2], "listen": d[3], "row": d[4], "page": d[5], "track": d[6], "edit_pg": d[7],
                      "fx_pg": d[8], "bind": []}
                for i in range(9, len(d) - 6, 7):
                    ch, cc, kind, trk, pid, hi, lo = d[i:i + 7]
                    st["bind"].append({"ch": ch, "cc": cc, "kind": "SND" if kind == 1 else "FX", "track": trk,
                                       "id": pid, "value": (hi << 7 | lo) - 1024})
                return st
        time.sleep(0.01)
    raise SystemExit("no report from the FM-1 (is the TELEMETRY build installed?)")


def cc(n, v, ch=0):
    out.send(mido.Message("control_change", channel=ch, control=n, value=v))
    time.sleep(0.3)


def check(ok, what):
    global fails
    print(("ok    " if ok else "FAIL  ") + what)
    fails += not ok


def bind(st, cc_):
    return next((b for b in st["bind"] if b["cc"] == cc_), None)


cmd(8)                                           # (no bindings)
st = report()
if st["learn"]:
    cmd(0)
cmd(0)
st = report()
check(st["learn"] == 1 and not st["bind"], "MIDI LEARN on, no bindings")

cmd(1, 0, 0)                                     # EDIT, track 1, SYN 1
for row, n in ((0, 30), (1, 31), (2, 32)):
    cmd(3, row)
    cmd(4)
    st = report()
    check(st["listen"] == 1 and st["row"] == row, f"SYN 1 row {'ABC'[row]}: SAVE, waiting for a CC")
    cc(n, 64)
    time.sleep(0.3)                              # (a report asked at once can go out before the FM-1's next pass
    st = report()                                #  takes the CC: the test's commands make it draw the page whole)
    b = bind(st, n)
    check(st["listen"] == 0 and b is not None and b["ch"] == 1 and b["kind"] == "SND" and b["track"] == 0 and
          b["id"] == row, f"CC {n} on channel 1 learned for track 1, SYN 1 {'ABC'[row]}")

cmd(2, 0, 0)                                     # CHORUS
cmd(3, 0)
cmd(4)
cc(33, 10)
st = report()
b = bind(st, 33)
check(b is not None and b["kind"] == "FX" and b["id"] == 0, "CC 33 learned for CHORUS DEPTH")

for v in (0, 127, 50):                           # the knobs turned: the parameters follow
    cc(30, v)
    cc(33, v)
    st = report()
    b30, b33 = bind(st, 30), bind(st, 33)
    print(f"      CC 30 = {v:3d} -> SYN 1 A = {b30['value']:4d};  CC 33 = {v:3d} -> CHORUS DEPTH = {b33['value']:4d}")
st0 = report()
for v in range(128):                             # a fast sweep (a knob turned quickly): every value through
    out.send(mido.Message("control_change", channel=0, control=31, value=v))
time.sleep(0.3)
st = report()
b31 = bind(st, 31)
print(f"      a sweep of 128 CC 31 values sent as fast as USB takes them: SYN 1 B = {b31['value']}")
check(b31["value"] == 127, "the FM-1 took the sweep: SYN 1 B at the last value's (the top)")

cmd(3, 0)                                        # CHORUS DEPTH: SHIFT + SAVE forgets its CC
cmd(5)
st = report()
check(bind(st, 33) is None and bind(st, 30) is not None, "SHIFT + SAVE: CC 33 forgotten, the others kept")

cmd(1, 0, 0)                                     # a CC learned again for another row: it moves
cmd(3, 3)
cmd(4)
cc(30, 64)
st = report()
b = bind(st, 30)
check(b is not None and b["id"] == 3 and len([x for x in st["bind"] if x["cc"] == 30]) == 1,
      "CC 30 learned again for SYN 1 D: moved from A (one parameter a CC)")

cmd(0)                                           # MIDI LEARN off: the bindings written to the flash
st = report()
check(st["learn"] == 0 and len(st["bind"]) == 3, "MIDI LEARN off, three bindings kept")
print("bindings:", [(b["cc"], b["kind"], b["track"] + 1, b["id"]) for b in st["bind"]])
print(f"{fails} failures")
sys.exit(1 if fails else 0)
