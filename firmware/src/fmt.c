/* SPDX-License-Identifier: GPL-3.0-only
 * FM TONE: four operators -- C, A, B1, B2 -- after the Digitone II's (manual, A.2.1 and Appendix B).
 *
 *   ALGO       how the operators are connected (FT_ALG below): who modulates whom, which operator feeds back,
 *              which carriers make outputs X and Y, each output through its operator envelope or direct
 *   RATIO C/A/B  the operators' frequency ratios (B: B1 and B2 together, as the hands of a watch)
 *   HARM       the operators' waveforms: below 0, C's; above, A's and B1's -- sines with upper partials,
 *              crossfaded between the steps (HARM_* tables)
 *   DTUN       A and B2 pulled off their ratios (slightly up to the middle, then strongly)
 *   FDBK       the feedback operator's self-modulation
 *   MIX        X and Y crossfaded
 *   A / B envelopes: delay, attack, decay to an end level; triggered (ADE) or gated (ASDE: held at full while the
 *              note is, then the decay); LEV the modulation amount (B's macro-mapped to B1 and B2)
 *   RATIO OFFSET C, A, B1, B2; KEY TRACK A, B1, B2 (less modulation up the keyboard)
 * Phase modulation, as the classic FM synths: an operator's output moves its target's phase, two cycles at
 * full. */

#define OP_C 0
#define OP_A 1
#define OP_B1 2
#define OP_B2 3
/* an algorithm: the operators in the order they are computed, each one's modulators (bits of OP_*), the
 * feedback operator, and the outputs: X and Y the operators summed into each, those of them through their
 * envelope */
typedef struct { uint8_t order[4], mod[4], fb, x, xe, y, ye; } fmalg_t;
#define B(o) (1u << (o))
/* the Digitone II's eight, as its screen draws them (OS 1.12): X and Y from direct (unenveloped) carriers except
 * in 7 and 8, where A, B1 and B2 reach their outputs through their envelopes */
static const fmalg_t FT_ALG[8] = {
    /* 1: A -> C, B2 -> B1 -> C; A feeds back; X C, Y B1 */
    {{OP_B2, OP_A, OP_B1, OP_C}, {B(OP_A) | B(OP_B1), 0, B(OP_B2), 0}, OP_A, B(OP_C), 0, B(OP_B1), 0},
    /* 2: A -> C, B2 -> B1; B2 feeds back; X C, Y B1 */
    {{OP_B2, OP_A, OP_B1, OP_C}, {B(OP_A), 0, B(OP_B2), 0}, OP_B2, B(OP_C), 0, B(OP_B1), 0},
    /* 3: A -> C, B1, B2; A feeds back; X C + B2, Y B1 */
    {{OP_A, OP_B2, OP_B1, OP_C}, {B(OP_A), 0, B(OP_A), B(OP_A)}, OP_A, B(OP_C) | B(OP_B2), 0, B(OP_B1), 0},
    /* 4: B2 -> B1 -> A -> C; B2 feeds back; X C, Y B1 */
    {{OP_B2, OP_B1, OP_A, OP_C}, {B(OP_A), B(OP_B1), B(OP_B2), 0}, OP_B2, B(OP_C), 0, B(OP_B1), 0},
    /* 5: B2 -> B1, B1 and B2 -> A -> C; B1 feeds back; X C, Y A */
    {{OP_B2, OP_B1, OP_A, OP_C}, {B(OP_A), B(OP_B1) | B(OP_B2), B(OP_B2), 0}, OP_B1, B(OP_C), 0, B(OP_A), 0},
    /* 6: A and B2 -> C and B1; A feeds back; X C, Y B1 */
    {{OP_A, OP_B2, OP_B1, OP_C}, {B(OP_A) | B(OP_B2), 0, B(OP_A) | B(OP_B2), 0}, OP_A, B(OP_C), 0, B(OP_B1), 0},
    /* 7: A -> C, B2 -> B1; A feeds back; X C + A (enveloped), Y B1 + B2 (enveloped) */
    {{OP_A, OP_B2, OP_B1, OP_C}, {B(OP_A), 0, B(OP_B2), 0}, OP_A, B(OP_C) | B(OP_A), B(OP_A), B(OP_B1) | B(OP_B2),
     B(OP_B1) | B(OP_B2)},
    /* 8: A -> C; B1 (feeding back) and B2 alone; X C + B2 (enveloped), Y B1 (enveloped) -- the outputs as its DSP
     * sums them (its screen draws B2 to Y) */
    {{OP_A, OP_B2, OP_B1, OP_C}, {B(OP_A), 0, 0, 0}, OP_B1, B(OP_C) | B(OP_B2), B(OP_B2), B(OP_B1), B(OP_B1)},
};
#undef B

/* the ratio lists (x 4: quarters), the Digitone II's: C (and each of B1, B2): 19; A: 36 */
static const uint8_t FT_RC4[19] = {1, 2, 3, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64};
static const uint8_t FT_RA4[36] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
                                   22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 44, 48, 52, 56, 60, 64};

/* the operator envelope, a block at a time: delay, attack to full, (gated: held while the note is), decay to END */
enum { OE_DEL, OE_ATT, OE_SUS, OE_DEC, OE_END };
typedef struct { uint8_t st; uint16_t n; int32_t lv; } openv_t;
typedef struct {
    uint16_t del;
    int32_t att, dec;            /* the attack's step (Q24), the decay's factor towards END (Q16) */
    int32_t end;                 /* Q24 */
    uint8_t trig, rst;
} opeck_t;

typedef struct {
    const fmalg_t *alg;
    uint32_t r16[4];             /* each operator's ratio, Q16, offsets and detune in */
    int32_t lev[4];              /* each operator's LEV, 0 .. 127 x 256 (B1, B2: their shares of B's, the split law) */
    int32_t kt[4];               /* key tracking: KT / 127, Q15 */
    int32_t fb;                  /* FDBK: the phase offset at full, Q15 of a cycle (2 / pi at 127) */
    int32_t mixx, mixy;          /* the outputs' shares, Q15 */
    int8_t harm_c, harm_ab;      /* HARM: the harmonic wave for C, and for A and B1 (0: the sine) */
    opeck_t ea, eb;
    uint8_t phrt;                /* PHASE RESET: 0 off, 1 all, 2 C, 3 A + B, 4 A + B2 */
} ftck_t;

typedef struct {
    uint32_t ph[4];
    int32_t fb1, fb2;            /* the feedback operator's last two outputs */
    int32_t dep[4];              /* each operator's modulation depth as the last block ended (phase units a Q15 unit) */
    openv_t ea, eb;
    uint8_t gate;
} ftv_t;

/* the operator envelope's times: the Digitone II's (T_OPATK, T_OPDEC: attack 20 s and decay 60 s at 127); ADEL and
 * BDEL a bar at 120 BPM at full */
static void ope_cook(opeck_t *e, int32_t del, int32_t a, int32_t d, int32_t end, int32_t trg, int32_t rst)
{
    e->del = (uint16_t)(del > 0 ? clampi(us_blocks((uint32_t)clampi(del, 0, 127) * 15748u), 0, 0xFFFE) : 0);
    e->att = a > 0 ? (1 << 24) / clampi(us_blocks(T_OPATK[a & 127]), 1, 1 << 23) : 1 << 24;
    e->dec = d > 0 ? fall60_us(T_OPDEC[d & 127]) : 0;        /* (0: straight to END) */
    e->end = clampi(end, 0, 127) * ((1 << 24) / 127);
    e->trig = (uint8_t)(trg != 0);
    e->rst = (uint8_t)(rst != 0);
}
static void ft_cook(ftck_t *c, const sound_t *s)
{
    const int16_t *p = s->syn;
    int32_t rb = clampi(p[FT_RATB], 0, 360), dt = clampi(p[FT_DTUN], 0, 127), k;
    int32_t ofs[4] = {p[FT_OFSC], p[FT_OFSA], p[FT_OFSB1], p[FT_OFSB2]};
    uint32_t r4[4];
    c->alg = &FT_ALG[clampi(p[FT_ALGO], 0, 7)];
    r4[OP_C] = FT_RC4[clampi(p[FT_RATC], 0, 18)];
    r4[OP_A] = FT_RA4[clampi(p[FT_RATA], 0, 35)];
    r4[OP_B1] = FT_RC4[rb / 19];
    r4[OP_B2] = FT_RC4[rb % 19];
    for (k = 0; k < 4; k++) {   /* the ratio plus its offset: (value - 64) / 64 */
        int32_t r = (int32_t)(r4[k] << 14) + (clampi(ofs[k], 0, 127) - 64) * 1024;
        c->r16[k] = (uint32_t)maxi(r, 256);
    }
    c->r16[OP_A] += T_DTA[dt];                             /* DTUN: added to A's ratio and B2's (up to 0.5, 0.75) */
    c->r16[OP_B2] += T_DTB[dt];
    c->lev[OP_C] = 0;
    c->lev[OP_A] = clampi(p[FT_ALEV], 0, 127) << 8;
    {   /* B's LEV split between B2 and B1 (the Digitone II's law): B2 rises to full by 3/8 of the turn, falls back to
         * an eighth by 3/4 while B1 rises to full, then rises to full again */
        int32_t x = clampi(p[FT_BLEV], 0, 127) * 256 / 127, w1, w2;   /* x / 256 */
        w1 = x <= 96 ? 0 : x <= 192 ? (8 * x - 768) / 3 : 256;
        w2 = x <= 96 ? 8 * x / 3 : x <= 192 ? 480 - 7 * x / 3 : (7 * x - 1280) / 2;
        c->lev[OP_B1] = clampi(w1, 0, 256) * 127;
        c->lev[OP_B2] = clampi(w2, 0, 256) * 127;
    }
    c->kt[OP_C] = 0;
    c->kt[OP_A] = clampi(p[FT_KTA], 0, 127) * Q15 / 127;
    c->kt[OP_B1] = clampi(p[FT_KTB1], 0, 127) * Q15 / 127;
    c->kt[OP_B2] = clampi(p[FT_KTB2], 0, 127) * Q15 / 127;
    c->fb = clampi(p[FT_FDBK], 0, 127) * 20860 / 127;      /* 2 / pi of a cycle at full */
    k = clampi(p[FT_MIX], 0, 127);                          /* 0 X .. 127 Y, linear */
    c->mixy = k * Q15 / 127;
    c->mixx = Q15 - c->mixy;
    k = clampi(p[FT_HARM], 37, 89) - 63;                   /* -26 .. 26 */
    c->harm_c = (int8_t)(k < 0 ? -k : 0);
    c->harm_ab = (int8_t)(k > 0 ? k : 0);
    ope_cook(&c->ea, p[FT_ADEL], p[FT_AATK], p[FT_ADEC], p[FT_AEND], p[FT_ATRG], p[FT_ARST]);
    ope_cook(&c->eb, p[FT_BDEL], p[FT_BATK], p[FT_BDEC], p[FT_BEND], p[FT_BTRG], p[FT_BRST]);
    c->phrt = (uint8_t)clampi(p[FT_PHRT], 0, 4);
}

static void ope_gate(openv_t *e, const opeck_t *c, int fresh)
{
    if (fresh || c->rst)
        e->lv = 0;
    e->st = c->del ? OE_DEL : OE_ATT;
    e->n = c->del;
}
static int32_t ope_step(openv_t *e, const opeck_t *c, int gate)
{
    switch (e->st) {
    case OE_DEL:
        if (e->n)
            e->n--;
        if (!e->n)
            e->st = OE_ATT;
        break;
    case OE_ATT:
        e->lv += c->att;
        if (e->lv >= 1 << 24) {
            e->lv = 1 << 24;
            e->st = c->trig || !gate ? OE_DEC : OE_SUS;
        }
        break;
    case OE_SUS:
        if (!gate)
            e->st = OE_DEC;
        break;
    case OE_DEC:
        e->lv = c->end + (int32_t)(((int64_t)(e->lv - c->end) * c->dec) >> 16);
        break;
    default:
        break;
    }
    return e->lv;
}

static void ft_on(ftv_t *v, const ftck_t *c, int fresh)
{
    uint32_t m = 0;
    if (fresh) {
        memset(v, 0, sizeof *v);
        m = 0xF;
    } else
        m = c->phrt == 1 ? 0xF : c->phrt == 2 ? 1u << OP_C : c->phrt == 3 ? 0xEu : c->phrt == 4 ? (1u << OP_A) | (1u << OP_B2) : 0;
    {
        uint32_t k;
        for (k = 0; k < 4u; k++)
            if ((m >> k) & 1u)
                v->ph[k] = 0;
    }
    ope_gate(&v->ea, &c->ea, fresh);
    ope_gate(&v->eb, &c->eb, fresh);
    v->gate = 1;
}
static void ft_off(ftv_t *v) { v->gate = 0; }

/* a wave of the single cycles (osc.c sc_ram): HARM's k (0: the sine, from its own table) */
AINL const int16_t *ft_wtab(int32_t k) { return k ? sc_tab(SC_FMH0 + (uint32_t)k) : 0; }
AINL int32_t ft_wave(const int16_t *t, uint32_t ph) { return t ? wt_rd(t, 24u, ph) : sinr(ph); }
/* the modulation depth of LEV x (0 .. 127 x 256) at envelope e (Q24): the Digitone II's curve looked up at LEV x the
 * envelope, 2.205 cycles at full -- as phase units a Q15 unit of the modulator (2^32 x 2.205 / 32768 at the top) */
AINL int32_t ft_depth(int32_t lev, int32_t e)
{
    int32_t x = (int32_t)(((int64_t)lev * (e >> 8)) >> 16), i = x >> 8, f = x & 255, a;   /* x: 0 .. 127 x 256 */
    if (i >= 127)
        return (T_LEV[127] * 9034) >> 10;
    a = T_LEV[i];
    return ((a + (((T_LEV[i + 1] - a) * f) >> 8)) * 9034) >> 10;  /* (x 2.205 x 2^32 / 2^30: 8.82) */
}

/* a block of the voice at its base phase increment inc (the note's), Q15 out */
static void ft_render(ftv_t *v, const ftck_t *c, uint32_t inc, int32_t note16, int32_t *out)
{
    const fmalg_t *g = c->alg;
    uint32_t inc_o[4], k, i, nx, ny;
    int32_t ea0 = v->ea.lv, eb0 = v->eb.lv, ea1, eb1, dea, deb, ra, rb, d1[4], dd[4], dep[4];
    const int16_t *hw[4];
    int32_t hz = hz_of_oct(198656 + note16 / 12), w = Q15;  /* the note in Hz (log2 440 + (n - 69) / 12) */
    ea1 = ope_step(&v->ea, &c->ea, v->gate);
    eb1 = ope_step(&v->eb, &c->eb, v->gate);
    for (k = 0; k < 4u; k++) {
        uint64_t q = ((uint64_t)inc * c->r16[k]) >> 16;
        int32_t l = c->lev[k], e = k == OP_A ? ea1 : k == OP_C ? 1 << 24 : eb1;
        inc_o[k] = q > 0x7FFFFFFFu ? 0x7FFFFFFFu : (uint32_t)q;
        if (c->kt[k]) {                                      /* key tracking: from C1 to C7, less modulation */
            w = Q15 - mulq15(c->kt[k], clampi((hz - 33) * 16, 0, Q15));   /* (x / 2060 Hz, Q15: x 15.9) */
            l = mulq15(l, clampi(w, 0, Q15));
        }
        d1[k] = ft_depth(l, e);
        dep[k] = v->dep[k];
        dd[k] = (d1[k] - dep[k]) / (int32_t)CTL;
    }
    hw[OP_C] = ft_wtab(c->harm_c);
    hw[OP_A] = hw[OP_B1] = ft_wtab(c->harm_ab);
    hw[OP_B2] = 0;
    ra = ea0 >> 9;                                           /* the envelopes across the block, Q15 */
    rb = eb0 >> 9;
    dea = ((ea1 >> 9) - ra) / (int32_t)CTL;
    deb = ((eb1 >> 9) - rb) / (int32_t)CTL;
    nx = ny = 0;
    for (k = 0; k < 4u; k++) {
        nx += (g->x >> k) & 1u;
        ny += (g->y >> k) & 1u;
    }
    for (i = 0; i < CTL; i++) {
        int32_t o[4] = {0, 0, 0, 0}, e[4], x = 0, y = 0;
        uint32_t j;
        ra += dea;
        rb += deb;
        e[OP_C] = Q15;
        e[OP_A] = ra;
        e[OP_B1] = e[OP_B2] = rb;
        for (k = 0; k < 4u; k++)
            dep[k] += dd[k];
        for (j = 0; j < 4u; j++) {
            uint32_t op = g->order[j], ph = v->ph[op], mm = g->mod[op], s;
            for (s = 0; s < 4u; s++)                         /* each modulator's output, x its depth */
                if ((mm >> s) & 1u)
                    ph += (uint32_t)((int64_t)o[s] * dep[s]);
            if (op == g->fb && c->fb)                        /* feedback: x the operator's own level */
                ph += (uint32_t)mulq15(mulq15((v->fb1 + v->fb2) >> 1, c->fb), e[op]) << 17;
            o[op] = ft_wave(hw[op], ph);
            if (op == g->fb) {
                v->fb2 = v->fb1;
                v->fb1 = o[op];
            }
            v->ph[op] += inc_o[op];
        }
        for (j = 0; j < 4u; j++) {
            if ((g->x >> j) & 1u)
                x += (g->xe >> j) & 1u ? mulq15(o[j], e[j]) : o[j];
            if ((g->y >> j) & 1u)
                y += (g->ye >> j) & 1u ? mulq15(o[j], e[j]) : o[j];
        }
        if (nx == 1u)                                        /* (each output is two buffers summed: one alone, twice) */
            x *= 2;
        if (ny == 1u)
            y *= 2;
        out[i] = (mulq15(x, c->mixx) + mulq15(y, c->mixy)) >> 1;
    }
    for (k = 0; k < 4u; k++)
        v->dep[k] = d1[k];
}
