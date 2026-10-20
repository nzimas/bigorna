/* SPDX-License-Identifier: GPL-3.0-only
 * WAVETONE: two oscillators and a noise, after the Digitone II's (manual, A.2.3).
 *
 *   TUN1/2     each oscillator's pitch (semitones from the note); OFS1/2 a further offset in hertz (the beating
 *              the same at every pitch); DRIF a slow random drift of both
 *   TBL1/2     PRIM (sine, triangle, saw, square) or HARM (waves sweeping through combinations of harmonics);
 *              WAV1/2 a position along the table, crossfading its waves
 *   PD1/2      phase distortion: the first and second halves of the cycle read at different speeds (on a square,
 *              its pulse width), 50 straight
 *   LEV1/2     their levels
 *   MOD        OFF, RING MOD (oscillator 2 multiplies oscillator 1), RING MOD FIXED (oscillator 2 at a fixed
 *              pitch, not following the note), HARD SYNC (oscillator 1 restarted by each of oscillator 2's cycles)
 *   RSET       the phases at a note: free, reset, or at random
 *   noise      its own envelope (ATK, HOLD -- 127: NOTE --, DEC), NLEV, a base-width band (BASE, WDTH), TYPE
 *              GRAIN (CHAR: its density), TUNED (sample and hold following the note; CHAR its pitch), S&H
 *              (sample and hold at a fixed rate, CHAR) */

typedef struct {
    int32_t tun16[2];            /* semitones Q16 */
    uint32_t ofs_inc[2];         /* the offset in hertz, as a phase step (signed, in two's complement) */
    uint8_t tbl[2];
    int32_t wav[2];              /* Q15 along the table */
    int32_t pd[2];               /* the breakpoint, Q16 of a cycle (0x8000: straight) */
    int32_t lev[2];              /* Q15 */
    uint8_t mod, rset;
    int32_t drif;                /* the drift's depth, semitones Q16 */
    envck_t ne;                  /* the noise's envelope: the AMP envelope's, as AHD (HOLD 127: NOTE) */
    int32_t nlev;
    uint8_t ntype;
    int32_t nchar;               /* 0 .. 127 */
    int32_t gran;                /* GRAIN: an impulse a sample at these odds (Q16) */
    bwc_t nb;
} wtck_t;

typedef struct {
    uint32_t ph[2];
    env_t ne;                    /* the noise's envelope */
    rnd_t rn;
    int32_t s1, s2;
    uint32_t sh_ph;
    int32_t sh_v;
    int32_t dr, dr_t;            /* the drift, now and where it goes (semitones Q16) */
    uint8_t gate;
} wtv_t;

static void wt_cook(wtck_t *c, const sound_t *s)
{
    const int16_t *p = s->syn;
    uint32_t k;
    for (k = 0; k < 2u; k++) {
        int32_t tun = p[k ? WT_TUN2 : WT_TUN1], ofs = p[k ? WT_OFS2 : WT_OFS1], pd = p[k ? WT_PD2 : WT_PD1];
        c->tun16[k] = semi16(clampi(tun, 4, 124) - 64);
        c->ofs_inc[k] = (uint32_t)((clampi(ofs, 0, 100) - 50) * 19478);   /* +-10 Hz: 0.2 Hz a step (97391 / 5) */
        c->tbl[k] = (uint8_t)(p[k ? WT_TBL2 : WT_TBL1] != 0);
        c->wav[k] = clampi(p[k ? WT_WAV2 : WT_WAV1], 0, 120) * Q15 / 127;   /* (as the original: v / 127 along it) */
        c->pd[k] = 2048 + clampi(pd, 0, 100) * (65536 - 4096) / 100;   /* 3 % .. 97 % of the cycle */
        c->lev[k] = clampi(p[k ? WT_LEV2 : WT_LEV1], 0, 127) * clampi(p[k ? WT_LEV2 : WT_LEV1], 0, 127) * Q15 / 16129;   /* p^2 */
    }
    c->mod = (uint8_t)clampi(p[WT_MOD], 0, 3);
    c->rset = (uint8_t)clampi(p[WT_RSET], 0, 2);
    c->drif = clampi(p[WT_DRIF], 0, 127) * (65536 / 2) / 127;   /* up to half a semitone */
    env_cook(&c->ne, 0, T_AATK[p[WT_NATK] & 127], p[WT_NHLD] >= 127 ? T_INF : T_AHOLD[p[WT_NHLD] & 127],
             T_ADEC[p[WT_NDEC] & 127], 0, T_ADEC[p[WT_NDEC] & 127], 0);
    c->nlev = clampi(p[WT_NLEV], 0, 127) * Q15 / 127;
    c->ntype = (uint8_t)clampi(p[WT_NTYP], 0, 2);
    c->nchar = clampi(p[WT_CHAR], 0, 127);
    {   /* GRAIN's odds 0.0001 + (0.992 - p)^2: white at 0, sparse at the top (as FM DRUM's GRAN) */
        int32_t d = 32512 - c->nchar * Q15 / 127;
        c->gran = maxi(7, (int32_t)(((int64_t)d * d) >> 14) + 7);
    }
    bw_cook(&c->nb, p[WT_NBAS], p[WT_NWID]);
}
static void wt_on(wtv_t *v, const wtck_t *c, int fresh, uint32_t seed)
{
    if (fresh) {
        memset(v, 0, sizeof *v);
        rnd_seed(&v->rn, seed);
    }
    if (c->rset == 1)
        v->ph[0] = v->ph[1] = 0;
    else if (c->rset == 2) {
        v->ph[0] = rnd_next(&v->rn);
        v->ph[1] = rnd_next(&v->rn);
    }
    env_gate(&v->ne, &c->ne, 1);
    v->gate = 1;
}
static void wt_off(wtv_t *v, const wtck_t *c)
{
    v->gate = 0;
    env_release(&v->ne, &c->ne);
}

/* phase distortion: the phase bent so that the first half of the wave takes bp of the cycle (Q16); m1, m2:
 * 2^31 / bp and 2^31 / (1 - bp), once a block */
typedef struct { uint32_t bp, m1, m2; } pd_t;
FAST static void pd_set(pd_t *d, int32_t bp)
{
    d->bp = (uint32_t)bp;
    d->m1 = 0x80000000u / (uint32_t)bp;
    d->m2 = 0x80000000u / (uint32_t)(65536 - bp);
}
AINL uint32_t wt_pd(uint32_t ph, const pd_t *d)
{
    uint32_t x = ph >> 16;                               /* Q16 of the cycle */
    if (d->bp == 0x8000u)
        return ph;
    if (x < d->bp)
        return x * d->m1;
    return 0x80000000u + (x - d->bp) * d->m2;
}

FAST static void wt_render(wtv_t *v, const wtck_t *c, int32_t note16, int32_t *out)
{
    uint32_t inc[2], i, k, sync16;
    morph_t m[2];
    pd_t pd[2];
    int32_t ne0, dne, n;
    if ((rnd_next(&v->rn) & 1023u) < 4u)                 /* the drift: a new place every ~0.2 s, glided to */
        v->dr_t = (int32_t)(((int64_t)rnd_bi(&v->rn) * c->drif) >> 15);
    v->dr += (v->dr_t - v->dr) >> 7;
    for (k = 0; k < 2u; k++) {
        int32_t p16 = (k == 1u && c->mod == 2) ? (60 << 16) + c->tun16[1] : note16 + c->tun16[k] + v->dr;
        inc[k] = note_inc(p16) + c->ofs_inc[k];
        if ((int32_t)inc[k] < 0)
            inc[k] = 0;
        {   /* (the level from the pitch without the drift: one that moves would flip at a boundary, clicking) */
            uint32_t lv_inc = note_inc(k == 1u && c->mod == 2 ? p16 : p16 - v->dr);
            if (c->tbl[k])
                morph_set(&m[k], WV_HARM0, WV_HARM_N, c->wav[k], lv_inc);
            else
                morph_set(&m[k], WV_PRIM0, WV_PRIM_N, c->wav[k], lv_inc);
        }
        pd_set(&pd[k], c->pd[k]);
    }
    sync16 = inc[1] >> 16 ? inc[0] / (inc[1] >> 16) : 0;  /* (hard sync: oscillator 1's phase per 2's, Q16) */
    ne0 = v->ne.lv;                                      /* the noise's envelope, a block on */
    env_step(&v->ne, &c->ne, v->gate);
    n = ne0 >> 9;
    dne = ((v->ne.lv >> 9) - n) / (int32_t)CTL;
    {
        uint32_t sh_inc = c->ntype == 1 ? note_inc(note16 + ((36 + (c->nchar - 64) / 2) << 16))   /* (TUNED: three
                                                                                         * octaves over the note) */
                                        : note_inc((c->nchar + 24) << 16);   /* (S&H: a fixed rate, from CHAR) */
        int32_t gr = c->gran;
        for (i = 0; i < CTL; i++) {
            uint32_t was = v->ph[1];
            int32_t o1, o2, y, nz;
            v->ph[1] += inc[1];
            o2 = morph_rd(&m[1], wt_pd(v->ph[1], &pd[1]));
            if (c->mod == 3 && v->ph[1] < was)           /* hard sync: oscillator 1 from the start */
                v->ph[0] = (uint32_t)(((uint64_t)v->ph[1] * sync16) >> 16);
            else
                v->ph[0] += inc[0];
            o1 = morph_rd(&m[0], wt_pd(v->ph[0], &pd[0]));
            if (c->mod == 1 || c->mod == 2)
                o1 = mulq15(o1, o2);
            y = mulq15(o1, c->lev[0]) + mulq15(o2, c->lev[1]);
            if (c->nlev) {
                n += dne;
                if (c->ntype == 0) {
                    if ((int32_t)(rnd_next(&v->rn) >> 16) < gr)
                        nz = rnd_bi(&v->rn);
                    else
                        nz = 0;
                } else {
                    uint32_t w = v->sh_ph;
                    v->sh_ph += sh_inc;
                    if (v->sh_ph < w)
                        v->sh_v = rnd_bi(&v->rn);
                    nz = v->sh_v;
                }
                nz = bw_run(&v->s1, &v->s2, &c->nb, nz);
                y += mulq15(mulq15(nz, n), c->nlev);
            }
            out[i] = y;
        }
    }
}

/* ------------------------------------------------------------ SWARMER --- */
/* SWARMER: a main oscillator and a swarm of six around it (manual, A.2.4).
 *   TUNE   semitones from the note
 *   SWRM, MAIN  the swarm's and the main oscillator's waveforms (a position along PRIM)
 *   DET    how far the six are spread about the main pitch: the Digitone II's spread, asymmetric, about 19 cents for
 *          the outermost at 64 and two semitones at 127 (T_SWDET)
 *   MIX    the swarm against the main oscillator (the main 0.25 down to 0.13, each of the six up to 0.105)
 *   M.OCT  the main oscillator one or two octaves down (the swarm stays)
 *   ANIM   the swarm's movement: its oscillators detuned in pairs by hidden LFOs at different rates
 *   N.MOD  noise modulating the swarm's pitches */
static const uint16_t SW_LFO[3] = {13, 21, 34};                         /* the pairs' LFOs, x 0.1 Hz */

typedef struct {
    int32_t tune16;
    int32_t swrm, main;          /* Q15 along PRIM */
    const int32_t *det;          /* the six offsets, semitones Q16 */
    int32_t det_hi;              /* the highest of them (the waves' level is chosen at it) */
    int32_t gm, gs;              /* the main's gain, each swarm voice's (Q15, x 4) */
    int32_t moct;                /* the main's octave, 0 .. -2 */
    int32_t anim16;              /* the LFOs' depth, semitones Q16 */
    int32_t nmod16;              /* the noise's depth, semitones Q16 */
} swck_t;
typedef struct {
    uint32_t ph[7];
    uint32_t lph[3];
    rnd_t rn;
    int32_t nlp[6];
} swv_t;

static void sw_cook(swck_t *c, const sound_t *s)
{
    const int16_t *p = s->syn;
    int32_t det = clampi(p[SW_DET], 0, 127), m = clampi(p[SW_MIX], 0, 127), a = clampi(p[SW_ANIM], 0, 127),
            n = clampi(p[SW_NMOD], 0, 127);
    c->tune16 = semi16(clampi(p[SW_TUNE], 4, 124) - 64);
    c->swrm = clampi(p[SW_SWRM], 0, 120) * Q15 / 127;      /* (as the original: v / 127 along it) */
    c->main = clampi(p[SW_MAIN], 0, 120) * Q15 / 127;
    c->det = &T_SWDET[det * 6];
    c->det_hi = c->det[5];
    c->gm = 32846 - m * 16240 / 127;                       /* 4 x (0.2506 - 0.1239 m) */
    c->gs = m * 13775 / 127;                               /* 4 x 0.1051 m */
    c->moct = clampi(p[SW_MOCT], 0, 2) - 2;
    c->anim16 = a * a * 32768 / 16129;                     /* p^2: up to half a semitone */
    c->nmod16 = n * n * 65536 / 16129;                     /* p^2: up to a semitone */
}
static void sw_on(swv_t *v, int fresh, uint32_t seed)
{
    uint32_t k;
    if (!fresh)
        return;
    memset(v, 0, sizeof *v);
    rnd_seed(&v->rn, seed);
    for (k = 0; k < 7u; k++)
        v->ph[k] = rnd_next(&v->rn);                      /* (free running: a swarm never starts in step) */
}
FAST static void sw_render(swv_t *v, const swck_t *c, int32_t note16, int32_t *out)
{
    uint32_t inc[7], i, k;
    morph_t ms, mm;
    int32_t p0 = note16 + c->tune16, gm, gs;
    for (k = 0; k < 3u; k++)
        v->lph[k] += (uint32_t)SW_LFO[k] * 9739u * CTL;   /* 2^32 / FS / 10 a sample, x CTL */
    for (k = 0; k < 6u; k++) {
        int32_t d = c->det[k], l = sinr(v->lph[k >> 1] + (k & 1u) * 0x80000000u);
        int32_t nz;
        d += mulq15(l, c->anim16);
        v->nlp[k] += (rnd_bi(&v->rn) - v->nlp[k]) >> 3;    /* (a slow noise, a block at a time) */
        nz = mulq15(v->nlp[k] * 4, c->nmod16);
        inc[k] = note_inc(p0 + d + nz);
    }
    inc[6] = note_inc(p0 + (c->moct << 16) * 12);
    /* (the waves' band-limited level from the pitch before detune, animation and noise: a level chosen from the
     * moving pitch flips back and forth at a boundary, a click each time) */
    morph_set(&ms, WV_PRIM0, WV_PRIM_N, c->swrm, note_inc(p0 + c->det_hi));
    morph_set(&mm, WV_PRIM0, WV_PRIM_N, c->main, inc[6]);
    gs = c->gs;
    gm = c->gm;
    {   /* (each of the six through the block, its phase in a register; the two waves summed apart, crossfaded once a
         * sample on the sums: the same as crossfading each, but one multiply, not six) */
        int32_t sa[CTL], sb[CTL];
        const int16_t *ta = ms.a, *tb = ms.b;
        uint32_t sh = ms.sh, ish = 32u - ms.sh;
        for (i = 0; i < CTL;) {
            REP4({
                sa[i] = sb[i] = 0;
            })
        }
        for (k = 0; k < 6u; k++) {
            uint32_t ph = v->ph[k], dp = inc[k];
            if (ms.x)
                for (i = 0; i < CTL;) {
                    REP4({
                        uint32_t j;
                        int32_t f;
                        ph += dp;
                        j = ph >> sh;
                        f = (int32_t)((ph << ish) >> 17);
                        sa[i] += ta[j] + (((ta[j + 1u] - ta[j]) * f) >> 15);
                        sb[i] += tb[j] + (((tb[j + 1u] - tb[j]) * f) >> 15);
                    })
                }
            else
                for (i = 0; i < CTL;) {
                    REP4({
                        uint32_t j;
                        int32_t f;
                        ph += dp;
                        j = ph >> sh;
                        f = (int32_t)((ph << ish) >> 17);
                        sa[i] += ta[j] + (((ta[j + 1u] - ta[j]) * f) >> 15);
                    })
                }
            v->ph[k] = ph;
        }
        if (ms.x)
            for (i = 0; i < CTL;) {
                REP4({
                    sa[i] += (int32_t)(((int64_t)(sb[i] - sa[i]) * ms.x) >> 15);
                })
            }
        for (i = 0; i < CTL;) {
            REP4({
                v->ph[6] += inc[6];
                out[i] = mulq15(sa[i], gs) + mulq15(morph_rd(&mm, v->ph[6]), gm);
            })
        }
    }
}
