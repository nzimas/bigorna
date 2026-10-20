#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# BIGORNA: build, and install only if the build printed its package line and no FAIL; then check that the FM-1
# answers over USB (an install that leaves it mute must be noticed at once).
#   tools/build_install.sh [build.py options, e.g. --telemetry]
cd "$(dirname "$0")/.." || exit 1
out=$(python3 tools/build.py "$@" 2>&1)
echo "$out" | grep -E "relocation|error|loader:|FAIL|package|image"
if echo "$out" | grep -q "FAIL" || ! echo "$out" | grep -q "^package "; then
    echo "build_install: the build failed; nothing installed"
    exit 1
fi
.venv/bin/python tools/fm1_install.py build/bigorna.fwsc --yes 2>&1 | tr '\r' '\n' | grep -v "writing\|checking" | tail -2
sleep 3
if ! .venv/bin/python tools/fm1_install.py --info 2>&1 | tail -1 | grep -q "\[running\]"; then
    echo "build_install: THE FM-1 DOES NOT ANSWER after the install"
    exit 2
fi
echo "build_install: installed, and the FM-1 answers"
