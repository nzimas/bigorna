/* SPDX-License-Identifier: GPL-3.0-only
 * The phrase generator: in the step view, the first black key writes a new phrase on the chosen track -- its steps,
 * their notes, velocities, lengths, trig conditions and parameter locks -- from the sound the track plays.
 *
 * The sound's family (gen.c's recipe) gives the track a role: KICK, SNARE, HAT, PERC, NOISE, BASS, LEAD, STAB or
 * DRONE (a sound with no family: its machine's). Each role has a few ways of making its rhythm (straight, broken,
 * Euclidean, clustered bursts), chosen at random, aimed at power noise, rhythmic noise and IDM rather than at a
 * dance floor: displaced accents, ghosts on probabilities, PRE / !PRE pairs that play one of two steps, A:B steps
 * that change every few times through.
 * The locks are moves from the sound's own values: the filter (FREQ, RESO, its envelope), the decay, the level,
 * the overdrive, the bit and sample rate reduction, the sends and the pan; drums lock sparingly where it accents,
 * noise and percussion lock almost every step. The notes come from the track's last note played (else its root) and
 * move in the scale chosen.
 * The pattern has room for so many locks (projects.c): a phrase that would not fit loses locks at random until it
 * does. */

enum { RO_KICK, RO_SNARE, RO_HAT, RO_PERC, RO_NOISE, RO_BASS, RO_LEAD, RO_STAB, RO_DRONE, NRO };
static const char *const RO_NAME[NRO] = {"KICK", "SNARE", "HAT", "PERC", "NOISE", "BASS", "LEAD", "STAB", "DRONE"};
/* each recipe's role, in RECIPES' order (gen.c) */
static const uint8_t RO_OF[] = {
    RO_KICK, RO_KICK, RO_KICK, RO_KICK,          /* TIGHT, DEEP, DRIVEN, CLICK KICK */
    RO_SNARE, RO_SNARE, RO_SNARE, RO_SNARE,      /* FM SNARE, TIGHT SNARE, CLAP, DIRTY SNARE */
    RO_HAT, RO_HAT, RO_HAT, RO_HAT,              /* METAL HAT, OPEN HAT, RIDE, CYMBAL */
    RO_PERC, RO_PERC, RO_PERC, RO_PERC,          /* TOM, RIM, FM PERC, CLANG */
    RO_KICK, RO_PERC, RO_SNARE, RO_PERC,         /* SLAM, STAMP, CRUNCH, COWBELL */
    RO_KICK, RO_PERC, RO_NOISE, RO_NOISE,        /* CRUSH KICK, COMB HIT, SCREECH, RATTLE */
    RO_NOISE, RO_KICK,                           /* STATIC BURST, IMPACT */
    RO_DRONE, RO_DRONE, RO_DRONE,                /* NOISE ENGINE, SCRAPE, MACHINE */
    RO_BASS, RO_LEAD, RO_LEAD, RO_STAB, RO_LEAD, RO_BASS, RO_NOISE, RO_PERC,   /* FM BASS .. FM CLANK */
    RO_DRONE,                                    /* FB DRONE */
    RO_STAB, RO_LEAD, RO_BASS, RO_NOISE, RO_BASS,   /* CHORD STAB, RING SCREAM, SYNC GRIND, DIGITAL HASH, PD BUZZ */
    RO_DRONE, RO_DRONE, RO_DRONE, RO_DRONE, RO_DRONE,   /* WAVE PAD, RING DRONE, GRAIN FIELD, BREATH, RADIO */
    RO_BASS, RO_LEAD, RO_STAB, RO_BASS, RO_LEAD,    /* SUB SWARM, SWARM LEAD, SWARM STAB, SWARM GRIND, HORNET */
    RO_DRONE, RO_DRONE, RO_DRONE,                /* SWARM PAD, SWARM DRONE, DARK WASH */
};
typedef char ro_fits[sizeof RO_OF == NRECIPE ? 1 : -1];

static uint32_t phrase_role(const sound_t *s)
{
    static const uint8_t BY_MACH[NENG] = {RO_LEAD, RO_PERC, RO_STAB, RO_BASS};
    return s->family < NRECIPE ? RO_OF[s->family] : BY_MACH[s->mach % NENG];
}

/* ------------------------------------------------------------- the dice --- */
static uint32_t ph_seed;
static uint32_t ph_rnd(uint32_t n)               /* 0 .. n - 1 */
{
    ph_seed = ph_seed * 1664525u + 1013904223u;
    return n ? (ph_seed >> 16) % n : 0u;
}
static int ph_ch(uint32_t pct) { return ph_rnd(100) < pct; }
static int32_t ph_ri(int32_t lo, int32_t hi) { return lo + (int32_t)ph_rnd((uint32_t)(hi - lo + 1)); }

/* ------------------------------------------------------------ the steps --- */
static uint32_t ph_t;                            /* the track written */
static int32_t ph_ref;                           /* the note the phrase is built from */
static const sound_t *ph_s;                      /* .. and the sound */

static void ph_step(uint32_t i, int32_t deg, int32_t vel, uint32_t glen, uint32_t cond)
{
    i &= NSTEP - 1u;
    seq_vel[ph_t][i] = (uint8_t)clampi(vel, 1, 127);
    seq_glen[ph_t][i] = (uint8_t)(glen < NGLEN ? glen : GLEN_DEF);
    seq_cond[ph_t][i] = (uint8_t)(cond < NCOND ? cond : CD_NONE);
    seq_note[ph_t][i] = (uint8_t)scale_move(ph_ref, deg);
}
static void ph_lock(uint32_t i, uint32_t lk, int32_t v) { seq_lock[ph_t][i & (NSTEP - 1u)][lk] = (uint8_t)clampi(v, 0, 127); }
static int ph_on(uint32_t i) { return seq_note[ph_t][i & (NSTEP - 1u)] != 0; }

/* the sound's own value of a lock (a move from it is what is locked) */
static int32_t ph_base(uint32_t lk) { return lk_base(ph_s, lk); }   /* (the sound's own value: ui.c) */
static void ph_move(uint32_t i, uint32_t lk, int32_t lo, int32_t hi) { ph_lock(i, lk, ph_base(lk) + ph_ri(lo, hi)); }

/* a Euclidean rhythm: k hits as evenly as 16 steps allow, turned by rot */
static uint32_t ph_euclid(uint32_t k, uint32_t rot)
{
    uint32_t m = 0, i;
    for (i = 0; i < NSTEP; i++)
        if ((i * k) % NSTEP < k)
            m |= 1u << ((i + rot) % NSTEP);
    return m;
}
/* a condition that thins a step out: a probability (lo .. hi %), or A:B */
static uint32_t ph_prob(uint32_t lo, uint32_t hi)
{
    uint32_t i, best = 0;
    uint32_t want = (uint32_t)ph_ri((int32_t)lo, (int32_t)hi);
    for (i = 0; i < NPROB; i++)
        if (PROB[i] <= want)
            best = i;
    return CD_PROB + best;
}
static uint32_t ph_ab(uint32_t a, uint32_t b, int neg)   /* A:B (neg: !A:B) */
{
    uint32_t c = 0, bb;
    for (bb = 2; bb < b; bb++)
        c += bb;
    return CD_AB + 2u * (c + a - 1u) + (neg ? 1u : 0u);
}
static uint32_t ph_cond_thin(void)
{
    switch (ph_rnd(5)) {
    case 0: return ph_ab(1, 2, 0);
    case 1: return ph_ab(2, 2, 0);
    case 2: return ph_ab((uint32_t)ph_ri(1, 4), 4, ph_ch(30));
    default: return ph_prob(25, 80);
    }
}

/* -------------------------------------------------------------- the roles --- */
static void ph_kick(void)
{
    uint32_t i, m, st = ph_rnd(3);
    if (st == 0)                                 /* four on the floor, displaced kicks among them */
        m = 0x1111u | (ph_ch(60) ? 1u << (ph_ch(50) ? 14 : 15) : 0u) | (ph_ch(40) ? 1u << (ph_ch(50) ? 6 : 10) : 0u);
    else if (st == 1)                            /* broken: Euclidean, the one kept */
        m = ph_euclid((uint32_t)ph_ri(5, 7), ph_rnd(4)) | 1u;
    else                                         /* sparse, off the grid */
        m = 1u | 1u << ph_ri(3, 6) | 1u << ph_ri(9, 11) | (ph_ch(50) ? 1u << ph_ri(13, 15) : 0u);
    for (i = 0; i < NSTEP; i++) {
        int on1 = (i & 3u) == 0u;
        if (!(m >> i & 1u))
            continue;
        ph_step(i, on1 ? 0 : (ph_ch(25) ? -1 : 0), on1 ? ph_ri(110, 127) : ph_ri(60, 95), (uint32_t)ph_ri(1, 5),
                on1 || st == 0 ? CD_NONE : ph_cond_thin());
        if (on1 && ph_ch(35))
            ph_move(i, LK_OD, 15, 60);           /* (the accents drive harder) */
        if (!on1 && ph_ch(40))
            ph_move(i, LK_BR, 20, 70);
        if (ph_ch(30))
            ph_move(i, LK_ADEC, on1 ? 5 : -40, on1 ? 30 : -10);
        if (ph_ch(25))
            ph_move(i, LK_FREQ, -25, 15);
    }
    if (ph_ch(40)) {                             /* the last kick thrown into the reverb */
        for (i = NSTEP; i-- > 0u && !ph_on(i);)
            ;
        ph_lock(i, LK_REV, ph_ri(70, 120));
    }
}
static void ph_snare(void)
{
    uint32_t i, m = 1u << 4 | 1u << 12, g = ph_euclid((uint32_t)ph_ri(2, 5), ph_rnd(16)) & ~m;
    if (ph_ch(30))                               /* (displaced: the second off its beat) */
        m = 1u << 4 | 1u << (ph_ch(50) ? 11 : 13);
    for (i = 0; i < NSTEP; i++) {
        if (m >> i & 1u) {
            ph_step(i, 0, ph_ri(105, 127), (uint32_t)ph_ri(2, 6), CD_NONE);
            if (ph_ch(35))
                ph_lock(i, LK_REV, ph_ri(60, 127));
            if (ph_ch(25))
                ph_move(i, LK_OD, 10, 50);
        } else if (g >> i & 1u) {
            ph_step(i, ph_ch(30) ? 1 : 0, ph_ri(40, 80), (uint32_t)ph_ri(0, 3), ph_cond_thin());
            if (ph_ch(50))
                ph_move(i, LK_SRR, 20, 80);
            if (ph_ch(40))
                ph_lock(i, LK_PAN, ph_ri(20, 108));
            if (ph_ch(30))
                ph_lock(i, LK_DEL, ph_ri(50, 110));
        }
    }
    if (ph_ch(45)) {                             /* a roll into the next bar, rising, every other time */
        uint32_t from = (uint32_t)ph_ri(13, 14), c = ph_ab(2, 2, 0);
        for (i = from; i < NSTEP; i++) {
            ph_step(i, (int32_t)(i - from), 60 + (int32_t)(i - from) * 20, 1, c);
            ph_lock(i, LK_FREQ, ph_base(LK_FREQ) - 20 + (int32_t)(i - from) * 15);
            ph_move(i, LK_ADEC, -50, -20);
        }
    }
}
static void ph_hat(void)
{
    uint32_t i, m, pan = (uint32_t)ph_ri(30, 98), st = ph_rnd(3);
    m = st == 0 ? 0xFFFFu & ~(ph_ch(50) ? 1u << ph_ri(0, 15) | 1u << ph_ri(0, 15) : 0u)   /* 16ths, a few gone */
        : st == 1 ? 0x4444u | ph_euclid((uint32_t)ph_ri(3, 5), ph_rnd(16))                 /* offbeats and more */
        : ph_euclid((uint32_t)ph_ri(7, 13), ph_rnd(16));
    for (i = 0; i < NSTEP; i++) {
        int acc = (i & 3u) == 2u;
        if (!(m >> i & 1u))
            continue;
        ph_step(i, ph_ch(15) ? ph_ri(-1, 1) : 0, acc ? ph_ri(95, 127) : ph_ri(45, 85), acc && ph_ch(30) ? 6u : 1u,
                acc || ph_ch(40) ? CD_NONE : ph_prob(50, 95));
        pan = (uint32_t)clampi((int32_t)pan + ph_ri(-18, 18), 10, 117);   /* (the pan wanders) */
        if (ph_ch(60))
            ph_lock(i, LK_PAN, (int32_t)pan);
        if (acc && ph_ch(40))
            ph_move(i, LK_ADEC, 20, 50);        /* (opened) */
        if (ph_ch(35))
            ph_move(i, LK_FREQ, -20, 20);
        if (ph_ch(12))
            ph_move(i, LK_BR, 40, 90);          /* (a glitch) */
        if (ph_ch(10))
            ph_move(i, LK_SRR, 40, 100);
    }
}
static void ph_perc(void)
{
    uint32_t i, m = ph_euclid((uint32_t)ph_ri(3, 7), ph_rnd(16)), pair = ph_ch(60);
    for (i = 0; i < NSTEP; i++) {
        uint32_t cond = CD_NONE;
        if (!(m >> i & 1u))
            continue;
        if (pair && ph_ch(40) && !(m >> ((i + 1u) % NSTEP) & 1u)) {   /* one of two: a 50 % step, !PRE after it */
            cond = ph_prob(50, 50);
            ph_step(i + 1u, ph_ri(-3, 5), ph_ri(60, 110), (uint32_t)ph_ri(0, 4), CD_NPRE);
            ph_lock(i + 1u, LK_PAN, ph_ri(0, 127));
        } else if (ph_ch(35))
            cond = ph_cond_thin();
        ph_step(i, ph_ri(-3, 5), ph_ri(60, 120), (uint32_t)ph_ri(0, 5), cond);
        ph_lock(i, LK_PAN, ph_ri(0, 127));
        if (ph_ch(50))
            ph_move(i, LK_FREQ, -30, 30);
        if (ph_ch(30))
            ph_lock(i, LK_DEL, ph_ri(40, 120));
        if (ph_ch(30))
            ph_move(i, LK_SRR, 10, 90);
        if (ph_ch(25))
            ph_move(i, LK_BR, 20, 80);
        if (ph_ch(20))
            ph_move(i, LK_ADEC, -30, 30);
    }
}
static void ph_noise(void)
{
    uint32_t i, n = (uint32_t)ph_ri(3, 6), k;
    for (k = 0; k < n; k++) {                    /* bursts: runs of one to four steps */
        uint32_t at = ph_rnd(16), len = (uint32_t)ph_ri(1, 4), j;
        for (j = 0; j < len; j++)
            ph_step(at + j, ph_ri(-2, 4), ph_ri(70, 127), (uint32_t)(ph_ch(30) ? ph_ri(6, 9) : ph_ri(0, 3)),
                    ph_ch(45) ? ph_prob(30, 75) : CD_NONE);
    }
    for (i = 0; i < NSTEP; i++) {
        if (!ph_on(i))
            continue;
        ph_lock(i, LK_PAN, ph_ch(50) ? (ph_ch(50) ? 0 : 127) : ph_ri(20, 108));
        if (ph_ch(70))
            ph_move(i, LK_FREQ, -45, 40);
        if (ph_ch(40))
            ph_move(i, LK_RESO, -20, 40);
        if (ph_ch(50))
            ph_lock(i, LK_SRR, ph_ri(20, 120));
        if (ph_ch(45))
            ph_lock(i, LK_BR, ph_ri(30, 115));
        if (ph_ch(40))
            ph_move(i, LK_OD, 10, 70);
        if (ph_ch(25))
            ph_lock(i, LK_DEL, ph_ri(60, 127));
        if (ph_ch(20))
            ph_lock(i, LK_VOL, ph_ri(50, 120));
    }
}
static void ph_bass(void)
{
    int32_t n = SCALE_N[ui.scale], deg[6];
    uint32_t i, m;
    deg[0] = deg[1] = deg[2] = 0;
    deg[3] = -n;                                 /* (the octave below) */
    deg[4] = n;
    deg[5] = ph_ri(1, n - 1);
    m = ph_ch(50) ? ph_euclid((uint32_t)ph_ri(5, 9), ph_rnd(3)) : (0xFFFFu & ~ph_euclid((uint32_t)ph_ri(4, 8), ph_rnd(16)));
    for (i = 0; i < NSTEP; i++) {
        int acc;
        if (!(m >> i & 1u))
            continue;
        acc = (i & 3u) == 0u || ph_ch(20);
        ph_step(i, deg[ph_rnd(6)], acc ? ph_ri(100, 127) : ph_ri(55, 90), (uint32_t)ph_ri(1, 5),
                acc || ph_ch(70) ? CD_NONE : ph_cond_thin());
        ph_move(i, LK_FREQ, acc ? 0 : -30, acc ? 35 : 5);   /* (the filter, step by step) */
        if (acc && ph_ch(50))
            ph_move(i, LK_RESO, 10, 40);
        if (acc && ph_ch(40))
            ph_move(i, LK_OD, 15, 55);
        if (ph_ch(25))
            ph_move(i, LK_FENV, -20, 30);
    }
}
static void ph_lead(int stab)
{
    int32_t d = 0;
    uint32_t i, m = stab ? (ph_euclid((uint32_t)ph_ri(3, 5), ph_rnd(16)) | (ph_ch(50) ? 0x0404u : 0u))
                         : ph_euclid((uint32_t)ph_ri(3, 6), ph_rnd(16));
    for (i = 0; i < NSTEP; i++) {
        if (!(m >> i & 1u))
            continue;
        d = stab ? (ph_ch(70) ? 0 : ph_ri(-2, 2)) : clampi(d + ph_ri(-3, 3), -5, 9);   /* (a walk in the scale) */
        ph_step(i, d, ph_ri(70, 120), stab ? (uint32_t)ph_ri(1, 4) : (uint32_t)ph_ri(4, 9),
                ph_ch(25) ? ph_ab((uint32_t)ph_ri(1, 2), 2, 0) : CD_NONE);
        if (ph_ch(35))
            ph_lock(i, LK_DEL, ph_ri(60, 127));  /* (thrown into the delay) */
        if (ph_ch(40))
            ph_move(i, LK_FREQ, -25, 25);
        if (ph_ch(20))
            ph_lock(i, LK_REV, ph_ri(50, 110));
        if (stab && ph_ch(20))
            ph_move(i, LK_BR, 20, 70);
        if (ph_ch(30))
            ph_lock(i, LK_PAN, ph_ri(35, 93));
    }
}
static void ph_drone(void)
{
    int32_t n = SCALE_N[ui.scale];
    uint32_t i;
    ph_step(0, 0, ph_ri(90, 120), ph_ch(50) ? GLEN_INF : 13u, CD_NONE);
    ph_move(0, LK_REV, 10, 60);
    if (ph_ch(70)) {                             /* a second voice, every other time or so */
        i = (uint32_t)(ph_ch(50) ? 8 : ph_ri(5, 11));
        ph_step(i, ph_ch(50) ? n : ph_ri(1, n - 1), ph_ri(60, 100), (uint32_t)ph_ri(9, 12), ph_ch(50) ? ph_ab(1, 2, 0) : CD_NONE);
        ph_move(i, LK_FREQ, -30, 20);
        ph_lock(i, LK_CHR, ph_ri(40, 120));
    }
    if (ph_ch(50)) {                             /* a low one under it, now and then */
        i = (uint32_t)ph_ri(12, 15);
        ph_step(i, -n, ph_ri(70, 110), (uint32_t)ph_ri(6, 10), ph_prob(30, 60));
        ph_move(i, LK_FREQ, -40, -10);
    }
}

/* SHIFT and the phrase key: new locks on the steps the track holds, as its role locks (each lock's chance a step,
 * in LK_* order: FREQ RESO ENV DECAY VOL DRIVE BITS SRR DELAY REVERB CHORUS PAN); the notes, velocities, lengths
 * and conditions as they were */
static const uint8_t RL_P[NRO][NLK] = {
    {25, 0, 10, 30, 0, 35, 20, 10, 0, 10, 0, 0},        /* KICK */
    {25, 10, 10, 25, 10, 20, 25, 30, 20, 25, 0, 30},    /* SNARE */
    {35, 10, 0, 35, 15, 0, 12, 10, 10, 5, 0, 60},       /* HAT */
    {50, 20, 10, 20, 15, 15, 25, 30, 30, 15, 10, 90},   /* PERC */
    {70, 40, 20, 20, 20, 40, 45, 50, 25, 15, 10, 90},   /* NOISE */
    {90, 40, 25, 15, 0, 35, 10, 5, 5, 5, 0, 0},         /* BASS */
    {40, 15, 15, 10, 0, 10, 10, 5, 35, 20, 15, 30},     /* LEAD */
    {40, 10, 10, 15, 0, 15, 20, 10, 35, 20, 10, 25},    /* STAB */
    {30, 10, 10, 0, 0, 5, 5, 5, 10, 50, 40, 20},        /* DRONE */
};
static void phrase_relock(uint32_t t)
{
    uint32_t i, l, role;
    int32_t pan = 64;
    ph_seed = next_seed();
    ph_t = t;
    ph_s = &ui.snd[t][ui.eng[t]];
    role = phrase_role(ph_s);
    for (i = 0; i < NSTEP; i++) {
        int acc = (i & 3u) == 0u;
        memset((void *)seq_lock[t][i], LK_NONE, NLK);
        if (!seq_note[t][i])
            continue;
        for (l = 0; l < NLK; l++) {
            if (!ph_ch(RL_P[role][l]))
                continue;
            switch (l) {
            case LK_FREQ: ph_move(i, l, acc ? -10 : -35, acc ? 35 : 20); break;
            case LK_RESO: ph_move(i, l, -20, 40); break;
            case LK_FENV: ph_move(i, l, -25, 30); break;
            case LK_ADEC: ph_move(i, l, acc ? 0 : -45, acc ? 35 : 15); break;
            case LK_VOL: ph_lock(i, l, ph_ri(acc ? 90 : 50, 127)); break;
            case LK_OD: ph_move(i, l, acc ? 20 : 5, acc ? 70 : 40); break;
            case LK_BR: ph_lock(i, l, ph_ri(20, 115)); break;
            case LK_SRR: ph_lock(i, l, ph_ri(10, 120)); break;
            case LK_DEL: ph_lock(i, l, ph_ri(45, 127)); break;
            case LK_REV: ph_lock(i, l, ph_ri(40, 127)); break;
            case LK_CHR: ph_lock(i, l, ph_ri(40, 120)); break;
            default:                             /* (the pan: hard sides now and then, else a walk) */
                pan = ph_ch(25) ? (ph_ch(50) ? 0 : 127) : clampi(pan + ph_ri(-30, 30), 0, 127);
                ph_lock(i, l, pan);
                break;
            }
        }
    }
    while (pat_bytes() > PAT_SEQ_ROOM) {
        uint32_t j = ph_rnd(NSTEP), k = ph_rnd(NLK);
        seq_lock[t][j][k] = LK_NONE;
    }
    pat_dirty = 1;
}

/* the chosen track's phrase, new; the pattern changed */
static void phrase_new(uint32_t t)
{
    sound_t *s = &ui.snd[t][ui.eng[t]];
    uint32_t i, role;
    ph_seed = next_seed();
    ph_t = t;
    ph_s = s;
    ph_ref = ui.last_note[t] ? ui.last_note[t] : (int32_t)key_note(0, ui.oct[t]);
    role = phrase_role(s);
    for (i = 0; i < NSTEP; i++) {
        seq_note[t][i] = 0;
        seq_vel[t][i] = VEL_DEF;
        seq_glen[t][i] = GLEN_DEF;
        seq_cond[t][i] = CD_NONE;
        memset((void *)seq_lock[t][i], LK_NONE, NLK);
    }
    switch (role) {
    case RO_KICK: ph_kick(); break;
    case RO_SNARE: ph_snare(); break;
    case RO_HAT: ph_hat(); break;
    case RO_PERC: ph_perc(); break;
    case RO_NOISE: ph_noise(); break;
    case RO_BASS: ph_bass(); break;
    case RO_LEAD: ph_lead(0); break;
    case RO_STAB: ph_lead(1); break;
    default: ph_drone(); break;
    }
    for (i = 0; i < NSTEP; i++)                  /* (no locks left on a step that holds no note) */
        if (!seq_note[t][i])
            memset((void *)seq_lock[t][i], LK_NONE, NLK);
    while (pat_bytes() > PAT_SEQ_ROOM) {         /* (over the pattern's room: locks go, at random) */
        uint32_t j = ph_rnd(NSTEP), l = ph_rnd(NLK);
        seq_lock[t][j][l] = LK_NONE;
    }
    pat_dirty = 1;
}
