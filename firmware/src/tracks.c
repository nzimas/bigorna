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
#define NVOICE 12
static const char *const ENG_NAME[NENG] = {"FM TONE", "FM DRUM", "WAVETONE", "SWARMER"};
static const char *const FLT_NAME[NFLT] = {"MULTI-MODE", "LOWPASS 4", "LEGACY", "COMB-", "COMB+", "EQUALIZER"};

typedef struct {
    uint8_t mach;
    vck_t ch;
    union { ftck_t ft; fdck_t fd; wtck_t wt; swck_t sw; } m;
    int32_t sd_del, sd_rev, sd_chr;      /* the sends, Q15 */
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
}

/* a voice's own memory: the chain (its comb line) and the machine's state */
typedef struct {
    vchain_t ch;
    union { ftv_t ft; fdv_t fd; wtv_t wt; swv_t sw; } m;
} vmem_t;
static vmem_t vmem[NVOICE] POOL;

#define FADE 12u                 /* blocks of a fade (~9 ms) */
#define QUICK 3u                 /* blocks of the budget's fade (~2 ms) */
enum { SRC_KEY, SRC_SEQ };       /* who plays a note: the keys (and MIDI), or a sequencer (to come) */
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
    vc_on(&m->ch, &c->ch, fresh);
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
    vc_off(&m->ch, &c->ch);
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
    int32_t buf[CTL], vb[CTL], pk = 0;
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
        alive = vc_block(&m->ch, &c->ch, vb, note16, v->vel);
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
    if (!any) {
        y->meter -= y->meter >> 4;
        y->g = trk_gain[t];
        y->pl = trk_pl[t];
        y->pr = trk_pr[t];
        y->sd = c->sd_del;
        y->sr = c->sd_rev;
        y->sc = c->sd_chr;
        return;
    }
    for (i = 0; i < CTL; i++) {                  /* (a DC blocker, ~10 Hz: FM with feedback can sit off centre) */
        int32_t xg = mulq12(buf[i], TRACK_GAIN);
        buf[i] = xg - lp8(&y->dc, xg, 47);
    }
    lim_block(&y->lim_env, &y->lim_g, buf);
    {   /* the level, the pan and the sends, gliding over the block */
        int32_t g1 = trk_gain[t], pl1 = trk_pl[t], pr1 = trk_pr[t];
        int32_t dg = (g1 - y->g) / CTL, dl = (pl1 - y->pl) / CTL, dr = (pr1 - y->pr) / CTL;
        int32_t dd = (c->sd_del - y->sd) / CTL, dv = (c->sd_rev - y->sr) / CTL, dc = (c->sd_chr - y->sc) / CTL;
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
        y->sd = c->sd_del;
        y->sr = c->sd_rev;
        y->sc = c->sd_chr;
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
    for (t = 0; t < NTRACK; t++)
        trk[t].lim_g = Q15;
    wt_init();
    fx_init();
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
