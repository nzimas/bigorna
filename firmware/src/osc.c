/* SPDX-License-Identifier: GPL-3.0-only
 * Single cycles, band-limited by octave (tools/gen_tables.py, WT_*): each wave at WT_LV levels, level k keeping
 * 128 >> k harmonics in WT_LEN(k) points (+ 1, the first again: no wrap in the interpolation). They are copied
 * from the flash into RAM at start-up (the flash is slow to read sample by sample) and read with linear
 * interpolation; a level is chosen from the phase increment so that no harmonic passes ~20 kHz. */

static int16_t wt_ram[WT_TOTAL] POOL;
static int16_t sc_ram[SC_TOTAL] POOL;            /* the single cycles (FM TONE's HARM, FM DRUM's operators) */
static int16_t sin_ram[1025];                    /* the sine, in RAM too (the FM operators read it every sample) */

static void wt_init(void)
{
    uint32_t i;
    for (i = 0; i < WT_TOTAL; i++)
        wt_ram[i] = WT_DATA[i];
    for (i = 0; i < SC_TOTAL; i++)
        sc_ram[i] = SC_DATA[i];
    for (i = 0; i < 1025u; i++)
        sin_ram[i] = SINE[i];
}
/* wave w's level for a phase increment, and the table */
AINL uint32_t wt_level(uint32_t inc)
{
    uint32_t k = 0;
    while (k + 1u < WT_LV && inc >= (1u << (24u + k)))   /* (2^24: ~172 Hz, where 128 harmonics reach 22 kHz) */
        k++;
    return k;
}
AINL const int16_t *wt_tab(uint32_t w, uint32_t lv) { return wt_ram + w * WT_WAVE + WT_OFF[lv]; }
AINL const int16_t *sc_tab(uint32_t w) { return sc_ram + w * (SC_LEN + 1u); }   /* (read at sh 24: 256 points) */
/* a read at phase ph of a level's table (sh: 32 - log2 of its length) */
AINL int32_t wt_rd(const int16_t *t, uint32_t sh, uint32_t ph)
{
    uint32_t i = ph >> sh;
    int32_t f = (int32_t)((ph << (32u - sh)) >> 17);
    return t[i] + (((t[i + 1u] - t[i]) * f) >> 15);
}
AINL int32_t sinr(uint32_t ph)                   /* the sine from RAM, Q15 */
{
    const int16_t *s = &sin_ram[ph >> 22];
    return s[0] + (((s[1] - s[0]) * (int32_t)((ph >> 7) & 0x7FFFu)) >> 15);
}

/* a morphing oscillator: a position along a list of waves (two neighbours crossfaded), for a block */
typedef struct {
    const int16_t *a, *b;        /* the two waves, at the level for this block */
    uint32_t sh;
    int32_t x;                   /* the crossfade, Q15 */
} morph_t;
/* x (Q15, 0 .. 1) along the n waves from wave first */
FAST static void morph_set(morph_t *m, uint32_t first, uint32_t n, int32_t x, uint32_t inc)
{
    uint32_t lv = wt_level(inc), seg;
    int32_t q = clampi(x, 0, Q15) * (int32_t)(n - 1u);       /* Q15 */
    seg = (uint32_t)q >> 15;
    if (seg >= n - 1u) {
        seg = n - 1u;
        q = (int32_t)(seg << 15);
    }
    m->a = wt_tab(first + seg, lv);
    m->b = wt_tab(first + (seg + 1u < n ? seg + 1u : seg), lv);
    m->x = q - (int32_t)(seg << 15);
    m->sh = WT_SH[lv];
}
AINL int32_t morph_rd(const morph_t *m, uint32_t ph)
{
    int32_t a = wt_rd(m->a, m->sh, ph);
    if (!m->x)
        return a;
    return a + (((wt_rd(m->b, m->sh, ph) - a) * m->x) >> 15);
}
/* .. along a bank of single cycles (no levels): x Q15 along n from wave first */
FAST static void morph_sc(morph_t *m, uint32_t first, uint32_t n, int32_t x)
{
    int32_t q = clampi(x, 0, Q15) * (int32_t)(n - 1u);
    uint32_t seg = (uint32_t)q >> 15;
    if (seg >= n - 1u) {
        seg = n - 1u;
        q = (int32_t)(seg << 15);
    }
    m->a = sc_tab(first + seg);
    m->b = sc_tab(first + (seg + 1u < n ? seg + 1u : seg));
    m->x = q - (int32_t)(seg << 15);
    m->sh = 24u;
}
