/* SPDX-License-Identifier: GPL-3.0-only
 * MANGLE, a track's (its own page, after FX): on the track's voices summed, before its limiter, set by the sound the
 * track plays and lockable step by step (a lock holds until the track's next step plays, as the sends' do).
 *
 *   DESTROY   digital destruction, on the samples as 16-bit words: the amount (0 off), and its kind --
 *               WRAP   the signal driven up to x 16 and let overflow, wrapping round as two's complement does
 *               XOR    the low bits XORed with a fixed pattern, more of them as it goes up: a noise that follows
 *                      the signal, then breaks it
 *               FLIP   one bit flipped, a higher one as it goes up: a square error riding the wave
 *               DROP   samples dropped and held, more often and longer as it goes up: data errors, crackle
 *             (fading in over its first eight steps)
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
    int32_t hold;                /* DROP: the sample held, and samples left */
    uint32_t hold_n;
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
/* one block of track t's mangling over x (in place): m the MANGLE values (the sound's, the track's locks over them) */
FAST static void mangle_block(uint32_t t, int32_t *x, const uint8_t *m)
{
    mangle_t *q = &mangle[t];
    uint32_t i, a = m[M_DST] & 127u, pat = m[M_SLICE] && m[M_SLICE] <= 16u ? SLICE_PAT[m[M_SLICE] - 1u] : 0u;
    if (m[M_FOLD] | m[M_RING])                  /* WRECK */
        FAR(wreck_block)(q, x, m);
    if (a) {
        int32_t w = a >= 8u ? Q15 : (int32_t)a * (Q15 / 8);
        uint32_t mode = m[M_DMODE] & 3u;
        int32_t gw = 4096 + (int32_t)a * (15 * 4096) / 127;          /* WRAP: x 1 .. x 16 */
        uint32_t xm = ((1u << (1u + a * 14u / 127u)) - 1u) & 0x5A5Au;  /* XOR: the pattern on the low bits */
        uint32_t fb = 1u << (a * 14u / 127u);                          /* FLIP: the bit */
        uint32_t pd = a * 260u;                                        /* DROP: the odds a sample, of 2^16 (to ~0.5) */
        for (i = 0; i < CTL; i++) {
            int32_t s = clampi(x[i], -32767, 32767), y;
            switch (mode) {
            case 0:
                y = (int16_t)(uint16_t)(uint32_t)mulq12(s, gw);         /* (two's complement: it wraps) */
                break;
            case 1:
                y = (int16_t)(uint16_t)((uint32_t)s ^ xm);
                break;
            case 2:
                y = (int16_t)(uint16_t)((uint32_t)s ^ fb);
                break;
            default:
                q->rnd = q->rnd * 1664525u + 1013904223u;
                if (q->hold_n) {
                    q->hold_n--;
                    y = q->hold;
                } else if ((q->rnd >> 16) < pd) {
                    q->hold = s;
                    q->hold_n = 1u + ((q->rnd >> 8) & 255u) % (1u + a / 3u);
                    y = s;
                } else
                    y = s;
                break;
            }
            x[i] = s + mulq15(y - s, w);
        }
    }
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
