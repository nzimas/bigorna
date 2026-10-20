/* SPDX-License-Identifier: GPL-3.0-only
 * The send effects, after the Digitone II's (manual, 12.2 .. 12.4): CHORUS, then DELAY, then REVERB, each fed
 * by the tracks' sends (mono, after each track's level), each returning in stereo to the mix; the chorus sends on
 * to the delay and the reverb, the delay to the reverb. Their parameters in the Digitone II's units (0 .. 127,
 * bipolar ones about 64) from the main loop: fx_set() cooks them for the audio ISR.
 *
 *   DELAY   TIME (1 .. 128 128th notes at the tempo), X (ping-pong), WID, FDBK, HPF, LPF (in the repeats), REV,
 *           VOL. Two lines at FS / 2, 8-bit mu-law, 2 s each: a ping-pong goes left, right, left, ... A change
 *           of TIME glides (the repeats bend in pitch rather than click).
 *   REVERB  PRE, DEC, FREQ and GAIN (a shelf in the tank: the highs above FREQ lost on each trip as GAIN is
 *           lowered), HPF, LPF (on its input), VOL. A plate after Jon Dattorro, "Effect Design, Part 1" (J.
 *           Audio Eng. Soc. 45(9), 1997), its lengths scaled to 44.1 kHz and stretched a little by DEC.
 *   CHORUS  DPTH, SPD, HPF (on its input), WDTH, DEL, REV, VOL: two taps on one short line, swept by an LFO a
 *           quarter of a cycle apart.
 *   COMP    the master compressor (the Digitone II's, manual 12.5): on the mix after the effects' returns, as the
 *           original's. THRESHOLD (-60 .. 0 dB), ATTACK (0.03 .. 30 ms), RELEASE (10 ms .. 2 s), RATIO (1.5, 2, 3, 4, 6,
 *           8, 16, 20), MAKEUP (0 .. +24 dB), MIX (dry .. wet). Stereo, linked: the level's peaks caught at once and
 *           let fall at RELEASE; the gain coming down at ATTACK, once a block, gliding. Its ATTACK and RELEASE are the
 *           ducks' too (each track's sidechain: tracks.c, dk_* below). Set at the start for pounding drums: -9 dB,
 *           10 ms, 100 ms, 4:1, +3 dB, fully wet. (fx_cp's SC FILTER and SC SOURCE places, kept for sessions'
 *           sake, are not read since build 29.) */

enum { DL_TIME, DL_X, DL_WID, DL_FDBK, DL_HPF, DL_LPF, DL_REV, DL_VOL, NDL };
enum { RV_PRE, RV_DEC, RV_FREQ, RV_GAIN, RV_HPF, RV_LPF, RV_VOL, NRV };
enum { CH_DPTH, CH_SPD, CH_HPF, CH_WDTH, CH_DEL, CH_REV, CH_VOL, NCH };
enum { CP_THR, CP_ATK, CP_REL, CP_RAT, CP_MUP, CP_SCF, CP_SCS, CP_MIX, NCP };
#define NCP_RAT 8u
#define NCP_SCS 7u
static const uint8_t CP_RAT10[NCP_RAT] = {15, 20, 30, 40, 60, 80, 160, 200};   /* (x 10) */
static const char *const CP_RATN[NCP_RAT] = {"1.5", "2", "3", "4", "6", "8", "16", "20"};
static const char *const SC_SRCN[NCP_SCS] = {"OFF", "T1", "T2", "T3", "T4", "T5", "T6"};   /* (a track's SC SOURCE) */

static volatile int32_t fx_bpm10 = 1200;         /* the tempo, x 10 (the delay's time follows it) */

/* --------------------------------------------------------------- mu-law --- */
static inline int32_t mu_dec(uint8_t b)
{
    int32_t m = MULAW_DEC[b & 0x7Fu];
    return b & 0x80u ? -m : m;
}
AINL uint8_t mu_enc(int32_t x)
{
    uint32_t a = (uint32_t)(x < 0 ? -x : x);
    if (a < 4u)
        return 0;
    if (a > 32767u)
        a = 32767u;
    return (uint8_t)((x < 0 ? 0x80u : 0u) | MULAW_ENC[a >> 4]);
}

/* ---------------------------------------------------------------- delay --- */
#define DL_N 44100u                     /* 2 s at FS / 2 */
static uint8_t dl_line[2][DL_N] POOL;
static struct {
    uint32_t w;
    int32_t t;                          /* the delay now, Q8 samples at FS / 2 (gliding to tw) */
    int32_t hp[2], lp[2], hpx[2];       /* the repeats' filters, each line */
    int32_t prev[2];                    /* the last output, each side (drawn out to the full rate) */
} dl;
static volatile struct {
    int32_t tw, fb, hp, lp, rev, vol, pl, pr;   /* Q8 samples; Q15 each (pl, pr: the sides' shares) */
    uint8_t x;
} dlc;

static uint32_t dl_samples8(int32_t time)       /* TIME (0 .. 127 = 1 .. 128 128th notes) at the tempo, Q8 at FS / 2 */
{
    /* a 128th note is 60 / bpm / 32 s: 22050 x 600 / (32 bpm10) samples */
    uint32_t n = (uint32_t)(clampi(time, 0, 127) + 1) * 22050u * 600u / (uint32_t)clampi(fx_bpm10, 300, 3000) * 8u;   /* (x 32 x 8) */
    return n > (DL_N - 4u) << 8 ? (DL_N - 4u) << 8 : n < 512u ? 512u : n;
}
static void dl_cook(const uint8_t *p)
{
    int32_t w = clampi(p[DL_WID], 0, 127) - 64;
    dlc.tw = (int32_t)dl_samples8(p[DL_TIME]);
    dlc.fb = clampi(p[DL_FDBK], 0, 127) * Q15 / 128;             /* v / 128, as the original's DSP (held by a soft
                                                                   * clip) */
    dlc.hp = p[DL_HPF] ? lp1_coef(hz_of_oct(bw_oct(p[DL_HPF]))) : 0;   /* (one-poles, as the base-width filter's; the
                                                                     * low-pass HPF + LPF up) */
    dlc.lp = p[DL_HPF] + p[DL_LPF] < 127 ? lp1_coef(hz_of_oct(bw_oct(p[DL_HPF] + p[DL_LPF]))) : Q15;
    dlc.rev = clampi(p[DL_REV], 0, 127) * clampi(p[DL_REV], 0, 127) * Q15 / 16129;   /* (sends: v^2) */
    dlc.vol = clampi(p[DL_VOL], 0, 127) * clampi(p[DL_VOL], 0, 127) * 2 * Q15 / 16129;   /* (returns: 2 v^2) */
    dlc.x = (uint8_t)(p[DL_X] != 0);
    {   /* X off: WID places the repeats (equal power); X on: how far apart the two sides are */
        uint32_t ph = (uint32_t)(w + 64) * (0x40000000u / 127u);
        if (dlc.x) {
            dlc.pl = Q15 / 2 + w * (Q15 / 2) / 64;
            dlc.pr = Q15 / 2 - w * (Q15 / 2) / 64;
        } else {
            dlc.pl = sin_ph(ph + 0x40000000u);
            dlc.pr = sin_ph(ph);
        }
    }
}
static void dl_init(void)
{
    memset(dl_line, 0, sizeof dl_line);
    memset(&dl, 0, sizeof dl);
}
AINL int32_t dl_rd(const uint8_t *ln, uint32_t p8)
{
    uint32_t i0 = (p8 >> 8) % DL_N, i1 = i0 + 1u >= DL_N ? 0u : i0 + 1u;
    int32_t a = mu_dec(ln[i0]);
    return a + (((mu_dec(ln[i1]) - a) * (int32_t)(p8 & 255u)) >> 8);
}
/* a block: in (mono) into the lines; the repeats added to l and r and into rvin */
FAST static void dl_block(const int32_t *in, int32_t *l, int32_t *r, int32_t *rvin)
{
    uint32_t i, k;
    int32_t fb = dlc.fb, hp = dlc.hp, lp = dlc.lp, vol = dlc.vol, rev = dlc.rev, tw = dlc.tw, pl = dlc.pl, pr = dlc.pr;
    for (i = 0; i + 1u < CTL; i += 2u) {
        int32_t x = (in[i] + in[i + 1u]) >> 1, y[2], o[2];
        uint32_t p;
        dl.t += (tw - dl.t) >> 10;
        p = (dl.w << 8) + (DL_N << 8) - (uint32_t)dl.t;
        for (k = 0; k < 2u; k++) {
            int32_t e = dl_rd(dl_line[k], p);
            e = lp8(&dl.lp[k], e, lp);                   /* the repeats darker .. */
            if (hp)                                      /* .. and thinner */
                e -= lp8(&dl.hp[k], e, hp);
            y[k] = e;
        }
        if (dlc.x) {                                     /* ping-pong: in to the left, each side into the other */
            dl_line[0][dl.w] = mu_enc(tanh_q15(x + mulq15(y[1], fb)));
            dl_line[1][dl.w] = mu_enc(tanh_q15(mulq15(y[0], fb)));
        } else {
            dl_line[0][dl.w] = mu_enc(tanh_q15(x + mulq15(y[0], fb)));
            dl_line[1][dl.w] = 0;
        }
        if (++dl.w >= DL_N)
            dl.w = 0;
        if (dlc.x) {
            o[0] = mulq15(mulq15(y[0], pl) + mulq15(y[1], Q15 - pl), vol);
            o[1] = mulq15(mulq15(y[1], Q15 - pr) + mulq15(y[0], pr), vol);
        } else {
            o[0] = mulq15(mulq15(y[0], pl), vol);
            o[1] = mulq15(mulq15(y[0], pr), vol);
        }
        for (k = 0; k < 2u; k++) {                       /* (to the full rate: half-way, then the sample) */
            int32_t h = (dl.prev[k] + o[k]) >> 1;
            int32_t *d = k ? r : l;
            d[i] += h;
            d[i + 1u] += o[k];
            dl.prev[k] = o[k];
        }
        rvin[i] += mulq15((o[0] + o[1]) >> 1, rev);
        rvin[i + 1u] += mulq15((o[0] + o[1]) >> 1, rev);
    }
}

/* --------------------------------------------------------------- chorus --- */
#define CH_N 2048u
static int16_t ch_line[CH_N];            /* (zeroed by fx_init; the RAM beside the code is the audio's code's now) */
static struct { uint32_t w, lfo; int32_t hp; } ch;
static volatile struct { int32_t depth8, inc, hp, wdth, del, rev, vol; } chc;
static void ch_cook(const uint8_t *p)
{
    chc.depth8 = clampi(p[CH_DPTH], 0, 127) * 441 * 256 / 127;   /* the sweep's reach: up to 10 ms, Q8 */
    chc.inc = (int32_t)((uint32_t)(28 + clampi(p[CH_SPD], 0, 127) * 282 / 127) * 974u);   /* 0.28 .. 3.1 Hz */
    chc.hp = lp1_coef(40 + clampi(p[CH_HPF], 0, 127) * clampi(p[CH_HPF], 0, 127) * 960 / 16129);   /* 40 Hz .. 1 kHz */
    chc.wdth = clampi(p[CH_WDTH], 0, 127) * Q15 / 127;
    chc.del = clampi(p[CH_DEL], 0, 127) * clampi(p[CH_DEL], 0, 127) * Q15 / 16129;
    chc.rev = clampi(p[CH_REV], 0, 127) * clampi(p[CH_REV], 0, 127) * Q15 / 16129;
    chc.vol = clampi(p[CH_VOL], 0, 127) * clampi(p[CH_VOL], 0, 127) * 2 * Q15 / 16129;
}
AINL int32_t ch_tap(uint32_t d8)
{
    uint32_t p = (ch.w << 8) + (CH_N << 8) - d8, i0 = (p >> 8) & (CH_N - 1u);
    int32_t a = ch_line[i0], b = ch_line[(i0 + 1u) & (CH_N - 1u)];   /* (towards the newer sample: the fraction's
                                                                     * direction -- the other way it jumps as it wraps,
                                                                     * a crackle under the sweep) */
    return a + (((b - a) * (int32_t)(p & 255u)) >> 8);
}
FAST static void ch_block(const int32_t *in, int32_t *l, int32_t *r, int32_t *dlin, int32_t *rvin)
{
    uint32_t i, base = 6u * 441u * 256u / 10u;            /* 6 ms */
    int32_t d = chc.depth8, w = chc.wdth, vol = chc.vol;
    if (!vol && !chc.del && !chc.rev)
        return;
    for (i = 0; i < CTL;) {
        REP4({
            int32_t x = in[i], a, b, m, s;
            if (chc.hp)
                x -= lp8(&ch.hp, x, chc.hp);
            ch_line[ch.w] = (int16_t)clampi(x, -32767, 32767);
            ch.lfo += (uint32_t)chc.inc;
            a = ch_tap(base + (uint32_t)mulq15(sinr(ch.lfo) + Q15, d) / 2u + 256u);
            b = ch_tap(base + (uint32_t)mulq15(sinr(ch.lfo + 0x40000000u) + Q15, d) / 2u + 256u);
            ch.w = (ch.w + 1u) & (CH_N - 1u);
            m = (a + b) >> 1;
            s = (a - b) >> 1;
            a = m + mulq15(s, w);                             /* WDTH: from both in the middle to apart */
            b = m - mulq15(s, w);
            l[i] += mulq15(a, vol);
            r[i] += mulq15(b, vol);
            dlin[i] += mulq15(m, chc.del);
            rvin[i] += mulq15(m, chc.rev);
        })
    }
}

/* --------------------------------------------------------------- reverb --- */
#define RV_F 24275               /* 22050 / 29761, Q15: the tank runs at half the rate (build 31: half the time) */
#define RV_SMAX 5120             /* the lines' largest stretch, Q12 (1.25) */
#define RV_EXC 12                /* the modulated allpasses' excursion, samples (at 22.05 kHz) */
#define RV_PREMAX 7440u          /* PRE up to 337 ms (at 22.05 kHz) */
static const uint16_t RV_DIF[4] = {142, 107, 379, 277};
static const uint16_t RV_TANK[8] = {672, 4453, 1800, 3720, 908, 4217, 2656, 3163};
static const int16_t RV_DIFG[4] = {24576, 24576, 20480, 20480};
static const uint8_t RV_TAPL_LINE[7] = {5, 5, 6, 7, 1, 2, 3};
static const uint16_t RV_TAPL_OFF[7] = {266, 2974, 1913, 1996, 1990, 187, 1066};
static const uint8_t RV_TAPR_LINE[7] = {1, 1, 2, 3, 5, 6, 7};
static const uint16_t RV_TAPR_OFF[7] = {353, 3627, 1228, 2673, 2111, 335, 121};
static const int8_t RV_TAP_SIGN[7] = {1, 1, -1, 1, -1, -1, -1};

typedef struct { int16_t *buf; uint32_t len, w; } rv_line;
#define RV_MEM 57300u
static int16_t rv_mem[RV_MEM] POOL;
static struct {
    rv_line pre, dif[4], tank[8];
    uint32_t tank_base[8], tank_total;
    int32_t ihp, ilp, shelf_l, shelf_r, fb_l, fb_r;
    uint32_t lfo, lfo2;
    int32_t scale, pyl, pyr;     /* (pyl, pyr: the tank's last output, for the step back up to 44.1 kHz) */
    int ok;
} rv;
static volatile struct { int32_t scale_t, decay, shelf_a, shelf_g, hp, lp, vol; uint32_t pre; } rvc;

AINL int32_t rvm(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b + (1 << 14)) >> 15); }
AINL void rv_write(rv_line *l, int32_t x)
{
    l->buf[l->w] = (int16_t)clampi(x, -32768, 32767);
    if (++l->w >= l->len)
        l->w = 0;
}
AINL int32_t rv_tap(const rv_line *l, uint32_t d)
{
    uint32_t i = l->w + l->len - 1u - d;
    if (i >= l->len)
        i -= l->len;
    return l->buf[i];
}
AINL int32_t rv_tapf(const rv_line *l, uint32_t d, int32_t frac)
{
    int32_t a = rv_tap(l, d), b = rv_tap(l, d + 1u);
    return a + (((b - a) * frac) >> 8);
}
AINL int32_t rv_allpass(rv_line *l, int32_t x, int32_t g, uint32_t d)
{
    int32_t dl_ = rv_tap(l, d - 1u), w = x + rvm(g, dl_);
    rv_write(l, w);
    return dl_ - rvm(g, w);
}
/* the Digitone II's reverb laws: PRE 0.8 .. 337 ms (37 + 16128 p^2 samples at 48 kHz); DEC a decay time of 0.46 s x
 * 1.0259^v up to 116 (~9 s), then rising to ~46 s at 126, 127 held for ever; FREQ the shelf's coefficient
 * (T_RVSH), GAIN the highs kept a trip (linear); HPF and LPF one-poles on its input (LPF from HPF up); VOL 2 v^2 */
static void rv_cook(const uint8_t *p)
{
    int32_t dec = clampi(p[RV_DEC], 0, 127), v = clampi(p[RV_PRE], 0, 127), e, rt8;
    uint32_t loop_q16;
    rvc.pre = (uint32_t)((37 + v * v * 16128 / 16129) * 147 / 320);   /* (48 kHz samples as 22.05 kHz) */
    rvc.scale_t = 3072 + dec * 2048 / 127;                    /* the lines x 0.75 .. 1.25 with DEC */
    /* the decay time, Q8 seconds: 0.46 x 1.0259^v (2^(v x 0.03695)), past 116 on to 46 s */
    rt8 = dec <= 116 ? (int32_t)((118u * exp2_q16(dec * 2422)) >> 16)
                     : (int32_t)((2336u * exp2_q16((dec - 116) * 15400)) >> 16);
    /* a trip round the tank takes T and loses 60 dB x T / RT over two stages: each 2^(-60 / 6.02 / 2 x T / RT) */
    loop_q16 = (uint32_t)(((uint64_t)rv.tank_total * (uint32_t)rvc.scale_t * 194783u) >> 28);  /* T, Q16 s (2^32 / 22050) */
    e = (int32_t)(((int64_t)loop_q16 * 326565) >> 16);       /* 4.983 T, Q16 */
    e = e * 256 / maxi(rt8, 1);                               /* / RT */
    rvc.decay = dec >= 127 ? 32760 : clampi((int32_t)(exp2_q16(-e) >> 1), 1000, 32700);
    {   /* (the table's one-pole is 44.1 kHz's: at half the rate, the same corner is 1 - (1 - a)^2) */
        int32_t a = T_RVSH[clampi(p[RV_FREQ], 0, 127)];
        rvc.shelf_a = clampi(2 * a - a * a / Q15, 0, Q15);
    }
    rvc.shelf_g = clampi(p[RV_GAIN], 0, 127) * Q15 / 127;
    rvc.hp = p[RV_HPF] ? lp1_coef(hz_of_oct(bw_oct(p[RV_HPF]))) : 0;
    rvc.lp = p[RV_HPF] + p[RV_LPF] < 127 ? lp1_coef(hz_of_oct(bw_oct(p[RV_HPF] + p[RV_LPF]))) : Q15;
    rvc.vol = clampi(p[RV_VOL], 0, 127) * clampi(p[RV_VOL], 0, 127) * 2 * Q15 / 16129;
}
static void rv_init(void)
{
    uint32_t k, at = 0;
    memset(&rv, 0, sizeof rv);
#define RV_ALLOC(ln, n) do { (ln)->buf = rv_mem + at; (ln)->len = (n); (ln)->w = 0; at += (n); } while (0)
    RV_ALLOC(&rv.pre, RV_PREMAX + 2u);
    for (k = 0; k < 4u; k++)
        RV_ALLOC(&rv.dif[k], (uint32_t)RV_DIF[k] * RV_F / 32768u + 2u);
    for (k = 0; k < 8u; k++) {
        rv.tank_base[k] = (uint32_t)RV_TANK[k] * RV_F / 32768u;
        rv.tank_total += rv.tank_base[k];
        RV_ALLOC(&rv.tank[k], rv.tank_base[k] * RV_SMAX / 4096u + RV_EXC + 4u);
    }
#undef RV_ALLOC
    rv.ok = at <= RV_MEM;
    memset(rv_mem, 0, sizeof rv_mem);
    rv.scale = 4096;
}
FAST static void rv_block(const int32_t *in, int32_t *ol, int32_t *or_)
{
    uint32_t k, d[8], dl_[7], dr[7], pre = rvc.pre;
    int32_t dec = rvc.decay, sa = rvc.shelf_a, sg = rvc.shelf_g, vol = rvc.vol, s;
    if (!rv.ok || !vol)
        return;
    rv.scale += (rvc.scale_t - rv.scale) >> 6;
    s = rv.scale;
    for (k = 0; k < 8u; k++) {
        d[k] = rv.tank_base[k] * (uint32_t)s >> 12;
        if (d[k] + RV_EXC + 2u >= rv.tank[k].len)
            d[k] = rv.tank[k].len - RV_EXC - 3u;
    }
    for (k = 0; k < 7u; k++) {
        dl_[k] = (uint32_t)RV_TAPL_OFF[k] * RV_F / 32768u * (uint32_t)s >> 12;
        dr[k] = (uint32_t)RV_TAPR_OFF[k] * RV_F / 32768u * (uint32_t)s >> 12;
        if (dl_[k] >= d[RV_TAPL_LINE[k]])
            dl_[k] = d[RV_TAPL_LINE[k]] - 1u;
        if (dr[k] >= d[RV_TAPR_LINE[k]])
            dr[k] = d[RV_TAPR_LINE[k]] - 1u;
    }
    {   /* the tank at 22.05 kHz: two input samples a step (each filtered at 44.1 kHz, then their mean), the output
         * back up between steps; the LFOs at the block's ends, a line between */
        uint32_t lf0 = rv.lfo, lg0 = rv.lfo2, j;
        int32_t e1a, e1b, e2a, e2b;
        rv.lfo += 97400u * (CTL / 2u);
        rv.lfo2 += 123800u * (CTL / 2u);
        e1a = (int32_t)(((uint32_t)(sinr(lf0) + Q15) * RV_EXC) >> 4);   /* (excursion, samples Q12) */
        e1b = (int32_t)(((uint32_t)(sinr(rv.lfo) + Q15) * RV_EXC) >> 4);
        e2a = (int32_t)(((uint32_t)(sinr(lg0 + 0x40000000u) + Q15) * RV_EXC) >> 4);
        e2b = (int32_t)(((uint32_t)(sinr(rv.lfo2 + 0x40000000u) + Q15) * RV_EXC) >> 4);
        for (j = 0; j < CTL / 2u; j++) {
            int32_t x, x2, a, b, yl = 0, yr = 0;
            uint32_t e1, e2;
            e1 = (uint32_t)(e1a + (e1b - e1a) * (int32_t)(j + 1u) / (int32_t)(CTL / 2u)) << 4;   /* (Q16, as before) */
            e2 = (uint32_t)(e2a + (e2b - e2a) * (int32_t)(j + 1u) / (int32_t)(CTL / 2u)) << 4;
            x = in[2u * j] >> 1;                          /* the input: HPF, LPF (at 44.1 kHz), PRE, the diffusers */
            x2 = in[2u * j + 1u] >> 1;
            if (rvc.hp) {
                x -= lp8(&rv.ihp, x, rvc.hp);
                x2 -= lp8(&rv.ihp, x2, rvc.hp);
            }
            x = lp8(&rv.ilp, x, rvc.lp);
            x2 = lp8(&rv.ilp, x2, rvc.lp);
            rv_write(&rv.pre, (x + x2) >> 1);
            x = rv_tap(&rv.pre, pre);
            for (k = 0; k < 4u; k++)
                x = rv_allpass(&rv.dif[k], x, RV_DIFG[k], rv.dif[k].len - 2u);
            a = x + rvm(rv.fb_r, dec);
            {
                rv_line *l = &rv.tank[0];
                uint32_t dd = d[0] - RV_EXC + (e1 >> 16);
                int32_t dly = rv_tapf(l, dd, (int32_t)((e1 >> 8) & 255u)), w = a + rvm(-22938, dly);
                rv_write(l, w);
                a = dly - rvm(-22938, w);
            }
            rv_write(&rv.tank[1], a);
            a = rv_tap(&rv.tank[1], d[1]);
            rv.shelf_l += rvm(a - rv.shelf_l, sa);        /* the shelf: below FREQ kept, above it x GAIN */
            a = rvm(rv.shelf_l + rvm(a - rv.shelf_l, sg), dec);
            a = rv_allpass(&rv.tank[2], a, 16384, d[2]);
            rv_write(&rv.tank[3], a);
            b = rv_tap(&rv.tank[3], d[3]);
            a = x + rvm(rv.fb_l, dec);
            rv.fb_l = b;
            {
                rv_line *l = &rv.tank[4];
                uint32_t dd = d[4] - RV_EXC + (e2 >> 16);
                int32_t dly = rv_tapf(l, dd, (int32_t)((e2 >> 8) & 255u)), w = a + rvm(-22938, dly);
                rv_write(l, w);
                a = dly - rvm(-22938, w);
            }
            rv_write(&rv.tank[5], a);
            a = rv_tap(&rv.tank[5], d[5]);
            rv.shelf_r += rvm(a - rv.shelf_r, sa);
            a = rvm(rv.shelf_r + rvm(a - rv.shelf_r, sg), dec);
            a = rv_allpass(&rv.tank[6], a, 16384, d[6]);
            rv_write(&rv.tank[7], a);
            rv.fb_r = rv_tap(&rv.tank[7], d[7]);
            for (k = 0; k < 7u; k++) {
                int32_t tl = rv_tap(&rv.tank[RV_TAPL_LINE[k]], dl_[k]), tr = rv_tap(&rv.tank[RV_TAPR_LINE[k]], dr[k]);
                yl += RV_TAP_SIGN[k] > 0 ? tl : -tl;
                yr += RV_TAP_SIGN[k] > 0 ? tr : -tr;
            }
            ol[2u * j] += rvm((rv.pyl + yl) >> 1, vol);  /* (back up: between the last step and this) */
            or_[2u * j] += rvm((rv.pyr + yr) >> 1, vol);
            ol[2u * j + 1u] += rvm(yl, vol);
            or_[2u * j + 1u] += rvm(yr, vol);
            rv.pyl = yl;
            rv.pyr = yr;
        }
    }
}

/* ----------------------------------------------------------- compressor --- */
static volatile struct {
    int32_t thr, mup;            /* dB, Q16 */
    int32_t slope;               /* 1 - 1 / ratio, Q16 */
    uint32_t rel;                /* the level's fall, a sample, Q24 (RELEASE: the ducks' too) */
    int32_t ka;                  /* the gain's coming down, a block, Q16 (ATTACK: the ducks' too) */
    int32_t mix;                 /* Q15 */
} cpc;
static struct {
    int32_t env;                 /* the level followed (x 256) */
    int32_t g;                   /* the gain as the last block ended, Q16 */
    int32_t gr;                  /* the gain taken off, dB Q16 */
} cp;
static uint32_t cp_step(uint32_t base8, int32_t oct16)   /* the level's fall: base8 (samples x 256) x 2^oct, Q24 */
{
    uint32_t n8 = (exp2_q16(oct16) >> 12) * (base8 >> 4);
    return 0xFFFFFFFFu / (n8 > 256u ? n8 : 256u);   /* (2^32 / (n x 256) = 2^24 / n) */
}
static void cp_cook(const uint8_t *p)
{
    cpc.thr = -(60 << 16) + clampi(p[CP_THR], 0, 127) * ((60 << 16) / 127);
    cpc.mup = clampi(p[CP_MUP], 0, 127) * ((24 << 16) / 127);
    cpc.slope = Q16 - (int32_t)((uint32_t)Q16 * 10u / CP_RAT10[p[CP_RAT] % NCP_RAT]);
    {   /* ATTACK, 0.03 ms x 1000^(v / 127): the gain's step a block (0.726 ms), 1 - e^-(block / attack) ~ */
        uint32_t us = (30u * exp2_q16(clampi(p[CP_ATK], 0, 127) * 5143)) >> 16;   /* (microseconds) */
        cpc.ka = (int32_t)(us <= 726u ? Q16 : 726u * (uint32_t)Q16 / (us + 363u));
    }
    cpc.rel = cp_step(112896u, clampi(p[CP_REL], 0, 127) * 3945);     /* 10 ms x 200^(v / 127) */
    cpc.mix = clampi(p[CP_MIX], 0, 127) * Q15 / 127;
}
FAST static void cp_block(int32_t *l, int32_t *r)
{
    uint32_t i;
    int32_t g0 = cp.g ? cp.g : Q16, g1, dg, db, gain, gr;
    for (i = 0; i < CTL;) { /* the mix's level: its peaks at once, falling at RELEASE */
        REP4({
            int32_t a = absi((l[i] + r[i]) >> 1) << 8;
            if (a > cp.env)
                cp.env = a;
            else
                cp.env += (int32_t)(((int64_t)(a - cp.env) * (int32_t)cpc.rel) >> 24);
        })
    }
    /* the gain: over the threshold, the ratio's share taken off; the makeup on */
    db = cp.env > 256 ? (log2_q16((uint32_t)cp.env) - (23 << 16)) * 602 / 100 : -(96 << 16);   /* (dBFS, Q16) */
    gr = db > cpc.thr ? (int32_t)(((int64_t)(db - cpc.thr) * cpc.slope) >> 16) : 0;
    cp.gr = gr > cp.gr ? cp.gr + (int32_t)(((int64_t)(gr - cp.gr) * cpc.ka) >> 16) : gr;   /* (down at ATTACK) */
    gain = cpc.mup - cp.gr;
    g1 = (int32_t)exp2_q16(gain * 100 / 602);
    dg = (g1 - g0) / (int32_t)CTL;
    for (i = 0; i < CTL;) {
        REP4({
            int32_t g = g0 + dg * (int32_t)(i + 1u);
            int32_t wl = (int32_t)(((int64_t)l[i] * g) >> 16), wr = (int32_t)(((int64_t)r[i] * g) >> 16);
            l[i] += mulq15(wl - l[i], cpc.mix);
            r[i] += mulq15(wr - r[i], cpc.mix);
        })
    }
    cp.g = g1;
}

/* ---------------------------------------------------------------- ducks --- */
/* a track's hits, for the tracks it ducks (tracks.c): each note the track strikes (the sequencer's, the keys', MIDI's)
 * is a hit; the level follows the track up to its peak for ~30 ms, then falls at RELEASE whatever the track still
 * sounds (a kick's swelling body never ducks again). dk_e: where the level is against its hit's, Q15 (1: struck now) */
typedef struct {
    int32_t env, hit;            /* the level, the hit's (x 256) */
    uint16_t since;              /* blocks since the hit */
} dk_t;
FAST static void dk_feed(dk_t *d, const int32_t *x, int struck)   /* a block of the track's signal; struck: a note began */
{
    uint32_t i;
    if (struck) {
        d->since = 0;
        d->hit = 0;
    }
    for (i = 0; i < CTL;) {
        REP4({
            int32_t a = absi(x[i]) << 8;
            if (d->since < 40u && a > d->hit)       /* (the hit: its peak, over its first ~30 ms) */
                d->hit = a;
            if (a > d->env)                          /* (its level after: followed, falling no faster than RELEASE) */
                d->env = a;
            else
                d->env -= (int32_t)(((int64_t)d->env * (int32_t)cpc.rel) >> 24);
        })
    }
    if (d->since < 0xFFFFu)
        d->since++;
}
/* how far the duck goes now, Q15: all of it at the hit's peak, none 12 dB below it (in dB between) */
FAST static int32_t dk_e(const dk_t *d)
{
    int32_t r;
    if (d->hit < 256 || d->env < 256)
        return 0;
    r = log2_q16((uint32_t)d->env) - log2_q16((uint32_t)d->hit);   /* (octaves of 6 dB, Q16; 0 at the peak) */
    return clampi(Q15 + r / 4, 0, Q15);          /* (2 octaves, 12 dB, Q16: 2^17 -> 2^15) */
}

/* --------------------------------------------------------- the interface --- */
static uint8_t fx_dl[NDL], fx_rv[NRV], fx_ch[NCH], fx_cp[NCP];   /* (the main loop's) */
static const uint8_t CP0[NCP] = {107, 107, 55, 3, 16, 14, 0, 127};   /* (pounding drums: see above) */
static void fx_cook_all(void)
{
    dl_cook(fx_dl);
    rv_cook(fx_rv);
    ch_cook(fx_ch);
    cp_cook(fx_cp);
}
static void fx_init(void)
{
    static const uint8_t DL0[NDL] = {31, 0, 64, 50, 0, 100, 0, 90};      /* a dotted 16th .. (TIME 32: a quarter) */
    static const uint8_t RV0[NRV] = {8, 80, 90, 80, 10, 110, 80};
    static const uint8_t CH0[NCH] = {40, 30, 10, 100, 0, 30, 90};
    memcpy(fx_dl, DL0, NDL);
    memcpy(fx_rv, RV0, NRV);
    memcpy(fx_ch, CH0, NCH);
    memcpy(fx_cp, CP0, NCP);
    memset(&cp, 0, sizeof cp);
    rv_init();
    dl_init();
    memset(ch_line, 0, sizeof ch_line);
    memset(&ch, 0, sizeof ch);
    fx_cook_all();
    rv.scale = rvc.scale_t;
    dl.t = dlc.tw;
}
