/* SPDX-License-Identifier: GPL-3.0-only
 * BIGORNA -- a firmware for the M-VAVE FM-1 after the synthesis of the Elektron Digitone II: six tracks,
 * the 4-OP, DRUM, WAVY and HIVE machines, their filters, envelopes and effects.
 * One compilation unit (the HAL is header-only); the order matters.
 *
 * Built on TONITOR (Nuno Zimas), itself built on Felucca (Leo Kuroshita, Hügelton Instruments) and SLOOP
 * (isod89), GPL-3.0: the HAL, the boot, the USB stack, the update entry and loader, the rescue mode. */
#include <stdint.h>
#ifndef FW_ID
#define FW_ID "FM-1_8058"         /* the package identity (tools/build.py sets it) */
#endif
#ifndef FW_VERSION
#define FW_VERSION "BUILD 58"
#endif
#define FW_NAME "BIGORNA"
#define FELUCCA_ID FW_ID
#ifndef TELEMETRY
#define TELEMETRY 0              /* 1: the audio's timing reported over USB-MIDI SysEx (a diagnostic build) */
#endif
#define FELUCCA_OTA 1            /* the update entry (the installer), and the rescue mode */
#define FELUCCA_OTA_DRYRUN 0
#define FELUCCA_CDC 0
#define FELUCCA_UAC 1            /* USB audio out to the host: the master, stereo, 44.1 kHz (build 49; usb.c) */
#define FELUCCA_UAC_48K 0        /* (no 48 kHz: the resampler cost 6.7 % of the CPU, and 44.1 is what is rendered) */
#define HALF_FRAMES 256u         /* an I2S half buffer: 5.8 ms (main.c renders it; usb.c's audio ring is sized on it) */
#define FM1_CORE1 1              /* CPU1 renders half the voices (BIGORNA build 32; hal/fm1_multicore.h) */

#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_power.h"
#include "fm1_irq.h"
#include "fm1_core1_bar.h"
#include "fm1_multicore.h"
#include "fm1_guard.h"
#include "fm1_input.h"
#include "fm1_timer.h"
#include "fm1_audio.h"
#include "fm1_adc.h"
#include "fm1_lcd_hw.h"
#define RAMTAB __attribute__((section(".data")))   /* (a table read every sample: in RAM, gen_tables.py) */
#include "fw_tables.h"

#define NELEM(a) (sizeof(a) / sizeof((a)[0]))
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")   /* a slot's store before the index's */
static volatile uint32_t fm1_ms;                           /* milliseconds since boot (TIMER5) */
#include "bootguard.h"
bootguard_t bootguard __attribute__((section(".noinit")));

#include "libc.c"
#include "lcd.c"
#include "dsp.h"
#include "gfx.c"
#include "sound.h"
#include "voice.c"
#include "osc.c"
#include "fmt.c"
#include "fmd.c"
#include "wav.c"
#include "fx.c"
#include "mangle.c"
#include "usb.c"
#include "midi.c"
#include "midi_uart.c"
#include "tracks.c"
#include "params.c"
#include "gen.c"
#include "ui.c"
#include "projects.c"
#include "setup.c"
#include "backup.c"
#include "phrase.c"
#include "main.c"
