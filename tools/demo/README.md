# tools/demo: what the home page's demo pattern is recorded with

`fm1_web.wasm` and `jit.js` are [fm1-emu](https://github.com/joakimeriksson/fm1-emu) (Joakim Eriksson, MIT licence,
`LICENSE`): `jit.js` from commit `51ca41af6734ea23871685450fe1f2e1e357bb0e`, `fm1_web.wasm` the build its site published
from it. `session-N.bin`: three sessions from an FM-1 as flash sectors, booting into session N
(`tools/make_demo.mjs BACKUP.json N 1`).

    node tools/render_demo.mjs 7 40 build/s3.raw tools/demo/session-3.bin
    tools/make_loop.py build/s3.raw 8 170 docs/assets/demo/session-3.mp3
    node tools/render_demo.mjs 1 40 build/s2.raw tools/demo/session-2.bin
    tools/make_loop.py build/s2.raw 16 140 docs/assets/demo/session-2.mp3
    node tools/render_demo.mjs 1 40 build/s1.raw tools/demo/session-1.bin
    tools/make_loop.py build/s1.raw 8 135 docs/assets/demo/session-1.mp3

Then each loop's end in `site/home.html` (`data-loop-end`), as `make_loop.py` prints it. (Session 2 is 16 bars: its
trig conditions repeat over 16, not 8.)
