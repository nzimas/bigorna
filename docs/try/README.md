# docs/try: the FM-1 emulator

[fm1-emu](https://github.com/joakimeriksson/fm1-emu), by Joakim Eriksson, MIT licence (`LICENSE`), copied here: its
page, scripts and WebAssembly module, from commit `51ca41af6734ea23871685450fe1f2e1e357bb0e`
(2026-10-24). The scripts are that commit's `web/` files; `fm1_web.wasm` is the build its site published from it.

Not from fm1-emu: `firmwares.json`, the catalog (Bigorna alone, written by `tools/make_site.py`), and
`smk37/firmwares.json` (empty: the page looks for it), `bigorna-demo.bin`, the demo's sessions
(`tools/make_demo.mjs`), and one change in `main.js` (marked BIGORNA): with nothing saved in the browser, the
catalog entry's `flash` sectors are booted with the package, and a save made before them gives way to them (a save
marks its demo). The home page's demo pattern, `docs/assets/demo/`, is recorded in this emulator offline
(`tools/render_demo.mjs`, then `tools/make_loop.py`). `tools/update_emu.sh COMMIT` copies a newer fm1-emu here.
