#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# BIGORNA: the home page's demo loop, from a pattern recorded by tools/render_demo.mjs (raw: 16-bit, stereo, 44.1 kHz,
# from PLAY). BARS bars at BPM, from a downbeat after SKIP bars (the effects' tails in); its last 30 ms, up to 10 ms
# before the downbeat, crossfaded into what came after it, so the end runs into the start. The audio is not periodic
# to the sample (the noise runs free), so the seam is a crossfade, on the beat. Written twice over as an MP3 (ffmpeg):
# the page loops LOOP_START .. LOOP_START + BARS bars of it, inside the two, whatever padding the encoder adds.
# Mastered on the way (MASTER, ffmpeg): rumble off, a little mud out, presence and air up, a slow bus compressor, then
# the gain that brings it to LUFS (measured, three times) into a limiter. The compressor and the limiter follow the audio, so
# the loop goes through four times over and the third time is kept: by then they play it as they would every time
# after, and its end runs into its start as the fourth does.
#   tools/make_loop.py IN.raw BARS BPM OUT.mp3 [SKIP]       -> prints the page's loop start and end (s)
import array
import re
import subprocess
import sys
import tempfile

SR = 44100
LOOP_START = 0.5            # (s into the file: past any encoder delay)
LUFS = -10.0                # (the loudness mastered to: integrated, EBU R128)
MASTER = ("highpass=f=28:poles=2,equalizer=f=280:t=q:w=1.0:g=-2,equalizer=f=3500:t=q:w=0.9:g=2,"
          "treble=g=3.5:f=7500,acompressor=threshold=-24dB:ratio=2.5:attack=20:release=180:knee=6:makeup=1")
# (at four times the rate, so that it holds the peaks between the samples too; -2 dBFS: room for the MP3's own overs)
LIMIT = "aresample=176400,alimiter=limit=0.79:attack=3:release=60:level=disabled:latency=1,aresample=44100"


def ff(pcm, chain):
    """16-bit stereo through an ffmpeg filter chain; back the same, and its integrated loudness (LUFS)"""
    out = subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-f", "s16le", "-ar", str(SR), "-ac", "2",
                          "-i", "-", "-af", chain, "-f", "s16le", "-"], input=pcm.tobytes(), capture_output=True,
                         check=True).stdout
    meas = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-f", "s16le", "-ar", str(SR), "-ac", "2", "-i", "-",
                           "-af", "ebur128", "-f", "null", "-"], input=out, capture_output=True, check=True).stderr
    lufs = float(re.findall(r"I:\s+(-?[\d.]+) LUFS", meas.decode())[-1])
    a = array.array("h")
    a.frombytes(out)
    return a, lufs


def master(y, length):
    """the loop (length frames) mastered, still a loop: its third time through of four"""
    four = y * 4
    _, lufs = ff(four, MASTER)
    gain = LUFS - lufs + 1.0                            # (the limiter takes some back)
    for _ in range(3):
        out, got = ff(four, f"{MASTER},volume={gain:.2f}dB,{LIMIT}")
        gain += LUFS - got
    y = out[2 * 2 * length:2 * 3 * length]
    if len(y) != 2 * length:
        raise SystemExit("make_loop: the mastered audio came back short")
    print(f"make_loop: mastered to {got:.1f} LUFS (EQ and compressor alone: {lufs:.1f})")
    return y


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
    y = master(y, length)
    # the file: the loop's last LOOP_START s, then the loop twice (looping any window of `length` inside it is seamless)
    lead = round(LOOP_START * SR)
    body = y[2 * (length - lead):] + y + y
    with tempfile.NamedTemporaryFile(suffix=".raw") as t:
        t.write(body.tobytes())
        t.flush()
        subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-f", "s16le", "-ar", str(SR), "-ac", "2", "-i", t.name,
                        "-codec:a", "libmp3lame", "-b:a", "256k", out], check=True)
    print(f"make_loop: {bars} bars at {bpm:g} BPM, {length / SR:.4f} s from {s / SR:.3f} s -> {out}")
    print(f"loop start {LOOP_START} s, end {LOOP_START + length / SR:.6f} s")


if __name__ == "__main__":
    main()
