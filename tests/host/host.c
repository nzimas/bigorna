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
static uint8_t host_led[64];
static void fm1_led_key(uint32_t id, int on) { if (id < 64u) host_led[id] = (uint8_t)!!on; }
#include "ui.c"
#include "projects.c"
#include "phrase.c"

/* the flash, emulated: NOR (erased to 0xFF, programming only clears bits) */
static uint8_t host_flash[0x100000];
static uint32_t host_erases;
static int prj_flash_read(uint32_t off, void *p, uint32_t n)
{
    memcpy(p, host_flash + off, n);
    return 0;
}
static int prj_flash_erase(uint32_t off)
{
    if (off < PRJ_LO || off + 4096u > PRJ_LO + PRJ_SLOTS * PRJ_SECTS * PRJ_SECT || (off & 4095u))
        return -8;
    memset(host_flash + off, 0xFF, 4096u);
    host_erases++;
    return 0;
}
static int prj_flash_prog(uint32_t off, const void *p, uint32_t n)
{
    uint32_t i;
    if (off < PRJ_LO || off + n > PRJ_LO + PRJ_SLOTS * PRJ_SECTS * PRJ_SECT)
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
    host_press = 1u << PANEL_BTN[B_SEQ];
    ui_input();
    shot(dir, "fx_reverb");
    host_press = 1u << PANEL_BTN[B_SEQ];
    ui_input();
    shot(dir, "fx_delay");
    host_press = 1u << PANEL_BTN[B_SEQ];
    ui_input();
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
    turn(EN_PRESET, -9);                         /* LOAD */
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
    turn(EN_PRESET, 2);                          /* COPY */
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
        uint32_t kw[16], k, n0, n1, i;
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
        ui_leds();
        EXPECT(host_led[14u + KEY_STEP], "STEP lit");
        fm1_in.notes |= 1u << kw[0];
        ui_input();
        EXPECT(seq_note[5][0] == n0 && pat_dirty, "the step view and an empty step: the last note there");
        EXPECT(seq_vel[5][0] == VEL_DEF && seq_glen[5][0] == GLEN_DEF && !seq_cond[5][0], "a new step: the defaults");
        turn(EN_K1, 1);                          /* (a step held, EDIT not up: the knobs its note, velocity ..) */
        n1 = seq_note[5][0];
        EXPECT(n1 > n0 && in_scale((int32_t)n1), "TRIG, KNOB 1: the note up the scale");
        turn(EN_K2, -20);
        turn(EN_K3, 2);
        turn(EN_K4, CD_AB - CD_NONE + 2);        /* 1:2, !1:2, 2:2 */
        EXPECT(seq_vel[5][0] == VEL_DEF - 20 && seq_glen[5][0] == GLEN_DEF + 2 && seq_cond[5][0] == CD_AB + 2,
               "KNOB 2 .. 4: velocity, length, condition");
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
        /* ARP held: the pattern (1) saved, its steps with it; the grid: ARP loads the one chosen */
        fm1_in.buttons |= 1u << PANEL_BTN[B_ARP];
        host_press = 1u << PANEL_BTN[B_ARP];
        ui_input();
        fm1_ms += 900;
        ui_input();
        fm1_in.buttons = 0;
        ui_input();
        fm1_ms += 2000;
        EXPECT(pat_cur == 0u && pat_store[0].used && !pat_dirty && pat_store[0].seq[SEQ_FMT] == SEQ_V3 &&
               pat_store[0].seq[SEQ_NREC] == 2u && pat_store[0].seq[SEQ_REC + 6u + 1u] == n0,
               "ARP held saves the pattern playing, its steps");
        {   /* (written into session 3's flash) */
            int rc = prj_read(2, 1u);
            const pat_t *pp = (const pat_t *)(const void *)(prj_buf + sizeof(prj_head_t));
            EXPECT(!rc && pp[0].used && pp[0].seq[SEQ_REC] == 5u * NSTEP && pp[0].seq[SEQ_REC + 1u] == n1,
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
        EXPECT(seq_cond[5][0] == CD_AB + 2 && seq_vel[5][0] == VEL_DEF - 20 && seq_glen[5][0] == GLEN_DEF + 2,
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
        fm1_in.notes |= 1u << kw[0];
        ui_input();
        fm1_ms += 900;
        ui_input();
        EXPECT(!pat_store[0].used && pat_cur == 12u && ui.flash_on && str_eq(ui.flash_l1, "EMPTY"),
               "a white key held: its pattern emptied, the one playing kept");
        ui_draw();
        shot(dir, "pat_cleared");
        {
            int rc = prj_read(2, 1u);
            const pat_t *pp = (const pat_t *)(const void *)(prj_buf + sizeof(prj_head_t));
            EXPECT(!rc && !pp[0].used, "emptied in the flash too");
        }
        fm1_in.notes &= ~(1u << kw[0]);
        ui_input();
        EXPECT(pat_cur == 12u, "let go after: nothing loaded");
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
            EXPECT(pl_val(5, st, PG_FLT1 * 8u + 4u) == clampi(f0 - 5, 0, 127) && pl_find(5, st, PG_FLT1 * 8u + 0u) >= 0 &&
                   pl_find(5, st, PG_AMP * 8u + 7u) >= 0 && pl_val(5, st, PL_PAN) == 73 && cur()->flt[F_FREQ] == fsnd,
                   "a step held, EDIT up: the knobs lock (the sound kept)");
            ui.dial_until = 0;
            ui_draw();
            shot(dir, "lock_edit");
            turn(EN_SELECT, 1);
            ui_draw();
            shot(dir, "lock_dial");
            EXPECT(ui.shown_page == 4u && ui.dial_lk == PG_AMP * 8u + 7u + 1u, "the level's lock shown as a dial");
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
                if (pat_bytes() > PAT_SEQ_ROOM)
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
            pat_give(15);
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
            (void)pl_set(0, 0, PG_FD_SYN_TEST, 0);   /* (a machine's own: FM DRUM's TUNE, SYN 1 knob A) */
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
        EXPECT(!ui.lrec && loops[0][0].on && loops[0][0].len == 9u && loops[0][0].anchor == 9u && !met_on,
               "REC: the loop ends, 9 steps long");
        for (i = 0; i < 2u; i++)
            mix_block(out);
        for (i = 0, found = 0; i < SEQ_HOLD; i++)
            found |= sq_note[0][i] == n0;
        EXPECT(found, "its first step played at once (it had gone by)");
        while (seq_gcount != 13u)
            mix_block(out);
        for (i = 0, found = 0; i < SEQ_HOLD; i++)
            found |= sq_note[0][i] == n2;
        EXPECT(found, "its fifth step plays, quantized");
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
        btn(B_GLO, 0);
        {   /* MANGLE: SLICE gates the track in time; DESTROY breaks its samples; off, the track as it was */
            int32_t out[2 * CTL];
            uint32_t i, kind;
            double e[3];
            for (kind = 0; kind < 3u; kind++) {
                sound_t *sn = &ui.snd[0][ui.eng[0]];
                int32_t gmin = Q15, gmax = 0;
                memset(sn->mg, 0, NM);
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
        for (i = 0; i < fr; i++) {
            uint32_t j;
            ui_publish();
            mix_block(out);
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
        for (t = 0; t < NTRACK; t++)
            printf(" %s", RO_NAME[phrase_role(&ui.snd[t][ui.eng[t]])]);
        printf("\n");
        if (pk < 1000)
            fails_++;
    }
    return fails_ ? 1 : 0;
}

/* host od: OVERDRIVE's level, out over in (RMS), at 0, 10 .. 100, on a few signals (OD_MK, voice.c, from it) */
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
            od_cook(&c, (int32_t)a);
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

int main(int argc, char **argv)
{
    wt_init();
    if (argc >= 3 && !strcmp(argv[1], "projects"))
        return cmd_projects(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "phrases"))
        return cmd_phrases(argv[2], argc >= 4 ? atoi(argv[3]) : 4);
    if (argc >= 2 && !strcmp(argv[1], "sizes")) {
        printf("sound_t %zu ck_t %zu vck_t %zu ftck %zu fdck %zu wtck %zu swck %zu vmem %zu voice %zu prj_body %zu\n",
               sizeof(sound_t), sizeof(ck_t), sizeof(vck_t), sizeof(ftck_t), sizeof(fdck_t), sizeof(wtck_t),
               sizeof(swck_t), sizeof(vmem_t), sizeof(voice_t), sizeof(prj_body_t));
        return 0;
    }
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
