# SPDX-License-Identifier: GPL-3.0-only
# BIGORNA: reads a diagnostic build's timing reports (python3 tools/build.py --telemetry) from the FM-1 over USB-MIDI,
# and plays a test on it: a kick on track 1 each beat, then 1, 2, 3 .. notes of a pad on track 2.
#   .venv/bin/python tools/telelog.py [listen | pads [MAXNOTES] [SECONDS]]
# Each report (0.2 s): the I2S halves' highest and mean load, late halves, voices sounding, tails let go (the guard's
# x 100), each machine's voice cost, each track's and the effects' time a block, all in 1/1000 of the time there was.
import re, sys, time, threading
import mido

ENG = {"FMT": 0, "FMD": 1, "WAV": 2, "SWM": 3}


def port_name(names):
    n = [x for x in names if re.search(r"fm-1|bigorna", x, re.I)]
    if not n:
        raise SystemExit(f"no FM-1 port among {names}")
    return n[0]


def parse(d):
    v = [d[i] << 7 | d[i + 1] for i in range(0, len(d) - 1, 2)]
    if len(v) < 17:
        return None
    return {"max": v[0], "mean": v[1], "late": v[2], "voices": v[3], "shed": v[4], "eng": v[5:9],
            "trk": v[9:15], "fx": v[15], "blk": v[16], "blkmax": v[17] if len(v) > 17 else 0,
            "mach": v[18:22], "chain": v[22:26], "fxk": v[26:30], "bench": v[30:38],
            "c1": v[38:45], "ph": v[45:49]}


def c1s(r):
    c = r.get("c1") or []
    if len(c) < 7:
        return ""
    return (f" | CPU1 {'on' if c[0] & 1 else 'OFF'} power {'ok' if c[0] & 2 else 'FAIL'}{' barred' if c[0] & 4 else ''}"
            f" jobs {c[1]} misreads {c[2]} faults {c[3]} wait max {c[4]/10:.1f}% timeouts {c[5]} rendered {c[6]/10:.1f}%"
            + (f" | worst: events {r['ph'][0]/10:.1f}% pre-voices {r['ph'][1]/10:.1f}% voices {r['ph'][3]/10:.1f}% (mean {r['ph'][2]/10:.1f}%)"
               if len(r.get("ph") or []) == 4 else ""))


def line(r):
    e = r["eng"]
    return (f"half max {r['max']/10:5.1f}% mean {r['mean']/10:5.1f}%  late {r['late']:2d}  voices {r['voices']:2d}  "
            f"shed {r['shed']:4d} | block {r['blk']/10:5.1f}% (max {r['blkmax']/10:5.1f}) tracks "
            + " ".join(f"{t/10:4.1f}" for t in r["trk"]) + f"  fx {r['fx']/10:4.1f} | voice FMT {e[0]/10:.1f} "
            f"FMD {e[1]/10:.1f} WAV {e[2]/10:.1f} SWM {e[3]/10:.1f}" + c1s(r))


class Reader(threading.Thread):
    def __init__(self, name):
        super().__init__(daemon=True)
        self.name_, self.label, self.log, self.crash = name, "", [], None

    def run(self):
        with mido.open_input(self.name_) as port:
            for m in port:
                if m.type == "sysex" and len(m.data) > 50 and m.data[0] == 0x7D and m.data[1] == 0x59:
                    v = [m.data[2 + i] << 7 | m.data[3 + i] for i in range(0, len(m.data) - 3, 2)]
                    self.crash = [v[3 * j] << 18 | v[3 * j + 1] << 4 | v[3 * j + 2] for j in range(len(v) // 3)]
                    continue
                if m.type != "sysex" or len(m.data) < 4 or m.data[0] != 0x7D or m.data[1] != 0x54:
                    continue
                r = parse(m.data[2:])
                if r:
                    self.log.append((self.label, r))
                    print(f"{self.label:10s} {line(r)}", flush=True)


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "listen"
    rd = Reader(port_name(mido.get_input_names()))
    rd.start()
    if mode == "status":                         # one report, as it comes
        while not rd.log:
            time.sleep(0.2)
        time.sleep(0.5)
        print(line(rd.log[-1][1]))
        return
    if mode == "crashrec":                       # the last crash's record (across its reset), CPU1's last fault
        while not rd.crash:
            time.sleep(0.2)
        c = rd.crash
        print("crashes %d  vec %08x  pc %08x  dbg %08x  emu %08x | CPU1 fault dbg %08x" % tuple(c[:6]))
        for j in range(2):
            print("  handler entry on core %d: times %d cnum %d dbg %08x emu0 %08x emu1 %08x pc %08x" % ((j,) + tuple(c[6 + 6 * j:12 + 6 * j])))
        return
    if mode == "crash1":                         # a test: CPU1 faults; CPU0 must go on alone, the USB answering
        out = mido.open_output(port_name(mido.get_output_names()))
        while not rd.log:
            time.sleep(0.2)
        print("before:", line(rd.log[-1][1]), flush=True)
        out.send(mido.Message("note_on", channel=1, note=48, velocity=100))
        time.sleep(1.0)
        out.send(mido.Message("sysex", data=[0x7D, 0x58, 0, 0]))
        time.sleep(2.0)
        print("after: ", line(rd.log[-1][1]), flush=True)
        out.send(mido.Message("note_off", channel=1, note=48))
        return
    if mode == "bench":                          # the chip's costs, measured at start-up: cycles (360 MHz) a step
        while not rd.log:
            time.sleep(0.2)
        b = rd.log[-1][1]["bench"]
        names = ["loop + add", "loop + 4 adds", "loop + store", "loop + 4 stores", "loop + load", "loop + 4 loads",
                 "loop + branch", "loop + 2 mul64"]
        for nm, t in zip(names, b):
            print(f"{nm:18s} {t * 360 / 24 / 4096:6.2f} cycles")
        return
    if mode == "listen":
        while True:
            time.sleep(1)
    out = mido.open_output(port_name(mido.get_output_names()))
    if mode == "arr":                            # the arrangement: kick, a pad triad, hats, a lead, a bass
        secs = float(sys.argv[2]) if len(sys.argv) > 2 else 8.0
        sx = lambda *b: out.send(mido.Message("sysex", data=[0x7D, *b]))
        for t, (m, r) in enumerate([("FMD", 2), ("SWM", 54), ("FMD", 8), ("FMT", 30), ("FMT", 29)]):
            sx(0x56, t, ENG[m], r)
            time.sleep(0.3)
        time.sleep(0.5)
        triad = [48, 55, 63]
        for n in triad:
            out.send(mido.Message("note_on", channel=1, note=n, velocity=110))
        rd.label = "arr"
        step, k, t0 = 0.125, 0, time.time()
        lead = [72, 75, 79, 82, 79, 75, 70, 67]
        bass = [36, 36, 48, 36, 39, 36, 43, 34]
        while time.time() - t0 < secs:
            on = []
            if k % 4 == 0:
                on.append((0, 36, 120))
            on.append((2, 60, 90))
            if k % 2 == 0:
                on.append((3, lead[(k // 2) % 8], 100))
                on.append((4, bass[(k // 2) % 8], 110))
            for ch, n, v in on:
                out.send(mido.Message("note_on", channel=ch, note=n, velocity=v))
            time.sleep(0.04)
            for ch, n, v in on:
                if ch != 3 and ch != 4:
                    out.send(mido.Message("note_off", channel=ch, note=n))
            time.sleep(step - 0.08)
            for ch, n, v in on:
                if ch == 3 or ch == 4:
                    out.send(mido.Message("note_off", channel=ch, note=n))
            time.sleep(0.04)
            k += 1
        for n in triad:
            out.send(mido.Message("note_off", channel=1, note=n))
        rd.label = "off"
        time.sleep(1.0)
        rs = [r for l, r in rd.log if l == "arr"][5:]
        av = lambda key, i: sum(r[key][i] for r in rs) / len(rs) / 10
        print(f"\narrangement: half max {max(r['max'] for r in rs)/10:.1f}%  mean {sum(r['mean'] for r in rs)/len(rs)/10:.1f}%"
              f"  block max {max(r['blkmax'] for r in rs)/10:.1f}%  sheds {sum(r['shed'] for r in rs)}  late {sum(r['late'] for r in rs)}"
              f"  voices max {max(r['voices'] for r in rs)}")
        print("  tracks " + " ".join(f"{av('trk', i):4.1f}" for i in range(6)) + f"  fx {sum(r['fx'] for r in rs)/len(rs)/10:.1f}")
        print("  machine FMT %.1f FMD %.1f WAV %.1f SWM %.1f | chain FMT %.1f FMD %.1f WAV %.1f SWM %.1f | chorus %.1f delay %.1f "
              "reverb %.1f comp %.1f" % tuple([av("mach", i) for i in range(4)] + [av("chain", i) for i in range(4)] +
                                              [av("fxk", i) for i in range(4)]))
        return
    maxn = int(sys.argv[2]) if len(sys.argv) > 2 else 4
    secs = float(sys.argv[3]) if len(sys.argv) > 3 else 4.0
    sx = lambda *b: out.send(mido.Message("sysex", data=[0x7D, *b]))
    for t in range(6):
        out.send(mido.Message("control_change", channel=t, control=123, value=0))
    sx(0x56, 0, ENG["FMD"], 2)                   # track 1: DRIVEN KICK
    time.sleep(0.3)
    sx(0x56, 1, ENG["SWM"], 54)                  # track 2: SWARM DRONE
    time.sleep(0.5)
    notes = [48, 55, 60, 63, 67, 70]
    held = []
    beat = 0.5

    def kick(until):
        while time.time() < until:
            out.send(mido.Message("note_on", channel=0, note=36, velocity=120))
            time.sleep(0.05)
            out.send(mido.Message("note_off", channel=0, note=36))
            time.sleep(beat - 0.05)
    for k in range(maxn + 1):
        if k:
            n = notes[k - 1]
            out.send(mido.Message("note_on", channel=1, note=n, velocity=110))
            held.append(n)
        rd.label = f"kick+{k}"
        kick(time.time() + secs)
    for n in held:
        out.send(mido.Message("note_off", channel=1, note=n))
    rd.label = "off"
    time.sleep(1.5)
    print("\nsummary (the reports past the first second of each part):")
    for k in range(maxn + 1):
        rs = [r for l, r in rd.log if l == f"kick+{k}"][5:]
        if rs:
            print(f"kick+{k}: half max {max(r['max'] for r in rs)/10:5.1f}%  mean {sum(r['mean'] for r in rs)/len(rs)/10:5.1f}%"
                  f"  block max {max(r['blkmax'] for r in rs)/10:5.1f}%  sheds {sum(r['shed'] for r in rs)}  late {sum(r['late'] for r in rs)}"
                  f"  track 1 {sum(r['trk'][0] for r in rs)/len(rs)/10:4.1f}%  track 2 {sum(r['trk'][1] for r in rs)/len(rs)/10:4.1f}%"
                  f"  fx {sum(r['fx'] for r in rs)/len(rs)/10:4.1f}%")
            av = lambda key, i: sum(r[key][i] for r in rs if len(r[key]) > i) / len(rs) / 10
            print("        machine FMT %.1f FMD %.1f WAV %.1f SWM %.1f | chain FMT %.1f FMD %.1f WAV %.1f SWM %.1f | "
                  "chorus %.1f delay %.1f reverb %.1f comp %.1f" % tuple([av("mach", i) for i in range(4)] +
                  [av("chain", i) for i in range(4)] + [av("fxk", i) for i in range(4)]))


if __name__ == "__main__":
    main()
