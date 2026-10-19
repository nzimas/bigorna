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
typedef char gr_fits[GR_TRACKS == NTRACK ? 1 : -1];
#define NVOICE 12
static const char *const ENG_NAME[NENG] = {"FM TONE", "FM DRUM", "WAVETONE", "SWARMER"};
static const char *const FLT_NAME[NFLT] = {"MULTI-MODE", "LOWPASS 4", "LEGACY", "COMB-", "COMB+", "EQUALIZER"};

typedef struct {
    uint8_t mach;
    vck_t ch;
    union { ftck_t ft; fdck_t fd; wtck_t wt; swck_t sw; } m;
    int32_t sd_del, sd_rev, sd_chr;      /* the sends, Q15 */
    uint8_t fmach, flt[NF], amp[NA], fx[NX];   /* (the sound's own: a voice's locks cooked from them) */
    int8_t grain;                        /* GRAIN (grain.c), -63 .. 63 */
} ck_t;
static ck_t ck_buf[NTRACK][2];
static volatile uint8_t ck_pub[NTRACK], ck_use[NTRACK];

static void ck_cook(ck_t *c, const sound_t *s)
{
    memset(c, 0, sizeof *c);
    c->mach = s->mach;
    vck_cook(&c->ch, s);
    switch (s->mach) {
    case ENG_FMT:
        ft_cook(&c->m.ft, s);
        break;
    case ENG_FMD:
        fd_cook(&c->m.fd, s);
        break;
    case ENG_WAV:
        wt_cook(&c->m.wt, s);
        break;
    default:
        sw_cook(&c->m.sw, s);
        break;
    }
    c->sd_del = s->fx[X_DEL] * s->fx[X_DEL] * Q15 / 16129;   /* (the sends: v^2, as the original) */
    c->sd_rev = s->fx[X_REV] * s->fx[X_REV] * Q15 / 16129;
    c->sd_chr = s->fx[X_CHR] * s->fx[X_CHR] * Q15 / 16129;
    c->fmach = s->fmach;
    c->grain = (int8_t)clampi((int8_t)s->fx[X_GRAIN], -GR_MAX, GR_MAX);
    memcpy(c->flt, s->flt, NF);
    memcpy(c->amp, s->amp, NA);
    memcpy(c->fx, s->fx, NX);
}
static int32_t send_q15(int32_t v) { return v * v * Q15 / 16129; }

/* a step's parameter locks (the sequencer's): the voice's own, cooked as its note starts (FREQ .. SRR), then the
 * track's, held from the step until the track's next step plays (the sends, the pan). LK_NONE: the sound's */
enum { LK_FREQ, LK_RESO, LK_FENV, LK_ADEC, LK_VOL, LK_OD, LK_BR, LK_SRR, NVLK, LK_DEL = NVLK, LK_REV, LK_CHR, LK_PAN, NLK };
#define LK_NONE 0xFFu

/* a voice's own memory: the chain (its comb line) and the machine's state */
typedef struct {
    vchain_t ch;
    union { ftv_t ft; fdv_t fd; wtv_t wt; swv_t sw; } m;
    vck_t lck;                   /* the chain cooked with its note's locks (voice_t own) */
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
    uint8_t lk[NVLK];            /* its note's locks (the pending note's, while it fades for it) */
    uint8_t own;                 /* .. cooked into its vmem's lck: the chain plays that */
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
    uint8_t tlk[NLK - NVLK];     /* the sends' and the pan's locks, from the step last played (LK_NONE: none) */
} track_t;
static track_t trk[NTRACK];
/* each track's level (Q14) and equal-power pan (Q15), set by the main loop */
static volatile int32_t trk_gain[NTRACK], trk_pl[NTRACK], trk_pr[NTRACK];

/* the budget (as TONITOR's): each machine's voice cost measured as it renders, so a start that would overrun the
 * block lets the oldest release tails go first, quickly, rather than the audio arriving late */
#define COST_NOW() fm1_ticks()
#define BLK_TICKS ((uint32_t)CTL * 1000000u / FS * FM1_TICKS_PER_US)
#define BUDGET (BLK_TICKS * 82u / 100u)
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
/* the voice's chain, cooked with its note's locks (none: the track's own) */
static void voice_locks(uint32_t k, const ck_t *c)
{
    static const uint8_t F[] = {F_FREQ, F_RESO, F_ENV};
    voice_t *v = &vox[k];
    sound_t s;
    uint32_t j;
    for (j = 0; j < NVLK && v->lk[j] == LK_NONE; j++)
        ;
    v->own = (uint8_t)(j < NVLK);
    if (!v->own)
        return;
    s.fmach = c->fmach;
    memcpy(s.flt, c->flt, NF);
    memcpy(s.amp, c->amp, NA);
    memcpy(s.fx, c->fx, NX);
    for (j = 0; j < 3u; j++)
        if (v->lk[j] != LK_NONE)
            s.flt[F[j]] = v->lk[j];
    if (v->lk[LK_ADEC] != LK_NONE)
        s.amp[A_DEC] = s.amp[A_REL] = v->lk[LK_ADEC];
    if (v->lk[LK_VOL] != LK_NONE)
        s.amp[A_VOL] = v->lk[LK_VOL];
    if (v->lk[LK_OD] != LK_NONE)
        s.fx[X_OD] = v->lk[LK_OD];
    if (v->lk[LK_BR] != LK_NONE)
        s.fx[X_BR] = v->lk[LK_BR];
    if (v->lk[LK_SRR] != LK_NONE)
        s.fx[X_SRR] = v->lk[LK_SRR];
    vck_cook(&vmem[k].lck, &s);
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
        ft_on(&m->m.ft, &c->m.ft, fresh);
        break;
    case ENG_FMD:
        fd_on(&m->m.fd, &c->m.fd, fresh, seed);
        break;
    case ENG_WAV:
        wt_on(&m->m.wt, &c->m.wt, fresh, seed);
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
        wt_off(&m->m.wt, &c->m.wt);
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
static const volatile uint8_t *note_lk;          /* the note's locks (the sequencer's step), 0 none */
static void note_on(uint32_t t, uint32_t note, uint32_t src)
{
    track_t *y = &trk[t];
    uint32_t best = voice_pick(t, y->xfade != 0);
    voice_t *v;
    if (best == NVOICE)
        return;
    v = &vox[best];
    v->note = (uint8_t)note;
    v->gate = 1;
    v->src = (uint8_t)src;
    v->vel = note_vel;
    {
        uint32_t j;
        for (j = 0; j < NVLK; j++)
            v->lk[j] = note_lk ? note_lk[j] : (uint8_t)LK_NONE;
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
static volatile uint8_t seq_lock[NTRACK][NSTEP][NLK];   /* the steps' locks (LK_NONE: none) */
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
static void seq_sound(uint32_t t, uint32_t n, uint32_t vel, uint32_t len8, const volatile uint8_t *lk)
{
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
    note_lk = lk;
    note_on(t, n, SRC_SEQ);
    note_lk = 0;
    note_vel = 127;
    sq_note[t][k] = (uint8_t)n;
    if (!len8)
        sq_left[t][k] = 0xFFFFu;
    else {   /* (blocks: eighths x the blocks of an eighth, x 16 for its fraction) */
        uint32_t bl = (len8 < 4096u ? len8 : 4096u) * (SEQ_16TH / 8u * 16u / (CTL * (uint32_t)clampi(fx_bpm10, 100, 3000))) / 16u;
        sq_left[t][k] = (uint16_t)(bl ? (bl < 0xFFFEu ? bl : 0xFFFEu) : 1u);
    }
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
    seq_sound(t, n, seq_vel[t][p], g >= GLEN_INF ? 0u : GLEN_8TH[g], seq_lock[t][p]);
    for (i = 0; i < NLK - NVLK; i++)             /* (the track's: until its next step plays) */
        trk[t].tlk[i] = seq_lock[t][p][NVLK + i];
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
                    seq_sound(t, p->ev[p->cur].note, p->ev[p->cur].vel, p->ev[p->cur].len8, 0);
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
                    seq_sound(t, p->ev[p->cur].note, p->ev[p->cur].vel, p->ev[p->cur].len8, 0);
        }
}
static void seq_block(void)
{
    uint32_t t, i;
    if (!seq_run) {
        if (seq_was) {
            seq_release();
            for (t = 0; t < NTRACK; t++)
                memset(trk[t].tlk, LK_NONE, sizeof trk[t].tlk);
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
        }
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
        for (t = 0; t < NTRACK; t++) {
            uint32_t len = seq_len[t] ? seq_len[t] : NSTEP, p = seq_pos[t] + 1u;
            if (p >= len) {
                p = 0;
                if (seq_pos[t] != 0xFFu)
                    seq_loop[t]++;
            }
            seq_pos[t] = (uint8_t)p;
            seq_trig(t, p);
        }
        loop_step();
    }
    loop_kick();
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
    int32_t buf[CTL], vb[CTL], pk = 0, sd1, sr1, sc1, pl1, pr1;
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
            ft_render(&m->m.ft, &c->m.ft, note_inc(note16), note16, vb);
            break;
        case ENG_FMD:
            fd_render(&m->m.fd, &c->m.fd, note16, vb);
            break;
        case ENG_WAV:
            wt_render(&m->m.wt, &c->m.wt, note16, vb);
            break;
        default:
            sw_render(&m->m.sw, &c->m.sw, note16, vb);
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
        sd1 = y->tlk[0] != LK_NONE ? send_q15(y->tlk[0]) : c->sd_del;
        sr1 = y->tlk[1] != LK_NONE ? send_q15(y->tlk[1]) : c->sd_rev;
        sc1 = y->tlk[2] != LK_NONE ? send_q15(y->tlk[2]) : c->sd_chr;
        if (y->tlk[3] != LK_NONE) {
            uint32_t ph = y->tlk[3] * (0x40000000u / 127u);
            pl1 = sin_ph(ph + 0x40000000u);
            pr1 = sin_ph(ph);
        } else {
            pl1 = trk_pl[t];
            pr1 = trk_pr[t];
        }
    }
    if (!any && !(gran[t].tail && (c->grain || gran[t].amt))) {   /* (grains ring on a while after the voices) */
        y->meter -= y->meter >> 4;
        y->g = trk_gain[t];
        y->pl = pl1;
        y->pr = pr1;
        y->sd = sd1;
        y->sr = sr1;
        y->sc = sc1;
        return;
    }
    gran_block(t, buf, c->grain, (int)any);      /* GRAIN */
    for (i = 0; i < CTL; i++) {                  /* (a DC blocker, ~10 Hz: FM with feedback can sit off centre) */
        int32_t xg = mulq12(buf[i], TRACK_GAIN);
        buf[i] = xg - lp8(&y->dc, xg, 47);
    }
    lim_block(&y->lim_env, &y->lim_g, buf);
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

/* overload: the oldest released voice fades; late (2): if none, the oldest held note that has sounded 1.5 s */
#define SHED_HELD_BLOCKS 2067u
static int shed_voice(int held)
{
    uint32_t k, ok = NVOICE;
    for (k = 0; k < NVOICE; k++) {
        const voice_t *v = &vox[k];
        if (!v->sounding || v->fade || (held ? !v->gate || blk_clock - v->born < SHED_HELD_BLOCKS : v->gate))
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
        uint32_t ok = NVOICE;
        for (k = 0; k < NVOICE; k++) {
            const voice_t *v = &vox[k];
            if (v->sounding && !v->gate && !v->fade && !v->pend && (ok == NVOICE || v->age < vox[ok].age))
                ok = k;
        }
        if (ok == NVOICE)
            return;
        vox[ok].fade = QUICK;
        vox[ok].fade_n = QUICK;
        pred -= pred < vox[ok].cost ? pred : vox[ok].cost;
    }
}

/* one block: Q15 stereo, interleaved */
static void mix_block(int32_t *out)
{
    int32_t l_[CTL], r_[CTL], dlin[CTL], rvin[CTL], chin[CTL];
    uint32_t i, t, c0 = COST_NOW();
    events_take();
    seq_block();
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
        memset(trk[t].tlk, LK_NONE, sizeof trk[t].tlk);
    }
    memset((void *)seq_lock, LK_NONE, sizeof seq_lock);
    wt_init();
    fx_init();
    gran_init();
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
