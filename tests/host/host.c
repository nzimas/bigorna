/* SPDX-License-Identifier: GPL-3.0-only
 * Host harness: the firmware's own DSP, tracks, effects, patch generator and screen, compiled for the computer.
 *
 *   host render DIR [SEEDS]   every recipe (gen.c), SEEDS sounds each (default 2): a note held 1 s and let go,
 *                             3 s through the real mixer and effects, a WAV each and a line of figures
 *   host check                every recipe, 12 seeds: none silent, none clipping hard, none running away, none
 *                             with DC; exits 1 on a failure
 *   host screens DIR          the screen in several states, as PPM images
 *   host phrases DIR [N]      N starts (default 4), a phrase on every track, 8 s played: a WAV each, its peak and
 *                             loudness
 * Build: tests/run_tests.sh */
#define HOST 1
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define RAMTAB
#include "fw_tables.h"

#define NELEM(a) (sizeof(a) / sizeof((a)[0]))
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define FM1_TICKS_PER_US 24u
static volatile uint32_t fm1_ms;
static uint32_t host_ticks;
static uint32_t fm1_ticks(void) { return host_ticks += 1u; }

static int str_eq(const char *a, const char *b) { return strcmp(a, b) == 0; }
static unsigned str_len(const char *s) { return (unsigned)strlen(s); }
static void str_cpy(char *d, const char *s, unsigned max)
{
    unsigned i = 0;
    while (s[i] && i + 1 < max) {
        d[i] = s[i];
        i++;
    }
    d[i] = 0;
}
static void fmt_fix(char *b, int32_t v, int decimals)
{
    char t[12];
    int n = 0, neg = v < 0, i;
    uint32_t u = (uint32_t)(neg ? -v : v);
    do {
        t[n++] = (char)('0' + u % 10u);
        u /= 10u;
        if (n == decimals)
            t[n++] = '.';
    } while (u || n <= decimals);
    if (neg)
        t[n++] = '-';
    for (i = 0; i < n; i++)
        b[i] = t[n - 1 - i];
    b[n] = 0;
}
static void fmt_int(char *b, int32_t v) { fmt_fix(b, v, 0); }

static uint16_t fb[240 * 240];
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u) {
                uint16_t c = px[j * w + i];
                fb[(y + j) * 240u + x + i] = (uint16_t)((c >> 8) | (c << 8));
            }
}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u)
                fb[(y + j) * 240u + x + i] = c;
}

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
#define MQ 64u
static uint32_t midi_in_q[MQ];
static volatile uint32_t mi_w, mi_r;
/* (usb.c's MIDI out, here a record of what is sent) */
static uint32_t host_mo[4096], host_mo_n;
static void midi_out_event(uint32_t pkt)
{
    if (host_mo_n < 4096u)
        host_mo[host_mo_n] = pkt;
    host_mo_n++;
}
#include "midi.c"
#include "midi_uart.c"
/* (the host renders on one core: CPU1's mailbox never takes a job, src/audio_worker.h) */
static int audio_worker_online;
static int audio_worker_submit(void (*fn)(void *), void *ctx) { (void)fn; (void)ctx; return 0; }
static int audio_worker_join(void) { return 1; }
static void audio_worker_check(void) {}
#include "tracks.c"
#include "params.c"
#include "gen.c"

#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, {5, 11, 4, 10, 3, 9, 2, 8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13, 7, 6, 12}, {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    {0, 1, 15, 14, 17, 16, 19, 18, 20, 21, 22}, {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}};
static struct { uint32_t notes, buttons; } fm1_in;
static uint32_t host_press;
static int32_t host_enc[7];
static uint8_t fm1_led_dim[FM1_NCOL];
static uint32_t fm1_input_edges(uint32_t *rel)
{
    uint32_t p = host_press;
    host_press = 0;
    if (rel)
        *rel = 0;
    return p;
}
static int32_t fm1_enc_take(uint32_t e)
{
    int32_t s = host_enc[e];
    host_enc[e] = 0;
    return s;
}
static struct { uint8_t config; volatile uint8_t suspended; } usb;   /* (usb.c's, as the battery reads it) */
static uint8_t host_led[64];
static void fm1_led_key(uint32_t id, int on) { if (id < 64u) host_led[id] = (uint8_t)!!on; }
#include "ui.c"
#include "projects.c"
#include "setup.c"
#include "backup.c"
#include "phrase.c"

/* the flash, emulated: NOR (erased to 0xFF, programming only clears bits) */
static uint8_t host_flash[0x100000];
static uint32_t host_erases;
static int prj_flash_read(uint32_t off, void *p, uint32_t n)
{
    memcpy(p, host_flash + off, n);
    return 0;
}
/* (main.c's areas: the sessions and the loop store's first 9 sectors; past the update's staging, its other 22 and
 * the last session's note) */
static int host_prj_in(uint32_t off, uint32_t n)
{
    return (off >= 0x97000u && off + n <= 0xE0000u) || (off >= 0xE5000u && off + n <= 0xFE000u);
}
static int prj_flash_erase(uint32_t off)
{
    if (!host_prj_in(off, 4096u) || (off & 4095u))
        return -8;
    memset(host_flash + off, 0xFF, 4096u);
    host_erases++;
    return 0;
}
static int prj_flash_prog(uint32_t off, const void *p, uint32_t n)
{
    uint32_t i;
    if (!host_prj_in(off, n))
        return -8;
    for (i = 0; i < n; i++)
        host_flash[off + i] &= ((const uint8_t *)p)[i];
    return 0;
}

/* ---------------------------------------------------------------- audio --- */
static void wav_write(const char *path, const int16_t *s, uint32_t frames)
{
    FILE *f = fopen(path, "wb");
    uint32_t bytes = frames * 4u, v;
    if (!f)
        return;
    fwrite("RIFF", 1, 4, f);
    v = 36u + bytes;
    fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    v = 16;
    fwrite(&v, 4, 1, f);
    {
        uint16_t fmt[2] = {1, 2};
        uint32_t r[2] = {44100u, 44100u * 4u};
        uint16_t al[2] = {4, 16};
        fwrite(fmt, 2, 2, f);
        fwrite(r, 4, 2, f);
        fwrite(al, 2, 2, f);
    }
    fwrite("data", 1, 4, f);
    fwrite(&bytes, 4, 1, f);
    fwrite(s, 2, frames * 2u, f);
    fclose(f);
}
/* blocks of the mix into out (Q15 stereo; the device scales Q15 to its 24 bits at -6 dBFS) */
static void run(int16_t *out, uint32_t blocks)
{
    int32_t b[2u * CTL];
    uint32_t k, i;
    for (k = 0; k < blocks; k++) {
        mix_block(b);
        for (i = 0; i < 2u * CTL; i++)
            out[2u * k * CTL + i] = (int16_t)clampi(b[i], -32768, 32767);
        fm1_ms += (k % 4u == 0u);
    }
}

typedef struct { double peak, rms1, rmstail, dc; } stats_t;
static stats_t measure(const int16_t *s, uint32_t frames, uint32_t note_frames)
{
    stats_t st = {0, 0, 0, 0};
    uint32_t i, n1 = 0, nt = 0;
    double a1 = 0, at = 0, dc = 0;
    for (i = 0; i < frames; i++) {
        double l = s[2 * i] / 32768.0, r = s[2 * i + 1] / 32768.0, m = (l + r) / 2;
        if (fabs(l) > st.peak)
            st.peak = fabs(l);
        if (fabs(r) > st.peak)
            st.peak = fabs(r);
        dc += m;
        if (i < note_frames) {
            a1 += m * m;
            n1++;
        } else if (i >= frames - frames / 6) {
            at += m * m;
            nt++;
        }
    }
    st.rms1 = sqrt(a1 / (n1 ? n1 : 1));
    st.rmstail = sqrt(at / (nt ? nt : 1));
    st.dc = dc / frames;
    return st;
}
static double db(double x) { return x > 1e-9 ? 20 * log10(x) : -180; }

/* one sound on track 0, dry (no sends) unless fx: note held hold_s, the whole render total_s */
static int16_t abuf[2 * 44100 * 4];
static stats_t render_one(const sound_t *s0, int fx, uint32_t note, const char *wav)
{
    sound_t s = *s0;
    if (getenv("FXSET")) {                        /* FXSET=k=v: an FX page value by hand */
        int a = atoi(getenv("FXSET")), b = atoi(strchr(getenv("FXSET"), '=') + 1);
        s.fx[a] = (uint8_t)b;
    }
    if (getenv("FMACH"))
        s.fmach = (uint8_t)atoi(getenv("FMACH"));
    if (getenv("FLTSET")) {
        int a = atoi(getenv("FLTSET")), b = atoi(strchr(getenv("FLTSET"), '=') + 1);
        s.flt[a] = (uint8_t)b;
    }
    uint32_t total = 44100u * 3u / CTL, hold = 44100u / CTL;
    stats_t st;
    if (!fx)
        s.fx[X_DEL] = s.fx[X_REV] = s.fx[X_CHR] = 0;
    tracks_init();
    vox_age = 0;                                  /* (the note's seed as in a render alone: not left by the sounds before) */
    memset(ck_buf, 0, sizeof ck_buf);
    ck_cook(&ck_buf[0][0], &s);
    ck_buf[0][1] = ck_buf[0][0];
    ck_pub[0] = ck_use[0] = 0;
    trk_gain[0] = 16384;
    trk_pl[0] = trk_pr[0] = 23170;
    ev_post(EV_ON, 0, note, 127);
    run(abuf, hold);
    ev_post(EV_OFF, 0, note, 0);
    run(abuf + 2u * hold * CTL, total - hold);
    st = measure(abuf, total * CTL, hold * CTL);
    if (wav)
        wav_write(wav, abuf, total * CTL);
    return st;
}
static int is_drone(const char *n)   /* (sounds meant to sustain or ring on: their tail is not a fault) */
{
    static const char *const D[] = {"DRONE", "PAD", "WASH", "FIELD", "BREATH", "RADIO", "ENGINE", "MACHINE", "SCRAPE", "CYMBAL", "RIDE", "BELL"};
    uint32_t k;
    for (k = 0; k < NELEM(D); k++)
        if (strstr(n, D[k]))
            return 1;
    return 0;
}
static int cmd_render(const char *dir, int seeds, int check)
{
    uint32_t r, k;
    int fails = 0;
    for (r = 0; r < NRECIPE; r++)
        for (k = 0; k < (uint32_t)seeds; k++) {
            sound_t s;
            char path[256], nm[32];
            stats_t st, wet;
            uint32_t i, note = RECIPES[r].mach == ENG_FMD ? 60u : 48u;
            gen_sound(&s, RECIPES[r].mach, (int32_t)r, -1, 1000u + k * 7919u + r * 31u);
            str_cpy(nm, RECIPES[r].name, sizeof nm);
            for (i = 0; nm[i]; i++)
                if (nm[i] == ' ')
                    nm[i] = '_';
            snprintf(path, sizeof path, "%s/%s_%u.wav", dir ? dir : ".", nm, k);
            st = render_one(&s, 0, note, check ? 0 : path);
            wet = render_one(&s, 1, note, 0);
            {
                int silent = db(st.peak) < -45, clip = st.peak > 0.999, dcf = fabs(st.dc) > 0.02,
                    tail = !is_drone(RECIPES[r].name) && db(st.rmstail) > db(st.rms1) - 6 && db(st.rmstail) > -50;
                int bad = silent || clip || dcf || tail;
                if (!check || bad)
                    printf("%-13s %u  %-8s peak %6.1f  rms %6.1f  tail %6.1f  dc %+.3f  wet peak %6.1f%s%s%s%s\n",
                           RECIPES[r].name, k, ENG_NAME[RECIPES[r].mach], db(st.peak), db(st.rms1), db(st.rmstail), st.dc,
                           db(wet.peak), silent ? "  SILENT" : "", clip ? "  CLIP" : "", dcf ? "  DC" : "",
                           tail ? "  TAIL" : "");
                fails += bad;
            }
        }
    if (check)
        printf("check: %d of %u sounds failed\n", fails, (uint32_t)(NRECIPE * (uint32_t)seeds));
    return fails ? 1 : 0;
}

/* ---------------------------------------------------------------- screens --- */
static void ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    uint32_t i;
    if (!f)
        return;
    fprintf(f, "P6 240 240 255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = fb[i];
        uint8_t px[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63u) << 2), (uint8_t)((c & 31u) << 3)};
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}
static void shot(const char *dir, const char *name)
{
    char p[256];
    ui.force = 1;
    ui_draw();
    snprintf(p, sizeof p, "%s/%s.ppm", dir, name);
    ppm(p);
}
static void key(uint32_t k, int down)
{
    if (down)
        fm1_in.notes |= 1u << k;
    else
        fm1_in.notes &= ~(1u << k);
    ui_input();
}
static int cmd_screens(const char *dir)
{
    tracks_init();
    ui_init();
    fm1_ms = 5000;
    shot(dir, "main");
    key(KEY_EDIT, 1);
    key(KEY_EDIT, 0);
    shot(dir, "edit_syn1");
    host_enc[PANEL_ENC[EN_K2]] = 3;
    ui_input();
    shot(dir, "edit_turned");
    host_press = 1u << PANEL_BTN[B_OCTDN];
    ui_input();
    shot(dir, "edit_fx");
    host_press = 1u << PANEL_BTN[B_OCTDN];
    ui_input();
    host_press = 1u << PANEL_BTN[B_OCTDN];
    ui_input();
    host_press = 1u << PANEL_BTN[B_OCTDN];
    ui_input();
    shot(dir, "edit_flt1");
    key(KEY_EDIT, 1);
    key(KEY_EDIT, 0);
    fm1_ms += 2000;
    host_enc[PANEL_ENC[EN_K1]] = -5;
    ui_input();
    shot(dir, "dial_freq");
    fm1_ms += 2000;
    host_press = 1u << PANEL_BTN[B_SEQ];        /* the effects: CHORUS, COMP, DELAY, REVERB, back */
    ui_input();
    shot(dir, "fx_chorus");
    host_press = 1u << PANEL_BTN[B_SEQ];
    ui_input();
    shot(dir, "fx_comp");
    host_enc[PANEL_ENC[EN_PRESET]] = 1;
    ui_input();
    ui_input();
    shot(dir, "fx_comp_eh");
    host_press = 1u << PANEL_BTN[B_SEQ];
    ui_input();
    shot(dir, "fx_delay");
    host_press = 1u << PANEL_BTN[B_SEQ];
    ui_input();
    shot(dir, "fx_reverb");
    host_press = 1u << PANEL_BTN[B_SEQ];
    ui_input();
    host_press = 1u << PANEL_BTN[B_ARP];
    ui_input();
    shot(dir, "scale");
    host_press = 1u << PANEL_BTN[B_ARP];
    ui_input();
    host_press = 1u << PANEL_BTN[B_GLO];
    ui_input();
    shot(dir, "track6");
    return 0;
}

static int cmd_one(int r, int k)   /* render's figures for one recipe and seed */
{
    sound_t s;
    stats_t st;
    gen_sound(&s, RECIPES[r].mach, r, -1, 1000u + (uint32_t)k * 7919u + (uint32_t)r * 31u);
    st = render_one(&s, 0, RECIPES[r].mach == ENG_FMD ? 60u : 48u, "build/sounds/one.wav");
    printf("%s %d peak %.1f rms %.1f\n", RECIPES[r].name, k, db(st.peak), db(st.rms1));
    return 0;
}
static int cmd_dump(int r, int k)   /* a recipe's sound, as drawn by render's seed k */
{
    sound_t s;
    int i;
    gen_sound(&s, RECIPES[r].mach, r, -1, 1000u + (uint32_t)k * 7919u + (uint32_t)r * 31u);
    printf("%s mach %d fmach %d\nsyn", RECIPES[r].name, s.mach, s.fmach);
    for (i = 0; i < 32; i++)
        printf(" %d", s.syn[i]);
    printf("\nflt");
    for (i = 0; i < NF; i++)
        printf(" %d", s.flt[i]);
    printf("\namp");
    for (i = 0; i < NA; i++)
        printf(" %d", s.amp[i]);
    printf("\nfx");
    for (i = 0; i < NX; i++)
        printf(" %d", s.fx[i]);
    printf("\n");
    return 0;
}

/* a chord of four held 4 s on track 0, then a click score: samples whose second difference passes 8 times the
 * running mean of its size (a discontinuity, against the sound's own smoothness) */
static int16_t cbuf[2 * 44100 * 6];
static int cmd_chord(int r, int k, const char *wav)
{
    sound_t s;
    static const uint8_t N[4] = {48, 55, 60, 63};
    uint32_t i, total = 44100u * 5u / CTL, hold = 44100u * 4u / CTL, clicks = 0;
    double avg = 0;
    gen_sound(&s, RECIPES[r].mach, r, -1, 1000u + (uint32_t)k * 7919u + (uint32_t)r * 31u);
    if (!getenv("WET"))
        s.fx[X_DEL] = s.fx[X_REV] = s.fx[X_CHR] = 0;
    if (getenv("SEND")) {                         /* SEND=d,r,c: the sends by hand */
        int d, r_, c_;
        sscanf(getenv("SEND"), "%d,%d,%d", &d, &r_, &c_);
        s.fx[X_DEL] = (uint8_t)d;
        s.fx[X_REV] = (uint8_t)r_;
        s.fx[X_CHR] = (uint8_t)c_;
    }
    if (getenv("SYN")) {                          /* SYN=k=v,k=v: parameters set by hand (a test of one part) */
        const char *q = getenv("SYN");
        while (*q) {
            int a = atoi(q), b;
            while (*q && *q != '=')
                q++;
            b = atoi(++q);
            s.syn[a] = (int16_t)b;
            while (*q && *q != ',')
                q++;
            if (*q)
                q++;
        }
    }
    if (getenv("FLT"))
        s.fmach = (uint8_t)atoi(getenv("FLT"));
    tracks_init();
    ck_cook(&ck_buf[0][0], &s);
    ck_buf[0][1] = ck_buf[0][0];
    trk_gain[0] = 16384;
    trk_pl[0] = trk_pr[0] = 23170;
    for (i = 0; i < 4u; i++)
        ev_post(EV_ON, 0, N[i], 127);
    run(cbuf, hold);
    for (i = 0; i < 4u; i++)
        ev_post(EV_OFF, 0, N[i], 0);
    run(cbuf + 2u * hold * CTL, total - hold);
    for (i = 2; i < total * CTL; i++) {
        double d = fabs((double)cbuf[2 * i] - 2.0 * cbuf[2 * i - 2] + cbuf[2 * i - 4]);
        if (i > 4410 && d > 8 * avg + 40) {
            if (getenv("CLICKS") && clicks < 20u)
                printf("  click at %u (block offset %u) d %.0f avg %.0f\n", i, i % CTL, d, avg);
            clicks++;
        }
        avg += (d - avg) / 200.0;
    }
    printf("%-13s %d  clicks %u\n", RECIPES[r].name, k, clicks);
    if (wav)
        wav_write(wav, cbuf, total * CTL);
    return 0;
}

/* the relative cost of one voice of each recipe (its machine and filter): host time for 20000 blocks */
#include <time.h>
static int cmd_cost(void)
{
    uint32_t r;
    for (r = 0; r < NRECIPE; r++) {
        sound_t s;
        int32_t b[CTL];
        uint32_t n;
        struct timespec t0, t1;
        static vmem_t m;
        ck_t c;
        gen_sound(&s, RECIPES[r].mach, (int32_t)r, -1, 1234u);
        ck_cook(&c, &s);
        memset(&m, 0, sizeof m);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (n = 0; n < 20000u; n++) {
            int32_t note16 = 48 << 16;
            switch (c.mach) {
            case ENG_FMT: if (!n) ft_on(&m.m.ft, &c.m.ft, 1); ft_render(&m.m.ft, &c.m.ft, note_inc(note16), note16, b); break;
            case ENG_FMD: if (!n) fd_on(&m.m.fd, &c.m.fd, 1, 1); fd_render(&m.m.fd, &c.m.fd, note16, b); break;
            case ENG_WAV: if (!n) wt_on(&m.m.wt, &c.m.wt, 1, 1); wt_render(&m.m.wt, &c.m.wt, note16, b); break;
            default: if (!n) sw_on(&m.m.sw, 1, 1); sw_render(&m.m.sw, &c.m.sw, note16, b); break;
            }
            if (!n)
                vc_on(&m.ch, &c.ch, 1);
            vc_block(&m.ch, &c.ch, b, note16, 127);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("%-13s %-9s %-7s %6.0f ns a block\n", RECIPES[r].name, ENG_NAME[c.mach], L_FLT[c.ch.fmach],
               ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / 20000.0);
    }
    return 0;
}

/* the sessions: save, change, load (back as saved), copy, delete, another firmware's data, the questions; the steps,
 * the patterns */
static int fails;
#define PG_FD_SYN_TEST 0u
#define EXPECT(c, what) do { if (!(c)) { printf("FAIL projects: %s\n", what); fails++; } } while (0)
static void btn(uint32_t b, int sh)
{
    if (sh)
        fm1_in.notes |= 1u << KEY_SHIFT;
    host_press = 1u << PANEL_BTN[b];
    ui_input();
    fm1_in.notes &= ~(1u << KEY_SHIFT);
    fm1_ms += 2000;
}
static int host_dim_on(uint32_t id)             /* a key's LED lit dimly */
{
    uint32_t p, r;
    for (p = 0; p < FM1_NCOL; p++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][p] == (int8_t)id)
                return (fm1_led_dim[p] >> r) & 1u;
    return 0;
}
static void tap(uint32_t k)                      /* a key pressed and let go */
{
    fm1_in.notes |= 1u << k;
    ui_input();
    fm1_in.notes &= ~(1u << k);
    ui_input();
}
static void turn(uint32_t e, int32_t n)
{
    host_enc[PANEL_ENC[e]] = n;
    ui_input();
}
/* a session's sector as now, made as builds 38 .. 49 kept it: each sound without its LFOs, the part after them moved
 * down (build 50's tests) */
static void sec_to49(uint8_t *sec)
{
    uint8_t *b = sec + sizeof(prj_head_t);
    uint32_t so = __builtin_offsetof(prj_body_t, snd), n;
    for (n = 0; n < NTRACK * NENG; n++)
        memmove(b + so + n * sizeof(sound49_t), b + so + n * sizeof(sound_t), sizeof(sound49_t));
    memmove(b + PRJ_LEN23, b + __builtin_offsetof(prj_body_t, cp), sizeof(prj_body_t) - __builtin_offsetof(prj_body_t, cp));
}
static int cmd_projects(const char *dir)
{
    sound_t keep;
    memset(host_flash, 0xFF, sizeof host_flash);
    memcpy(host_flash + prj_sector(6, 0), "ACS1 another firmware's session", 31);   /* (TONITOR's, say) */
    prj_scan(1);
    tracks_init();
    ui_init();
    fm1_ms = 5000;
    EXPECT(prj_cur == -1 && !prj_has[6], "a slot with another firmware's data reads empty");
    shot(dir, "prj_main_none");
    btn(B_SAVE, 0);
    EXPECT(ui.page == PG_PRJ, "SAVE opens the list");
    turn(EN_PRESET, 2);                          /* SESSION 3 */
    shot(dir, "prj_list");
    btn(B_SAVE, 0);
    EXPECT(ui.page == PG_PRJD && pj.slot[0] == 2u, "SAVE opens slot 3's page");
    shot(dir, "prj_page_empty");
    turn(EN_PRESET, 4);                          /* TEMPO */
    btn(B_SAVE, 0);                              /* the focus on its value */
    EXPECT(pj.edit, "SAVE on TEMPO moves the focus to the value");
    turn(EN_PRESET, 17);                         /* 137 BPM */
    shot(dir, "prj_page_editing");
    EXPECT(ui.tempo == 120, "the value is not set before SAVE");
    btn(B_SAVE, 0);
    EXPECT(!pj.edit && ui.tempo == 137 && fx_bpm10 == 1370, "SAVE sets the tempo (and the delay follows)");
    turn(EN_PRESET, 1);
    btn(B_SAVE, 0);
    turn(EN_PRESET, 3);                          /* ROOT D# */
    btn(B_SAVE, 0);
    turn(EN_PRESET, 1);
    btn(B_SAVE, 0);
    turn(EN_PRESET, 4);                          /* SCALE: dorian, then left as it was */
    btn(B_SAVE, 1);
    EXPECT(!pj.edit && ui.scale == 6 && ui.page == PG_PRJD, "SHIFT + SAVE leaves the value as it was");
    btn(B_SAVE, 0);
    turn(EN_PRESET, -2);
    btn(B_SAVE, 0);
    EXPECT(ui.root == 3 && ui.scale == 4, "ROOT and SCALE set");
    shot(dir, "prj_page_values");
    turn(EN_PRESET, -5);                         /* SAVE */
    EXPECT(pj.row == PR_SAVE, "PRESETS back to SAVE");
    btn(B_SAVE, 0);
    EXPECT(prj_has[2] && prj_cur == 2 && !prj_dirty, "SAVE into an empty slot, at once");
    keep = ui.snd[1][ui.eng[1]];
    btn(B_GLO, 0);                               /* (back to playing) */
    EXPECT(ui.page == PG_MAIN, "a track's button goes back to playing");
    shot(dir, "prj_main_loaded");
    btn(B_HOME, 0);                              /* a new sound on track 6, and other changes */
    ui.tempo = 90;
    ui.root = 0;
    EXPECT(prj_dirty, "a change marks the project");
    shot(dir, "prj_main_dirty");
    btn(B_SAVE, 0);
    btn(B_SAVE, 0);                              /* slot 3's page (the list stays on it) */
    EXPECT(pj.row == PR_LOAD, "a session's page opens on LOAD");
    turn(EN_PRESET, -9);                         /* (BACK, at the top) */
    EXPECT(pj.row == PR_BACK, "BACK at the top of a session's page");
    btn(B_SAVE, 0);
    EXPECT(ui.page == PG_PRJ, "SAVE on BACK: the list again");
    btn(B_SAVE, 0);                              /* (slot 3's page again: on LOAD) */
    btn(B_SAVE, 0);
    EXPECT(pj.ask == PQ_LOAD, "LOAD with changes not saved asks first");
    shot(dir, "prj_ask_load");
    turn(EN_PRESET, 1);                          /* YES */
    btn(B_SAVE, 0);
    EXPECT(ui.tempo == 137 && ui.root == 3 && ui.page == PG_MAIN && !prj_dirty, "LOAD brings the project back as saved");
    EXPECT(!memcmp(&keep, &ui.snd[1][ui.eng[1]], sizeof keep), "LOAD brings the sounds back");
    btn(B_SAVE, 0);
    btn(B_SAVE, 0);
    turn(EN_PRESET, -9);
    turn(EN_PRESET, 3);                          /* COPY (BACK, LOAD, SAVE, COPY) */
    btn(B_SAVE, 0);
    EXPECT(pj.ask == PQ_COPYTO && pj.to == 3u, "COPY asks where to");
    turn(EN_PRESET, 2);                          /* to 6 */
    shot(dir, "prj_copy_to");
    btn(B_SAVE, 0);
    EXPECT(prj_has[5], "COPY into an empty slot, at once");
    {
        int rc = prj_read(5, 0);
        const prj_body_t *b = (const prj_body_t *)(const void *)(prj_buf + sizeof(prj_head_t));
        EXPECT(!rc && b->tempo == 137, "the copy reads back, as the original");
    }
    btn(B_SAVE, 1);                              /* back to the list */
    EXPECT(ui.page == PG_PRJ, "SHIFT + SAVE goes back a page");
    turn(EN_PRESET, 3);                          /* SESSION 6 */
    btn(B_SAVE, 0);
    turn(EN_PRESET, 1);                          /* SAVE over another project */
    btn(B_SAVE, 0);
    EXPECT(pj.ask == PQ_SAVE, "SAVE over another project asks first");
    shot(dir, "prj_ask_overwrite");
    btn(B_SAVE, 0);                              /* NO */
    EXPECT(prj_cur == 2, "NO leaves it");
    turn(EN_PRESET, 2);                          /* DELETE */
    btn(B_SAVE, 0);
    EXPECT(pj.ask == PQ_DELETE, "DELETE asks first");
    shot(dir, "prj_ask_delete");
    turn(EN_PRESET, 1);
    btn(B_SAVE, 0);
    EXPECT(!prj_has[5] && prj_has[2], "DELETE empties the slot, the others kept");
    prj_scan(1);
    EXPECT(prj_has[2] && !prj_has[5] && !prj_has[6], "a scan after: as it was");
    btn(B_SAVE, 1);
    shot(dir, "prj_list_after");
    /* the steps: SHIFT held, the white keys are the chosen track's; the last note played goes in */
    btn(B_GLO, 0);
    {
        uint32_t kw[16], k, n0, n1, i, w0;
        int32_t out[2 * CTL];
        for (k = 0; k < NKEY; k++)
            if (white_ix(k) >= 0)
                kw[white_ix(k)] = k;
        fm1_in.notes |= 1u << kw[2];
        ui_input();
        fm1_in.notes &= ~(1u << kw[2]);
        ui_input();
        n0 = ui.last_note[5];
        EXPECT(n0 == key_note(kw[2], ui.oct[5]) && !pat_dirty, "the note played kept as the track's last");
        tap(KEY_STEP);
        EXPECT(step_view(), "STEP: the step view");
        tap(KEY_TRIG);
        EXPECT(ui.trig, "the 3rd black key: TRIG on");
        ui_leds();
        EXPECT(host_led[14u + KEY_STEP], "STEP lit");
        w0 = ev_w;
        fm1_in.notes |= 1u << kw[0];
        ui_input();
        EXPECT(seq_note[5][0] == n0 && pat_dirty, "the step view and an empty step: the last note there");
        EXPECT(ev_w == w0, "a step pressed: silent (build 70)");
        EXPECT(seq_vel[5][0] == VEL_DEF && seq_glen[5][0] == GLEN_DEF && !seq_cond[5][0], "a new step: the defaults");
        turn(EN_K1, 1);                          /* (a step held, EDIT not up: the knobs its note, velocity ..) */
        n1 = seq_note[5][0];
        EXPECT(n1 > n0 && in_scale((int32_t)n1), "TRIG, KNOB 1: the note up the scale");
        EXPECT(ev_w == w0, "its note turned: still silent");
        turn(EN_K2, -20);
        turn(EN_K3, 2);
        turn(EN_K4, CD_AB - CD_NONE + 2);        /* 1:2, !1:2, 2:2 */
        EXPECT(seq_vel[5][0] == pct_raw(pct_disp(VEL_DEF) - 20) && seq_glen[5][0] == GLEN_DEF + 2 && seq_cond[5][0] == CD_AB + 2,
               "KNOB 2 .. 4: velocity (20 of 100 down), length, condition");
        fm1_ms += 100;
        ui_draw();
        shot(dir, "step_trig");
        {
            char b[8];
            cond_text(b, seq_cond[5][0]);
            EXPECT(str_eq(b, "2:2"), "the condition reads 2:2");
        }
        fm1_ms += 2000;
        fm1_in.notes &= ~(1u << kw[0]);
        ui_input();
        EXPECT(seq_note[5][0] == n1, "let go after a change: the step kept");
        fm1_in.notes |= 1u << kw[0];             /* held long, nothing changed: kept */
        ui_input();
        fm1_ms += 600;
        fm1_in.notes &= ~(1u << kw[0]);
        ui_input();
        EXPECT(seq_note[5][0] == n1, "held long, let go: the step kept");
        for (i = 0; i < 3u; i++) {               /* 5 on, 5 off again, 9 on */
            k = kw[i == 2u ? 8 : 4];
            fm1_in.notes |= 1u << k;
            ui_input();
            fm1_in.notes &= ~(1u << k);
            ui_input();
        }
        EXPECT(!seq_note[5][4] && seq_note[5][8] == n0, "a step that holds a note, pressed and let go: emptied");
        ui_leds();
        EXPECT(host_led[14u + kw[0]] && host_led[14u + kw[8]] && !host_led[14u + kw[4]], "the step view: the steps lit");
        fm1_ms += 2000;
        ui.force = 1;
        ui_draw();
        shot(dir, "steps");
        tap(KEY_STEP);
        EXPECT(!step_view(), "STEP again: the view off");
        btn(B_PLAY, 0);
        EXPECT(seq_run, "PLAY: the sequencer runs");
        for (i = 0; i < 4000u && seq_pos[5] != 8u; i++)
            mix_block(out);
        EXPECT(seq_pos[5] == 8u && track_sounding(5), "it steps through, step 9 sounding");
        EXPECT(i > 140u * 8u && i < 160u * 8u, "a 16th note at the tempo");
        {   /* step 1 is 2:2: silent the first time through, sounding the second */
            uint32_t on[3] = {0, 0, 0}, loop;
            for (loop = 0; loop < 3u; loop++) {
                while (seq_pos[5] != 15u)
                    mix_block(out);
                while (seq_pos[5] != 0u)
                    mix_block(out);
                for (k = 0; k < SEQ_HOLD; k++)
                    on[loop] |= sq_note[5][k] == n1;
            }
            EXPECT(on[0] && !on[1] && on[2] && seq_loop[5] == 3u, "2:2 plays every second time through (the 2nd, the 4th)");
        }
        btn(B_PLAY, 0);
        for (i = 0; i < 4u; i++)
            mix_block(out);
        EXPECT(!seq_run, "PLAY again: stopped");
        {   /* SHIFT + STEP: the track's steps emptied, the view as it was; kept again for what follows */
            uint8_t keep[NSTEP], kv[NSTEP], kc[NSTEP], kg[NSTEP];
            uint32_t any = 0, keep0 = 0x55u;
            seq_note[0][0] = (uint8_t)keep0;     /* (another track's step: kept) */
            for (i = 0; i < NSTEP; i++) {
                keep[i] = seq_note[5][i];
                kv[i] = seq_vel[5][i];
                kc[i] = seq_cond[5][i];
                kg[i] = seq_glen[5][i];
            }
            (void)pl_set(5, 8, PL_PAN, 20);
            fm1_in.notes |= 1u << KEY_SHIFT;
            ui_input();
            tap(KEY_STEP);
            fm1_in.notes &= ~(1u << KEY_SHIFT);
            ui_input();
            for (i = 0; i < NSTEP; i++)
                any |= seq_note[5][i] | (seq_cond[5][i] != CD_NONE) | (seq_lid[5][i][0] != PL_NONE);
            EXPECT(!any && !step_view() && seq_note[0][0] == keep0, "SHIFT + STEP: the track's steps emptied, the view off still");
            for (i = 0; i < NSTEP; i++) {
                seq_note[5][i] = keep[i];
                seq_vel[5][i] = kv[i];
                seq_cond[5][i] = kc[i];
                seq_glen[5][i] = kg[i];
            }
            seq_note[0][0] = 0;
            pat_dirty = 1;
        }
        /* ARP held: the pattern (1) saved, its steps with it; the grid: ARP loads the one chosen */
        fm1_in.buttons |= 1u << PANEL_BTN[B_ARP];
        host_press = 1u << PANEL_BTN[B_ARP];
        ui_input();
        fm1_ms += 900;
        ui_input();
        fm1_in.buttons = 0;
        ui_input();
        fm1_ms += 2000;
        EXPECT(pat_cur == 0u && pat_store[0].used && !pat_dirty && pat_store[0].seq[SEQ_FMT] == SEQ_V4 &&
               pat_store[0].seq[SEQ_NREC] == 2u && pat_store[0].seq[SEQ_REC4 + 6u + 1u] == n0,
               "ARP held saves the pattern playing, its steps");
        {   /* (written into session 3's flash) */
            int rc = prj_read(2, 1u);
            const pat_t *pp = (const pat_t *)(const void *)(prj_buf + sizeof(prj_head_t));
            EXPECT(!rc && pp[0].used && pp[0].seq[SEQ_REC4] == 5u * NSTEP && pp[0].seq[SEQ_REC4 + 1u] == n1,
                   "the steps are in the session's flash");
        }
        btn(B_ARP, 0);
        EXPECT(ui.page == PG_PAT, "ARP opens the patterns");
        turn(EN_PRESET, 11);                     /* PATTERN 12 */
        shot(dir, "pat_grid");
        btn(B_ARP, 0);
        EXPECT(ui.page == PG_MAIN && pat_cur == 11u && !seq_note[5][0] && !seq_note[5][8], "ARP on the grid: pattern 12 loaded (empty)");
        btn(B_ARP, 0);
        turn(EN_PRESET, -11);
        btn(B_ARP, 0);
        EXPECT(pat_cur == 0u && seq_note[5][0] == n1 && seq_note[5][8] == n0, "and pattern 1 back, its steps");
        EXPECT(seq_cond[5][0] == CD_AB + 2 && seq_vel[5][0] == pct_raw(pct_disp(VEL_DEF) - 20) && seq_glen[5][0] == GLEN_DEF + 2,
               "with their velocity, length, condition");
        /* the PATTERNS grid: a white key loads its pattern; held, empties it */
        btn(B_ARP, 0);
        EXPECT(ui.page == PG_PAT, "ARP shows the patterns");
        fm1_in.notes |= 1u << kw[12];
        ui_input();
        fm1_in.notes &= ~(1u << kw[12]);
        ui_input();
        ui_leds();
        EXPECT(pat_cur == 12u && !seq_note[5][0], "a white key loads its pattern");
        EXPECT(host_led[14u + kw[12]], "its key lit");
        shot(dir, "pat_quick");
        {   /* a white key held: its pattern copied; another held: pasted there, saved, loaded */
            static pat_t keep;
            keep = pat_store[0];
            fm1_in.notes |= 1u << kw[0];
            ui_input();
            fm1_ms += 900;
            ui_input();
            fm1_in.notes &= ~(1u << kw[0]);
            ui_input();
            EXPECT(pat_clip_from == 0 && pat_cur == 12u, "a white key held: its pattern copied (nothing loaded)");
            ui_leds();
            ui_draw();
            shot(dir, "pat_copied");
            fm1_in.notes |= 1u << kw[5];
            ui_input();
            fm1_ms += 900;
            ui_input();
            fm1_in.notes &= ~(1u << kw[5]);
            ui_input();
            EXPECT(pat_clip_from < 0 && pat_cur == 5u && !memcmp(&pat_store[5], &keep, sizeof keep) &&
                   str_eq(ui.flash_l1, "PASTED"), "another held: pasted there, and loaded");
            {
                int rc = prj_read(2, 1u);
                const pat_t *pq = (const pat_t *)(const void *)(prj_buf + sizeof(prj_head_t));
                EXPECT(!rc && !memcmp(&pq[5], &keep, sizeof keep), "pasted into the flash too");
            }
            fm1_ms += 2000;
            tap(kw[12]);
            EXPECT(pat_cur == 12u, "(pattern 13 back)");
        }
        {   /* SHIFT and a white key held: the pattern playing (13) saved there, the one playing kept (build 66) */
            static pat_t keep;
            uint32_t playing = pat_cur;
            pat_take(&keep);
            fm1_in.notes |= 1u << KEY_SHIFT;
            fm1_in.notes |= 1u << kw[0];
            ui_input();
            fm1_ms += 900;
            ui_input();
            fm1_in.notes &= ~(1u << KEY_SHIFT);
            fm1_in.notes &= ~(1u << kw[0]);
            ui_input();
            EXPECT(pat_store[0].used && !memcmp(&pat_store[0], &keep, sizeof keep) && pat_cur == playing &&
                   str_eq(ui.flash_l1, "SAVED"), "SHIFT and a white key held: the pattern playing saved there, it plays on");
            {
                int rc = prj_read(2, 1u);
                const pat_t *pp = (const pat_t *)(const void *)(prj_buf + sizeof(prj_head_t));
                EXPECT(!rc && !memcmp(&pp[0], &keep, sizeof keep), "saved in the flash too");
            }
        }
        fm1_ms += 2000;
        fm1_in.notes |= 1u << KEY_SHIFT;         /* SHIFT and the 1st black key held, a white key pressed: emptied
                                                  * (build 66) */
        fm1_in.notes |= 1u << KEY_GEN;
        ui_input();
        tap(kw[0]);
        fm1_in.notes &= ~(1u << KEY_SHIFT);
        fm1_in.notes &= ~(1u << KEY_GEN);
        ui_input();
        for (k = 0; k < 8u; k++)
            pat_poll();
        EXPECT(!pat_store[0].used && pat_cur == 12u && ui.flash_on && str_eq(ui.flash_l1, "EMPTY"),
               "SHIFT and the 1st black key held, a white key pressed: its pattern emptied, the one playing kept");
        ui_draw();
        shot(dir, "pat_cleared");
        {
            int rc = prj_read(2, 1u);
            const pat_t *pp = (const pat_t *)(const void *)(prj_buf + sizeof(prj_head_t));
            EXPECT(!rc && !pp[0].used, "emptied in the flash too");
        }
        EXPECT(ui.page == PG_PAT, "the grid stays");
        fm1_ms += 2000;
        tap(kw[12]);
        EXPECT(pat_cur == 12u, "(pattern 13 back)");
        EXPECT(ui.page == PG_PAT, "the grid stays");
        fm1_ms += 2000;
        btn(B_GLO, 0);
        EXPECT(ui.page == PG_MAIN, "a track's button: back to playing");
        /* the phrase key: a new phrase on the track, from its sound's role */
        tap(KEY_STEP);
        ui_leds();
        EXPECT(host_led[14u + KEY_GEN] == 0, "the phrase key dim");
        tap(KEY_GEN);
        ui_leds();
        EXPECT(host_led[14u + KEY_GEN] && ui.flash_on && pat_dirty, "the phrase key: lit, NEW PHRASE, the pattern changed");
        for (i = 0, k = 0; i < NSTEP; i++)
            k += seq_note[5][i] != 0;
        EXPECT(k > 0u, "a phrase has steps");
        ui_draw();
        shot(dir, "phrase_new");
        fm1_ms += 2000;
        {   /* a step held, the EDIT pages up: what the knobs turn is locked on it; SELECT the level, ALGORITHM the
             * pan; SHIFT + the phrase key: new locks, the steps kept */
            uint32_t st = 0, kk, f1 = SYN_PAGES[ui.eng[5]];   /* (FLTR 1's page) */
            uint8_t nn[NSTEP], vv[NSTEP], cc[NSTEP], gg[NSTEP];
            int32_t f0, fsnd = cur()->flt[F_FREQ];
            while (!seq_note[5][st])
                st++;
            for (kk = 0; white_ix(kk) != (int32_t)st; kk++)
                ;
            pl_clear(5, st);
            f0 = pl_val(5, st, PG_FLT1 * 8u + 4u);
            tap(KEY_EDIT);
            ui.edit_pg = (uint8_t)f1;
            fm1_in.notes |= 1u << kk;
            ui_input();
            turn(EN_PRESET, -1);                 /* (A .. D) */
            ui_input();
            turn(EN_K1, -5);                     /* FLTR 1, knob A: ATTACK */
            ui.ehalf = 1;
            ui_input();
            turn(EN_K1, -5);                     /* knob E: FREQ */
            turn(EN_SELECT, -7);
            turn(EN_ALGO, 9);
            EXPECT(pl_val(5, st, PG_FLT1 * 8u + 4u) == pct_raw(pct_disp(f0) - 5) && pl_find(5, st, PG_FLT1 * 8u + 0u) >= 0 &&
                   pl_find(5, st, PG_AMP * 8u + 7u) >= 0 && pl_val(5, st, PL_PAN) == bip_raw(9) && cur()->flt[F_FREQ] == fsnd,
                   "a step held, EDIT up: the knobs lock (the sound kept)");
            ui.dial_until = 0;
            ui_draw();
            shot(dir, "lock_edit");
            turn(EN_SELECT, 1);
            ui_draw();
            shot(dir, "lock_dial");
            EXPECT(ui.shown_page == SHOWN_DIAL && ui.dial_lk == PG_AMP * 8u + 7u + 1u, "the level's lock shown as a dial");
            fm1_in.notes &= ~(1u << kk);
            ui_input();
            EXPECT(seq_note[5][st] != 0, "let go after a lock: the step kept");
            fm1_ms += 2000;
            ui.edit_pg = (uint8_t)(f1 + 4u);     /* MANGLE: a track's lock */
            ui.ehalf = 0;
            fm1_in.notes |= 1u << kk;
            ui_input();
            turn(EN_K3, 3);
            EXPECT(pl_val(5, st, PG_MNG * 8u + M_SLICE) == 3, "MANGLE's SLICE locked");
            for (kk = 0; kk < 7u; kk++)          /* (eight a step: the ninth refused) */
                (void)pl_set(5, st, (PG_FLT1 + (kk & 1u)) * 8u + 1u + kk / 2u, 10);
            {
                int32_t before = pl_val(5, st, PG_MNG * 8u + M_DST);
                turn(EN_K1, 5);
                EXPECT(pl_val(5, st, PG_MNG * 8u + M_DST) == before && str_eq(ui.msg, "8 LOCKS"), "the ninth lock refused");
            }
            for (kk = 0; white_ix(kk) != (int32_t)st; kk++)
                ;
            fm1_in.notes &= ~(1u << kk);
            ui_input();
            tap(KEY_EDIT);
            fm1_ms += 2000;
            memcpy(nn, (const void *)seq_note[5], NSTEP);
            memcpy(vv, (const void *)seq_vel[5], NSTEP);
            memcpy(cc, (const void *)seq_cond[5], NSTEP);
            memcpy(gg, (const void *)seq_glen[5], NSTEP);
            fm1_in.notes |= 1u << KEY_SHIFT;
            tap(KEY_GEN);
            fm1_in.notes &= ~(1u << KEY_SHIFT);
            ui_input();
            EXPECT(!memcmp(nn, (const void *)seq_note[5], NSTEP) && !memcmp(vv, (const void *)seq_vel[5], NSTEP) &&
                   !memcmp(cc, (const void *)seq_cond[5], NSTEP) && !memcmp(gg, (const void *)seq_glen[5], NSTEP),
                   "SHIFT + the phrase key: the steps as they were");
            {
                uint32_t on = 0, off = 0, i2, l2;
                for (i2 = 0; i2 < NSTEP; i2++)
                    for (l2 = 0; l2 < NPL; l2++) {
                        if (seq_lid[5][i2][l2] == PL_NONE)
                            continue;
                        if (seq_note[5][i2])
                            on++;
                        else
                            off++;
                    }
                EXPECT(!off && ui.flash_on && str_eq(ui.flash_l2, "LOCKS"), "new locks, on the steps that hold notes only");
                (void)on;
            }
            fm1_ms += 2000;
        }
        tap(KEY_STEP);
    }
    {   /* every recipe's phrase: notes in the scale, room in the pattern, back the same from a pattern */
        uint32_t f, t, i, l, locks = 0, bad = 0, roles[NRO];
        static pat_t pp;
        memset(roles, 0, sizeof roles);
        for (f = 0; f < NRECIPE; f++)
            for (t = 0; t < NTRACK; t++) {
                gen_sound(&ui.snd[t][RECIPES[f].mach], RECIPES[f].mach, (int32_t)f, -1, f * 7u + t);
                ui.eng[t] = RECIPES[f].mach;
                phrase_new(t);
                roles[phrase_role(&ui.snd[t][ui.eng[t]])]++;
                for (i = 0; i < NSTEP; i++) {
                    if (seq_note[t][i] && !in_scale(seq_note[t][i]))
                        bad++;
                    for (l = 0; l < NPL; l++)
                        locks += seq_lid[t][i][l] != PL_NONE;
                }
                if (pat_bytes() > SEQ_PX)
                    bad++;
            }
        EXPECT(!bad, "phrases: in the scale, within the pattern's room");
        EXPECT(locks > NRECIPE * 6u * 3u, "phrases lock");
        for (i = 0; i < NRO; i++)
            EXPECT(roles[i] > 0u, "every role made");
        {
            static uint8_t n0[NTRACK][NSTEP], i0[NTRACK][NSTEP][NPL], c0[NTRACK][NSTEP];
            static int16_t v0[NTRACK][NSTEP][NPL];
            uint32_t same = 1, j;
            memcpy(n0, (const void *)seq_note, sizeof n0);
            memcpy(i0, (const void *)seq_lid, sizeof i0);
            memcpy(v0, (const void *)seq_lv, sizeof v0);
            memcpy(c0, (const void *)seq_cond, sizeof c0);
            pat_take(&pp);
            pat_store[15] = pp;
            pat_give(15, 1);
            for (t = 0; t < NTRACK; t++)         /* (each lock back, wherever its place) */
                for (i = 0; i < NSTEP; i++)
                    for (j = 0; j < NPL; j++)
                        if (i0[t][i][j] != PL_NONE && (pl_find(t, i, i0[t][i][j]) < 0 || pl_val(t, i, i0[t][i][j]) != v0[t][i][j]))
                            same = 0;
            EXPECT(same && !memcmp(n0, (const void *)seq_note, sizeof n0) && !memcmp(c0, (const void *)seq_cond, sizeof c0),
                   "a pattern gives the phrases back as they were");
        }
        {   /* locks reach the voice: a step's FREQ lock cooks the voice's own chain */
            int32_t out[2 * CTL];
            memset((void *)seq_note, 0, sizeof seq_note);
            memset((void *)seq_lid, PL_NONE, sizeof seq_lid);
            seq_note[0][0] = 48;
            seq_vel[0][0] = 100;
            seq_glen[0][0] = GLEN_DEF;
            seq_cond[0][0] = CD_NONE;
            (void)pl_set(0, 0, LK_PID[LK_FREQ], 3);
            (void)pl_set(0, 0, LK_PID[LK_REV], 127);
            (void)pl_set(0, 0, PG_FD_SYN_TEST, 0);   /* (a machine's own: DRUM's TUNE, SYN 1 knob A) */
            seq_run = 1;
            for (i = 0; i < 4u; i++)
                mix_block(out);
            for (i = 0, t = NVOICE; i < NVOICE; i++)
                if (vox[i].track == 0u && vox[i].sounding && vox[i].note == 48u)
                    t = i;
            EXPECT(t < NVOICE && vox[t].own && vmem[t].lck.f_v == 3 && pl_find(0, 0, PL_REV) >= 0 &&
                   (trk[0].tid[0] == PL_REV || trk[0].tid[1] == PL_REV || trk[0].tid[2] == PL_REV),
                   "a step's locks: the voice's own FREQ, the track's reverb send");
            {
                int k2, found = 0;
                for (k2 = 0; k2 < (int)NPL; k2++)
                    found |= vox[t].lid[k2] == PG_FD_SYN_TEST;
                EXPECT(found, "a machine's parameter locked: the voice's own");
            }
            seq_run = 0;
            for (i = 0; i < 4u; i++)
                mix_block(out);
            EXPECT(trk[0].tid[0] == PL_NONE && trk[0].tid[1] == PL_NONE, "stopped: the track's locks gone");
        }
    }
    {   /* the TRACK page (the step view's 2nd black key): LENGTH and CLOCK; the keys past the length dark */
        int32_t out[2 * CTL];
        uint32_t kw[16], k, i, n0, n1;
        static pat_t pp;
        for (k = 0; k < NKEY; k++)
            if (white_ix(k) >= 0)
                kw[white_ix(k)] = k;
        ui.sel = 2;
        tap(KEY_STEP);
        tap(KEY_TRK);
        EXPECT(ui.page == PG_TRK, "the 2nd black key: the TRACK page");
        turn(EN_K1, -4);
        turn(EN_K2, -1);
        EXPECT(seq_len[2] == 12u && seq_div[2] == SEQ_DIV1 - 1u && pat_dirty, "LENGTH 12, CLOCK 1/2");
        ui_leds();
        EXPECT(!host_led[14u + kw[13]] && !host_dim_on(14u + kw[13]) && host_dim_on(14u + kw[11]), "past the length: dark");
        fm1_ms += 2000;
        ui_draw();
        shot(dir, "track_page");
        memset((void *)seq_note[2], 0, NSTEP);
        seq_note[2][0] = 60;
        seq_run = 1;
        for (i = 0; i < 4u; i++)
            mix_block(out);
        for (n0 = 0; seq_pos[2] != 1u && n0 < 5000u; n0++)
            mix_block(out);
        for (n1 = 0; seq_pos[2] != 2u && n1 < 5000u; n1++)
            mix_block(out);
        EXPECT(n1 > 2u * 140u && n1 < 2u * 160u, "CLOCK 1/2: a step two of the tempo's 16ths");
        while (seq_pos[2] != 11u)
            mix_block(out);
        while (seq_pos[2] == 11u)
            mix_block(out);
        EXPECT(seq_pos[2] == 0u, "LENGTH 12: round after step 12");
        seq_run = 0;
        for (i = 0; i < 4u; i++)
            mix_block(out);
        pat_take(&pp);
        seq_len[2] = 0;
        seq_div[2] = SEQ_DIV1;
        pat_store[14] = pp;
        pat_give(14, 1);
        EXPECT(seq_len[2] == 12u && seq_div[2] == SEQ_DIV1 - 1u && seq_div[0] == SEQ_DIV1, "a pattern keeps them");
        tap(KEY_TRK);
        EXPECT(ui.page == PG_MAIN, "again: down");
        tap(KEY_STEP);
        seq_len[2] = 0;
        seq_div[2] = SEQ_DIV1;
    }
    {   /* patterns keep their changes when left; copied, the original stays; switched while playing: at the end of
         * the chosen track's time through */
        int32_t out[2 * CTL];
        uint32_t kw[16], k, i, n;
        for (k = 0; k < NKEY; k++)
            if (white_ix(k) >= 0)
                kw[white_ix(k)] = k;
        seq_run = 0;
        for (i = 0; i < 4u; i++)
            mix_block(out);
        pat_switch(7);                           /* (pattern 8, empty, never saved) */
        memset(&pat_store[7], 0, sizeof pat_store[7]);
        memset(&pat_store[8], 0, sizeof pat_store[8]);
        pat_give(7, 1);
        ui.sel = 1;
        memset((void *)seq_note, 0, sizeof seq_note);
        seq_note[1][0] = seq_note[1][4] = 50;
        pat_dirty = 1;                           /* (steps placed, not saved) */
        btn(B_ARP, 0);
        fm1_in.notes |= 1u << kw[7];             /* copy 8 .. */
        ui_input();
        fm1_ms += 900;
        ui_input();
        fm1_in.notes &= ~(1u << kw[7]);
        ui_input();
        fm1_in.notes |= 1u << kw[8];             /* .. paste into 9 */
        ui_input();
        fm1_ms += 900;
        ui_input();
        fm1_in.notes &= ~(1u << kw[8]);
        ui_input();
        EXPECT(pat_cur == 8u && seq_note[1][0] == 50u && seq_note[1][4] == 50u, "pasted into 9: its steps");
        fm1_ms += 2000;
        tap(kw[7]);                              /* back to 8 */
        EXPECT(pat_cur == 7u && seq_note[1][0] == 50u && seq_note[1][4] == 50u && pat_dirty,
               "8 again: its steps still there (kept when left), not saved yet");
        seq_note[1][8] = 52;                     /* 8 changed; to 9 and back: 9 as pasted, 8 as changed */
        tap(kw[8]);
        EXPECT(pat_cur == 8u && seq_note[1][8] == 0u, "9: as pasted");
        tap(kw[7]);
        EXPECT(pat_cur == 7u && seq_note[1][8] == 52u, "8: as changed");
        btn(B_GLO, 0);
        /* playing: the switch waits for the chosen track (2, 16 steps at 1X) to come round */
        ui.sel = 1;
        seq_len[1] = 0;
        seq_div[1] = SEQ_DIV1;
        seq_run = 1;
        for (i = 0; i < 4u; i++)
            mix_block(out);
        while (seq_pos[1] != 5u)
            mix_block(out);
        pat_switch(8);
        EXPECT(seq_pend == 1u && pat_cur == 7u, "playing: 9 waits");
        ui_leds();
        ui_draw();
        for (n = 0; seq_pend == 1u && n < 20000u; n++) {
            mix_block(out);
            if (seq_pend == 1u && seq_pos[1] == 0u)
                break;                           /* (should not: it comes in at step 1) */
        }
        EXPECT(seq_pend == 2u && seq_pos[1] == 0u, "it comes in as track 2 comes round to its step 1");
        EXPECT(n > 140u * 9u && n < 160u * 11u, "after the rest of the bar");
        ui_input();
        EXPECT(pat_cur == 8u && seq_note[1][8] == 0u && seq_pend == 0u, "9 playing");
        pat_switch(7);
        for (i = 0; i < 10u; i++)
            mix_block(out);
        EXPECT(seq_pend == 1u, "and 8 waits in turn");
        seq_run = 0;
        for (i = 0; i < 4u; i++)
            mix_block(out);
        ui_input();
        EXPECT(pat_cur == 7u && seq_note[1][8] == 52u && seq_pend == 0u, "stopped: in at once, 8 as changed");
    }
    {   /* SHIFT + a step held: copied; SHIFT + another held: pasted (another track too); PLAY held: the TEMPO page */
        uint32_t kw[16], k;
        for (k = 0; k < NKEY; k++)
            if (white_ix(k) >= 0)
                kw[white_ix(k)] = k;
        ui.sel = 4;
        memset((void *)seq_note[4], 0, NSTEP);
        memset((void *)seq_note[1], 0, NSTEP);
        seq_note[4][2] = 66;
        seq_vel[4][2] = 77;
        seq_rtg[4][2] = 2;
        seq_cond[4][2] = CD_AB + 1;
        pl_clear(4, 2);
        (void)pl_set(4, 2, PL_PAN, 10);
        tap(KEY_STEP);
        fm1_in.notes |= 1u << KEY_SHIFT;
        fm1_in.notes |= 1u << kw[2];
        ui_input();
        fm1_ms += 900;
        ui_input();
        fm1_in.notes &= ~(1u << kw[2]);
        ui_input();
        EXPECT(ui.sclip.on && ui.sclip.i == 2u && seq_note[4][2] == 66u, "SHIFT + a step held: copied (the step kept)");
        ui_draw();
        shot(dir, "step_copied");
        btn(B_ENV, 0);                           /* (track 3) */
        fm1_in.notes |= 1u << KEY_SHIFT;
        fm1_in.notes |= 1u << kw[9];
        ui_input();
        fm1_ms += 900;
        ui_input();
        fm1_in.notes &= ~(1u << kw[9]);
        fm1_in.notes &= ~(1u << KEY_SHIFT);
        ui_input();
        EXPECT(!ui.sclip.on && seq_note[2][9] == 66u && seq_vel[2][9] == 77u && seq_rtg[2][9] == 2u &&
               seq_cond[2][9] == CD_AB + 1 && pl_val(2, 9, PL_PAN) == 10 && str_eq(ui.flash_l1, "PASTED"),
               "SHIFT + another held: pasted, another track, all of it");
        fm1_ms += 2000;
        tap(KEY_STEP);
        seq_note[2][9] = 0;
        /* PLAY held: the TEMPO page; PRESETS the tempo; held again: down */
        k = (uint32_t)ui.tempo;
        fm1_in.buttons |= 1u << PANEL_BTN[B_PLAY];
        host_press = 1u << PANEL_BTN[B_PLAY];
        ui_input();
        fm1_ms += 900;
        ui_input();
        fm1_in.buttons = 0;
        ui_input();
        EXPECT(ui.page == PG_TEMPO && !seq_run, "PLAY held: the TEMPO page (not played)");
        turn(EN_PRESET, 5);
        EXPECT(ui.tempo == (int16_t)(k + 5u) && fx_bpm10 == (int32_t)(k + 5u) * 10, "PRESETS: the tempo");
        ui_draw();
        shot(dir, "tempo_page");
        btn(B_FX, 0);
        EXPECT(ui.page == PG_MAIN, "a track's button: down");
        ui.tempo = (int16_t)k;
        tempo_apply();
    }
    {   /* the directions (the step view's 4th .. 6th black keys), ratchets (TRIG's other half), the PHRASE page */
        int32_t out[2 * CTL];
        uint32_t kw[16], k, i, seen[NSTEP], hits;
        static pat_t pp;
        for (k = 0; k < NKEY; k++)
            if (white_ix(k) >= 0)
                kw[white_ix(k)] = k;
        ui.sel = 3;
        memset((void *)seq_note[3], 0, NSTEP);
        for (i = 0; i < 4u; i++)
            seq_note[3][i] = 60;
        seq_len[3] = 4;
        tap(KEY_STEP);
        tap(KEY_DIR[0]);
        EXPECT(seq_dir[3] == DIR_REV, "the 4th black key: REVERSE");
        ui_leds();
        EXPECT(host_led[14u + KEY_DIR[0]] && !host_led[14u + KEY_DIR[1]], "its key lit");
        seq_run = 1;
        memset(seen, 0, sizeof seen);
        for (i = 0, k = 0xFF; i < 3000u; i++) {   /* (the order it plays its four steps) */
            mix_block(out);
            if (seq_pos[3] != k && seen[0] < 6u) {
                k = seq_pos[3];
                seen[++seen[0]] = k;
            }
        }
        EXPECT(seen[1] == 3u && seen[2] == 2u && seen[3] == 1u && seen[4] == 0u && seen[5] == 3u, "REVERSE: 4 3 2 1 4");
        seq_run = 0;
        for (i = 0; i < 4u; i++)
            mix_block(out);
        tap(KEY_DIR[1]);
        EXPECT(seq_dir[3] == DIR_PING, "the 5th: PING-PONG");
        seq_run = 1;
        memset(seen, 0, sizeof seen);
        for (i = 0, k = 0xFF; i < 4000u; i++) {
            mix_block(out);
            if (seq_pos[3] != k && seen[0] < 8u) {
                k = seq_pos[3];
                seen[++seen[0]] = k;
            }
        }
        EXPECT(seen[1] == 0u && seen[4] == 3u && seen[5] == 2u && seen[7] == 0u && seen[8] == 1u, "PING-PONG: 1 2 3 4 3 2 1 2");
        seq_run = 0;
        for (i = 0; i < 4u; i++)
            mix_block(out);
        tap(KEY_DIR[1]);
        EXPECT(seq_dir[3] == DIR_FWD, "again: forward");
        {   /* (build 71) PING-PONG chosen while playing, never started in it: it bounces, as from PLAY */
            seq_ppd[3] = 0;
            seq_run = 1;
            for (i = 0; i < 1000u && seq_pos[3] != 1u; i++)
                mix_block(out);
            tap(KEY_DIR[1]);
            memset(seen, 0, sizeof seen);
            for (i = 0, k = seq_pos[3]; i < 4000u; i++) {
                mix_block(out);
                if (seq_pos[3] != k && seen[0] < 6u) {
                    k = seq_pos[3];
                    seen[++seen[0]] = k;
                }
            }
            EXPECT(seen[0] == 6u && seen[1] == 2u && seen[2] == 3u && seen[3] == 2u && seen[4] == 1u && seen[5] == 0u &&
                   seen[6] == 1u, "PING-PONG chosen while playing: 3 4 3 2 1 2");
            seq_run = 0;
            for (i = 0; i < 4u; i++)
                mix_block(out);
            tap(KEY_DIR[1]);
            EXPECT(seq_dir[3] == DIR_FWD, "and forward again");
        }
        /* TRIG's other half: RATCHET 4 on step 1 -- four hits in the step */
        fm1_in.notes |= 1u << kw[0];
        ui_input();
        turn(EN_PRESET, 1);
        ui_input();
        turn(EN_K1, 3);
        EXPECT(seq_rtg[3][0] == 3u && RTG_N[3] == 4u, "PRESETS right, KNOB 1: RATCHET 4");
        ui_draw();
        shot(dir, "step_ratchet");
        fm1_in.notes &= ~(1u << kw[0]);
        ui_input();
        for (i = 1; i < 4u; i++)
            seq_note[3][i] = 0;
        seq_run = 1;
        for (i = 0, hits = 0; i < 151u * 4u; i++) {   /* (the first step: the notes played in it) */
            uint32_t a0 = seq_sounded[3];
            mix_block(out);
            hits += i < 140u ? seq_sounded[3] - a0 : 0u;
        }
        EXPECT(hits == 4u, "four hits in the step");
        seq_run = 0;
        for (i = 0; i < 4u; i++)
            mix_block(out);
        /* the phrase key held: the PHRASE page; DENSITY and COMPLEXITY */
        fm1_in.notes |= 1u << KEY_GEN;
        ui_input();
        fm1_ms += 900;
        ui_input();
        fm1_in.notes &= ~(1u << KEY_GEN);
        ui_input();
        EXPECT(ui.page == PG_GEN, "the phrase key held: the PHRASE page");
        turn(EN_K1, -50);
        turn(EN_K2, 50);
        EXPECT(ui.gen_den[3] == 0u && ui.gen_cx[3] == 100u, "DENSITY 0, COMPLEXITY 100");
        fm1_ms += 2000;
        ui_draw();
        shot(dir, "phrase_page");
        gen_sound(&ui.snd[3][ENG_FMD], ENG_FMD, 8, -1, 4242u);   /* (METAL HAT: a role that ratchets) */
        ui.eng[3] = ENG_FMD;
        {
            uint32_t thin = 0, full = 0, rt = 0, r;
            for (r = 0; r < 20u; r++) {
                ui.gen_den[3] = 0;
                phrase_new(3);
                for (i = 0; i < NSTEP; i++)
                    thin += seq_note[3][i] != 0;
                ui.gen_den[3] = 100;
                phrase_new(3);
                for (i = 0; i < NSTEP; i++) {
                    full += seq_note[3][i] != 0;
                    rt += seq_rtg[3][i] != 0;
                }
            }
            EXPECT(full > thin * 2u, "DENSITY: fuller as it goes up");
            EXPECT(rt > 0u, "COMPLEXITY 100: ratchets now and then");
            pat_take(&pp);
            pat_store[13] = pp;
            seq_dir[3] = DIR_RND;
            pat_give(13, 1);
            EXPECT(seq_dir[3] == DIR_FWD && ui.gen_den[3] == 100u && ui.gen_cx[3] == 100u, "a pattern keeps direction, settings");
        }
        tap(KEY_GEN);                            /* (a short press on the page: a new phrase, the page kept) */
        EXPECT(ui.page == PG_GEN && ui.flash_on, "a short press: a new phrase");
        fm1_ms += 2000;
        fm1_in.notes |= 1u << KEY_GEN;
        ui_input();
        fm1_ms += 900;
        ui_input();
        fm1_in.notes &= ~(1u << KEY_GEN);
        ui_input();
        EXPECT(ui.page == PG_MAIN, "held again: the page down");
        tap(KEY_STEP);
        seq_len[3] = 0;
        ui.gen_den[3] = ui.gen_cx[3] = 50;
    }
    {   /* MOD (the step view's 4th black key): MUTATE every so many times, a MIDI FX */
        int32_t out[2 * CTL];
        uint32_t kw[16], k, i, seen[4], m;
        static pat_t pp;
        for (k = 0; k < NKEY; k++)
            if (white_ix(k) >= 0)
                kw[white_ix(k)] = k;
        seq_run = 0;
        for (i = 0; i < 4u; i++)
            mix_block(out);
        ui.sel = 3;
        memset((void *)seq_note[3], 0, NSTEP);
        seq_len[3] = 1;                          /* (one step: it plays every 16th) */
        seq_dir[3] = DIR_FWD;
        tap(KEY_STEP);
        tap(KEY_MOD);
        EXPECT(ui.mod && !ui.trig, "the 4th black key: MOD (TRIG off)");
        fm1_in.notes |= 1u << kw[0];
        ui_input();                              /* (an empty step: the last note, placed) */
        m = seq_note[3][0];
        turn(EN_K1, 2);                          /* MUTATE +2 */
        turn(EN_K2, 1);                          /* .. every 2 */
        turn(EN_K3, MFX_FIFTH);                  /* MIDI FX: FIFTH .. */
        EXPECT(seq_mut[3][0] == 2 && seq_mcy[3][0] == 1u && seq_mfx[3][0] == MFX_FIFTH && seq_fcy[3][0] == 0u,
               "KNOB 1 .. 4: MUTATE +2, every 2, FIFTH, every 1");
        ui_draw();
        shot(dir, "step_mod");
        fm1_in.notes &= ~(1u << kw[0]);
        ui_input();
        seq_rtg[3][0] = 0;
        seq_cond[3][0] = CD_NONE;
        seq_run = 1;
        for (i = 0; i < 4u; i++)
            mix_block(out);
        for (k = 0; k < 4u; k++) {               /* (the note the step played, four times) */
            uint32_t a0 = seq_sounded[3], j;
            while (seq_sounded[3] == a0)
                mix_block(out);
            seen[k] = seq_played[3];
            (void)j;
            for (i = 0; i < 60u; i++)
                mix_block(out);
        }
        EXPECT(seen[0] == seen[2] && seen[1] == seen[3] && seen[0] != seen[1] &&
               (seen[0] == m || seen[1] == m) && (seen[0] == (uint32_t)sc_move((int32_t)m, 2) ||
                                                   seen[1] == (uint32_t)sc_move((int32_t)m, 2)),
               "MUTATE +2 every 2: as it is and two steps of the scale up, in turn");
        {
            uint32_t j, two = 0;
            for (j = 0; j < SEQ_HOLD; j++)
                two += sq_note[3][j] != 0;
            EXPECT(two >= 2u, "FIFTH: a second note with it");
        }
        seq_run = 0;
        for (i = 0; i < 4u; i++)
            mix_block(out);
        pat_take(&pp);
        seq_mut[3][0] = 0;
        seq_mfx[3][0] = 0;
        pat_store[12] = pp;
        pat_give(12, 1);
        EXPECT(seq_mut[3][0] == 2 && seq_mcy[3][0] == 1u && seq_mfx[3][0] == MFX_FIFTH, "a pattern keeps MOD");
        tap(KEY_MOD);
        tap(KEY_STEP);
        seq_len[3] = 0;
        memset((void *)seq_note[3], 0, NSTEP);
    }
    {   /* REC: armed (the metronome counts), the first note starts it on step 1; each note to the nearest step */
        int32_t out[2 * CTL];
        uint32_t kw[16], k, i, clicks = 0, n;
        for (k = 0; k < NKEY; k++)
            if (white_ix(k) >= 0)
                kw[white_ix(k)] = k;
        seq_run = 0;
        for (i = 0; i < 4u; i++)
            mix_block(out);
        memset((void *)seq_note, 0, sizeof seq_note);
        ui.sel = 0;
        btn(B_REC, 0);
        EXPECT(ui.rec == REC_ARMED && met_on && !seq_run, "REC, stopped: armed, the metronome on");
        for (i = 0; i < 1300u; i++) {
            mix_block(out);
            clicks += met_n == MET_LEN - CTL;
        }
        EXPECT(clicks >= 2u, "armed: the metronome clicks the beats");
        ui_draw();
        shot(dir, "rec_armed");
        fm1_in.notes |= 1u << kw[2];
        ui_input();
        n = key_note(kw[2], ui.oct[0]);
        EXPECT(seq_run && ui.rec == REC_ON && seq_note[0][0] == n && seq_skip[0][0], "the first note: it plays, step 1");
        fm1_ms += 15000u * 10u / (uint32_t)fx_bpm10 * 2u;   /* (held two steps) */
        fm1_in.notes &= ~(1u << kw[2]);
        ui_input();
        EXPECT(seq_glen[0][0] == 7u, "held two steps: LEN 2");
        mix_block(out);
        EXPECT(!seq_skip[0][0] && seq_pos[0] == 0u, "step 1 not played again (the key did)");
        while (!(seq_pos[0] == 4u && seq_acc_now > SEQ_16TH * 3u / 5u))   /* late in step 5: to step 6 */
            mix_block(out);
        tap(kw[4]);
        EXPECT(seq_note[0][5] == key_note(kw[4], ui.oct[0]) && seq_skip[0][5], "late in a step: the next one");
        while (!(seq_pos[0] == 9u && seq_acc_now < SEQ_16TH / 3u))         /* early in step 10: there */
            mix_block(out);
        tap(kw[6]);
        EXPECT(seq_note[0][9] == key_note(kw[6], ui.oct[0]) && !seq_skip[0][9], "early in a step: that one");
        ui_draw();
        shot(dir, "rec_on");
        btn(B_REC, 0);
        EXPECT(ui.rec == REC_OFF && seq_run && !met_on, "REC again: off, it plays on");
        btn(B_REC, 0);
        EXPECT(ui.rec == REC_ON && met_on, "REC playing: recording at once (punch in)");
        btn(B_PLAY, 1);
        EXPECT(!ui.click && !met_on, "SHIFT + PLAY: the metronome off");
        btn(B_PLAY, 1);
        btn(B_PLAY, 0);
        EXPECT(!seq_run && ui.rec == REC_OFF, "PLAY: stopped, recording off");
        for (i = 0; i < 4u; i++)
            mix_block(out);
    }
    {   /* BREAK: the 3rd black key held, other steps played while the track's own position goes on; let go, it plays
         * on from where it is (build 37) */
        int32_t out[2 * CTL];
        uint32_t i, last, moved = 0, kept = 1, on = 1, snd, way = 0xFFu, ways = 0;
        ui.sel = 0;
        ui.page = PG_MAIN;
        ui.sv = 0;
        seq_len[0] = 0;
        seq_dir[0] = DIR_FWD;
        seq_div[0] = SEQ_DIV1;
        for (i = 0; i < NSTEP; i++) {
            seq_note[0][i] = (uint8_t)(48u + i);
            seq_vel[0][i] = 100;
            seq_glen[0][i] = GLEN_DEF;
            seq_cond[0][i] = CD_NONE;
            seq_rtg[0][i] = 0;
            seq_mut[0][i] = 0;
            seq_mfx[0][i] = MFX_OFF;
            seq_chf[0][i] = 0;
            memset((void *)seq_lid[0][i], PL_NONE, NPL);
        }
        seq_run = 1;
        while (seq_pos[0] != 5u)
            mix_block(out);
        key(KEY_TRIG, 1);
        brk_keep = 0;                            /* (whatever the sound: not left alone) */
        EXPECT(ui.brk == 1u && brk_mask == 1u && !key_plays(KEY_TRIG), "the 3rd black key held: track 1 under BREAK");
        for (i = 0, last = seq_pos[0]; i < 64u; i++) {   /* (four bars: one way all through -- build 53) */
            snd = seq_sounded[0];
            if (brk_shown != 0xFFu && brk_shown != way) {
                way = brk_shown;
                ways++;
            }
            while (seq_pos[0] == last)
                mix_block(out);
            on &= seq_pos[0] == (last + 1u) % 16u;
            last = seq_pos[0];
            if (seq_sounded[0] == snd || seq_played[0] != 48u + last)
                moved++;
        }
        EXPECT(on, "under BREAK the track's own position goes on, a step at a time");
        EXPECT(moved > 0u, "under BREAK other steps play (or none, or leaps)");
        EXPECT(ways == 1u, "BREAK held four bars: one way all through (build 53)");
        ui_input();
        ui_draw();
        shot(dir, "break");
        EXPECT(ui.brk_shown != 0xFFu && ui.msg[0], "the way playing is shown");
        key(KEY_TRIG, 0);
        EXPECT(!ui.brk && !brk_mask, "let go: BREAK off");
        for (i = 0; i < 16u; i++) {
            snd = seq_sounded[0];
            while (seq_pos[0] == last)
                mix_block(out);
            last = seq_pos[0];
            kept &= seq_sounded[0] > snd && seq_played[0] == 48u + last;
        }
        EXPECT(kept, "let go: each step plays its own note again, from where the track is");
        fm1_in.notes |= 1u << KEY_SHIFT;
        key(KEY_TRIG, 1);
        EXPECT(ui.brk == 0x3Fu, "SHIFT and the 3rd black key: every track");
        fm1_in.notes &= ~(1u << KEY_SHIFT);
        key(KEY_TRIG, 0);
        EXPECT(LOOP_MAX == 512u && loop_len8(64000u * 1200u / (uint32_t)fx_bpm10) == 4096u,
               "a loop: up to 32 bars, a note held through all of them");
        seq_run = 0;
        for (i = 0; i < 4u; i++)
            mix_block(out);
        for (i = 0; i < NSTEP; i++)
            seq_note[0][i] = 0;
    }
    {   /* BREAK leaves the loops alone (build 45, as before 41): a loop on track 2 the shape of track 1's steps;
         * under BREAK on every track the pattern is broken up, the loop plays its own steps -- during and after */
        int32_t out[2 * CTL];
        uint32_t i, last, k, n1, both = 0, same = 1, after = 1, moved = 0, phase;
        loop_t *lp = &loops[1][0];
        ui.sel = 0;
        ui.page = PG_MAIN;
        ui.sv = 0;
        seq_len[0] = seq_len[1] = 0;
        seq_dir[0] = DIR_FWD;
        seq_div[0] = SEQ_DIV1;
        for (i = 0; i < NSTEP; i++) {
            seq_note[0][i] = (uint8_t)(48u + i);
            seq_vel[0][i] = 100;
            seq_glen[0][i] = 0;                  /* (1/8 of a step: over by the next) */
            seq_cond[0][i] = CD_NONE;
            seq_rtg[0][i] = 0;
            seq_mut[0][i] = 0;
            seq_mfx[0][i] = MFX_OFF;
            seq_chf[0][i] = 0;
            memset((void *)seq_lid[0][i], PL_NONE, NPL);
            seq_note[1][i] = 0;
            lp->ev[i].step = (uint16_t)i;
            lp->ev[i].len8 = 1;
            lp->ev[i].note = (uint8_t)(72u + i);
            lp->ev[i].vel = 100;
        }
        lp->n = 16;
        lp->len = 16;
        lp->mute = 0;
        lp->cur = 0;
        lp->kick = 0;
        lp->anchor = 0;
        lp->on = 1;
        seq_run = 1;
        while (seq_pos[0] != 6u)
            mix_block(out);
        fm1_in.notes |= 1u << KEY_SHIFT;
        key(KEY_TRIG, 1);
        fm1_in.notes &= ~(1u << KEY_SHIFT);
        brk_keep = 0;                            /* (both tracks: drums, no kick -- no leaps, the same rules) */
        brk_kick = 0;
        brk_drum = 0x3F;
        for (phase = 0; phase < 2u; phase++) {
            if (phase)
                key(KEY_TRIG, 0);
            for (i = 0, last = seq_pos[0]; i < (phase ? 32u : 64u); i++) {
                uint32_t s0 = seq_sounded[0], s1 = seq_sounded[1], n0 = 0;
                while (seq_pos[0] == last)
                    mix_block(out);
                last = seq_pos[0];
                for (k = 0, n1 = 0; k < SEQ_HOLD; k++)
                    if (sq_note[1][k] >= 72u && sq_note[1][k] < 88u)
                        n1 = sq_note[1][k];
                if (seq_sounded[0] > s0)
                    n0 = seq_played[0];
                same &= seq_sounded[1] > s1 && n1 - 72u == last;   /* (the loop: its own step, every step) */
                if (seq_sounded[0] == s0) {
                    after &= !phase;
                    continue;
                }
                both++;
                if (phase)
                    after &= n0 - 48u == last;
                else
                    moved += n0 - 48u != last;
            }
        }
        EXPECT(both > 40u && moved > 0u, "BREAK on every track: the pattern broken up");
        EXPECT(same, "BREAK: the loop plays its own steps, every one, during the break and after");
        EXPECT(after, "let go: the pattern on its own steps again");
        lp->on = 0;
        lp->n = 0;
        seq_run = 0;
        for (i = 0; i < 4u; i++)
            mix_block(out);
        for (i = 0; i < NSTEP; i++)
            seq_note[0][i] = 0;
    }
    {   /* a loop of held chords through BREAK on every track (build 45): never cut, never moved -- at every step, the
         * chord due then sounds, and only it, during the break and after */
        int32_t out[2 * CTL];
        uint32_t i, c, k, ok = 1, g, ph, want, n;
        loop_t *lp = &loops[1][0];
        for (i = 0; i < NSTEP; i++)
            seq_note[1][i] = 0;
        for (c = 0; c < 4u; c++)                 /* (four chords, each held two bars) */
            for (i = 0; i < 3u; i++) {
                lev_t *e = &lp->ev[c * 3u + i];
                e->step = (uint16_t)(c * 32u);
                e->len8 = 32u * 8u;
                e->note = (uint8_t)(57u + c * 2u + i * 4u);
                e->vel = 100;
            }
        lp->n = 12;
        lp->len = 128;
        lp->mute = 0;
        lp->cur = 0;
        lp->kick = 0;
        lp->anchor = 0;
        lp->on = 1;
        seq_run = 1;
        while (seq_gcount != 20u)
            mix_block(out);
        fm1_in.notes |= 1u << KEY_SHIFT;
        key(KEY_TRIG, 1);
        fm1_in.notes &= ~(1u << KEY_SHIFT);
        brk_keep = 0;
        for (c = 0; c < 80u; c++) {              /* (48 steps broken up, let go at 16th 68, then 32 more) */
            if (seq_gcount == 68u)
                key(KEY_TRIG, 0);
            g = seq_gcount;
            while (seq_gcount == g)
                mix_block(out);
            mix_block(out);
            ph = (seq_gcount - lp->anchor) % lp->len;
            want = 57u + (ph / 32u) * 2u;
            for (k = 0, n = 0; k < SEQ_HOLD; k++)
                if (sq_note[1][k]) {
                    n++;
                    ok &= sq_note[1][k] == want || sq_note[1][k] == want + 4u || sq_note[1][k] == want + 8u;
                }
            ok &= n == 3u;
        }
        EXPECT(ok, "BREAK: a loop's held chords never cut or moved, during it and after");
        lp->on = 0;
        lp->n = 0;
        seq_run = 0;
        for (i = 0; i < 4u; i++)
            mix_block(out);
    }
    {   /* BREAK as played (builds 44, 45): a loop recorded with the keys from a downbeat -- a note held most of a
         * bar, then notes on beats -- and a kick on every beat; 200 breaks, each held a different time, on one track
         * or (SHIFT) all, let go anywhere in a step. During each and for two bars after: every loop note starts on
         * its own step; after, every kick on a beat */
        int32_t out[2 * CTL];
        uint32_t kw2[16], k, run, bad = 0, total = 0, kbad = 0, ktot = 0, i;
        loop_t *lp = &loops[2][0];
        for (k = 0; k < NKEY; k++)
            if (white_ix(k) >= 0)
                kw2[white_ix(k)] = k;
        for (k = 0; k < NSTEP; k++) {
            seq_note[0][k] = (k % 4u) ? 0 : 48;
            seq_vel[0][k] = 100;
            seq_glen[0][k] = GLEN_DEF;
            seq_cond[0][k] = CD_NONE;
            seq_rtg[0][k] = 0;
            seq_mut[0][k] = 0;
            seq_mfx[0][k] = MFX_OFF;
            seq_chf[0][k] = 0;
            seq_note[1][k] = seq_note[2][k] = 0;
        }
        lop_clear();
        seq_run = 1;
        while (seq_gcount != 16u)
            mix_block(out);
        ui.sel = 2;
        btn(B_REC, 1);                           /* (the looper armed on track 3) */
        while (!(seq_gcount == 31u && seq_acc_now > SEQ_16TH * 3u / 4u))
            mix_block(out);
        fm1_in.notes |= 1u << kw2[0];            /* (just before 32: on it) -- held to 44 */
        ui_input();
        while (seq_gcount != 44u)
            mix_block(out);
        fm1_ms += 12u * 15000u * 10u / (uint32_t)fx_bpm10;   /* (12 steps held) */
        fm1_in.notes &= ~(1u << kw2[0]);
        ui_input();
        for (i = 48; i < 64u; i += 4u) {         /* (then a note on each beat of the next bar) */
            while (seq_gcount != i)
                mix_block(out);
            tap(kw2[2 + i / 4u % 4u]);
        }
        while (!(seq_gcount == 63u && seq_acc_now > SEQ_16TH * 2u / 3u))
            mix_block(out);
        btn(B_REC, 0);
        EXPECT(lp->on && lp->len == 32u && lp->anchor == 32u && lp->n == 5u && lp->ev[0].len8 >= 80u,
               "(the loop: 2 bars from the downbeat, a held note and four on the beats)");
        for (run = 0; run < 200u; run++) {
            uint32_t hold = 200u + (run * 7919u) % 3000u, g;
            ui.sel = (uint8_t)(run % 3u);
            ui.page = PG_MAIN;
            if (run & 1u)
                fm1_in.notes |= 1u << KEY_SHIFT;
            key(KEY_TRIG, 1);
            fm1_in.notes &= ~(1u << KEY_SHIFT);
            brk_keep = 0;                        /* (whatever track 3's sound: broken up with the rest) */
            for (i = 0; i < hold; i++) {
                uint32_t s2 = seq_sounded[2], ph, ok, e;
                mix_block(out);
                if (seq_sounded[2] != s2) {
                    ph = (seq_gcount - lp->anchor) % lp->len;
                    for (e = 0, ok = 0; e < lp->n; e++)
                        ok |= lp->ev[e].step == ph;
                    total++;
                    bad += !ok;
                }
            }
            key(KEY_TRIG, 0);
            for (g = seq_gcount + 32u; seq_gcount != g;) {
                uint32_t s2 = seq_sounded[2], s0 = seq_sounded[0], ph, ok, e;
                mix_block(out);
                if (seq_sounded[2] != s2) {
                    ph = (seq_gcount - lp->anchor) % lp->len;
                    for (e = 0, ok = 0; e < lp->n; e++)
                        ok |= lp->ev[e].step == ph;
                    total++;
                    bad += !ok;
                }
                if (seq_sounded[0] != s0) {
                    ktot++;
                    kbad += seq_gcount % 4u != 0u;
                }
            }
            for (i = 0; i < (run * 31u) % 300u; i++)
                mix_block(out);
        }
        EXPECT(total > 600u && !bad, "200 breaks: during and after each, every loop note on its own step");
        EXPECT(ktot > 1000u && !kbad, "200 breaks: after each, every kick on a beat");
        lop_clear();
        seq_run = 0;
        for (i = 0; i < 4u; i++)
            mix_block(out);
        EXPECT(seq_gcount == 0xFFFFFFFFu && !loop_qnow(), "stopped after a long run: the count reads not started (the looper from 0)");
        for (k = 0; k < NSTEP; k++)
            seq_note[0][k] = 0;
    }
    {   /* BREAK on the beat (build 45), every way, 3000 plans: a track plays as written until its next beat (half
         * bar for REPEAT 8); from there, what is on a beat stays on a beat (HALF TIME: on the 8ths it is stretched
         * to); no window of 3 */
        uint32_t seed, p0, i, p, src, entered, ok = 1, w3 = 0, seen = 0;
        for (seed = 1; seed <= 3000u; seed++) {
            brk_r = seed * 2654435761u | 1u;
            brk_prev = 0xFF;
            brk_draw();
            seen |= 1u << brk_rec;
            w3 |= brk_rec == BR_REPEAT && brk_w == 3u;
            brk_lay = 0;
            brk_kick = 0;
            p0 = seed % 16u;
            for (i = 0, entered = 0; i < 40u; i++) {
                p = (p0 + i) % 16u;
                src = brk_step(0, p, 16u);
                if (!entered && (brk_fresh & 1u)) {
                    ok &= src == p;              /* (not yet: as written) */
                    continue;
                }
                entered = 1;
                if (p % 4u == 0u)
                    ok &= brk_rec == BR_HALF ? src % 2u == 0u : src % 4u == 0u;
            }
        }
        brk_shown = 0xFF;
        EXPECT(ok && !w3 && seen == (1u << NBR) - 1u, "BREAK: every way on the beat, into it on a beat, no window of 3");
    }
    {   /* the looper: SHIFT + REC armed; the first note starts it; REC ends it at the nearest step; it plays */
        int32_t out[2 * CTL];
        uint32_t kw[16], k, i, n0, n2, found;
        for (k = 0; k < NKEY; k++)
            if (white_ix(k) >= 0)
                kw[white_ix(k)] = k;
        ui.sel = 0;
        btn(B_REC, 1);
        EXPECT(ui.lrec == 1 && met_on && !seq_run, "SHIFT + REC: the looper armed, the metronome on");
        fm1_in.notes |= 1u << kw[0];
        ui_input();
        n0 = key_note(kw[0], ui.oct[0]);
        EXPECT(seq_run && ui.lrec == 2 && loops[0][0].n == 1u && loops[0][0].ev[0].step == 0u, "the first note: step 0");
        fm1_ms += 15000u * 10u / (uint32_t)fx_bpm10;   /* (held a step) */
        fm1_in.notes &= ~(1u << kw[0]);
        ui_input();
        EXPECT(loops[0][0].ev[0].len8 == 8u, "held a step: 8 eighths");
        while (!(seq_gcount == 3u && seq_acc_now > SEQ_16TH * 3u / 5u))
            mix_block(out);
        tap(kw[2]);
        n2 = key_note(kw[2], ui.oct[0]);
        EXPECT(loops[0][0].n == 2u && loops[0][0].ev[1].step == 4u, "late in step 4: quantized to step 5");
        ui_draw();
        shot(dir, "loop_rec");
        while (!(seq_gcount == 9u && seq_acc_now < SEQ_16TH / 3u))
            mix_block(out);
        btn(B_REC, 0);
        EXPECT(!ui.lrec && loops[0][0].on && loops[0][0].len == 16u && loops[0][0].anchor == 0u && !met_on &&
               !loops[0][0].kick, "REC after 9 steps: the loop ends, a bar long (the nearest), from its first note");
        EXPECT(!strcmp(ui.msg, "LOOP 1  1 BAR"), "LOOP 1  1 BAR shown");
        while (seq_gcount != 16u)                /* (on, at step 9 of 16: its first note at the next bar) */
            mix_block(out);
        for (i = 0; i < 2u; i++)
            mix_block(out);
        for (i = 0, found = 0; i < SEQ_HOLD; i++)
            found |= sq_note[0][i] == n0;
        EXPECT(found, "its first step plays on the next bar");
        while (seq_gcount != 20u)
            mix_block(out);
        for (i = 0, found = 0; i < SEQ_HOLD; i++)
            found |= sq_note[0][i] == n2;
        EXPECT(found, "its fifth step plays, quantized");
        {   /* the rounding: recorded so many steps, the loop's length in whole bars, its phase where it was */
            static const uint32_t REC[4] = {23, 24, 37, 600}, LEN[4] = {16, 32, 32, 512};
            uint32_t c, gc = seq_gcount, ac = seq_acc_now, ok = 1;
            loop_t keep = loops[0][2];
            for (c = 0; c < 4u; c++) {
                loop_t *p = &loops[0][2];
                p->n = 1;
                p->ev[0].step = 0;
                p->ev[0].note = 60;
                p->ev[0].len8 = 4;
                p->on = 0;
                ui.lt = 0;
                ui.ll = 2;
                ui.lrec = 2;
                ui.lstart = 1000u + 3u;
                seq_gcount = ui.lstart + REC[c];
                seq_acc_now = 0;
                loop_end();
                ok &= p->len == LEN[c] && p->anchor == ui.lstart &&
                      (seq_gcount - p->anchor) % p->len == REC[c] % LEN[c];
            }
            EXPECT(ok, "a loop's length: the nearest whole bars (23 steps 1, 24 2, 37 2, 600 the most, 32), its phase kept");
            loops[0][2] = keep;
            seq_gcount = gc;
            seq_acc_now = ac;
        }
        /* SHIFT + REC held: the loops on the white keys */
        fm1_in.notes |= 1u << KEY_SHIFT;
        fm1_in.buttons |= 1u << PANEL_BTN[B_REC];
        host_press = 1u << PANEL_BTN[B_REC];
        ui_input();
        fm1_ms += 500;
        ui_input();
        EXPECT(ui.lview, "SHIFT + REC held: the loops shown");
        ui_leds();
        EXPECT(host_led[14u + kw[0]] && !host_led[14u + kw[1]], "loop 1 lit, no loop 2");
        tap(kw[0]);
        EXPECT(loops[0][0].mute, "its key: muted");
        fm1_in.notes |= 1u << kw[0];
        ui_input();
        fm1_ms += 900;
        ui_input();
        EXPECT(!loops[0][0].on, "held: emptied");
        fm1_in.notes = 0;
        ui_input();
        fm1_in.buttons = 0;
        ui_input();
        EXPECT(!ui.lrec && !ui.lview, "let go after the loops were shown: not armed");
        btn(B_PLAY, 0);
        EXPECT(!seq_run, "stopped");
        for (i = 0; i < 4u; i++)
            mix_block(out);
        btn(B_PLAY, 1);
        EXPECT(!ui.click, "SHIFT + PLAY: the metronome off");
        btn(B_PLAY, 1);
        /* SHIFT + REC held on 1.5 s: the track's loops emptied */
        loops[0][1].on = 1;
        loops[0][1].len = 16;
        loops[0][1].n = 0;
        fm1_in.notes |= 1u << KEY_SHIFT;
        fm1_in.buttons |= 1u << PANEL_BTN[B_REC];
        host_press = 1u << PANEL_BTN[B_REC];
        ui_input();
        fm1_ms += 600;
        ui_input();
        EXPECT(loops[0][1].on, "SHIFT + REC held 0.6 s: the loops shown, kept");
        fm1_ms += 1000;
        ui_input();
        EXPECT(!loops[0][1].on && ui.flash_on, "held 1.5 s: the track's loops emptied");
        fm1_in.buttons = 0;
        fm1_in.notes = 0;
        ui_input();
        EXPECT(!ui.lrec, "let go: not armed");
        fm1_ms += 2000;
        /* SHIFT + PRESETS: the families of the machine */
        {
            sound_t *sn = cur();
            uint32_t m = ui.eng[ui.sel], e0 = m;
            int32_t f0 = sn->family;
            fm1_in.notes |= 1u << KEY_SHIFT;
            turn(EN_PRESET, -1);
            EXPECT(ui.eng[ui.sel] == e0 && cur()->family != f0 && RECIPES[cur()->family].mach == m,
                   "SHIFT + PRESETS: another family, the same machine");
            turn(EN_PRESET, 200);
            {
                int32_t last = -1, i;
                for (i = 0; i < (int32_t)NRECIPE; i++)
                    if (RECIPES[i].mach == m)
                        last = i;
                EXPECT(cur()->family == last, "turned on: the machine's last family, there it stays");
            }
            fm1_in.notes &= ~(1u << KEY_SHIFT);
            ui_input();
            ui_draw();
            shot(dir, "family_browse");
        }
        /* PRESETS on a session's page: the page's, never the machine */
        k = ui.eng[ui.sel];
        btn(B_SAVE, 0);
        turn(EN_PRESET, 2);
        btn(B_SAVE, 0);
        turn(EN_PRESET, 3);
        EXPECT(ui.eng[ui.sel] == k, "PRESETS on the sessions' pages leaves the machine");
        btn(B_SAVE, 1);
        btn(B_GLO, 0);
        tap(KEY_EDIT);                           /* the EDIT pages: OCT the pages, PRESETS the half, never the machine */
        ui.edit_pg = 0;
        btn(B_OCTUP, 0);
        btn(B_OCTUP, 0);
        EXPECT(ui.page == PG_EDIT && ui.edit_pg == 2u && ui.half == 0u, "EDIT: OCT+ the next page, its A .. D");
        turn(EN_PRESET, 1);
        ui_input();
        EXPECT(ui.half == 1u && ui.eng[ui.sel] == k, "PRESETS right: E .. H, the machine kept");
        fm1_ms += 2000;
        ui_draw();
        shot(dir, "edit_half");
        turn(EN_PRESET, -1);
        ui_input();
        EXPECT(ui.half == 0u && ui.eng[ui.sel] == k, "PRESETS left: A .. D");
        fm1_in.notes |= 1u << KEY_SHIFT;
        ui_input();
        EXPECT(ui.half == 0u, "SHIFT no longer turns the half");
        fm1_in.notes &= ~(1u << KEY_SHIFT);
        ui_input();
        tap(KEY_EDIT);
        btn(B_SEQ, 0);
        btn(B_OCTUP, 0);
        turn(EN_PRESET, 1);
        ui_input();
        EXPECT(ui.page == PG_FXP && ui.fx_pg == 1u && ui.half == 1u && ui.eng[ui.sel] == k,
               "the effects: OCT the next, PRESETS the half");
        btn(B_SEL, 1);
        EXPECT(ui.page == PG_FXP && ui.sel == 1u, "SHIFT and a track's button on the effects' pages: the track chosen, "
                                                   "the page kept");
        ui_draw();
        shot(dir, "fx_comp_track");
        btn(B_GLO, 0);
        EXPECT(ui.page == PG_MAIN && ui.sel == 5u, "a track's button on the effects' pages: back to the track's view");
        {   /* the octave at the top right: drawn again as OCT- / OCT+ change it */
            int32_t o0 = ui.oct[ui.sel];
            ui_draw();
            btn(B_OCTUP, 0);
            ui_draw();
            EXPECT(ui.oct[ui.sel] == o0 + 1 && ui.shown_prj == (int32_t)(pat_cur * 32 + pat_dirty * 16 + ui.rec * 4 + ui.lrec) +
                   (ui.oct[ui.sel] + 3) * 4096 + (ui.chord_on[ui.sel] ? (ui.chord_form[ui.sel] + 1) * 32768 : 0) +
                   (ui.trn[ui.sel] + 64) * (1 << 21),
                   "the octave shown follows OCT+");
            btn(B_OCTDN, 0);
        }
        {   /* CHORD mode: the 1st black key (the track's view, the step view off) on or off, lit while on; a white key
             * its chord (three voices); SHIFT and it, the next form; a step put in after a chord: the chord; kept with
             * the pattern and the session */
            int32_t out[2 * CTL];
            uint32_t kw = 0, i, ng = 0, t = ui.sel, si;
            pat_t pp;
            prj_body_t bb;
            while (white_ix(kw) != 0)
                kw++;
            ui_input();
            ui.chord_on[t] = 0;
            ui.chord_form[t] = 0;
            for (i = 0; i < 8u; i++)
                mix_block(out);
            tap(KEY_GEN);
            ui_leds();
            EXPECT(ui.chord_on[t] && host_led[14u + KEY_GEN], "the 1st black key: CHORD mode on, lit");
            fm1_in.notes |= 1u << kw;
            ui_input();
            for (i = 0; i < 4u; i++)
                mix_block(out);
            for (i = 0; i < NVOICE; i++)
                ng += vox[i].sounding && vox[i].gate && vox[i].track == t;
            EXPECT(ng == 3u && ui.last_chf[t] == 1u, "a white key in CHORD mode: its triad, three voices");
            fm1_in.notes &= ~(1u << kw);
            ui_input();
            for (i = 0; i < 4u; i++)
                mix_block(out);
            for (i = 0, ng = 0; i < NVOICE; i++)
                ng += vox[i].gate && vox[i].track == t;
            EXPECT(ng == 0u, "the key let go: the chord's three let go");
            fm1_in.notes |= 1u << KEY_SHIFT;
            tap(KEY_GEN);
            fm1_in.notes &= ~(1u << KEY_SHIFT);
            ui_input();
            EXPECT(ui.chord_on[t] && ui.chord_form[t] == 1u, "SHIFT and the 1st black key: the next form");
            ui_draw();
            shot(dir, "chord_main");
            tap(kw);                             /* (an INV 1 played: what a step is given) */
            tap(KEY_STEP);
            for (si = 0; seq_note[t][si] && si < NSTEP; si++)
                ;
            {
                uint32_t ks = 0;
                while (white_ix(ks) != (int32_t)si)
                    ks++;
                fm1_in.notes |= 1u << ks;
                ui_input();
                fm1_in.notes &= ~(1u << ks);
                fm1_ms += 1000;
                ui_input();
            }
            EXPECT(si < NSTEP && seq_note[t][si] && seq_chf[t][si] == 2u, "a step put in after a chord: the chord (INV 1)");
            tap(KEY_STEP);
            pat_take(&pp);
            seq_chf[t][si] = 0;
            pat_store[15] = pp;
            pat_give(15, 1);
            EXPECT(seq_chf[t][si] == 2u, "a chord step kept with the pattern");
            prj_take(&bb);
            ui.chord_on[t] = 0;
            ui.chord_form[t] = 0;
            prj_give(&bb);
            EXPECT(ui.chord_on[t] && ui.chord_form[t] == 1u, "CHORD mode kept with the session");
            {   /* two chords with notes in common, legato: the first let go, the second's three all held */
                uint32_t k2 = 0, nh = 0, j, ok = 1;
                ui.chord_form[t] = 0;
                while (white_ix(k2) != 5)        /* (the 6th degree: its triad shares two notes with the 1st's) */
                    k2++;
                fm1_in.notes |= 1u << kw;
                ui_input();
                for (i = 0; i < 4u; i++)
                    mix_block(out);
                fm1_in.notes |= 1u << k2;
                ui_input();
                for (i = 0; i < 4u; i++)
                    mix_block(out);
                fm1_in.notes &= ~(1u << kw);
                ui_input();
                for (i = 0; i < 4u; i++)
                    mix_block(out);
                for (i = 0; i < NVOICE; i++)
                    if (vox[i].gate && vox[i].track == t) {
                        nh++;
                        for (j = 0, ok &= 0u; j < 3u; j++)
                            ok |= vox[i].note == ui.key_cn[k2][j];
                    }
                EXPECT(nh == 3u, "legato chords with notes in common: the first let go, the second's three held");
                {   /* (the same note held twice, the newer first in the pool: a note-off lets go the older) */
                    voice_t w0 = vox[0], w5 = vox[5];
                    vox[0].gate = vox[5].gate = 1;
                    vox[0].track = vox[5].track = (uint8_t)t;
                    vox[0].pend = vox[5].pend = 0;
                    vox[0].note = vox[5].note = 61;
                    vox[0].src = vox[5].src = SRC_KEY;
                    vox[0].age = 1000000u;
                    vox[5].age = 999000u;
                    note_off(t, 61, SRC_KEY);
                    EXPECT(vox[0].gate && !vox[5].gate, "the same note held twice: a note-off lets go the older");
                    vox[0] = w0;
                    vox[5] = w5;
                }
                fm1_in.notes &= ~(1u << k2);
                ui_input();
                for (i = 0; i < 4u; i++)
                    mix_block(out);
            }
            {   /* DRUM: no chords (the key says so; a white key one note) */
                uint32_t e0 = ui.eng[t], ng2 = 0;
                ui.eng[t] = ENG_FMD;
                tap(KEY_GEN);
                EXPECT(ui.chord_on[t] && str_eq(ui.msg, "NO CHORDS"), "DRUM: the chord key says NO CHORDS");
                fm1_in.notes |= 1u << kw;
                ui_input();
                EXPECT(ui.key_cn[kw][1] == 0u && ui.last_chf[t] == 0u, "DRUM: a white key plays its note alone");
                fm1_in.notes &= ~(1u << kw);
                ui_input();
                (void)ng2;
                ui.eng[t] = (uint8_t)e0;
                for (i = 0; i < 8u; i++)
                    mix_block(out);
            }
            tap(KEY_GEN);
            EXPECT(!ui.chord_on[t], "the 1st black key again: CHORD mode off");
            {   /* MUTE mode: the 2nd black key; a track's button let go soon mutes it (lit playing, dim muted), held
                 * 1 s solos it; off, the mutes stay and the buttons choose tracks again; kept with the session */
                uint32_t j, sel0 = ui.sel;
                prj_body_t mb;
                tap(KEY_TRK);
                ui_leds();
                EXPECT(ui.mute_mode && host_led[14u + KEY_TRK], "the 2nd black key: MUTE mode on, lit");
                ui_draw();
                shot(dir, "mute_mode");
                btn(B_SEL, 0);
                ui_input();
                ui_leds();
                EXPECT(trk_mute[1] && ui.sel == sel0 && !host_led[PANEL_BTN[B_SEL]] && host_led[PANEL_BTN[B_FX]],
                       "MUTE mode: a track's button let go soon mutes it (dim), the track not chosen");
                tap(KEY_TRK);
                btn(B_SEL, 0);                   /* (track 2, muted, chosen: its number dark) */
                ui.msg_until = fm1_ms;
                ui_draw();
                shot(dir, "muted_track");
                btn((uint32_t)TRACK_BTN[sel0], 0);
                tap(KEY_TRK);
                fm1_in.buttons |= 1u << PANEL_BTN[B_ENV];
                host_press = 1u << PANEL_BTN[B_ENV];
                ui_input();
                fm1_ms += 1100;
                ui_input();
                fm1_in.buttons &= ~(1u << PANEL_BTN[B_ENV]);
                ui_input();
                for (j = 0, i = 1; j < NTRACK; j++)
                    i &= trk_mute[j] == (j != 2u);
                EXPECT(i, "MUTE mode: a track's button held 1 s solos it, every other muted");
                {   /* held again: the mutes before the solo back (track 2 muted, as before) */
                    uint32_t q, okm = 1;
                    fm1_in.buttons |= 1u << PANEL_BTN[B_ENV];
                    host_press = 1u << PANEL_BTN[B_ENV];
                    ui_input();
                    fm1_ms += 1100;
                    ui_input();
                    fm1_in.buttons &= ~(1u << PANEL_BTN[B_ENV]);
                    ui_input();
                    for (q = 0; q < NTRACK; q++)
                        okm &= trk_mute[q] == (q == 1u);
                    EXPECT(okm && ui.solo == 0xFFu, "the soloed track's button held again: the mutes as before the solo");
                    fm1_in.buttons |= 1u << PANEL_BTN[B_ENV];   /* (and soloed again, for what follows) */
                    host_press = 1u << PANEL_BTN[B_ENV];
                    ui_input();
                    fm1_ms += 1100;
                    ui_input();
                    fm1_in.buttons &= ~(1u << PANEL_BTN[B_ENV]);
                    ui_input();
                }
                for (j = 0; j < 16u; j++)
                    mix_block(out);
                EXPECT(trk[0].mg == 0 && trk[2].mg == Q15, "a muted track's gain at 0 (gliding), the soloed one's whole");
                tap(KEY_TRK);
                EXPECT(!ui.mute_mode && trk_mute[0] && !trk_mute[2], "MUTE mode off: the mutes stay");
                prj_take(&mb);
                memset((void *)trk_mute, 0, NTRACK);
                prj_give(&mb);
                EXPECT(trk_mute[0] && !trk_mute[2], "the mutes kept with the session");
                btn(B_SEL, 0);
                EXPECT(ui.sel == 1u, "MUTE mode off: a track's button chooses the track");
                btn((uint32_t)TRACK_BTN[sel0], 0);
                memset((void *)trk_mute, 0, NTRACK);
                for (j = 0; j < 16u; j++)
                    mix_block(out);
            }
            seq_note[t][si] = 0;
            seq_chf[t][si] = 0;
        }
        {   /* MANGLE: SLICE gates the track in time; DESTROY breaks its samples; off, the track as it was */
            int32_t out[2 * CTL];
            uint32_t i, kind;
            double e[3];
            uint8_t run0 = seq_run;
            seq_run = 0;                         /* (the note alone: not the steps, wherever the tests before left
                                                  * the sequencer) */
            for (i = 0; i < 400u; i++)
                mix_block(out);
            for (kind = 0; kind < 3u; kind++) {
                sound_t *sn = &ui.snd[0][ui.eng[0]];
                int32_t gmin = Q15, gmax = 0;
                memset(sn->mg, 0, NM);
                lfos_off(sn);                    /* (the same note each time: no LFO, a recipe's sample-and-hold on
                                                  * the cutoff, drawn anew each note) */
                if (kind == 1u) {
                    sn->mg[M_SLICE] = 1;             /* (every other 1/16) */
                    sn->mg[M_SRATE] = 1;
                    sn->mg[M_SDEPTH] = 127;
                } else if (kind == 2u) {
                    sn->mg[M_DST] = 90;
                    sn->mg[M_DMODE] = 0;
                }
                ui.dirty[0] = 1;
                for (i = 0; i < 8u && ui.dirty[0]; i++) {
                    ui_publish();
                    mix_block(out);
                }
                ev_post(EV_ON, 0, 48, 127);
                e[kind] = 0;
                for (i = 0; i < 1500u; i++) {
                    uint32_t j;
                    mix_block(out);
                    for (j = 0; j < 2u * CTL; j++)
                        e[kind] += (double)out[j] * out[j];
                    gmin = mangle[0].g < gmin ? mangle[0].g : gmin;
                    gmax = mangle[0].g > gmax ? mangle[0].g : gmax;
                }
                ev_post(EV_STOP, 0, 0, 0);
                for (i = 0; i < 400u; i++)
                    mix_block(out);
                if (kind == 1u)
                    EXPECT(gmin < 1000 && gmax > 32000, "SLICE: the gate shut and open, in time");
            }
            EXPECT(e[1] < e[0] * 0.8 && e[2] != e[0], "SLICE cuts, DESTROY changes the sound");
            seq_run = run0;
            memset(ui.snd[0][ui.eng[0]].mg, 0, NM);
            ui.dirty[0] = 1;
            for (i = 0; i < 8u && ui.dirty[0]; i++) {
                ui_publish();
                mix_block(out);
            }
        }
        {
            par_t pp;
            sound_t sn = *cur();
            par_put(&sn, PG_MNG, M_SLICE, 5);
            par_get(&sn, PG_MNG, M_SLICE, &pp);
            EXPECT(str_eq(pp.name, "SLICE") && par_val(&sn, PG_MNG, M_SLICE) == 5 && pages_n(&sn) == SYN_PAGES[sn.mach] + 5u,
                   "MANGLE: its own page, SLICE on knob C");
        }
        {
            vck_t c;
            sound_t sn = *cur();
            sn.fx[X_OD] = 127;
            vck_cook(&c, &sn);
            EXPECT(c.od_g == 100 * 256, "OVERDRIVE: 0 .. 100 (more read as 100)");
        }
    }
    {   /* a session saved before build 18 (its sounds without MANGLE): found, loaded, the sounds as they were */
        static uint8_t sec[PRJ_SECT];
        prj_head_t *h = (prj_head_t *)(void *)sec;
        prj_body17_t *b = (prj_body17_t *)(void *)(sec + sizeof(prj_head_t));
        uint32_t t, e, same = 1;
        memset(sec, 0xFF, sizeof sec);
        memset(b, 0, sizeof *b);
        b->tempo = 111;
        for (t = 0; t < NTRACK; t++)
            for (e = 0; e < NENG; e++) {
                const sound_t *u = &ui.snd[t][e];
                sound17_t *o = &b->snd[t][e];
                o->mach = u->mach;
                o->fmach = u->fmach;
                memcpy(o->syn, u->syn, sizeof o->syn);
                memcpy(o->flt, u->flt, NF);
                memcpy(o->amp, u->amp, NA);
                memcpy(o->fx, u->fx, NX);
                o->family = u->family;
                o->seed = u->seed;
                b->eng[t] = ui.eng[t];
            }
        h->magic = PRJ_MAGIC;
        h->format = PRJ_FORMAT;
        h->what = 9u * 16u;
        h->len = sizeof(prj_body17_t);
        h->body_crc = prj_crc((const uint8_t *)b, sizeof *b);
        h->head_crc = prj_crc(sec, __builtin_offsetof(prj_head_t, head_crc));
        memcpy(host_flash + prj_sector(9, 0), sec, sizeof sec);
        prj_scan(1);
        EXPECT(prj_has[9], "a session from before build 18: found");
        EXPECT(!prj_load(9) && ui.tempo == 111, "and loaded");
        for (t = 0; t < NTRACK; t++)
            for (e = 0; e < NENG; e++)
                same &= !memcmp(ui.snd[t][e].syn, b->snd[t][e].syn, sizeof b->snd[t][e].syn) &&
                        !memcmp(ui.snd[t][e].flt, b->snd[t][e].flt, NF) && ui.snd[t][e].family == b->snd[t][e].family &&
                        ui.snd[t][e].mg[M_SLICE] == 0;
        EXPECT(same, "its sounds as they were, MANGLE off");
        EXPECT(!prj_load(2), "(session 3 back)");
    }
    {   /* the compressor: kept with the session; a session from builds 18 .. 23 loads with its start values */
        static uint8_t sec[PRJ_SECT];
        prj_head_t *h = (prj_head_t *)(void *)sec;
        fx_cp[CP_THR] = 50;
        sc_src[2] = 1;
        sc_lvl[2] = 100;
        EXPECT(!prj_save(2), "(session 3 saved)");
        fx_cp[CP_THR] = 0;
        sc_src[2] = 0;
        sc_lvl[2] = 0;
        EXPECT(!prj_load(2) && fx_cp[CP_THR] == 50u && sc_src[2] == 1u && sc_lvl[2] == 100u,
               "the compressor and the tracks' sidechains kept with the session");
        memcpy(sec, host_flash + prj_sector(2, 0), sizeof sec);   /* (as build 28 kept it: no tracks' sidechains) */
        sec_to49(sec);
        h->what = 11u * 16u;
        h->len = PRJ_LEN28;
        h->body_crc = prj_crc(sec + sizeof(prj_head_t), PRJ_LEN28);
        h->head_crc = prj_crc(sec, __builtin_offsetof(prj_head_t, head_crc));
        memcpy(host_flash + prj_sector(11, 0), sec, sizeof sec);
        prj_scan(1);
        EXPECT(prj_has[11] && !prj_load(11) && fx_cp[CP_THR] == 50u && !sc_src[2], "a session from build 28: no track ducked");
        EXPECT(!prj_load(2), "(session 3 back)");
        memcpy(sec, host_flash + prj_sector(2, 0), sizeof sec);   /* (session 3 as build 23 kept it: no compressor) */
        sec_to49(sec);
        h->what = 10u * 16u;
        h->len = PRJ_LEN23;
        h->body_crc = prj_crc(sec + sizeof(prj_head_t), PRJ_LEN23);
        h->head_crc = prj_crc(sec, __builtin_offsetof(prj_head_t, head_crc));
        memcpy(host_flash + prj_sector(10, 0), sec, sizeof sec);
        prj_scan(1);
        EXPECT(prj_has[10] && !prj_load(10) && !memcmp(fx_cp, CP0, NCP), "a session from build 23: the compressor's start");
        EXPECT(!prj_load(2), "(session 3 back)");
        memcpy(sec, host_flash + prj_sector(2, 0), sizeof sec);   /* (session 3 as builds 35 .. 37 kept it: the body to
                                                                   * its mutes, rounded up to 4 -- 2780 bytes) */
        sec_to49(sec);
        {
            prj_body49_t *b = (prj_body49_t *)(void *)(sec + sizeof(prj_head_t));
            b->mute[3] = 1;
            sec[sizeof(prj_head_t) + __builtin_offsetof(prj_body49_t, sel)] = 0;     /* (the padding, as memset left it) */
            sec[sizeof(prj_head_t) + __builtin_offsetof(prj_body49_t, sel) + 1u] = 0;
        }
        h->what = 12u * 16u;
        h->len = 2780u;
        h->body_crc = prj_crc(sec + sizeof(prj_head_t), 2780u);
        h->head_crc = prj_crc(sec, __builtin_offsetof(prj_head_t, head_crc));
        memcpy(host_flash + prj_sector(12, 0), sec, sizeof sec);
        prj_scan(1);
        EXPECT(prj_has[12] && !prj_load(12) && trk_mute[3] && !loops[0][0].on, "a session from build 37 (2780 bytes): found, loaded");
        EXPECT(!prj_load(2), "(session 3 back)");
        {   /* (build 50) a session as builds 38 .. 49 kept it -- its loops' generation where they had it -- found, its
             * loops' sectors counted in use, loaded, its sounds as they were and their LFOs off; LFOs saved now */
            sound_t keep = ui.snd[1][ui.eng[1]];
            uint32_t g49, ok = 1, t2, e2;
            ui.snd[1][ui.eng[1]].lfo[0].dest = 1 + PG_FLT1 * 8 + 4;
            ui.snd[1][ui.eng[1]].lfo[0].dep = 90;
            ui.snd[1][ui.eng[1]].lfo[2].wave = LW_RND;
            EXPECT(!prj_save(2), "(session 3 saved, an LFO set)");
            ui.snd[1][ui.eng[1]] = keep;
            EXPECT(!prj_load(2) && ui.snd[1][ui.eng[1]].lfo[0].dest == 1 + PG_FLT1 * 8 + 4 &&
                   ui.snd[1][ui.eng[1]].lfo[0].dep == 90 && ui.snd[1][ui.eng[1]].lfo[2].wave == LW_RND,
                   "LFOs kept with the session");
            memcpy(sec, host_flash + prj_sector(2, 0), sizeof sec);
            sec_to49(sec);
            ((prj_body49_t *)(void *)(sec + sizeof(prj_head_t)))->loop_gen = 4242u;
            ((prj_body49_t *)(void *)(sec + sizeof(prj_head_t)))->loop_nsec = 0;
            h->what = 13u * 16u;
            h->len = PRJ_LEN49;
            h->body_crc = prj_crc(sec + sizeof(prj_head_t), PRJ_LEN49);
            h->head_crc = prj_crc(sec, __builtin_offsetof(prj_head_t, head_crc));
            memcpy(host_flash + prj_sector(13, 0), sec, sizeof sec);
            prj_scan(1);
            g49 = prj_lgen[13];
            EXPECT(prj_has[13] && g49 == 4242u, "a session from build 49: found, its loops' generation read where it was");
            EXPECT(!prj_load(13), "and loaded");
            for (t2 = 0; t2 < NTRACK; t2++)
                for (e2 = 0; e2 < NENG; e2++) {
                    const sound_t *u = &ui.snd[t2][e2];
                    ok &= !memcmp(u, (const uint8_t *)host_flash + prj_sector(2, 0) + sizeof(prj_head_t) +
                                  __builtin_offsetof(prj_body_t, snd) + (t2 * NENG + e2) * sizeof(sound_t), sizeof(sound49_t));
                    ok &= u->lfo[0].dest == 0 && u->lfo[1].dep == 64 && u->lfo[2].fade == 64;
                }
            EXPECT(ok, "its sounds as they were, their LFOs off");
            prj_delete(13);
            EXPECT(!prj_load(2), "(session 3 back)");
            ui.snd[1][ui.eng[1]] = keep;             /* (and saved again without the LFO, for what follows) */
            EXPECT(!prj_save(2) && !prj_load(2), "(session 3 saved as it was, and loaded: its sounds published)");
            events_take();                       /* (the loads' events taken, as the audio would: the queue has room) */
        }
    }
    {   /* the compressor works: a loud mix takes gain off, more at a lower threshold; a track as the sidechain */
        int32_t out[2 * CTL];
        uint32_t i;
        int32_t gr_hi, gr_lo;
        memcpy(fx_cp, CP0, NCP);
        fx_cp[CP_SCF] = 64;                      /* (listening to it all) */
        fx_cp[CP_THR] = 127;                     /* 0 dB: next to nothing off */
        fx_cook_all();
        for (i = 0; i < 4u; i++)
            ev_post(EV_ON, i, 48 + 7 * i, 127);
        for (i = 0; i < 600u; i++)
            mix_block(out);
        gr_hi = cp.gr;
        fx_cp[CP_THR] = 40;                      /* -41 dB */
        fx_cook_all();
        for (i = 0; i < 600u; i++)
            mix_block(out);
        gr_lo = cp.gr;
        EXPECT(gr_lo > gr_hi + (6 << 16), "a lower threshold: more taken off");
        {   /* a track's sidechain: a kick on track 1 each beat, a pad held on track 2 (SC T1), another pad on track 3
             * (OFF): track 2 ducked by its SC LEVEL, on the hit and back within the beat; track 3 and the kick let be */
            int32_t gmin = Q16, gmax = 0, g3min = Q16;
            uint32_t b, lvl, over = 0, all = 0;
            for (i = 0; i < 4u; i++)
                ev_post(EV_STOP, i, 0, 0);
            gen_sound(&ui.snd[0][ENG_FMD], ENG_FMD, 1, -1, 99u);
            ui.eng[0] = ENG_FMD;
            gen_sound(&ui.snd[1][ENG_WAV], ENG_WAV, 43, -1, 7u);
            ui.eng[1] = ENG_WAV;
            gen_sound(&ui.snd[2][ENG_WAV], ENG_WAV, 43, -1, 8u);
            ui.eng[2] = ENG_WAV;
            mark(0);
            mark(1);
            mark(2);
            for (i = 0; i < 8u; i++) {
                ui_publish();
                mix_block(out);
            }
            memset((void *)seq_note, 0, sizeof seq_note);
            memset((void *)seq_len, 0, sizeof seq_len);
            memset((void *)seq_div, SEQ_DIV1, sizeof seq_div);
            memset((void *)seq_dir, DIR_FWD, sizeof seq_dir);
            seq_note[0][0] = seq_note[0][4] = seq_note[0][8] = seq_note[0][12] = 36;
            memcpy(fx_cp, CP0, NCP);
            fx_cook_all();
            memset((void *)sc_src, 0, sizeof sc_src);
            sc_src[1] = 1;
            ev_post(EV_ON, 1, 60, 127);
            ev_post(EV_ON, 2, 64, 127);
            seq_run = 1;
            for (lvl = 0; lvl < 3u; lvl++) {     /* SC LEVEL 50, 100, 0 */
                sc_lvl[1] = (uint8_t)(lvl == 0u ? 64 : lvl == 1u ? 127 : 0);
                gmin = Q16;
                gmax = 0;
                g3min = Q16;
                over = all = 0;
                for (b = 0; b < 2800u; b++) {
                    mix_block(out);
                    if (b > 700u) {
                        gmin = dk_gp[1] < gmin ? dk_gp[1] : gmin;
                        gmax = dk_gp[1] > gmax ? dk_gp[1] : gmax;
                        g3min = dk_gp[2] < g3min ? dk_gp[2] : g3min;
                        all++;
                        over += dk_gp[1] < Q16 / 2;
                    }
                }
                if (getenv("DKDBG"))
                    printf("lvl %u gmin %d gmax %d over %u/%u\n", lvl, gmin, gmax, over, all);
                if (lvl == 0u)
                    EXPECT(gmin < Q16 / 4 && gmin > Q16 / 64 && gmax > Q16 * 9 / 10,
                           "SC LEVEL 50: ducked ~20 dB on the hit, back between");
                else if (lvl == 1u)
                    EXPECT(gmin < Q16 / 50 && over * 10u < all * 7u,
                           "SC LEVEL 100: down near 40 dB on the hit (a gate), back for part of the beat");
                else
                    EXPECT(gmin == Q16, "SC LEVEL 0: let be");
                EXPECT(g3min == Q16, "a track with SC SOURCE OFF: let be");
            }
            sc_src[1] = 0;
            seq_run = 0;
            ev_post(EV_STOP, 1, 0, 0);
            ev_post(EV_STOP, 2, 0, 0);
            for (i = 0; i < 400u; i++)
                mix_block(out);
            for (i = 0; i < 4u; i++)
                ev_post(EV_STOP, i, 0, 0);
        }
        for (i = 0; i < 4u; i++)
            ev_post(EV_STOP, i, 0, 0);
        for (i = 0; i < 400u; i++)
            mix_block(out);
        memcpy(fx_cp, CP0, NCP);
        fx_cook_all();
    }
    /* the long presses: ARP held saves the pattern playing, HOME held the project */
    pat_tempo = 77;
    pat_dirty = 1;
    fm1_in.buttons |= 1u << PANEL_BTN[B_ARP];
    host_press = 1u << PANEL_BTN[B_ARP];
    ui_input();
    fm1_ms += 900;
    ui_input();
    fm1_in.buttons = 0;
    ui_input();
    fm1_ms += 2000;
    EXPECT(ui.page == PG_MAIN && pat_store[pat_cur].tempo == 77 && pat_store[pat_cur].used && !pat_dirty,
           "ARP held saves the pattern playing");
    ui.tempo = 99;
    prj_dirty = 1;
    fm1_in.buttons |= 1u << PANEL_BTN[B_HOME];
    host_press = 1u << PANEL_BTN[B_HOME];
    ui_input();
    fm1_ms += 900;
    ui_input();
    fm1_in.buttons = 0;
    ui_input();
    fm1_ms += 2000;
    EXPECT(!prj_dirty && !prj_read(2, 0) && ((const prj_body_t *)(const void *)(prj_buf + sizeof(prj_head_t)))->tempo == 99,
           "HOME held saves the project");
    /* SAVE held: the session saved at once; never saved, the first empty slot asked (SAVE NEW?, YES), SAVE: saved */
    ui.tempo = 101;
    prj_dirty = 1;
    fm1_in.buttons |= 1u << PANEL_BTN[B_SAVE];
    host_press = 1u << PANEL_BTN[B_SAVE];
    ui_input();
    fm1_ms += 900;
    ui_input();
    fm1_in.buttons = 0;
    ui_input();
    fm1_ms += 2000;
    EXPECT(ui.page == PG_MAIN && !prj_dirty && !prj_read((uint32_t)prj_cur, 0) &&
           ((const prj_body_t *)(const void *)(prj_buf + sizeof(prj_head_t)))->tempo == 101, "SAVE held saves the session");
    {
        int32_t was = prj_cur;
        uint32_t k;
        for (k = 0; k < PRJ_SLOTS && prj_has[k]; k++)
            ;
        prj_cur = -1;
        fm1_in.buttons |= 1u << PANEL_BTN[B_SAVE];
        host_press = 1u << PANEL_BTN[B_SAVE];
        ui_input();
        fm1_ms += 900;
        ui_input();
        fm1_in.buttons = 0;
        ui_input();
        EXPECT(k < PRJ_SLOTS && ui.page == PG_PRJD && pj.ask == PQ_SAVENEW && pj.yes && pj.slot[0] == k,
               "SAVE held, never saved: the first empty slot, SAVE NEW? (YES)");
        ui_draw();
        shot(dir, "save_new");
        btn(B_SAVE, 0);
        EXPECT(ui.page == PG_MAIN && prj_cur == (int32_t)k && prj_has[k], "SAVE: saved into it");
        (void)was;
    }
    {   /* SAVE pressed and let go apart, as the panel does it: the list, a session's page, LOAD (build 39) */
        uint32_t k, x;
        for (k = 0; k < PRJ_SLOTS && !prj_has[k]; k++)
            ;
        prj_dirty = 0;
        pat_dirty = 0;
        ui.page = PG_MAIN;
        prj_cur = -1;
        for (x = 0; x < 2u; x++) {               /* (the list; the slot's page) */
            fm1_in.buttons |= 1u << PANEL_BTN[B_SAVE];
            host_press = 1u << PANEL_BTN[B_SAVE];
            ui_input();
            fm1_ms += 150;
            ui_input();
            fm1_in.buttons = 0;
            ui_input();
            fm1_ms += 300;
            if (!x)
                pj.slot[0] = (uint8_t)k;
        }
        EXPECT(ui.page == PG_PRJD, "SAVE tapped twice: the slot's page");
        pj.row = PR_LOAD;
        {   /* (SAVE held a second and more on LOAD, as a firm press: loaded, not saved) */
            uint32_t e0 = host_erases;
            fm1_in.buttons |= 1u << PANEL_BTN[B_SAVE];
            host_press = 1u << PANEL_BTN[B_SAVE];
            ui_input();
            for (x = 0; x < 12u; x++) {
                fm1_ms += 100;
                ui_input();
            }
            fm1_in.buttons = 0;
            ui_input();
            EXPECT(ui.page == PG_MAIN && prj_cur == (int8_t)k, "SAVE held 1.2 s on LOAD: the session loaded");
            EXPECT(host_erases == e0, "SAVE held on LOAD: nothing saved");
        }
    }
    {   /* the loops with the session (build 38): saved, loaded back whole; every loop full (7 sectors) saved over and
         * over into one slot; copied; the last session loaded at the start */
        static loop_t want[NTRACK][NLOOP];
        uint32_t t, l, i, k, same, j;
        int rc;
        for (k = 0; k < PRJ_SLOTS && prj_has[k]; k++)
            ;
        lop_clear();
        loops[2][1].n = 3;
        loops[2][1].len = 400;
        loops[2][1].mute = 1;
        for (i = 0; i < 3u; i++) {
            loops[2][1].ev[i].step = (uint16_t)(i * 100u);
            loops[2][1].ev[i].len8 = (uint16_t)(1000u + i);
            loops[2][1].ev[i].note = (uint8_t)(50u + i);
            loops[2][1].ev[i].vel = (uint8_t)(90u + i);
        }
        loops[2][1].on = 1;
        ui.sel = 4;
        memcpy(want, loops, sizeof want);
        rc = prj_save(k);
        EXPECT(!rc && prj_lgen[k] && prj_has[k], "a session saved with a loop");
        lop_clear();
        ui.sel = 0;
        rc = prj_load(k);
        for (t = 0, same = 1; t < NTRACK; t++)
            for (l = 0; l < NLOOP; l++)
                same &= loops[t][l].on == want[t][l].on && loops[t][l].n == want[t][l].n &&
                        loops[t][l].len == want[t][l].len && loops[t][l].mute == want[t][l].mute &&
                        !memcmp(loops[t][l].ev, want[t][l].ev, loops[t][l].n * sizeof(lev_t));
        EXPECT(!rc && same && ui.sel == 4u, "loaded: its loop back, length, notes and mute; the track chosen");
        for (t = 0; t < NTRACK; t++)             /* every loop full: 32 bars, 256 notes */
            for (l = 0; l < NLOOP; l++) {
                loop_t *p = &loops[t][l];
                p->n = LOOP_EV;
                p->len = LOOP_MAX;
                p->mute = (uint8_t)(l == 2u);
                for (i = 0; i < LOOP_EV; i++) {
                    p->ev[i].step = (uint16_t)(i * 2u);
                    p->ev[i].len8 = (uint16_t)(1u + (i * 37u + t) % 4096u);
                    p->ev[i].note = (uint8_t)(24u + (i + t * 3u + l) % 90u);
                    p->ev[i].vel = (uint8_t)(1u + (i * 7u) % 127u);
                }
                p->on = 1;
            }
        memcpy(want, loops, sizeof want);
        for (j = 0, rc = 0; j < 12u && !rc; j++)
            rc = prj_save(k);
        EXPECT(!rc && j == 12u, "every loop full: saved into one slot 12 times over (the store reused)");
        lop_clear();
        rc = prj_load(k);
        for (t = 0, same = 1; t < NTRACK; t++)
            for (l = 0; l < NLOOP; l++)
                same &= loops[t][l].on && loops[t][l].n == LOOP_EV && loops[t][l].len == LOOP_MAX &&
                        loops[t][l].mute == want[t][l].mute && !memcmp(loops[t][l].ev, want[t][l].ev, sizeof want[t][l].ev);
        EXPECT(!rc && same, "every loop full: loaded back whole");
        {
            uint32_t to;
            for (to = 0; to < PRJ_SLOTS && prj_has[to]; to++)
                ;
            rc = prj_copy(k, to);
            EXPECT(!rc && prj_has[to] && prj_lgen[to] && prj_lgen[to] != prj_lgen[k], "copied, its loops with it");
            EXPECT(!prj_delete(k), "the first deleted");
            lop_clear();
            rc = prj_load(to);
            for (t = 0, same = 1; t < NTRACK; t++)
                for (l = 0; l < NLOOP; l++)
                    same &= loops[t][l].n == LOOP_EV && !memcmp(loops[t][l].ev, want[t][l].ev, sizeof want[t][l].ev);
            EXPECT(!rc && same, "the copy's loops whole, the first deleted");
            /* the start: the last session (the copy, just loaded) loaded again */
            lop_clear();
            prj_cur = -1;
            prj_scan(1);
            EXPECT(lst_slot == (int8_t)to, "the last session noted");
            prj_boot();
            EXPECT(prj_cur == (int8_t)to && loops[0][0].n == LOOP_EV, "the start: the last session loaded, its loops");
            /* a session from before (no loops): loaded, the loops emptied */
            {
                uint32_t old;
                for (old = 0; old < PRJ_SLOTS && !prj_has[old]; old++)
                    ;
                if (old != to && prj_lgen[old] == 0u) {
                    rc = prj_load(old);
                    EXPECT(!rc && !loops[0][0].on && !loops[0][0].n, "a session with no loops: the loops emptied");
                }
            }
        }
        lop_clear();
    }
    {   /* MIDI (build 46) */
        int32_t out[2 * CTL];
        static const midi_cfg_t D = MIDI_CFG0;
        uint32_t i, k, n, b, f8, last, ok, lo, hi;
        const uint32_t BT = (uint32_t)((uint64_t)CTL * 24000000u / FS);   /* (a block, in ticks) */
        lop_clear();
        for (k = 0; k < NSTEP; k++) {            /* (track 1: a note on each beat; the rest empty) */
            for (i = 0; i < NTRACK; i++)
                seq_note[i][k] = 0;
            seq_note[0][k] = (k % 4u) ? 0 : 60;
            seq_vel[0][k] = 100;
            seq_glen[0][k] = GLEN_DEF;
            seq_cond[0][k] = CD_NONE;
            seq_rtg[0][k] = 0;
            seq_mut[0][k] = 0;
            seq_mfx[0][k] = MFX_OFF;
            seq_chf[0][k] = 0;
            memset((void *)seq_lid[0][k], PL_NONE, NPL);
        }
        memset((void *)trk_mute, 0, sizeof trk_mute);
        mcfg = D;
        mcfg.clk_out = mcfg.send_note = 1;
        fx_bpm10 = 1200;
        /* clock out and transport: SPP 0 and START with the first pulse, as step 1 plays; 6 pulses a 16th, evenly */
        host_mo_n = 0;
        seq_run = 1;
        mix_block(out);
        EXPECT(host_mo_n >= 3u && host_mo[0] == (0x3u | 0xF2u << 8) && host_mo[1] == (0xFu | 0xFAu << 8) &&
               host_mo[2] == (0xFu | 0xF8u << 8), "PLAY: SPP 0, START and the first clock, in the first block");
        for (i = 0, f8 = 0, last = 0, ok = 1, lo = 99, hi = 0; seq_gcount < 64u; i++) {
            uint32_t m0 = host_mo_n;
            mix_block(out);
            for (k = m0; k < host_mo_n && k < 4096u; k++)
                if (host_mo[k] == (0xFu | 0xF8u << 8)) {
                    if (f8) {
                        lo = i - last < lo ? i - last : lo;
                        hi = i - last > hi ? i - last : hi;
                    }
                    f8++;
                    last = i;
                }
        }
        EXPECT(f8 >= 64u * 6u - 1u && f8 <= 64u * 6u + 1u, "CLOCK OUT: 6 pulses a 16th (4 bars: 384)");
        EXPECT(lo >= 28u && hi <= 29u, "CLOCK OUT at 120: a pulse every 28 or 29 blocks (20.8 ms, to the block)");
        for (k = 0, n = 0, b = 0; k < host_mo_n && k < 4096u; k++) {   /* (the notes: on channel 1, on and off) */
            n += host_mo[k] == (0x9u | 0x90u << 8 | 60u << 16 | 100u << 24);
            b += host_mo[k] == (0x8u | 0x80u << 8 | 60u << 16);
        }
        EXPECT(n >= 16u && n <= 17u && b >= n - 1u, "SEND NOTES: each step's note on channel 1, and its off");
        trk_mute[0] = 1;
        host_mo_n = 0;
        while (seq_gcount < 80u)
            mix_block(out);
        for (k = 0, n = 0; k < host_mo_n && k < 4096u; k++)
            n += (host_mo[k] & 0xF0FFu) == (0x9u | 0x90u << 8);
        EXPECT(!n, "a muted track sends no notes");
        trk_mute[0] = 0;
        host_mo_n = 0;
        seq_run = 0;
        mix_block(out);
        EXPECT(host_mo_n >= 1u && host_mo[0] == (0xFu | 0xFCu << 8), "STOP sent as it stops");
        /* following a clock: 127.3 BPM, with up to +-0.5 ms of jitter on each pulse; START; then 90.7 BPM; CONTINUE */
        mcfg = D;
        mcfg.clk_in = 1;
        {
            uint32_t now = 1000000u, next, pulses = 0, started = 0, maxss = 0, iv = 471328u, seed = 7;   /* (127.3 BPM) */
            int32_t err;
            host_ticks = now;
            midi_rt(0xF8u, 0);                   /* (a clock before START: the tempo taken, nothing played) */
            next = now + iv;
            for (i = 0; i < 24u * 4u; ) {        /* (a bar of clock, then START) */
                host_ticks += BT;
                mix_block(out);
                if ((int32_t)(host_ticks - next) >= 0) {
                    midi_rt(0xF8u, 0);
                    next += iv;
                    i++;
                }
            }
            ui_input();
            EXPECT(!seq_run && fx_bpm10 >= 1271 && fx_bpm10 <= 1275, "CLOCK IN: its tempo taken (127.3), not playing");
            midi_rt(0xFAu, 0);
            for (i = 0; i < 24u * 4u * 32u; ) {  /* (32 bars) */
                host_ticks += BT;
                mix_block(out);
                if ((int32_t)(host_ticks - next) >= 0) {
                    if (started) {               /* (as pulse pulses + 1 comes: where the sequencer is, against where
                                                  * it should be, in thousandths of a pulse) */
                        err = (int32_t)(seq_gcount * 6000u + (uint32_t)((uint64_t)seq_acc_now * 6000u / SEQ_16TH)) -
                              (int32_t)((pulses + 1u) * 1000u);
                        err = err < 0 ? -err : err;
                        if (pulses > 24u * 4u * 17u && (uint32_t)err > maxss)   /* (a bar after the change) */
                            maxss = (uint32_t)err;
                    }
                    seed = seed * 1664525u + 1013904223u;
                    host_ticks = next + (seed >> 16) % 24000u - 12000u;   /* (jitter: +-0.5 ms) */
                    midi_rt(0xF8u, 0);
                    host_ticks = next;
                    next += iv;
                    i++;
                    if (seq_run && !started) {   /* (the pulse after the one it started on: the 2nd) */
                        started = 1;
                        pulses = 1;
                    } else if (started)
                        pulses++;
                    if (i == 24u * 4u * 16u) {   /* (half way: 90.7) */
                        iv = 661521u;
                        ui_input();
                    }
                }
            }
            ui_input();
            EXPECT(started && seq_gcount / 16u >= 31u, "START: from the next pulse, 32 bars played");
            EXPECT(maxss <= 100u, "CLOCK IN: within a tenth of a pulse of the master, jitter and a tempo change");
            EXPECT(fx_bpm10 >= 905 && fx_bpm10 <= 909, "CLOCK IN: the tempo follows (90.7)");
            midi_rt(0xFCu, 0);
            mix_block(out);
            EXPECT(!seq_run, "STOP received: stopped");
            midi_rt(0xF2u, 20);                  /* (song position: the 21st 16th) */
            midi_rt(0xFBu, 0);
            host_ticks += BT;
            mix_block(out);
            EXPECT(!seq_run, "CONTINUE: not before the next pulse");
            host_ticks = next;
            midi_rt(0xF8u, 0);
            mix_block(out);
            EXPECT(seq_run && seq_gcount == 20u && seq_pos[0] == 4u, "CONTINUE: from the song position (16th 21: step 5)");
            midi_rt(0xFCu, 0);
            mix_block(out);
        }
        /* in: notes by channel (AUTO: the chosen track), CCs, program changes */
        mcfg = D;
        mcfg.pc_in = 1;
        ui.sel = 4;
        midi_in_q[mi_w++ % MQ] = 0x9u | 0x92u << 8 | 62u << 16 | 90u << 24;    /* (channel 3: track 3) */
        midi_in_q[mi_w++ % MQ] = 0x9u | 0x9Fu << 8 | 64u << 16 | 90u << 24;    /* (channel 16, AUTO: track 5) */
        midi_in_q[mi_w++ % MQ] = 0x9u | 0x96u << 8 | 65u << 16 | 90u << 24;    /* (channel 7: none) */
        midi_in_q[mi_w++ % MQ] = 0xBu | 0xB1u << 8 | CC_LEVEL << 16 | 0u << 24;   /* (channel 2: track 2's level off) */
        midi_in_q[mi_w++ % MQ] = 0xBu | 0xB1u << 8 | (uint32_t)MIDI_CC[PG_FLT1 * 8u + 4u] << 16 | 127u << 24;
        midi_in_q[mi_w++ % MQ] = 0xCu | 0xCFu << 8 | 3u << 16;                  /* (pattern 4) */
        ui_input();
        EXPECT(ui.midi_track[62] == 1u << 2 && ui.midi_track[64] == 1u << 4 && !ui.midi_track[65],
               "notes in: channel 3 track 3, AUTO the chosen track, channel 7 none");
        EXPECT(ui.level[1] == 0 && ui.snd[1][ui.eng[1]].flt[4] == 127, "CCs in: track 2's level, its filter frequency");
        EXPECT(pj.slot[1] == 3u, "PROG CH IN: pattern 4");
        /* out: a knob's CC on the track's channel */
        mcfg.send_cc = 1;
        ui.sel = 0;
        mm_r = mm_w;
        par_turn(PG_FLT1, 4, -10);
        EXPECT(mm_w - mm_r == 1u && (mm_q[mm_r % MMQ] & 0xFFFFu) == (0xBu | 0xB0u << 8) &&
               ((mm_q[mm_r % MMQ] >> 16) & 0x7Fu) == MIDI_CC[PG_FLT1 * 8u + 4u], "SEND CC: the knob's CC on channel 1");
        mm_r = mm_w;
        /* GLOBAL SETUP: SHIFT + SAVE; MIDI; FILTER; SEND NOTES on; left: in the flash, and read back */
        mcfg = D;
        ui.page = PG_MAIN;
        btn(B_SAVE, 1);
        EXPECT(ui.page == PG_SET && !set.lvl, "SHIFT + SAVE: GLOBAL SETUP");
        ui_draw();
        shot(dir, "setup");
        btn(B_SAVE, 0);                          /* (MIDI) */
        turn(EN_PRESET, 4);                      /* (SEND) */
        btn(B_SAVE, 0);                          /* (NOTES) */
        btn(B_SAVE, 0);
        turn(EN_PRESET, 1);
        ui_draw();
        shot(dir, "setup_midi");
        btn(B_SAVE, 0);
        EXPECT(mcfg.send_note == 1u && set.lvl == 2u && set.part == 4u, "MIDI > SEND > NOTES: ON");
        btn(B_SAVE, 1);
        btn(B_SAVE, 1);
        btn(B_SAVE, 1);
        EXPECT(ui.page == PG_MAIN, "SHIFT + SAVE three times: out");
        ui_input();                              /* (the next pass: what changed written) */
        mcfg = D;
        setup_load();
        EXPECT(mcfg.send_note == 1u, "the setting kept in the flash, read back at the start");
        {   /* the TRS jack (build 47): bytes as the DMA leaves them -- running status, a clock inside a note, a song
             * position; PORTS leaving either input out; a record from build 46 (no PORTS) read with both on */
            static const uint8_t B[] = {0x91, 60, 100, 62, 90, 0x90, 64, 0xF8, 80, 0xF2, 5, 1};
            uint32_t r0, n0 = xk_n;
            for (i = 0; i < UM_RING; i++)
                um_ring[i] = UM_EMPTY;
            um.rd = 0;
            mcfg = D;
            mcfg.clk_in = 1;
            r0 = mi_w;
            for (i = 0; i < sizeof B; i++)
                um_ring[i] = B[i];
            uart_midi_take(sizeof B);
            EXPECT(mi_w - r0 == 3u && midi_in_q[r0 % MQ] == (0x19u | 0x91u << 8 | 60u << 16 | 100u << 24) &&
                   midi_in_q[(r0 + 1u) % MQ] == (0x19u | 0x91u << 8 | 62u << 16 | 90u << 24) &&
                   midi_in_q[(r0 + 2u) % MQ] == (0x19u | 0x90u << 8 | 64u << 16 | 80u << 24),
                   "TRS: notes (running status too, a clock inside one) as cable 1's packets");
            EXPECT(xk_n == n0 + 1u && xk_spp == 5u + 128u, "TRS: the clock to midi_rt, the song position (133)");
            ui.sel = 0;
            memset(ui.midi_track, 0, sizeof ui.midi_track);
            ui_input();
            EXPECT(ui.midi_track[60] == 1u << 1 && ui.midi_track[64] == 1u, "TRS: its notes routed by channel as USB's");
            mcfg.trs_in = 0;                     /* (PORTS: TRS IN off) */
            n0 = xk_n;
            for (i = 0; i < 4u; i++)
                um_ring[(um.rd + i) % UM_RING] = (uint8_t)(i == 0u ? 0x92u : i == 3u ? 0xF8u : 61u + i);
            uart_midi_take(4);
            memset(ui.midi_track, 0, sizeof ui.midi_track);
            ui_input();
            EXPECT(xk_n == n0 && !ui.midi_track[62], "TRS IN off: its notes and clock left out");
            mcfg = D;
            mcfg.usb_in = 0;                     /* (PORTS: USB IN off) */
            midi_in_q[mi_w++ % MQ] = 0x9u | 0x92u << 8 | 63u << 16 | 90u << 24;
            ui_input();
            EXPECT(!ui.midi_track[63], "USB IN off: USB's notes left out");
            mcfg = D;
            mcfg.rcv_cc = 0;                     /* (a record as build 46 wrote it: up to PORTS) */
            memset(prj_buf, 0, sizeof(prj_head_t) + sizeof(midi_cfg_t));
            memcpy(prj_buf + sizeof(prj_head_t), &mcfg, MIDI_CFG46);
            prj_head_fill(SET_MAGIC, 0, MIDI_CFG46);
            prj_flash_erase(SET_SECT);
            prj_flash_prog(SET_SECT, prj_buf, sizeof(prj_head_t) + MIDI_CFG46);
            mcfg = D;
            setup_load();
            EXPECT(!mcfg.rcv_cc && mcfg.usb_in && mcfg.trs_in, "build 46's settings read, both inputs on");
        }
        {   /* BACK at the top of GLOBAL SETUP's levels (build 48) */
            ui.page = PG_MAIN;
            btn(B_SAVE, 1);
            btn(B_SAVE, 0);                      /* (MIDI: on SYNC, BACK above) */
            EXPECT(set.lvl == 1u && set.pc == 1u, "MIDI opens on its first part");
            turn(EN_PRESET, -1);
            ui_draw();
            shot(dir, "setup_back");
            btn(B_SAVE, 0);
            EXPECT(set.lvl == 0u, "SAVE on BACK: GLOBAL SETUP's top");
            btn(B_SAVE, 0);
            btn(B_SAVE, 0);                      /* (SYNC: on CLK IN) */
            EXPECT(set.lvl == 2u && set.row == 1u && set.part == 0u, "SYNC opens on CLK IN");
            turn(EN_PRESET, -3);
            btn(B_SAVE, 0);
            EXPECT(set.lvl == 1u && set.pc == 1u, "SAVE on BACK: MIDI's parts, on SYNC");
            btn(B_SAVE, 1);
            btn(B_SAVE, 1);
            ui_input();
        }
        {   /* MIDI LEARN (build 48): the 4th black key; a sound's parameter and an effect's learned from a controller's
             * CCs; a CC learned before the map; SHIFT + SAVE clears; off, kept in the flash */
            uint32_t fl, kk;
            sound_t *sn;
            memset(mlearn, 0, sizeof mlearn);
            mcfg = D;
            ui.page = PG_MAIN;
            ui.sv = 0;
            ui.sel = 2;
            for (kk = 0; kk < NKEY; kk++)
                ;
            key(KEY_MOD, 1);
            key(KEY_MOD, 0);
            EXPECT(ui.learn && !key_plays(KEY_MOD), "the 4th black key: MIDI LEARN on");
            key(KEY_EDIT, 1);
            key(KEY_EDIT, 0);
            sn = cur();
            fl = pages_n(sn) - 5u;               /* (FLTR 1) */
            ui.edit_pg = (uint8_t)fl;
            turn(EN_PRESET, 4);                  /* (E: FREQ) */
            EXPECT(ui.lsel == 4u && ui.ehalf == 1u, "PRESETS: the row (E), the half shown with it");
            btn(B_SAVE, 0);
            EXPECT(ui.listen, "SAVE: waiting for a CC");
            ui_draw();
            shot(dir, "learn_wait");
            midi_in_q[mi_w++ % MQ] = 0xBu | 0xB8u << 8 | 74u << 16 | 10u << 24;   /* (channel 9, CC 74) */
            ui_input();
            EXPECT(!ui.listen && learn_of(LK_SND, 2, PG_FLT1 * 8u + 4u) >= 0 && ui.learn_dirty, "CC 74 learned for FREQ");
            turn(EN_PRESET, 1);                  /* (F: RESO) */
            btn(B_SAVE, 0);
            midi_in_q[mi_w++ % MQ] = 0xBu | 0xB8u << 8 | 71u << 16 | 10u << 24;
            ui_input();
            ui_draw();
            shot(dir, "learn_edit");
            midi_in_q[mi_w++ % MQ] = 0xBu | 0xB8u << 8 | 74u << 16 | 127u << 24;
            ui_input();
            EXPECT(ui.snd[2][ui.eng[2]].flt[4] == 127, "CC 74 on channel 9 turns track 3's FREQ");
            btn(B_SEQ, 0);                       /* (CHORUS) */
            EXPECT(ui.page == PG_FXP && ui.lsel == 0u, "the effects' pages: the rows from A");
            btn(B_SAVE, 0);
            midi_in_q[mi_w++ % MQ] = 0xBu | 0xB0u << 8 | (uint32_t)MIDI_CC[PG_FLT1 * 8u] << 16 | 5u << 24;
            ui_input();                          /* (CC 50 on channel 1: the map's -- track 1's FLTR 1 A -- learned) */
            ui_draw();
            shot(dir, "learn_fx");
            kk = ui.snd[0][ui.eng[0]].flt[0];
            midi_in_q[mi_w++ % MQ] = 0xBu | 0xB0u << 8 | (uint32_t)MIDI_CC[PG_FLT1 * 8u] << 16 | 100u << 24;
            ui_input();
            EXPECT(fx_ch[0] == 100u && ui.snd[0][ui.eng[0]].flt[0] == kk,
                   "a learned CC sets its parameter (CHORUS DEPTH), not the map's");
            btn(B_SAVE, 1);
            EXPECT(learn_of(LK_FX, 2, 0) < 0 && ui.page == PG_FXP, "SHIFT + SAVE: the binding cleared");
            key(KEY_MOD, 1);
            key(KEY_MOD, 0);
            EXPECT(!ui.learn && !ui.learn_dirty, "the 4th black key again: MIDI LEARN off");
            memset(mlearn, 0, sizeof mlearn);
            setup_load();
            EXPECT(learn_of(LK_SND, 2, PG_FLT1 * 8u + 4u) >= 0 && learn_of(LK_SND, 2, PG_FLT1 * 8u + 5u) >= 0 &&
                   learn_of(LK_FX, 2, 0) < 0, "the bindings kept in the flash, read back");
            ui.page = PG_MAIN;
            memset(mlearn, 0, sizeof mlearn);
        }
        mcfg = D;
        setup_write();
        seq_run = 0;
        for (i = 0; i < 4u; i++)
            mix_block(out);
        for (k = 0; k < NSTEP; k++)
            seq_note[0][k] = 0;
    }
    shot(dir, "pat_main");
    printf("projects: %d failures, %u erases\n", fails, host_erases);
    return fails ? 1 : 0;
}

/* host phrases DIR [N]: N sessions (default 4) as the start makes them, a phrase on every track, 8 s played: a WAV
 * each and its peak and loudness */
static int cmd_phrases(const char *dir, int n)
{
    static int16_t pb[44100 * 8 * 2];
    int32_t out[2 * CTL];
    uint32_t k, t, i, fr = 44100u * 8u / CTL, fails_ = 0;
    for (k = 0; k < (uint32_t)n; k++) {
        double sq = 0;
        int32_t pk = 0;
        char path[256];
        tracks_init();
        fm1_ticks();
        host_ticks = k * 7919u;
        ui_init();
        for (t = 0; t < NTRACK; t++) {
            phrase_new(t);
            ui_publish();
        }
        seq_run = 1;
        double grs = 0;
        int32_t grmax = 0;
        if (getenv("CP")) {                       /* CP=thr,atk,rel,rat,mup,scf,scs,mix: the compressor by hand */
            int v[8];
            sscanf(getenv("CP"), "%d,%d,%d,%d,%d,%d,%d,%d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]);
            for (i = 0; i < 8u; i++)
                fx_cp[i] = (uint8_t)v[i];
            fx_cook_all();
        }
        for (i = 0; i < fr; i++) {
            uint32_t j;
            ui_publish();
            mix_block(out);
            grs += cp.gr / 65536.0;
            grmax = cp.gr > grmax ? cp.gr : grmax;
            for (j = 0; j < 2u * CTL; j++) {
                int32_t x = out[j] > 32767 ? 32767 : out[j] < -32768 ? -32768 : out[j];
                pb[i * 2u * CTL + j] = (int16_t)x;
                pk = x > pk ? x : -x > pk ? -x : pk;
                sq += (double)x * x;
            }
        }
        seq_run = 0;
        snprintf(path, sizeof path, "%s/phrase_%02u.wav", dir, k);
        wav_write(path, pb, fr * CTL);
        printf("phrase %2u  peak %5.1f dBFS  rms %5.1f dBFS ", k, 20 * log10(pk / 32768.0 + 1e-9),
               10 * log10(sq / (fr * CTL * 2.0) / (32768.0 * 32768.0) + 1e-12));
        printf(" GR mean %4.1f max %4.1f dB ", grs / fr, grmax / 65536.0);
        for (t = 0; t < NTRACK; t++)
            printf(" %s", RO_NAME[phrase_role(&ui.snd[t][ui.eng[t]])]);
        printf("\n");
        if (pk < 1000)
            fails_++;
    }
    return fails_ ? 1 : 0;
}

/* host od: OVERDRIVE's level, out over in (RMS), at 0, 10 .. 100, on a few signals (OD_MK, voice.c, from it; ODT=1, 2:
 * RAT's, FOLD's -- OD_MK_RAT, OD_MK_FOLD) */
static int cmd_od(void)
{
    static int32_t sig[3][44100];
    uint32_t i, k, a;
    uint32_t r = 1;
    for (i = 0; i < 44100u; i++) {
        r = r * 1664525u + 1013904223u;
        sig[0][i] = mulq15(sin_ph(i * 110u * 97391u), 16384);                                   /* a sine, -6 dB */
        sig[1][i] = mulq15(sin_ph(i * 55u * 97391u + (uint32_t)sin_ph(i * 165u * 97391u) * 40000u), 20000);   /* FM */
        sig[2][i] = ((int32_t)(r >> 16) - 32768) >> 2;                                          /* noise, -12 dB */
    }
    for (a = 0; a <= 100u; a += 10u) {
        printf("%3u", a);
        for (k = 0; k < 3u; k++) {
            vck_t c;
            vchain_t v;
            int32_t b[CTL];
            double si = 0, so = 0;
            memset(&c, 0, sizeof c);
            memset(&v, 0, sizeof v);
            od_cook(&c, (int32_t)a, getenv("ODT") ? atoi(getenv("ODT")) : 0);
            if (getenv("RAW"))
                c.od_mk = 4096;
            for (i = 0; i + CTL <= 44100u; i += CTL) {
                uint32_t j;
                for (j = 0; j < CTL; j++)
                    b[j] = sig[k][i + j];
                if (a)
                    vc_od(&v, &c, b);
                for (j = 0; j < CTL; j++) {
                    si += (double)sig[k][i + j] * sig[k][i + j];
                    so += (double)b[j] * b[j];
                }
            }
            printf("  %6.3f", sqrt(so / si));
        }
        printf("\n");
    }
    return 0;
}

/* host dsp: DRUM's AB NOISE (build 60) -- off, the sound as before it; on, A and B held noise: the hit changed, not
 * silent, not clipping; OVERDRIVE's RAT and FOLD (build 61) */
static int cmd_dsp(void)
{
    static const int R[3] = {8, 15, 22};          /* METAL HAT, CLANG, SCREECH */
    int fails = 0;
    uint32_t r;
    for (r = 0; r < 3u; r++) {
        sound_t s;
        stats_t a, b;
        uint32_t i, n = 44100u * 3u;
        double d = 0, e = 0;
        static int16_t keep[44100 * 3 * 2];
        gen_sound(&s, ENG_FMD, R[r], -1, 77u);
        s.syn[FD_ABN] = 0;
        a = render_one(&s, 0, 60, 0);
        memcpy(keep, abuf, sizeof keep);
        s.syn[FD_ABN] = 127;
        b = render_one(&s, 0, 60, 0);
        for (i = 0; i < 2u * n; i++) {
            d += ((double)abuf[i] - keep[i]) * ((double)abuf[i] - keep[i]);
            e += (double)keep[i] * keep[i];
        }
        printf("dsp: %-10s AB NOISE 0: peak %6.1f rms %6.1f; 127: peak %6.1f rms %6.1f, the difference %6.1f dB\n",
               RECIPES[R[r]].name, db(a.peak), db(a.rms1), db(b.peak), db(b.rms1), 10.0 * log10(d / (e + 1)));
        if (db(b.peak) < -45 || b.peak > 0.999 || 10.0 * log10(d / (e + 1)) < -20) {
            printf("FAIL dsp: %s with AB NOISE\n", RECIPES[R[r]].name);
            fails++;
        }
    }
    {   /* OD TYPE (build 61): RAT and FOLD at each amount, on a sine and on noise -- in range, the level brought back
         * (0.4 .. 2.5 of the input's), each unlike VALVE from 40 on (FOLD folds once
         * past full scale: below, little) */
        static int32_t sig[2][44100], ref[44100];
        uint32_t i, k, t, am;
        uint32_t rr = 1;
        for (i = 0; i < 44100u; i++) {
            rr = rr * 1664525u + 1013904223u;
            sig[0][i] = mulq15(sin_ph(i * 110u * 97391u), 16384);
            sig[1][i] = ((int32_t)(rr >> 16) - 32768) >> 2;
        }
        for (t = 1; t <= 2u; t++)
            for (am = 10; am <= 100u; am += 30u)
                for (k = 0; k < 2u; k++) {
                    vck_t c, c0;
                    vchain_t v, v0;
                    int32_t b[CTL], b0[CTL], pk = 0;
                    double si = 0, so = 0, sd = 0;
                    memset(&c, 0, sizeof c); memset(&v, 0, sizeof v);
                    memset(&c0, 0, sizeof c0); memset(&v0, 0, sizeof v0);
                    od_cook(&c, (int32_t)am, (int32_t)t);
                    od_cook(&c0, (int32_t)am, 0);
                    for (i = 0; i + CTL <= 44100u; i += CTL) {
                        uint32_t j;
                        for (j = 0; j < CTL; j++)
                            b[j] = b0[j] = sig[k][i + j];
                        vc_od(&v, &c, b);
                        vc_od(&v0, &c0, b0);
                        for (j = 0; j < CTL; j++) {
                            si += (double)sig[k][i + j] * sig[k][i + j];
                            so += (double)b[j] * b[j];
                            sd += ((double)b[j] - b0[j]) * ((double)b[j] - b0[j]);
                            pk = absi(b[j]) > pk ? absi(b[j]) : pk;
                            ref[i + j] = b[j];
                        }
                    }
                    if (am == 100u)
                        printf("dsp: OD TYPE %s at %3u on %s: level %.2f, peak %5d, off VALVE by %5.1f dB\n",
                               t == 1 ? "RAT " : "FOLD", am, k ? "noise" : "a sine", sqrt(so / si), pk,
                               10.0 * log10(sd / (so + 1)));
                    if (pk > 40000 || sqrt(so / si) < 0.4 || sqrt(so / si) > 2.5 ||
                        (am >= 40u && 10.0 * log10(sd / (so + 1)) < -20)) {
                        printf("FAIL dsp: OD TYPE %u at %u on signal %u (level %.2f, peak %d)\n", t, am, k, sqrt(so / si), pk);
                        fails++;
                    }
                }
    }
    printf("dsp: %d failures\n", fails);
    return fails ? 1 : 0;
}

/* host duck [LEVEL] [WAV]: a kick (DRIVEN KICK) on track 1 each beat, a reverb locked on its first step; a HIVE pad
 * (SWARM DRONE) held on track 2, two notes, its SC SOURCE T1 at SC LEVEL (0 .. 100, default 70): the pad's duck over a
 * beat (dB, each 25 ms), rendered to WAV */
static int cmd_duck(int lvl, const char *wav)
{
    static int16_t pb[44100 * 4 * 2];
    int32_t out[2 * CTL];
    uint32_t i, n = 0, fr = 44100u * 4u / CTL;
    tracks_init();
    ui_init();
    gen_sound(&ui.snd[0][ENG_FMD], ENG_FMD, 2, -1, 99u);
    ui.eng[0] = ENG_FMD;
    gen_sound(&ui.snd[1][ENG_SWM], ENG_SWM, 54, -1, 7u);
    ui.eng[1] = ENG_SWM;
    if (getenv("NOSEND"))
        ui.snd[1][ENG_SWM].fx[X_DEL] = ui.snd[1][ENG_SWM].fx[X_REV] = ui.snd[1][ENG_SWM].fx[X_CHR] = 0;
    for (i = 2; i < NTRACK; i++)
        ui.level[i] = 0;
    if (getenv("KICKMUTE"))
        ui.level[0] = 0;
    if (getenv("PADMUTE"))
        ui.level[1] = 0;                         /* (the kick unheard: the pad alone, the kick still its source) */
    for (i = 0; i < NTRACK; i++) {
        level_pan_apply(i);
        mark(i);
    }
    for (i = 0; i < 8u; i++) {
        ui_publish();
        mix_block(out);
    }
    memset((void *)seq_note, 0, sizeof seq_note);
    seq_note[0][0] = seq_note[0][4] = seq_note[0][8] = seq_note[0][12] = 36;
    if (getenv("NOCOMP")) {                      /* (the master compressor out: the effects' timing alone) */
        fx_cp[CP_MIX] = 0;
        fx_cook_all();
    }
    if (getenv("KICKREV"))
        (void)pl_set(0, 0, PL_REV, 110);
    sc_src[1] = 1;
    sc_lvl[1] = (uint8_t)pct_raw(lvl);
    ev_post(EV_ON, 1, 48, 127);
    ev_post(EV_ON, 1, 55, 127);
    seq_run = 1;
    for (i = 0; i < fr; i++) {
        uint32_t j;
        mix_block(out);
        for (j = 0; j < 2u * CTL; j++)
            pb[n++] = (int16_t)clampi(out[j], -32768, 32767);
        if (i >= 1378u && i < 1378u + 690u && (i - 1378u) % 34u == 0u)
            printf("%5.0f", 20.0 * log10(dk_gp[1] / 65536.0 + 1e-9));
    }
    printf("   (the pad's duck, dB, each 25 ms over a beat; SC LEVEL %d)\n", lvl);
    if (wav)
        wav_write(wav, pb, fr * CTL);
    return 0;
}

/* host prof: where a block's time goes -- the mix with no voice (the effects), then a SWARM DRONE voice, two, a kick;
 * and each effect alone */
static double prof_ns(void (*fn)(void), uint32_t n)
{
    struct timespec t0, t1;
    uint32_t i;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (i = 0; i < n; i++)
        fn();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / n;
}
static int32_t prof_out[2 * CTL], prof_a[CTL], prof_b[CTL], prof_c[CTL], prof_d[CTL], prof_e[CTL];
static void prof_mix(void) { mix_block(prof_out); }
static void prof_rv(void) { rv_block(prof_a, prof_b, prof_c); }
static void prof_dl(void) { dl_block(prof_a, prof_b, prof_c, prof_d); }
static void prof_ch(void) { ch_block(prof_a, prof_b, prof_c, prof_d, prof_e); }
static void prof_cp(void) { cp_block(prof_b, prof_c); }
static int cmd_prof(void)
{
    uint32_t i;
    double base;
    tracks_init();
    ui_init();
    host_ticks = 0;
    gen_sound(&ui.snd[0][ENG_FMD], ENG_FMD, 2, -1, 99u);    /* DRIVEN KICK */
    ui.eng[0] = ENG_FMD;
    gen_sound(&ui.snd[1][ENG_SWM], ENG_SWM, 54, -1, 7u);    /* SWARM DRONE */
    ui.eng[1] = ENG_SWM;
    for (i = 0; i < NTRACK; i++)
        mark(i);
    for (i = 0; i < 8u; i++) {
        ui_publish();
        mix_block(prof_out);
    }
    for (i = 0; i < CTL; i++)
        prof_a[i] = prof_b[i] = prof_c[i] = (int32_t)((i * 7919u) % 20000u) - 10000;
    base = prof_ns(prof_mix, 20000);
    printf("the mix, no voice:          %7.0f ns\n", base);
    printf("  reverb %6.0f  delay %6.0f  chorus %6.0f  comp %6.0f ns\n", prof_ns(prof_rv, 20000), prof_ns(prof_dl, 20000),
           prof_ns(prof_ch, 20000), prof_ns(prof_cp, 20000));
    ev_post(EV_ON, 1, 48, 127);
    for (i = 0; i < 200u; i++)
        mix_block(prof_out);
    printf("+ one SWARM DRONE voice:    %7.0f ns\n", prof_ns(prof_mix, 20000));
    ev_post(EV_ON, 1, 55, 127);
    for (i = 0; i < 200u; i++)
        mix_block(prof_out);
    printf("+ two:                      %7.0f ns\n", prof_ns(prof_mix, 20000));
    ev_post(EV_ON, 0, 36, 127);
    for (i = 0; i < 20u; i++)
        mix_block(prof_out);
    printf("+ a DRIVEN KICK:            %7.0f ns\n", prof_ns(prof_mix, 2000));
    {   /* one voice in parts: the machine, then the chain's stages (each recipe of interest) */
        static const int R[4] = {54, 53, 2, 43};       /* SWARM DRONE, SWARM PAD, DRIVEN KICK, WAVE PAD */
        uint32_t r;
        for (r = 0; r < 4u; r++) {
            sound_t sn;
            ck_t c;
            vmem_t m;
            int32_t b[CTL];
            struct timespec t0, t1;
            uint32_t n, N = 20000;
            double tm, tc;
            gen_sound(&sn, RECIPES[R[r]].mach, R[r], -1, 5u);
            ck_cook(&c, &sn);
            memset(&m, 0, sizeof m);
            switch (c.mach) {
            case ENG_FMT: ft_on(&m.m.ft, &c.m.ft, 1); break;
            case ENG_FMD: fd_on(&m.m.fd, &c.m.fd, 1, 1); break;
            case ENG_WAV: wt_on(&m.m.wt, &c.m.wt, 1, 1); break;
            default: sw_on(&m.m.sw, 1, 1); break;
            }
            vc_on(&m.ch, &c.ch, 1);
            clock_gettime(CLOCK_MONOTONIC, &t0);
            for (n = 0; n < N; n++) {
                int32_t note16 = 48 << 16;
                switch (c.mach) {
                case ENG_FMT: ft_render(&m.m.ft, &c.m.ft, note_inc(note16), note16, b); break;
                case ENG_FMD: fd_render(&m.m.fd, &c.m.fd, note16, b); break;
                case ENG_WAV: wt_render(&m.m.wt, &c.m.wt, note16, b); break;
                default: sw_render(&m.m.sw, &c.m.sw, note16, b); break;
                }
            }
            clock_gettime(CLOCK_MONOTONIC, &t1);
            tm = ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / N;
            clock_gettime(CLOCK_MONOTONIC, &t0);
            for (n = 0; n < N; n++) {
                uint32_t q;
                for (q = 0; q < CTL; q++)
                    b[q] = (int32_t)(q * 977u % 16000u) - 8000;
                vc_block(&m.ch, &c.ch, b, 48 << 16, 127);
            }
            clock_gettime(CLOCK_MONOTONIC, &t1);
            tc = ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / N;
            printf("%-12s %-9s machine %5.0f ns, chain %5.0f ns (filter %s, drive %d, bits %d, srr %d)\n", RECIPES[R[r]].name,
                   ENG_NAME[c.mach], tm, tc, FLT_NAME[c.ch.fmach], sn.fx[X_OD], sn.fx[X_BR], sn.fx[X_SRR]);
            {   /* the chain's stages, one by one */
                double tf, to, tb;
                clock_gettime(CLOCK_MONOTONIC, &t0);
                for (n = 0; n < N; n++)
                    vc_filter(&m.ch, &c.ch, b, 48 << 16, 1 << 23);
                clock_gettime(CLOCK_MONOTONIC, &t1);
                tf = ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / N;
                to = 0;
                if (c.ch.od_g) {
                    clock_gettime(CLOCK_MONOTONIC, &t0);
                    for (n = 0; n < N; n++)
                        vc_od(&m.ch, &c.ch, b);
                    clock_gettime(CLOCK_MONOTONIC, &t1);
                    to = ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / N;
                }
                tb = 0;
                if (c.ch.br_bits) {
                    clock_gettime(CLOCK_MONOTONIC, &t0);
                    for (n = 0; n < N; n++)
                        vc_br(&c.ch, b);
                    clock_gettime(CLOCK_MONOTONIC, &t1);
                    tb = ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / N;
                }
                printf("               filter %5.0f, drive %5.0f, bits %5.0f ns\n", tf, to, tb);
            }
        }
    }
    return 0;
}

/* the LFOs (build 50): the rate, the waves' depth, the modes, FADE, an LFO on another's DEPTH, the cooked copy;
 * the pages after the effects, per track */
#define LEXPECT(c, what) do { if (!(c)) { printf("FAIL lfo: %s\n", what); fails++; } } while (0)
static void lfo_use(const sound_t *s)            /* (s on track 0, as the audio takes a published sound) */
{
    ck_cook(&ck_buf[0][0], s);
    ck_buf[0][1] = ck_buf[0][0];
    ck_pub[0] = ck_use[0] = 0;
    lfo_flips[0]++;
}
static void lfo_one(sound_t *s, uint32_t dest, uint32_t wave, uint32_t mode, int32_t spd, uint32_t mult, int32_t dep)
{
    lfos_off(s);
    s->lfo[0].dest = (uint8_t)dest;
    s->lfo[0].wave = (uint8_t)wave;
    s->lfo[0].mode = (uint8_t)mode;
    s->lfo[0].spd = (uint8_t)(64 + spd);
    s->lfo[0].mult = (uint8_t)mult;
    s->lfo[0].dep = (uint8_t)(64 + dep);
    lfo_use(s);
}
static int cmd_lfo(const char *dir)
{
    sound_t s;
    int32_t i, prev, up[4], nup = 0, mx = -99999, mn = 99999, a, b;
    const uint32_t FREQ = 1u + PG_FLT1 * 8u + F_FREQ;
    uint32_t per;
    tracks_init();
    fx_bpm10 = 1200;
    gen_sound(&s, ENG_SWM, -1, -1, 77);
    s.flt[F_FREQ] = 64;
    /* SPEED +32, MULT x4: a cycle in 2048 / 128 = 16 16ths, a bar: 2 s at 120 BPM, 2756.25 blocks, 689 steps */
    lfo_one(&s, FREQ, LW_TRI, LM_FREE, 32, 2, 32);
    per = 44100u * 2u / CTL / LFO_EVERY;         /* (a step: LFO_EVERY blocks) */
    prev = 0;
    for (i = 0; i < (int32_t)per * 3 + 10; i++) {
        lfo_step(0);
        if (i > 0 && prev < 0 && lfo_mod[0][0] >= 0 && nup < 4)
            up[nup++] = i;
        prev = lfo_mod[0][0];
        mx = lfo_mod[0][0] > mx ? lfo_mod[0][0] : mx;
        mn = lfo_mod[0][0] < mn ? lfo_mod[0][0] : mn;
    }
    LEXPECT(nup >= 3 && absi(up[2] - up[1] - (int32_t)per) <= 2, "SPEED +32, MULT x4 at 120 BPM: a cycle a bar");
    LEXPECT(absi(mx - 16383) < 300 && absi(mn + 16384) < 300, "DEPTH +32: half the destination's range each way");
    if (nup >= 3)
        printf("lfo: a cycle %d steps (%u a bar), share %d .. %d\n", up[2] - up[1], per, mn, mx);
    fx_bpm10 = 600;                              /* (60 BPM: twice as long; MULT .4 at 120 BPM whatever the tempo) */
    nup = 0;
    for (i = 0; i < (int32_t)per * 8 && nup < 3; i++) {
        lfo_step(0);
        if (i > 0 && prev < 0 && lfo_mod[0][0] >= 0)
            up[nup++] = i;
        prev = lfo_mod[0][0];
    }
    LEXPECT(nup == 3 && absi(up[2] - up[1] - 2 * (int32_t)per) <= 3, "the tempo halved: the cycle twice as long");
    lfo_one(&s, FREQ, LW_TRI, LM_FREE, 32, 12u + 2u, 32);
    nup = 0;
    for (i = 0; i < (int32_t)per * 4 && nup < 3; i++) {
        lfo_step(0);
        if (i > 0 && prev < 0 && lfo_mod[0][0] >= 0)
            up[nup++] = i;
        prev = lfo_mod[0][0];
    }
    LEXPECT(nup == 3 && absi(up[2] - up[1] - (int32_t)per) <= 2, "MULT .4: at 120 BPM, not the tempo's");
    fx_bpm10 = 1200;

    /* the cooked copy: the destination moved by the share, the copy the one the voices play */
    lfo_one(&s, FREQ, LW_SQR, LM_FREE, 8, 0, 20);
    for (i = 0; i < 2 * (int32_t)LFO_EVERY; i++)
        lfo_block();
    {
        sound_t c = ck_buf[0][0].snd;
        lfo_apply(&c, lfo_mod[0]);
        LEXPECT(lfo_live(0) && ck_lfo[0].snd.flt[F_FREQ] == c.flt[F_FREQ] && c.flt[F_FREQ] == 64 + 20 * 127 / 64,
                "SQR, DEPTH +20 on FREQ 64: the copy cooked at FREQ 103");
    }
    s.lfo[0].dep = 64;                           /* (DEPTH 0: off, the track's own sound) */
    lfo_use(&s);
    for (i = 0; i < (int32_t)LFO_EVERY; i++)
        lfo_block();
    LEXPECT(!lfo_live(0), "DEPTH 0: the LFO off, the track's own cooked sound played");

    /* TRIG: from START at each note; FREE: on as it was */
    lfo_one(&s, FREQ, LW_SINE, LM_TRIG, 20, 3, 63);
    for (i = 0; i < 700; i++)
        lfo_step(0);
    a = lfo_mod[0][0];
    lfo_trig[0] = 1;
    lfo_step(0);
    LEXPECT(absi(lfo_mod[0][0]) < 2000 && absi(a) > 2000, "TRIG: a note starts the wave again (SINE from 0)");
    s.lfo[0].mode = LM_FREE;
    lfo_use(&s);
    for (i = 0; i < 700; i++)
        lfo_step(0);
    a = lfo_mod[0][0];
    lfo_trig[0] = 1;
    lfo_step(0);
    LEXPECT(absi(lfo_mod[0][0] - a) < 2000 && absi(a) > 2000, "FREE: a note does not");

    /* HOLD: the wave as it was at the note, held till the next */
    lfo_one(&s, FREQ, LW_TRI, LM_HOLD, 40, 3, 63);
    for (i = 0; i < 333; i++)
        lfo_step(0);
    lfo_trig[0] = 1;
    lfo_step(0);
    a = lfo_mod[0][0];
    for (i = 0, b = 1; i < 900; i++) {
        lfo_step(0);
        b &= lfo_mod[0][0] == a;
    }
    lfo_trig[0] = 1;
    lfo_step(0);
    LEXPECT(b && a != 0 && lfo_mod[0][0] != a, "HOLD: the value at a note, held until the next note");

    /* ONE: a cycle and stop; HALF: half */
    lfo_one(&s, FREQ, LW_SAW, LM_ONE, 32, 2, 63);
    lfo_trig[0] = 1;
    for (i = 0; i < (int32_t)per * 3 / 2; i++)
        lfo_step(0);
    a = lfo_mod[0][0];
    for (i = 0, b = 1; i < (int32_t)per; i++) {
        lfo_step(0);
        b &= lfo_mod[0][0] == a;
    }
    LEXPECT(b && a > 30000 * 63 / 64, "ONE: a cycle from a note, then still (SAW: at its start, the top)");
    lfo_one(&s, FREQ, LW_TRI, LM_HALF, 32, 2, 63);
    lfo_trig[0] = 1;
    for (i = 0; i < (int32_t)per; i++)
        lfo_step(0);
    a = lfo_mod[0][0];
    lfo_step(0);
    LEXPECT(lfo_mod[0][0] == a && absi(a) < 1500, "HALF: half a cycle, then still (TRI: back at 0)");

    /* FADE -16: in over 16 16ths (a bar) from each note; FADE +16: out */
    lfo_one(&s, FREQ, LW_SQR, LM_FREE, 1, 0, 63);
    s.lfo[0].fade = 64 - 16;
    lfo_use(&s);
    lfo_trig[0] = 1;
    lfo_step(0);
    a = absi(lfo_mod[0][0]);
    for (i = 0; i < (int32_t)per / 2; i++)
        lfo_step(0);
    b = absi(lfo_mod[0][0]);
    for (i = 0; i < (int32_t)per; i++)
        lfo_step(0);
    LEXPECT(a < 100 && absi(b - 16000) < 800 && absi(lfo_mod[0][0]) > 32000 * 63 / 64 - 300,
            "FADE -16: none at a note, half at half a bar, all after a bar");
    s.lfo[0].fade = 64 + 16;
    lfo_use(&s);
    lfo_trig[0] = 1;
    lfo_step(0);
    a = absi(lfo_mod[0][0]);
    for (i = 0; i < (int32_t)per + 2; i++)
        lfo_step(0);
    LEXPECT(a > 31000 && lfo_mod[0][0] == 0, "FADE +16: all at a note, none after a bar");

    /* RND: a new value each cycle, within the depth; START slews it */
    lfo_one(&s, FREQ, LW_RND, LM_FREE, 64, 9, 32);
    {
        int32_t ch = 0, last = 0, big = 0;
        for (i = 0; i < 2000; i++) {
            lfo_step(0);
            ch += lfo_mod[0][0] != last;
            big |= absi(lfo_mod[0][0]) > 16384;
            last = lfo_mod[0][0];
        }
        LEXPECT(ch > 20 && !big, "RND: new values, within the depth (and on again in FREE after HALF stopped it)");
    }

    /* LFO 2 on LFO 1's DEPTH: LFO 1 at DEPTH 0 moves as LFO 2 swings it */
    lfo_one(&s, FREQ, LW_SQR, LM_FREE, 8, 0, 0);
    s.lfo[1].dest = LFO_D_LFO + 1u;
    s.lfo[1].wave = LW_SQR;
    s.lfo[1].spd = 64 + 2;
    s.lfo[1].mult = 0;
    s.lfo[1].dep = 64 + 32;
    lfo_use(&s);
    for (i = 0, mx = 0; i < 4000; i++) {
        lfo_step(0);
        mx = absi(lfo_mod[0][0]) > mx ? absi(lfo_mod[0][0]) : mx;
    }
    LEXPECT(lfo_any[0] && absi(mx - 16383) < 600 && lfo_mod[0][1] == 0, "LFO 2 on LFO 1's DEPTH: LFO 1 moves by it");
    LEXPECT(!lfo_dest_ok(&s, 0, LFO_D_LFO) && !lfo_dest_ok(&s, 1, LFO_D_LFO + 2u) && lfo_dest_ok(&s, 2, LFO_D_LFO + 3u),
            "an LFO reaches only the earlier LFOs");

    /* the pages: after REVERB, LFO 1 .. 3 of the chosen track; DEST steps over what the machine lacks */
    tracks_init();
    ui_init();
    fm1_ms = 5000;
    ui.sel = 2;
    for (i = 0; i < 5; i++)
        btn(B_SEQ, 0);
    LEXPECT(ui.page == PG_FXP && ui.fx_pg == FXP_L1, "SEQ: CHORUS, COMP, DELAY, REVERB, then LFO 1");
    shot(dir, "lfo1");
    lfos_off(cur());
    fx_turn(LF_DEST, 1);
    LEXPECT(cur()->lfo[0].dest == 1u && ui.snd[0][ui.eng[0]].lfo[0].dest == 0u && ui.dirty[2],
            "DEST on LFO 1: the chosen track's sound's, the others' not");
    fx_turn(LF_DEST, 200);
    LEXPECT(cur()->lfo[0].dest == LFO_D_LFO - 1u || lfo_dest_ok(cur(), 0, cur()->lfo[0].dest),
            "DEST at its end: a parameter LFO 1 may take");
    fx_turn(LF_DEST, -200);
    fx_turn(LF_DEST, (int32_t)(FREQ - 1u));
    {
        char t[24];
        lfo_dest_text(cur(), cur()->lfo[0].dest, t);
        printf("lfo: DEST %u reads %s\n", cur()->lfo[0].dest, t);
    }
    fx_turn(LF_DEP, 20);
    fx_turn(LF_MULT, 30);
    fx_turn(LF_WAVE, 3);
    LEXPECT(cur()->lfo[0].dep == 84u && cur()->lfo[0].mult == NLMULT - 1u && cur()->lfo[0].wave == LW_SAW,
            "DEPTH, MULT (at its end), WAVE turned");
    ui.half = 0;
    shot(dir, "lfo1_set");
    btn(B_SEQ, 0);
    btn(B_SEQ, 0);
    LEXPECT(ui.fx_pg == FXP_L3, "SEQ: LFO 2, LFO 3");
    btn(B_SEQ, 0);
    LEXPECT(ui.page == PG_MAIN, "SEQ after LFO 3: back");
    {   /* SHIFT + SEQ (build 58): the chosen track's three LFOs thrown anew, the others' kept; 400 throws, each
         * machine: two or three on, each to a parameter that moves smoothly or an earlier LFO, none twice */
        uint32_t i, bad = 0, dup = 0, onl = 0, ok3 = 0, lfo2 = 0, seen = 0;
        sound_t keep = ui.snd[1][ui.eng[1]];
        ui.page = PG_MAIN;
        ui.sel = 0;
        for (i = 0; i < 400u; i++) {
            sound_t *sn = cur();
            uint32_t n, on = 0, j;
            if (i % 100u == 0u) {
                ui.eng[0] = (uint8_t)(i / 100u);
                gen_sound(cur(), ui.eng[0], -1, -1, 7u + i);
                sn = cur();
            }
            btn(B_SEQ, 1);
            for (n = 0; n < NLFO; n++) {
                const lfo_t *l = &sn->lfo[n];
                if (!lfo_on(l)) {
                    bad += l->dest != 0;
                    continue;
                }
                on++;
                bad += !lfo_dest_ok(sn, n, l->dest) || (l->dest < LFO_D_LFO && !lfo_rnd_dest_ok(sn, l->dest)) ||
                       l->spd == 64 || l->wave >= NLW || l->mode >= NLM || l->mult >= NLMULT;
                lfo2 += l->dest >= LFO_D_LFO;
                for (j = 0; j < n; j++)
                    dup += sn->lfo[j].dest == l->dest;
                seen |= 1u << l->wave;
            }
            onl += on < 2u || on > 3u;
            ok3 += on == 3u;
        }
        printf("lfo: SHIFT + SEQ 400 throws: %u with 3 on, %u on another LFO, %u twice to one, %u bad\n", ok3, lfo2, dup,
               bad);
        LEXPECT(!bad && !onl && ok3 > 100u && ok3 < 300u && lfo2 > 20u && dup < 20u && seen == (1u << NLW) - 1u,
                "SHIFT + SEQ: two or three LFOs on, each to a smooth parameter or an earlier LFO, every wave");
        LEXPECT(!memcmp(&keep, &ui.snd[1][ui.eng[1]], sizeof keep) && ui.dirty[0] && !strcmp(ui.msg, "LFO RANDOM"),
                "SHIFT + SEQ: the chosen track's alone, published, LFO RANDOM shown");
        ui.fx_pg = FXP_L1;
        ui.page = PG_FXP;
        ui_draw();
        shot(dir, "lfo_random");
    }
    printf("lfo: %d failures\n", fails);
    return fails != 0;
}

/* transposition (build 51): OCT- / OCT+ the keys' octave in the track's view, the track's steps in the step view,
 * SHIFT every track's; in steps of the scale; kept with the pattern */
#define LPN(t, l, i) lp_note(t, l, loops[t][l].ev[i].note)   /* (a loop's note as it plays: build 68) */
#define TEXPECT(c, what) do { if (!(c)) { printf("FAIL trn: %s\n", what); fails++; } } while (0)
static int cmd_trn(const char *dir)
{
    memset(host_flash, 0xFF, sizeof host_flash);
    prj_scan(1);
    tracks_init();
    ui_init();
    fm1_ms = 5000;
    memset((void *)seq_note, 0, sizeof seq_note);
    ui.scale = 1;                                /* MAJOR, from C */
    ui.root = 0;
    seq_note[0][0] = 60; seq_note[0][1] = 64; seq_note[0][2] = 71;
    seq_note[1][0] = 48;
    ui.sel = 0;
    ui.sv = 0;
    ui.oct[0] = 0;
    btn(B_OCTUP, 0);
    TEXPECT(ui.oct[0] == 1 && seq_note[0][0] == 60 && !ui.trn[0], "the track's view: OCT+ the keys' octave, the steps kept");
    btn(B_OCTDN, 0);
    ui.sv = 1;
    btn(B_OCTUP, 0);
    TEXPECT(seq_note[0][0] == 62 && seq_note[0][1] == 65 && seq_note[0][2] == 72 && ui.trn[0] == 1 && !ui.oct[0] &&
            seq_note[1][0] == 48 && !ui.trn[1], "the step view: OCT+ the track's steps a step of the scale up (C E B: D F C)");
    ui_draw();
    shot(dir, "trn_step");
    btn(B_OCTDN, 0);
    btn(B_OCTDN, 0);
    TEXPECT(seq_note[0][0] == 59 && seq_note[0][1] == 62 && seq_note[0][2] == 69 && ui.trn[0] == -1,
            "OCT- twice: a step of the scale below where they were (B D A)");
    ui.sv = 0;
    btn(B_OCTUP, 1);
    TEXPECT(seq_note[0][0] == 60 && seq_note[0][2] == 71 && !ui.trn[0] && seq_note[1][0] == 50 && ui.trn[1] == 1 &&
            !ui.oct[0], "SHIFT + OCT+: every track's steps up a step of the scale, the octave kept");
    {   /* the track's loops with its steps (build 54): a loop on track 1 (C, A), another not on */
        loop_t *p = &loops[0][0];
        memset(loops, 0, sizeof loops);
        p->n = 2; p->len = 16; p->ev[0].note = 60; p->ev[1].note = 69; p->ev[0].step = 0; p->ev[1].step = 8;
        p->on = 1;
        lp_ref[0][0] = lp_trn[0] = ui.trn[0];    /* (recorded now: build 68) */
        loops[0][1].n = 1; loops[0][1].ev[0].note = 60;   /* (not on: an empty slot's leftovers) */
        ui.sv = 1;
        ui.sel = 0;
        btn(B_OCTUP, 0);
        TEXPECT(LPN(0, 0, 0) == 62 && LPN(0, 0, 1) == 71 && loops[0][1].ev[0].note == 60 && seq_note[0][0] == 62 &&
                ui.trn[0] == 1, "the step view: OCT+ the track's loop up a step of the scale too, with its steps");
        btn(B_OCTDN, 1);
        TEXPECT(LPN(0, 0, 0) == 60 && LPN(0, 0, 1) == 69 && seq_note[0][0] == 60 && !ui.trn[0] && ui.trn[1] == 0,
                "SHIFT + OCT-: every track's steps and loops down");
        btn(B_OCTUP, 1);                         /* (back as the tests below had it: track 1 at 0, track 2 at +1) */
        btn(B_OCTDN, 0);
        memset(loops, 0, sizeof loops);
    }
    ui.scale = 0;                                /* CHROMATIC: semitones */
    ui.sv = 1;
    ui.sel = 1;
    btn(B_OCTUP, 0);
    TEXPECT(seq_note[1][0] == 51 && ui.trn[1] == 2, "CHROMATIC: a semitone");
    ui.scale = 1;
    btn(B_OCTDN, 0);
    TEXPECT(seq_note[1][0] == 50 && ui.trn[1] == 1, "a note out of the scale: to the scale's next");

    /* the pattern keeps it: pattern 1 copied to 2, 2 transposed, each as it was left */
    pat_take(&pat_store[0]);
    pat_store[1] = pat_store[0];
    pat_give(1, 1);
    TEXPECT(ui.trn[1] == 1 && seq_note[1][0] == 50, "a copy of the pattern: its transposition with it");
    btn(B_OCTUP, 1);
    TEXPECT(ui.trn[0] == 1 && ui.trn[1] == 2 && seq_note[0][0] == 62 && seq_note[1][0] == 52, "pattern 2 transposed");
    pat_take(&pat_store[1]);
    pat_give(0, 1);
    TEXPECT(!ui.trn[0] && ui.trn[1] == 1 && seq_note[0][0] == 60 && seq_note[1][0] == 50, "pattern 1 as it was");
    pat_give(1, 1);
    TEXPECT(ui.trn[0] == 1 && ui.trn[1] == 2 && seq_note[0][0] == 62 && seq_note[1][0] == 52, "pattern 2 as transposed");
    {   /* (build 67) the loops follow the pattern's transposition: moved as it comes in, back as the other does */
        loop_t *lq = &loops[1][0];
        memset(lq, 0, sizeof *lq);
        lq->len = 16;
        lq->n = 2;
        lq->ev[0].note = 52;                     /* (as pattern 2 has it: TRN +2 on track 2) */
        lq->ev[1].note = 55;
        lq->ev[1].step = 8;
        lq->on = 1;
        lp_ref[1][0] = lp_trn[1] = ui.trn[1];    /* (recorded now) */
        pat_give(0, 1);
        TEXPECT(ui.trn[1] == 1 && LPN(1, 0, 0) == 50 && LPN(1, 0, 1) == 53,
                "pattern 1 in: track 2's loop moved as its transposition, one step down");
        pat_give(1, 1);
        TEXPECT(ui.trn[1] == 2 && LPN(1, 0, 0) == 52 && LPN(1, 0, 1) == 55, "pattern 2 again: the loop as it was");
        pat_give(1, 0);
        TEXPECT(LPN(1, 0, 0) == 52, "(a session loaded: the loops as saved, not moved)");
        {   /* playing: as the pattern is taken in */
            int32_t out[2 * CTL];
            uint32_t j;
            seq_run = 1;
            pat_switch(0);
            for (j = 0; j < 4000u && seq_pend; j++) {
                mix_block(out);
                pat_poll();
            }
            TEXPECT(!seq_pend && pat_cur == 0u && ui.trn[1] == 1 && LPN(1, 0, 0) == 50,
                    "playing: pattern 1 taken in, the loop moved with it");
            seq_run = 0;
            pat_give(1, 1);
        }
        memset(lq, 0, sizeof *lq);
    }
    {   /* a pattern saved before build 51: no transposition */
        pat_t p = pat_store[1];
        memset(p.seq + SEQ_TRN, 0, NTRACK);
        pat_decode(&p, &sb_tmp);
        TEXPECT(!sb_tmp.trn[0] && !sb_tmp.trn[1] && sb_tmp.note[1][0] == 52, "an older pattern: TRN 0, its notes");
    }
    flash("SAVED", "SESSION 16", C_OK);          /* (the second line in the size that fits: build 52) */
    ui_draw();
    shot(dir, "saved_session16");
    {   /* the battery, top right (build 56): each level, and on a USB host's power; redrawn as it changes */
        static const int32_t RAW[5] = {600, 570, 540, 500, 600};
        static const int32_t KEY[5] = {3, 2, 1, 0, 4};
        uint32_t i;
        char nm[32];
        ui.oct[ui.sel] = 1;
        ui.sv = 0;
        ui.page = PG_MAIN;
        ui.flash_on = 0;
        for (i = 0; i < 5u; i++) {
            batt_raw = RAW[i];
            usb.config = i == 4u;
            ui_draw();
            TEXPECT(ui.shown_batt == KEY[i], "the battery shown as its level (USB: the bolt)");
            snprintf(nm, sizeof nm, "batt_%u", i);
            shot(dir, nm);
        }
        batt_raw = 0;
        usb.config = 0;
        ui_draw();
        TEXPECT(ui.shown_batt == -1, "the battery not read yet: none shown");
    }
    {   /* the knobs' dial (build 59): its name and the track's number within the screen -- REVERB SEND 6, CHORUS
         * SEND 6 in the size that fits; nothing at its edges */
        static const uint32_t K[4] = {EN_K1, EN_K2, EN_K3, EN_K4};
        uint32_t j, sh, y, x, edge = 0;
        ui.page = PG_MAIN;
        ui.sv = 0;
        ui.sel = 5;
        for (sh = 0; sh < 2u; sh++)
            for (j = 0; j < 4u; j++) {
                char nm[32];
                if (sh)
                    fm1_in.notes |= 1u << KEY_SHIFT;
                host_enc[PANEL_ENC[K[j]]] = 1;
                ui_input();
                fm1_in.notes &= ~(1u << KEY_SHIFT);
                ui.force = 1;
                ui_draw();
                for (y = 0; y < 40u; y++)
                    for (x = 0; x < 240u; x += 239u)
                        edge += fb[y * 240u + x] != C_BLACK;
                snprintf(nm, sizeof nm, "dial_k%u%s", j + 1u, sh ? "_shift" : "");
                shot(dir, nm);
                fm1_ms += 2000;
            }
        TEXPECT(!edge, "the knobs' dial: its name and the track within the screen (track 6, KNOB 1 .. 4, SHIFT too)");
    }
    printf("trn: %d failures\n", fails);
    return fails != 0;
}

/* the arpeggiator (build 52): the 5th black key, its page, its modes on the tempo's grid, LATCH, recorded into the
 * steps and into a loop, kept with the session */
#define AEXPECT(c, what) do { if (!(c)) { printf("FAIL arp: %s\n", what); fails++; } } while (0)
static uint8_t arp_got[64];
static uint32_t arp_ngot, arp_gotb[64];
static void arp_run(uint32_t blocks)             /* (the audio and, every 8 blocks, the main loop) */
{
    int32_t b[2u * CTL];
    uint32_t k, c0 = arps[0].c;
    for (k = 0; k < blocks; k++) {
        mix_block(b);
        if (arps[0].c != c0 && arps[0].snd && arp_ngot < 64u) {
            arp_gotb[arp_ngot] = k;
            arp_got[arp_ngot++] = arps[0].snd;
        }
        c0 = arps[0].c;
        fm1_ms += (k % 4u == 0u);
        if (k % 8u == 7u)
            ui_input();
    }
}
static int arp_seq(const uint8_t *want, uint32_t n)
{
    uint32_t i;
    if (arp_ngot < n)
        return 0;
    for (i = 0; i < n; i++)
        if (arp_got[i] != want[i])
            return 0;
    return 1;
}
static void arp_print(const char *w)
{
    uint32_t i;
    printf("arp: %s:", w);
    for (i = 0; i < arp_ngot && i < 16u; i++)
        printf(" %u", arp_got[i]);
    printf("\n");
}
static int cmd_arp(const char *dir)
{
    const uint32_t B16 = 44100u * 60u / 120u / 4u / CTL;   /* (a 16th at 120 BPM: 172 blocks) */
    memset(host_flash, 0xFF, sizeof host_flash);
    prj_scan(1);
    tracks_init();
    ui_init();
    fm1_ms = 5000;
    ui.scale = 0;
    ui.root = 5;                                 /* (CHROMATIC: key k plays 53 + k) */
    ui.oct[0] = 0;
    ui.sel = 0;
    ui.tempo = 120;
    tempo_apply();
    tap(KEY_ARP);
    AEXPECT(arp_on[0] && !arp_on[1], "the 5th black key: the track's arpeggiator on");
    ui_draw();
    shot(dir, "arp_main");
    key(0, 1); key(4, 1); key(7, 1);             /* F A C, held */
    arp_ngot = 0;
    arp_run(B16 * 7u + 10u);
    {
        static const uint8_t UP[7] = {53, 57, 60, 53, 57, 60, 53};
        arp_print("UP");
        AEXPECT(arp_seq(UP, 7) && arp_gotb[0] < 3u && absi((int32_t)(arp_gotb[3] - arp_gotb[0]) - 3 * (int32_t)B16) <= 2,
                "UP, 1/16: the notes held in turn, the first at once, a 16th apart");
    }
    arp_p[0][AR_MODE] = AM_UPDN;
    arp_p[0][AR_RNG] = 1;                        /* (2 octaves) */
    arps[0].c = 0;
    arp_ngot = 0;
    arp_run(B16 * 10u + 4u);
    {
        static const uint8_t UD[10] = {53, 57, 60, 65, 69, 72, 69, 65, 60, 57};
        arp_print("UP DOWN, 2 OCT");
        AEXPECT(arp_seq(UD, 10), "UP DOWN over two octaves: the ends once");
    }
    arp_p[0][AR_MODE] = AM_THUMB;
    arp_p[0][AR_RNG] = 0;
    arps[0].c = 0;
    arp_ngot = 0;
    arp_run(B16 * 4u + 4u);
    {
        static const uint8_t TH[4] = {53, 57, 53, 60};
        arp_print("THUMB");
        AEXPECT(arp_seq(TH, 4), "THUMB: the lowest between the others");
    }
    arp_p[0][AR_MODE] = AM_UP;
    arp_p[0][AR_BASS] = 3;                       /* (every 4th: the lowest an octave down) */
    arps[0].c = 0;
    arp_ngot = 0;
    arp_run(B16 * 5u + 4u);
    {
        static const uint8_t BS[5] = {41, 57, 60, 53, 41};
        arp_print("BASS EVERY 4");
        AEXPECT(arp_seq(BS, 5), "BASS EVERY 4: the lowest an octave down every 4th note");
    }
    arp_p[0][AR_BASS] = 0;
    key(0, 0); key(4, 0); key(7, 0);
    arp_ngot = 0;
    arp_run(B16 * 3u);
    AEXPECT(!arps[0].n && !arps[0].snd && !arp_ngot, "the keys let go: it stops");
    arp_p[0][AR_LATCH] = 1;                      /* LATCH: kept with the keys up; a new chord in its place */
    key(2, 1); key(2, 0);
    arp_run(B16 * 2u);
    AEXPECT(arps[0].n == 1u && arps[0].note[0] == 55, "LATCH: the note kept, the key up");
    key(9, 1); key(11, 1); key(9, 0); key(11, 0);
    arp_run(8);
    AEXPECT(arps[0].n == 2u && arps[0].note[0] == 62 && arps[0].note[1] == 64, "LATCH: a new chord in place of the last");
    ui.page = PG_FXP;                            /* LATCH turned off on the page: the notes let go */
    ui.fx_pg = FXP_ARP;
    fx_turn(AR_LATCH, -1);
    arp_run(8);
    AEXPECT(!arps[0].n && !arps[0].snd, "LATCH off: the notes kept let go");
    ui.page = PG_MAIN;

    /* REC: the arpeggio into the steps, at the 16ths, each its length */
    memset((void *)seq_note, 0, sizeof seq_note);
    arp_p[0][AR_GATE] = 127;
    btn(B_REC, 0);
    key(0, 1); key(4, 1);
    arp_run(B16 * 16u + B16 / 2u);
    {
        uint32_t i, ok = 1;
        for (i = 0; i < 16u; i++)
            ok &= seq_note[0][i] == (i & 1u ? 57u : 53u);
        printf("arp: steps %u %u %u %u .. %u, len %u\n", seq_note[0][0], seq_note[0][1], seq_note[0][2], seq_note[0][3],
               seq_note[0][15], seq_glen[0][1]);
        AEXPECT(ok && seq_run, "REC armed: the arpeggio started the sequencer, each note into its step");
    }
    key(0, 0); key(4, 0);
    btn(B_REC, 0);
    btn(B_PLAY, 0);
    arp_run(4);

    /* the looper: SHIFT + REC, the arpeggio in, REC ends it */
    arp_p[0][AR_RATE] = 4;                       /* (1/8) */
    btn(B_REC, 1);
    arp_run(8);
    key(7, 1);
    arp_run(B16 * 16u);
    btn(B_REC, 0);
    {
        const loop_t *p = &loops[0][0];
        uint32_t i, ok = p->on && p->len == 16u && p->n == 8u;
        for (i = 0; ok && i < p->n; i++)
            ok &= p->ev[i].note == 60u && p->ev[i].step == 2u * i;
        printf("arp: loop on %u len %u n %u, steps %u %u %u, len8 %u\n", p->on, p->len, p->n, p->ev[0].step,
               p->ev[1].step, p->ev[2].step, p->ev[0].len8);
        AEXPECT(ok, "SHIFT + REC: the arpeggio at 1/8 into a loop, a note every two steps");
    }
    key(7, 0);

    /* its page; kept with the session */
    btn(B_PLAY, 0);
    {
        uint32_t was = arp_on[0];
        key(KEY_ARP, 1);
        fm1_ms += 900;
        ui_input();
        key(KEY_ARP, 0);
        AEXPECT(ui.page == PG_FXP && ui.fx_pg == FXP_ARP && arp_on[0] == was,
                "the 5th black key held 0.8 s: the ARP page, the arpeggiator as it was");
    }
    fx_turn(AR_MODE, 8);                         /* PINKY */
    fx_turn(AR_DENS, -20);
    ui_draw();
    shot(dir, "arp_page");
    AEXPECT(arp_p[0][AR_MODE] == AM_PINKY && arp_p[0][AR_DENS] < 127u, "MODE and DENSITY turned on the page");
    {
        uint8_t keep[NARP];
        memcpy(keep, (const void *)arp_p[0], NARP);
        AEXPECT(!prj_save(4), "(session 5 saved)");
        tap(KEY_ARP);
        arp_p[0][AR_MODE] = AM_UP;
        AEXPECT(!prj_load(4) && arp_on[0] == 1u && !memcmp(keep, (const void *)arp_p[0], NARP),
                "the session loaded: the arpeggiator on, its settings as saved");
        /* a session of builds 50, 51: no arpeggiator */
        prj_flash_read(prj_sector(4, 0), prj_buf, PRJ_SECT);
        prj_head_fill(PRJ_MAGIC, 4u * 16u, PRJ_LEN50);
        prj_flash_erase(prj_sector(4, 0));
        prj_flash_prog(prj_sector(4, 0), prj_buf, PRJ_SECT);
        {                                        /* (its patterns as builds before 62 saved them: no state) */
            uint32_t s, j;
            for (s = 1; s <= PAT_SECTS; s++)
                if (!prj_read(4, s)) {
                    pat_t *pp = (pat_t *)(void *)(prj_buf + sizeof(prj_head_t));
                    for (j = 0; j < PAT_PER; j++)
                        pp[j].st = 0;
                    prj_write(4, s);
                }
        }
        prj_scan(1);
        AEXPECT(prj_has[4] && !prj_load(4) && !arp_on[0] && arp_p[0][AR_MODE] == AM_UP && arp_p[0][AR_DENS] == 127u,
                "a session of build 51 loads: the arpeggiators off, their start settings");
    }
    printf("arp: %d failures\n", fails);
    return fails != 0;
}

/* bkserve (build 55): the FM-1's side of the backup page, for tests/web/test_backup.mjs -- two sessions (one with
 * loops, one without) and GLOBAL SETUP in the flash, then each line from stdin a message (hex) or a command (WIPE,
 * SCAN, LOAD k: its tempo and loops), the replies to stdout as hex */
static int bk_serving;
static int bk_wire(const uint8_t *p, uint32_t n)
{
    uint32_t i;
    if (!bk_serving)
        return 0;
    for (i = 0; i < n; i++)
        printf("%02x", p[i]);
    printf("\n");
    return 0;
}
static int cmd_bkserve(void)
{
    char line[4096];
    memset(host_flash, 0xFF, sizeof host_flash);
    prj_scan(1);
    tracks_init();
    ui_init();
    fm1_ms = 5000;
    ui.tempo = 133;
    loops[1][0].n = 3; loops[1][0].len = 32; loops[1][0].on = 1;
    loops[1][0].ev[0].note = 60; loops[1][0].ev[1].note = 63; loops[1][0].ev[2].note = 67;
    loops[1][0].ev[1].step = 8; loops[1][0].ev[2].step = 20;
    loops[4][0].n = loops[4][1].n = loops[4][2].n = 250;   /* (three long ones: the stream over two sectors) */
    loops[4][0].len = loops[4][1].len = loops[4][2].len = 512;
    loops[4][0].on = loops[4][1].on = loops[4][2].on = 1;
    if (prj_save(2))
        printf("SAVE FAILED\n");
    memset(loops, 0, sizeof loops);
    ui.tempo = 97;
    if (prj_save(9))
        printf("SAVE FAILED\n");
    mcfg.send_cc = 1;
    setup_write();
    prj_scan(1);
    bk_serving = 1;
    printf("READY\n");
    fflush(stdout);
    while (fgets(line, sizeof line, stdin)) {
        static uint8_t w[2048], m[2048];
        uint32_t n = 0, i, d = 0, acc = 0, nb = 0, sm = 0;
        if (!strncmp(line, "WIPE", 4)) {
            memset(host_flash + 0x97000u, 0x5A, 0xFE000u - 0x97000u);
            prj_scan(1);
            printf("OK\n");
        } else if (!strncmp(line, "SCAN", 4)) {
            prj_scan(1);
            printf("OK\n");
        } else if (!strncmp(line, "CLEAN", 5)) {      /* (the store erased, as a new FM-1's: tools/make_demo.mjs) */
            memset(host_flash + PRJ_LO, 0xFF, sizeof host_flash - PRJ_LO);
            prj_scan(1);
            printf("OK\n");
        } else if (!strncmp(line, "PAT", 3)) {        /* PAT k j: session k (0 ..) loads at pattern j (0 ..) */
            uint32_t k = 0, j = 0;
            prj_body_t b;
            sscanf(line + 4, "%u %u", &k, &j);
            if (prj_read(k, 0) || j >= PAT_SLOTS) {
                printf("FAIL\n");
            } else {
                memcpy(&b, prj_buf + sizeof(prj_head_t), sizeof b);
                b.pat = (uint8_t)j;
                memcpy(prj_buf + sizeof(prj_head_t), &b, sizeof b);
                printf(prj_write(k, 0) ? "FAIL\n" : "OK\n");
            }
        } else if (!strncmp(line, "DUMP", 4)) {       /* DUMP FILE: the store's sectors not erased, as fm1-emu packs them */
            FILE *f = fopen(strtok(line + 5, "\n"), "wb");
            uint32_t at, ns = 0, e;
            for (at = PRJ_LO; f && at < sizeof host_flash; at += PRJ_SECT) {
                for (e = 0; e < PRJ_SECT && host_flash[at + e] == 0xFFu; e++)
                    ;
                if (e < PRJ_SECT) {
                    uint8_t le[4] = {(uint8_t)at, (uint8_t)(at >> 8), (uint8_t)(at >> 16), (uint8_t)(at >> 24)};
                    fwrite(le, 1, 4, f);
                    fwrite(host_flash + at, 1, PRJ_SECT, f);
                    ns++;
                }
            }
            if (f)
                fclose(f);
            printf(f ? "OK %u\n" : "FAIL\n", ns);
        } else if (!strncmp(line, "LOAD", 4)) {
            int rc = prj_load((uint32_t)atoi(line + 5));
            uint32_t t, l, nl = 0;
            for (t = 0; t < NTRACK; t++)
                for (l = 0; l < NLOOP; l++)
                    nl += loops[t][l].on ? loops[t][l].n : 0u;
            setup_load();
            printf(rc ? "FAIL %d\n" : "OK %d %u %u\n", rc ? rc : ui.tempo, nl, mcfg.send_cc);
        } else {
            for (i = 0; line[i] && line[i + 1] && line[i] != '\n'; i += 2) {
                unsigned v;
                sscanf(line + i, "%2x", &v);
                w[n++] = (uint8_t)v;
            }
            for (i = 1; i + 1u < n; i++) {             /* (ota_unpack7) */
                acc |= (uint32_t)(w[i] & 0x7Fu) << nb;
                nb += 7u;
                while (nb >= 8u) {
                    m[d++] = (uint8_t)acc;
                    acc >>= 8;
                    nb -= 8u;
                }
            }
            for (i = 6; i + 1u < d; i++)
                sm += m[i];
            if (d >= 7u && m[0] == 0 && m[1] == 0x59 && (uint32_t)(m[3] | m[4] << 8 | m[5] << 16) + 7u == d &&
                (uint8_t)~sm == m[d - 1u] && m[2] >= 0x50 && m[2] <= 0x54)
                bk_handle(m, d);
            else
                printf("BAD\n");
        }
        fflush(stdout);
    }
    return 0;
}

/* a new session (build 57): SAVE on an empty slot while a saved session plays -- a blank one there, nothing of the
 * one before (asked first if it has changes not saved); with nothing saved yet, what plays goes into it */
#define NEXPECT(c, what) do { if (!(c)) { printf("FAIL newses: %s\n", what); fails++; } } while (0)
static void ns_slot_save(uint32_t k)             /* (the sessions' list, slot k, its page, SAVE) */
{
    ui.page = PG_MAIN;                           /* (from the track's view) */
    btn(B_SAVE, 0);
    turn(EN_PRESET, -20);
    turn(EN_PRESET, (int32_t)k);
    btn(B_SAVE, 0);                              /* (its page, on LOAD) */
    turn(EN_PRESET, 1);                          /* SAVE */
    btn(B_SAVE, 0);
}
static int cmd_newses(const char *dir)
{
    memset(host_flash, 0xFF, sizeof host_flash);
    prj_scan(1);
    tracks_init();
    ui_init();
    fm1_ms = 5000;
    ui.tempo = 150;
    seq_note[0][0] = 60;
    loops[2][0].n = 2; loops[2][0].len = 16; loops[2][0].on = 1; loops[2][0].ev[0].note = 50; loops[2][0].ev[1].note = 52;
    arp_on[1] = 1;
    ui.chord_on[3] = 1;
    fx_dl[0] = 5;
    ui.snd[0][ui.eng[0]].amp[A_VOL] = 33;
    ns_slot_save(0);                             /* (nothing saved yet: what plays goes into slot 1) */
    NEXPECT(prj_cur == 0 && prj_has[0] && ui.tempo == 150 && seq_note[0][0] == 60 && loops[2][0].on,
            "nothing saved yet: SAVE on an empty slot keeps what plays");
    ns_slot_save(3);
    NEXPECT(prj_cur == 3 && prj_has[3] && ui.page == PG_MAIN, "a saved session playing: SAVE on an empty slot, a new "
            "session there, saved");
    NEXPECT(ui.tempo == 120 && !seq_note[0][0] && !loops[2][0].on && !arp_on[1] && !ui.chord_on[3] && fx_dl[0] == DL0[0] &&
            ui.snd[0][ui.eng[0]].amp[A_VOL] != 33 && !prj_dirty && !pat_dirty && !seq_run,
            "the new session: nothing of the one before (tempo, steps, loops, ARP, chords, effects, sounds)");
    ui_draw();
    shot(dir, "new_session");
    NEXPECT(!prj_load(0) && ui.tempo == 150 && seq_note[0][0] == 60 && loops[2][0].on && arp_on[1],
            "the session before, loaded again: all of it");
    ui.tempo = 151;                              /* (changes not saved: asked) */
    prj_dirty = 1;
    ns_slot_save(5);
    NEXPECT(pj.ask == PQ_NEW && !prj_has[5], "changes not saved: NEW SESSION? asked");
    btn(B_SAVE, 0);                              /* (NO, as it opens) */
    NEXPECT(!prj_has[5] && ui.tempo == 151, "NO: nothing changed");
    btn(B_SAVE, 0);
    turn(EN_PRESET, 1);                          /* (YES) */
    btn(B_SAVE, 0);
    NEXPECT(prj_has[5] && prj_cur == 5 && ui.tempo == 120, "YES: the new session");
    printf("newses: %d failures\n", fails);
    return fails != 0;
}

/* (build 62) a pattern holds the tracks' state as it was saved -- muted, machine, octave, level, pan, sidechain,
 * CHORD mode, arpeggiator -- and gives it back when it plays; a pattern saved before (or empty) leaves it as it is */
#define PEXPECT(c, what) do { if (!(c)) { printf("FAIL pstate: %s\n", what); fails++; } } while (0)
static void ps_set_a(void)                       /* (state A) */
{
    uint8_t ar[NARP];
    memcpy(ar, AR_DEF, NARP);
    ar[AR_MODE] = AM_DOWN;
    ar[AR_GATE] = 100;
    memset((void *)trk_mute, 0, NTRACK);
    trk_mute[2] = 1;
    arp_set(1, 1, ar);
    ui.chord_on[3] = 1;
    ui.chord_form[3] = 2;
    ui.eng[4] = 2;
    ui.oct[0] = 2;
    ui.level[1] = 7;
    ui.pan[2] = -10;
    level_pan_apply(1);
    level_pan_apply(2);
    sc_src[5] = 2;
    sc_lvl[5] = 90;
}
static int ps_is_a(void)
{
    return !trk_mute[0] && !trk_mute[1] && trk_mute[2] && arp_on[1] && arp_p[1][AR_MODE] == AM_DOWN &&
           arp_p[1][AR_GATE] == 100u && ui.chord_on[3] && ui.chord_form[3] == 2u && ui.eng[4] == 2u && ui.oct[0] == 2 &&
           ui.level[1] == 7 && ui.pan[2] == -10 && sc_src[5] == 2u && sc_lvl[5] == 90u;
}
static void ps_set_b(void)                       /* (state B) */
{
    memset((void *)trk_mute, 0, NTRACK);
    trk_mute[4] = trk_mute[5] = 1;
    arp_set(1, 0, AR_DEF);
    ui.chord_on[3] = 0;
    ui.eng[4] = 0;
    ui.oct[0] = -1;
    ui.level[1] = LEVEL_UNITY;
    ui.pan[2] = 0;
    sc_src[5] = 0;
    sc_lvl[5] = 64;
}
static int ps_is_b(void)
{
    return !trk_mute[2] && trk_mute[4] && trk_mute[5] && !arp_on[1] && !ui.chord_on[3] && ui.eng[4] == 0u &&
           ui.oct[0] == -1 && ui.level[1] == LEVEL_UNITY && ui.pan[2] == 0 && sc_src[5] == 0u && sc_lvl[5] == 64u;
}
static int cmd_pstate(const char *dir)
{
    int32_t out[2 * CTL];
    uint32_t i, t;
    memset(host_flash, 0xFF, sizeof host_flash);
    prj_scan(1);
    tracks_init();
    ui_init();
    fm1_ms = 5000;
    seq_note[0][0] = 60;
    ps_set_a();
    PEXPECT(!prj_save(0) && pat_store[0].st == PX_ON, "(session 1 saved, pattern 1 with it, its state)");
    pat_switch(1);                               /* (stopped: at once) */
    PEXPECT(pat_cur == 1u && ps_is_a(), "an empty pattern: the state left as it plays");
    ps_set_b();
    pat_poll();
    PEXPECT(pat_dirty, "the state changed: the pattern changed");
    seq_note[1][4] = 62;
    PEXPECT(!pat_save(1) && !pat_dirty && pat_store[1].st == PX_ON, "pattern 2 saved, with state B");
    pat_switch(0);
    PEXPECT(pat_cur == 0u && ps_is_a() && seq_note[0][0] == 60u && !pat_dirty, "pattern 1: state A back, its steps");
    pat_switch(1);
    PEXPECT(pat_cur == 1u && ps_is_b() && seq_note[1][4] == 62u && !seq_note[0][0], "pattern 2: state B back");
    {   /* (the flash holds it: the session loaded again, at pattern 2) */
        PEXPECT(!prj_save(0), "(session 1 saved at pattern 2)");
        ps_set_a();
        PEXPECT(!prj_load(0) && pat_cur == 1u && ps_is_b(), "the session loaded: its pattern's state");
        pat_switch(0);
        PEXPECT(ps_is_a(), "and pattern 1's, from the flash");
    }
    /* a change not saved: kept in the store as the steps are, while another plays */
    pat_switch(1);
    trk_mute[0] = 1;
    pat_switch(0);
    PEXPECT(!trk_mute[0] && pat_dirtyv[1], "pattern 2's mute not saved: kept in the store, pattern 1 as saved");
    pat_switch(1);
    PEXPECT(trk_mute[0] && pat_dirty, "pattern 2 again: the mute as it was left");
    /* switched while playing: at the end of the chosen track's time through, with its state */
    seq_run = 1;
    pat_switch(0);
    PEXPECT(trk_mute[0] && seq_pend == 1u, "playing: pattern 1 waits, the state as it plays");
    for (i = 0; i < 4000u && seq_pend; i++) {
        mix_block(out);
        pat_poll();
    }
    PEXPECT(!seq_pend && pat_cur == 0u && ps_is_a() && !trk_mute[0], "taken in: pattern 1's state");
    seq_run = 0;
    /* a pattern saved before build 62: no state -- the state left as it plays; its steps as ever */
    pat_store[2] = pat_store[0];
    pat_store[2].st = 0;
    pat_switch(1);
    pat_switch(2);
    PEXPECT(pat_cur == 2u && ps_is_b() && seq_note[0][0] == 60u && !pat_dirty,
            "a pattern of build 61: the state left as it plays, its steps");
    /* its session's mutes, as build 61 kept them (a session loaded: they, then its pattern's -- none) */
    PEXPECT(!prj_save(3), "(session 4 saved, at pattern 3)");
    {
        uint32_t s, j;
        for (s = 1; s <= PAT_SECTS; s++)
            if (!prj_read(3, s)) {
                pat_t *pp = (pat_t *)(void *)(prj_buf + sizeof(prj_head_t));
                for (j = 0; j < PAT_PER; j++)
                    pp[j].st = 0;
                prj_write(3, s);
            }
    }
    ps_set_a();
    PEXPECT(!prj_load(3) && pat_cur == 2u && ps_is_b(), "a session of build 61: the session's mutes and settings");
    /* the room: every step a note and four locks -- the steps end before the state, the state whole */
    {
        static pat_t pp;
        static seqbuf_t sb, sb2;
        uint32_t o = SEQ_REC4, r;
        for (t = 0; t < NTRACK; t++)
            for (i = 0; i < NSTEP; i++) {
                uint32_t l;
                seq_note[t][i] = (uint8_t)(48u + i);
                for (l = 0; l < 4u && l < NPL; l++) {
                    seq_lid[t][i][l] = (uint8_t)l;
                    seq_lv[t][i][l] = 5;
                }
            }
        ps_set_a();
        pat_take(&pp);
        for (r = 0; r < pp.seq[SEQ_NREC]; r++)
            o += 6u + 3u * (pp.seq[o + 5u] & 63u) + 3u * (pp.seq[o + 5u] >> 7) + (pp.seq[o + 5u] >> 6 & 1u);
        PEXPECT(o <= SEQ_PX && pp.seq[SEQ_NREC] > 0u, "a full pattern: its steps end before the state");
        ps_set_b();
        pat_store[7] = pp;
        pat_give(7, 1);
        PEXPECT(ps_is_a(), "a full pattern: its state whole");
        pat_decode(&pp, &sb);
        memcpy(sb2.note, sb.note, sizeof sb.note);
        memcpy(sb2.lid, sb.lid, sizeof sb.lid);
        pp.st = 0;                               /* (as build 61 reads it: the same steps) */
        pat_decode(&pp, &sb);
        PEXPECT(!memcmp(sb2.note, sb.note, sizeof sb.note) && !memcmp(sb2.lid, sb.lid, sizeof sb.lid) &&
                sb.note[0][0] == 48u, "a full pattern read without its state: the steps as ever");
    }
    {   /* (build 64) 96 steps of notes, no locks: every one kept beside the state */
        static pat_t pq;
        static seqbuf_t sq;
        uint32_t ok = 1;
        for (t = 0; t < NTRACK; t++)
            for (i = 0; i < NSTEP; i++) {
                seq_note[t][i] = (uint8_t)(40u + i);
                memset((void *)seq_lid[t][i], PL_NONE, NPL);
            }
        ps_set_a();
        pat_take(&pq);
        pat_decode(&pq, &sq);
        for (t = 0; t < NTRACK; t++)
            for (i = 0; i < NSTEP; i++)
                ok &= sq.note[t][i] == 40u + i;
        PEXPECT(ok && pq.seq[SEQ_NREC] == NTRACK * NSTEP, "96 steps of notes: all kept with the state");
        /* a pattern of builds 62, 63: the state unpacked, its last 78 bytes */
        {
            uint8_t x[NTRACK * PX_T];
            px_take(x);
            pq.st = PX_ON1;
            memset(pq.seq + SEQ_PX1, 0, PAT_SEQ_ROOM - SEQ_PX1);
            memcpy(pq.seq + SEQ_PX1, x, sizeof x);
            ps_set_b();
            pat_store[8] = pq;
            pat_give(8, 1);
            PEXPECT(ps_is_a(), "a pattern of build 62 or 63: its state, as they saved it");
        }
    }
    ui_draw();
    shot(dir, "pattern_state");
    /* (build 63) MUTE mode on the PATTERNS grid: kept when the grid opens, the 2nd black key turns it on or off there,
     * the tracks' buttons mute and the grid stays */
    ui.page = PG_MAIN;
    ui.sv = 0;
    ui.mute_mode = 0;
    memset((void *)trk_mute, 0, NTRACK);
    tap(KEY_TRK);
    btn(B_ARP, 0);
    PEXPECT(ui.page == PG_PAT && ui.mute_mode, "MUTE mode on, ARP: the grid, MUTE mode still on");
    btn((uint32_t)TRACK_BTN[3], 0);
    ui_input();
    ui_leds();
    PEXPECT(trk_mute[3] && ui.page == PG_PAT && host_led[14u + KEY_TRK] && !host_led[PANEL_BTN[TRACK_BTN[3]]] &&
            host_led[PANEL_BTN[TRACK_BTN[0]]], "on the grid: a track's button mutes it (dim), the grid stays");
    ui_draw();
    shot(dir, "pat_grid_mute");
    tap(KEY_TRK);
    ui_leds();
    PEXPECT(!ui.mute_mode && ui.page == PG_PAT && !host_led[14u + KEY_TRK], "the 2nd black key on the grid: MUTE mode off");
    tap(KEY_TRK);
    PEXPECT(ui.mute_mode && ui.page == PG_PAT, "and on again, on the grid");
    tap(KEY_TRK);
    btn((uint32_t)TRACK_BTN[0], 0);
    ui_input();
    PEXPECT(ui.page == PG_MAIN && ui.sel == 0u && trk_mute[3], "MUTE mode off: a track's button back to the track, the mutes stay");
    ui.sv = 1;                                   /* (the step view on: on the grid, MUTE mode all the same) */
    btn(B_ARP, 0);
    tap(KEY_TRK);
    ui_input();
    PEXPECT(ui.page == PG_PAT && ui.mute_mode, "the step view on: MUTE mode on the grid all the same");
    tap(KEY_TRK);
    ui.sv = 0;
    printf("pstate: %d failures\n", fails);
    return fails != 0;
}

/* (build 64) BREAK on the PATTERNS grid; the 3rd black key held and a white key held 0.8 s: the break saved there */
#define BEXPECT(c, what) do { if (!(c)) { printf("FAIL brksave: %s\n", what); fails++; } } while (0)
static uint32_t bs_kw[16];
static void bs_hold(uint32_t w)                  /* (white key w held 0.8 s, let go) */
{
    key(bs_kw[w], 1);
    fm1_ms += 900;
    ui_input();
    key(bs_kw[w], 0);
}
static void bs_break(int all)                    /* (the 3rd black key down -- SHIFT too: every track -- its plan
                                                  * drawn) */
{
    int32_t out[2 * CTL];
    uint32_t i;
    if (all)
        fm1_in.notes |= 1u << KEY_SHIFT;
    key(KEY_TRIG, 1);
    fm1_in.notes &= ~(1u << KEY_SHIFT);
    for (i = 0; i < 400u && brk_shown == 0xFFu; i++)
        mix_block(out);
    ui_input();
}
/* pattern k's steps: each broken track's step p the step its plan plays there (or 0 in the gap, or an octave
 * away); the others as written (w) */
static int bs_check(uint32_t k, const seqbuf_t *w, uint32_t mask)
{
    static seqbuf_t b;
    uint32_t t, p, ok = 1;
    pat_decode(&pat_store[k], &b);
    for (t = 0; t < NTRACK; t++) {
        uint32_t len = w->len[t] ? w->len[t] : NSTEP, a = brk_fresh >> t & 1u ? 0u : brk_anchor[t];
        for (p = 0; p < NSTEP; p++) {
            if (!(mask >> t & 1u) || (brk_keep >> t & 1u) || p >= len) {
                ok &= b.note[t][p] == w->note[t][p];
                continue;
            }
            {
                uint32_t src = brk_map(a, p, len, brk_kick >> t & 1u), n = w->note[t][src], g = b.note[t][p];
                ok &= g == n || (n && (g == n + 12u || g + 12u == n)) ||
                      (!g && (brk_lay & BL_GAP) && p % 16u >= brk_gap);
            }
        }
    }
    return (int)ok;
}
static int cmd_brksave(const char *dir)
{
    static seqbuf_t w;
    int32_t out[2 * CTL];
    uint32_t k, t, i, cur0;
    memset(host_flash, 0xFF, sizeof host_flash);
    prj_scan(1);
    tracks_init();
    ui_init();
    fm1_ms = 5000;
    for (k = 0; k < NKEY; k++)
        if (white_ix(k) >= 0)
            bs_kw[white_ix(k)] = k;
    for (t = 0; t < NTRACK; t++)                 /* (three steps of four a note of its own: where it came from,
                                                  * readable; the pattern's room not the question) */
        for (i = 0; i < NSTEP; i++)
            seq_note[t][i] = (uint8_t)(i % 4u == 3u ? 0u : 40u + t * 2u + i);
    trk_mute[4] = 1;
    BEXPECT(!prj_save(0), "(session 1 saved)");
    seq_copy_out(&w);
    btn(B_PLAY, 0);
    for (i = 0; i < 40u; i++)
        mix_block(out);
    btn(B_ARP, 0);
    cur0 = pat_cur;
    bs_break(1);
    ui_leds();
    BEXPECT(ui.page == PG_PAT && ui.brk == (1u << NTRACK) - 1u && brk_shown != 0xFFu && host_led[14u + KEY_TRIG],
            "the grid: SHIFT and the 3rd black key, every track broken, the key lit");
    for (i = 0; i < 1500u; i++)                  /* (each track into it, on its next beat) */
        mix_block(out);
    ui_input();
    ui_draw();
    shot(dir, "pat_grid_break");
    BEXPECT(!(brk_fresh & ~brk_keep & ((1u << NTRACK) - 1u)), "(every track into the break)");
    bs_hold(6);
    BEXPECT(pat_store[6].used && pat_cur == cur0 && ui.page == PG_PAT && str_eq(ui.flash_l1, "BREAK SAVED"),
            "a white key held 0.8 s: the break saved as pattern 7, the one playing kept");
    BEXPECT(bs_check(6, &w, (1u << NTRACK) - 1u), "pattern 7: each track's steps the ones its break plays");
    {
        uint8_t x[NTRACK * PX_T];
        px_unpack(x, pat_store[6].seq + SEQ_PX);
        BEXPECT(pat_store[6].st == PX_ON && (x[4u * PX_T] & 1u), "with the tracks' state (track 5 muted)");
    }
    {
        int rc = prj_read(0, 2u);
        const pat_t *pp = (const pat_t *)(const void *)(prj_buf + sizeof(prj_head_t));
        BEXPECT(!rc && !memcmp(&pp[0], &pat_store[6], sizeof pp[0]), "pattern 7 in the session's flash");
    }
    for (t = 0, i = 1; t < NTRACK; t++)
        for (k = 0; k < NSTEP; k++)
            i &= seq_note[t][k] == w.note[t][k];
    BEXPECT(i, "the steps playing untouched");
    key(KEY_TRIG, 0);
    BEXPECT(!ui.brk && !brk_mask, "the key let go: the break over");
    /* the chosen track alone (no SHIFT): the others as written; saved over an occupied slot */
    ui.sel = 2;
    bs_break(0);
    for (i = 0; i < 1500u; i++)
        mix_block(out);
    bs_hold(6);
    BEXPECT(bs_check(6, &w, 1u << 2), "the chosen track's break over pattern 7: the other tracks as written");
    key(KEY_TRIG, 0);
    /* over the pattern playing: its steps the break's, at the end of the chosen track's time through */
    bs_break(1);
    for (i = 0; i < 300u; i++)
        mix_block(out);
    bs_hold(cur0);
    BEXPECT(bs_check(cur0, &w, (1u << NTRACK) - 1u), "over the one playing: the break saved there");
    BEXPECT(seq_pend == 1u && !pat_dirty, "over the one playing: the break's steps waiting");
    for (i = 0; i < 4000u && seq_pend; i++) {
        mix_block(out);
        pat_poll();
    }
    BEXPECT(!seq_pend && pat_cur == cur0, "taken in, at the end of the chosen track's time through");
    key(KEY_TRIG, 0);
    {
        static seqbuf_t b;
        pat_decode(&pat_store[cur0], &b);
        for (t = 0, i = 1; t < NTRACK; t++)
            for (k = 0; k < NSTEP; k++)
                i &= seq_note[t][k] == b.note[t][k];
        BEXPECT(i && !pat_dirty, "the break's steps playing, the pattern as saved");
    }
    /* stopped: nothing to save */
    btn(B_PLAY, 0);
    for (i = 0; i < 8u; i++)
        mix_block(out);
    memset(&pat_store[9], 0, sizeof pat_store[9]);
    bs_break(1);
    bs_hold(9);
    BEXPECT(!pat_store[9].used, "stopped: no break, nothing saved");
    key(KEY_TRIG, 0);
    printf("brksave: %d failures\n", fails);
    return fails != 0;
}

/* (build 65) a pattern change that unmutes a track: its notes from the muted time let go, its loops silent until each
 * comes round to its first step, then on in its place; the mutes as the pattern comes in */
#define QEXPECT(c, what) do { if (!(c)) { printf("FAIL loopin: %s\n", what); fails++; } } while (0)
static void li_until(uint32_t g)                 /* (played until the tempo's 16th g; the main loop's poll each block) */
{
    int32_t out[2 * CTL];
    uint32_t i;
    for (i = 0; i < 200000u && (int32_t)(seq_gcount - g) < 0; i++) {
        mix_block(out);
        pat_poll();
    }
}
static int li_sounding(uint32_t t)               /* (a voice of track t sounding, not fading) */
{
    uint32_t k, n = 0;
    for (k = 0; k < NVOICE; k++)
        n += vox[k].track == t && vox[k].sounding && !vox[k].fade;
    return n != 0;
}
static int cmd_loopin(const char *dir)
{
    loop_t *lp = &loops[3][0];
    uint32_t n0, i;
    memset(host_flash, 0xFF, sizeof host_flash);
    prj_scan(1);
    tracks_init();
    ui_init();
    fm1_ms = 5000;
    for (i = 0; i < NSTEP; i += 4u)
        seq_note[0][i] = 36;
    ui.sel = 0;
    trk_mute[3] = 1;                             /* pattern 1: track 4 muted; pattern 2: playing */
    QEXPECT(!pat_save(0), "(pattern 1 saved, track 4 muted)");
    trk_mute[3] = 0;
    QEXPECT(!pat_save(1), "(pattern 2 saved, track 4 playing)");
    pat_switch(0);
    QEXPECT(trk_mute[3] && !loop_wait[3], "(pattern 1: track 4 muted)");
    btn(B_PLAY, 0);
    li_until(1);                                 /* (started: the loops' anchors from it) */
    /* a loop on track 4: two bars, a long note at its first step, another at its 25th; its first step at bar 2 */
    memset(lp, 0, sizeof *lp);
    lp->len = 32;
    lp->n = 2;
    lp->ev[0].step = 0;  lp->ev[0].note = 60; lp->ev[0].vel = 100; lp->ev[0].len8 = 8 * 12;
    lp->ev[1].step = 24; lp->ev[1].note = 62; lp->ev[1].vel = 100; lp->ev[1].len8 = 8 * 12;
    lp->anchor = 16;
    lp->on = 1;
    li_until(17);
    QEXPECT(seq_sounded[3] == 1u && li_sounding(3), "muted, the loop plays on unheard (its first note, at bar 2)");
    pat_switch(1);                               /* (in at bar 3: the loop at its 17th step) */
    QEXPECT(seq_pend == 1u && trk_mute[3], "pattern 2 waiting, track 4 still muted");
    li_until(32);
    QEXPECT(pat_cur == 1u && !trk_mute[3] && (loop_wait[3] & 1u) && !li_sounding(3),
            "pattern 2 in (bar 3): track 4 unmuted as it comes in, the note from the muted time let go, the loop waiting");
    n0 = seq_sounded[3];
    li_until(47);
    QEXPECT(seq_sounded[3] == n0 && (loop_wait[3] & 1u), "the loop's 25th step (bar 3): not played, waiting");
    li_until(48);
    QEXPECT(seq_sounded[3] == n0 + 1u && !(loop_wait[3] & 1u) && li_sounding(3),
            "bar 4, the loop's first step: on, in its place");
    li_until(72);
    QEXPECT(seq_sounded[3] == n0 + 2u, "and on from there (its 25th step)");
    /* in at the loop's own first step: played at once, nothing lost */
    pat_switch(0);
    li_until(81);
    QEXPECT(pat_cur == 0u && trk_mute[3], "(pattern 1 again at bar 6: track 4 muted)");
    li_until(100);
    pat_switch(1);
    li_until(112);                               /* (bar 8: the loop's first step, as pattern 2 comes in) */
    {
        int32_t out[2 * CTL];
        for (i = 0; i < 4u; i++)
            mix_block(out);
    }
    pat_poll();
    QEXPECT(pat_cur == 1u && !trk_mute[3] && !(loop_wait[3] & 1u) && li_sounding(3) && sq_note[3][0] + sq_note[3][1] +
            sq_note[3][2] + sq_note[3][3] >= 60u, "in at the loop's first step: its first note played at once");
    /* stopped: a pattern that unmutes a track -- its loops from their first step once started */
    btn(B_PLAY, 0);
    {
        int32_t out[2 * CTL];
        for (i = 0; i < 8u; i++)                 /* (the sequencer stopped at its next block) */
            mix_block(out);
    }
    QEXPECT(!seq_run, "(stopped)");
    pat_switch(0);
    QEXPECT(pat_cur == 0u && trk_mute[3], "(stopped: pattern 1, track 4 muted)");
    loop_wait[3] = 0;
    pat_switch(1);
    QEXPECT(pat_cur == 1u && !trk_mute[3] && (loop_wait[3] & 1u), "stopped: unmuted, the loop waiting for its first step");
    /* a pattern without the mutes (empty): nothing changed as it comes in */
    btn(B_PLAY, 0);
    {
        int32_t out[2 * CTL];
        for (i = 0; i < 2u; i++)                 /* (started again: from the first 16th) */
            mix_block(out);
    }
    li_until(20);
    trk_mute[3] = 1;
    pat_switch(5);
    li_until(36);
    QEXPECT(pat_cur == 5u && trk_mute[3], "an empty pattern: the mutes as they were");
    (void)dir;
    printf("loopin: %d failures\n", fails);
    return fails != 0;
}

/* (build 68) a pattern with another transposition, switched to while playing: the loops play its transposition from
 * the step it comes in on (before: from the next, the main loop moving the notes after the sequencer had played) */
#define XEXPECT(c, what) do { if (!(c)) { printf("FAIL lptrn: %s\n", what); fails++; } } while (0)
static int lx_sounds(uint32_t t, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < SEQ_HOLD; i++)
        if (sq_note[t][i] == n)
            return 1;
    return 0;
}
static void lx_settle(void)                      /* (a few blocks on: a pattern due taken in) */
{
    int32_t out[2 * CTL];
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        mix_block(out);
        pat_poll();
    }
}
static int cmd_lptrn(const char *dir)
{
    loop_t *lp = &loops[3][0];
    uint32_t i;
    memset(host_flash, 0xFF, sizeof host_flash);
    prj_scan(1);
    tracks_init();
    ui_init();
    fm1_ms = 5000;
    ui.scale = 0;                                /* (CHROMATIC: a step a semitone) */
    for (i = 0; i < NSTEP; i += 4u)
        seq_note[0][i] = 36;
    ui.sel = 0;
    XEXPECT(!pat_save(0), "(pattern 1: no transposition)");
    ui.trn[3] = 2;
    XEXPECT(!pat_save(1), "(pattern 2: track 4 at +2)");
    ui.trn[3] = 0;
    pat_switch(0);
    XEXPECT(!ui.trn[3] && !lp_trn[3], "(pattern 1)");
    btn(B_PLAY, 0);
    li_until(1);
    memset(lp, 0, sizeof *lp);                   /* a loop on track 4, a bar, C at its first step (short) */
    lp->len = 16;
    lp->n = 1;
    lp->ev[0].note = 60; lp->ev[0].vel = 100; lp->ev[0].len8 = 4;
    lp->anchor = 0;
    lp_ref[3][0] = 0;
    lp->on = 1;
    li_until(16);
    XEXPECT(lx_sounds(3, 60), "pattern 1: the loop's C at bar 2");
    li_until(20);
    pat_switch(1);
    li_until(32);                                /* (bar 3: pattern 2 comes in with the loop's first step) */
    XEXPECT(lx_sounds(3, 62) && !lx_sounds(3, 60), "pattern 2 in at bar 3: the loop's first step there already at +2 (D)");
    lx_settle();
    XEXPECT(pat_cur == 1u && lp_trn[3] == 2, "(pattern 2 playing)");
    li_until(36);
    pat_switch(0);
    li_until(48);
    XEXPECT(lx_sounds(3, 60) && !lx_sounds(3, 62), "pattern 1 back at bar 4: C at once");
    lx_settle();
    XEXPECT(pat_cur == 0u && !lp_trn[3], "(pattern 1 playing)");
    /* the session saved at pattern 2: the loop as it plays there; loaded, the same */
    pat_switch(1);
    li_until(64);
    lx_settle();
    XEXPECT(pat_cur == 1u && !prj_save(2), "(session 3 saved, at pattern 2)");
    XEXPECT(lp->ev[0].note == 60 && lp_ref[3][0] == 0, "(in RAM the loop as recorded)");
    XEXPECT(!prj_load(2) && pat_cur == 1u && lp->ev[0].note == 62 && lp_ref[3][0] == 2 && lp_note(3, 0, 62) == 62,
            "loaded: the loop saved as it played (D, at +2), played the same");
    btn(B_PLAY, 0);
    {
        int32_t out[2 * CTL];
        for (i = 0; i < 8u; i++)
            mix_block(out);
    }
    pat_switch(0);
    XEXPECT(lp_note(3, 0, lp->ev[0].note) == 60, "and pattern 1 from there: C");
    /* (build 69) the session loaded at a pattern: the grid's cursor (white) on it, not on pattern 1 */
    {
        int32_t out[2 * CTL];
        for (i = 0; i < 8u; i++)
            mix_block(out);
        pat_switch(6);
        XEXPECT(pat_cur == 6u && !prj_save(4), "(session 5 saved at pattern 7)");
        pj.slot[1] = 0;
        pat_switch(0);
        pat_poll();
        XEXPECT(!prj_load(4) && pat_cur == 6u, "(session 5 loaded: pattern 7 playing)");
        pat_poll();
        btn(B_ARP, 0);
        XEXPECT(ui.page == PG_PAT && pj.slot[1] == 6u, "the grid: its cursor on pattern 7, the one playing");
        ui_draw();
        shot(dir, "pat_grid_loaded7");
        btn(B_ARP, 0);
    }
    printf("lptrn: %d failures\n", fails);
    return fails != 0;
}

int main(int argc, char **argv)
{
    wt_init();
    if (argc >= 3 && !strcmp(argv[1], "newses"))
        return cmd_newses(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "pstate"))
        return cmd_pstate(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "brksave"))
        return cmd_brksave(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "loopin"))
        return cmd_loopin(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "lptrn"))
        return cmd_lptrn(argv[2]);
    if (argc >= 2 && !strcmp(argv[1], "bkserve"))
        return cmd_bkserve();
    if (argc >= 3 && !strcmp(argv[1], "arp"))
        return cmd_arp(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "trn"))
        return cmd_trn(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "lfo"))
        return cmd_lfo(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "projects"))
        return cmd_projects(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "phrases"))
        return cmd_phrases(argv[2], argc >= 4 ? atoi(argv[3]) : 4);
    if (argc >= 3 && !strcmp(argv[1], "tw")) {
        printf("M %d S %d XS %d\n", (int)text_w(&FONT_M, argv[2]), (int)text_w(&FONT_S, argv[2]), (int)text_w(&FONT_XS, argv[2]));
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "sizes")) {
        printf("sound_t %zu ck_t %zu vck_t %zu ftck %zu fdck %zu wtck %zu swck %zu vmem %zu voice %zu prj_body %zu\n",
               sizeof(sound_t), sizeof(ck_t), sizeof(vck_t), sizeof(ftck_t), sizeof(fdck_t), sizeof(wtck_t),
               sizeof(swck_t), sizeof(vmem_t), sizeof(voice_t), sizeof(prj_body_t));
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "prof"))
        return cmd_prof();
    if (argc >= 2 && !strcmp(argv[1], "duck"))
        return cmd_duck(argc >= 3 ? atoi(argv[2]) : 70, argc >= 4 ? argv[3] : 0);
    if (argc >= 2 && !strcmp(argv[1], "dsp"))
        return cmd_dsp();
    if (argc >= 2 && !strcmp(argv[1], "od"))
        return cmd_od();
    if (argc >= 2 && !strcmp(argv[1], "cost"))
        return cmd_cost();
    if (argc >= 4 && !strcmp(argv[1], "chord"))
        return cmd_chord(atoi(argv[2]), atoi(argv[3]), argc >= 5 ? argv[4] : 0);
    if (argc >= 4 && !strcmp(argv[1], "one"))
        return cmd_one(atoi(argv[2]), atoi(argv[3]));
    if (argc >= 4 && !strcmp(argv[1], "dump"))
        return cmd_dump(atoi(argv[2]), atoi(argv[3]));
    if (argc >= 3 && !strcmp(argv[1], "render"))
        return cmd_render(argv[2], argc >= 4 ? atoi(argv[3]) : 2, 0);
    if (argc >= 2 && !strcmp(argv[1], "check"))
        return cmd_render(0, 12, 1);
    if (argc >= 3 && !strcmp(argv[1], "screens"))
        return cmd_screens(argv[2]);
    fprintf(stderr, "host render DIR [SEEDS] | check | screens DIR\n");
    return 2;
}
