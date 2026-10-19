# Licensing

The firmware is free software, licensed under the GNU General Public License, version 3 only (`GPL-3.0-only`,
full text in `LICENSE`). If you distribute it, or firmware built from it, give your recipients its complete
corresponding source under the same licence.

It is a work based on **Tonitor** (Nuno Zimas), itself based on **Felucca** (Copyright (C) 2026 Leo Kuroshita
(@kurogedelic), Hügelton Instruments) and **SLOOP** (isod89), all GPL-3.0-only. Files taken from them keep their
copyright headers; changes are noted in each file.

| What | Licence | Where |
| --- | --- | --- |
| This firmware's own code: `firmware/src/{fw,voice,osc,fmt,fmd,wav,fx,tracks,params,gen,ui,main}.c`, `sound.h`, `dsp.h`, `tools/{build,gen_tables,gen_font,sdk_from_package}.py`, `tests/`, the scripts | GPL-3.0-only | |
| From Tonitor: `gfx.c`, parts of `ui.c`, `main.c`, `tracks.c` (its voice pool and budget), `fx.c` (its plate reverb), `dsp.h` | GPL-3.0-only | |
| From Felucca / SLOOP: `firmware/hal/`, `firmware/loader/`, `firmware/crt0.S`, `firmware/app.ld`, `firmware/src/{usb,ota,recovery,lcd,libc}.c`, `bootguard.h`, `tools/{fm1pkg_make,lz4blk,fm1_install}.py`, `tools/get_toolchain.sh`, parts of `tools/build.py` and `tools/gen_tables.py` | GPL-3.0-only | |
| The patch recipes (`gen.c`): ported from SynthSeeder, by the same author | GPL-3.0-only (in this firmware) | |
| Terminus Bold 8x16, Atkinson Hyperlegible Bold (the screen's type) | SIL OFL 1.1 | `assets/fonts/` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK file is in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

The machines follow the behaviour and parameters of the Elektron Digitone II as its manual describes them and as
its OS 1.12 displays them (algorithm diagrams, ratio lists, value ranges, read from the owner's own copy of the
firmware). No Elektron code or data is copied into this firmware. Elektron and Digitone are trademarks of Elektron
Music Machines; this firmware is not made, endorsed or supported by Elektron or by M-VAVE.
