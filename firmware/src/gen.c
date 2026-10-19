/* SPDX-License-Identifier: GPL-3.0-only
 * HOME: a new sound for the chosen track, on its machine, drawn from a recipe (a family: a kick, a hat, an FM
 * bass, a swarm pad ..), its values drawn within the recipe's ranges. SHIFT + HOME draws again from the family
 * the track plays. The recipes are ported from SynthSeeder (the same author's patch generator for the Digitone
 * II), whose ranges were set against the Digitone II's factory sounds and listening tests; they are written in
 * the Digitone II's own units, as the sounds here are. Each draw also leans, at random, towards three tastes --
 * industrial, IDM, abstract -- as SynthSeeder's rolls do.
 * (LFOs are not in this firmware yet: the recipes' LFO lines are left out.) */

typedef struct { rnd_t r; int32_t ind, idm, abs_; } gctx_t;   /* the tastes, 0 .. 100 */
static int32_t RI(gctx_t *g, int32_t a, int32_t b) { return a + rnd_below(&g->r, b - a + 1); }
static int CH(gctx_t *g, int32_t pct) { return rnd_below(&g->r, 100) < pct; }
#define PICK(g, ...) pick_(g, (const int16_t[]){__VA_ARGS__}, sizeof((const int16_t[]){__VA_ARGS__}) / sizeof(int16_t))
static int32_t pick_(gctx_t *g, const int16_t *v, uint32_t n) { return v[rnd_below(&g->r, (int32_t)n)]; }
#define SY(s, k, v) ((s)->syn[k] = (int16_t)(v))

/* ---------------------------------------------------------- sections --- */
enum { SH_LP, SH_BP, SH_HP };
static void amp_open(sound_t *s, int32_t dec)
{
    s->amp[A_MODE] = 0; s->amp[A_ATK] = 0; s->amp[A_HOLD] = 127; s->amp[A_DEC] = (uint8_t)dec;
    s->amp[A_SUS] = 127; s->amp[A_REL] = 40; s->amp[A_RSET] = 1;
}
static void amp_hit(sound_t *s, int32_t atk, int32_t hold, int32_t dec)
{
    s->amp[A_MODE] = 0; s->amp[A_ATK] = (uint8_t)atk; s->amp[A_HOLD] = (uint8_t)hold; s->amp[A_DEC] = (uint8_t)dec;
    s->amp[A_SUS] = 0; s->amp[A_REL] = 0; s->amp[A_RSET] = 1;
}
static void amp_adsr(sound_t *s, int32_t a, int32_t d, int32_t su, int32_t r)
{
    s->amp[A_MODE] = 1; s->amp[A_ATK] = (uint8_t)a; s->amp[A_HOLD] = 127; s->amp[A_DEC] = (uint8_t)d;
    s->amp[A_SUS] = (uint8_t)su; s->amp[A_REL] = (uint8_t)r; s->amp[A_RSET] = 1;
}
static void band(sound_t *s, int32_t base, int32_t width) { s->flt[F_BASE] = (uint8_t)base; s->flt[F_WDTH] = (uint8_t)width; }
static void filt(sound_t *s, int shape, int32_t cut, int32_t res, int32_t envd, int32_t ea, int32_t ed, int32_t es, int32_t er, int32_t kt)
{
    s->flt[F_FREQ] = (uint8_t)cut;
    s->flt[F_RESO] = (uint8_t)res;
    s->flt[F_TYPE] = s->fmach == FLT_MM ? (uint8_t)(shape == SH_LP ? 0 : shape == SH_BP ? 64 : 127)
                     : s->fmach == FLT_LEG ? (uint8_t)(shape == SH_HP) : 0;
    s->flt[F_ENV] = (uint8_t)(64 + envd);
    s->flt[F_ATK] = (uint8_t)ea; s->flt[F_DEC] = (uint8_t)ed; s->flt[F_SUS] = (uint8_t)es; s->flt[F_REL] = (uint8_t)er;
    s->flt[F_DEL] = 0;
    s->flt[F_KEYT] = (uint8_t)kt;
    s->flt[F_RSET] = 1;
    band(s, 0, 127);
}
#define FILT(s, sh, cut, res) filt(s, sh, cut, res, 0, 0, 64, 0, 64, 0)
static void sends(sound_t *s, int32_t d, int32_t r, int32_t c) { s->fx[X_DEL] = (uint8_t)d; s->fx[X_REV] = (uint8_t)r; s->fx[X_CHR] = (uint8_t)c; }
static void dirt(sound_t *s, int32_t drive, int32_t bits, int32_t rate, int dpost, int rpost)
{
    s->fx[X_OD] = (uint8_t)drive; s->fx[X_ODRT] = (uint8_t)dpost; s->fx[X_BR] = (uint8_t)bits;
    s->fx[X_SRR] = (uint8_t)rate; s->fx[X_SRRT] = (uint8_t)rpost;
}
static void level(sound_t *s, int32_t lv)   /* the recipe's volume, less for drive and high resonance */
{
    int32_t v = lv - s->fx[X_OD] / 5;
    if (s->fmach == FLT_MM || s->fmach == FLT_LP4 || s->fmach == FLT_LEG)
        v -= maxi(0, s->flt[F_RESO] - 80) / 4;
    s->amp[A_VOL] = (uint8_t)clampi(v, 40, 124);
}
static void pan(sound_t *s, int32_t p) { s->amp[A_PAN] = (uint8_t)(64 + p); }

/* ------------------------------------------------------------- bases --- */
static void drum_base(sound_t *s)
{
    static const int16_t D0[32] = {64, 0, 0, 0, 0, 0, 0, 0, 0, 20, 0, 0, 0, 20, 0, 0,
                                   0, 40, 0, 110, 0, 0, 1, 0, 0, 20, 0, 0, 40, 60, 0, 0};
    memcpy(s->syn, D0, sizeof D0);
    amp_open(s, 64);
    pan(s, 0);
    sends(s, 0, 0, 0);
    dirt(s, 0, 0, 0, 0, 0);
}
static void noise_base(sound_t *s)
{
    static const int16_t W0[32] = {64, 0, 50, 0, 64, 0, 50, 0, 50, 0, 0, 1, 50, 0, 0, 0,
                                   0, 0, 30, 120, 50, 80, 0, 0};
    memset(s->syn, 0, sizeof s->syn);
    memcpy(s->syn, W0, sizeof W0);
    pan(s, 0);
    sends(s, 0, 0, 0);
    dirt(s, 0, 0, 0, 0, 0);
}
static void common_base(sound_t *s)
{
    sends(s, 0, 0, 0);
    dirt(s, 0, 0, 0, 0, 0);
    pan(s, 0);
}
#define RC_HALF 1
#define RC_ONE 3
#define RC_TWO 4
#define RC_THREE 5
#define RATB(b1, b2) ((b1) * 19 + (b2))
static void fm_base(sound_t *s)
{
    static const int16_t F0[32] = {0, RC_ONE, 4, RATB(RC_ONE, RC_TWO), 63, 0, 0, 64, 0, 40, 0, 0, 0, 40, 0, 0,
                                   0, 1, 1, 1, 0, 1, 1, 0, 64, 64, 64, 64, 0, 0, 0, 0};
    memcpy(s->syn, F0, sizeof F0);
    common_base(s);
}
static void wt_base(sound_t *s)
{
    static const int16_t W0[32] = {64, 0, 50, 110, 64, 0, 50, 0, 50, 0, 0, 1, 50, 0, 0, 0,
                                   0, 127, 127, 0, 40, 127, 0, 0};
    memset(s->syn, 0, sizeof s->syn);
    memcpy(s->syn, W0, sizeof W0);
    common_base(s);
}
static void sw_base(sound_t *s)
{
    static const int16_t S0[8] = {64, 60, 40, 110, 1, 60, 30, 0};
    memset(s->syn, 0, sizeof s->syn);
    memcpy(s->syn, S0, sizeof S0);
    common_base(s);
}
/* the FM ratio A list's index nearest r (x 4) */
static int32_t ra_ix(int32_t r4)
{
    int32_t i, b = 0;
    for (i = 0; i < 36; i++)
        if (absi(FT_RA4[i] - r4) < absi(FT_RA4[b] - r4))
            b = i;
    return b;
}
static void harmonic_fm(sound_t *s, gctx_t *g, int32_t bright)   /* bright 0 .. 100 */
{
#define LV(lo, hi) ((lo) + ((hi) - (lo)) * bright / 100)
    SY(s, FT_ALGO, PICK(g, 1, 2, 3, 5));
    SY(s, FT_RATC, PICK(g, RC_ONE, RC_ONE, RC_TWO));
    SY(s, FT_RATA, ra_ix(4 * PICK(g, 1, 2, 2, 3, 4)));
    SY(s, FT_RATB, RATB(PICK(g, RC_ONE, RC_TWO), PICK(g, RC_ONE, RC_TWO, RC_THREE)));
    SY(s, FT_HARM, clampi(63 + RI(g, -10, 10), 50, 76));
    SY(s, FT_ALEV, RI(g, LV(20, 60), LV(60, 110)));
    SY(s, FT_BLEV, RI(g, LV(10, 50), LV(50, 100)));
#undef LV
}
static void drum_filter(sound_t *s, gctx_t *g, int shape, int32_t c0, int32_t c1, int32_t r0, int32_t r1, int click, int32_t trim)
{
    filt(s, shape, RI(g, c0, c1), RI(g, r0, r1), click ? RI(g, 8, 18) : 0, 0, click ? RI(g, 18, 34) : 64, 0, 40, 0);
    band(s, trim, 127);
}

/* ----------------------------------------------------------- recipes --- */
typedef struct { const char *name; uint8_t mach, fmach, amb; void (*fn)(sound_t *, gctx_t *); } recipe_t;   /* (amb: a
                                                                                                  * dark-ambient family, left clean) */

static void r_tight_kick(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 36, 44)); SY(s, FD_STIM, RI(g, 18, 38)); SY(s, FD_SDEP, RI(g, 60, 95));
    SY(s, FD_MODA, RI(g, 0, 18)); SY(s, FD_DECA, RI(g, 8, 20)); SY(s, FD_RATA, RI(g, 0, 8));
    SY(s, FD_HOLD, RI(g, 6, 20)); SY(s, FD_DEC, RI(g, 50, 66)); SY(s, FD_LEV, 124);
    SY(s, FD_TRAN, RI(g, 0, 40)); SY(s, FD_TLEV, RI(g, 40, 75));
    SY(s, FD_NDEC, RI(g, 8, 16)); SY(s, FD_NLEV, RI(g, 0, 14)); SY(s, FD_NBAS, RI(g, 50, 80));
    drum_filter(s, g, SH_LP, 78, 100, 0, 15, 1, 0);
    level(s, 122);
}
static void r_deep_kick(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 34, 39)); SY(s, FD_STIM, RI(g, 30, 50)); SY(s, FD_SDEP, RI(g, 55, 80));
    SY(s, FD_MODA, RI(g, 0, 12)); SY(s, FD_DECA, RI(g, 8, 20));
    SY(s, FD_HOLD, RI(g, 15, 30)); SY(s, FD_DEC, RI(g, 62, 76)); SY(s, FD_LEV, 124);
    SY(s, FD_TRAN, RI(g, 0, 30)); SY(s, FD_TLEV, RI(g, 30, 60));
    drum_filter(s, g, SH_LP, 70, 90, 0, 15, 1, 0);
    level(s, 122);
}
static void r_driven_kick(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 35, 43)); SY(s, FD_STIM, RI(g, 20, 40)); SY(s, FD_SDEP, RI(g, 65, 100));
    SY(s, FD_FOLD, RI(g, 10, 35)); SY(s, FD_MODA, RI(g, 15, 40)); SY(s, FD_DECA, RI(g, 10, 25)); SY(s, FD_RATA, RI(g, 0, 10));
    SY(s, FD_HOLD, RI(g, 6, 18)); SY(s, FD_DEC, RI(g, 46, 58)); SY(s, FD_LEV, 122);
    SY(s, FD_TRAN, RI(g, 20, 80)); SY(s, FD_TLEV, RI(g, 55, 90));
    SY(s, FD_NDEC, RI(g, 14, 26)); SY(s, FD_GRAN, RI(g, 40, 90)); SY(s, FD_NLEV, RI(g, 10, 28));
    SY(s, FD_NBAS, RI(g, 40, 70)); SY(s, FD_NWID, RI(g, 50, 90));
    drum_filter(s, g, SH_LP, 85, 105, 0, 15, 1, 0);
    dirt(s, RI(g, 45, 80), 0, 0, 1, 0);
    level(s, 124);
}
static void r_click_kick(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 38, 46)); SY(s, FD_STIM, RI(g, 4, 14)); SY(s, FD_SDEP, RI(g, 90, 124));
    SY(s, FD_MODA, RI(g, 25, 55)); SY(s, FD_DECA, RI(g, 3, 9)); SY(s, FD_RATA, RI(g, 10, 30));
    SY(s, FD_HOLD, RI(g, 0, 8)); SY(s, FD_DEC, RI(g, 42, 56)); SY(s, FD_LEV, 124);
    SY(s, FD_TRAN, RI(g, 50, 110)); SY(s, FD_TLEV, RI(g, 75, 105));
    drum_filter(s, g, SH_LP, 95, 115, 0, 15, 0, 0);
    level(s, 120);
}
static void fm_snare(sound_t *s, gctx_t *g, int tight)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 58, 72)); SY(s, FD_STIM, RI(g, 0, 10)); SY(s, FD_SDEP, RI(g, 0, 25));
    SY(s, FD_ALGO, PICK(g, 0, 3, 3, 6)); SY(s, FD_OPC, RI(g, 35, 65)); SY(s, FD_OPAB, RI(g, 40, 65));
    SY(s, FD_FDBK, tight ? RI(g, 10, 30) : RI(g, 25, 45)); SY(s, FD_FOLD, RI(g, 0, 20));
    SY(s, FD_RATA, RI(g, 0, 12)); SY(s, FD_DECA, RI(g, 55, 85)); SY(s, FD_MODA, RI(g, 100, 127));
    SY(s, FD_RATB, RI(g, 0, 20)); SY(s, FD_DECB, RI(g, 65, 92)); SY(s, FD_MODB, RI(g, 95, 127));
    SY(s, FD_HOLD, 0); SY(s, FD_DEC, tight ? RI(g, 22, 30) : RI(g, 28, 38)); SY(s, FD_LEV, RI(g, 96, 110));
    SY(s, FD_NRST, 0); SY(s, FD_NHLD, RI(g, 0, 6)); SY(s, FD_NDEC, tight ? RI(g, 30, 40) : RI(g, 38, 52));
    SY(s, FD_TRAN, RI(g, 10, 60)); SY(s, FD_TLEV, RI(g, 60, 95));
    SY(s, FD_NBAS, RI(g, 0, 30)); SY(s, FD_NWID, RI(g, 100, 127)); SY(s, FD_GRAN, RI(g, 20, 60));
    SY(s, FD_NLEV, RI(g, 80, 104));
    drum_filter(s, g, SH_LP, 122, 127, 0, 15, 0, RI(g, 18, 34));
}
static void r_fm_snare(sound_t *s, gctx_t *g)
{
    fm_snare(s, g, 0);
    sends(s, 0, g->abs_ * RI(g, 0, 30) / 100, 0);
    level(s, 112);
}
static void r_tight_snare(sound_t *s, gctx_t *g) { fm_snare(s, g, 1); level(s, 116); }
static void r_clap(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 66, 80)); SY(s, FD_LEV, RI(g, 30, 50)); SY(s, FD_DEC, RI(g, 16, 26));
    SY(s, FD_MODA, RI(g, 40, 80)); SY(s, FD_DECA, RI(g, 10, 25));
    SY(s, FD_NRST, 0); SY(s, FD_NHLD, RI(g, 4, 10)); SY(s, FD_NDEC, RI(g, 38, 50));
    SY(s, FD_NBAS, RI(g, 40, 64)); SY(s, FD_NWID, RI(g, 70, 110)); SY(s, FD_GRAN, RI(g, 60, 90));
    SY(s, FD_NLEV, 127); SY(s, FD_TRAN, RI(g, 60, 110)); SY(s, FD_TLEV, RI(g, 50, 80));
    drum_filter(s, g, SH_LP, 108, 124, 0, 15, 0, RI(g, 34, 48));
    sends(s, 0, RI(g, 8, 24) + g->abs_ * 20 / 100, 0);
    level(s, 126);
}
static void r_dirty_snare(sound_t *s, gctx_t *g)
{
    fm_snare(s, g, 0);
    SY(s, FD_FOLD, RI(g, 20, 45)); SY(s, FD_GRAN, RI(g, 60, 100));
    dirt(s, RI(g, 40, 70), CH(g, 40) ? RI(g, 10, 25) : 0, 0, 1, 0);
    level(s, 118);
}
static void metal_hat(sound_t *s, gctx_t *g, int32_t b0, int32_t b1, int32_t n0, int32_t n1)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 76, 92)); SY(s, FD_ALGO, RI(g, 0, 1));
    SY(s, FD_OPC, RI(g, 25, 85)); SY(s, FD_OPAB, RI(g, 48, 110)); SY(s, FD_FDBK, RI(g, 0, 30));
    SY(s, FD_RATA, CH(g, 50) ? RI(g, 7, 35) : RI(g, 90, 110)); SY(s, FD_DECA, RI(g, 0, 20)); SY(s, FD_ENDA, RI(g, 100, 127));
    SY(s, FD_MODA, RI(g, 60, 110));
    SY(s, FD_RATB, RI(g, 18, 70)); SY(s, FD_DECB, RI(g, 0, 20)); SY(s, FD_ENDB, RI(g, 70, 110)); SY(s, FD_MODB, RI(g, 60, 115));
    SY(s, FD_DEC, RI(g, b0, b1)); SY(s, FD_PHC, PICK(g, 0, 76)); SY(s, FD_LEV, RI(g, 96, 106));
    SY(s, FD_NRST, 1); SY(s, FD_NHLD, 0); SY(s, FD_NDEC, RI(g, n0, n1));
    SY(s, FD_TRAN, RI(g, 0, 40)); SY(s, FD_TLEV, RI(g, 0, 60));
    SY(s, FD_NBAS, RI(g, 76, 106)); SY(s, FD_NWID, RI(g, 40, 95)); SY(s, FD_GRAN, RI(g, 0, 50));
    SY(s, FD_NLEV, RI(g, 60, 95));
}
static void r_metal_hat(sound_t *s, gctx_t *g)
{
    metal_hat(s, g, 11, 26, 14, 32);
    drum_filter(s, g, SH_HP, 72, 104, 0, 25, 0, RI(g, 60, 86));
    pan(s, RI(g, -8, 8));
    level(s, 114);
}
static void r_open_hat(sound_t *s, gctx_t *g)
{
    metal_hat(s, g, 52, 72, 56, 80);
    SY(s, FD_MODA, RI(g, 100, 127)); SY(s, FD_MODB, RI(g, 100, 127));
    drum_filter(s, g, SH_HP, 80, 104, 0, 20, 0, RI(g, 56, 80));
    pan(s, RI(g, -12, 12));
    level(s, 110);
}
static void r_ride(sound_t *s, gctx_t *g)
{
    metal_hat(s, g, 72, 96, 60, 90);
    SY(s, FD_TUNE, RI(g, 80, 100)); SY(s, FD_NLEV, RI(g, 40, 70));
    drum_filter(s, g, SH_HP, 76, 100, 0, 20, 0, RI(g, 52, 76));
    sends(s, 0, g->abs_ * RI(g, 0, 30) / 100, 0);
    pan(s, RI(g, -14, 14));
    level(s, 108);
}
static void r_cymbal(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 110, 124)); SY(s, FD_MODA, 127); SY(s, FD_MODB, 127); SY(s, FD_ENDA, 127); SY(s, FD_ENDB, 127);
    SY(s, FD_RATA, RI(g, 4, 12)); SY(s, FD_RATB, RI(g, 4, 12)); SY(s, FD_DEC, RI(g, 80, 110)); SY(s, FD_LEV, 88);
    SY(s, FD_NHLD, 0); SY(s, FD_NDEC, RI(g, 80, 110)); SY(s, FD_NBAS, RI(g, 60, 80));
    SY(s, FD_NWID, RI(g, 20, 40)); SY(s, FD_GRAN, RI(g, 60, 100)); SY(s, FD_NLEV, RI(g, 60, 85));
    drum_filter(s, g, SH_HP, 84, 104, 0, 15, 0, RI(g, 50, 70));
    pan(s, RI(g, -14, 14));
    level(s, 106);
}
static void r_tom(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 46, 64)); SY(s, FD_STIM, RI(g, 12, 26)); SY(s, FD_SDEP, RI(g, 18, 38));
    SY(s, FD_MODA, RI(g, 0, 25)); SY(s, FD_DECA, RI(g, 10, 30));
    SY(s, FD_HOLD, RI(g, 0, 10)); SY(s, FD_DEC, RI(g, 48, 64)); SY(s, FD_LEV, 124);
    SY(s, FD_TRAN, RI(g, 10, 50)); SY(s, FD_TLEV, RI(g, 40, 90)); SY(s, FD_NLEV, RI(g, 0, 30));
    SY(s, FD_NDEC, RI(g, 15, 30)); SY(s, FD_NBAS, RI(g, 60, 90));
    drum_filter(s, g, SH_LP, 100, 124, 0, 15, 0, RI(g, 8, 20));
    sends(s, 0, g->abs_ * RI(g, 0, 30) / 100, 0);
    pan(s, RI(g, -16, 16));
    level(s, 118);
}
static void r_rim(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 76, 88)); SY(s, FD_LEV, RI(g, 0, 60)); SY(s, FD_DEC, RI(g, 10, 18));
    SY(s, FD_MODA, RI(g, 0, 40)); SY(s, FD_DECA, RI(g, 0, 10));
    SY(s, FD_NHLD, RI(g, 4, 9)); SY(s, FD_NDEC, RI(g, 4, 10)); SY(s, FD_NLEV, 127);
    SY(s, FD_TRAN, RI(g, 60, 110)); SY(s, FD_TLEV, RI(g, 40, 90));
    SY(s, FD_NBAS, RI(g, 64, 78)); SY(s, FD_NWID, RI(g, 12, 22)); SY(s, FD_GRAN, RI(g, 0, 12));
    drum_filter(s, g, SH_BP, 84, 100, 30, 60, 0, RI(g, 30, 45));
    pan(s, RI(g, -18, 18));
    level(s, 108);
}
static void r_fm_perc(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 68, 86)); SY(s, FD_SDEP, RI(g, 40, 75)); SY(s, FD_STIM, RI(g, 0, 10));
    SY(s, FD_RATA, RI(g, 6, 14)); SY(s, FD_DECA, RI(g, 4, 20)); SY(s, FD_MODA, RI(g, 80, 110));
    SY(s, FD_DECB, RI(g, 15, 30)); SY(s, FD_ENDB, RI(g, 0, 120)); SY(s, FD_MODB, RI(g, 100, 127));
    SY(s, FD_HOLD, RI(g, 0, 16)); SY(s, FD_DEC, RI(g, 20, 40)); SY(s, FD_LEV, 110);
    SY(s, FD_NHLD, RI(g, 0, 17)); SY(s, FD_NDEC, RI(g, 18, 28)); SY(s, FD_NLEV, RI(g, 60, 106));
    SY(s, FD_NBAS, RI(g, 70, 90)); SY(s, FD_NWID, RI(g, 10, 30)); SY(s, FD_TRAN, RI(g, 50, 90));
    drum_filter(s, g, SH_LP, 96, 120, 0, 15, 0, RI(g, 30, 50));
    pan(s, RI(g, -18, 18));
    level(s, 114);
}
static void r_clang(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 62, 84)); SY(s, FD_ALGO, RI(g, 0, 3));
    SY(s, FD_OPC, RI(g, 20, 80)); SY(s, FD_OPAB, RI(g, 40, 100)); SY(s, FD_FDBK, RI(g, 0, 35)); SY(s, FD_FOLD, RI(g, 10, 40));
    SY(s, FD_RATA, RI(g, 20, 60)); SY(s, FD_DECA, RI(g, 30, 60)); SY(s, FD_ENDA, RI(g, 40, 100)); SY(s, FD_MODA, RI(g, 70, 110));
    SY(s, FD_RATB, RI(g, 30, 90)); SY(s, FD_DECB, RI(g, 30, 60)); SY(s, FD_ENDB, RI(g, 40, 100)); SY(s, FD_MODB, RI(g, 70, 110));
    SY(s, FD_HOLD, RI(g, 0, 10)); SY(s, FD_DEC, RI(g, 40, 60)); SY(s, FD_LEV, 116);
    SY(s, FD_NDEC, RI(g, 24, 40)); SY(s, FD_NLEV, RI(g, 40, 80)); SY(s, FD_GRAN, RI(g, 60, 100));
    SY(s, FD_NBAS, RI(g, 60, 90)); SY(s, FD_TRAN, RI(g, 50, 110)); SY(s, FD_TLEV, RI(g, 70, 105));
    drum_filter(s, g, SH_LP, 100, 124, 0, 20, 0, RI(g, 30, 50));
    sends(s, 0, g->abs_ * RI(g, 0, 30) / 100, 0);
    dirt(s, RI(g, 30, 60), 0, 0, 1, 0);
    pan(s, RI(g, -14, 14));
    level(s, 116);
}
static void r_slam(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 52, 66)); SY(s, FD_STIM, RI(g, 5, 20)); SY(s, FD_SDEP, RI(g, 30, 60));
    SY(s, FD_FOLD, RI(g, 10, 30)); SY(s, FD_MODA, RI(g, 30, 60)); SY(s, FD_DECA, RI(g, 10, 25));
    SY(s, FD_DEC, RI(g, 26, 40)); SY(s, FD_LEV, 110);
    SY(s, FD_NHLD, RI(g, 4, 14)); SY(s, FD_NDEC, RI(g, 34, 50)); SY(s, FD_NLEV, 124);
    SY(s, FD_NBAS, RI(g, 30, 55)); SY(s, FD_NWID, RI(g, 70, 110)); SY(s, FD_GRAN, RI(g, 80, 115));
    SY(s, FD_TRAN, RI(g, 70, 120)); SY(s, FD_TLEV, RI(g, 80, 110));
    drum_filter(s, g, SH_LP, 105, 125, 0, 15, 0, RI(g, 28, 42));
    dirt(s, RI(g, 50, 80), CH(g, 35) ? RI(g, 8, 22) : 0, 0, 1, 0);
    pan(s, RI(g, -12, 12));
    level(s, 118);
}
static void r_stamp(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 44, 56)); SY(s, FD_STIM, RI(g, 8, 20)); SY(s, FD_SDEP, RI(g, 60, 90));
    SY(s, FD_FOLD, RI(g, 20, 45)); SY(s, FD_MODA, RI(g, 40, 70)); SY(s, FD_DECA, RI(g, 8, 20)); SY(s, FD_RATA, RI(g, 0, 20));
    SY(s, FD_HOLD, RI(g, 0, 8)); SY(s, FD_DEC, RI(g, 34, 48)); SY(s, FD_LEV, 120);
    SY(s, FD_NDEC, RI(g, 16, 28)); SY(s, FD_GRAN, RI(g, 90, 120)); SY(s, FD_NLEV, RI(g, 50, 80));
    SY(s, FD_NBAS, RI(g, 50, 80)); SY(s, FD_TRAN, RI(g, 80, 124)); SY(s, FD_TLEV, RI(g, 90, 115));
    drum_filter(s, g, SH_LP, 96, 118, 0, 15, 1, RI(g, 14, 26));
    dirt(s, RI(g, 55, 85), 0, 0, 1, 0);
    level(s, 118);
}
static void r_crunch(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 80, 100)); SY(s, FD_LEV, RI(g, 60, 90)); SY(s, FD_DEC, RI(g, 8, 16));
    SY(s, FD_MODA, RI(g, 60, 100)); SY(s, FD_RATA, RI(g, 30, 70)); SY(s, FD_DECA, RI(g, 4, 12));
    SY(s, FD_NDEC, RI(g, 8, 16)); SY(s, FD_NLEV, RI(g, 90, 120)); SY(s, FD_GRAN, RI(g, 80, 120));
    SY(s, FD_NBAS, RI(g, 70, 95)); SY(s, FD_TRAN, RI(g, 80, 124)); SY(s, FD_TLEV, RI(g, 80, 110));
    drum_filter(s, g, SH_LP, 100, 124, 0, 15, 0, RI(g, 45, 65));
    dirt(s, 0, RI(g, 15, 35), 0, 0, 0);
    pan(s, RI(g, -24, 24));
    level(s, 112);
}
static void r_cowbell(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 74, 90)); SY(s, FD_ALGO, PICK(g, 2, 3)); SY(s, FD_OPC, RI(g, 40, 70)); SY(s, FD_OPAB, RI(g, 40, 70));
    SY(s, FD_RATA, RI(g, 24, 40)); SY(s, FD_DECA, RI(g, 30, 50)); SY(s, FD_ENDA, RI(g, 30, 60)); SY(s, FD_MODA, RI(g, 30, 60));
    SY(s, FD_DEC, RI(g, 26, 40)); SY(s, FD_LEV, 120);
    SY(s, FD_TRAN, RI(g, 50, 100)); SY(s, FD_TLEV, RI(g, 40, 80));
    drum_filter(s, g, SH_BP, 92, 108, 10, 35, 0, RI(g, 40, 56));
    pan(s, RI(g, -18, 18));
    level(s, 114);
}
static void r_noise_engine(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 56, 96)); SY(s, FD_ALGO, RI(g, 0, 6)); SY(s, FD_FDBK, RI(g, 10, 60)); SY(s, FD_FOLD, RI(g, 0, 90));
    SY(s, FD_RATA, RI(g, 0, 90)); SY(s, FD_DECA, 127); SY(s, FD_ENDA, RI(g, 30, 100)); SY(s, FD_MODA, RI(g, 20, 90));
    SY(s, FD_HOLD, 127); SY(s, FD_DEC, 127); SY(s, FD_LEV, RI(g, 0, 60));
    SY(s, FD_NRM, CH(g, 50)); SY(s, FD_NHLD, 127); SY(s, FD_NDEC, 127);
    SY(s, FD_NBAS, RI(g, 20, 90)); SY(s, FD_NWID, RI(g, 20, 80)); SY(s, FD_GRAN, RI(g, 50, 127));
    SY(s, FD_NLEV, RI(g, 110, 127));
    amp_adsr(s, RI(g, 15, 70), 127, 127, RI(g, 60, 110));
    filt(s, SH_BP, RI(g, 70, 100), RI(g, 15, 45), 0, 0, 64, 0, 64, 0);
    band(s, RI(g, 22, 40), 127);
    sends(s, RI(g, 10, 45), RI(g, 40, 90), 0);
    dirt(s, g->ind * RI(g, 20, 70) / 100, 0, 0, 0, 0);
    level(s, 108);
}
static void r_scrape(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 60, 90)); SY(s, FD_ALGO, RI(g, 1, 5)); SY(s, FD_OPAB, RI(g, 2, 5));
    SY(s, FD_RATA, RI(g, 30, 90)); SY(s, FD_MODA, RI(g, 30, 70)); SY(s, FD_DECA, RI(g, 90, 127)); SY(s, FD_ENDA, RI(g, 40, 90));
    SY(s, FD_HOLD, RI(g, 60, 110)); SY(s, FD_DEC, RI(g, 90, 120)); SY(s, FD_LEV, RI(g, 70, 100));
    SY(s, FD_NLEV, RI(g, 60, 100)); SY(s, FD_NHLD, RI(g, 60, 110)); SY(s, FD_NDEC, RI(g, 90, 120));
    SY(s, FD_GRAN, RI(g, 70, 120)); SY(s, FD_NBAS, RI(g, 60, 90)); SY(s, FD_NWID, RI(g, 20, 50));
    amp_adsr(s, RI(g, 10, 50), 127, RI(g, 90, 127), RI(g, 60, 100));
    filt(s, SH_BP, RI(g, 60, 95), RI(g, 20, 55), 0, 0, 64, 0, 64, 0);
    band(s, RI(g, 24, 45), 127);
    sends(s, RI(g, 10, 40), RI(g, 50, 95), 0);
    dirt(s, RI(g, 10, 40) + g->ind * 30 / 100, 0, 0, 0, 0);
    level(s, 104);
}
static void r_machine(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 40, 70)); SY(s, FD_ALGO, RI(g, 0, 4)); SY(s, FD_RATA, RI(g, 0, 40)); SY(s, FD_MODA, RI(g, 10, 50));
    SY(s, FD_DECA, 127); SY(s, FD_ENDA, RI(g, 40, 90)); SY(s, FD_HOLD, 127); SY(s, FD_DEC, 127); SY(s, FD_LEV, RI(g, 60, 100));
    SY(s, FD_NHLD, 127); SY(s, FD_NDEC, 127); SY(s, FD_NLEV, RI(g, 30, 70));
    SY(s, FD_NBAS, RI(g, 30, 60)); SY(s, FD_NWID, RI(g, 20, 50)); SY(s, FD_GRAN, RI(g, 20, 80));
    amp_adsr(s, RI(g, 40, 90), 127, 127, RI(g, 80, 115));
    filt(s, SH_BP, RI(g, 45, 75), RI(g, 20, 50), 0, 0, 64, 0, 64, 0);
    band(s, RI(g, 18, 34), 127);
    sends(s, RI(g, 10, 30), RI(g, 60, 100), 0);
    dirt(s, g->ind * RI(g, 10, 50) / 100, 0, 0, 0, 0);
    level(s, 106);
}

/* FM TONE */
static void r_fm_bass(sound_t *s, gctx_t *g)
{
    fm_base(s);
    harmonic_fm(s, g, 40 + g->ind * 40 / 100);
    SY(s, FT_RATC, PICK(g, RC_HALF, RC_ONE));
    SY(s, FT_DTUN, RI(g, 15, 55)); SY(s, FT_FDBK, RI(g, 20, 80)); SY(s, FT_MIX, RI(g, 40, 100));
    SY(s, FT_ADEC, RI(g, 20, 60)); SY(s, FT_AEND, RI(g, 20, 80)); SY(s, FT_BDEC, RI(g, 30, 80)); SY(s, FT_BEND, RI(g, 40, 100));
    amp_adsr(s, RI(g, 0, 6), RI(g, 30, 65), RI(g, 80, 115), RI(g, 15, 40));
    filt(s, SH_LP, RI(g, 64, 96), RI(g, 10, 45), RI(g, 8, 24), 0, RI(g, 35, 70), RI(g, 0, 30), RI(g, 30, 60), RI(g, 0, 30));
    dirt(s, RI(g, 25, 45) + g->ind * 35 / 100, 0, 0, 0, 0);
    level(s, 120);
}
static void r_fm_lead(sound_t *s, gctx_t *g)
{
    fm_base(s);
    harmonic_fm(s, g, 60);
    SY(s, FT_DTUN, RI(g, 20, 70)); SY(s, FT_FDBK, RI(g, 40, 95)); SY(s, FT_MIX, RI(g, 30, 90));
    SY(s, FT_ADEC, RI(g, 20, 50)); SY(s, FT_AEND, RI(g, 30, 90)); SY(s, FT_BDEC, RI(g, 20, 60)); SY(s, FT_BEND, RI(g, 30, 90));
    amp_adsr(s, RI(g, 0, 12), RI(g, 35, 70), RI(g, 70, 110), RI(g, 20, 45));
    filt(s, SH_LP, RI(g, 62, 98), RI(g, 25, 65), RI(g, 5, 25), 0, RI(g, 35, 70), RI(g, 20, 60), RI(g, 30, 60), RI(g, 20, 60));
    band(s, RI(g, 12, 30), 127);
    sends(s, RI(g, 15, 55), RI(g, 15, 50), RI(g, 0, 35));
    dirt(s, RI(g, 15, 50) + g->ind * 35 / 100, 0, 0, 0, 0);
    level(s, 116);
}
static void r_fm_bell(sound_t *s, gctx_t *g)
{
    fm_base(s);
    SY(s, FT_ALGO, RI(g, 0, 7)); SY(s, FT_RATC, RC_ONE);
    SY(s, FT_RATA, PICK(g, 9, 13, 17, 21, 25)); SY(s, FT_RATB, RATB(RI(g, 5, 10), RI(g, 4, 14)));
    SY(s, FT_DTUN, RI(g, 10, 60)); SY(s, FT_ALEV, RI(g, 60, 95)); SY(s, FT_BLEV, RI(g, 50, 85));
    SY(s, FT_ADEC, RI(g, 55, 95)); SY(s, FT_AEND, RI(g, 0, 20)); SY(s, FT_BDEC, RI(g, 50, 90)); SY(s, FT_BEND, RI(g, 0, 20));
    amp_adsr(s, 0, RI(g, 65, 95), 0, RI(g, 50, 80));
    filt(s, SH_LP, RI(g, 80, 115), RI(g, 0, 25), 0, 0, 64, 0, 64, 50);
    band(s, RI(g, 10, 26), 127);
    sends(s, RI(g, 15, 50), RI(g, 30, 75) + g->abs_ * 20 / 100, 0);
    dirt(s, RI(g, 10, 25), 0, 0, 0, 0);
    level(s, 116);
}
static void r_fm_stab(sound_t *s, gctx_t *g)
{
    fm_base(s);
    harmonic_fm(s, g, 75);
    SY(s, FT_FDBK, RI(g, 40, 110)); SY(s, FT_DTUN, RI(g, 30, 90));
    SY(s, FT_ADEC, RI(g, 20, 45)); SY(s, FT_BDEC, RI(g, 20, 50));
    amp_adsr(s, 0, RI(g, 30, 52), RI(g, 0, 20), RI(g, 20, 40));
    filt(s, SH_LP, RI(g, 55, 90), RI(g, 25, 65), RI(g, 15, 35), 0, RI(g, 25, 45), 0, RI(g, 20, 40), 0);
    band(s, RI(g, 8, 22), 127);
    sends(s, RI(g, 20, 55), RI(g, 25, 65), 0);
    dirt(s, RI(g, 20, 50) + g->ind * 40 / 100, g->ind > 50 && CH(g, 30) ? RI(g, 20, 45) : 0, 0, 0, 0);
    level(s, 114);
}
static void r_fb_drone(sound_t *s, gctx_t *g)
{
    fm_base(s);
    SY(s, FT_ALGO, RI(g, 0, 7)); SY(s, FT_RATC, RC_ONE);
    SY(s, FT_RATA, RI(g, 2, 20)); SY(s, FT_RATB, RATB(RI(g, 3, 8), RI(g, 3, 10)));
    SY(s, FT_DTUN, RI(g, 40, 110)); SY(s, FT_FDBK, RI(g, 70, 127));
    SY(s, FT_AATK, RI(g, 40, 100)); SY(s, FT_ADEC, 127); SY(s, FT_AEND, RI(g, 40, 90)); SY(s, FT_ALEV, RI(g, 30, 80));
    SY(s, FT_BATK, RI(g, 60, 110)); SY(s, FT_BDEC, 127); SY(s, FT_BEND, RI(g, 40, 100)); SY(s, FT_BLEV, RI(g, 30, 80));
    SY(s, FT_ATRG, 0); SY(s, FT_BTRG, 0); SY(s, FT_PHRT, 0);
    amp_adsr(s, RI(g, 30, 90), 127, 127, RI(g, 70, 115));
    filt(s, SH_LP, RI(g, 58, 90), RI(g, 10, 40), 0, 0, 64, 0, 64, 0);
    band(s, RI(g, 14, 30), 127);
    sends(s, 0, RI(g, 50, 95), 0);
    level(s, 108);
}

/* WAVETONE */
static void r_chord_stab(sound_t *s, gctx_t *g)
{
    wt_base(s);
    SY(s, WT_WAV1, RI(g, 0, 50)); SY(s, WT_LEV1, 112);
    SY(s, WT_TUN2, PICK(g, 67, 71, 71, 76)); SY(s, WT_WAV2, RI(g, 0, 50)); SY(s, WT_LEV2, RI(g, 95, 112));
    SY(s, WT_OFS2, RI(g, 47, 53)); SY(s, WT_DRIF, RI(g, 5, 25));
    amp_adsr(s, 0, RI(g, 34, 56), RI(g, 0, 15), RI(g, 28, 48));
    filt(s, SH_LP, RI(g, 72, 100), RI(g, 10, 35), RI(g, 12, 28), 0, RI(g, 28, 50), 0, RI(g, 25, 45), 0);
    band(s, RI(g, 8, 22), 127);
    sends(s, RI(g, 20, 60), RI(g, 25, 65) + g->abs_ * 20 / 100, 0);
    dirt(s, RI(g, 15, 35), 0, 0, 0, 0);
    level(s, 116);
}
static void r_wt_pad(sound_t *s, gctx_t *g)
{
    wt_base(s);
    SY(s, WT_TBL1, RI(g, 0, 1)); SY(s, WT_WAV1, RI(g, 0, 120)); SY(s, WT_LEV1, 116);
    SY(s, WT_TBL2, RI(g, 0, 1)); SY(s, WT_WAV2, RI(g, 0, 120)); SY(s, WT_LEV2, 110);
    SY(s, WT_TUN2, PICK(g, 64, 64, 71, 76)); SY(s, WT_OFS2, RI(g, 46, 54)); SY(s, WT_DRIF, RI(g, 30, 65));
    SY(s, WT_NLEV, RI(g, 0, 30)); SY(s, WT_NBAS, RI(g, 40, 90)); SY(s, WT_NWID, RI(g, 20, 60));
    amp_adsr(s, RI(g, 45, 100), RI(g, 60, 100), RI(g, 100, 127), RI(g, 55, 95));
    filt(s, SH_LP, RI(g, 64, 96), RI(g, 0, 30), RI(g, 0, 12), RI(g, 40, 90), RI(g, 60, 100), RI(g, 40, 100), RI(g, 50, 90), 30);
    band(s, RI(g, 8, 22), 127);
    sends(s, RI(g, 0, 35), RI(g, 60, 105), RI(g, 10, 50));
    dirt(s, RI(g, 5, 20) + g->ind * RI(g, 0, 30) / 100, 0, 0, 0, 0);
    level(s, 114);
}
static void r_ring_drone(sound_t *s, gctx_t *g)
{
    wt_base(s);
    SY(s, WT_MOD, PICK(g, 1, 2)); SY(s, WT_WAV1, RI(g, 0, 120)); SY(s, WT_TUN2, RI(g, 64, 100)); SY(s, WT_WAV2, RI(g, 0, 120));
    SY(s, WT_LEV2, 0); SY(s, WT_DRIF, RI(g, 40, 90));
    SY(s, WT_NLEV, RI(g, 0, 40)); SY(s, WT_NBAS, RI(g, 20, 70)); SY(s, WT_NWID, RI(g, 20, 60));
    amp_adsr(s, RI(g, 30, 90), 127, 127, RI(g, 70, 115));
    filt(s, SH_LP, RI(g, 60, 92), RI(g, 15, 45), 0, 0, 64, 0, 64, 0);
    band(s, RI(g, 14, 30), 127);
    sends(s, RI(g, 0, 35), RI(g, 50, 95), 0);
    dirt(s, g->ind * RI(g, 20, 70) / 100, 0, 0, 0, 0);
    level(s, 110);
}
static void r_grain_field(sound_t *s, gctx_t *g)
{
    wt_base(s);
    SY(s, WT_LEV1, RI(g, 0, 35)); SY(s, WT_WAV1, RI(g, 0, 120));
    SY(s, WT_NATK, RI(g, 20, 70)); SY(s, WT_NHLD, 127); SY(s, WT_NDEC, 127); SY(s, WT_NLEV, RI(g, 95, 120));
    SY(s, WT_NTYP, RI(g, 0, 2)); SY(s, WT_CHAR, RI(g, 30, 120));
    SY(s, WT_NBAS, RI(g, 20, 85)); SY(s, WT_NWID, RI(g, 15, 60));
    amp_adsr(s, RI(g, 20, 70), 127, 127, RI(g, 60, 110));
    filt(s, SH_BP, RI(g, 55, 100), RI(g, 25, 70), 0, 0, 64, 0, 64, 0);
    band(s, RI(g, 22, 40), 127);
    sends(s, RI(g, 15, 55), RI(g, 45, 95), 0);
    dirt(s, 0, g->ind > 40 && CH(g, 40) ? RI(g, 20, 60) : 0, CH(g, 30 + g->idm * 30 / 100) ? RI(g, 10, 50) : 0, 0, 0);
    level(s, 110);
}
static void r_breath(sound_t *s, gctx_t *g)
{
    wt_base(s);
    SY(s, WT_LEV1, 0); SY(s, WT_LEV2, 0); SY(s, WT_NLEV, RI(g, 110, 127)); SY(s, WT_NTYP, RI(g, 0, 1));
    SY(s, WT_CHAR, RI(g, 0, 60)); SY(s, WT_NATK, RI(g, 60, 110)); SY(s, WT_NHLD, 127); SY(s, WT_NDEC, 127);
    SY(s, WT_NBAS, RI(g, 30, 80)); SY(s, WT_NWID, RI(g, 30, 80));
    amp_adsr(s, RI(g, 60, 110), 127, 127, RI(g, 80, 115));
    filt(s, SH_BP, RI(g, 50, 85), RI(g, 30, 70), 0, 0, 64, 0, 64, 0);
    band(s, RI(g, 24, 40), 127);
    sends(s, RI(g, 20, 60), RI(g, 70, 110), 0);
    level(s, 108);
}
static void r_radio(sound_t *s, gctx_t *g)
{
    wt_base(s);
    SY(s, WT_MOD, 2); SY(s, WT_WAV1, RI(g, 0, 120)); SY(s, WT_TUN2, RI(g, 20, 110)); SY(s, WT_WAV2, RI(g, 0, 120));
    SY(s, WT_LEV2, 0); SY(s, WT_NLEV, RI(g, 50, 100)); SY(s, WT_NTYP, 2); SY(s, WT_CHAR, RI(g, 20, 100));
    SY(s, WT_NBAS, RI(g, 40, 80)); SY(s, WT_NWID, RI(g, 15, 40));
    amp_adsr(s, RI(g, 0, 30), 127, RI(g, 80, 127), RI(g, 40, 90));
    filt(s, SH_BP, RI(g, 65, 95), RI(g, 30, 70), 0, 0, 64, 0, 64, 0);
    band(s, RI(g, 40, 70), RI(g, 20, 50));
    sends(s, RI(g, 20, 60), RI(g, 30, 70), 0);
    dirt(s, RI(g, 20, 60), CH(g, 50) ? RI(g, 20, 60) : 0, RI(g, 10, 50), 0, 0);
    level(s, 104);
}

/* SWARMER */
static void r_sub_swarm(sound_t *s, gctx_t *g)
{
    sw_base(s);
    SY(s, SW_SWRM, RI(g, 20, 80)); SY(s, SW_MAIN, RI(g, 0, 40)); SY(s, SW_DET, RI(g, 8, 30));
    SY(s, SW_MIX, RI(g, 80, 112)); SY(s, SW_MOCT, 1); SY(s, SW_ANIM, RI(g, 5, 25));
    amp_adsr(s, RI(g, 0, 8), RI(g, 40, 70), RI(g, 90, 120), RI(g, 20, 45));
    filt(s, SH_LP, RI(g, 58, 86), RI(g, 5, 35), RI(g, 5, 18), 0, RI(g, 40, 70), RI(g, 0, 40), RI(g, 30, 60), 0);
    dirt(s, RI(g, 20, 35) + g->ind * RI(g, 10, 40) / 100, 0, 0, 0, 0);
    level(s, 120);
}
static void r_swarm_lead(sound_t *s, gctx_t *g)
{
    sw_base(s);
    SY(s, SW_SWRM, RI(g, 40, 110)); SY(s, SW_MAIN, RI(g, 40, 110)); SY(s, SW_DET, RI(g, 20, 50));
    SY(s, SW_MIX, RI(g, 70, 110)); SY(s, SW_MOCT, 2); SY(s, SW_ANIM, RI(g, 20, 60));
    amp_adsr(s, RI(g, 0, 15), RI(g, 40, 70), RI(g, 80, 115), RI(g, 25, 55));
    filt(s, SH_LP, RI(g, 74, 104), RI(g, 15, 45), RI(g, 5, 18), 0, 64, 0, 64, 30);
    band(s, RI(g, 12, 30), 127);
    sends(s, RI(g, 15, 50), RI(g, 20, 55), RI(g, 0, 40));
    dirt(s, RI(g, 10, 30), 0, 0, 0, 0);
    level(s, 112);
}
static void r_swarm_stab(sound_t *s, gctx_t *g)
{
    sw_base(s);
    SY(s, SW_SWRM, RI(g, 40, 110)); SY(s, SW_MAIN, RI(g, 30, 110)); SY(s, SW_DET, RI(g, 40, 85));
    SY(s, SW_MIX, RI(g, 90, 127)); SY(s, SW_MOCT, PICK(g, 2, 1)); SY(s, SW_ANIM, RI(g, 20, 60));
    amp_adsr(s, 0, RI(g, 32, 55), RI(g, 0, 20), RI(g, 25, 45));
    filt(s, SH_LP, RI(g, 45, 80), RI(g, 20, 55), RI(g, 18, 40), 0, RI(g, 28, 50), 0, RI(g, 25, 45), 0);
    band(s, RI(g, 8, 22), 127);
    sends(s, RI(g, 15, 50), RI(g, 30, 75), 0);
    dirt(s, g->ind * RI(g, 20, 70) / 100, 0, 0, 0, 0);
    level(s, 116);
}
static void r_swarm_pad(sound_t *s, gctx_t *g)
{
    sw_base(s);
    SY(s, SW_SWRM, RI(g, 20, 100)); SY(s, SW_MAIN, RI(g, 20, 110)); SY(s, SW_DET, RI(g, 45, 90));
    SY(s, SW_MIX, RI(g, 100, 127)); SY(s, SW_MOCT, PICK(g, 2, 1)); SY(s, SW_ANIM, RI(g, 30, 70));
    SY(s, SW_NMOD, g->ind * RI(g, 0, 40) / 100);
    amp_adsr(s, RI(g, 40, 95), RI(g, 60, 100), RI(g, 100, 127), RI(g, 50, 90));
    filt(s, SH_LP, RI(g, 64, 94), RI(g, 0, 30), RI(g, 0, 12), RI(g, 40, 90), RI(g, 60, 100), RI(g, 40, 100), RI(g, 50, 90), 30);
    band(s, RI(g, 8, 22), 127);
    sends(s, RI(g, 0, 30), RI(g, 60, 100), RI(g, 15, 55));
    dirt(s, RI(g, 5, 20), 0, 0, 0, 0);
    level(s, 114);
}
static void r_swarm_drone(sound_t *s, gctx_t *g)
{
    sw_base(s);
    SY(s, SW_SWRM, RI(g, 0, 120)); SY(s, SW_MAIN, RI(g, 0, 120)); SY(s, SW_DET, RI(g, 30, 80));
    SY(s, SW_MIX, RI(g, 100, 127)); SY(s, SW_MOCT, PICK(g, 2, 1)); SY(s, SW_ANIM, RI(g, 40, 100));
    SY(s, SW_NMOD, g->ind * RI(g, 10, 60) / 100);
    amp_adsr(s, RI(g, 30, 90), 127, 127, RI(g, 70, 115));
    filt(s, SH_LP, RI(g, 60, 90), RI(g, 10, 40), 0, 0, 64, 0, 64, 0);
    band(s, RI(g, 14, 30), 127);
    sends(s, RI(g, 0, 25), RI(g, 55, 100), 0);
    dirt(s, RI(g, 10, 25) + g->ind * RI(g, 10, 50) / 100, 0, 0, 0, 0);
    level(s, 116);
}
static void r_dark_wash(sound_t *s, gctx_t *g)
{
    sw_base(s);
    SY(s, SW_SWRM, RI(g, 20, 100)); SY(s, SW_MAIN, RI(g, 20, 100)); SY(s, SW_DET, RI(g, 60, 120));
    SY(s, SW_MIX, RI(g, 110, 127)); SY(s, SW_MOCT, 1); SY(s, SW_ANIM, RI(g, 40, 100)); SY(s, SW_NMOD, RI(g, 30, 90));
    amp_adsr(s, RI(g, 60, 110), 127, 127, RI(g, 90, 120));
    filt(s, SH_LP, RI(g, 35, 65), RI(g, 10, 40), 0, 0, 64, 0, 64, 0);
    band(s, RI(g, 18, 30), 127);
    sends(s, RI(g, 10, 40), RI(g, 80, 110), RI(g, 20, 60));
    level(s, 110);
}


/* ------------------------------------------------- the gnarly families (build 2) --- */
/* a comb in place of the filter: tuned near the note (FREQ is a note number, KEY.T follows the keys), feedback high */
static void comb(sound_t *s, gctx_t *g, int neg)
{
    s->fmach = neg ? FLT_CBM : FLT_CBP;
    s->flt[F_FREQ] = (uint8_t)RI(g, 52, 96);
    s->flt[F_RESO] = (uint8_t)RI(g, 70, 118);
    s->flt[F_TYPE] = (uint8_t)RI(g, 55, 120);
    s->flt[F_KEYT] = (uint8_t)PICK(g, 0, 127, 127);
    s->flt[F_ENV] = (uint8_t)(64 + RI(g, -20, 20));
}
static void r_crush_kick(sound_t *s, gctx_t *g)
{
    r_driven_kick(s, g);
    SY(s, FD_FOLD, RI(g, 30, 70)); SY(s, FD_MODA, RI(g, 30, 70));
    dirt(s, RI(g, 60, 110), RI(g, 45, 95), CH(g, 50) ? RI(g, 20, 70) : 0, 1, CH(g, 50));
    level(s, 124);
}
static void r_comb_hit(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 50, 90)); SY(s, FD_ALGO, RI(g, 0, 6)); SY(s, FD_FDBK, RI(g, 10, 60));
    SY(s, FD_RATA, RI(g, 10, 80)); SY(s, FD_MODA, RI(g, 50, 120)); SY(s, FD_DECA, RI(g, 4, 30));
    SY(s, FD_DEC, RI(g, 12, 40)); SY(s, FD_LEV, RI(g, 90, 124));
    SY(s, FD_NDEC, RI(g, 6, 24)); SY(s, FD_NLEV, RI(g, 60, 120)); SY(s, FD_GRAN, RI(g, 30, 110));
    SY(s, FD_TRAN, RI(g, 0, 124)); SY(s, FD_TLEV, RI(g, 60, 110));
    drum_filter(s, g, SH_LP, 110, 127, 0, 15, 0, RI(g, 0, 30));
    comb(s, g, CH(g, 50));
    dirt(s, RI(g, 0, 70), 0, 0, 1, 0);
    pan(s, RI(g, -18, 18));
    level(s, 112);
}
static void r_screech(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 80, 118)); SY(s, FD_ALGO, PICK(g, 0, 1, 3)); SY(s, FD_FDBK, RI(g, 70, 127));
    SY(s, FD_OPC, RI(g, 40, 127)); SY(s, FD_OPAB, RI(g, 60, 127)); SY(s, FD_FOLD, RI(g, 40, 110));
    SY(s, FD_RATA, RI(g, 40, 127)); SY(s, FD_MODA, RI(g, 100, 127)); SY(s, FD_DECA, RI(g, 20, 60)); SY(s, FD_ENDA, RI(g, 40, 127));
    SY(s, FD_RATB, RI(g, 40, 127)); SY(s, FD_MODB, RI(g, 90, 127)); SY(s, FD_DECB, RI(g, 20, 60)); SY(s, FD_ENDB, RI(g, 40, 127));
    SY(s, FD_DEC, RI(g, 20, 50)); SY(s, FD_LEV, 110);
    drum_filter(s, g, SH_BP, 90, 115, 30, 80, 0, RI(g, 40, 70));
    dirt(s, RI(g, 40, 100), CH(g, 40) ? RI(g, 30, 70) : 0, 0, 1, 0);
    pan(s, RI(g, -20, 20));
    level(s, 112);
}
static void r_rattle(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 40, 100)); SY(s, FD_LEV, RI(g, 0, 50)); SY(s, FD_DEC, RI(g, 6, 20)); SY(s, FD_MODA, RI(g, 40, 110));
    SY(s, FD_NRST, 0); SY(s, FD_NRM, CH(g, 70)); SY(s, FD_NHLD, RI(g, 10, 40)); SY(s, FD_NDEC, RI(g, 20, 45));
    SY(s, FD_GRAN, RI(g, 90, 125)); SY(s, FD_NLEV, 127); SY(s, FD_NBAS, RI(g, 30, 80)); SY(s, FD_NWID, RI(g, 30, 90));
    SY(s, FD_TRAN, RI(g, 60, 124)); SY(s, FD_TLEV, RI(g, 40, 100));
    drum_filter(s, g, SH_LP, 100, 127, 0, 30, 0, RI(g, 20, 50));
    dirt(s, RI(g, 30, 90), CH(g, 50) ? RI(g, 30, 80) : 0, CH(g, 40) ? RI(g, 10, 50) : 0, 1, 1);
    pan(s, RI(g, -24, 24));
    level(s, 112);
}
static void r_static_burst(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_LEV, RI(g, 0, 40)); SY(s, FD_TUNE, RI(g, 60, 110)); SY(s, FD_MODA, RI(g, 60, 127)); SY(s, FD_RATA, RI(g, 60, 127));
    SY(s, FD_NHLD, RI(g, 0, 30)); SY(s, FD_NDEC, RI(g, 10, 35)); SY(s, FD_NLEV, 127); SY(s, FD_GRAN, RI(g, 0, 60));
    SY(s, FD_NBAS, RI(g, 40, 100)); SY(s, FD_NWID, RI(g, 10, 50));
    drum_filter(s, g, SH_BP, 80, 115, 20, 70, 0, RI(g, 30, 60));
    dirt(s, RI(g, 30, 80), RI(g, 50, 105), RI(g, 20, 80), 1, CH(g, 50));
    pan(s, RI(g, -24, 24));
    level(s, 110);
}
static void r_impact(sound_t *s, gctx_t *g)
{
    drum_base(s);
    SY(s, FD_TUNE, RI(g, 30, 50)); SY(s, FD_STIM, RI(g, 10, 30)); SY(s, FD_SDEP, RI(g, 70, 127));
    SY(s, FD_FOLD, RI(g, 50, 120)); SY(s, FD_FDBK, RI(g, 20, 70)); SY(s, FD_MODA, RI(g, 50, 100)); SY(s, FD_RATA, RI(g, 0, 30));
    SY(s, FD_HOLD, RI(g, 10, 30)); SY(s, FD_DEC, RI(g, 40, 64)); SY(s, FD_LEV, 124);
    SY(s, FD_NHLD, RI(g, 5, 20)); SY(s, FD_NDEC, RI(g, 30, 55)); SY(s, FD_NLEV, RI(g, 80, 120)); SY(s, FD_GRAN, RI(g, 60, 120));
    SY(s, FD_NBAS, RI(g, 20, 60)); SY(s, FD_NWID, RI(g, 50, 100)); SY(s, FD_TRAN, RI(g, 60, 124)); SY(s, FD_TLEV, RI(g, 80, 120));
    drum_filter(s, g, SH_LP, 90, 120, 0, 30, 1, 0);
    dirt(s, RI(g, 70, 120), CH(g, 40) ? RI(g, 30, 70) : 0, 0, 1, 0);
    level(s, 124);
}
static void r_fm_scream(sound_t *s, gctx_t *g)
{
    fm_base(s);
    SY(s, FT_ALGO, PICK(g, 0, 3, 4, 5)); SY(s, FT_RATC, PICK(g, RC_ONE, RC_TWO, RC_HALF));
    SY(s, FT_RATA, RI(g, 4, 35)); SY(s, FT_RATB, RATB(RI(g, 3, 18), RI(g, 3, 18)));
    SY(s, FT_HARM, RI(g, 63, 89)); SY(s, FT_DTUN, RI(g, 60, 127)); SY(s, FT_FDBK, RI(g, 90, 127));
    SY(s, FT_ALEV, RI(g, 95, 127)); SY(s, FT_BLEV, RI(g, 80, 127)); SY(s, FT_ADEC, RI(g, 60, 110)); SY(s, FT_AEND, RI(g, 60, 127));
    SY(s, FT_BDEC, RI(g, 60, 110)); SY(s, FT_BEND, RI(g, 60, 127));
    amp_adsr(s, RI(g, 0, 10), RI(g, 40, 80), RI(g, 70, 127), RI(g, 15, 40));
    filt(s, SH_BP, RI(g, 70, 105), RI(g, 50, 100), RI(g, -20, 30), 0, RI(g, 30, 70), RI(g, 0, 60), 40, 60);
    band(s, RI(g, 20, 45), 127);
    dirt(s, RI(g, 60, 120), CH(g, 40) ? RI(g, 30, 70) : 0, CH(g, 30) ? RI(g, 10, 40) : 0, 1, 1);
    sends(s, RI(g, 0, 50), RI(g, 0, 40), 0);
    level(s, 116);
}
static void r_fm_growl(sound_t *s, gctx_t *g)
{
    fm_base(s);
    SY(s, FT_ALGO, PICK(g, 0, 1, 3)); SY(s, FT_RATC, PICK(g, RC_HALF, RC_HALF, RC_ONE));
    SY(s, FT_RATA, PICK(g, 1, 2, 4, 5, 6)); SY(s, FT_RATB, RATB(PICK(g, RC_HALF, RC_ONE), PICK(g, RC_ONE, RC_TWO, RC_THREE)));
    SY(s, FT_DTUN, RI(g, 70, 120)); SY(s, FT_FDBK, RI(g, 60, 120)); SY(s, FT_ALEV, RI(g, 80, 115)); SY(s, FT_BLEV, RI(g, 60, 110));
    SY(s, FT_ADEC, RI(g, 30, 70)); SY(s, FT_AEND, RI(g, 40, 100)); SY(s, FT_BDEC, RI(g, 30, 80)); SY(s, FT_BEND, RI(g, 50, 110));
    amp_adsr(s, 0, RI(g, 30, 60), RI(g, 80, 120), RI(g, 10, 30));
    filt(s, SH_LP, RI(g, 55, 85), RI(g, 60, 110), RI(g, 12, 35), 0, RI(g, 25, 55), RI(g, 0, 40), RI(g, 20, 40), RI(g, 0, 40));
    dirt(s, RI(g, 70, 120), CH(g, 35) ? RI(g, 25, 60) : 0, 0, 1, 0);
    level(s, 118);
}
static void r_fm_glitch(sound_t *s, gctx_t *g)
{
    fm_base(s);
    SY(s, FT_ALGO, RI(g, 0, 7)); SY(s, FT_RATC, RI(g, 2, 14)); SY(s, FT_RATA, RI(g, 10, 35));
    SY(s, FT_RATB, RATB(RI(g, 4, 18), RI(g, 0, 18))); SY(s, FT_FDBK, RI(g, 30, 127)); SY(s, FT_DTUN, RI(g, 40, 127));
    SY(s, FT_ALEV, RI(g, 70, 127)); SY(s, FT_BLEV, RI(g, 60, 127)); SY(s, FT_ADEC, RI(g, 2, 24)); SY(s, FT_BDEC, RI(g, 2, 30));
    SY(s, FT_PHRT, 1);
    amp_hit(s, 0, RI(g, 0, 6), RI(g, 8, 24));
    drum_filter(s, g, SH_BP, 80, 120, 20, 70, 0, RI(g, 20, 60));
    dirt(s, RI(g, 20, 90), RI(g, 30, 90), RI(g, 10, 70), CH(g, 50), CH(g, 50));
    sends(s, g->idm * RI(g, 0, 40) / 100, 0, 0);
    pan(s, RI(g, -24, 24));
    level(s, 120);
}
static void r_fm_clank(sound_t *s, gctx_t *g)
{
    fm_base(s);
    SY(s, FT_ALGO, PICK(g, 2, 5, 6, 7)); SY(s, FT_RATC, RI(g, 3, 12)); SY(s, FT_RATA, PICK(g, 9, 13, 17, 21, 25, 29, 33));
    SY(s, FT_RATB, RATB(RI(g, 5, 18), RI(g, 4, 18))); SY(s, FT_DTUN, RI(g, 60, 127)); SY(s, FT_FDBK, RI(g, 20, 90));
    SY(s, FT_ALEV, RI(g, 80, 120)); SY(s, FT_BLEV, RI(g, 70, 120)); SY(s, FT_ADEC, RI(g, 20, 60)); SY(s, FT_BDEC, RI(g, 20, 60));
    amp_adsr(s, 0, RI(g, 40, 70), 0, RI(g, 30, 60));
    filt(s, SH_LP, 110, 0, 0, 0, 64, 0, 64, 0);
    comb(s, g, CH(g, 50));
    dirt(s, RI(g, 30, 90), CH(g, 30) ? RI(g, 20, 60) : 0, 0, 1, 0);
    pan(s, RI(g, -18, 18));
    level(s, 108);
}
static void r_ring_scream(sound_t *s, gctx_t *g)
{
    wt_base(s);
    SY(s, WT_MOD, PICK(g, 1, 2, 2)); SY(s, WT_WAV1, RI(g, 40, 120)); SY(s, WT_TBL1, RI(g, 0, 1));
    SY(s, WT_TUN2, RI(g, 70, 124)); SY(s, WT_WAV2, RI(g, 40, 120)); SY(s, WT_LEV2, 0); SY(s, WT_PD1, RI(g, 10, 90));
    SY(s, WT_DRIF, RI(g, 0, 60));
    amp_adsr(s, 0, RI(g, 30, 70), RI(g, 40, 120), RI(g, 15, 40));
    filt(s, SH_BP, RI(g, 70, 110), RI(g, 50, 110), RI(g, -30, 30), 0, RI(g, 30, 60), 40, 40, 60);
    band(s, RI(g, 25, 50), 127);
    dirt(s, RI(g, 50, 110), CH(g, 40) ? RI(g, 30, 70) : 0, 0, 1, 0);
    sends(s, RI(g, 0, 40), RI(g, 0, 40), 0);
    level(s, 104);
}
static void r_sync_grind(sound_t *s, gctx_t *g)
{
    wt_base(s);
    SY(s, WT_MOD, 3); SY(s, WT_TUN2, PICK(g, 52, 64, 64)); SY(s, WT_TUN1, RI(g, 76, 124));
    SY(s, WT_WAV1, RI(g, 60, 120)); SY(s, WT_PD1, RI(g, 0, 100)); SY(s, WT_LEV1, 120); SY(s, WT_LEV2, RI(g, 0, 60));
    amp_adsr(s, 0, RI(g, 30, 60), RI(g, 60, 120), RI(g, 10, 30));
    filt(s, SH_LP, RI(g, 60, 95), RI(g, 50, 105), RI(g, 15, 40), 0, RI(g, 20, 50), RI(g, 0, 40), RI(g, 20, 40), 30);
    dirt(s, RI(g, 70, 120), RI(g, 0, 60), 0, 1, 0);
    level(s, 112);
}
static void r_digital_hash(sound_t *s, gctx_t *g)
{
    noise_base(s);
    SY(s, WT_NTYP, PICK(g, 1, 2, 2)); SY(s, WT_CHAR, RI(g, 30, 120)); SY(s, WT_NDEC, RI(g, 14, 40)); SY(s, WT_NHLD, RI(g, 0, 20));
    SY(s, WT_NBAS, RI(g, 30, 90)); SY(s, WT_NWID, RI(g, 20, 80)); SY(s, WT_NLEV, 127);
    SY(s, WT_LEV1, CH(g, 40) ? RI(g, 40, 90) : 0); SY(s, WT_MOD, 2); SY(s, WT_WAV1, RI(g, 60, 120));
    amp_hit(s, 0, RI(g, 0, 10), RI(g, 25, 45));
    drum_filter(s, g, SH_LP, 95, 127, 0, 40, 0, RI(g, 20, 50));
    dirt(s, RI(g, 20, 80), RI(g, 50, 110), RI(g, 20, 90), 1, CH(g, 50));
    pan(s, RI(g, -24, 24));
    level(s, 110);
}
static void r_pd_buzz(sound_t *s, gctx_t *g)
{
    wt_base(s);
    SY(s, WT_WAV1, RI(g, 80, 120)); SY(s, WT_PD1, PICK(g, 0, 5, 95, 100)); SY(s, WT_LEV1, 120);
    SY(s, WT_TUN2, PICK(g, 64, 64, 52, 71)); SY(s, WT_WAV2, RI(g, 80, 120)); SY(s, WT_PD2, PICK(g, 0, 10, 90, 100));
    SY(s, WT_LEV2, RI(g, 80, 115)); SY(s, WT_OFS2, RI(g, 40, 60));
    amp_hit(s, 0, RI(g, 0, 20), RI(g, 30, 55));
    filt(s, SH_LP, RI(g, 70, 105), RI(g, 30, 90), RI(g, 10, 40), 0, RI(g, 20, 45), 0, 30, 30);
    dirt(s, RI(g, 60, 120), CH(g, 40) ? RI(g, 25, 70) : 0, 0, 1, 0);
    pan(s, RI(g, -16, 16));
    level(s, 112);
}
static void r_swarm_grind(sound_t *s, gctx_t *g)
{
    sw_base(s);
    SY(s, SW_SWRM, RI(g, 70, 120)); SY(s, SW_MAIN, RI(g, 70, 120)); SY(s, SW_DET, RI(g, 90, 127));
    SY(s, SW_MIX, RI(g, 100, 127)); SY(s, SW_MOCT, PICK(g, 1, 2)); SY(s, SW_ANIM, RI(g, 60, 127)); SY(s, SW_NMOD, RI(g, 60, 127));
    amp_adsr(s, 0, RI(g, 30, 60), RI(g, 60, 120), RI(g, 15, 40));
    filt(s, SH_LP, RI(g, 55, 95), RI(g, 50, 105), RI(g, 15, 40), 0, RI(g, 25, 55), RI(g, 0, 40), 30, 20);
    dirt(s, RI(g, 70, 120), RI(g, 0, 70), CH(g, 30) ? RI(g, 10, 50) : 0, 1, 0);
    level(s, 110);
}
static void r_hornet(sound_t *s, gctx_t *g)
{
    sw_base(s);
    SY(s, SW_SWRM, RI(g, 60, 120)); SY(s, SW_MAIN, RI(g, 60, 120)); SY(s, SW_DET, RI(g, 60, 110));
    SY(s, SW_MIX, RI(g, 90, 127)); SY(s, SW_MOCT, 2); SY(s, SW_ANIM, RI(g, 40, 110)); SY(s, SW_NMOD, RI(g, 20, 90));
    amp_adsr(s, RI(g, 0, 20), RI(g, 40, 70), RI(g, 60, 120), RI(g, 20, 50));
    filt(s, SH_LP, 110, 0, 0, 0, 64, 0, 64, 0);
    comb(s, g, CH(g, 60));
    dirt(s, RI(g, 30, 100), CH(g, 30) ? RI(g, 20, 60) : 0, 0, 1, 0);
    sends(s, RI(g, 0, 40), RI(g, 10, 50), 0);
    level(s, 114);
}

/* the grit pass: after a rhythmic family's recipe (never a dark-ambient one), more dirt drawn at random -- drive after
 * the filter, bits, a lower rate, more fold or feedback, sometimes a comb in place of the filter */
static void grit(sound_t *s, gctx_t *g)
{
    if (CH(g, 55))
        dirt(s, maxi(s->fx[X_OD], RI(g, 40, 110)), s->fx[X_BR], s->fx[X_SRR], 1, s->fx[X_SRRT]);
    if (CH(g, 35))
        s->fx[X_BR] = (uint8_t)maxi(s->fx[X_BR], RI(g, 25, 80));
    if (CH(g, 28)) {
        s->fx[X_SRR] = (uint8_t)maxi(s->fx[X_SRR], RI(g, 8, 60));
        s->fx[X_SRRT] = (uint8_t)CH(g, 50);
    }
    if (s->mach == ENG_FMD && CH(g, 40))
        SY(s, FD_FOLD, maxi(s->syn[FD_FOLD], RI(g, 25, 80)));
    if (s->mach == ENG_FMT && CH(g, 45)) {
        SY(s, FT_FDBK, maxi(s->syn[FT_FDBK], RI(g, 50, 127)));
        SY(s, FT_DTUN, maxi(s->syn[FT_DTUN], RI(g, 60, 127)));
    }
    if (s->mach == ENG_SWM && CH(g, 45))
        SY(s, SW_NMOD, maxi(s->syn[SW_NMOD], RI(g, 30, 100)));
    if (s->fmach != FLT_CBM && s->fmach != FLT_CBP && CH(g, 15))
        comb(s, g, CH(g, 50));
}

/* the recipes, by machine; the FLTR machine each uses */
static const recipe_t RECIPES[] = {
    {"TIGHT KICK", ENG_FMD, FLT_MM, 0, r_tight_kick},     {"DEEP KICK", ENG_FMD, FLT_MM, 0, r_deep_kick},
    {"DRIVEN KICK", ENG_FMD, FLT_MM, 0, r_driven_kick},   {"CLICK KICK", ENG_FMD, FLT_MM, 0, r_click_kick},
    {"FM SNARE", ENG_FMD, FLT_MM, 0, r_fm_snare},         {"TIGHT SNARE", ENG_FMD, FLT_MM, 0, r_tight_snare},
    {"CLAP", ENG_FMD, FLT_MM, 0, r_clap},                 {"DIRTY SNARE", ENG_FMD, FLT_MM, 0, r_dirty_snare},
    {"METAL HAT", ENG_FMD, FLT_MM, 0, r_metal_hat},       {"OPEN HAT", ENG_FMD, FLT_MM, 0, r_open_hat},
    {"RIDE", ENG_FMD, FLT_MM, 0, r_ride},                 {"CYMBAL", ENG_FMD, FLT_MM, 0, r_cymbal},
    {"TOM", ENG_FMD, FLT_MM, 0, r_tom},                   {"RIM", ENG_FMD, FLT_MM, 0, r_rim},
    {"FM PERC", ENG_FMD, FLT_MM, 0, r_fm_perc},           {"CLANG", ENG_FMD, FLT_MM, 0, r_clang},
    {"SLAM", ENG_FMD, FLT_MM, 0, r_slam},                 {"STAMP", ENG_FMD, FLT_MM, 0, r_stamp},
    {"CRUNCH", ENG_FMD, FLT_MM, 0, r_crunch},             {"COWBELL", ENG_FMD, FLT_MM, 0, r_cowbell},
    {"CRUSH KICK", ENG_FMD, FLT_MM, 0, r_crush_kick},     {"COMB HIT", ENG_FMD, FLT_MM, 0, r_comb_hit},
    {"SCREECH", ENG_FMD, FLT_MM, 0, r_screech},           {"RATTLE", ENG_FMD, FLT_MM, 0, r_rattle},
    {"STATIC BURST", ENG_FMD, FLT_MM, 0, r_static_burst}, {"IMPACT", ENG_FMD, FLT_MM, 0, r_impact},
    {"NOISE ENGINE", ENG_FMD, FLT_MM, 1, r_noise_engine}, {"SCRAPE", ENG_FMD, FLT_MM, 1, r_scrape},
    {"MACHINE", ENG_FMD, FLT_MM, 1, r_machine},
    {"FM BASS", ENG_FMT, FLT_LP4, 0, r_fm_bass},          {"FM LEAD", ENG_FMT, FLT_MM, 0, r_fm_lead},
    {"FM BELL", ENG_FMT, FLT_MM, 0, r_fm_bell},           {"FM STAB", ENG_FMT, FLT_MM, 0, r_fm_stab},
    {"FM SCREAM", ENG_FMT, FLT_MM, 0, r_fm_scream},       {"FM GROWL", ENG_FMT, FLT_LP4, 0, r_fm_growl},
    {"FM GLITCH", ENG_FMT, FLT_MM, 0, r_fm_glitch},       {"FM CLANK", ENG_FMT, FLT_MM, 0, r_fm_clank},
    {"FB DRONE", ENG_FMT, FLT_LP4, 1, r_fb_drone},
    {"CHORD STAB", ENG_WAV, FLT_LP4, 0, r_chord_stab},    {"RING SCREAM", ENG_WAV, FLT_MM, 0, r_ring_scream},
    {"SYNC GRIND", ENG_WAV, FLT_LP4, 0, r_sync_grind},    {"DIGITAL HASH", ENG_WAV, FLT_MM, 0, r_digital_hash},
    {"PD BUZZ", ENG_WAV, FLT_LP4, 0, r_pd_buzz},
    {"WAVE PAD", ENG_WAV, FLT_LP4, 1, r_wt_pad},          {"RING DRONE", ENG_WAV, FLT_MM, 1, r_ring_drone},
    {"GRAIN FIELD", ENG_WAV, FLT_MM, 1, r_grain_field},   {"BREATH", ENG_WAV, FLT_MM, 1, r_breath},
    {"RADIO", ENG_WAV, FLT_MM, 1, r_radio},
    {"SUB SWARM", ENG_SWM, FLT_LP4, 0, r_sub_swarm},      {"SWARM LEAD", ENG_SWM, FLT_LP4, 0, r_swarm_lead},
    {"SWARM STAB", ENG_SWM, FLT_LP4, 0, r_swarm_stab},    {"SWARM GRIND", ENG_SWM, FLT_LP4, 0, r_swarm_grind},
    {"HORNET", ENG_SWM, FLT_LP4, 0, r_hornet},
    {"SWARM PAD", ENG_SWM, FLT_LP4, 1, r_swarm_pad},      {"SWARM DRONE", ENG_SWM, FLT_LP4, 1, r_swarm_drone},
    {"DARK WASH", ENG_SWM, FLT_LP4, 1, r_dark_wash},
};
#define NRECIPE (sizeof RECIPES / sizeof RECIPES[0])

/* a new sound for machine m: of family fam (>= 0), or of any family but avoid (-1: none) */
static void gen_sound(sound_t *s, uint32_t m, int32_t fam, int32_t avoid, uint32_t seed)
{
    gctx_t g;
    uint32_t k, n = 0, pick;
    rnd_seed(&g.r, seed);
    g.ind = RI(&g, 40, 100);                         /* (the tastes leaning industrial and IDM, always) */
    g.idm = RI(&g, 30, 100);
    g.abs_ = RI(&g, 0, 100);
    if (fam < 0 || (uint32_t)fam >= NRECIPE || RECIPES[fam].mach != m) {
        for (k = 0; k < NRECIPE; k++)
            n += RECIPES[k].mach == m && (int32_t)k != avoid;
        pick = (uint32_t)rnd_below(&g.r, (int32_t)maxi((int32_t)n, 1));
        for (k = 0; k < NRECIPE; k++)
            if (RECIPES[k].mach == m && (int32_t)k != avoid && !pick--)
                break;
        fam = (int32_t)(k < NRECIPE ? k : 0);
    }
    memset(s, 0, sizeof *s);
    s->mach = (uint8_t)m;
    s->fmach = RECIPES[fam].fmach;
    s->flt[F_BWRT] = 0;
    RECIPES[fam].fn(s, &g);
    if (!RECIPES[fam].amb) {
        int32_t od0 = s->fx[X_OD];
        grit(s, &g);
        s->amp[A_VOL] = (uint8_t)clampi(s->amp[A_VOL] - (s->fx[X_OD] - od0) / 5, 40, 124);   /* (more drive: less level) */
    }
    s->family = (uint8_t)fam;
    s->seed = seed;
}
static const char *family_name(const sound_t *s) { return s->family < NRECIPE ? RECIPES[s->family].name : ""; }
