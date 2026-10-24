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
| From **Melodee** 1.4.5 (Copyright (C) 2026 Kerem Kilic (Ellic Studio), keremimo/melodee): the dual-core start, hold and fault handling, its job mailbox and the stock supply setup — `firmware/hal/{fm1_multicore,fm1_core1_stop,fm1_core1_bar,fm1_power}.h`, `firmware/src/audio_worker.h`, the CPU1 parts of `firmware/hal/{fm1_irq,fm1_sys}.h`, `fm1_vec.S` and `crt0.S`; the TRS MIDI IN parser, `firmware/src/midi_uart.c` | GPL-3.0-only | <https://github.com/keremimo/melodee> |
| After the **LXR** firmware (Copyright (C) 2012-2026 Erica Synths, GPL-3.0-only, erica-synths/lxr-desktop-app): OVERDRIVE's RAT and FOLD and its 1N914 diode table (`OD_DIODE`, from its `FX/Distortion.c`) in `voice.c`; DRUM's AB NOISE in `fmd.c` (after its noise oscillator). Not made, endorsed or supported by Erica Synths | GPL-3.0-only | <https://github.com/erica-synths/lxr-desktop-app> |
| The patch recipes (`gen.c`): ported from SynthSeeder, by the same author | GPL-3.0-only (in this firmware) | |
| Terminus Bold 8x16, Atkinson Hyperlegible Bold (the screen's type) | SIL OFL 1.1 | `assets/fonts/` |
| Michroma Regular (the logo's lettering, turned into paths by `tools/make_logo.py`; the font is not in the logo files) | SIL OFL 1.1 | `assets/fonts/`, <https://github.com/google/fonts/tree/main/ofl/michroma> |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK file is in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

The machines follow the behaviour and parameters of the Elektron Digitone II as its manual describes them and as
its OS 1.12 displays them (algorithm diagrams, ratio lists, value ranges, read from the owner's own copy of the
firmware). No Elektron code or data is copied into this firmware. Elektron and Digitone are trademarks of Elektron
Music Machines; this firmware is not made, endorsed or supported by Elektron or by M-VAVE.
