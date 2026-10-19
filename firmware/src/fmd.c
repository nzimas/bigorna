/* SPDX-License-Identifier: GPL-3.0-only
 * FM DRUM: an FM engine for drums and percussion, after the Digitone II's (manual, A.2.2; the algorithms and ratios as
 * its OS 1.12 shows them). Three parts, summed:
 *
 *   body       three operators (C, A, B) in one of seven algorithms, the pitch swept down from SDEP above TUNE
 *              over STIM; OP.C and OP.AB their waveforms, FDBK the feedback operator's, A's and B's modulation by
 *              decay envelopes (DEC to END, MOD the amount), FOLD a wavefolder; its own envelope (HOLD, DEC; the
 *              last value: no decay), LEV; PH.C where C starts (0 .. 90 degrees; 91: not reset)
 *   noise      white to grainy (GRAN), through a base-width band (BASE, WDTH), its envelope (NHLD, NDEC; NRST the
 *              same noise every note; NRM: ring-modulated by operator C), NLEV
 *   transient  one of a set of short attacks (TRAN), TLEV
 * In algorithms 5 .. 7 operator B sounds as well as C. */

#define FD_C 0
#define FD_A 1
#define FD_B 2
typedef struct { uint8_t order[3], mod[3], fb, bout; } fdalg_t;   /* (fb: a bit an operator) */
#define B(o) (1u << (o))
/* the Digitone II's seven, as its screen draws them (OS 1.12) */
static const fdalg_t FD_ALG[7] = {
    {{FD_A, FD_B, FD_C}, {B(FD_A) | B(FD_B), 0, 0}, B(FD_B), 0},                 /* 1: A, B -> C; B feeds back */
    {{FD_A, FD_B, FD_C}, {B(FD_A) | B(FD_B), 0, 0}, B(FD_A) | B(FD_B), 0},       /* 2: the same, both feed back */
    {{FD_B, FD_A, FD_C}, {B(FD_A) | B(FD_B), B(FD_B), 0}, B(FD_B), 0},           /* 3: B -> A, A and B -> C */
    {{FD_B, FD_A, FD_C}, {B(FD_A), B(FD_B), 0}, B(FD_B), 0},                     /* 4: B -> A -> C */
    {{FD_A, FD_B, FD_C}, {B(FD_A), 0, 0}, B(FD_B), 1},                           /* 5: A -> C; B heard, feeding back */
    {{FD_A, FD_B, FD_C}, {B(FD_A), 0, 0}, B(FD_A), 1},                           /* 6: A -> C, A feeding back; B heard */
    {{FD_A, FD_B, FD_C}, {B(FD_A), 0, 0}, B(FD_A) | B(FD_B), 1},                 /* 7: A -> C; B heard; both feed back */
};
#undef B

typedef struct {
    const fdalg_t *alg;
    int32_t tune16;              /* TUNE: semitones Q16 */
    int32_t sdep;                /* SDEP: the sweep's start, 16 p^2 times the note above it (Q12) */
    int32_t sfall;               /* the sweep's fall a block, Q16 */
    int32_t opc, opab;           /* the waveforms, Q15 along the drum waves */
    int32_t fb;
    int32_t fold, fwet;          /* the folder: its gain p^2 + 0.07 (Q15), its share min(16 p, 1) (Q15); 0 off */
    uint32_t ra16, rb16;         /* the ratios, Q16 */
    int32_t moda, modb, enda, endb, deca, decb;   /* Q15 amounts and ends; the decays a block, Q16 */
    uint16_t bhold, nhold;       /* blocks */
    int32_t bdec, ndec;          /* a block, Q16 (0x10000: none) */
    int32_t blev, tlev;          /* Q15 */
    int32_t nlev;                /* Q12 */
    int32_t phc;                 /* C's start (0 .. 90 degrees as a phase), -1 not reset */
    uint8_t nrst, nrm, tran;
    int32_t gran;                /* the grain's odds a sample, Q16 (0x10000: every sample) */
    int32_t gran_g;              /* .. and its level brought back (the square root of 1 / odds), Q12 */
    bwc_t nb;                    /* the noise's band */
} fdck_t;

typedef struct {
    uint32_t ph[3];
    int32_t fb1[3], fb2[3];
    int32_t sw;                  /* the sweep's envelope, Q15 (1 at the note's start) */
    int32_t ea, eb;              /* A's and B's envelopes, Q15 */
    int32_t be, ne;              /* the body's and the noise's envelopes, Q24 */
    uint16_t bh, nh;
    rnd_t rn;
    int32_t s1, s2;
    uint32_t tn;                 /* samples since the note began (the transient's clock) */
    uint32_t tph;
    int32_t tlp;
} fdv_t;

/* RATIO A, B: the Digitone II's, linear: 0.001 .. 31.75 (value x 256 x 31999 / 32768 + 1, thousandths), Q16 */
static uint32_t fd_ratio16(int32_t v)
{
    uint32_t th = ((uint32_t)clampi(v, 0, 127) * 256u * 31999u >> 15) + 1u;
    return th * 65536u / 1000u;
}
/* FM DRUM's times (the Digitone II's): the decays reach e^-6 in 8 (0.1 p + 0.9 p^4) s (+ 0.67 ms); the holds last 1000
 * p^2 ms; the sweep falls to e^-6 in 5 (0.1 p + 0.9 p^4) s. As the factor a block (2^(-8.656 x 32 / N)) */
static int32_t fd_curve(int32_t v)                          /* 0.1 p + 0.9 p^4, Q16 */
{
    int32_t p = clampi(v, 0, 127) * 65536 / 127, p2 = mulq16(p, p);
    return p / 10 + mulq16(mulq16(p2, p2), 58982);
}
static int32_t fd_fall(int32_t v, int32_t secs)              /* secs x curve, as the factor a block */
{
    int32_t ms = mulq16(fd_curve(v), secs * 1000) + 1, blocks = maxi(1, ms * 1378 / 1000);
    return (int32_t)exp2_q16(-(567280 / blocks));
}
static void fd_cook(fdck_t *c, const sound_t *s)
{
    const int16_t *p = s->syn;
    int32_t v;
    c->alg = &FD_ALG[clampi(p[FD_ALGO], 0, 6)];
    c->tune16 = semi16(clampi(p[FD_TUNE], 4, 124) - 64);
    v = clampi(p[FD_SDEP], 0, 127);
    c->sdep = v * v * 65536 / 16129;                        /* 16 p^2, Q12: up to 17 times the note (+49 semitones) */
    c->sfall = fd_fall(p[FD_STIM], 5);
    c->opc = clampi(p[FD_OPC], 0, 127) * Q15 / 127;
    c->opab = clampi(p[FD_OPAB], 0, 127) * Q15 / 127;
    v = clampi(p[FD_FDBK], 0, 127);
    c->fb = v * v * 20860 / 16129;                          /* p^2, 2 / pi of a cycle at full */
    v = clampi(p[FD_FOLD], 0, 127);
    c->fold = v ? v * v * Q15 / 16129 + 2294 : 0;           /* p^2 + 0.07 */
    c->fwet = clampi(v * 16 * Q15 / 127, 0, Q15);           /* min(16 p, 1) */
    c->ra16 = fd_ratio16(p[FD_RATA]);
    c->rb16 = fd_ratio16(p[FD_RATB]);
    v = clampi(p[FD_MODA], 0, 127);
    c->moda = v * v * Q15 / 16129;                          /* p^2 of a cycle */
    v = clampi(p[FD_MODB], 0, 127);
    c->modb = v * v * Q15 / 16129;
    c->enda = clampi(p[FD_ENDA], 0, 127) * Q15 / 127;
    c->endb = clampi(p[FD_ENDB], 0, 127) * Q15 / 127;
    c->deca = fd_fall(p[FD_DECA], 8);
    c->decb = fd_fall(p[FD_DECB], 8);
    v = clampi(p[FD_HOLD], 0, 127);
    c->bhold = (uint16_t)(v >= 127 ? 0xFFFF : v * v * 1378 / 16129);   /* 1000 p^2 ms; 127 for ever */
    v = clampi(p[FD_NHLD], 0, 127);
    c->nhold = (uint16_t)(v >= 127 ? 0xFFFF : v * v * 1378 / 16129);
    c->bdec = p[FD_DEC] >= 127 ? Q16 : fd_fall(p[FD_DEC], 8);
    c->ndec = p[FD_NDEC] >= 127 ? Q16 : fd_fall(p[FD_NDEC], 8);
    v = clampi(p[FD_LEV], 0, 127);
    c->blev = v * v * Q15 / 16129;                          /* p^2 */
    v = clampi(p[FD_NLEV], 0, 127);
    c->nlev = (int32_t)((uint32_t)v * (uint32_t)v * (uint32_t)v / 1000u * 8192u / 2048u);   /* 2 p^3, Q12 */
    v = clampi(p[FD_TLEV], 0, 127);
    c->tlev = v * v * Q15 / 16129;                          /* p^2 */
    c->phc = p[FD_PHC] >= 91 ? -1 : clampi(p[FD_PHC], 0, 90);
    c->nrst = (uint8_t)(p[FD_NRST] != 0);
    c->nrm = (uint8_t)(p[FD_NRM] != 0);
    c->tran = (uint8_t)clampi(p[FD_TRAN], 0, 127);
    {   /* GRAN: an impulse a sample at odds 0.0001 + (0.992 - p)^2 -- white at 0, sparse grains at the top */
        int32_t g = clampi(p[FD_GRAN], 0, 127), d = 32512 - g * Q15 / 127;   /* 0.992 - p, Q15 */
        c->gran = maxi(7, (int32_t)(((int64_t)d * d) >> 14) + 7);   /* Q16 */
        c->gran_g = clampi((int32_t)isqrt32((uint32_t)(Q16 << 8) / (uint32_t)maxi(c->gran, 1)) << 8, 4096, 8 * 4096);
    }
    bw_cook(&c->nb, p[FD_NBAS], p[FD_NWID]);
}
static void fd_on(fdv_t *v, const fdck_t *c, int fresh, uint32_t seed)
{
    if (fresh)
        memset(v, 0, sizeof *v);
    v->ph[FD_A] = v->ph[FD_B] = 0;
    if (c->phc >= 0)
        v->ph[FD_C] = (uint32_t)c->phc * 47721858u;          /* degrees: 2^32 / 360 */
    v->sw = Q15;
    v->ea = v->eb = Q15;
    v->be = v->ne = 1 << 24;
    v->bh = c->bhold;
    v->nh = c->nhold;
    if (c->nrst || fresh)
        rnd_seed(&v->rn, c->nrst ? 0xD1u : seed);
    v->tn = 0;
    v->tph = 0;
    v->tlp = 0;
}

/* the transients: short attacks drawn by type (TRAN 0 .. 127, eight kinds across the range) at sample n since the
 * note began; 0 once over */
static int32_t fd_transient(fdv_t *v, uint32_t kind, uint32_t n)
{
    int32_t env, x;
    uint32_t len = 220u + kind * 90u;                       /* 5 .. 20 ms */
    if (n >= len)
        return 0;
    env = (int32_t)((len - n) * (uint32_t)Q15 / len);
    env = mulq15(env, env);
    switch (kind) {
    case 0:   /* a click: two samples */
        return n < 2u ? (n ? -24000 : 30000) : 0;
    case 1:   /* a tick: a high sine burst */
        v->tph += 0x1C000000u;
        return mulq15(sinr(v->tph), env);
    case 2:   /* a noise snap */
        return mulq15(rnd_bi(&v->rn), env);
    case 3:   /* a pop: a low sine falling */
        v->tph += (1u << 25) + (len - n) * 0x8000u;
        return mulq15(sinr(v->tph), env);
    case 4:   /* a zap: a fast downward chirp */
        v->tph += 0x00800000u + (len - n) * 0x30000u;
        return mulq15(sinr(v->tph), env);
    case 5:   /* a thump: a low-passed noise burst */
        x = rnd_bi(&v->rn);
        v->tlp += mulq15(x - v->tlp, 2400);
        return mulq15(v->tlp * 3, env);
    case 6:   /* metal: a square at an odd high pitch */
        v->tph += 0x0F3B0000u;
        return mulq15((int32_t)(v->tph >> 31) ? 20000 : -20000, env);
    default:  /* a burst of clicks */
        return (n % 90u) < 3u ? mulq15(26000, env) : 0;
    }
}

AINL int32_t fd_wave(const morph_t *m, uint32_t ph) { return morph_rd(m, ph); }
/* the folder (the Digitone II's shape): u = clip(g x), 1.12 u - 0.672 tanh(4 u), mixed in by its share */
AINL int32_t fd_folder(int32_t x, int32_t g, int32_t wet)
{
    int32_t u = clampi(mulq15(x, g), -Q15, Q15), y = mulq15(u, 36700) - mulq15(tanh_q15(u * 4), 22007);
    return x + mulq15(y * 2 - x, wet);                       /* (x 2: the shape peaks near half scale) */
}

static void fd_render(fdv_t *v, const fdck_t *c, int32_t note16, int32_t *out)
{
    const fdalg_t *g = c->alg;
    uint32_t inc, ic[3], i, k;
    int32_t be0 = v->be, ne0 = v->ne, be1, ne1, dbe, dne, ea0 = v->ea, eb0 = v->eb, dea, deb, b, n;
    morph_t wv[3];
    inc = note_inc(note16 + c->tune16);                      /* the sweep: x (1 + 16 p^2 env), linear in Hz */
    inc = (uint32_t)clampi((int32_t)(((uint64_t)inc * (uint32_t)(4096 + mulq15(c->sdep, v->sw))) >> 12), 0, 0x3FFFFFFF);
    v->sw = (int32_t)(((int64_t)v->sw * c->sfall) >> 16);
    ic[FD_C] = inc;
    ic[FD_A] = (uint32_t)clampi((int32_t)(((uint64_t)inc * c->ra16) >> 16), 0, 0x7FFFFFFF);
    ic[FD_B] = (uint32_t)clampi((int32_t)(((uint64_t)inc * c->rb16) >> 16), 0, 0x7FFFFFFF);
    morph_sc(&wv[FD_C], SC_DRUM0, SC_DRUM_N, c->opc);
    morph_sc(&wv[FD_A], SC_DRUM0, SC_DRUM_N, c->opab);
    wv[FD_B] = wv[FD_A];
    /* the envelopes, a block on */
    v->ea = c->enda + (int32_t)(((int64_t)(v->ea - c->enda) * c->deca) >> 16);
    v->eb = c->endb + (int32_t)(((int64_t)(v->eb - c->endb) * c->decb) >> 16);
    if (v->bh) {
        if (v->bh != 0xFFFF)
            v->bh--;
    } else
        v->be = (int32_t)(((int64_t)v->be * c->bdec) >> 16);
    if (v->nh) {
        if (v->nh != 0xFFFF)
            v->nh--;
    } else
        v->ne = (int32_t)(((int64_t)v->ne * c->ndec) >> 16);
    be1 = v->be;
    ne1 = v->ne;
    b = be0 >> 9;
    n = ne0 >> 9;
    dbe = ((be1 >> 9) - b) / (int32_t)CTL;
    dne = ((ne1 >> 9) - n) / (int32_t)CTL;
    dea = (v->ea - ea0) / (int32_t)CTL;
    deb = (v->eb - eb0) / (int32_t)CTL;
    for (i = 0; i < CTL; i++) {
        int32_t o[3] = {0, 0, 0}, lv[3], body, nz, y;
        b += dbe;
        n += dne;
        ea0 += dea;
        eb0 += deb;
        lv[FD_C] = 0;
        lv[FD_A] = mulq15(c->moda, ea0);
        lv[FD_B] = mulq15(c->modb, eb0);
        for (k = 0; k < 3u; k++) {
            uint32_t op = g->order[k], ph = v->ph[op], s;
            for (s = 0; s < 3u; s++)
                if ((g->mod[op] >> s) & 1u)
                    ph += (uint32_t)mulq15(o[s], lv[s]) << 17;   /* (a cycle at full) */
            if ((g->fb >> op) & 1u && c->fb)
                ph += (uint32_t)mulq15((v->fb1[op] + v->fb2[op]) >> 1, c->fb) << 17;
            o[op] = fd_wave(&wv[op], ph);
            v->fb2[op] = v->fb1[op];
            v->fb1[op] = o[op];
            v->ph[op] += ic[op];
        }
        body = mulq15(o[FD_C], b);                           /* (the body's envelope on C; B, heard in 5 .. 7, after
                                                              * the folder) */
        if (c->fold)
            body = fd_folder(body, c->fold, c->fwet);
        if (g->bout)
            body = (body + mulq15(o[FD_B], eb0)) >> 1;
        body = mulq15(body, c->blev);
        /* the noise: white, or grains (a fresh value now and then, silence between) */
        if (c->gran >= Q16 || (int32_t)(rnd_next(&v->rn) >> 16) < c->gran)
            nz = rnd_bi(&v->rn);
        else
            nz = 0;
        if (c->gran < Q16)
            nz = mulq12(nz, c->gran_g);
        if (c->nrm)
            nz = mulq15(nz, o[FD_C]);
        nz = bw_run(&v->s1, &v->s2, &c->nb, nz);
        nz = mulq12(mulq15(nz, n), c->nlev);
        y = body + nz;
        if (c->tlev)
            y += mulq15(fd_transient(v, c->tran / 16u, v->tn), c->tlev);
        v->tn++;
        out[i] = y;
    }
}
