#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Host tests (no toolchain, no FM-1 needed): the whole firmware checked for a runtime the chip lacks, then the
# machines, the effects, the patch generator and the screen run on the computer. Renders land in build/sounds
# (WAV) and build/screens (PPM).
set -e
cd "$(dirname "$0")/.."
PY=python3
[ -x .venv/bin/python ] && PY=.venv/bin/python
mkdir -p build/gen build/sounds build/screens
$PY tools/gen_tables.py build/gen/fw_tables.h >/dev/null
$PY tools/gen_font.py build/gen/fw_font.h >/dev/null
tests/check_target_ir.sh
clang -O2 -std=gnu99 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function -Werror \
    -Ifirmware/src -Ibuild/gen tests/host/host.c -o build/host
build/host check
build/host screens build/screens
build/host projects build/screens
build/host lfo build/screens
build/host trn build/screens
build/host arp build/screens
build/host newses build/screens
if command -v node >/dev/null; then                  # the backup page's protocol against the firmware's (build 55)
    node tests/web/test_backup.mjs build/host
fi
