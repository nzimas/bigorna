/* SPDX-License-Identifier: GPL-3.0-only
 * The six tracks and their voices (after TONITOR's layers.c: its voice pool, its events, its budget).
 *
 * A track plays one SYN machine, with a sound of each machine of its own, so turning the machine back finds the
 * sound it had. The voices come from one pool of NVOICE for all the tracks: a note takes a free voice, whichever
 * track it is on, and lets the others ring, so a track has as many voices as its notes need. With none free, a
 * note takes the voice that has been releasing longest (any track's), else the track's own oldest held note,
 * else the oldest held note of all -- faded out over FADE blocks (~9 ms) first.
 *
 * The main loop owns the sounds (sound_t, the Digitone II's units); it cooks the one a track plays into ck_t and
 * publishes it in one of two buffers a track: the ISR takes the new one at a block start (use = pub). A new
 * machine under sounding voices fades the track out first, then plays the notes still held with the new one; a
 * changed parameter is simply taken, the voices sounding on (as turning a knob on the original).
 *
 * The ISR, a block: the events (notes from the main loop), the budget, each track's voices (machine -> the voice
 * chain, voice.c) summed, its level and pan, its sends; the chorus, the delay, the reverb; the master. */

#define NTRACK 6
typedef char mg_fits[MG_TRACKS == NTRACK ? 1 : -1];
#define NVOICE 12
static const char *const ENG_NAME[NENG] = {"FM TONE", "FM DRUM", "WAVETONE", "SWARMER"};
static const char *const FLT_NAME[NFLT] = {"MULTI-MODE", "LOWPASS 4", "LEGACY", "COMB-", "COMB+", "EQUALIZER"};

typedef union { ftck_t ft; fdck_t fd; wtck_t wt; swck_t sw; } mck_t;   /* a machine's, cooked */
typedef struct {
    uint8_t mach;
    vck_t ch;
    mck_t m;
    int32_t sd_del, sd_rev, sd_chr;      /* the sends, Q15 */
    sound_t snd;                         /* (the sound itself: a voice's locks cooked from it; MANGLE's values) */
} ck_t;
static ck_t ck_buf[NTRACK][2];
static volatile uint8_t ck_pub[NTRACK], ck_use[NTRACK];

static void mck_cook(mck_t *m, const sound_t *s)
{
    switch (s->mach) {
    case ENG_FMT:
        ft_cook(&m->ft, s);
        break;
    case ENG_FMD:
        fd_cook(&m->fd, s);
        break;
    case ENG_WAV:
        wt_cook(&m->wt, s);
        break;
    default:
        sw_cook(&m->sw, s);
        break;
    }
}
static void ck_cook(ck_t *c, const sound_t *s)
{
    memset(c, 0, sizeof *c);
    c->mach = s->mach;
    vck_cook(&c->ch, s);
    mck_cook(&c->m, s);
    c->sd_del = s->fx[X_DEL] * s->fx[X_DEL] * Q15 / 16129;   /* (the sends: v^2, as the original) */
    c->sd_rev = s->fx[X_REV] * s->fx[X_REV] * Q15 / 16129;
    c->sd_chr = s->fx[X_CHR] * s->fx[X_CHR] * Q15 / 16129;
    c->snd = *s;
}
static int32_t send_q15(int32_t v) { return v * v * Q15 / 16129; }

/* each track's sidechain (the COMP page's second half, the chosen track's: SC SOURCE, SC LEVEL): the track ducked
 * under another's hits (fx.c's dk_*), by SC LEVEL, 0 .. 100 (raw 0 .. 127): up to 45 dB on the hit, near a gate.
 * SC SOURCE 0: off */
static volatile uint8_t sc_src[NTRACK], sc_lvl[NTRACK] = {64, 64, 64, 64, 64, 64};
static dk_t dk[NTRACK];                          /* (the ISR's: each source's hits) */
static int32_t dk_att[NTRACK];                   /* (the duck now, dB Q16) */
static int32_t dk_gp[NTRACK] = {Q16, Q16, Q16, Q16, Q16, Q16};   /* (its gain as the last block ended) */
static uint8_t dk_mask;                          /* (the tracks some track ducks under) */
static uint8_t dk_struck[NTRACK];                /* (a note began on the track this block: a hit) */

/* a step's parameter locks (the sequencer's): up to NPL a step, each a parameter of the sound -- its id, page x 8 +
 * knob (params.c: SYN 1 .. 4, FLTR 1, FLTR 2, AMP, FX, MANGLE), or PL_PAN -- and its value. The note's own (the
 * machine, the filter, the amp, the drive ..) are cooked into the voice as its note starts; the track's (the sends,
 * MANGLE, the pan) hold from the step until the track's next step plays. PL_NONE: a free place */
#define NPL 8u
#define PL_NONE 0xFFu
#define PL_PAN 72u
#define PL_DEL (7u * 8u + 4u)                    /* (FX: DELAY, REVERB, CHORUS SEND) */
#define PL_REV (7u * 8u + 5u)
#define PL_CHR (7u * 8u + 6u)
#define PL_MNG (8u * 8u)                         /* (MANGLE: 64 .. 71) */
static int pl_track(uint32_t id) { return id == PL_DEL || id == PL_REV || id == PL_CHR || id >= PL_MNG; }
static void par_put(sound_t *s, uint32_t id, uint32_t j, int32_t v);   /* (params.c) */

/* a voice's own memory: the chain (its comb line) and the machine's state */
typedef struct {
    vchain_t ch;
    union { ftv_t ft; fdv_t fd; wtv_t wt; swv_t sw; } m;
    vck_t lck;                   /* the chain and .. */
    mck_t lm;                    /* .. the machine, cooked with its note's locks (voice_t own) */
} vmem_t;
static vmem_t vmem[NVOICE] POOL;

#define FADE 12u                 /* blocks of a fade (~9 ms) */
#define QUICK 3u                 /* blocks of the budget's fade (~2 ms) */
enum { SRC_KEY, SRC_SEQ };       /* who plays a note: the keys (and MIDI), or the sequencer */
typedef struct {
    uint8_t note, gate, src;     /* the note, held, by whom */
    uint8_t sounding;            /* the voice renders */
    uint8_t fade, fade_n;        /* blocks of a steal's fade left (0 none), and its length when not FADE */
    uint8_t pend;                /* the fade ends in a note-on: note / gate / src are the new note's */
    uint8_t track, pend_track;   /* whose it is; (pend) whose the new note is */
    uint8_t eng;                 /* the machine its memory is playing */
    uint8_t vel;                 /* its note's level, 1 .. 127 */
    uint32_t age, born;
    uint16_t cost;
    uint8_t lid[NPL];            /* its note's own locks (the pending note's, while it fades for it): ids .. */
    int16_t lv[NPL];             /* .. and values */
    uint8_t own;                 /* .. cooked into its vmem's lck and lm: the voice plays them */
} voice_t;
static voice_t vox[NVOICE];
static uint32_t vox_owner(const voice_t *v) { return v->pend ? v->pend_track : v->track; }

typedef struct {
    uint8_t xfade;               /* blocks of the track's fade left (a change of machine) */
    int32_t meter;               /* |output| peak, decaying (Q15): the screen's */
    int32_t lim_env, lim_g;      /* the limiter */
    int32_t g, pl, pr;           /* the level and pan as the last block ended (they glide) */
    int32_t sd, sr, sc;          /* .. the sends */
    int32_t dc;                  /* the DC blocker's low-pass (x 256) */
    uint8_t tid[NPL];            /* the track's locks from the step last played (the sends, MANGLE, the pan): ids .. */
    int16_t tv[NPL];             /* .. and values */
} track_t;
static track_t trk[NTRACK];
/* each track's level (Q14) and equal-power pan (Q15), set by the main loop */
static volatile int32_t trk_gain[NTRACK], trk_pl[NTRACK], trk_pr[NTRACK];

/* the budget (as TONITOR's): each machine's voice cost measured as it renders, so a start that would overrun the
 * block lets the quietest release tails go first (tail_victim), over ~6 ms, rather than the audio arriving late */
#define COST_NOW() fm1_ticks()
#define BLK_TICKS ((uint32_t)CTL * 1000000u / FS * FM1_TICKS_PER_US)
#define BUDGET (BLK_TICKS * 90u / 100u)   /* (82 % until build 28: tails went that the time had room for) */
#define SHED 8u                  /* blocks of the budget's fade (~6 ms: no click) */
static uint32_t eng_cost[NENG];
static uint32_t blk_last, blk_new;
static uint32_t blk_clock;
static volatile uint8_t shed_req;
static volatile uint8_t load_pct;
static int32_t master_q12 = 2048;

/* --------------------------------------------------------------- events --- */
enum { EV_ON, EV_OFF, EV_STOP };
typedef struct { uint8_t type, track, note, val; } ev_t;
#define EVQ 64u
static ev_t evq[EVQ];
static volatile uint32_t ev_w, ev_r;
static void ev_post(uint32_t type, uint32_t t, uint32_t note, uint32_t val)   /* main loop */
{
    ev_t e;
    if (ev_w - ev_r >= EVQ || t >= NTRACK)
        return;
    e.type = (uint8_t)type;
    e.track = (uint8_t)t;
    e.note = (uint8_t)note;
    e.val = (uint8_t)val;
    evq[ev_w % EVQ] = e;
    RING_PUBLISH();
    ev_w++;
}

/* ---------------------------------------------------------------- voices --- */
static uint32_t vox_age;
static const vck_t *vck_of(uint32_t k, const ck_t *c) { return vox[k].own ? &vmem[k].lck : &c->ch; }
static const mck_t *mck_of(uint32_t k, const ck_t *c) { return vox[k].own ? &vmem[k].lm : &c->m; }
/* the voice cooked with its note's locks: the track's sound, the locked parameters set (none: the track's own) */
static void voice_locks(uint32_t k, const ck_t *c)
{
    voice_t *v = &vox[k];
    sound_t s;
    uint32_t j;
    for (j = 0; j < NPL && v->lid[j] == PL_NONE; j++)
        ;
    v->own = (uint8_t)(j < NPL);
    if (!v->own)
        return;
    s = c->snd;
    for (j = 0; j < NPL; j++)
        if (v->lid[j] < PL_MNG)
            par_put(&s, v->lid[j] >> 3, v->lid[j] & 7u, v->lv[j]);
    vck_cook(&vmem[k].lck, &s);
    mck_cook(&vmem[k].lm, &s);
}
static void voice_on(uint32_t t, uint32_t k, int fresh)
{
    voice_t *v = &vox[k];
    const ck_t *c = &ck_buf[t][ck_use[t]];
    vmem_t *m = &vmem[k];
    uint32_t seed = 0x5EEDu + k * 977u + vox_age * 131u;
    if (v->track != t || v->eng != c->mach || !v->sounding)
        fresh = 1;
    if (!v->sounding || v->track != t)
        blk_new += eng_cost[c->mach];
    v->track = (uint8_t)t;
    v->age = ++vox_age;
    v->born = blk_clock;
    v->eng = c->mach;
    voice_locks(k, c);
    switch (c->mach) {
    case ENG_FMT:
        ft_on(&m->m.ft, &mck_of(k, c)->ft, fresh);
        break;
    case ENG_FMD:
        fd_on(&m->m.fd, &mck_of(k, c)->fd, fresh, seed);
        break;
    case ENG_WAV:
        wt_on(&m->m.wt, &mck_of(k, c)->wt, fresh, seed);
        break;
    default:
        sw_on(&m->m.sw, fresh, seed);
        break;
    }
    vc_on(&m->ch, vck_of(k, c), fresh);
    v->sounding = 1;
}
static void voice_off(uint32_t t, uint32_t k)
{
    const ck_t *c = &ck_buf[t][ck_use[t]];
    vmem_t *m = &vmem[k];
    if (vox[k].eng == ENG_FMT)
        ft_off(&m->m.ft);
    else if (vox[k].eng == ENG_WAV)
        wt_off(&m->m.wt, &mck_of(k, c)->wt);
    vc_off(&m->ch, vck_of(k, c));
}
static void voice_silence(uint32_t k)
{
    vox[k].sounding = 0;
    vox[k].fade = vox[k].fade_n = 0;
}
/* a voice for a note on track t: a free one; else the oldest releasing one (any track's); else the track's own
 * oldest held; else the oldest held of all. own: only free ones or the track's own (it is fading for a change) */
static uint32_t voice_pick(uint32_t t, int own)
{
    uint32_t k, best = NVOICE, pass;
    for (k = 0; k < NVOICE; k++)
        if (!vox[k].sounding && !vox[k].gate && !vox[k].pend)
            return k;
    for (pass = 0; pass < 3u && best == NVOICE; pass++)
        for (k = 0; k < NVOICE; k++) {
            const voice_t *v = &vox[k];
            uint32_t o = vox_owner(v);
            if (v->pend || (own && o != t) || (pass == 0u ? v->gate : pass == 1u ? o != t : 0))
                continue;
            if (best == NVOICE || v->age < vox[best].age)
                best = k;
        }
    return best;
}
static uint8_t note_vel = 127;
static const volatile uint8_t *note_lid;         /* the note's locks (the sequencer's step): ids, 0 none .. */
static const volatile int16_t *note_lv;          /* .. values */
static void note_on(uint32_t t, uint32_t note, uint32_t src)
{
    track_t *y = &trk[t];
    uint32_t best;
    voice_t *v;
    if (!y->xfade && ck_buf[t][ck_use[t]].mach == ENG_FMD) {   /* an FM DRUM track chokes, as a drum machine's: its
                                                              * last hit faded out (~2 ms) as the next strikes,
                                                              * the tails never piling up */
        uint32_t k;
        for (k = 0; k < NVOICE; k++) {
            voice_t *w = &vox[k];
            if (w->track == t && w->sounding && !w->pend) {
                w->gate = 0;
                if (!w->fade) {
                    w->fade = QUICK;
                    w->fade_n = QUICK;
                }
            }
        }
    }
    dk_struck[t] = 1;                            /* (a hit, for the tracks it ducks) */
    best = voice_pick(t, y->xfade != 0);
    if (best == NVOICE)
        return;
    v = &vox[best];
    v->note = (uint8_t)note;
    v->gate = 1;
    v->src = (uint8_t)src;
    v->vel = note_vel;
    {
        uint32_t j;
        for (j = 0; j < NPL; j++) {              /* (the note's own: not the track's) */
            uint32_t id = note_lid ? note_lid[j] : PL_NONE;
            v->lid[j] = (uint8_t)(id != PL_NONE && !pl_track(id) ? id : PL_NONE);
            v->lv[j] = note_lid ? note_lv[j] : 0;
        }
    }
    v->age = ++vox_age;
    if (y->xfade) {                              /* played when the track's change is made */
        if (!v->sounding)
            v->track = (uint8_t)t;
        return;
    }
    if (v->sounding) {
        if (!v->fade)
            v->fade = FADE;
        v->pend = 1;
        v->pend_track = (uint8_t)t;
    } else
        voice_on(t, best, 1);
}
static void note_off(uint32_t t, uint32_t note, uint32_t src)
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++) {
        voice_t *v = &vox[k];
        if (v->gate && vox_owner(v) == t && v->note == note && v->src == src) {
            v->gate = 0;
            if (!v->pend && !trk[t].xfade)
                voice_off(t, k);
            return;
        }
    }
}
/* a voice's fade over: the note it waited for, on the track it goes to */
static void voice_take(uint32_t k)
{
    voice_t *v = &vox[k];
    uint32_t to = v->pend_track;
    voice_silence(k);
    v->pend = 0;
    v->track = (uint8_t)to;
    if (trk[to].xfade)
        return;
    voice_on(to, k, 1);
    if (!v->gate)
        voice_off(to, k);
}
/* the track's new cooked sound taken; if its machine changed, its voices were faded out first: silenced now, and
 * the notes still held played again with the new machine */
static void change_apply(uint32_t t)
{
    uint32_t k;
    uint8_t nn[NVOICE], ns[NVOICE], n = 0, j;
    for (k = 0; k < NVOICE; k++) {
        voice_t *v = &vox[k];
        if (v->gate && vox_owner(v) == t && n < NVOICE) {
            nn[n] = v->note;
            ns[n++] = v->src;
        }
        if (v->track == t && (v->sounding || v->pend)) {
            voice_silence(k);
            if (v->pend && v->pend_track != t) {
                v->pend = 0;
                v->track = v->pend_track;
                if (!trk[v->track].xfade) {
                    voice_on(v->track, k, 1);
                    if (!v->gate)
                        voice_off(v->track, k);
                }
                continue;
            }
        }
        if (vox_owner(v) == t) {
            v->gate = 0;
            v->pend = 0;
        }
    }
    ck_use[t] = ck_pub[t];
    for (j = 0; j < n; j++) {
        for (k = 0; k < NVOICE && (vox[k].sounding || vox[k].gate || vox[k].pend); k++)
            ;
        if (k == NVOICE)
            break;
        vox[k].note = nn[j];
        vox[k].src = ns[j];
        vox[k].gate = 1;
        voice_on(t, k, 1);
    }
}
static int track_sounding(uint32_t t)
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++)
        if (vox[k].track == t && vox[k].sounding)
            return 1;
    return 0;
}

static void events_take(void)
{
    while (ev_r != ev_w) {
        ev_t e = evq[ev_r % EVQ];
        uint32_t k;
        RING_PUBLISH();
        ev_r++;
        switch (e.type) {
        case EV_ON:
            note_vel = (uint8_t)(e.val && e.val < 127u ? e.val : 127u);
            note_on(e.track, e.note, SRC_KEY);
            note_vel = 127;
            break;
        case EV_OFF:
            note_off(e.track, e.note, SRC_KEY);
            break;
        case EV_STOP:
            for (k = 0; k < NVOICE; k++) {
                voice_t *v = &vox[k];
                if (vox_owner(v) == e.track) {
                    v->gate = 0;
                    if (v->pend && v->pend_track == e.track)
                        v->pend = 0;
                }
                if (v->track == e.track && v->sounding && !v->fade)
                    v->fade = FADE;
            }
            break;
        default:
            break;
        }
    }
}

/* ------------------------------------------------------------ the sequencer --- */
/* 16 steps a track, a 16th note each at the tempo. A step holds its note (0: empty), its velocity, its length and its
 * trig condition (the Digitone II's: PRE, NEI, 1ST, a probability, A:B, each also negated but the probability). The
 * main loop writes the steps and the lengths (bytes: whole as written), the ISR plays them */
#define NSTEP 16u
#define SEQ_16TH (FS * 15u * 10u)         /* a 16th note, in samples x the tempo x 10 */
#define VEL_DEF 100u
/* a step's length, in eighths of a step; the last, INF, held until the step plays again or the sequencer stops */
#define NGLEN 15u
#define GLEN_DEF 3u                       /* (half a step) */
#define GLEN_INF (NGLEN - 1u)
static const uint8_t GLEN_8TH[NGLEN] = {1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 0};
static const char *const GLEN_NAME[NGLEN] = {"1/8", "1/4", "3/8", "1/2", "3/4", "1", "1.5", "2", "3", "4", "6", "8",
                                            "12", "16", "INF"};
/* the conditions: 0 none, then PRE, !PRE, NEI, !NEI, 1ST, !1ST, the probabilities, then A:B and !A:B in turn for
 * B 2 .. 8, A 1 .. B */
enum { CD_NONE, CD_PRE, CD_NPRE, CD_NEI, CD_NNEI, CD_1ST, CD_N1ST, CD_PROB };
#define NPROB 21u
#define CD_AB (CD_PROB + NPROB)
#define NCOND (CD_AB + 70u)
static const uint8_t PROB[NPROB] = {1, 2, 4, 6, 9, 13, 19, 25, 33, 41, 50, 59, 67, 75, 81, 87, 91, 94, 96, 98, 99};
static void cond_ab(uint32_t c, uint32_t *a, uint32_t *b)   /* (c an A:B condition: its A and B) */
{
    uint32_t i = (c - CD_AB) / 2u;
    for (*b = 2u; i >= *b; *b += 1u)
        i -= *b;
    *a = i + 1u;
}
static volatile uint8_t seq_note[NTRACK][NSTEP];
static volatile uint8_t seq_vel[NTRACK][NSTEP], seq_glen[NTRACK][NSTEP], seq_cond[NTRACK][NSTEP];
static volatile uint8_t seq_lid[NTRACK][NSTEP][NPL];    /* the steps' locks: ids (PL_NONE: a free place) .. */
static volatile int16_t seq_lv[NTRACK][NSTEP][NPL];     /* .. and values (written before the id: the ISR reads a
                                                         * lock whole) */
static volatile uint8_t seq_skip[NTRACK][NSTEP];   /* a step not to play when it next comes (recorded just before
                                                    * it: the key played it already) */
static volatile uint32_t seq_acc_now;     /* (the ISR's: how far into the step playing, of SEQ_16TH) */
/* the metronome: a click on each beat (higher on the bar's first), while met_on; with the sequencer stopped it
 * keeps the tempo by itself */
static volatile uint8_t met_on;
static uint32_t met_acc, met_n, met_ph, met_inc;
static uint8_t met_was, met_step, seq_gstep;
#define MET_LEN 1100u                     /* ~25 ms */
static void met_tick(uint32_t step)
{
    if (!met_on || (step & 3u))
        return;
    met_n = MET_LEN;
    met_ph = 0;
    met_inc = (step ? 1000u : 2000u) * 97391u;   /* (Hz x 2^32 / FS) */
}
static void met_block(int32_t *l, int32_t *r)
{
    uint32_t i;
    for (i = 0; i < CTL && met_n; i++) {
        int32_t x = mulq15(sin_ph(met_ph), (int32_t)(met_n * (uint32_t)(Q15 / 5) / MET_LEN));
        met_ph += met_inc;
        met_n--;
        l[i] += x;
        r[i] += x;
    }
}
static volatile uint8_t seq_len[NTRACK];  /* (0: 16) */
/* each track's clock against the tempo's 16ths (TRACK page): 1/8, 1/4, 1/2, 1 (SEQ_DIV1), 2, 4 times as fast -- in
 * eighths of the tempo's (DIV_R8). Its own count (seq_tacc: a step is SEQ_STEP8), from PLAY; the metronome, the looper
 * and MANGLE's clock keep the tempo's */
#define NDIV 6u
#define SEQ_DIV1 3u
#define SEQ_STEP8 (SEQ_16TH * 8u)
static const uint8_t DIV_R8[NDIV] = {1, 2, 4, 8, 16, 32};
static const char *const DIV_NAME[NDIV] = {"1/8", "1/4", "1/2", "1X", "2X", "4X"};
static volatile uint8_t seq_div[NTRACK] = {SEQ_DIV1, SEQ_DIV1, SEQ_DIV1, SEQ_DIV1, SEQ_DIV1, SEQ_DIV1};
static uint32_t seq_tacc[NTRACK];
/* each track's direction through its steps (the step view's 4th .. 6th black keys) */
enum { DIR_FWD, DIR_REV, DIR_PING, DIR_RND, NDIR };
static volatile uint8_t seq_dir[NTRACK];
static int8_t seq_ppd[NTRACK];            /* (PING: the way it goes now) */
static uint8_t seq_rnd_n[NTRACK];         /* (RANDOM: steps since the count of its times through moved) */
/* a step's ratchet: the step played RTG_N times, evenly through it, each a little softer (index 0: once) */
#define NRTG 6u
static const uint8_t RTG_N[NRTG] = {1, 2, 3, 4, 6, 8};
static const char *const RTG_NAME[NRTG] = {"OFF", "2", "3", "4", "6", "8"};
static volatile uint8_t seq_rtg[NTRACK][NSTEP];
static uint8_t rt_left[NTRACK], rt_note[NTRACK], rt_vel[NTRACK], rt_step[NTRACK];
static uint32_t rt_next[NTRACK], rt_int[NTRACK];
/* a step's MOD (the step view's 4th black key): MUTATE -- its note moved by so many steps of the scale (-12 .. 12;
 * MUT_RND: a step of the scale drawn at random, up to an octave either way) -- every (seq_mcy + 1) times it plays;
 * and a MIDI FX every (seq_fcy + 1) times it plays (after TONITOR's looper VARY and BRICKLAYER): echoes (rising,
 * falling), a stutter, a flam, late, a grace note before it, a fifth or a triad with it, an octave up or down, or
 * dropped. The times are the step's own: counted as it plays (its condition met), from PLAY */
#define MUT_RND 100
enum { MFX_OFF, MFX_ECHO, MFX_ECHOUP, MFX_ECHODN, MFX_STUT, MFX_FLAM, MFX_LATE, MFX_GRACE, MFX_FIFTH, MFX_TRIAD,
       MFX_OCTUP, MFX_OCTDN, MFX_DROP, NMFX };
static const char *const MFX_NAME[NMFX] = {"OFF", "ECHO", "ECHO UP", "ECHO DN", "STUTTER", "FLAM", "LATE", "GRACE",
                                           "FIFTH", "TRIAD", "OCT UP", "OCT DN", "DROP"};
static volatile int8_t seq_mut[NTRACK][NSTEP];
static volatile uint8_t seq_mcy[NTRACK][NSTEP], seq_mfx[NTRACK][NSTEP], seq_fcy[NTRACK][NSTEP];
static uint8_t seq_plays[NTRACK][NSTEP];         /* (the ISR's: the times each step has played since PLAY) */
static uint8_t seq_played[NTRACK];               /* (the note a track's step played last, mutated: the tests') */
/* the scale, for MUTATE and the MIDI FX (the main loop's, as it is set): its notes from the root (bit k: k
 * semitones up), the root */
static volatile uint16_t seq_smask = 0xFFFu;
static volatile uint8_t seq_sroot;
static int sc_in(int32_t n) { return (int)((seq_smask >> ((uint32_t)(n - seq_sroot + 120) % 12u)) & 1u); }
static int32_t sc_move(int32_t n, int32_t d)     /* n moved d steps of the scale */
{
    int32_t u = d > 0 ? 1 : -1, m;
    for (; d; d -= u) {
        for (m = n + u; m >= 12 && m <= 120 && !sc_in(m); m += u)
            ;
        if (m < 12 || m > 120)
            break;
        n = m;
    }
    return n;
}
static int32_t sc_snap(int32_t n)                /* the note of the scale nearest n (below first) */
{
    int32_t d;
    for (d = 0; d < 6; d++) {
        if (sc_in(n - d))
            return n - d;
        if (sc_in(n + d))
            return n + d;
    }
    return n;
}
static int32_t sc_count(void)
{
    uint32_t m = seq_smask & 0xFFFu, c = 0;
    for (; m; m &= m - 1u)
        c++;
    return (int32_t)(c ? c : 12u);
}
/* the MIDI FX's notes to come: a track's, each so many blocks from now (note 0: a free place) */
#define FXQ 8u
typedef struct { uint16_t left; uint8_t note, vel, len8, step; } fxq_t;
static fxq_t fxq[NTRACK][FXQ];
static void fxq_add(uint32_t t, uint32_t blocks, int32_t note, int32_t vel, uint32_t len8, uint32_t step)
{
    uint32_t k;
    for (k = 0; k < FXQ && fxq[t][k].note; k++)
        ;
    if (k == FXQ || note < 12 || note > 120)
        return;
    fxq[t][k].left = (uint16_t)(blocks ? (blocks < 0xFFFFu ? blocks : 0xFFFFu) : 1u);
    fxq[t][k].vel = (uint8_t)clampi(vel, 1, 127);
    fxq[t][k].len8 = (uint8_t)clampi((int32_t)len8, 1, 255);
    fxq[t][k].step = (uint8_t)step;
    fxq[t][k].note = (uint8_t)note;
}

static volatile uint32_t seq_tacc_now[NTRACK];   /* (as the block ended: where in its step the track is) */
static volatile uint8_t seq_run;          /* PLAY */
static volatile uint8_t seq_pos[NTRACK];  /* the step playing (the ISR's; the screen and the LEDs read it) */
static uint32_t seq_acc, seq_rnd = 0x2545F491u;
static uint8_t seq_was;
static uint16_t seq_loop[NTRACK];         /* the times the track has played through since PLAY */
static uint8_t seq_pre[NTRACK];           /* the last condition evaluated on the track (but PRE): 0 none yet, 1 false,
                                           * 2 true */
#define SEQ_HOLD 6u                       /* notes a track's steps and loops keep sounding at once */
static uint8_t sq_note[NTRACK][SEQ_HOLD];
static uint16_t sq_left[NTRACK][SEQ_HOLD];   /* blocks left (0xFFFF: INF) */
static void seq_release(void)
{
    uint32_t t, i;
    memset(fxq, 0, sizeof fxq);
    for (t = 0; t < NTRACK; t++)
        for (i = 0; i < SEQ_HOLD; i++)
            if (sq_note[t][i]) {
                note_off(t, sq_note[t][i], SRC_SEQ);
                sq_note[t][i] = 0;
            }
}
static int seq_cond_ok(uint32_t t, uint32_t c)
{
    int r;
    if (c == CD_NONE)
        return 1;
    if (c == CD_PRE || c == CD_NPRE)
        return (seq_pre[t] == 2u) == (c == CD_PRE);
    if (c == CD_NEI || c == CD_NNEI)
        r = (t > 0u && seq_pre[t - 1u] == 2u) == (c == CD_NEI);
    else if (c == CD_1ST || c == CD_N1ST)
        r = (seq_loop[t] == 0u) == (c == CD_1ST);
    else if (c < CD_AB) {
        seq_rnd = seq_rnd * 1664525u + 1013904223u;
        r = (seq_rnd >> 16) % 100u < PROB[c - CD_PROB];
    } else {
        uint32_t a, b;
        cond_ab(c, &a, &b);
        r = (seq_loop[t] % b == a - 1u) == !((c - CD_AB) & 1u);
    }
    seq_pre[t] = (uint8_t)(r ? 2u : 1u);
    return r;
}
/* a note of the sequencer's (a step's, a loop's) on track t, for len8 eighths of a step (0: until it plays again or
 * the sequencer stops), with the locks lk (0 none) */
static uint32_t seq_sounded[NTRACK];             /* (the notes the sequencer has played a track: the tests count them) */
static void seq_sound(uint32_t t, uint32_t n, uint32_t vel, uint32_t len8, const volatile uint8_t *lid,
                      const volatile int16_t *lv)
{
    seq_sounded[t]++;
    uint32_t i, k = SEQ_HOLD;
    for (i = 0; i < SEQ_HOLD; i++)               /* (the same note sounding: let go first, its slot taken) */
        if (sq_note[t][i] == n || (!sq_note[t][i] && k == SEQ_HOLD))
            k = i;
    if (k == SEQ_HOLD)                           /* (all sounding: the one nearest its end) */
        for (k = 0, i = 1; i < SEQ_HOLD; i++)
            if (sq_left[t][i] < sq_left[t][k])
                k = i;
    if (sq_note[t][k])
        note_off(t, sq_note[t][k], SRC_SEQ);
    note_vel = (uint8_t)(vel ? vel : VEL_DEF);
    note_lid = lid;
    note_lv = lv;
    note_on(t, n, SRC_SEQ);
    note_lid = 0;
    note_vel = 127;
    sq_note[t][k] = (uint8_t)n;
    if (!len8)
        sq_left[t][k] = 0xFFFFu;
    else {   /* (blocks: eighths x the blocks of an eighth, x 16 for its fraction) */
        uint32_t bl = (len8 < 4096u ? len8 : 4096u) * (SEQ_16TH / 8u * 16u / (CTL * (uint32_t)clampi(fx_bpm10, 100, 3000))) / 16u;
        sq_left[t][k] = (uint16_t)(bl ? (bl < 0xFFFEu ? bl : 0xFFFEu) : 1u);
    }
}
/* the step track t plays next, by its direction (and its times through counted, for A:B and 1ST) */
static uint32_t seq_next(uint32_t t, uint32_t len)
{
    uint32_t pos = seq_pos[t], p;
    int first = pos == 0xFFu;
    if (!first && pos >= len)
        pos = len - 1u;                          /* (the length made shorter under it) */
    switch (seq_dir[t]) {
    case DIR_REV:
        p = first || pos == 0u ? len - 1u : pos - 1u;
        if (!first && pos == 0u)
            seq_loop[t]++;
        break;
    case DIR_PING:                               /* (0 .. len - 1 .. 0 .., the ends once each) */
        if (first || len < 2u) {
            seq_ppd[t] = 1;
            p = 0;
            break;
        }
        if (seq_ppd[t] > 0 && pos + 1u >= len)
            seq_ppd[t] = -1;
        else if (seq_ppd[t] < 0 && pos == 0u) {
            seq_ppd[t] = 1;
            seq_loop[t]++;
        }
        p = (uint32_t)((int32_t)pos + seq_ppd[t]);
        break;
    case DIR_RND:
        seq_rnd = seq_rnd * 1664525u + 1013904223u;
        p = (seq_rnd >> 16) % len;
        if (!first && ++seq_rnd_n[t] >= len) {
            seq_rnd_n[t] = 0;
            seq_loop[t]++;
        }
        break;
    default:
        p = first ? 0u : pos + 1u;
        if (p >= len) {
            p = 0;
            seq_loop[t]++;
        }
        break;
    }
    return p < len ? p : 0u;
}
static void seq_trig(uint32_t t, uint32_t p)
{
    uint32_t n = seq_note[t][p], g = seq_glen[t][p], i;
    if (seq_skip[t][p]) {
        seq_skip[t][p] = 0;
        return;
    }
    if (!n || !seq_cond_ok(t, seq_cond[t][p]))
        return;
    {
        uint32_t r8 = DIV_R8[seq_div[t] % NDIV], rn = RTG_N[seq_rtg[t][p] % NRTG], c = ++seq_plays[t][p], fx = MFX_OFF;
        uint32_t len8 = g >= GLEN_INF ? 0u : (uint32_t)maxi(1, GLEN_8TH[g] * 8 / (int32_t)r8);   /* (in the tempo's
                                                                                              * 16ths) */
        uint32_t sb = SEQ_STEP8 / (CTL * (uint32_t)clampi(fx_bpm10, 100, 3000) * r8);   /* (the step, in blocks) */
        uint32_t vel = seq_vel[t][p], k, l8 = len8 ? len8 : 64u / r8;
        int32_t m = seq_mut[t][p];
        if (m && c % (seq_mcy[t][p] + 1u) == 0u) {   /* MUTATE */
            int32_t d = m;
            if (m == MUT_RND) {
                int32_t nn = sc_count();
                seq_rnd = seq_rnd * 1664525u + 1013904223u;
                d = (int32_t)((seq_rnd >> 16) % (uint32_t)(2 * nn + 1)) - nn;
            }
            n = (uint32_t)sc_move((int32_t)n, d);
        }
        if (seq_mfx[t][p] && c % (seq_fcy[t][p] + 1u) == 0u)
            fx = seq_mfx[t][p] % NMFX;
        for (k = 0; k < NPL; k++) {              /* (the track's locks: until its next step plays) */
            uint32_t id = seq_lid[t][p][k];
            trk[t].tv[k] = seq_lv[t][p][k];
            trk[t].tid[k] = (uint8_t)(id != PL_NONE && pl_track(id) ? id : PL_NONE);
        }
        seq_played[t] = (uint8_t)n;
        if (fx == MFX_DROP)
            return;
        if (fx == MFX_LATE || fx == MFX_GRACE) { /* the note a little late (a grace note a step below before it) */
            if (fx == MFX_GRACE)
                seq_sound(t, (uint32_t)sc_move((int32_t)n, -1), vel * 3u / 5u, 2u, seq_lid[t][p], seq_lv[t][p]);
            fxq_add(t, fx == MFX_LATE ? sb / 4u : sb / 6u, (int32_t)n, (int32_t)vel, l8, p);
            return;
        }
        if (rn > 1u) {                           /* a ratchet: the hits after the first, as the step goes on */
            uint32_t hit8 = (uint32_t)maxi(1, (int32_t)(64u / r8 / (2u * rn)));
            if (len8 == 0u || len8 > hit8)
                len8 = hit8;
            rt_left[t] = (uint8_t)(rn - 1u);
            rt_int[t] = SEQ_STEP8 / rn;
            rt_next[t] = seq_tacc[t] + rt_int[t];
            rt_note[t] = (uint8_t)n;
            rt_vel[t] = seq_vel[t][p];
            rt_step[t] = (uint8_t)p;
        }
        seq_sound(t, n, vel, len8, seq_lid[t][p], seq_lv[t][p]);
        switch (fx) {                            /* the MIDI FX's notes with it, and to come */
        case MFX_ECHO:
        case MFX_ECHOUP:
        case MFX_ECHODN: {
            int32_t e = (int32_t)n, d = fx == MFX_ECHOUP ? 1 : fx == MFX_ECHODN ? -1 : 0;
            for (k = 1; k <= 2u; k++) {
                e = sc_move(e, d);
                fxq_add(t, sb * k / 2u, e, (int32_t)(vel * (k == 1u ? 3u : 9u) / (k == 1u ? 5u : 25u)), l8, p);
            }
            break;
        }
        case MFX_STUT:
            for (k = 1; k <= 3u; k++)
                fxq_add(t, sb * k / 8u, (int32_t)n, (int32_t)(vel * (8u - k) / 8u), 1u, p);
            break;
        case MFX_FLAM:
            fxq_add(t, 16u, (int32_t)n, (int32_t)(vel * 3u / 4u), l8, p);   /* (~12 ms) */
            break;
        case MFX_FIFTH:
            seq_sound(t, (uint32_t)sc_snap((int32_t)n + 7), vel * 4u / 5u, len8, seq_lid[t][p], seq_lv[t][p]);
            break;
        case MFX_TRIAD:
            seq_sound(t, (uint32_t)sc_move((int32_t)n, 2), vel * 4u / 5u, len8, seq_lid[t][p], seq_lv[t][p]);
            seq_sound(t, (uint32_t)sc_snap((int32_t)n + 7), vel * 4u / 5u, len8, seq_lid[t][p], seq_lv[t][p]);
            break;
        case MFX_OCTUP:
        case MFX_OCTDN:
            if ((fx == MFX_OCTUP && n <= 108u) || (fx == MFX_OCTDN && n >= 24u))
                seq_sound(t, fx == MFX_OCTUP ? n + 12u : n - 12u, vel * 4u / 5u, len8, seq_lid[t][p], seq_lv[t][p]);
            break;
        default:
            break;
        }
    }
    (void)i;
}
static void fxq_block(void)                      /* (each block, playing: the MIDI FX's notes come due) */
{
    uint32_t t, k;
    for (t = 0; t < NTRACK; t++)
        for (k = 0; k < FXQ; k++) {
            fxq_t *q = &fxq[t][k];
            if (q->note && !--q->left) {
                seq_sound(t, q->note, q->vel, q->len8, seq_lid[t][q->step], seq_lv[t][q->step]);
                q->note = 0;
            }
        }
}

/* ---------------------------------------------------------------- the looper --- */
/* up to three loops a track, each its own length in steps (up to 16 bars), played over the track's steps in time
 * with them: notes at steps (recorded quantized: ui.c), each with its velocity and its length in eighths of a step.
 * A loop being recorded is not played (on 0) until it is ended; then it plays from its first step on the step it
 * ended at (anchor, in seq_gcount's count) */
#define NLOOP 3u
#define LOOP_EV 128u
#define LOOP_MAX 256u
typedef struct { uint16_t step, len8; uint8_t note, vel; } lev_t;
typedef struct {
    volatile uint8_t on, mute;
    volatile uint8_t kick;       /* ended after its first step had gone by: that step's notes played at once */
    uint16_t len, n;
    volatile uint32_t anchor;
    uint16_t cur;                /* (the ISR's: the next note) */
    lev_t ev[LOOP_EV];
} loop_t;
static loop_t loops[NTRACK][NLOOP];
static volatile uint32_t seq_gcount;      /* the steps played since PLAY (the first: 0) */
static void loop_kick(void)                      /* (each block: a loop's first step, late) */
{
    uint32_t t, l;
    for (t = 0; t < NTRACK; t++)
        for (l = 0; l < NLOOP; l++) {
            loop_t *p = &loops[t][l];
            if (!p->on || !p->kick)
                continue;
            for (p->cur = 0; p->cur < p->n && p->ev[p->cur].step == 0u; p->cur++)
                if (!p->mute)
                    seq_sound(t, p->ev[p->cur].note, p->ev[p->cur].vel, p->ev[p->cur].len8, 0, 0);
            p->kick = 0;
        }
}
static void loop_step(void)
{
    uint32_t t, l;
    for (t = 0; t < NTRACK; t++)
        for (l = 0; l < NLOOP; l++) {
            loop_t *p = &loops[t][l];
            uint32_t ph;
            if (!p->on || !p->len || (int32_t)(seq_gcount - p->anchor) < 0 || p->kick)
                continue;
            ph = (seq_gcount - p->anchor) % p->len;
            if (!ph)
                p->cur = 0;
            while (p->cur < p->n && p->ev[p->cur].step < ph)
                p->cur++;
            for (; p->cur < p->n && p->ev[p->cur].step == ph; p->cur++)
                if (!p->mute)
                    seq_sound(t, p->ev[p->cur].note, p->ev[p->cur].vel, p->ev[p->cur].len8, 0, 0);
        }
}
/* a pattern's steps whole, as the sequencer plays them (projects.c fills one, the ISR takes it in) */
typedef struct {
    uint8_t note[NTRACK][NSTEP], vel[NTRACK][NSTEP], glen[NTRACK][NSTEP], cond[NTRACK][NSTEP], rtg[NTRACK][NSTEP];
    int8_t mut[NTRACK][NSTEP];
    uint8_t mcy[NTRACK][NSTEP], mfx[NTRACK][NSTEP], fcy[NTRACK][NSTEP];
    uint8_t lid[NTRACK][NSTEP][NPL];
    int16_t lv[NTRACK][NSTEP][NPL];
    uint8_t len[NTRACK], div[NTRACK], dir[NTRACK];
    uint8_t den[NTRACK], cx[NTRACK];             /* (the phrase generator's, the main loop's: not the ISR's) */
} seqbuf_t;
static void seq_copy_out(seqbuf_t *b)            /* the steps playing, into b */
{
    memcpy(b->note, (const void *)seq_note, sizeof b->note);
    memcpy(b->vel, (const void *)seq_vel, sizeof b->vel);
    memcpy(b->glen, (const void *)seq_glen, sizeof b->glen);
    memcpy(b->cond, (const void *)seq_cond, sizeof b->cond);
    memcpy(b->rtg, (const void *)seq_rtg, sizeof b->rtg);
    memcpy(b->mut, (const void *)seq_mut, sizeof b->mut);
    memcpy(b->mcy, (const void *)seq_mcy, sizeof b->mcy);
    memcpy(b->mfx, (const void *)seq_mfx, sizeof b->mfx);
    memcpy(b->fcy, (const void *)seq_fcy, sizeof b->fcy);
    memcpy(b->lid, (const void *)seq_lid, sizeof b->lid);
    memcpy(b->lv, (const void *)seq_lv, sizeof b->lv);
    memcpy(b->len, (const void *)seq_len, sizeof b->len);
    memcpy(b->div, (const void *)seq_div, sizeof b->div);
    memcpy(b->dir, (const void *)seq_dir, sizeof b->dir);
}
static void seq_copy_in(const seqbuf_t *b)       /* b, as the steps playing */
{
    memcpy((void *)seq_note, b->note, sizeof b->note);
    memcpy((void *)seq_vel, b->vel, sizeof b->vel);
    memcpy((void *)seq_glen, b->glen, sizeof b->glen);
    memcpy((void *)seq_cond, b->cond, sizeof b->cond);
    memcpy((void *)seq_rtg, b->rtg, sizeof b->rtg);
    memcpy((void *)seq_mut, b->mut, sizeof b->mut);
    memcpy((void *)seq_mcy, b->mcy, sizeof b->mcy);
    memcpy((void *)seq_mfx, b->mfx, sizeof b->mfx);
    memcpy((void *)seq_fcy, b->fcy, sizeof b->fcy);
    memcpy((void *)seq_lid, b->lid, sizeof b->lid);
    memcpy((void *)seq_lv, b->lv, sizeof b->lv);
    memcpy((void *)seq_len, b->len, sizeof b->len);
    memcpy((void *)seq_div, b->div, sizeof b->div);
    memcpy((void *)seq_dir, b->dir, sizeof b->dir);
}
/* a pattern waiting (projects.c's pat_switch, the sequencer playing): sb_next taken in as track seq_pend_t ends its
 * time through -- the steps it leaves into sb_old, every track from its start -- then seq_pend 2 (the main loop
 * finishes it: pat_poll) */
static seqbuf_t sb_next, sb_old;
static volatile uint8_t seq_pend, seq_pend_t;
static int seq_wraps(uint32_t t)                 /* track t's next step begins its time through again */
{
    uint32_t len = seq_len[t] ? seq_len[t] : NSTEP, pos = seq_pos[t];
    if (pos == 0xFFu || pos >= len)
        return 1;
    switch (seq_dir[t]) {
    case DIR_REV: return pos == 0u;
    case DIR_PING: return len < 2u || (seq_ppd[t] < 0 && pos == 1u);
    case DIR_RND: return seq_rnd_n[t] + 1u >= len;
    default: return pos + 1u >= len;
    }
}
static void seq_block(void)
{
    uint32_t t, i;
    if (!seq_run) {
        if (seq_was) {
            seq_release();
            for (t = 0; t < NTRACK; t++)
                memset(trk[t].tid, PL_NONE, sizeof trk[t].tid);
        }
        seq_was = 0;
        if (met_on && !met_was) {                /* (the metronome alone: a bar's first beat now) */
            met_acc = SEQ_16TH;
            met_step = 15;
        }
        met_was = met_on;
        if (met_on) {
            if (met_acc >= SEQ_16TH) {
                met_acc -= SEQ_16TH;
                met_step = (uint8_t)((met_step + 1u) & 15u);
                met_tick(met_step);
            }
            met_acc += CTL * (uint32_t)fx_bpm10;
        }
        return;
    }
    met_was = 0;
    if (!seq_was) {                              /* started: step 1 now */
        seq_was = 1;
        seq_acc = SEQ_16TH;
        for (t = 0; t < NTRACK; t++) {
            seq_pos[t] = 0xFF;
            seq_loop[t] = 0;
            seq_pre[t] = 0;
            seq_tacc[t] = SEQ_STEP8;
            seq_rnd_n[t] = 0;
            rt_left[t] = 0;
        }
        memset(seq_plays, 0, sizeof seq_plays);
        seq_gstep = 15;
        seq_gcount = 0xFFFFFFFFu;
        for (t = 0; t < NTRACK; t++)             /* (the loops from their first steps) */
            for (i = 0; i < NLOOP; i++)
                loops[t][i].anchor = 0;
    }
    for (t = 0; t < NTRACK; t++)                 /* the lengths run out */
        for (i = 0; i < SEQ_HOLD; i++)
            if (sq_note[t][i] && sq_left[t][i] != 0xFFFFu && !--sq_left[t][i]) {
                note_off(t, sq_note[t][i], SRC_SEQ);
                sq_note[t][i] = 0;
            }
    if (seq_acc >= SEQ_16TH) {
        seq_acc -= SEQ_16TH;
        seq_gstep = (uint8_t)((seq_gstep + 1u) & 15u);
        seq_gcount++;
        met_tick(seq_gstep);
        loop_step();
    }
    if (seq_pend == 1u && seq_tacc[seq_pend_t] >= SEQ_STEP8 && seq_wraps(seq_pend_t)) {   /* the pattern waiting, in */
        seq_copy_out(&sb_old);
        seq_copy_in(&sb_next);
        for (t = 0; t < NTRACK; t++) {
            seq_pos[t] = 0xFF;
            seq_loop[t] = 0;
            seq_pre[t] = 0;
            seq_rnd_n[t] = 0;
            rt_left[t] = 0;
            seq_tacc[t] = SEQ_STEP8;
        }
        memset(seq_plays, 0, sizeof seq_plays);
        seq_pend = 2;
    }
    for (t = 0; t < NTRACK; t++) {               /* each track on its own clock */
        if (seq_tacc[t] >= SEQ_STEP8) {
            uint32_t len = seq_len[t] ? seq_len[t] : NSTEP, p = seq_next(t, len);
            seq_tacc[t] -= SEQ_STEP8;
            rt_left[t] = 0;                      /* (a ratchet not done by the next step: cut) */
            seq_pos[t] = (uint8_t)p;
            seq_trig(t, p);
        } else if (rt_left[t] && seq_tacc[t] >= rt_next[t]) {   /* the ratchet's next hit */
            uint32_t r8 = DIV_R8[seq_div[t] % NDIV], rn = RTG_N[seq_rtg[t][rt_step[t]] % NRTG];
            rt_vel[t] = (uint8_t)maxi(1, rt_vel[t] - rt_vel[t] / 8);
            seq_sound(t, rt_note[t], rt_vel[t], (uint32_t)maxi(1, (int32_t)(64u / r8 / (2u * (rn > 1u ? rn : 2u)))),
                      seq_lid[t][rt_step[t]], seq_lv[t][rt_step[t]]);
            rt_next[t] += rt_int[t];
            rt_left[t]--;
        }
        seq_tacc[t] += CTL * (uint32_t)fx_bpm10 * DIV_R8[seq_div[t] % NDIV];
        seq_tacc_now[t] = seq_tacc[t];
    }
    loop_kick();
    fxq_block();
    seq_acc += CTL * (uint32_t)fx_bpm10;
    seq_acc_now = seq_acc;
}

/* ------------------------------------------------------------- the mix --- */
#define TRACK_GAIN 6144          /* Q12, x 1.5 */
#define LIM 19661                /* a track's ceiling, 0.6 (Q15) */
static void lim_block(int32_t *env, int32_t *g, int32_t *x)
{
    int32_t pk = 0, want, g0 = *g, dg;
    uint32_t i;
    for (i = 0; i < CTL; i++)
        pk = maxi(pk, absi(x[i]));
    if (pk > *env)
        *env = pk;
    else
        *env -= *env >> 8;
    want = *env > LIM ? (int32_t)((uint32_t)LIM * 32768u / (uint32_t)*env) : Q15;
    if (want > g0 && g0 < Q15)
        want = g0 + ((want - g0) >> 3);
    dg = (want - g0) / (int32_t)CTL;
    for (i = 0; i < CTL; i++) {
        g0 += dg;
        x[i] = mulq15(x[i], g0);
    }
    *g = want;
}

static void track_block(uint32_t t, int32_t *mixl, int32_t *mixr, int32_t *dlin, int32_t *rvin, int32_t *chin)
{
    track_t *y = &trk[t];
    int32_t buf[CTL], vb[CTL], pk = 0, sd1, sr1, sc1, pl1, pr1, dk_dg = 0, dk_next = Q16;
    int duck = 0;
    uint8_t mg[NM];
    uint32_t i, k, any = 0;
    const ck_t *c;
    if (!y->xfade && ck_pub[t] != ck_use[t]) {
        if (ck_buf[t][ck_pub[t]].mach != ck_buf[t][ck_use[t]].mach && track_sounding(t))
            y->xfade = FADE;                     /* another machine: out first */
        else if (ck_buf[t][ck_pub[t]].mach != ck_buf[t][ck_use[t]].mach)
            change_apply(t);
        else
            ck_use[t] = ck_pub[t];               /* the same machine: on, with the new values */
    }
    c = &ck_buf[t][ck_use[t]];
    memset(buf, 0, sizeof buf);
    for (k = 0; k < NVOICE; k++) {
        voice_t *v = &vox[k];
        vmem_t *m = &vmem[k];
        int alive;
        int32_t note16;
        uint32_t c0, cc;
        if (v->track != t || !v->sounding)
            continue;
        note16 = (int32_t)v->note << 16;
        c0 = COST_NOW();
        switch (v->eng) {
        case ENG_FMT:
            ft_render(&m->m.ft, &mck_of(k, c)->ft, note_inc(note16), note16, vb);
            break;
        case ENG_FMD:
            fd_render(&m->m.fd, &mck_of(k, c)->fd, note16, vb);
            break;
        case ENG_WAV:
            wt_render(&m->m.wt, &mck_of(k, c)->wt, note16, vb);
            break;
        default:
            sw_render(&m->m.sw, &mck_of(k, c)->sw, note16, vb);
            break;
        }
        alive = vc_block(&m->ch, vck_of(k, c), vb, note16, v->vel);
        cc = COST_NOW() - c0;
        v->cost = (uint16_t)(cc > 0xFFFFu ? 0xFFFFu : cc);
        eng_cost[v->eng] = (eng_cost[v->eng] * 7u + cc + 7u) / 8u;
        if (!alive) {
            v->sounding = 0;
            if (v->fade && v->pend)
                voice_take(k);
            else
                v->fade = v->fade_n = 0;
            continue;
        }
        any = 1;
        if (v->fade) {
            uint32_t fn = v->fade_n ? v->fade_n : FADE;
            int32_t g0 = (int32_t)(v->fade * (uint32_t)Q15 / fn), g1 = (int32_t)((v->fade - 1u) * (uint32_t)Q15 / fn);
            for (i = 0; i < CTL; i++)
                buf[i] += mulq15(vb[i], g0 + (((g1 - g0) * (int32_t)i) >> 5));
            if (--v->fade == 0) {
                if (v->pend)
                    voice_take(k);
                else
                    voice_silence(k);
            }
        } else
            for (i = 0; i < CTL; i++)
                buf[i] += vb[i];
    }
    if (y->xfade) {
        int32_t g0 = (int32_t)(y->xfade * (uint32_t)Q15 / FADE), g1 = (int32_t)((y->xfade - 1u) * (uint32_t)Q15 / FADE);
        for (i = 0; i < CTL; i++)
            buf[i] = mulq15(buf[i], g0 + (((g1 - g0) * (int32_t)i) >> 5));
        if (--y->xfade == 0 || !any) {
            y->xfade = 0;
            change_apply(t);
        }
    }
    {   /* the sends and the pan: the step's locks over the sound's and the track's */
        int32_t pan = -1;
        memcpy(mg, c->snd.mg, NM);
        sd1 = c->sd_del;
        sr1 = c->sd_rev;
        sc1 = c->sd_chr;
        for (i = 0; i < NPL; i++) {
            uint32_t id = y->tid[i];
            int32_t v = clampi(y->tv[i], 0, 127);
            if (id == PL_NONE)
                continue;
            if (id == PL_DEL)
                sd1 = send_q15(v);
            else if (id == PL_REV)
                sr1 = send_q15(v);
            else if (id == PL_CHR)
                sc1 = send_q15(v);
            else if (id == PL_PAN)
                pan = v;
            else if (id >= PL_MNG && id < PL_MNG + NM)
                mg[id - PL_MNG] = (uint8_t)v;
        }
        if (pan >= 0) {
            uint32_t ph = (uint32_t)pan * (0x40000000u / 127u);
            pl1 = sin_ph(ph + 0x40000000u);
            pr1 = sin_ph(ph);
        } else {
            pl1 = trk_pl[t];
            pr1 = trk_pr[t];
        }
    }
    if (!any) {
        if (dk_mask >> t & 1u) {                 /* (a source, silent: its level falls) */
            memset(buf, 0, sizeof buf);
            dk_feed(&dk[t], buf, dk_struck[t]);
            dk_struck[t] = 0;
        }
        y->meter -= y->meter >> 4;
        y->g = trk_gain[t];
        y->pl = pl1;
        y->pr = pr1;
        y->sd = sd1;
        y->sr = sr1;
        y->sc = sc1;
        return;
    }
    mangle_block(t, buf, mg);                    /* MANGLE */
    for (i = 0; i < CTL; i++) {                  /* (a DC blocker, ~10 Hz: FM with feedback can sit off centre) */
        int32_t xg = mulq12(buf[i], TRACK_GAIN);
        buf[i] = xg - lp8(&y->dc, xg, 47);
    }
    lim_block(&y->lim_env, &y->lim_g, buf);
    if (dk_mask >> t & 1u)                       /* (a source: its hits followed) */
        dk_feed(&dk[t], buf, dk_struck[t]);
    dk_struck[t] = 0;
    {   /* this track ducked under its source's hits: by SC LEVEL (0 .. 100: up to 45 dB, near a gate) as the source's
         * level stands against its hit, coming down at ATTACK, back as the source's level falls (RELEASE) */
        uint32_t src = sc_src[t];
        if (src && src - 1u != t && src <= NTRACK && sc_lvl[t]) {
            int32_t e = dk_e(&dk[src - 1u]), d = (sc_lvl[t] * 100 + 63) / 127, att = e * d / 10 * 9;   /* (dB,
                                                                                         * Q16: 0.45 dB a step of 100) */
            int32_t g1;
            dk_att[t] = att > dk_att[t] ? dk_att[t] + (int32_t)(((int64_t)(att - dk_att[t]) * cpc.ka) >> 16) : att;
            g1 = (int32_t)exp2_q16(-(dk_att[t] * 100 / 602));
            dk_dg = (g1 - dk_gp[t]) / (int32_t)CTL;
            duck = 1;
            dk_next = g1;
        } else {
            dk_att[t] = 0;
            dk_gp[t] = Q16;
        }
    }
    {   /* the level, the pan and the sends, gliding over the block */
        int32_t g1 = trk_gain[t];
        int32_t dg = (g1 - y->g) / CTL, dl = (pl1 - y->pl) / CTL, dr = (pr1 - y->pr) / CTL;
        int32_t dd = (sd1 - y->sd) / CTL, dv = (sr1 - y->sr) / CTL, dc = (sc1 - y->sc) / CTL;
        int32_t g = y->g, pl = y->pl, pr = y->pr, sd = y->sd, sr = y->sr, sc = y->sc;
        for (i = 0; i < CTL; i++) {
            int32_t x;
            g += dg;
            pl += dl;
            pr += dr;
            sd += dd;
            sr += dv;
            sc += dc;
            x = (int32_t)(((int64_t)buf[i] * g) >> 14);
            pk = maxi(pk, absi(x));
            if (duck)                            /* (ducked: under its source's hits) */
                x = (int32_t)(((int64_t)x * (dk_gp[t] + dk_dg * (int32_t)(i + 1u))) >> 16);
            mixl[i] += mulq15(x, pl);
            mixr[i] += mulq15(x, pr);
            dlin[i] += mulq15(x, sd);
            rvin[i] += mulq15(x, sr);
            chin[i] += mulq15(x, sc);
        }
        y->g = g1;
        y->pl = pl1;
        y->pr = pr1;
        y->sd = sd1;
        y->sr = sr1;
        y->sc = sc1;
    }
    y->meter = pk > y->meter ? pk : y->meter - (y->meter >> 5);
    if (duck)
        dk_gp[t] = dk_next;
}

/* above half scale, a soft knee into a ceiling at full scale */
static inline int32_t ceiling(int32_t x)
{
    int32_t a = absi(x);
    if (a <= Q15 / 2)
        return x;
    a = Q15 / 2 + (tanh_q15((a - Q15 / 2) * 2) >> 1);
    return x < 0 ? -a : a;
}

/* a released voice to let go, the budget short: the quietest (its AMP envelope lowest) -- a voice struck in the last
 * ~150 ms only if there is no other (a kick's gate ends early: its body is a release, and it must not go) */
#define SHED_YOUNG_BLOCKS 200u
static uint32_t tail_victim(void)
{
    uint32_t k, ok = NVOICE, pass;
    for (pass = 0; pass < 2u && ok == NVOICE; pass++)
        for (k = 0; k < NVOICE; k++) {
            const voice_t *v = &vox[k];
            if (!v->sounding || v->gate || v->fade || v->pend || (!pass && blk_clock - v->born < SHED_YOUNG_BLOCKS))
                continue;
            if (ok == NVOICE || vmem[k].ch.ae.lv < vmem[ok].ch.ae.lv)
                ok = k;
        }
    return ok;
}
/* overload: a released voice fades (tail_victim); late (2): if none, the oldest held note that has sounded 1.5 s */
#define SHED_HELD_BLOCKS 2067u
static int shed_voice(int held)
{
    uint32_t k, ok = held ? NVOICE : tail_victim();
    for (k = 0; held && k < NVOICE; k++) {
        const voice_t *v = &vox[k];
        if (!v->sounding || v->fade || !v->gate || blk_clock - v->born < SHED_HELD_BLOCKS)
            continue;
        if (ok == NVOICE || v->age < vox[ok].age)
            ok = k;
    }
    if (ok == NVOICE)
        return 0;
    vox[ok].fade = FADE;
    return 1;
}
static void budget_guard(void)
{
    uint32_t pred = blk_last + blk_new, k;
    blk_new = 0;
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding && vox[k].fade && !vox[k].pend)
            pred -= pred < vox[k].cost ? pred : vox[k].cost;
    while (pred > BUDGET) {
        uint32_t ok = tail_victim();
        if (ok == NVOICE)
            return;
        vox[ok].fade = SHED;
        vox[ok].fade_n = SHED;
        pred -= pred < vox[ok].cost ? pred : vox[ok].cost;
    }
}

/* one block: Q15 stereo, interleaved */
static void mix_block(int32_t *out)
{
    int32_t l_[CTL], r_[CTL], dlin[CTL], rvin[CTL], chin[CTL];
    uint32_t i, t, c0 = COST_NOW();
    events_take();
    {   /* the tracks some track ducks under (their hits followed: dk_feed) */
        uint32_t m = 0, q;
        for (q = 0; q < NTRACK; q++)
            if (sc_src[q] && sc_src[q] - 1u != q && sc_src[q] <= NTRACK && sc_lvl[q])
                m |= 1u << (sc_src[q] - 1u);
        dk_mask = (uint8_t)m;
    }
    seq_block();
    {   /* MANGLE's clock: where in the bar this block starts (the sequencer's, playing; else its own at the tempo) */
        uint32_t bpm = (uint32_t)clampi(fx_bpm10, 100, 3000);
        mg_dpos = bpm * 16777u / 6615u;          /* (steps Q24 a sample: 2^24 / (SEQ_16TH / bpm10)) */
        if (seq_run && seq_was && seq_gstep < 16u) {
            uint32_t a0 = seq_acc_now - CTL * bpm;
            mg_pos = ((uint32_t)seq_gstep << 24) + (a0 / 6615u) * 16777u + (a0 % 6615u) * 16777u / 6615u;
            mg_pos &= (16u << 24) - 1u;
        } else
            mg_pos = (mg_pos + CTL * mg_dpos) & ((16u << 24) - 1u);
    }
    budget_guard();
    if (shed_req) {
        if (!shed_voice(0) && shed_req > 1u)
            shed_voice(1);
        shed_req = 0;
    }
    for (i = 0; i < CTL; i++)
        l_[i] = r_[i] = dlin[i] = rvin[i] = chin[i] = 0;
    for (t = 0; t < NTRACK; t++)
        track_block(t, l_, r_, dlin, rvin, chin);
    ch_block(chin, l_, r_, dlin, rvin);
    dl_block(dlin, l_, r_, rvin);
    rv_block(rvin, l_, r_);
    cp_block(l_, r_);                            /* the compressor: the master's, the returns in it (a pad's tails duck) */
    met_block(l_, r_);
    for (i = 0; i < CTL; i++) {
        out[2u * i] = ceiling(mulq12(l_[i], master_q12));
        out[2u * i + 1u] = ceiling(mulq12(r_[i], master_q12));
    }
    blk_last = COST_NOW() - c0;
    blk_clock++;
}

/* ------------------------------------------------------------ main loop side --- */
static void tracks_init(void)
{
    uint32_t t;
    memset(vox, 0, sizeof vox);
    memset(trk, 0, sizeof trk);
    eng_cost[ENG_FMT] = BLK_TICKS * 5u / 100u;   /* (first guesses, until measured) */
    eng_cost[ENG_FMD] = BLK_TICKS * 5u / 100u;
    eng_cost[ENG_WAV] = BLK_TICKS * 5u / 100u;
    eng_cost[ENG_SWM] = BLK_TICKS * 7u / 100u;
    blk_last = blk_new = 0;
    for (t = 0; t < NTRACK; t++) {
        trk[t].lim_g = Q15;
        memset(trk[t].tid, PL_NONE, sizeof trk[t].tid);
    }
    memset((void *)seq_lid, PL_NONE, sizeof seq_lid);
    wt_init();
    fx_init();
    mangle_init();
}
/* the sound s cooked for track t and published; 0 if the ISR has not yet taken the last one (try again) */
static int track_publish(uint32_t t, const sound_t *s)
{
    uint32_t k;
    if (ck_pub[t] != ck_use[t])
        return 0;
    k = ck_use[t] ^ 1u;
    ck_cook(&ck_buf[t][k], s);
    RING_PUBLISH();
    ck_pub[t] = (uint8_t)k;
    return 1;
}
