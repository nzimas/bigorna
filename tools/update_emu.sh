#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# BIGORNA: docs/try's emulator (fm1-emu, MIT) from another fm1-emu commit: its web/ files, and its WebAssembly module
# built with its web/build.sh (Rust and the wasm32-unknown-unknown target needed). Then tools/make_site.py, and the
# commit noted in docs/try/README.md.   tools/update_emu.sh COMMIT
set -e
[ -n "$1" ] || { echo "update_emu.sh: COMMIT"; exit 1; }
cd "$(dirname "$0")/.."
T=$(mktemp -d)
git clone -q https://github.com/joakimeriksson/fm1-emu.git "$T/fm1-emu"
git -C "$T/fm1-emu" checkout -q "$1"
"$T/fm1-emu/web/build.sh" "$T/dist" >/dev/null
for f in index.html style.css main.js persistence.js worklet.js jit.js fwsc.js fm1_web.wasm; do
    cp "$T/dist/$f" docs/try/
done
cp "$T/fm1-emu/LICENSE" docs/try/LICENSE
rm -rf "$T"
echo "docs/try: fm1-emu $1 (now: tools/make_site.py, and the commit in docs/try/README.md)"
