/* SPDX-License-Identifier: GPL-3.0-only
 * What every voice does after its machine, in the Digitone II's order:
 *
 *   machine -> [SRR: PRE] -> [OVERDRIVE: PRE] -> [base-width: PRE] -> FLTR machine -> [base-width: POST]
 *           -> [SRR: POST] -> [OVERDRIVE: POST] -> BIT REDUCTION -> AMP envelope x VOL
 *
 * The FLTR machines: MULTI-MODE (a 12 dB state-variable filter morphing low-pass -> band-pass -> high-pass),
 * LOWPASS 4 (a 24 dB ladder, its feedback through a soft clip), LEGACY LP/HP (12 dB, low- or high-pass), COMB-
 * and COMB+ (a delay tuned to FREQ, negative or positive feedback through a low-pass), EQUALIZER (a bell). Each
 * has its own envelope (delay, A D S R) moving its frequency by ENV, and follows the note by KEY.T.
 * The base-width filter: a high-pass at BASE and a low-pass WDTH above it (BASE 0 and WDTH 127: no effect).
 * The AMP envelope: AHD (HOLD fixed, or NOTE: until the key is let go) or ADSR.
 *
 * The main loop cooks a track's FLTR, AMP and FX pages into vck_t (times as per-block steps, frequencies as
 * log2 Hz); the audio ISR runs the chain a block at a time: the envelopes and the filter's coefficients once a
 * block, the rest per sample. */

/* ---------------------------------------------------------- the curves --- */
/* The Digitone II's curves (docs/dt2-curves.md), as tools/gen_tables.py fits them: T_* are times in us for each value
 * 0 .. 127 (0xFFFFFFFF: none, INF), a block is 32 samples, 726 us */
#define T_INF 0xFFFFFFFFu
static int32_t us_blocks(uint32_t us) { return us == T_INF ? 0x7FFFFFFF : maxi(1, (int32_t)((us + 363u) / 726u)); }
/* FREQ 0 .. 127: log2 Hz Q16, 5 Hz .. 22 kHz exponentially (5 x 4400^(v / 127)) */
static int32_t freq_oct(int32_t v) { return 152170 + clampi(v, 0, 127) * 6246; }
#define FREQ_SPAN 793215         /* the 12.1 octaves of FREQ, Q16: the ENV depth's reach at full */
/* the base-width filter's frequencies: 5 x 8000^(v / 127) Hz, log2 Q16 */
static int32_t bw_oct(int32_t v) { return 152170 + clampi(v, 0, 127) * 6691; }
/* the decay towards a level over T blocks: the factor a block that falls 60 dB in T, Q16 */
static int32_t fall60(int32_t blocks) { return (int32_t)exp2_q16(-(653137 / maxi(1, blocks))); }   /* 2^(-9.966 / T) */
static int32_t fall60_us(uint32_t us) { return us == T_INF ? Q16 : fall60(us_blocks(us)); }

/* the base-width filter (and the machines' noise bands): a one-pole high-pass at BASE, a one-pole low-pass at BASE +
 * WDTH (BASE 0: no high-pass; BASE + WDTH at 127 or more: no low-pass) */
typedef struct { int32_t hp, lp; } bwc_t;
static void bw_cook(bwc_t *c, int32_t base, int32_t width)
{
    int32_t top = clampi(base, 0, 127) + clampi(width, 0, 127);
    c->hp = base > 0 ? lp1_coef(hz_of_oct(bw_oct(base))) : 0;
    c->lp = top < 127 ? lp1_coef(hz_of_oct(bw_oct(top))) : 0;
}
AINL int32_t bw_run(int32_t *sh, int32_t *sl, const bwc_t *c, int32_t x)
{
    if (c->hp)
        x -= lp8(sh, x, c->hp);
    if (c->lp)
        x = lp8(sl, x, c->lp);
    return x;
}

/* ------------------------------------------------------- the envelope --- */
enum { EV_OFFST, EV_DEL, EV_ATT, EV_HOLD, EV_DEC, EV_SUS, EV_REL, EV_DONE };
typedef struct {
    uint16_t del, hold;          /* blocks (hold 0xFFFF: until the note is let go) */
    int32_t att;                 /* the attack's step a block, Q24 */
    int32_t dec, rel;            /* the decay's and release's factor a block, Q16 (dec 0x10000: none) */
    int32_t sus;                 /* the sustain level, Q24 */
    uint8_t adsr;                /* ADSR (else AHD: hold, then decay to nothing) */
} envck_t;
typedef struct { uint8_t st; uint16_t n; int32_t lv; } env_t;

/* the envelope from its times (us; hold T_INF: until the note is let go), the sustain 0 .. 127 */
static void env_cook(envck_t *e, uint32_t del, uint32_t a, uint32_t h, uint32_t d, int32_t s, uint32_t r, int adsr)
{
    e->del = (uint16_t)(del ? clampi(us_blocks(del), 0, 0xFFFE) : 0);
    e->att = (1 << 24) / clampi(us_blocks(a), 1, 1 << 23);
    e->hold = (uint16_t)(adsr ? 0 : h == T_INF ? 0xFFFF : h > 363u ? clampi(us_blocks(h), 0, 0xFFFE) : 0);
    e->dec = fall60_us(d);
    e->sus = clampi(s, 0, 127) * ((1 << 24) / 127);
    e->rel = fall60_us(r);
    e->adsr = (uint8_t)(adsr != 0);
}
static void env_gate(env_t *e, const envck_t *c, int reset)   /* a note on */
{
    if (reset || e->st == EV_OFFST || e->st == EV_DONE)
        e->lv = 0;
    e->st = c->del ? EV_DEL : EV_ATT;
    e->n = c->del;
}
static void env_release(env_t *e, const envck_t *c)           /* a note let go */
{
    if (e->st == EV_DONE || e->st == EV_OFFST)
        return;
    if (c->adsr)
        e->st = EV_REL;
    else if (c->hold == 0xFFFF && e->st == EV_HOLD)
        e->st = EV_DEC;                                       /* (AHD, HOLD NOTE: the decay from here; an attack
                                                               * still rising decays from its top) */
}
/* a block on; returns the level, Q24 */
static int32_t env_step(env_t *e, const envck_t *c, int gate)
{
    switch (e->st) {
    case EV_DEL:
        if (e->n)
            e->n--;
        if (!e->n)
            e->st = EV_ATT;
        break;
    case EV_ATT:
        e->lv += c->att;
        if (e->lv >= 1 << 24) {
            e->lv = 1 << 24;
            if (c->adsr)
                e->st = EV_DEC;
            else if (c->hold == 0xFFFF && !gate)
                e->st = EV_DEC;
            else {
                e->st = EV_HOLD;
                e->n = c->hold;
            }
        }
        break;
    case EV_HOLD:
        if (c->hold != 0xFFFF) {
            if (e->n)
                e->n--;
            if (!e->n)
                e->st = EV_DEC;
        } else if (!gate)
            e->st = EV_DEC;
        break;
    case EV_DEC:
        if (c->adsr) {
            e->lv = c->sus + (int32_t)(((int64_t)(e->lv - c->sus) * c->dec) >> 16);
            if (c->dec < Q16 && absi(e->lv - c->sus) < 256)
                e->st = EV_SUS;
        } else {
            e->lv = (int32_t)(((int64_t)e->lv * c->dec) >> 16);
            if (e->lv < (1 << 24) / 1000) {
                e->lv = 0;
                e->st = EV_DONE;
            }
        }
        break;
    case EV_SUS:
        e->lv = c->sus;
        break;
    case EV_REL:
        e->lv = (int32_t)(((int64_t)e->lv * c->rel) >> 16);
        if (e->lv < (1 << 24) / 1000) {
            e->lv = 0;
            e->st = EV_DONE;
        }
        break;
    default:
        break;
    }
    return e->lv;
}

/* ------------------------------------------------------ the cooked page --- */
AINL int32_t od_asym(int32_t u)  /* OVERDRIVE's second stage: tanh, clipping at 5/8 below */
{
    return u >= 0 ? tanh_q15(u) : (tanh_q15((u * 8) / 5) * 5) / 8;
}
#define COMB_N 1024u             /* a voice's comb line: down to ~43 Hz */
typedef struct {
    uint8_t fmach, bw_on, bw_pre, od_post, frst, arst, leg_hp;
    int32_t f_oct;               /* the cutoff (or the bell's frequency), log2 Hz Q16 */
    int32_t f_v;                 /* FREQ as set (the combs': a note number) */
    int32_t f_q;                 /* the resonance, Q12 */
    int32_t mm;                  /* MULTI-MODE: low-pass 0 .. band-pass Q15 .. high-pass 2 Q15 */
    int32_t f_env;               /* the envelope's depth, -1 .. 1 of FREQ's span, Q16 */
    int32_t f_key;               /* the key tracking: octaves of cutoff an octave of note (from note 48), Q16 */
    int32_t lp4_k;               /* LOWPASS 4's feedback, Q12 (0 .. 4) */
    int32_t cb_fb, cb_lpv;       /* the combs: the feedback (signed, Q15), the low-pass in it (LPF, x / 127 Q15) */
    int32_t eq_g, eq_k, eq_q;    /* the bell: A^2 - 1 (Q12), 1 / Q (Q12), Q (Q12) */
    bwc_t bw;                    /* the base-width filter */
    envck_t fe, ae;              /* the FLTR and AMP envelopes */
    int32_t br_bits;             /* BIT REDUCTION: the bits x 256, 0 off */
    int32_t od_g;                /* OVERDRIVE: the amount, 0 .. 100 (x 256), 0 off; then its stages' (vc_od) */
    int32_t od_g1, od_b, od_fb, od_g2, od_beta, od_tone, od_w, od_mk;
    uint32_t srr_inc;            /* SRR: the hold's phase step a sample, 0 off */
    int32_t vol;                 /* VOL, Q15 */
} vck_t;

/* OVERDRIVE, after TONITOR's distortion (its layers.c): two gain stages in series as in a valve preamp, the first
 * symmetric (odd harmonics), the second clipping sooner on one side (even harmonics) -- at the sample rate (TONITOR's
 * runs at twice it; on a voice, short of time, the cost halved); the highs lifted 6 dB before them and lowered after
 * (the mids and highs distort, the bass less: less mud); a DC blocker, and a low-pass coming down from 16 kHz to
 * 6.5 kHz as the amount goes up. The amount (0 .. 100) sets the drive into the first stage (x 0.5 .. x 32), the
 * second's gain (x 1 .. x 2.5) and bias, and the first's bias as a share (0.15 .. 0.35) of the input's own peak, so
 * the clipped wave stays lopsided however hard it is driven; below 6 it fades in. TONITOR brings the level back by
 * following it (~0.2 s): on a voice that would pump a drum's hit, so here the level comes back by a curve of the
 * amount, measured (host od) */
static const uint16_t OD_MK[11] = {4096, 5362, 3674, 2831, 2425, 2237, 2155, 2122, 2117, 2127, 2147};   /* Q12, a 10 */
static void od_cook(vck_t *c, int32_t a)
{
    int32_t amt = a * 256;
    if (a < 2)
        a = 0;                                    /* (1: not heard, and not worth its cost) */
    amt = a * 256;
    c->od_g = amt;
    if (!a)
        return;
    c->od_g1 = (int32_t)((2048u * (uint64_t)exp2_q16(amt * 6 / 100 * 256)) >> 16);   /* x 0.5 .. x 32, Q12 */
    c->od_b = 3277 + amt * 6554 / 25600;          /* the second stage's bias, 0.1 .. 0.3 */
    c->od_fb = od_asym(c->od_b);
    c->od_g2 = 4096 + amt * 6144 / 25600;         /* x 1 .. x 2.5, Q12 */
    c->od_beta = 4915 + amt * 6554 / 25600;       /* the first stage's bias: 0.15 .. 0.35 of the input's peak */
    c->od_tone = 29430 - amt * (29430 - 19800) / 25600;   /* 16 kHz .. 6.5 kHz */
    c->od_w = a >= 6 ? Q15 : amt * Q15 / (6 * 256);
    c->od_mk = OD_MK[a / 10] + (a < 100 ? (OD_MK[a / 10 + 1] - OD_MK[a / 10]) * (a % 10) / 10 : 0);
}
static void vck_cook(vck_t *c, const sound_t *s)
{
    const uint8_t *f = s->flt, *a = s->amp, *x = s->fx;
    int32_t v, r = clampi(f[F_RESO], 0, 127);
    memset(c, 0, sizeof *c);
    c->fmach = s->fmach;
    c->f_v = clampi(f[F_FREQ], 0, 127);
    c->f_oct = freq_oct(f[F_FREQ]);
    /* the resonance: MULTI-MODE Q = (1 + 40 x^2) / 2 (0.5 .. 20.5); LEGACY (1 + 43.7 x^2) / 2; LOWPASS 4's feedback
     * 3.5 x^2 */
    c->f_q = s->fmach == FLT_LEG ? 2048 + r * r * 5549 / 1000 : 2048 + r * r * 5079 / 1000;
    c->mm = clampi(f[F_TYPE], 0, 127) * 2 * Q15 / 127;
    c->leg_hp = (uint8_t)(f[F_TYPE] != 0);
    c->f_env = (clampi(f[F_ENV], 0, 127) - 64) * 1024;      /* -1 .. 1 (Q16) */
    c->f_key = clampi(f[F_KEYT], 0, 127) * 65536 / 127;
    c->lp4_k = r * r * 889 / 1000;
    c->cb_fb = (s->fmach == FLT_CBM ? -1 : 1) * (r * 32440 / 127);   /* up to 0.99 */
    c->cb_lpv = clampi(f[F_TYPE], 0, 127) * Q15 / 127;      /* (LPF on knob G) */
    {   /* the bell: GAIN +-18 dB on knob F, Q 0.3 .. 12 on knob G */
        int32_t db16 = (r - 64) * (18 << 16) / 64, q;
        c->eq_g = (int32_t)(exp2_q16(db16 / 10000 * 1661 + (db16 % 10000) * 1661 / 10000) >> 4) - 4096;   /* A^2 - 1 =
                                                                                     * 10^(dB / 20) - 1 */
        q = (int32_t)((1229u * exp2_q16(clampi(f[F_TYPE], 0, 127) * 5 * 65536 / 127 + 2 * 65536 / 10)) >> 16);
        c->eq_q = maxi(q, 64);
        c->eq_k = (int32_t)(4096u * 4096u / (uint32_t)c->eq_q);
    }
    bw_cook(&c->bw, f[F_BASE], f[F_WDTH]);
    c->bw_on = (uint8_t)(c->bw.hp || c->bw.lp);
    c->bw_pre = (uint8_t)(f[F_BWRT] == 0);
    /* the FLTR envelope: DEL, and the AMP envelope's attack (the time per sample: see docs/dt2-curves.md 1.2) */
    env_cook(&c->fe, T_FDEL[f[F_DEL] & 127], T_AATK[f[F_ATK] & 127], 0, T_FDEC[f[F_DEC] & 127], f[F_SUS],
             T_FDEC[f[F_REL] & 127], 1);
    c->frst = (uint8_t)(f[F_RSET] != 0);
    /* the AMP envelope: AHD has no sustain, its release the decay's */
    env_cook(&c->ae, 0, T_AATK[a[A_ATK] & 127], a[A_HOLD] >= 127 ? T_INF : T_AHOLD[a[A_HOLD] & 127],
             T_ADEC[a[A_DEC] & 127], a[A_MODE] ? a[A_SUS] : 0, T_ADEC[(a[A_MODE] ? a[A_REL] : a[A_DEC]) & 127], a[A_MODE] != 0);
    c->arst = (uint8_t)(a[A_RSET] != 0);
    c->vol = clampi(a[A_VOL], 0, 127) * Q15 / 127;
    v = clampi(x[X_BR], 0, 127);
    c->br_bits = v ? T_BITS[v] : 0;
    od_cook(c, clampi(x[X_OD], 0, 100));
    v = clampi(x[X_SRR], 0, 127);                     /* each sample held 1 + 100 (v / 127)^2 samples */
    c->srr_inc = v ? (0xFFFFFFFFu / (uint32_t)(256 + v * v * 25600 / 16129)) << 8 : 0;
    c->od_post = (uint8_t)(x[X_ODRT] != 0);
}

/* ------------------------------------------------------- a voice's chain --- */
typedef struct {
    env_t fe, ae;
    svf_t s1, s2;                /* the FLTR machine (MULTI-MODE, LEGACY, the bell: s1; LOWPASS 4: s1 s2 as its four) */
    int32_t bh, bl;              /* the base-width filter */
    int32_t cb_lp;               /* the comb's low-pass */
    uint32_t cb_w;
    uint32_t srr_ph;
    int32_t srr_hold;
    uint8_t gate;
    int32_t od_pre, od_post, od_dc, od_lp, od_env, od_ep, od_ymp, od_yp, od_dryp;   /* OVERDRIVE's (vc_od) */
    int16_t cb[COMB_N];
} vchain_t;

static void vc_on(vchain_t *v, const vck_t *c, int fresh)
{
    if (fresh) {
        memset(&v->s1, 0, sizeof v->s1);
        memset(&v->s2, 0, sizeof v->s2);
        v->bh = v->bl = 0;
        if (c->fmach == FLT_CBM || c->fmach == FLT_CBP)
            memset(v->cb, 0, sizeof v->cb);
        v->cb_lp = 0;
        v->od_pre = v->od_post = v->od_dc = v->od_lp = v->od_env = v->od_ep = v->od_ymp = v->od_yp = v->od_dryp = 0;
        v->ae.st = v->fe.st = EV_OFFST;
    }
    env_gate(&v->ae, &c->ae, c->arst || fresh);
    env_gate(&v->fe, &c->fe, c->frst || fresh);
    v->gate = 1;
}
static void vc_off(vchain_t *v, const vck_t *c)
{
    v->gate = 0;
    env_release(&v->ae, &c->ae);
    env_release(&v->fe, &c->fe);
}

AINL int32_t sat16(int32_t x) { return clampi(x, -32767, 32767); }
static void vc_srr(vchain_t *v, const vck_t *c, int32_t *x)
{
    uint32_t i, inc = c->srr_inc;
    for (i = 0; i < CTL; i++) {
        uint32_t was = v->srr_ph;
        v->srr_ph += inc;
        if (v->srr_ph < was)
            v->srr_hold = x[i];
        x[i] = v->srr_hold;
    }
}
#define OD_EMPH 3110             /* 1 - e^(-2 pi 700 / FS), Q15 */
AINL int32_t od_stage(const vck_t *c, int32_t e)   /* the two stages (e biased) */
{
    int32_t u = tanh_q15((int32_t)(((int64_t)e * c->od_g1) >> 12));
    return od_asym(mulq12(u, c->od_g2) + c->od_b) - c->od_fb;
}
static void vc_od(vchain_t *v, const vck_t *c, int32_t *x)
{
    uint32_t i;
    for (i = 0; i < CTL; i++) {
        int32_t dry = x[i], e, o;
        v->od_pre += mulq15(dry - v->od_pre, OD_EMPH);
        e = 2 * dry - v->od_pre;                 /* the highs +6 dB */
        v->od_env = absi(e) > v->od_env ? absi(e) : v->od_env - (v->od_env >> 12);
        e += mulq15(v->od_env, c->od_beta);      /* (a bias in step with the level: lopsided however hard) */
        o = od_stage(c, e);
        v->od_post += mulq15(o - v->od_post, OD_EMPH);
        o = (o + v->od_post) >> 1;               /* the highs -6 dB again */
        v->od_dc += (((o << 8) - v->od_dc) * 37) >> 15;   /* (~8 Hz) */
        o -= v->od_dc >> 8;
        v->od_lp += mulq15(o - v->od_lp, c->od_tone);
        o = mulq12(v->od_lp, c->od_mk);
        x[i] = dry + mulq15(o - dry, c->od_w);
    }
}
static void vc_bw(vchain_t *v, const vck_t *c, int32_t *x)
{
    uint32_t i;
    for (i = 0; i < CTL; i++)
        x[i] = bw_run(&v->bh, &v->bl, &c->bw, x[i]);
}
/* the FLTR machine, its frequency moved by its envelope (Q24 level) and the note */
static void vc_filter(vchain_t *v, const vck_t *c, int32_t *x, int32_t note16, int32_t elv)
{
    int32_t ef = (int32_t)(((int64_t)c->f_env * elv) >> 24);   /* the envelope's move, of FREQ's span, Q16 */
    int32_t oct = c->f_oct + (int32_t)(((int64_t)ef * FREQ_SPAN) >> 16)
                  + (int32_t)(((int64_t)c->f_key * (note16 / 12 - (4 << 16))) >> 16);   /* (octaves from note 48) */
    uint32_t i;
    oct = clampi(oct, 4 << 16, 936005);                     /* 16 Hz .. 20 kHz */
    switch (c->fmach) {
    case FLT_MM: {
        svf_c k;
        int32_t m = c->mm;
        svf_coef(&k, hz_of_oct(oct), c->f_q);
        for (i = 0; i < CTL; i++) {                          /* (svf_run's step, its three outputs) */
            int32_t v3 = x[i] - v->s1.ic2;
            int32_t v1 = mulr16(k.a1, v->s1.ic1) + mulr24(k.a2, v3);
            int32_t v2 = v->s1.ic2 + mulr24(k.a2, v->s1.ic1) + mulr24(k.a3, v3);
            int32_t hp = x[i] - mulr24(k.k, v1) - v2;
            v->s1.ic1 = 2 * v1 - v->s1.ic1;
            v->s1.ic2 = 2 * v2 - v->s1.ic2;
            x[i] = m < Q15 ? v2 + mulq15(v1 - v2, m) : v1 + mulq15(hp - v1, m - Q15);
        }
        break;
    }
    case FLT_LEG: {
        svf_c k;
        svf_coef(&k, hz_of_oct(oct), c->f_q);
        for (i = 0; i < CTL; i++)
            x[i] = svf_run(&v->s1, &k, x[i], c->leg_hp ? 3 : 1);
        break;
    }
    case FLT_EQ: {
        svf_c k;
        int32_t g = c->eq_g;
        svf_coef(&k, hz_of_oct(oct), c->eq_q);
        for (i = 0; i < CTL; i++) {                          /* x + (A^2 - 1) k bp */
            int32_t bp = svf_run(&v->s1, &k, x[i], 2);
            x[i] += mulq12(mulq12(bp, c->eq_k), g);
        }
        break;
    }
    case FLT_LP4: {   /* a ladder: four TPT one-poles, the feedback from the last output a sample before (the zero-
                       * delay solve cost twice as much, on a voice that is short of time), the input soft-clipped;
                       * the feedback kept below where the sample's delay would let it run away */
        int32_t g = svf_g(hz_of_oct(oct)), G, k = c->lp4_k, comp = 4096 + k / 2, y4 = v->s2.ic2 >> 1;
        int32_t s0 = v->s1.ic1, s1 = v->s1.ic2, s2 = v->s2.ic1, s3 = v->s2.ic2;
        uint32_t den = (uint32_t)((1 << 24) + g) >> 8;
        G = Q16 - (int32_t)(0xFFFFFFFFu / den);              /* g / (1 + g) = 1 - 1 / (1 + g), Q16 */
        k = mulq16(k, Q16 - (G >> 1));                       /* (less of it as the cutoff nears the top) */
        for (i = 0; i < CTL; i++) {
            int32_t u = tanh_q15(mulq12(x[i], comp) - mulq12(y4, k)), vv;
            vv = mulq16(u - s0, G);                          /* v = (u - s) G, y = v + s, s = y + v */
            u = vv + s0;
            s0 = u + vv;
            vv = mulq16(u - s1, G);
            u = vv + s1;
            s1 = u + vv;
            vv = mulq16(u - s2, G);
            u = vv + s2;
            s2 = u + vv;
            vv = mulq16(u - s3, G);
            u = vv + s3;
            s3 = u + vv;
            y4 = u;
            x[i] = u;
        }
        v->s1.ic1 = s0;
        v->s1.ic2 = s1;
        v->s2.ic1 = s2;
        v->s2.ic2 = s3;
        break;
    }
    default: {   /* COMB- / COMB+: a line tuned to FREQ as a note (220 x 2^((n - 69) / 12) Hz: n 127 is 6.3 kHz; the
                  * envelope moves it 127 semitones at full, KEY.T follows the note), the feedback through a low-pass
                  * at 22 kHz x ((LPF / 127)^2 + 0.2 f^2) */
        int32_t n16 = (c->f_v << 16) + ef * 127 + (int32_t)(((int64_t)c->f_key * (note16 - (48 << 16))) >> 16), fr, lp;
        int32_t coct = 133123 + n16 / 12;                        /* log2 220 + (n - 69) / 12, Q16 */
        uint32_t d8, w = v->cb_w;
        int32_t fb = c->cb_fb;
        coct = clampi(coct, 4 << 16, 909000);                 /* (~43 Hz the line's floor .. 15 kHz) */
        fr = clampi(n16 / 127, 0, Q16);                       /* f, 0 .. 1 (Q16) */
        lp = clampi(mulq15(c->cb_lpv, c->cb_lpv) + (int32_t)(((((int64_t)fr * fr) >> 17) * 13107) >> 16), 0, Q15);   /* (+ 0.2 f^2) */
        lp = lp1_coef(mulq15(22000, lp) + 20);
        d8 = (uint32_t)FS * 256u / (uint32_t)maxi(hz_of_oct(coct), 16);   /* the period, Q8 */
        d8 = d8 < 512u ? 512u : d8 > (COMB_N - 2u) << 8 ? (COMB_N - 2u) << 8 : d8;
        for (i = 0; i < CTL; i++) {
            uint32_t p = ((w << 8) - d8) & ((COMB_N << 8) - 1u), i0 = p >> 8, f8 = p & 255u;
            int32_t a = v->cb[i0], b = v->cb[(i0 + 1u) & (COMB_N - 1u)], d = a + (((b - a) * (int32_t)f8) >> 8), y;
            y = x[i] + mulq15(lp8(&v->cb_lp, d, lp), fb);
            v->cb[w] = (int16_t)sat16(y);
            w = (w + 1u) & (COMB_N - 1u);
            x[i] = y >> 1;
        }
        v->cb_w = w;
        break;
    }
    }
}
/* BIT REDUCTION: bits from 16 down to ~1.9; a fractional depth crossfades the two whole ones about it */
AINL int32_t quant(int32_t x, uint32_t st, uint32_t inv)
{
    return (int32_t)(((int64_t)x * inv + 0x80000000LL) >> 32) * (int32_t)st;
}
static void vc_br(const vck_t *c, int32_t *x)
{
    uint32_t i, b = (uint32_t)c->br_bits, bi = b >> 8, st0, st1, inv0, inv1;
    int32_t fr = (int32_t)(b & 255u) << 7;                    /* Q15 */
    if (bi >= 16u)
        return;
    st0 = 1u << (16u - bi);                                   /* the step at bi bits (Q15 full scale is 2^16 wide) */
    st1 = bi < 15u ? st0 >> 1 : 1u;
    inv0 = 0xFFFFFFFFu / st0;
    inv1 = 0xFFFFFFFFu / st1;
    for (i = 0; i < CTL; i++) {
        int32_t a = quant(x[i], st0, inv0);
        x[i] = a + mulq15(quant(x[i], st1, inv1) - a, fr);
    }
}

/* the chain over a block of the machine's output (note16: the note played, semitones Q16; vel 1 .. 127); returns
 * 0 once the AMP envelope has ended */
static int vc_block(vchain_t *v, const vck_t *c, int32_t *x, int32_t note16, int32_t vel)
{
    int32_t a0 = v->ae.lv, a1, fl;
    uint32_t i;
    fl = env_step(&v->fe, &c->fe, v->gate);
    a1 = env_step(&v->ae, &c->ae, v->gate);
    if (c->srr_inc)                              /* (SRR: always before the filter) */
        vc_srr(v, c, x);
    if (c->od_g && !c->od_post)
        vc_od(v, c, x);
    if (c->bw_on && c->bw_pre)
        vc_bw(v, c, x);
    vc_filter(v, c, x, note16, fl);
    if (c->bw_on && !c->bw_pre)
        vc_bw(v, c, x);
    if (c->od_g && c->od_post)
        vc_od(v, c, x);
    if (c->br_bits)
        vc_br(c, x);
    {   /* the AMP envelope (a line across the block), VOL and the note's level */
        int32_t g = mulq15(c->vol, vel * 258), e = a0 >> 9, de = ((a1 >> 9) - e) / (int32_t)CTL;
        for (i = 0; i < CTL; i++) {
            e += de;
            x[i] = mulq15(mulq15(x[i], e), g);
        }
    }
    return v->ae.st != EV_DONE;
}
