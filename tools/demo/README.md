# tools/demo: what the home page's demo pattern is recorded with

`fm1_web.wasm` and `jit.js` are [fm1-emu](https://github.com/joakimeriksson/fm1-emu) (Joakim Eriksson, MIT licence,
`LICENSE`): `jit.js` from commit `51ca41af6734ea23871685450fe1f2e1e357bb0e`, `fm1_web.wasm` the build its site published
from it. `bigorna-demo.bin` holds three sessions from an FM-1 as flash sectors (`tools/make_demo.mjs`).

    node tools/render_demo.mjs 7 40 build/p7.raw
    tools/make_loop.py build/p7.raw 8 170 docs/assets/demo/pattern-7.mp3

Then the loop's end in `site/home.html` (`data-loop-end`), as `make_loop.py` prints it.
