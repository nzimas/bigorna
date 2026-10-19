# Bigorna

An alternative firmware for the M-VAVE FM-1 after the synthesis of the Elektron Digitone II: six tracks, the FM
TONE, FM DRUM, WAVETONE and SWARMER machines, their filters, envelopes and send effects. Built on Tonitor.
Unofficial: not made, endorsed or supported by M-VAVE or Elektron. Installing it may void any warranty.

Build and install (as Tonitor's): `./setup.sh` once, then `./build.sh` (writes `build/bigorna.fwsc`), then
`./install.sh --no-build` with the FM-1 connected. Rescue mode: hold OCT- while switching on.

Versions are build numbers: `BUILD` in `tools/build.py` (identity FM-1_8 and the build, three digits).

Host tests (no FM-1 or toolchain needed): `tests/run_tests.sh` -- renders every recipe through the real mixer and
checks for silence, clipping, DC and runaway tails, and draws the screens to `build/screens`.

See [CHANGELOG.md](CHANGELOG.md) for what is in each build and the controls.
