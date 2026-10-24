#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# BIGORNA: the home page's demo loop, from a pattern recorded by tools/render_demo.mjs (raw: 16-bit, stereo, 44.1 kHz,
# from PLAY). BARS bars at BPM, from a downbeat after SKIP bars (the effects' tails in); its last 30 ms, up to 10 ms
# before the downbeat, crossfaded into what came after it, so the end runs into the start. The audio is not periodic
# to the sample (the noise runs free), so the seam is a crossfade, on the beat. Written twice over as an MP3 (ffmpeg):
# the page loops LOOP_START .. LOOP_START + BARS bars of it, inside the two, whatever padding the encoder adds.
#   tools/make_loop.py IN.raw BARS BPM OUT.mp3 [SKIP]       -> prints the page's loop start and end (s)
import array
import subprocess
import sys
import tempfile

SR = 44100
LOOP_START = 0.5            # (s into the file: past any encoder delay)


def main():
    raw, bars, bpm, out = sys.argv[1], int(sys.argv[2]), float(sys.argv[3]), sys.argv[4]
    skip = int(sys.argv[5]) if len(sys.argv) > 5 else 4
    x = array.array("h")
    with open(raw, "rb") as f:
        x.frombytes(f.read())
    n = len(x) // 2
    bar = 60.0 / bpm * 4 * SR
    onset = next(i for i in range(n) if abs(x[2 * i]) + abs(x[2 * i + 1]) > 1000)   # (step 1's first hit)
    length = round(bars * bar)
    pre, fade = round(0.010 * SR), round(0.030 * SR)
    s = onset + round(skip * bar) - pre - fade          # (the seam: 40 .. 10 ms before the downbeat)
    if s + length + fade > n:
        raise SystemExit("make_loop: the recording is too short for that")
    y = array.array("h", x[2 * s:2 * (s + length)])
    for i in range(fade):                               # (an equal-power crossfade)
        g = (i + 0.5) / fade
        gi, go = g ** 0.5, (1 - g) ** 0.5
        for c in (0, 1):
            v = x[2 * (s + i) + c] * gi + x[2 * (s + length + i) + c] * go
            y[2 * i + c] = max(-32768, min(32767, round(v)))
    # the file: the loop's last LOOP_START s, then the loop twice (looping any window of `length` inside it is seamless)
    lead = round(LOOP_START * SR)
    body = y[2 * (length - lead):] + y + y
    with tempfile.NamedTemporaryFile(suffix=".raw") as t:
        t.write(body.tobytes())
        t.flush()
        subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-f", "s16le", "-ar", str(SR), "-ac", "2", "-i", t.name,
                        "-codec:a", "libmp3lame", "-q:a", "2", out], check=True)
    print(f"make_loop: {bars} bars at {bpm:g} BPM, {length / SR:.4f} s from {s / SR:.3f} s -> {out}")
    print(f"loop start {LOOP_START} s, end {LOOP_START + length / SR:.6f} s")


if __name__ == "__main__":
    main()
