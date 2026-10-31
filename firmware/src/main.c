/* SPDX-License-Identifier: GPL-3.0-only
 * Boot, the interrupts, the main loop (TONITOR's). The boot order, the TIMER5 and audio ISRs, the crash screen
 * and the update hooks follow SLOOP's / Felucca's main.c and audio.c (Leo Kuroshita, GPL-3.0). */
extern uint32_t _data_start[], _data_end[], _data_load[], _bss_start[], _bss_end[];
extern uint32_t _pool_start[], _pool_end[], _rt_start[], _rt_end[], _rt_load[];
extern uint32_t _ft_start[], _ft_end[], _ft_load[];

/* ------------------------------------------------------------ audio --- */
#ifndef HALF_FRAMES
#define HALF_FRAMES 256u         /* an I2S half buffer: 5.8 ms (fw.c) */
#endif
#define HALF_WORDS (HALF_FRAMES * 2u)
#define OUT_SHIFT 7              /* Q15 -> 24 bit, a -6 dBFS ceiling */
static int32_t abuf[2u * HALF_WORDS] __attribute__((aligned(4)));

static struct {
    volatile uint32_t in_audio;  /* TIMER5 reads it: what may nest in the render */
    uint32_t halves, late, max_us, last_us, cpu_q8;
    uint8_t over;                /* bit k: the half k halves ago was over 90 % */
} dbg;
static volatile uint32_t t5_nested_ticks;

#if TELEMETRY
/* a diagnosis of the audio's time (a TELEMETRY build): each 0.2 s the main loop sends over USB-MIDI (SysEx F0 7D 54,
 * 14-bit values) the highest and the mean load of the I2S halves, the late ones, the voices sounding, the tails let go,
 * each machine's voice cost and where a block's time went (tracks.c tl_*), all in 1/1000 of the time there was.
 * tools/telelog.py reads it, and plays: MIDI channel 1 .. 6 is track 1 .. 6 here; F0 7D 56 t m r F7 gives track t
 * machine m's recipe r; F0 7D 57 t v F7 its level */
static volatile uint32_t tele_max, tele_sum, tele_n, tele_lates;
static void tele_half(uint32_t pct, uint32_t late)
{
    tele_max = pct > tele_max ? pct : tele_max;
    tele_sum += pct;
    tele_n++;
    tele_lates += late;
}
/* the chip's costs (a TELEMETRY build, at start-up, interrupts off): ticks (24 MHz) for 4096 of each -- an add; a
 * load from .bss RAM, from POOL, from the RAM beside the code, from the flash (XIP); the loop's own step (5); a
 * 32 x 32 -> 64 multiply; a divide */
static uint32_t tl_bench[8];
static volatile int32_t tl_sink;
static int16_t tl_bss[1024];
static void tl_bench_run(void)                   /* (before the interrupts are on) */
{
    uint32_t t, i;
    int32_t acc = 1;
#define TB(k, body) do { t = fm1_ticks(); for (i = 0; i < 4096u; i++) { body; } tl_bench[k] = fm1_ticks() - t; } while (0)
#define KEEP(x) ((x) = fm1_keep(x))
    TB(0, { acc += (int32_t)i; KEEP(acc); });                                   /* the loop alone (+ an add) */
    TB(1, { acc += (int32_t)i; KEEP(acc); acc += 3; KEEP(acc); acc += 5; KEEP(acc); acc += 7; KEEP(acc); });   /* 4 adds */
    TB(2, { acc += (int32_t)i; tl_bss[0] = (int16_t)acc; KEEP(acc); });        /* + a store */
    TB(3, { acc += (int32_t)i; tl_bss[0] = (int16_t)acc; tl_bss[1] = (int16_t)acc; tl_bss[2] = (int16_t)acc;
            tl_bss[3] = (int16_t)acc; KEEP(acc); });                             /* + 4 stores */
    TB(4, { acc += tl_bss[i & 1023u]; KEEP(acc); });                             /* + a load */
    TB(5, { acc += tl_bss[i & 1023u] + tl_bss[(i + 7u) & 1023u] + tl_bss[(i + 99u) & 1023u] + tl_bss[(i + 301u) & 1023u];
            KEEP(acc); });                                                       /* + 4 loads */
    TB(6, { if (acc & 1) acc += 3; else acc += 1; KEEP(acc); });                 /* + a branch on the data */
    TB(7, { acc = (int32_t)(((int64_t)acc * 23456) >> 15) + 7; KEEP(acc); acc = (int32_t)(((int64_t)acc * 23456) >> 15) + 7;
            KEEP(acc); });                                                       /* + 2 dependent 64-bit multiplies */
#undef KEEP
#undef TB
    tl_sink = acc;
    {   /* (a sound of each machine, drawn as HOME would: its chain's cook and its machine's, 64 times each) */
        static sound_t s;
        static vck_t vc;
        static mck_t mc;
        uint32_t m;
        for (m = 0; m < NENG; m++) {
            gen_sound(&s, m, -1, -1, 1234u + m);
            t = fm1_ticks();
            for (i = 0; i < 64u; i++)
                vck_cook(&vc, &s);
            tl_cook[m] = (fm1_ticks() - t) / 64u;
            t = fm1_ticks();
            for (i = 0; i < 64u; i++)
                mck_cook(&mc, &s);
            tl_cook[4u + m] = (fm1_ticks() - t) / 64u;
        }
    }
}
static void tele_put(uint8_t *f, uint32_t *n, uint32_t v)
{
    v = v > 16383u ? 16383u : v;
    f[(*n)++] = (uint8_t)(v >> 7);
    f[(*n)++] = (uint8_t)(v & 0x7Fu);
}
static void tele_send(void)
{
    static uint32_t sent_at, tele_k;
    uint8_t f[180];
    uint32_t n = 0, k, nv = 0, nb;
    if ((uint32_t)(fm1_ms - sent_at) < 200u || !usb.config)
        return;
    sent_at = fm1_ms;
    if (++tele_k % 5u == 0u) {                   /* (each fifth: the crash record, a message of its own: F0 7D 59) */
        f[n++] = 0xF0; f[n++] = 0x7D; f[n++] = 0x59;
        {   /* (the last crash's record, .noinit: across the reset it caused) and CPU1's last fault */
            uint32_t w[18] = {fm1_crash.magic == FM1_CRASH_MAGIC ? fm1_crash.count : 0u, fm1_crash.vec, fm1_crash.pc, fm1_crash.dbg,
                              fm1_crash.emu, fm1_core1_fault.dbg}, j;
            for (j = 0; j < 2u; j++) {               /* (each core's last entry into the handler) */
                const fm1_fault_seen_t *z = &fm1_fault_seen[j];
                int ok = z->magic == 0x53454E31u;
                w[6u + 6u * j] = ok ? z->n : 0u;
                w[7u + 6u * j] = ok ? z->cpu : 0u;
                w[8u + 6u * j] = ok ? z->dbg : 0u;
                w[9u + 6u * j] = ok ? z->emu0 : 0u;
                w[10u + 6u * j] = ok ? z->emu1 : 0u;
                w[11u + 6u * j] = ok ? z->pc : 0u;
            }
            for (j = 0; j < 18u; j++) {
                tele_put(f, &n, w[j] >> 18);
                tele_put(f, &n, (w[j] >> 4) & 0x3FFFu);
                tele_put(f, &n, w[j] & 15u);
            }
        }
        f[n++] = 0xF7;
        if (so_w - so_r + (n + 2u) / 3u < SXQ)
            ota_wire_send(f, n);
        return;
    }
    for (k = 0; k < NVOICE; k++)
        nv += vox[k].sounding;
    f[n++] = 0xF0; f[n++] = 0x7D; f[n++] = 0x54;
    nb = tl_n ? tl_n : 1u;
    tele_put(f, &n, tele_max);
    tele_put(f, &n, tele_n ? tele_sum / tele_n : 0u);
    tele_put(f, &n, tele_lates);
    tele_put(f, &n, nv);
    tele_put(f, &n, tl_shed);
    for (k = 0; k < 4u; k++)
        tele_put(f, &n, eng_cost[k] * 1000u / BLK_TICKS);
    for (k = 0; k < NTRACK; k++)
        tele_put(f, &n, tl_trk[k] / nb * 1000u / BLK_TICKS);
    tele_put(f, &n, tl_fx / nb * 1000u / BLK_TICKS);
    tele_put(f, &n, tl_all / nb * 1000u / BLK_TICKS);
    tele_put(f, &n, tl_max * 1000u / BLK_TICKS);
    for (k = 0; k < NENG; k++)                   /* (a block's machine and chain time, all voices of each machine) */
        tele_put(f, &n, tl_mach[k] / nb * 1000u / BLK_TICKS);
    for (k = 0; k < NENG; k++)
        tele_put(f, &n, tl_chain[k] / nb * 1000u / BLK_TICKS);
    for (k = 0; k < 4u; k++)
        tele_put(f, &n, tl_fxk[k] / nb * 1000u / BLK_TICKS);
    for (k = 0; k < 8u; k++)                     /* (the chip's costs, measured at start-up) */
        tele_put(f, &n, tl_bench[k]);
    tele_put(f, &n, (uint32_t)audio_worker_online | (uint32_t)fm1_power_ready << 1 | (uint32_t)fm1_core1_barred() << 2);
    tele_put(f, &n, audio_worker.jobs & 0x3FFFu);   /* (CPU1: its jobs, mod 16384) */
    tele_put(f, &n, fm1_aw_load(&audio_worker.rejected));
    tele_put(f, &n, fm1_core1_fault.count);
    tele_put(f, &n, audio_worker.max_wait_ticks * 1000u / BLK_TICKS);
    tele_put(f, &n, audio_worker.timeouts);
    tele_put(f, &n, tl_c1 / nb * 1000u / BLK_TICKS); /* (a block's time CPU1 rendered, 1/1000) */
    tele_put(f, &n, tl_evmax * 1000u / BLK_TICKS);  /* (the worst block's: events; + the sequencer and guard) */
    tele_put(f, &n, tl_seqmax * 1000u / BLK_TICKS);
    tele_put(f, &n, tl_par / nb * 1000u / BLK_TICKS);   /* (the voices' phase, both cores: mean, worst) */
    tele_put(f, &n, tl_parmax * 1000u / BLK_TICKS);
    tele_put(f, &n, tl_steal);                   /* (voices taken while sounding; notes with no voice) */
    tele_put(f, &n, tl_lost);
    tele_put(f, &n, tl_stealmax >> 2);
    tl_steal = tl_lost = tl_stealmax = 0;
    tl_evmax = tl_seqmax = tl_par = tl_parmax = 0;
    audio_worker.max_wait_ticks = 0;
    tl_c1 = 0;
    for (k = 0; k < NENG; k++)
        tl_mach[k] = tl_chain[k] = 0;
    for (k = 0; k < 4u; k++)
        tl_fxk[k] = 0;
    tele_max = tele_sum = tele_n = tele_lates = 0;
    for (k = 0; k < NTRACK; k++)
        tl_trk[k] = 0;
    tl_fx = tl_all = tl_n = tl_max = tl_shed = 0;
    f[n++] = 0xF7;
    if (so_w - so_r + (n + 2u) / 3u < SXQ)      /* (room: else this report is dropped, never waited for) */
        ota_wire_send(f, n);
}
#endif

void fm1_alnk0_irq(void)         /* via isr_alnk0 (hal/fm1_isr.S) */
{
    uint8_t p;
    uint32_t t0;
    dbg.in_audio = 1;
    p = fm1_audio_pending();
    t0 = fm1_ticks();
    t5_nested_ticks = 0;
    fm1_audio_ack_aux(p);
    if (p & FM1_AUDIO_HALF) {
        uint32_t half = fm1_audio_free_half(), b, i, all, us;
        int32_t *o = &abuf[half * HALF_WORDS];
#if FELUCCA_UAC
        uac_render_start();                      /* (USB audio: the ring checked, primed if the host starts reading) */
#endif
        for (b = 0; b < HALF_FRAMES; b += CTL) {
            mix_block(o + 2u * b);
#if FELUCCA_UAC
            uac_tap(usb_blk, CTL);               /* (the block, Q15, to the host -- before the MASTER knob: usb.c) */
#endif
            for (i = 0; i < 2u * CTL; i++)
                o[2u * b + i] <<= OUT_SHIFT;
        }
        fm1_audio_ack_half();
        all = fm1_ticks() - t0;
        us = (all - t5_nested_ticks) / FM1_TICKS_PER_US;
        {
            uint32_t pc = all / FM1_TICKS_PER_US * 100u / (HALF_FRAMES * 1000000u / FS);
            dbg.over = (uint8_t)(dbg.over << 1 | (pc > 90u));
            if (pc >= 100u)
                shed_req = 2;                    /* late: something goes, a held note if it must (layers.c) */
            else if ((dbg.over & 3u) == 3u && !shed_req)
                shed_req = 1;                    /* two near the edge in a row: a tail goes */
            load_pct = (uint8_t)(pc > 255u ? 255u : pc);
        }
#if TELEMETRY
        tele_half(all * 1000u / (HALF_FRAMES * 1000000u / FS * FM1_TICKS_PER_US), fm1_audio_free_half() != half);
#endif
        dbg.cpu_q8 = (dbg.cpu_q8 * 15u + (us * 256u) / (HALF_FRAMES * 1000000u / FS)) / 16u;
        if (us > dbg.max_us)
            dbg.max_us = us;
        dbg.last_us = us;
        if (fm1_audio_free_half() != half)
            dbg.late++;
        dbg.halves++;
    }
    dbg.in_audio = 0;
}
extern void isr_alnk0(void);

static void audio_init(void)
{
    uint32_t i;
    for (i = 0; i < 2u * HALF_WORDS; i++)
        abuf[i] = 0;
    fm1_audio_init(abuf, HALF_WORDS, isr_alnk0, 3);
}

/* --------------------------------------- TIMER5: the panel, time, USB --- */
/* 10 kHz, above the audio (it may nest in the render: the scan keeps its pace, the LEDs do not shimmer);
 * USB is polled at 2 kHz, never nested -- but for its audio endpoint, which is served nested too (a packet a 1 ms
 * frame: the host must not find it empty while a half renders, 5.8 ms; usb.c uac_service); the TRS MIDI IN jack at 2 kHz too, nested as well (as MELODEE does: a clock
 * byte timed as it comes, not after the render; it touches only its DMA ring, the MIDI ring and midi.c's clock) */
void fm1_timer5_irq(void)
{
    static uint32_t sub, owed, last, acc;
    uint32_t t0 = fm1_ticks();
    fm1_timer5_ack();
    fm1_input_tick();
    acc += t0 - last;
    last = t0;
    while (acc >= 1000u * FM1_TICKS_PER_US) {
        acc -= 1000u * FM1_TICKS_PER_US;
        fm1_ms++;
    }
    if (sub % 5u == 0u)
        owed |= 1u;
    if (sub % 5u == 2u)
        uart_midi_poll();                        /* (the TRS jack: build 47) */
    if (++sub == 10u)
        sub = 0;
    if (dbg.in_audio) {
#if FELUCCA_UAC
        if (owed & 1u)
            uac_service();                       /* (owed stays: usb_poll runs after the render, as before) */
#endif
        t5_nested_ticks += fm1_ticks() - t0;
        return;
    }
    if (owed & 1u)
        usb_poll();
    owed = 0;
}
extern void isr_timer5(void);

/* -------------------------------------------------------------- faults --- */
static void hexs(char *b, uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        b[i] = "0123456789ABCDEF"[(v >> (28u - 4u * i)) & 15u];
    b[8] = 0;
}
static void fm1_fault(const fm1_crash_t *c)
{
    char b[12];
    uint32_t t0;
    fm1_audio_stop();
    lcd_fill(0, 0, 240, 240, RGB(150, 0, 0));
    draw_text_box(0, 20, 240, &FONT_M, "CRASH", C_WHITE, 1);
    hexs(b, c->vec);
    draw_text_box(0, 80, 240, &FONT_M, b, C_WHITE, 1);
    hexs(b, c->pc);
    draw_text_box(0, 130, 240, &FONT_M, b, C_WHITE, 1);
    t0 = fm1_ticks();
    while ((uint32_t)(fm1_ticks() - t0) < 4000u * 1000u * FM1_TICKS_PER_US)
        ;
    fm1_reboot();
}

/* ------------------------------------------- the flash and the update --- */
#include "fm1_flash.h"
static uint8_t flash_ok;
static uint8_t recovery_active;
static int st_read(uint32_t off, void *dst, uint32_t n)   /* 256-byte IRQ-off windows: the audio keeps up */
{
    uint8_t *d = dst;
    while (n) {
        uint32_t k = n > 256u ? 256u : n, f = irq_save();
        int rc = FL_FAR(fl_read_ram)(off, d, k);
        irq_restore(f);
        if (rc)
            return rc;
        off += k;
        d += k;
        n -= k;
    }
    return 0;
}
/* a sector erase with the audio silenced in the same interrupts-off window (else the DMA loops stale audio) */
static int fl_erase4k_quiet(uint32_t off, uint32_t *took)
{
    uint32_t f = irq_save(), i;
    int rc;
    for (i = 0; i < sizeof abuf / sizeof abuf[0]; i++)
        abuf[i] = 0;
    rc = FL_FAR(fl_erase4k_ram)(off, took);
    irq_restore(f);
    return rc;
}

#define OTA_IDENTITY (recovery_active ? "FM-1_000" : FW_ID)
#include "ota.c"
static int bk_wire(const uint8_t *p, uint32_t n) { return ota_wire_send(p, n); }   /* (backup.c's way out) */
static void recovery_poll(void);
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void)
{
    fm1_wdt_feed();
    if (recovery_active)
        recovery_poll();
}
static int ota_in_area(uint32_t off, uint32_t n) { return FL_IN(off, n, OTA_AREA, OTA_AREA + OTA_AREA_LEN); }
static int ota_erase(uint32_t off)
{
    uint32_t took;
    if (!ota_in_area(off, 0x1000u) || (off & 0xFFFu))
        return -8;
    return fl_erase4k_quiet(off, &took);
}
static int ota_prog(uint32_t off, const void *p, uint32_t n)
{
    if (!ota_in_area(off, n))
        return -8;
    return fl_write(off, p, n);
}
static int ota_fread(uint32_t off, void *p, uint32_t n) { return st_read(off, p, n); }
static void ota_show(uint32_t step, int32_t code)
{
    static const char *const STEP[] = {"", "PACKAGE", "CHECK", "LOADER", "CONFIRM", "RESTART"};
    char b[16];
    if (recovery_active)
        return;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 60, 240, &FONT_L, "UPDATE", C_WHITE, 1);
    if (step < 9u) {
        draw_text_box(0, 130, 240, &FONT_M, STEP[step < 6u ? step : 0], RGB(255, 214, 0), 1);
        return;
    }
    if (code == 1)
        str_cpy(b, "DRY RUN OK", sizeof b);
    else {
        uint32_t k = 0, v = (uint32_t)-code;
        str_cpy(b, "FAILED -", sizeof b);
        while (b[k])
            k++;
        if (v >= 10u)
            b[k++] = (char)('0' + v / 10u);
        b[k++] = (char)('0' + v % 10u);
        b[k] = 0;
    }
    draw_text_box(0, 130, 240, &FONT_M, b, RGB(255, 214, 0), 1);
    fm1_delay_ms(1500);
}
static void ota_commit(const uint8_t *parm)
{
    bootguard_clear(&bootguard);                     /* an intended reset */
    usb_detach();
    fm1_delay_ms(30);
    fm1_enter_update(parm);
}
#include "recovery.c"

/* the projects' flash (projects.c): their area only */
#define PRJ_AREA_LO 0x00097000u
#define PRJ_AREA_HI 0x000E0000u                  /* (the sessions; then the loop store's first 9 sectors) */
#define PRJ_AREA2_LO 0x000E5000u                 /* (past the update's staging: the loop store's other 22, and the
                                                  * last session's note at 0xFC000) */
#define PRJ_AREA2_HI 0x000FE000u                 /* (.. and GLOBAL SETUP's at 0xFD000: setup.c) */
#define PRJ_IN(off, n) (FL_IN(off, n, PRJ_AREA_LO, PRJ_AREA_HI) || FL_IN(off, n, PRJ_AREA2_LO, PRJ_AREA2_HI))
static int prj_flash_read(uint32_t off, void *p, uint32_t n) { return st_read(off, p, n); }
static int prj_flash_erase(uint32_t off)
{
    uint32_t took;
    if (!PRJ_IN(off, 0x1000u) || (off & 0xFFFu))
        return -8;
    return fl_erase4k_quiet(off, &took);
}
static int prj_flash_prog(uint32_t off, const void *p, uint32_t n)
{
    if (!PRJ_IN(off, n))
        return -8;
    return fl_write(off, p, n);
}

/* ------------------------------------------------------------ the loop --- */
static void splash(void)
{
    splash_draw(FW_VERSION);                         /* (the logo: logo.c) */
}

static void enter_uboot(const char *why)
{
    fm1_audio_stop();
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 104, 240, &FONT_M, why, RGB(80, 140, 255), 1);
    fm1_delay_ms(20);
    usb_detach();
    fm1_delay_ms(30);
    bootguard.pending = 0;                           /* an intended reset: not a failed boot */
    fm1_enter_uboot();
}



static void fm1_main(void)
{
    int32_t knob = 512 * 16;
    uint32_t healthy_since, t_both = 0;
    {
        uint32_t f = irq_save();
        flash_ok = FL_FAR(fl_jedec_ram)() == 0x856014u;   /* the expected 1 MiB part */
        irq_restore(f);
    }
    if (flash_ok)
        ota_boot_cleanup();                          /* a staging area left by an update */

    lcd_init();
    splash();
    fm1_input_init();
    fm1_adc_init();
    prj_scan(flash_ok);                              /* the projects in flash */
    setup_load();                                    /* GLOBAL SETUP (the MIDI settings) as last left */
#if TELEMETRY
    tl_bench_run();
#endif
    tracks_init();
    ui_init();                                       /* (a sound of each machine for every track) */
    if (!bootguard.failed)                           /* (after a boot that failed: not, in case it was the session) */
        prj_boot();                                  /* the last session used, loaded again */
    /* CPU1: after a supply setup that held, on a boot after no failed one, and not barred since power-on. It starts
     * through the mask ROM before it reaches our RAM loop: the PC limit only after (Melodee) */
    if (fm1_core1_boot_allowed(fm1_power_ready, bootguard.failed))
        fm1_multicore_start();
    fm1_guard_enable(FM1_GUARD_PC);
    if (!audio_worker_online)                        /* (CPU1 held: until power-off after a fault, or a failed boot --
                                                      * half the voices' room. Said, not silent) */
        say("1 CORE", 4000);
    audio_init();
    usb_start();
    uart_midi_init();                                /* the TRS MIDI IN jack (before TIMER5: PORTH) */
    fm1_timer5_start(isr_timer5, 4);
    fm1_guard_lock_top();
    fm1_irq_enable_all();
    fm1_delay_ms(700);
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.force = 1;
    healthy_since = fm1_ms;
    for (;;) {
        uint32_t m = fm1_ms;
        fm1_wdt_feed();
        usb_retry(fm1_ms);
        if ((uint32_t)(m - healthy_since) >= 30000u && bootguard.pending)
            bootguard_clear(&bootguard);             /* 30 s of this boot: a good one */
        {   /* the battery's divider (ADC 3), smoothed slowly (build 56; as MELODEE's) */
            int32_t b = fm1_adc_read(FM1_ADC_BATT);
            if (b > 0)
                batt_raw = batt_raw ? batt_raw + (b - batt_raw) / 32 : b;
        }
        {
            int32_t a = fm1_adc_read(FM1_ADC_MASTER);
            if (a >= 0) {
                uint32_t k10;
                knob += (a * 16 - knob) / 8;
                k10 = (uint32_t)(knob / 16);
                master_q12 = (int32_t)((k10 * k10) >> 8);   /* 0 .. ~4096 */
            }
        }
        {   /* OCT- and OCT+ held 5 s: UBOOT, the chip's own updater (a way back whatever happens) */
            uint32_t both = (1u << PANEL_BTN[B_OCTDN]) | (1u << PANEL_BTN[B_OCTUP]);
            if ((fm1_in.buttons & both) != both)
                t_both = fm1_ms;
            else if ((uint32_t)(fm1_ms - t_both) > 5000u)
                enter_uboot("UBOOT");
        }
        ota_service();                               /* the installer's handshake */
        if (usb.ota_req) {
            uint32_t l;
            usb.ota_req = 0;
            for (l = 0; l < NTRACK; l++)
                ev_post(EV_STOP, l, 0, 0);
            if (flash_ok)
                ota_session();                       /* returns only if nothing was committed */
            lcd_fill(0, 0, 240, 240, C_BLACK);
            ui.force = 1;
        }
        if (usb.uboot_req)
            enter_uboot("UBOOT (USB)");
        ui_input();
        ui_leds();
        ui_draw();
#if TELEMETRY
        tele_send();
#endif
        ui_publish();
        while ((uint32_t)(fm1_ms - m) < 15u)        /* ~60 frames a second at most */
            ui_input();
        ui_publish();
    }
}

void fm1_cstart(void)
{
    uint32_t *s, *d, p3, src, wdt, boot_mode;
    fm1_time_init();
    fm1_reset_reason();
    p3 = fm1_boot.p3_rst;
    src = fm1_boot.rst_src;
    wdt = fm1_boot.wdt_con;
    fm1_wdt_arm(0x0D);
    fm1_core1_stop();                                /* CPU1 held before the RAM is cleared and copied (Melodee) */
    if ((p3 & 1u) && !(p3 & (4u | 0x40u)))
        bootguard_clear(&bootguard);                /* a power-on: no failed boot to count */
    boot_mode = bootguard_begin(&bootguard);
    if (boot_mode == BOOT_ROM)
        fm1_enter_uboot();
    fm1_irq_init();
    for (d = _bss_start; d < _bss_end; d++)
        *d = 0;
    for (d = _pool_start; d < _pool_end; d++)
        *d = 0;
    for (s = _data_load, d = _data_start; d < _data_end; s++, d++)
        *d = *s;
    for (s = _rt_load, d = _rt_start; d < _rt_end; s++, d++)
        *d = *s;                                     /* the flash driver, which runs from RAM */
    for (s = _ft_load, d = _ft_start; d < _ft_end; s++, d++)
        *d = *s;                                     /* the audio's hottest code, run from RAM too */
    fm1_mailbox_clear();
    fm1_power_ready = (uint8_t)fm1_power_init();   /* stock V15's supplies, before CPU1 (Melodee: its RAM reads) */
    fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS);   /* (the PC limit once CPU1 has started) */
    fm1_boot.p3_rst = (uint8_t)p3;
    fm1_boot.rst_src = src;
    fm1_boot.wdt_con = (uint8_t)wdt;
    if (boot_mode == BOOT_RECOVERY || recovery_key()) {
        fm1_guard_enable(FM1_GUARD_PC);
        recovery_main();                             /* two failed boots, or OCT- held at power-on */
    }
    fm1_main();
    for (;;)
        ;
}
