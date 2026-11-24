/* SPDX-License-Identifier: GPL-3.0-only
 * MANGLE, a track's (its own page, after FX): on the track's voices summed, before its limiter, set by the sound the
 * track plays and lockable step by step (a lock holds until the track's next step plays, as the sends' do).
 *
 *   DESTROY   destruction (build 112, redone): the amount (0 off), and its kind --
 *               CLIP   (WRAP before build 112) the signal, its mids and highs lifted (+3.5 dB above ~250 Hz),
 *                      driven 0 .. +42 dB into a hard clipper with a rounded knee, a little more one side than the
 *                      other as it goes up (even harmonics; the DC blocker after takes the offset): the
 *                      hardcore / gabber kind of distortion
 *               XOR    the bits under the signal's own top XORed with a fixed pattern, at -33 dB of the signal
 *                      at 1 up to all but the top one at 127, smoothly: a noise that follows the signal, then
 *                      breaks it
 *               FLIP   one bit flipped, from -36 dB of the signal up to its top: a square error riding the wave
 *                      (both measured against the signal's level, a peak follower of ~12 ms -- the same bite on a
 *                      quiet tail as on the hit; under ~-60 dB, nothing: silence stays silent)
 *               DROP   the signal frozen (its sample held) or cut to silence, at random: 6 times a second at 1 to
 *                      ~300 at 127, each up to 2 .. 25 ms long: data errors, stutter, crunch
 *             (fading in over its first eight steps; in the flash, as WRECK)
 *   SLICE     a gate in time with the tempo (the sequencer's steps; stopped, a clock of its own at the tempo): one of
 *             16 patterns (OFF: none) of 16 slices, a slice an 1/8, 1/16, 1/32 or 1/16 triplet; DEPTH how far a closed
 *             slice cuts (127: to silence); EDGE how soft its edges (0: ~0.3 ms, a click; 127: ~30 ms); SHIFT the
 *             pattern turned by slices
 *   WRECK     (build 89, after Quasar's, before the others; its own page) FOLD a triangle wavefolder, the signal
 *             driven x 1 .. x 5 into it; RING a ring modulator crossfaded in, its own sine at RING FRQ (20 Hz .. 5 kHz,
 *             whatever the note). Both at 0: passed by
 * Each costs a few operations a sample; none needs memory beyond a few words a track. */

#define MG_TRACKS 9u             /* (NLANE: tracks.c -- a lane mangled as a track) */

typedef struct {
    int32_t g;                   /* the gate's level as it moves, Q15 */
    int32_t hold;                /* DROP: the sample held (0: a cut), and samples left */
    uint32_t hold_n;
    int32_t env;                 /* XOR, FLIP: the signal's level (a peak follower) */
    int32_t lp;                  /* CLIP: the lows, under ~250 Hz (the rest lifted) */
    uint32_t rnd;
    uint32_t rph;                /* WRECK: RING's phase */
} mangle_t;
static mangle_t mangle[MG_TRACKS];

/* the gate patterns: bit i, slice i (1: open) -- straight, broken, Euclidean, stutters, halves */
static const uint16_t SLICE_PAT[16] = {
    0x5555, 0x7777, 0x3333, 0x6DB6, 0x4925, 0xB5AD, 0x2D2D, 0x1111,
    0xF0F0, 0x0F3F, 0xAB55, 0x9249, 0xEEEE, 0x5B5B, 0x00FF, 0x1F1F,
};

static void mangle_init(void)
{
    uint32_t t;
    memset(mangle, 0, sizeof mangle);
    for (t = 0; t < MG_TRACKS; t++) {
        mangle[t].g = Q15;
        mangle[t].rnd = 0xD3571u + t * 977u;
    }
}

/* the bar's clock, for SLICE: where the block starts, in steps (Q24, 0 .. 16 steps), and its step a sample */
static uint32_t mg_pos, mg_dpos;
#ifdef HOST
static int32_t mg_host_peak;
#endif

/* WRECK over x (in place): FOLD, then RING. In the flash, as the 4-OP's render is (the RAM code's room is spent),
 * called only when one of them is up */
__attribute__((noinline)) static void wreck_block(mangle_t *q, int32_t *x, const uint8_t *m)
{
    int32_t gf = 4096 + (int32_t)(m[M_FOLD] & 127u) * (4 * 4096) / 127, rg = (int32_t)(m[M_RING] & 127u) * 258;
    /* (RING FRQ: 20 Hz x 2^(8 octaves, less a little) -- 20 Hz a step of 2^32 / FS each sample) */
    uint32_t i, ph = q->rph, inc = (uint32_t)(((uint64_t)1947820u * exp2_q16((int32_t)(m[M_RFRQ] & 127u) * 4111)) >> 16);
    int fold = m[M_FOLD] != 0;
    for (i = 0; i < CTL; i++) {
        int32_t v = x[i];
        if (fold) {                              /* (a triangle of period 4 x 32768: past full scale, it turns back) */
            uint32_t u = (uint32_t)(mulq12(v, gf) + 32768) & ((4u << 15) - 1u);
            v = (int32_t)(u < (2u << 15) ? u : (4u << 15) - u) - 32768;
        }
        v += mulq15(mulq15(v, sin_ph(ph)) - v, rg);
        ph += inc;
        x[i] = v;
    }
    q->rph = ph;
}
/* DESTROY over x (in place), amount a (1 .. 127) of mode mode. In the flash, as WRECK */
__attribute__((noinline)) static void destroy_block(mangle_t *q, int32_t *x, uint32_t a, uint32_t mode)
{
    int32_t w = a >= 8u ? Q15 : (int32_t)a * (Q15 / 8);
    uint32_t i;
    if (mode == 0u) {                            /* CLIP */
        int32_t g = (int32_t)exp2_q16((int32_t)a * 3612), bias = (int32_t)(a * a * 48u / 127u), lp = q->lp;
        for (i = 0; i < CTL; i++) {
            int32_t s = clampi(x[i], -32767, 32767), e, t;
            lp += mulq15(s - lp, 1167);          /* (~250 Hz) */
            e = s + ((s - lp) >> 1);
            t = clampi(2 * (mulq16(e, g) + bias), -49152, 49152);   /* (Q15 of v / 16384: the knee 0 .. 1.5) */
            t -= mulq15(mulq15(mulq15(t, t), t), 4855);   /* t - 4/27 t^3: flat at 1.5 */
            x[i] = s + mulq15((t >> 1) - s, w);   /* (unity below the knee; the ceiling -6 dBFS) */
        }
        q->lp = lp;
        return;
    }
    if (mode <= 2u) {                            /* XOR, FLIP: on the signal scaled to near full scale */
        uint32_t pos = 9u * 256u + a * 5u * 256u / 127u, b = pos >> 8;   /* (bit 9 .. 14, Q8) */
        int32_t fr = (int32_t)(pos & 255u) << 7, env = q->env;
        uint32_t m0 = mode == 1u ? 0x5A5Au >> (14u - b) : 1u << b,
                 m1 = mode == 1u ? 0x5A5Au >> (b < 14u ? 13u - b : 0u) : 1u << (b < 14u ? b + 1u : 14u);
        for (i = 0; i < CTL; i++) {
            int32_t s = clampi(x[i], -32767, 32767), as = absi(s), sh, u, e0, e1;
            env = as > env ? as : env - (env >> 9);
            sh = env > 0 ? __builtin_clz((uint32_t)env) - 17 : 10;   /* (env << sh in 16384 .. 32767) */
            if (sh > 10)
                sh = 10;
            u = s << sh;
            e0 = (int32_t)(int16_t)(uint16_t)((uint32_t)u ^ m0) - u;
            e1 = (int32_t)(int16_t)(uint16_t)((uint32_t)u ^ m1) - u;
            e0 += mulq15(e1 - e0, fr);
            x[i] = s + mulq15(e0 >> sh, w);
        }
        q->env = env;
        return;
    }
    {                                            /* DROP */
        uint32_t p16 = (uint32_t)((9u * exp2_q16((int32_t)a * 2890)) >> 16),   /* (the odds a sample, of 2^16) */
                 dmax = (uint32_t)((88u * exp2_q16((int32_t)a * 1884)) >> 16);  /* (2 .. 25 ms) */
        for (i = 0; i < CTL; i++) {
            int32_t s = clampi(x[i], -32767, 32767), y = s;
            q->rnd = q->rnd * 1664525u + 1013904223u;
            if (q->hold_n) {
                q->hold_n--;
                y = q->hold;
            } else if ((q->rnd >> 16) < p16) {
                q->hold_n = 1u + (q->rnd >> 4) % dmax;
                q->hold = (q->rnd & 0x300u) == 0u ? 0 : s;   /* (a quarter of them cut, the rest frozen) */
                y = q->hold;
            }
            x[i] = s + mulq15(y - s, w);
        }
    }
}
/* one block of track t's mangling over x (in place): m the MANGLE values (the sound's, the track's locks over them) */
FAST static void mangle_block(uint32_t t, int32_t *x, const uint8_t *m)
{
    mangle_t *q = &mangle[t];
    uint32_t i, a = m[M_DST] & 127u, pat = m[M_SLICE] && m[M_SLICE] <= 16u ? SLICE_PAT[m[M_SLICE] - 1u] : 0u;
    if (m[M_FOLD] | m[M_RING])                  /* WRECK */
        FAR(wreck_block)(q, x, m);
#ifdef HOST
    for (i = 0; i < CTL; i++)                    /* (the host's DESTROY lab: the level coming in) */
        if (absi(x[i]) > mg_host_peak)
            mg_host_peak = absi(x[i]);
#endif
    if (a)                                       /* DESTROY */
        FAR(destroy_block)(q, x, a, m[M_DMODE] & 3u);
    if (pat || q->g != Q15) {   /* SLICE (or a gate still opening back up after it was turned off) */
        uint32_t r = m[M_SRATE] & 3u, sh = m[M_SSHIFT] & 15u, pos = mg_pos;
        int32_t shut = Q15 - (int32_t)(m[M_SDEPTH] & 127u) * Q15 / 127;
        /* the edges: a one-pole's step, 0.074 (~0.3 ms) down to 0.00076 (~30 ms), exponentially (Q15) */
        int32_t k = (int32_t)((2430u * exp2_q16(-(int32_t)(m[M_SEDGE] & 127u) * 3406)) >> 16);
        for (i = 0; i < CTL; i++) {
            uint32_t sp = r == 0u ? pos >> 1 : r == 1u ? pos : r == 2u ? pos << 1 : pos + (pos >> 1);   /* slices, Q24 */
            int32_t want = !pat || ((pat >> (((sp >> 24) + sh) & 15u)) & 1u) ? Q15 : shut;
            q->g += mulq15(want - q->g, k);
            x[i] = mulq15(x[i], q->g);
            pos = (pos + mg_dpos) & ((16u << 24) - 1u);
        }
        if (!pat && absi(Q15 - q->g) < 64)
            q->g = Q15;
    }
}
