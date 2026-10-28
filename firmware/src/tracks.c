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
typedef char midi_fits[MIDI_TRACKS == NTRACK ? 1 : -1];   /* (midi.c's) */
static const char *const ENG_NAME[NENG] = {"4-OP", "DRUM", "WAVY", "HIVE"};
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
#if TELEMETRY
static uint32_t tl_cook[8];                      /* (a TELEMETRY build's: ticks a cook takes, each machine -- the
                                                  * chain's (vck_cook), the machine's (mck_cook); main.c measures) */
#endif
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
AINL int32_t send_q15(int32_t v) { return v * v * Q15 / 16129; }

/* each track's sidechain (the COMP page's second half, the chosen track's: SC SOURCE, SC LEVEL): the track ducked
 * under another's hits (fx.c's dk_*), by SC LEVEL, 0 .. 100 (raw 0 .. 127): up to 45 dB on the hit, near a gate.
 * SC SOURCE 0: off */
static volatile uint8_t sc_src[NTRACK], sc_lvl[NTRACK] = {64, 64, 64, 64, 64, 64};
static dk_t dk[NTRACK];                          /* (the ISR's: each source's hits) */
static int32_t dk_att[NTRACK];                   /* (the duck now, dB Q16) */
static int32_t dk_gp[NTRACK] = {Q16, Q16, Q16, Q16, Q16, Q16};   /* (its gain as the last block ended) */
static uint8_t dk_mask;                          /* (the tracks some track ducks under) */
static uint8_t dk_struck[NTRACK];                /* (a note began on the track this block: a hit) */
/* a ducked track's effects ducked with it: each return (chorus, delay, reverb) brought down by the tracks in it, as
 * much as each is in it -- what each sent, followed over ~0.7 s (its tail) */
static uint32_t dk_sent[NTRACK][3];              /* (this block's sends, |x| summed: chorus, delay, reverb) */
static uint32_t dk_in[NTRACK][3];                /* (followed) */
static int32_t dk_rg[3] = {Q16, Q16, Q16};       /* (each return's gain as the last block ended) */
static uint8_t dk_on;                            /* (some track ducked) */

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
static void lfo_apply(sound_t *s, const int32_t *mod);   /* (params.c: the LFOs' shares onto their destinations) */
static volatile uint8_t lfo_trig[NTRACK];       /* (a note began on the track: its LFOs' trigger; build 50) */
static uint32_t lfo_flips[NTRACK];               /* (each track's cooked sound taken anew: its LFOs' copy is old) */

/* a voice's own memory: the chain (its comb line) and the machine's state */
typedef struct {
    vchain_t ch;
    union { ftv_t ft; fdv_t fd; wtv_t wt; swv_t sw; } m;
    vck_t lck;                   /* the chain and .. */
    mck_t lm;                    /* .. the machine, cooked with its note's locks (voice_t own) */
} __attribute__((aligned(32))) vmem_t;   /* (each voice's on cache lines of its own: two cores render them) */
static vmem_t vmem[NVOICE] POOL;

#define FADE 12u                 /* blocks of a fade (~9 ms) */
#define QUICK 3u                 /* blocks of the budget's fade (~2 ms) */
enum { SRC_KEY, SRC_SEQ, SRC_ARP };   /* who plays a note: the keys (and MIDI), the sequencer, the arpeggiator */
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
    int32_t mg;                  /* the mute's gain, Q15 (gliding: ~9 ms, no click; build 35) */
    uint8_t tid[NPL];            /* the track's locks from the step last played (the sends, MANGLE, the pan): ids .. */
    int16_t tv[NPL];             /* .. and values */
} track_t;
static track_t trk[NTRACK];
/* each track's level (Q14) and equal-power pan (Q15), set by the main loop */
static volatile int32_t trk_gain[NTRACK], trk_pl[NTRACK], trk_pr[NTRACK];
static volatile uint8_t trk_mute[NTRACK];        /* each track muted (the MUTE mode's, the main loop's; build 35) */
static const int32_t zero_blk[CTL];

/* the budget (as TONITOR's): each machine's voice cost measured as it renders, so a start that would overrun the
 * block lets the quietest release tails go first (tail_victim), over ~6 ms, rather than the audio arriving late */
#define COST_NOW() FAR(fm1_ticks)()   /* (from the RAM code too: through a pointer) */
#define BLK_TICKS ((uint32_t)CTL * 1000000u / FS * FM1_TICKS_PER_US)
#define BUDGET (BLK_TICKS * 90u / 100u)   /* (82 % until build 28: tails went that the time had room for) */
#define SHED 16u                 /* blocks of the budget's fade (~12 ms: a loud tail without a click; build 34) */
static uint32_t eng_cost[NENG];
static uint32_t blk_last, blk_new;
#if !TELEMETRY
#define TL_FX(k, call) { call; }
#endif
#if TELEMETRY
static uint32_t tl_trk[NTRACK], tl_fx, tl_all, tl_n, tl_max, tl_shed;   /* (main.c's report: ticks, summed) */
#define TL_FX(k, call) { uint32_t c4 = COST_NOW(); call; tl_fxk[k] += COST_NOW() - c4; }
static uint32_t tl_mach[NENG], tl_chain[NENG], tl_fxk[4], tl_c1, tl_evmax, tl_seqmax, tl_par, tl_parmax, tl_steal, tl_lost, tl_stealmax;   /* (a machine's render, its chain; CH DL RV CP) */
#endif
static uint32_t blk_clock;
static volatile uint8_t shed_req;
static volatile uint8_t load_pct;
static int32_t master_q12 = 2048;
#if defined(FELUCCA_UAC) && FELUCCA_UAC
static int32_t usb_blk[2 * CTL];                 /* (the block for USB audio: the master before the MASTER knob, at
                                                  * unity -- a recording does not follow the headphones' level) */
#endif

/* --------------------------------------------------------------- events --- */
enum { EV_ON, EV_OFF, EV_STOP, EV_ARP_ON, EV_ARP_OFF, EV_ARP_CLR };   /* (EV_ARP_*: build 52) */
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
AINL const vck_t *vck_of(uint32_t k, const ck_t *c) { return vox[k].own ? &vmem[k].lck : &c->ch; }
AINL const mck_t *mck_of(uint32_t k, const ck_t *c) { return vox[k].own ? &vmem[k].lm : &c->m; }
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
AINL void voice_silence(uint32_t k)
{
    vox[k].sounding = 0;
    vox[k].fade = vox[k].fade_n = 0;
}
/* a voice for a note on track t: a free one; else the quietest releasing one (any track's; one already fading out
 * first: build 34 -- the oldest was often a loud pad's tail, faded in 9 ms: a click); else the track's own oldest
 * held; else the oldest held of all. own: only free ones or the track's own (it is fading for a change) */
static uint32_t voice_pick(uint32_t t, int own)
{
    uint32_t k, best = NVOICE, pass;
    for (k = 0; k < NVOICE; k++)
        if (!vox[k].sounding && !vox[k].gate && !vox[k].pend)
            return k;
    for (k = 0; k < NVOICE; k++) {               /* (the releasing ones: the quietest) */
        const voice_t *v = &vox[k];
        int32_t lv = v->fade ? -1 : vmem[k].ch.ae.lv, lb;
        if (v->pend || v->gate || (own && vox_owner(v) != t))
            continue;
        lb = best == NVOICE ? 0 : vox[best].fade ? -1 : vmem[best].ch.ae.lv;
        if (best == NVOICE || lv < lb || (lv == lb && v->age < vox[best].age))
            best = k;
    }
    for (pass = 1; pass < 3u && best == NVOICE; pass++)
        for (k = 0; k < NVOICE; k++) {
            const voice_t *v = &vox[k];
            uint32_t o = vox_owner(v);
            if (v->pend || (own && o != t) || (pass == 1u ? o != t : 0))
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
    midi_note_out(t, note, note_vel, trk_mute[t]);   /* (out on the track's channel, SEND NOTES: midi.c) */
    lfo_trig[t] = 1;                             /* (the track's LFOs: TRIG, HOLD, ONE, HALF and the FADE start again) */
    if (!y->xfade && ck_buf[t][ck_use[t]].mach == ENG_FMD) {   /* an DRUM track chokes, as a drum machine's: its
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
    if (best == NVOICE) {
#if TELEMETRY
        tl_lost++;                               /* (no voice at all: the note not played) */
#endif
        return;
    }
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
#if TELEMETRY
        tl_steal++;                              /* (a voice still sounding taken: faded over FADE) */
        if ((uint32_t)(vmem[best].ch.ae.lv >> 9) > tl_stealmax)
            tl_stealmax = (uint32_t)(vmem[best].ch.ae.lv >> 9);   /* (its envelope's level, Q15: the loudest taken) */
#endif
        if (!v->fade)
            v->fade = FADE;
        v->pend = 1;
        v->pend_track = (uint8_t)t;
    } else
        voice_on(t, best, 1);
}
static void note_off(uint32_t t, uint32_t note, uint32_t src)
{
    uint32_t k, o = NVOICE;
    midi_note_out(t, note, 0, 0);
    for (k = 0; k < NVOICE; k++) {               /* (the oldest held: two keys can hold the same note -- two chords
                                                  * with a note in common, legato -- and the first let go is the first
                                                  * struck, not the new chord's; build 34) */
        const voice_t *v = &vox[k];
        if (v->gate && vox_owner(v) == t && v->note == note && v->src == src && (o == NVOICE || v->age < vox[o].age))
            o = k;
    }
    if (o < NVOICE) {
        vox[o].gate = 0;
        if (!vox[o].pend && !trk[t].xfade)
            voice_off(t, o);
    }
}
/* a voice's fade over: the note it waited for, on the track it goes to */
AINL void voice_take(uint32_t k)
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
    lfo_flips[t]++;
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

/* ---------------------------------------------------------- the arpeggiator --- */
/* (build 52) each track's, on the 5th black key (ui.c): the keys held go to it, not to the voices; it plays them in
 * turn on the tempo's grid -- on the sequencer's while it plays, on its own clock stopped -- and what it plays is
 * recorded as the keys are (arp_rec: the main loop's, ui.c). Its settings, the main loop's, are read here as they are */
enum { AR_MODE, AR_RATE, AR_RNG, AR_GATE, AR_LATCH, AR_DENS, AR_ORND, AR_BASS, NARP };
enum { AM_UP, AM_DOWN, AM_UPDN, AM_DNUP, AM_ORDER, AM_RND, AM_CONV, AM_DIV, AM_PINKY, AM_THUMB, NAM };
#define NAR_RATE 6u
static const uint8_t AR_R6[NAR_RATE] = {3, 4, 6, 8, 12, 24};   /* (1/32, 1/16T, 1/16, 1/8T, 1/8, 1/4: sixths of a 16th) */
#define NAR_RNG 4u
#define NAR_BASS 8u                              /* (OFF, then every 2 .. 8 notes) */
#define ARP_N 12u
static volatile uint8_t arp_on[NTRACK], arp_p[NTRACK][NARP];
typedef struct {
    uint8_t n, note[ARP_N], vel[ARP_N];          /* the notes held, in the order played */
    uint8_t down;                                /* keys down now (LATCH: the notes kept with none) */
    uint8_t fresh;                               /* the first note of a new chord: at once */
    uint8_t snd;                                 /* the note sounding (0 none) .. */
    uint32_t left;                               /* .. its blocks left */
    uint32_t c, acc, since;                      /* notes in turn; stopped: its clock; blocks since a note */
    int32_t k;                                   /* playing: the grid's place it played last */
} arp_t;
static arp_t arps[NTRACK];
static uint32_t arp_rnd = 0x9E3779B9u;
typedef struct { uint8_t t, note, vel, pos; uint16_t len8; uint32_t acc, q; } arp_rec_t;   /* (a note played, for the
                                                  * main loop to record: where the sequencer was) */
#define ARP_RQ 16u
static arp_rec_t arp_rq[ARP_RQ];
static volatile uint32_t arp_rw, arp_rr;
static const uint8_t AR_DEF[NARP] = {AM_UP, 2, 0, 64, 0, 127, 0, 0};   /* (UP, 1/16, 1 OCT, GATE 50, LATCH OFF,
                                                  * DENSITY 100, OCT RND 0, BASS OFF) */
static void arp_defaults(uint32_t t)
{
    uint32_t j;
    for (j = 0; j < NARP; j++)
        arp_p[t][j] = AR_DEF[j];
}
static void arp_quiet(uint32_t t)                /* (its note let go) */
{
    arp_t *a = &arps[t];
    if (a->snd)
        note_off(t, a->snd, SRC_ARP);
    a->snd = 0;
    a->left = 0;
}
static void arp_key(uint32_t t, uint32_t note, uint32_t vel, int on)   /* (the audio's: an event) */
{
    arp_t *a = &arps[t];
    uint32_t i, j;
    if (!on) {
        if (a->down)
            a->down--;
        if (arp_p[t][AR_LATCH])
            return;
        for (i = 0; i < a->n && a->note[i] != note; i++)
            ;
        if (i < a->n) {
            for (j = i; j + 1u < a->n; j++) {
                a->note[j] = a->note[j + 1u];
                a->vel[j] = a->vel[j + 1u];
            }
            a->n--;
        }
        if (!a->n)
            arp_quiet(t);
        return;
    }
    if (!a->down && arp_p[t][AR_LATCH])          /* (LATCH: a new chord after all were let go, in place of the last) */
        a->n = 0;
    a->down++;
    for (i = 0; i < a->n && a->note[i] != note; i++)
        ;
    if (i < a->n || a->n >= ARP_N)
        return;
    if (!a->n) {
        a->fresh = 1;
        a->c = 0;
    }
    a->note[a->n] = (uint8_t)note;
    a->vel[a->n] = (uint8_t)(vel ? vel : 127u);
    a->n++;
}
static void arp_clear(uint32_t t)
{
    arps[t].n = arps[t].down = 0;
    arp_quiet(t);
}
/* (the main loop's: a session's) track t's arpeggiator on or off and its settings, each in its range; its notes let go */
static void arp_set(uint32_t t, uint32_t on, const uint8_t *p)
{
    static const uint8_t N[NARP] = {NAM, NAR_RATE, NAR_RNG, 128, 2, 128, 128, NAR_BASS};
    uint32_t j;
    arp_on[t] = (uint8_t)(on != 0);
    for (j = 0; j < NARP; j++)
        arp_p[t][j] = (uint8_t)(p[j] < N[j] ? p[j] : 0u);
    ev_post(EV_ARP_CLR, t, 0, 0);
}

static void trk_hush(uint32_t t)                 /* (the ISR's: track t's notes let go, faded out at once) */
{
    uint32_t k;
    midi_track_off(t);
    for (k = 0; k < NVOICE; k++) {
        voice_t *v = &vox[k];
        if (vox_owner(v) == t) {
            v->gate = 0;
            if (v->pend && v->pend_track == t)
                v->pend = 0;
        }
        if (v->track == t && v->sounding && !v->fade)
            v->fade = FADE;
    }
}
static void events_take(void)
{
    while (ev_r != ev_w) {
        ev_t e = evq[ev_r % EVQ];
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
        case EV_ARP_ON:
        case EV_ARP_OFF:
            arp_key(e.track, e.note, e.val, e.type == EV_ARP_ON);
            break;
        case EV_ARP_CLR:
            arp_clear(e.track);
            break;
        case EV_STOP:
            trk_hush(e.track);
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
typedef char seq_midi16[SEQ_16TH == MIDI_16TH ? 1 : -1];   /* (midi.c's) */
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
static uint8_t rt_left[NTRACK], rt_note[NTRACK], rt_vel[NTRACK], rt_step[NTRACK], rt_chf[NTRACK], rt_n[NTRACK];
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
static volatile uint8_t seq_chf[NTRACK][NSTEP];   /* a step's chord: its form + 1 (0: the note alone; build 33) */
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
/* the chord forms (CHORD mode, after TONITOR's): three notes as steps of the scale from the root, so in the scale
 * and of the quality it gives; in CHROMATIC, as semitones (a major triad, its inversions ..) */
#define CHF_N 7u
static const int8_t CH_STEP[CHF_N][3] = {{0, 2, 4}, {2, 4, 7}, {-3, 0, 2}, {0, 1, 4}, {0, 3, 4}, {0, 4, 9}, {0, 3, 6}};
static const int8_t CH_SEMI[CHF_N][3] = {{0, 4, 7}, {4, 7, 12}, {-5, 0, 4}, {0, 2, 7}, {0, 5, 7}, {0, 7, 16}, {0, 5, 10}};
static const char *const CH_NAME[CHF_N] = {"TRIAD", "INV 1", "INV 2", "SUS2", "SUS4", "OPEN", "FOURTHS"};
static const char *const CH_ABBR[CHF_N] = {"TRI", "IN1", "IN2", "SU2", "SU4", "OPN", "4TH"};
static void chord_of(int32_t root, uint32_t form, int32_t *n)   /* the chord's three notes */
{
    uint32_t i;
    form %= CHF_N;
    for (i = 0; i < 3u; i++)
        n[i] = clampi((seq_smask & 0xFFFu) == 0xFFFu ? root + CH_SEMI[form][i] : sc_move(root, CH_STEP[form][i]), 12,
                      120);
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
/* a step's note, or its chord (f1: form + 1) */
static void seq_soundc(uint32_t t, uint32_t n, uint32_t vel, uint32_t len8, const volatile uint8_t *lid,
                       const volatile int16_t *lv, uint32_t f1)
{
    int32_t c[3];
    uint32_t i;
    if (!f1 || ck_buf[t][ck_use[t]].mach == ENG_FMD) {   /* (DRUM: the root alone) */
        seq_sound(t, n, vel, len8, lid, lv);
        return;
    }
    chord_of((int32_t)n, f1 - 1u, c);
    for (i = 0; i < 3u; i++)
        seq_sound(t, (uint32_t)c[i], vel, len8, lid, lv);
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
        if (!seq_ppd[t])                         /* (PING chosen while playing, never started in it: on up, build 71 --
                                                  * at 0 it stood on its step for good) */
            seq_ppd[t] = 1;
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
/* BREAK (build 37; after MOTOR:CYCLES' brk.c): while the 3rd black key is held, the tracks in brk_mask play other
 * steps than the ones they are on -- a beat repeated, dragged to half time, rushed to double, its steps played
 * backwards, the bar's beats shuffled, or a roll -- with ratchets that speed up the longer it is held, flams, octave
 * leaps and a gap before the bar's end. Nothing is written: each track's own position goes on underneath, so let go,
 * it carries on where it would have been. The loops are not broken up: they play on as recorded (from build 45, as
 * before 41).
 * It stays on the pattern's beat (build 45): a track goes into the break on its next beat (its next half bar for
 * REPEAT 8), playing as written until then; every way keeps what is on a beat on a beat -- windows of 1, 2, 4 or 8
 * steps, REVERSE keeping each window's first step, CHOP moving whole beats. One plan a press (build 53: until then
 * another every two bars held), never the same way twice running. The kick keeps the bar's first step and takes no leaps, no
 * gap and few ratchets; a drone is left alone */
enum { BR_REPEAT, BR_HALF, BR_DOUBLE, BR_REVERSE, BR_CHOP, BR_ROLL, NBR };
static const char *const BR_NAME[NBR] = {"REPEAT", "HALF TIME", "DOUBLE", "REVERSE", "CHOP", "ROLL"};
enum { BL_RAT = 1, BL_OCT = 2, BL_FLAM = 4, BL_GAP = 8, BL_ACC = 16 };
static volatile uint8_t brk_mask;                /* (the main loop's: the tracks under the break; 0 none) */
static volatile uint8_t brk_kick, brk_drum, brk_keep;   /* (.. the kicks, the drums, the drones: as the press found them) */
static volatile uint32_t brk_seed;               /* (.. a new one each press) */
static volatile uint8_t brk_shown = 0xFF;        /* (the ISR's: the way playing, BR_*, for the screen; w after) */
static volatile uint8_t brk_shown_w;
static uint32_t brk_seen, brk_r, brk_n16, brk_since;
static uint8_t brk_rec, brk_prev = 0xFF, brk_w, brk_chop[4], brk_gap, brk_lay, brk_fresh;
static uint8_t brk_anchor[NTRACK];
static uint8_t brk_rt[NTRACK], brk_fl[NTRACK];   /* (a step's: its ratchet, flam, velocity %) */
static uint8_t brk_vp[NTRACK] = {100, 100, 100, 100, 100, 100};
static int8_t brk_oct[NTRACK];
static uint8_t brk_last;                         /* (the tracks under it as the last block ended) */
static uint32_t brk_rnd(uint32_t n)
{
    brk_r = brk_r * 1664525u + 1013904223u;
    return n ? (brk_r >> 16) % n : 0u;
}
static void brk_draw(void)                       /* a plan: the way, its window, its beats' order, its layers */
{
    static const uint8_t RW[6] = {1, 2, 2, 4, 4, 8};   /* (each a whole part of the bar: on the beat, always) */
    uint32_t i;
    do
        brk_rec = (uint8_t)brk_rnd(NBR);
    while (brk_rec == brk_prev);
    brk_prev = brk_rec;
    brk_w = brk_rec == BR_REPEAT ? RW[brk_rnd(6)] : brk_rec == BR_REVERSE ? (brk_rnd(2) ? 8u : 4u) : 4u;
    for (i = 0; i < 4u; i++)                     /* CHOP: the bar's four beats, the first kept, the others swapped */
        brk_chop[i] = (uint8_t)i;
    for (i = 1; i < 4u; i++)
        if (brk_rnd(100) < 60u) {
            uint32_t j = 1u + brk_rnd(3);
            uint8_t x = brk_chop[i];
            brk_chop[i] = brk_chop[j];
            brk_chop[j] = x;
        }
    brk_gap = brk_rnd(3) ? 12u : 14u;
    for (brk_lay = 0; !brk_lay;)
        for (i = 0; i < 5u; i++)
            if (brk_rnd(100) < 40u)
                brk_lay |= (uint8_t)(1u << i);
    if (brk_rec == BR_ROLL)
        brk_lay |= BL_RAT;
    brk_fresh = (1u << NTRACK) - 1u;
    brk_since = 0;
    brk_shown_w = brk_w;
    brk_shown = brk_rec;
}
static volatile uint32_t seq_gcount;             /* (the looper's, below: the tempo's 16ths since PLAY) */
static void brk_tick(void)                       /* (each of the tempo's 16ths, playing) */
{
    if (!brk_mask) {
        brk_shown = 0xFF;
        return;
    }
    if (brk_seed != brk_seen) {                  /* pressed: a plan */
        brk_seen = brk_seed;
        brk_r = brk_seed | 1u;
        brk_n16 = 0;
        brk_draw();
        return;
    }
    brk_n16++;                                   /* (one way a press, however long it is held: build 53) */
    brk_since++;
}
/* the way's step for step p of len, from anchor a (a beat) */
static uint32_t brk_map(uint32_t a, uint32_t p, uint32_t len, uint32_t kick)
{
    uint32_t rel, w, src = p;
    a = a < len ? a : 0u;
    rel = (p + len - a) % len;
    w = brk_w < len ? brk_w : len;
    switch (brk_rec) {
    case BR_REPEAT:
        src = (a + rel % w) % len;
        break;
    case BR_HALF:
        src = (a + rel / 2u) % len;
        break;
    case BR_DOUBLE:
        src = (a + 2u * rel) % len;
        break;
    case BR_REVERSE:                             /* (each window backwards after its first step: 0 3 2 1) */
        src = p - p % w + (w - p % w) % w;
        break;
    case BR_CHOP:
        src = len >= 16u ? p - p % 16u + brk_chop[(p % 16u) / 4u] * 4u + p % 4u : p;
        break;
    default:
        break;
    }
    if (src >= len || (kick && p % 16u == 0u))   /* (the kick: the bar's first step its own) */
        src = p;
    return src;
}
/* the step track t plays for its step p (of len), and its layers for that hit (0xFF: none plays) */
static uint32_t brk_step(uint32_t t, uint32_t p, uint32_t len)
{
    uint32_t src, kick = brk_kick >> t & 1u, drum = brk_drum >> t & 1u, ro, rate;
    if (brk_fresh >> t & 1u) {                   /* (into the break on the track's next beat -- its next half bar for
                                                  * REPEAT 8: as written until then) */
        if (p % (brk_w > 4u ? 8u : 4u))
            return p;
        brk_anchor[t] = (uint8_t)p;
        brk_fresh &= (uint8_t)~(1u << t);
    }
    src = brk_map(brk_anchor[t], p, len, kick);
    if ((brk_lay & BL_GAP) && !kick && p % 16u >= brk_gap)   /* a gap before the bar's end: the kick alone */
        return 0xFFu;
    ro = brk_rec == BR_ROLL ? (kick ? 10u : drum ? 70u : 35u) : (kick ? 5u : drum ? 35u : 15u);
    if ((brk_lay & BL_RAT) && !seq_rtg[t][src] && brk_rnd(100) < ro) {   /* (faster the longer it is held) */
        rate = 2u + (brk_rec == BR_ROLL) + brk_n16 / 16u + brk_rnd(2);
        brk_rt[t] = (uint8_t)(rate < 6u ? rate : 6u);
    }
    if ((brk_lay & BL_OCT) && !kick && !drum && brk_rnd(100) < 30u)
        brk_oct[t] = (int8_t)(brk_rnd(2) ? 12 : -12);
    if ((brk_lay & BL_FLAM) && !kick && drum && brk_rnd(100) < 25u)
        brk_fl[t] = 1;
    if (brk_lay & BL_ACC)                        /* (accents: on the beat full, between softer) */
        brk_vp[t] = (uint8_t)(p % 4u == 0u ? 100u : 55u + brk_rnd(40));
    return src;
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
        uint32_t r8 = DIV_R8[seq_div[t] % NDIV], rn = brk_rt[t] ? brk_rt[t] : RTG_N[seq_rtg[t][p] % NRTG];
        uint32_t c = ++seq_plays[t][p], fx = MFX_OFF;
        uint32_t len8 = g >= GLEN_INF ? 0u : (uint32_t)maxi(1, GLEN_8TH[g] * 8 / (int32_t)r8);   /* (in the tempo's
                                                                                              * 16ths) */
        uint32_t sb = SEQ_STEP8 / (CTL * (uint32_t)clampi(fx_bpm10, 100, 3000) * r8);   /* (the step, in blocks) */
        uint32_t vel = (seq_vel[t][p] ? seq_vel[t][p] : VEL_DEF) * brk_vp[t] / 100u, k, l8 = len8 ? len8 : 64u / r8;
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
        if (brk_oct[t] && (int32_t)n + brk_oct[t] >= 24 && (int32_t)n + brk_oct[t] <= 108)   /* (BREAK's leap) */
            n = (uint32_t)((int32_t)n + brk_oct[t]);
        if (brk_fl[t] && fx == MFX_OFF)          /* (BREAK's flam) */
            fx = MFX_FLAM;
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
            rt_vel[t] = (uint8_t)maxi(1, (int32_t)vel);
            rt_step[t] = (uint8_t)p;
            rt_n[t] = (uint8_t)rn;
            rt_chf[t] = seq_chf[t][p];
        }
        seq_soundc(t, n, vel, len8, seq_lid[t][p], seq_lv[t][p], seq_chf[t][p]);
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
                seq_soundc(t, q->note, q->vel, q->len8, seq_lid[t][q->step], seq_lv[t][q->step], seq_chf[t][q->step]);
                q->note = 0;
            }
        }
}

/* ---------------------------------------------------------------- the looper --- */
/* up to three loops a track, each its own length in steps (up to 32 bars; build 37), played over the track's steps in time
 * with them: notes at steps (recorded quantized: ui.c), each with its velocity and its length in eighths of a step.
 * A loop being recorded is not played (on 0) until it is ended; then it plays from its first step on the step it
 * ended at (anchor, in seq_gcount's count) */
#define NLOOP 3u
#define LOOP_EV 256u             /* (notes a loop) */
#define LOOP_MAX 512u            /* (steps: 32 bars) */
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
/* (build 65) a track a pattern change unmuted: each of its loops silent until it comes round to its first step, then
 * on in its place (a bit a loop; set at the switch -- the ISR's, playing; the main loop's, stopped -- cleared here) */
static volatile uint8_t loop_wait[NTRACK];
/* (build 68) a loop's notes kept as recorded, at the transposition lp_ref (the track's TRN as it was recorded, or as
 * the session was saved); played moved by the track's transposition playing, lp_trn, less it -- so that the loops
 * follow a pattern's transposition from its first step (lp_trn: set by the main loop, and by the sequencer as a
 * pattern comes in) */
static int8_t lp_ref[NTRACK][NLOOP];
static volatile int8_t lp_trn[NTRACK];
static uint32_t lp_note(uint32_t t, uint32_t l, uint32_t n)
{
    int32_t d = lp_trn[t] - lp_ref[t][l];
    return d ? (uint32_t)sc_move((int32_t)n, d) : n;
}
static void loop_wait_set(uint32_t t)
{
    uint32_t l, m = 0;
    for (l = 0; l < NLOOP; l++)
        if (loops[t][l].on && loops[t][l].len)
            m |= 1u << l;
    loop_wait[t] = (uint8_t)m;
}
static void loop_kick(void)                      /* (each block: a loop's first step, late) */
{
    uint32_t t, l;
    for (t = 0; t < NTRACK; t++)
        for (l = 0; l < NLOOP; l++) {
            loop_t *p = &loops[t][l];
            if (!p->on || !p->kick)
                continue;
            loop_wait[t] &= (uint8_t)~(1u << l);  /* (its first step: no more waiting) */
            for (p->cur = 0; p->cur < p->n && p->ev[p->cur].step == 0u; p->cur++)
                if (!p->mute)
                    seq_sound(t, lp_note(t, l, p->ev[p->cur].note), p->ev[p->cur].vel, p->ev[p->cur].len8, 0, 0);
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
            if (!ph) {
                p->cur = 0;
                loop_wait[t] &= (uint8_t)~(1u << l);   /* (its first step: on, in its place) */
            }
            while (p->cur < p->n && p->ev[p->cur].step < ph)
                p->cur++;
            for (; p->cur < p->n && p->ev[p->cur].step == ph; p->cur++)
                if (!p->mute && !(loop_wait[t] >> l & 1u))
                    seq_sound(t, lp_note(t, l, p->ev[p->cur].note), p->ev[p->cur].vel, p->ev[p->cur].len8, 0, 0);
        }
}
/* a pattern's steps whole, as the sequencer plays them (projects.c fills one, the ISR takes it in) */
typedef struct {
    uint8_t note[NTRACK][NSTEP], vel[NTRACK][NSTEP], glen[NTRACK][NSTEP], cond[NTRACK][NSTEP], rtg[NTRACK][NSTEP];
    int8_t mut[NTRACK][NSTEP];
    uint8_t mcy[NTRACK][NSTEP], mfx[NTRACK][NSTEP], fcy[NTRACK][NSTEP];
    uint8_t chf[NTRACK][NSTEP];
    uint8_t lid[NTRACK][NSTEP][NPL];
    int16_t lv[NTRACK][NSTEP][NPL];
    uint8_t len[NTRACK], div[NTRACK], dir[NTRACK];
    uint8_t den[NTRACK], cx[NTRACK];             /* (the phrase generator's, the main loop's: not the ISR's) */
    int8_t trn[NTRACK];                          /* (each track's transposition, steps of the scale: the main loop's) */
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
    memcpy(b->chf, (const void *)seq_chf, sizeof b->chf);
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
    memcpy((void *)seq_chf, b->chf, sizeof b->chf);
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
static volatile uint8_t seq_pend_mu;             /* (build 65: the pattern waiting's mutes, a bit a track, bit 7 set: it has
                                                  * them -- applied as it is taken in) */
static volatile uint8_t seq_mu_was;              /* (.. and the mutes it found, bit 7 set: for the main loop) */
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
/* the arpeggiator's next note: the held notes over RANGE octaves, in MODE's order; BASS: every so many, the lowest an
 * octave down; OCT RND: an octave up or down at random (0: none) */
static uint32_t arp_rand(void)
{
    arp_rnd = arp_rnd * 1664525u + 1013904223u;
    return arp_rnd >> 8;
}
static uint32_t arp_pick(uint32_t t)
{
    arp_t *a = &arps[t];
    const volatile uint8_t *p = arp_p[t];
    uint8_t s[ARP_N];
    uint32_t n = a->n, rng = p[AR_RNG] % NAR_RNG + 1u, nn = n * rng, c = a->c, i, j, m, b = p[AR_BASS] % NAR_BASS;
    int32_t note;
    for (i = 0; i < n; i++) {                    /* (the notes by pitch; ORDER: as played) */
        uint8_t x = a->note[i];
        for (j = i; p[AR_MODE] != AM_ORDER && j > 0u && s[j - 1u] > x; j--)
            s[j] = s[j - 1u];
        s[j] = x;
    }
    m = c % nn;
    switch (p[AR_MODE] % NAM) {
    case AM_DOWN: i = nn - 1u - m; break;
    case AM_UPDN: m = nn < 2u ? 0u : c % (2u * nn - 2u); i = m < nn ? m : 2u * nn - 2u - m; break;
    case AM_DNUP: m = nn < 2u ? 0u : c % (2u * nn - 2u); i = nn - 1u - (m < nn ? m : 2u * nn - 2u - m); break;
    case AM_RND: i = arp_rand() % nn; break;
    case AM_CONV: i = m & 1u ? nn - 1u - m / 2u : m / 2u; break;   /* (the ends, in to the middle) */
    case AM_DIV: m = nn - 1u - m; i = m & 1u ? nn - 1u - m / 2u : m / 2u; break;   /* (the middle, out) */
    case AM_PINKY: m = nn < 2u ? 0u : c % (2u * nn - 2u); i = m & 1u ? nn - 1u : m / 2u; break;   /* (the top between) */
    case AM_THUMB: m = nn < 2u ? 0u : c % (2u * nn - 2u); i = m & 1u ? 1u + m / 2u : 0u; break;   /* (the bottom between) */
    default: i = m; break;                       /* UP, ORDER */
    }
    note = (int32_t)s[i % n] + 12 * (int32_t)(i / n);
    if (b && c % (b + 1u) == 0u)
        note = (int32_t)s[0] - 12 >= 12 ? (int32_t)s[0] - 12 : (int32_t)s[0];
    else if (p[AR_ORND] && arp_rand() % 127u < p[AR_ORND])
        note += arp_rand() & 1u ? 12 : -12;
    while (note > 120)
        note -= 12;
    while (note < 12)
        note += 12;
    return (uint32_t)note;
}
/* a note of the arpeggiator now (DENSITY: or a rest), its length GATE's share of the rate's; for the main loop too */
static void arp_fire(uint32_t t, uint32_t l, uint32_t adv)
{
    arp_t *a = &arps[t];
    const volatile uint8_t *p = arp_p[t];
    uint32_t note, vel = 127, g = (uint32_t)p[AR_GATE] + 1u, i;
    a->since = 0;
    if (p[AR_DENS] < 127u && arp_rand() % 127u >= p[AR_DENS]) {
        a->c++;
        return;
    }
    note = arp_pick(t);
    for (i = 0; i < a->n; i++)                   /* (its velocity: the key's it came from) */
        if (a->note[i] % 12u == note % 12u) {
            vel = a->vel[i];
            break;
        }
    a->c++;
    arp_quiet(t);
    note_vel = (uint8_t)vel;
    note_on(t, note, SRC_ARP);
    note_vel = 127;
    a->snd = (uint8_t)note;
    a->left = adv ? (l >> 7) * g / adv : 1u;
    if (!a->left)
        a->left = 1;
    if (arp_rw - arp_rr < ARP_RQ) {
        arp_rec_t *r = &arp_rq[arp_rw % ARP_RQ];
        uint32_t e = AR_R6[p[AR_RATE] % NAR_RATE] * g / 96u;   /* (eighths of a 16th: rate x 8 / 6, x the gate / 128) */
        r->t = (uint8_t)t;
        r->note = (uint8_t)note;
        r->vel = (uint8_t)vel;
        r->len8 = (uint16_t)(e ? e : 1u);
        r->pos = seq_pos[t];
        r->acc = seq_tacc[t];
        r->q = seq_gcount == 0xFFFFFFFFu ? 0u : seq_gcount + (seq_acc >= SEQ_16TH / 2u);
        RING_PUBLISH();
        arp_rw++;
    }
}
/* each block: each track's arpeggiator on (notes held), on the sequencer's grid while it plays (its place in four
 * 16ths, at the rate's), else on its own clock; a new chord plays at once (playing: if no later than a quarter of
 * the rate's past its place, else from the next) */
static void arp_block(uint32_t adv, int run)
{
    uint32_t t;
    for (t = 0; t < NTRACK; t++) {
        arp_t *a = &arps[t];
        uint32_t l = SEQ_16TH / 6u * AR_R6[arp_p[t][AR_RATE] % NAR_RATE];
        if (a->snd && a->left && !--a->left)
            arp_quiet(t);
        if (a->since < 0xFFFFu)
            a->since++;
        if (!arp_on[t] || !a->n)
            continue;
        if (run) {
            uint32_t ph = (seq_gcount == 0xFFFFFFFFu ? 0u : seq_gcount % 4u) * SEQ_16TH + (seq_acc < SEQ_16TH ? seq_acc : 0u);
            int32_t k = (int32_t)(ph / l);
            if (a->fresh) {
                a->fresh = 0;
                a->k = k;
                if (ph % l <= l / 4u)
                    arp_fire(t, l, adv);
            } else if (k != a->k) {
                a->k = k;
                arp_fire(t, l, adv);
            }
            a->acc = 0;
        } else {
            if (a->fresh) {
                a->fresh = 0;
                a->acc = 0;
                arp_fire(t, l, adv);
            } else if (a->acc >= l) {
                a->acc -= l;
                arp_fire(t, l, adv);
            }
            a->acc += adv;
        }
    }
}
static void seq_block(void)
{
    uint32_t t, i, adv, xm = 0;
    midi_main_drain();                           /* (the main loop's MIDI out: CCs, program changes) */
    if (xk_stop) {                               /* MIDI STOP, START, CONTINUE (midi.c) */
        xk_stop = 0;
        seq_run = 0;
    }
    if (xk_start) {
        xm = xk_start;
        xk_start = 0;
        if (seq_run)                             /* (playing: from the start again) */
            seq_was = 0;
        seq_run = 1;
    }
    adv = midi_adv(seq_run && seq_was);          /* this block's way on: the tempo's, or the clock followed's */
    if (!seq_run) {
        midi_clock_out(adv);
        if (seq_was) {
            midi_stopped();
            seq_gcount = 0xFFFFFFFFu;            /* (stopped: not started -- else the looper, armed and its first
                                                  * note playing, measured from the last run's count and ended at
                                                  * once after 32 bars of it; build 44) */
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
        arp_block(adv, 0);
        if (met_on) {
            if (met_acc >= SEQ_16TH) {
                met_acc -= SEQ_16TH;
                met_step = (uint8_t)((met_step + 1u) & 15u);
                met_tick(met_step);
            }
            met_acc += adv;
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
        for (t = 0; t < NTRACK; t++)             /* (the arpeggiators on the grid from step 1: a note just played --
                                                  * the one that started it, REC or the looper armed -- is step 1's) */
            arps[t].k = arps[t].since < 64u && arps[t].since * adv <= SEQ_16TH / 24u * AR_R6[arp_p[t][AR_RATE] % NAR_RATE]
                        ? 0 : -1;                /* (a note no more than a quarter of the rate's ago) */
        seq_gstep = 15;
        seq_gcount = 0xFFFFFFFFu;
        for (t = 0; t < NTRACK; t++)             /* (the loops from their first steps) */
            for (i = 0; i < NLOOP; i++)
                loops[t][i].anchor = 0;
        if (xm == 2u && xk_spp) {                /* MIDI CONTINUE: from the song position, in 16ths -- each track
                                                  * where it would be by then on its clock, the loops too */
            uint32_t spp = xk_spp;
            seq_gcount = spp - 1u;
            seq_gstep = (uint8_t)((spp - 1u) & 15u);
            for (t = 0; t < NTRACK; t++) {
                uint32_t len = seq_len[t] ? seq_len[t] : NSTEP, p8 = spp * DIV_R8[seq_div[t] % NDIV], st = p8 / 8u;
                seq_loop[t] = (uint16_t)(st / len);
                if (p8 % 8u) {                   /* (inside a step: that one, played already; the next to come) */
                    seq_pos[t] = (uint8_t)(st % len);
                    seq_tacc[t] = (p8 % 8u) * SEQ_16TH;
                } else
                    seq_pos[t] = (uint8_t)(st ? (st - 1u) % len : 0xFFu);
            }
            midi_started(spp);
        } else
            midi_started(0);
        adv = midi_adv(1);
    }
    midi_clock_out(adv);
    for (t = 0; t < NTRACK; t++)                 /* the lengths run out */
        for (i = 0; i < SEQ_HOLD; i++)
            if (sq_note[t][i] && sq_left[t][i] != 0xFFFFu && !--sq_left[t][i]) {
                note_off(t, sq_note[t][i], SRC_SEQ);
                sq_note[t][i] = 0;
            }
    /* the pattern waiting, in -- before the tempo's 16th that comes with it, so that its loops play that step as the
     * pattern has them (build 68) */
    if (seq_pend == 1u && seq_tacc[seq_pend_t] >= SEQ_STEP8 && seq_wraps(seq_pend_t)) {
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
        for (t = 0; t < NTRACK; t++)             /* (its transpositions: the loops' from now) */
            lp_trn[t] = sb_next.trn[t];
        if (seq_pend_mu & 0x80u) {               /* (build 65) its mutes, now: a track it unmutes, its notes from the
                                                  * muted time let go, its loops waiting for their first step (one at
                                                  * its first step now: played at once) */
            uint32_t was = 0x80u, l;
            for (t = 0; t < NTRACK; t++) {
                uint32_t mu = seq_pend_mu >> t & 1u;
                was |= (uint32_t)(trk_mute[t] != 0) << t;
                if (trk_mute[t] && !mu) {
                    trk_hush(t);
                    loop_wait_set(t);
                    for (l = 0; l < NLOOP; l++) {
                        loop_t *p = &loops[t][l];
                        if (seq_acc < SEQ_16TH && p->on && p->len && (int32_t)(seq_gcount - p->anchor) >= 0 &&
                            (seq_gcount - p->anchor) % p->len == 0u) {
                            loop_wait[t] &= (uint8_t)~(1u << l);
                            p->kick = 1;
                        }
                    }
                }
                trk_mute[t] = (uint8_t)mu;
            }
            seq_mu_was = (uint8_t)was;
        }
        RING_PUBLISH();
        seq_pend = 2;
    }
    if (seq_acc >= SEQ_16TH) {
        seq_acc -= SEQ_16TH;
        seq_gstep = (uint8_t)((seq_gstep + 1u) & 15u);
        seq_gcount++;
        met_tick(seq_gstep);
        brk_tick();
        if (seq_pend == 1u && seq_wraps(seq_pend_t) && seq_tacc[seq_pend_t] >= SEQ_STEP8 - SEQ_STEP8 / 4u)
            for (t = 0; t < NTRACK; t++)         /* (the pattern waiting comes in within a quarter step, the track's
                                                  * clock a little behind the tempo's: its transpositions for the loops
                                                  * already, build 68) */
                lp_trn[t] = sb_next.trn[t];
        loop_step();
    }
    {   /* BREAK let go: its ratchets and echoes still to come dropped at once, so the pattern is back as the key
         * comes up (the notes sounding ring on) */
        uint8_t m = brk_mask, gone = (uint8_t)(brk_last & ~m);
        for (t = 0; gone && t < NTRACK; t++)
            if (gone >> t & 1u) {
                rt_left[t] = 0;
                memset(fxq[t], 0, sizeof fxq[t]);
            }
        brk_last = m;
    }
    for (t = 0; t < NTRACK; t++) {               /* each track on its own clock */
        if (seq_tacc[t] >= SEQ_STEP8) {
            uint32_t len = seq_len[t] ? seq_len[t] : NSTEP, p = seq_next(t, len), q = p;
            seq_tacc[t] -= SEQ_STEP8;
            rt_left[t] = 0;                      /* (a ratchet not done by the next step: cut) */
            seq_pos[t] = (uint8_t)p;
            brk_vp[t] = 100;
            if ((brk_mask >> t & 1u) && !(brk_keep >> t & 1u) && brk_shown != 0xFFu)
                q = brk_step(t, p, len);         /* (BREAK: another step, its own position kept) */
            if (q != 0xFFu)
                seq_trig(t, q);
            brk_rt[t] = brk_fl[t] = 0;
            brk_oct[t] = 0;
        } else if (rt_left[t] && seq_tacc[t] >= rt_next[t]) {   /* the ratchet's next hit */
            uint32_t r8 = DIV_R8[seq_div[t] % NDIV], rn = rt_n[t];
            rt_vel[t] = (uint8_t)maxi(1, rt_vel[t] - rt_vel[t] / 8);
            seq_soundc(t, rt_note[t], rt_vel[t], (uint32_t)maxi(1, (int32_t)(64u / r8 / (2u * (rn > 1u ? rn : 2u)))),
                       seq_lid[t][rt_step[t]], seq_lv[t][rt_step[t]], rt_chf[t]);
            rt_next[t] += rt_int[t];
            rt_left[t]--;
        }
        seq_tacc[t] += adv * DIV_R8[seq_div[t] % NDIV];
        seq_tacc_now[t] = seq_tacc[t];
    }
    loop_kick();
    fxq_block();
    arp_block(adv, 1);
    seq_acc += adv;
    seq_acc_now = seq_acc;
}

/* ------------------------------------------------------------- the mix --- */
#define TRACK_GAIN 6144          /* Q12, x 1.5 */
#define LIM 19661                /* a track's ceiling, 0.6 (Q15) */
FAST static void lim_block(int32_t *env, int32_t *g, int32_t *x)
{
    int32_t pk = 0, want, g0 = *g, dg;
    uint32_t i;
    for (i = 0; i < CTL;) {
        REP4({
            pk = maxi(pk, absi(x[i]));
        })
    }
    if (pk > *env)
        *env = pk;
    else
        *env -= *env >> 8;
    want = *env > LIM ? (int32_t)((uint32_t)LIM * 32768u / (uint32_t)*env) : Q15;
    if (want > g0 && g0 < Q15)
        want = g0 + ((want - g0) >> 3);
    dg = (want - g0) / (int32_t)CTL;
    for (i = 0; i < CTL;) {
        REP4({
            g0 += dg;
            x[i] = mulq15(x[i], g0);
        })
    }
    *g = want;
}

/* the voices' machines and chains, a block ahead of the tracks (BIGORNA build 32): each voice into its own buffer,
 * half of them (by their cost) on CPU1 (hal/fm1_multicore.h, src/audio_worker.h) while CPU0 renders the others;
 * the tracks then fade and mix the buffers as before. Alone (CPU1 off, retired, or the host), CPU0 renders all:
 * the same output either way */
typedef struct {                                 /* a voice's block (on cache lines of its own: either core writes it) */
    int32_t buf[CTL];
    uint32_t alive, trk, ticks;                  /* (trk: the track it was rendered for this block; 0xFF none) */
#if TELEMETRY
    uint32_t tm, tc;                             /* (its machine's, its chain's time) */
#endif
} __attribute__((aligned(32))) vr_t;
static vr_t vr[NVOICE] POOL;
static uint8_t vr_core[NVOICE];                  /* (1: CPU1's this block; CPU0 writes it before the job only) */
/* the effects a block behind (0.73 ms: their returns' pre-delay): the sends the tracks made last block, the chorus,
 * delay and reverb rendered from them while the voices render (on CPU1 with its voices), their returns added after
 * the tracks; on one core in the same order, the same output */
static struct {
    int32_t ch[CTL], dl[CTL], rv[CTL];           /* (the sends, last block's) */
    int32_t wl[3][CTL], wr[3][CTL];              /* (each return, from them) */
    uint32_t ticks;                              /* (their time, the last block) */
} __attribute__((aligned(32))) fxb POOL;
FAST static void fx_render(void)
{
    uint32_t i, f, c0 = COST_NOW();
    for (f = 0; f < 3u; f++)
        for (i = 0; i < CTL;) {
            REP4({
                fxb.wl[f][i] = fxb.wr[f][i] = 0;
            })
        }
    TL_FX(0, ch_block(fxb.ch, fxb.wl[0], fxb.wr[0], fxb.dl, fxb.rv))
    TL_FX(1, dl_block(fxb.dl, fxb.wl[1], fxb.wr[1], fxb.rv))
    TL_FX(2, rv_block(fxb.rv, fxb.wl[2], fxb.wr[2]))
    fxb.ticks = COST_NOW() - c0;
}
static const ck_t *vr_ck[NTRACK];                /* (each track's cooked sound, for the block) */
FAST static void voice_render(uint32_t k)
{
    voice_t *v = &vox[k];
    vmem_t *m = &vmem[k];
    const ck_t *c = vr_ck[v->track];
    int32_t *vb = vr[k].buf, note16 = (int32_t)v->note << 16;
    uint32_t c0 = COST_NOW();
    switch (v->eng) {
    case ENG_FMT:
        FAR(ft_render)(&m->m.ft, &mck_of(k, c)->ft, note_inc(note16), note16, vb);
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
#if TELEMETRY
    {
        uint32_t c3 = COST_NOW();
        vr[k].tm = c3 - c0;
        vr[k].alive = (uint8_t)vc_block(&m->ch, vck_of(k, c), vb, note16, v->vel);
        vr[k].tc = COST_NOW() - c3;
    }
#else
    vr[k].alive = (uint8_t)vc_block(&m->ch, vck_of(k, c), vb, note16, v->vel);
#endif
    vr[k].ticks = COST_NOW() - c0;
    vr[k].trk = v->track;
}
#if TELEMETRY
static volatile uint8_t tl_c1_crash;
static uint32_t *volatile tl_c1_crash_at;   /* (0: the address written; volatile, so not folded) */
#endif
FAST static void vr_job(void *ctx)               /* (CPU1: the effects, and its share of the voices) */
{
    uint32_t k;
    (void)ctx;
#if TELEMETRY
    if (tl_c1_crash) {
        tl_c1_crash = 0;
        *tl_c1_crash_at = 0;                     /* (the NULL page: a bus fault, on CPU1) */
    }
#endif
    fx_render();
    for (k = 0; k < NVOICE; k++)
        if (vr_core[k])
            voice_render(k);
}
/* ---------------------------------------------------------------- LFOs --- */
/* Three LFOs a track (build 50), the Digitone II's (its MOD pages; sound.h lfo_t): each runs in the track's sound,
 * its output (a wave, -1 .. 1, EXP and RAMP 0 .. 1) times its FADE and its DEPTH (+-63: the destination's whole
 * range) moving one parameter of the sound -- or LFO 1's or LFO 2's SPEED or DEPTH, for LFO 2 and 3 (computed first,
 * LFO 3 to 1). The rate is the Digitone II's law: a cycle every 2048 / (|SPEED| x MULT) 16ths, at the tempo (MULT
 * 0 .. 11: x1 .. x2048) or at 120 BPM (12 .. 23); a SPEED below 0 runs it backwards. MODE: FREE runs on; TRIG starts
 * it again at its START PHASE at each note on the track; HOLD runs on and holds its value at each note; ONE and HALF
 * start at a note, run one cycle (half), and stay. FADE: from each note, over |FADE| 16ths, in (below 64) or out
 * (above). RND: a value drawn at each cycle, START PHASE its slew.
 * They are the track's, not each voice's (as the Digitone II's POLY M.LFO): one copy of the track's cooked sound
 * moves, and its voices play it. That copy -- and each voice with step locks on such a track -- is cooked anew in
 * turn, one a block (~15 us at most: 2 % of a block, whatever is modulated); a track whose sound was just taken anew
 * first, the plain sound played until its copy is ready */
#define LFO_K 664862u                            /* (2^32 x CTL / (FS x 307200), x 65536: a block's phase, the rate's
                                                  * BPM x 10 x |SPEED| x MULT apart) */
typedef char lfo_k_ok[CTL == 32 && FS == 44100 ? 1 : -1];
typedef struct {
    uint32_t ph[NLFO], run[NLFO], go[NLFO];      /* the phase; how far a ONE or HALF has run; blocks since a note */
    int32_t live[NLFO], held[NLFO], tgt[NLFO];   /* the wave now; HOLD's value; RND's next */
    uint8_t stop[NLFO];
    uint32_t cooked;                             /* lfo_flips[t] + 1 when its copy was cooked (0: none) */
} lfo_st_t;
static lfo_st_t lfs[NTRACK];
static ck_t ck_lfo[NTRACK] POOL;                 /* (each track's cooked sound, as its LFOs have it now) */
static int32_t lfo_mod[NTRACK][NLFO];            /* (each LFO's share on its sound's destination, Q15; 0 none) */
static uint8_t lfo_any[NTRACK];                  /* (an LFO of the track has a destination and a depth) */
static uint32_t lfo_rr, lfo_rnd = 0x2545F491u;
static void lfo_off(lfo_t *l)
{
    l->spd = 80;
    l->mult = 2;
    l->fade = 64;
    l->dest = 0;
    l->wave = LW_TRI;
    l->sph = 0;
    l->mode = LM_FREE;
    l->dep = 64;
}
static void lfos_off(sound_t *s)
{
    uint32_t n;
    for (n = 0; n < NLFO; n++)
        lfo_off(&s->lfo[n]);
}
static int lfo_on(const lfo_t *l) { return l->dest && l->dest < LFO_ND && l->dep != 64; }
static int32_t lfo_wave(uint32_t w, uint32_t ph)  /* Q15 */
{
    uint32_t x;
    switch (w) {
    case LW_SINE:
        return sin_ph(ph);
    case LW_SQR:
        return ph < 0x80000000u ? 32767 : -32768;
    case LW_SAW:                                 /* (+1 down to -1) */
        return 32767 - (int32_t)(ph >> 16);
    case LW_EXP:                                 /* (1 down to 0, (1 - x)^3) */
        x = 32767u - (ph >> 17);
        return (int32_t)((x * x >> 15) * x >> 15);
    case LW_RAMP:                                /* (0 up to 1) */
        return (int32_t)(ph >> 17);
    default:                                     /* TRI: from 0 up, as SINE */
        x = (ph + 0x40000000u) >> 17;
        return x < 16384u ? (int32_t)(x * 4u) - 32768 : (x == 16384u ? 32767 : 98304 - (int32_t)(x * 4u));
    }
}
#define LFO_EVERY 4u                             /* (the LFOs' work every 4 blocks, 2.9 ms: the steps on one, a cook on
                                                  * another -- a cook in the audio takes ~3x what it does warm) */
static void lfo_step(uint32_t t)                 /* (LFO_EVERY blocks on: each LFO on, its share) */
{
    const sound_t *s = &ck_buf[t][ck_use[t]].snd;
    lfo_st_t *f = &lfs[t];
    int32_t sm[NLFO] = {0, 0, 0}, dm[NLFO] = {0, 0, 0}, n;
    int trig = lfo_trig[t];
    uint32_t any = 0;
    lfo_trig[t] = 0;
    for (n = NLFO - 1; n >= 0; n--) {            /* (LFO 3 first: it may move LFO 1's or 2's SPEED or DEPTH) */
        const lfo_t *l = &s->lfo[n];
        int32_t spd = (int32_t)l->spd - 64 + sm[n], dep = clampi((int32_t)l->dep - 64 + dm[n], -64, 64), o, fq = Q15;
        uint32_t mult = 1u << (l->mult % 12u), bpm10 = l->mult < 12u ? (uint32_t)clampi(fx_bpm10, 100, 3000) : 1200u, inc, p0;
        lfo_mod[t][n] = 0;
        if (!lfo_on(l) && !(sm[n] || dm[n]))
            continue;
        spd = clampi(spd, -64, 64);
        {
            uint64_t i64 = (((uint64_t)bpm10 * (uint32_t)absi(spd) * mult * LFO_K) >> 16) * LFO_EVERY;
            inc = i64 > 0x7FFFFFFFu ? 0x7FFFFFFFu : (uint32_t)i64;   /* (faster than a cycle in two: as good as) */
        }
        if (trig) {
            f->go[n] = 0;
            if (l->mode == LM_HOLD)
                f->held[n] = f->live[n];
            else if (l->mode != LM_FREE) {
                f->ph[n] = (uint32_t)l->sph << 25;
                f->run[n] = 0;
                f->stop[n] = 0;
            }
        }
        p0 = f->ph[n];
        if (l->mode != LM_ONE && l->mode != LM_HALF)
            f->stop[n] = 0;                      /* (stopped by ONE or HALF, then another MODE: on again) */
        if (!f->stop[n]) {
            f->ph[n] += spd >= 0 ? inc : (uint32_t)-inc;
            if ((l->mode == LM_ONE && f->run[n] + inc < f->run[n]) ||
                (l->mode == LM_HALF && f->run[n] < 0x80000000u && f->run[n] + inc >= 0x80000000u))
                f->stop[n] = 1;                  /* (ONE: a cycle run; HALF: half) */
            f->run[n] += inc;
        }
        if (l->wave == LW_RND) {                 /* (a new value each cycle; START PHASE its slew) */
            if ((spd >= 0 ? f->ph[n] < p0 : f->ph[n] > p0) || trig) {
                lfo_rnd = lfo_rnd * 1664525u + 1013904223u;
                f->tgt[n] = (int32_t)(lfo_rnd >> 16) - 32768;
            }
            f->live[n] += l->sph >= 48u ? (f->tgt[n] - f->live[n]) >> (l->sph / 16u - 1u) : f->tgt[n] - f->live[n];
        } else
            f->live[n] = lfo_wave(l->wave, f->ph[n]);
        o = l->mode == LM_HOLD ? f->held[n] : f->live[n];
        if (l->fade != 64) {                     /* (from the last note, over |FADE| 16ths) */
            uint32_t len = (uint32_t)absi((int32_t)l->fade - 64) * (SEQ_16TH / CTL / LFO_EVERY) / bpm10 + 1u;
            uint32_t g = f->go[n] < len ? f->go[n] : len;
            fq = (int32_t)(g * (uint32_t)Q15 / len);
            if (l->fade > 64)
                fq = Q15 - fq;
            if (f->go[n] < 0xFFFFFFF0u)
                f->go[n]++;
        }
        o = (o * fq >> 15) * dep / 64;           /* (32767 x 32767 < 2^31: no 64 bits) */
        if (l->dest >= LFO_D_LFO) {              /* (LFO 1's or 2's SPEED or DEPTH, for a later LFO) */
            uint32_t m = (l->dest - LFO_D_LFO) / 2u;
            if ((int32_t)m < n) {
                if ((l->dest - LFO_D_LFO) & 1u)
                    dm[m] += o * 64 / 32768;
                else
                    sm[m] += o * 64 / 32768;
            }
        } else if (l->dest && l->dest < LFO_D_LFO) {   /* (on, or its DEPTH moved by a later LFO) */
            lfo_mod[t][n] = o;
            any = 1;
        }
    }
    lfo_any[t] = (uint8_t)any;
}
static int lfo_live(uint32_t t) { return lfo_any[t] && lfs[t].cooked == lfo_flips[t] + 1u; }
/* one sound cooked anew a block: a track whose own copy is old first, else the next in turn of the tracks with LFOs
 * on and their voices with step locks */
#if TELEMETRY
static uint32_t tl_lfo[4];                       /* (a TELEMETRY build's: the steps' and the cook's ticks, mean x 16 and
                                                  * highest) */
#endif
static void lfo_cook(void);
static void lfo_block(void)
{
    static uint8_t ph;
#if TELEMETRY
    uint32_t t0 = fm1_ticks(), t1, t2, t;
    ph = (uint8_t)((ph + 1u) % LFO_EVERY);
    for (t = 0; ph == 0u && t < NTRACK; t++)
        lfo_step(t);
    t1 = fm1_ticks();
    if (ph == LFO_EVERY / 2u)
        lfo_cook();
    t2 = fm1_ticks();
    tl_lfo[0] += (t1 - t0) - (tl_lfo[0] >> 4);
    tl_lfo[1] = t1 - t0 > tl_lfo[1] ? t1 - t0 : tl_lfo[1];
    tl_lfo[2] += (t2 - t1) - (tl_lfo[2] >> 4);
    tl_lfo[3] = t2 - t1 > tl_lfo[3] ? t2 - t1 : tl_lfo[3];
#else
    uint32_t t;
    ph = (uint8_t)((ph + 1u) % LFO_EVERY);
    for (t = 0; ph == 0u && t < NTRACK; t++)
        lfo_step(t);
    if (ph == LFO_EVERY / 2u)
        lfo_cook();
#endif
}
static void lfo_cook(void)
{
    uint32_t t, i, k;
    sound_t s;
    for (t = 0; t < NTRACK; t++)
        if (lfo_any[t] && lfs[t].cooked != lfo_flips[t] + 1u)
            break;
    if (t == NTRACK)
        for (i = 0; i < NTRACK + NVOICE; i++) {
            uint32_t j = (lfo_rr + i) % (NTRACK + NVOICE);
            if (j < NTRACK ? lfo_any[j] : vox[j - NTRACK].own && vox[j - NTRACK].sounding && lfo_any[vox[j - NTRACK].track]) {
                lfo_rr = j + 1u;
                t = j;
                break;
            }
        }
    if (t == NTRACK)
        return;
    if (t < NTRACK) {
        s = ck_buf[t][ck_use[t]].snd;
        lfo_apply(&s, lfo_mod[t]);
        ck_cook(&ck_lfo[t], &s);
        lfs[t].cooked = lfo_flips[t] + 1u;
        return;
    }
    k = t - NTRACK;                              /* (a voice with step locks: its locks, then the LFOs) */
    s = ck_buf[vox[k].track][ck_use[vox[k].track]].snd;
    for (i = 0; i < NPL; i++)
        if (vox[k].lid[i] < PL_MNG)
            par_put(&s, vox[k].lid[i] >> 3, vox[k].lid[i] & 7u, vox[k].lv[i]);
    lfo_apply(&s, lfo_mod[vox[k].track]);
    if (s.mach == vox[k].eng) {                  /* (the machine the voice plays: else not) */
        vck_cook(&vmem[k].lck, &s);
        mck_cook(&vmem[k].lm, &s);
    }
}

/* before the tracks: each track's sound for the block (a machine changed: its voices out first), then the voices */
static void vr_block(void)
{
    uint32_t t, k, s0 = 0, s1 = fxb.ticks, any1 = 0, n = 0, j;
    uint8_t ord[NVOICE];
    uint32_t cst[NVOICE];
    for (t = 0; t < NTRACK; t++) {
        track_t *y = &trk[t];
        if (!y->xfade && ck_pub[t] != ck_use[t]) {
            if (ck_buf[t][ck_pub[t]].mach != ck_buf[t][ck_use[t]].mach && track_sounding(t))
                y->xfade = FADE;                 /* another machine: out first */
            else if (ck_buf[t][ck_pub[t]].mach != ck_buf[t][ck_use[t]].mach)
                change_apply(t);
            else
            {
                ck_use[t] = ck_pub[t];           /* the same machine: on, with the new values */
                lfo_flips[t]++;
            }
        }
    }
    lfo_block();                                 /* the LFOs: on a block, one sound cooked anew (build 50) */
    for (t = 0; t < NTRACK; t++)
        vr_ck[t] = lfo_live(t) ? &ck_lfo[t] : &ck_buf[t][ck_use[t]];
    audio_worker_check();                        /* (a CPU1 that misread or faulted: alone from here) */
    for (k = 0; k < NVOICE; k++) {               /* (the voices by their last cost, the dearest first ..) */
        vr_core[k] = 0;
        vr[k].trk = 0xFFu;
        if (!vox[k].sounding)
            continue;
        cst[k] = vox[k].cost ? vox[k].cost : eng_cost[vox[k].eng];
        for (j = n; j > 0u && cst[ord[j - 1u]] < cst[k]; j--)
            ord[j] = ord[j - 1u];
        ord[j] = (uint8_t)k;
        n++;
    }
    if (audio_worker_online) {
        for (j = 0; j < n; j++) {                /* (.. each to the core with less so far: CPU1 has the effects) */
            k = ord[j];
            if (s1 <= s0) {
                vr_core[k] = 1;
                s1 += cst[k];
            } else
                s0 += cst[k];
        }
        any1 = audio_worker_submit(vr_job, 0);   /* (the effects on CPU1 even with no voice there) */
    }
    if (!any1) {
        for (k = 0; k < NVOICE; k++)
            vr_core[k] = 0;                      /* (CPU1 off or not free: all here) */
        FAR(fx_render)();
    }
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding && !vr_core[k])
            FAR(voice_render)(k);                /* (RAM code: through a pointer) */
    if (any1 && !audio_worker_join())
        for (k = 0; k < NVOICE; k++)
            if (vr_core[k]) {                    /* (CPU1 lost: its voices' block dropped, never rendered twice; the
                                                  * effects' too: their returns silent a block) */
                uint32_t i;
                for (i = 0; i < CTL; i++)
                    vr[k].buf[i] = 0;
                vr[k].alive = 1;
                vr[k].ticks = 0;
                vr[k].trk = vox[k].track;
            }
#if TELEMETRY
    for (k = 0; k < NVOICE; k++)
        if (vr_core[k])
            tl_c1 += vr[k].ticks;
#endif
}

FAST static void track_block(uint32_t t, int32_t *mixl, int32_t *mixr, int32_t *dlin, int32_t *rvin, int32_t *chin)
{
    track_t *y = &trk[t];
    int32_t buf[CTL], pk = 0, sd1, sr1, sc1, pl1, pr1, dk_dg = 0, dk_next = Q16;
    int duck = 0;
    uint8_t mg[NM];
    uint32_t i, k, any = 0;
    const ck_t *c = vr_ck[t];
    for (i = 0; i < CTL;) {
        REP4({
            buf[i] = 0;
        })
    }
    for (k = 0; k < NVOICE; k++) {
        voice_t *v = &vox[k];
        const int32_t *vb;
        int alive;
        uint32_t cc;
        if (v->track != t || !v->sounding)
            continue;
        if (vr[k].trk != t)                      /* (not rendered for this track this block: taken by it since) */
            voice_render(k);
        vb = vr[k].buf;
        alive = (int)vr[k].alive;
        cc = vr[k].ticks;
#if TELEMETRY
        tl_mach[v->eng] += vr[k].tm;
        tl_chain[v->eng] += vr[k].tc;
#endif
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
            for (i = 0; i < CTL;) {
                REP4({
                    buf[i] += mulq15(vb[i], g0 + (((g1 - g0) * (int32_t)i) >> 5));
                })
            }
            if (--v->fade == 0) {
                if (v->pend)
                    voice_take(k);
                else
                    voice_silence(k);
            }
        } else
            for (i = 0; i < CTL;) {
                REP4({
                    buf[i] += vb[i];
                })
            }
    }
    if (y->xfade) {
        int32_t g0 = (int32_t)(y->xfade * (uint32_t)Q15 / FADE), g1 = (int32_t)((y->xfade - 1u) * (uint32_t)Q15 / FADE);
        for (i = 0; i < CTL;) {
            REP4({
                buf[i] = mulq15(buf[i], g0 + (((g1 - g0) * (int32_t)i) >> 5));
            })
        }
        if (--y->xfade == 0 || !any) {
            y->xfade = 0;
            FAR(change_apply)(t);
        }
    }
    {   /* the sends and the pan: the step's locks over the sound's and the track's */
        int32_t pan = -1;
        for (k = 0; k < NM; k++)
            mg[k] = c->snd.mg[k];
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
            for (i = 0; i < CTL;) {
                REP4({
                    buf[i] = 0;
                })
            }
            dk_feed(&dk[t], buf, dk_struck[t]);
            dk_struck[t] = 0;
        }
        dk_sent[t][0] = dk_sent[t][1] = dk_sent[t][2] = 0;
        y->mg = trk_mute[t] ? 0 : Q15;           /* (silent: muted or not at once) */
        y->meter -= y->meter >> 4;
        y->g = (int32_t)(((int64_t)trk_gain[t] * y->mg) >> 15);
        y->pl = pl1;
        y->pr = pr1;
        y->sd = sd1;
        y->sr = sr1;
        y->sc = sc1;
        return;
    }
    mangle_block(t, buf, mg);                    /* MANGLE */
    {
        int32_t dc = y->dc;                      /* (in a register) */
        for (i = 0; i < CTL;) { /* (a DC blocker, ~10 Hz: FM with feedback can sit off centre) */
            REP4({
                int32_t xg = mulq12(buf[i], TRACK_GAIN);
                buf[i] = xg - lp8(&dc, xg, 47);
            })
        }
        y->dc = dc;
    }
    lim_block(&y->lim_env, &y->lim_g, buf);
    if (dk_mask >> t & 1u)                       /* (a source: its hits followed; muted, it ducks nothing) */
        dk_feed(&dk[t], trk_mute[t] ? zero_blk : buf, dk_struck[t] && !trk_mute[t]);
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
        int32_t mg1 = y->mg + clampi((trk_mute[t] ? 0 : Q15) - y->mg, -Q15 / 12, Q15 / 12);   /* (the mute, gliding) */
        int32_t g1 = (int32_t)(((int64_t)trk_gain[t] * mg1) >> 15);
        int32_t dg = (g1 - y->g) / CTL, dl = (pl1 - y->pl) / CTL, dr = (pr1 - y->pr) / CTL;
        int32_t dd = (sd1 - y->sd) / CTL, dv = (sr1 - y->sr) / CTL, dc = (sc1 - y->sc) / CTL;
        int32_t g = y->g, pl = y->pl, pr = y->pr, sd = y->sd, sr = y->sr, sc = y->sc;
        uint32_t sch = 0, sdl = 0, srv = 0;
        int ud = (sd | sd1) != 0, uv = (sr | sr1) != 0, uc = (sc | sc1) != 0;   /* (a send at 0 the whole block: none) */
        for (i = 0; i < CTL;) {
            REP4({
                int32_t x;
                g += dg;
                pl += dl;
                pr += dr;
                sd += dd;
                sr += dv;
                sc += dc;
                x = (int32_t)(((int64_t)buf[i] * g) >> 14);
                pk = maxi(pk, absi(x));
                if (dk_on) {                         /* (what it sends, for its share of each return) */
                    int32_t a = absi(x);
                    if (uc)
                        sch += (uint32_t)mulq15(a, sc);
                    if (ud)
                        sdl += (uint32_t)mulq15(a, sd);
                    if (uv)
                        srv += (uint32_t)mulq15(a, sr);
                }
                if (duck)                            /* (ducked: under its source's hits) */
                    x = (int32_t)(((int64_t)x * (dk_gp[t] + dk_dg * (int32_t)(i + 1u))) >> 16);
                mixl[i] += mulq15(x, pl);
                mixr[i] += mulq15(x, pr);
                if (ud)
                    dlin[i] += mulq15(x, sd);
                if (uv)
                    rvin[i] += mulq15(x, sr);
                if (uc)
                    chin[i] += mulq15(x, sc);
            })
        }
        y->g = g1;
        y->mg = mg1;
        y->pl = pl1;
        y->pr = pr1;
        y->sd = sd1;
        y->sr = sr1;
        y->sc = sc1;
        dk_sent[t][0] = sch;
        dk_sent[t][1] = sdl;
        dk_sent[t][2] = srv;
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
#if TELEMETRY
    tl_shed++;
#endif
    return 1;
}
static void budget_guard(void)
{
    uint32_t pred = blk_last + (audio_worker_online ? blk_new / 2u : blk_new), k;   /* (two cores: half each) */
    blk_new = 0;
    for (k = 0; k < NVOICE; k++)
        if (vox[k].sounding && vox[k].fade && !vox[k].pend)
            pred -= pred < vox[k].cost ? pred : vox[k].cost;
    while (pred > BUDGET) {
        uint32_t ok = tail_victim();
        if (ok == NVOICE)
            return;
        vox[ok].fade = SHED;
#if TELEMETRY
        tl_shed += 100u;                         /* (the guard's: counted apart, x 100) */
#endif
        vox[ok].fade_n = SHED;
        pred -= pred < vox[ok].cost ? pred : vox[ok].cost;
    }
}

/* one block: Q15 stereo, interleaved */
static void mix_block(int32_t *out)
{
    int32_t l_[CTL], r_[CTL], dlin[CTL], rvin[CTL], chin[CTL];
    uint32_t i, t, c0 = COST_NOW();
#if TELEMETRY
    uint32_t ce;
#endif
    events_take();
#if TELEMETRY
    ce = COST_NOW() - c0;
    tl_evmax = ce > tl_evmax ? ce : tl_evmax;
#endif
    {   /* the tracks some track ducks under (their hits followed: dk_feed) */
        uint32_t m = 0, q;
        for (q = 0; q < NTRACK; q++)
            if (sc_src[q] && sc_src[q] - 1u != q && sc_src[q] <= NTRACK && sc_lvl[q])
                m |= 1u << (sc_src[q] - 1u);
        dk_mask = (uint8_t)m;
        dk_on = m != 0u;
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
    for (i = 0; i < CTL;) {
        REP4({
            l_[i] = r_[i] = dlin[i] = rvin[i] = chin[i] = 0;
        })
    }
#if TELEMETRY
    ce = COST_NOW() - c0;
    tl_seqmax = ce > tl_seqmax ? ce : tl_seqmax;  /* (events, the sequencer, the guard: before the voices) */
    {
        uint32_t cp = COST_NOW();
        vr_block();
        cp = COST_NOW() - cp;
        tl_par += cp;
        tl_parmax = cp > tl_parmax ? cp : tl_parmax;
    }
#else
    vr_block();
#endif
    for (t = 0; t < NTRACK; t++) {
#if TELEMETRY
        uint32_t c1 = COST_NOW();
        FAR(track_block)(t, l_, r_, dlin, rvin, chin);
        tl_trk[t] += COST_NOW() - c1;
#else
        FAR(track_block)(t, l_, r_, dlin, rvin, chin);
#endif
    }
    {   /* the returns (last block's sends): each ducked by its tracks' share in it (a ducked pad's reverb with it) */
        uint32_t f;
        for (f = 0; f < 3u; f++) {
            int32_t g1 = Q16, dg, g;
            if (dk_on) {
                uint32_t num = 0, den = 0;
                for (t = 0; t < NTRACK; t++) {
                    uint32_t w;
                    dk_in[t][f] += ((dk_sent[t][f] < (1u << 21) ? dk_sent[t][f] : 1u << 21) >> 6) - (dk_in[t][f] >> 10);   /* (below 2^25) */
                    w = dk_in[t][f] >> 9;
                    num += w * ((uint32_t)dk_gp[t] >> 4);   /* (Q12: below 2^28 each) */
                    den += w;
                }
                g1 = den ? (int32_t)(num / den) << 4 : Q16;
            }
            g = dk_rg[f];
            if (g == Q16 && g1 == Q16)
                for (i = 0; i < CTL;) {
                    REP4({
                        l_[i] += fxb.wl[f][i];
                        r_[i] += fxb.wr[f][i];
                    })
                }
            else {
                dg = (g1 - g) / (int32_t)CTL;
                for (i = 0; i < CTL;) {
                    REP4({
                        g += dg;
                        l_[i] += (int32_t)(((int64_t)fxb.wl[f][i] * g) >> 16);
                        r_[i] += (int32_t)(((int64_t)fxb.wr[f][i] * g) >> 16);
                    })
                }
            }
            dk_rg[f] = g1;
        }
        for (i = 0; i < CTL;) {
            REP4({
                fxb.ch[i] = chin[i];
                fxb.dl[i] = dlin[i];
                fxb.rv[i] = rvin[i];
            })
        }
#if TELEMETRY
        tl_fx += fxb.ticks;
#endif
    }
    TL_FX(3, FAR(cp_block)(l_, r_))                            /* the compressor: the master's, the returns in it (a pad's tails duck) */
    met_block(l_, r_);
    for (i = 0; i < CTL;) {
        REP4({
            out[2u * i] = ceiling(mulq12(l_[i], master_q12));
            out[2u * i + 1u] = ceiling(mulq12(r_[i], master_q12));
        })
    }
#if defined(FELUCCA_UAC) && FELUCCA_UAC
    for (i = 0; i < CTL; i++) {
        usb_blk[2u * i] = ceiling(l_[i]);
        usb_blk[2u * i + 1u] = ceiling(r_[i]);
    }
#endif
    blk_last = COST_NOW() - c0;
#if TELEMETRY
    tl_all += blk_last;
    tl_max = blk_last > tl_max ? blk_last : tl_max;
    tl_n++;
#endif
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
        trk[t].mg = Q15;
        memset(trk[t].tid, PL_NONE, sizeof trk[t].tid);
    }
    memset((void *)seq_lid, PL_NONE, sizeof seq_lid);
    memset(arps, 0, sizeof arps);                /* (the arpeggiators off, their settings the start's) */
    memset((void *)arp_on, 0, sizeof arp_on);
    for (t = 0; t < NTRACK; t++)
        arp_defaults(t);
    memset(lfs, 0, sizeof lfs);                  /* (no LFO copy cooked: each track plays its own sound) */
    memset(lfo_any, 0, sizeof lfo_any);
    memset(lfo_mod, 0, sizeof lfo_mod);
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
