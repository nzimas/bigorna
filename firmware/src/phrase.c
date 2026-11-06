/* SPDX-License-Identifier: GPL-3.0-only
 * The phrase generator: in the step view, the first black key writes a new phrase on the chosen track -- its steps,
 * their notes, velocities, lengths, trig conditions and parameter locks -- from the sound the track plays.
 *
 * The sound's family (gen.c's recipe) gives the track a role: KICK, SNARE, HAT, PERC, NOISE, BASS, LEAD, STAB or
 * DRONE (a sound with no family: its machine's). Each role draws its rhythm from about ten of fifteen shapes (four on
 * the floor, Euclidean, groups of 2, 3 and 4 added up, a 3, 5, 6 or 7 step cell against the bar, clusters, gaps
 * shrinking or growing, a half bar mirrored or answered, bursts, 16ths, offbeats, walks), chosen at random, aimed at
 * power noise, rhythmic noise and IDM rather than at a dance floor: displaced accents, ghosts on probabilities,
 * PRE / !PRE pairs that play one of two steps, A:B steps that change every few times through. Now and then a part
 * fits itself to the other tracks (percussion in the kick's and snare's gaps, a bass on the kick's steps). The
 * snare picks a feel: a backbeat, displaced, half or double time, broken, or none.
 * Then a form over bars, often: a fill every 4th (8th, 2nd) time, a variant bar every 2nd or 3rd time, or the
 * second half the first again every other time; and the velocities, now and then, rising, falling, or alternating.
 * The locks are moves from the sound's own values: the filter (FREQ, RESO, its envelope), the decay, the level,
 * the overdrive, the bit and sample rate reduction, the sends and the pan; drums lock sparingly where it accents,
 * noise and percussion lock almost every step. The notes come from the track's last note played (else its root) and
 * move in the scale chosen.
 * The track's DENSITY and COMPLEXITY (the PHRASE page, 0 .. 100, 50 as drawn) thin the phrase out or fill it with
 * ghosts, and take locks away or add more (and ratchets).
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
    RO_BASS, RO_PERC,                            /* PULSE BASS, METAL HIT (build 95) */
};
typedef char ro_fits[sizeof RO_OF == NRECIPE ? 1 : -1];

static uint32_t phrase_role(const sound_t *s)
{
    static const uint8_t BY_MACH[NENG] = {RO_LEAD, RO_PERC, RO_STAB, RO_BASS};
    return s->family < NRECIPE ? RO_OF[s->family] : BY_MACH[s->mach % NENG];
}
static int snd_kick(const sound_t *s) { return phrase_role(s) == RO_KICK; }   /* (a sidechain's source: ui.c) */

/* BREAK's view of the tracks (tracks.c): the kicks, the drums (a kick, snare, hat, percussion, noise) and the drones,
 * from the sound each plays now */
static void phrase_brk_roles(void)
{
    uint32_t t, r;
    uint16_t k = 0, d = 0, n = 0;
    for (t = 0; t < NLANE; t++) {                /* (each lane: a subtrack's own sound, build 79) */
        r = phrase_role(lane_snd(t));
        k |= (uint16_t)((r == RO_KICK) << t);
        d |= (uint16_t)((r <= RO_NOISE) << t);
        n |= (uint16_t)((r == RO_DRONE) << t);
    }
    brk_kick = k;
    brk_drum = d;
    brk_keep = n;
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
static void ph_lock(uint32_t i, uint32_t lk, int32_t v)   /* (within the parameter's range; a step's eight full: not) */
{
    par_t p;
    pl_par(ph_s, LK_PID[lk], &p);
    (void)pl_set(ph_t, i & (NSTEP - 1u), LK_PID[lk], clampi(v, p.lo, p.hi));
}
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

/* ------------------------------------------------------------- the shapes --- */
/* (build 94) a role's steps come from one of many shapes, as 16 bits (step i: bit i), with the steps it accents:
 *   FOUR      a hit on each beat, displaced ones among them
 *   EUCLID    k hits as even as 16 steps allow, turned
 *   ADD       groups of 2, 3 and 4 steps added up to 16 (3+3+2, 3+3+3+3+4, 2+3+2+3+3+3 ..), a hit at each group's start
 *   CELL      a cell of 3, 5, 6 or 7 steps over and over against the bar's 4s (again from step 8, now and then)
 *   CLUSTER   hits bunched in runs of 2 .. 4, long gaps between
 *   ACCEL     gaps shrinking to the bar's end; DECEL growing
 *   SPARSE    one to three hits anywhere
 *   MIRROR    a half bar and the same backwards
 *   ANSWER    a half bar and its answer: the same, turned or with a hit moved
 *   BURST     a run of 16ths, the rest near silent
 *   STRAIGHT  16ths, a few gone; OFFBEAT the beat's offbeats (or its 16ths between), more on them
 *   WALK      gaps of 1 .. 6 steps in turn
 *   GAPS      16ths but a Euclidean rhythm's steps */
enum { SH_FOUR, SH_EUCLID, SH_ADD, SH_CELL, SH_CLUSTER, SH_ACCEL, SH_DECEL, SH_SPARSE, SH_MIRROR, SH_ANSWER, SH_BURST,
       SH_STRAIGHT, SH_OFFBEAT, SH_WALK, SH_GAPS, NSH };
/* each role's shapes (ten picks: one twice, twice as likely) and its Euclidean hits (lo .. hi) */
static const uint8_t RSH[NRO][10] = {
    {SH_FOUR, SH_FOUR, SH_EUCLID, SH_EUCLID, SH_SPARSE, SH_ADD, SH_ADD, SH_CLUSTER, SH_ACCEL, SH_WALK},          /* KICK */
    {SH_EUCLID, SH_EUCLID, SH_CLUSTER, SH_SPARSE, SH_WALK, SH_ACCEL, SH_DECEL, SH_ADD, SH_CELL, SH_MIRROR},      /* SNARE */
    {SH_STRAIGHT, SH_OFFBEAT, SH_EUCLID, SH_EUCLID, SH_CELL, SH_CELL, SH_WALK, SH_ACCEL, SH_MIRROR, SH_CLUSTER}, /* HAT */
    {SH_EUCLID, SH_EUCLID, SH_ADD, SH_CELL, SH_CLUSTER, SH_MIRROR, SH_ANSWER, SH_WALK, SH_SPARSE, SH_DECEL},     /* PERC */
    {SH_BURST, SH_BURST, SH_CLUSTER, SH_CLUSTER, SH_CELL, SH_ACCEL, SH_DECEL, SH_SPARSE, SH_STRAIGHT, SH_WALK},  /* NOISE */
    {SH_EUCLID, SH_EUCLID, SH_GAPS, SH_GAPS, SH_ADD, SH_CELL, SH_ANSWER, SH_STRAIGHT, SH_OFFBEAT, SH_SPARSE},    /* BASS */
    {SH_EUCLID, SH_EUCLID, SH_ADD, SH_CELL, SH_ANSWER, SH_ANSWER, SH_MIRROR, SH_SPARSE, SH_WALK, SH_CLUSTER},    /* LEAD */
    {SH_EUCLID, SH_ADD, SH_ADD, SH_CELL, SH_ANSWER, SH_SPARSE, SH_OFFBEAT, SH_CLUSTER, SH_MIRROR, SH_DECEL},     /* STAB */
    {SH_SPARSE, SH_SPARSE, SH_SPARSE, SH_SPARSE, SH_SPARSE, SH_SPARSE, SH_SPARSE, SH_SPARSE, SH_SPARSE, SH_SPARSE},
};
static const uint8_t REK[NRO][2] = {{3, 7}, {2, 5}, {7, 13}, {3, 7}, {3, 9}, {5, 9}, {3, 6}, {3, 5}, {1, 2}};

static uint32_t ph_rot(uint32_t m, uint32_t r)  /* (turned r steps later) */
{
    r %= NSTEP;
    return r ? ((m << r) | (m >> (NSTEP - r))) & 0xFFFFu : m;
}
static uint32_t ph_nbits(uint32_t m)
{
    uint32_t n = 0;
    for (; m; m &= m - 1u)
        n++;
    return n;
}
static uint32_t ph_hits(uint32_t lo, uint32_t hi, uint32_t n)   /* (n hits in lo .. hi, the first on lo) */
{
    uint32_t m = 1u << lo, k;
    for (k = 1; k < n; k++)
        m |= 1u << ph_ri((int32_t)lo + 1, (int32_t)hi);
    return m;
}
static uint32_t ph_add(uint32_t *acc)
{
    static const uint8_t GW[3] = {2, 3, 4};
    uint32_t r = NSTEP, at = 0, m = 0, g, k, big = ph_rnd(3);
    while (r) {
        g = ph_ch(55) ? GW[big] : GW[ph_rnd(3)];
        for (k = 0; k < 4u && (g > r || r - g == 1u); k++)
            g = GW[ph_rnd(3)];
        if (g > r || r - g == 1u)
            g = r <= 4u ? r : 2u;
        m |= 1u << at;
        if (!at || g != GW[big] || ph_ch(30))    /* (the odd groups' starts are the accents) */
            *acc |= 1u << at;
        at += g;
        r -= g;
    }
    return m;
}
static uint32_t ph_cell(uint32_t *acc)
{
    static const uint8_t PER[6] = {3, 5, 6, 7, 3, 5};
    uint32_t p = PER[ph_rnd(6)], c = ph_hits(0, p - 1u, (uint32_t)ph_ri(1, (int32_t)(p + 1u) / 2)), i, m = 0,
             per = ph_ch(30) ? 8u : NSTEP;
    for (i = 0; i < NSTEP; i++) {
        uint32_t j = (i % per) % p;
        if (c >> j & 1u) {
            m |= 1u << i;
            if (!j)
                *acc |= 1u << i;
        }
    }
    return m;
}
static uint32_t ph_shape(uint32_t sh, uint32_t role, uint32_t *acc)
{
    static const uint8_t GAP[8] = {1, 2, 2, 3, 3, 4, 5, 6};
    uint32_t m = 0, i, k, p, g, turn = ph_ch(25);
    *acc = 0;
    switch (sh) {
    case SH_FOUR:
        m = 0x1111u | (ph_ch(60) ? 1u << (ph_ch(50) ? 14 : 15) : 0u) | (ph_ch(40) ? 1u << (ph_ch(50) ? 6 : 10) : 0u);
        turn = 0;
        break;
    case SH_EUCLID:
        m = ph_euclid((uint32_t)ph_ri(REK[role][0], REK[role][1]), ph_rnd(NSTEP));
        turn = 0;
        break;
    case SH_ADD: m = ph_add(acc); break;
    case SH_CELL: m = ph_cell(acc); break;
    case SH_CLUSTER:
        for (k = (uint32_t)ph_ri(2, 4); k; k--) {
            p = ph_rnd(NSTEP);
            *acc |= 1u << p;
            for (g = (uint32_t)ph_ri(2, 4); g; g--, p = (p + 1u) % NSTEP)
                m |= 1u << p;
        }
        break;
    case SH_ACCEL:
    case SH_DECEL:
        for (p = 0, g = (uint32_t)ph_ri(4, 7); p < NSTEP; p += g, g -= g > 1u && ph_ch(75))
            m |= 1u << p;
        if (sh == SH_DECEL) {                    /* (the same backwards) */
            for (i = 0, k = 0; i < NSTEP; i++)
                k |= (m >> i & 1u) << (NSTEP - 1u - i);
            m = k;
        }
        *acc = m & (0u - m);                     /* (its first) */
        break;
    case SH_SPARSE:
        m = ph_hits(ph_rnd(NSTEP), NSTEP - 1u, (uint32_t)ph_ri(1, 3));
        turn = 0;
        break;
    case SH_MIRROR:
        k = ph_hits(ph_rnd(3), 7, (uint32_t)ph_ri(2, 4));
        for (i = 0, m = k; i < 8u; i++)
            m |= (k >> i & 1u) << (15u - i);
        for (i = 7u; !(k >> i & 1u); i--)
            ;
        *acc = (k & (0u - k)) | 0x8000u >> i;   /* (each half's first) */
        break;
    case SH_ANSWER:
        k = ph_hits(0, 7, (uint32_t)ph_ri(2, 4));
        g = ph_ch(50) ? ((k << 1 | k >> 7) & 0xFFu)                      /* (turned a step) */
                      : ((k & ~(1u << ph_rnd(8))) | 1u << ph_rnd(8));   /* (a hit moved) */
        m = k | (g ? g : k) << 8;
        *acc = 1u | (m & 0x0100u);
        break;
    case SH_BURST:
        i = p = ph_ch(50) ? 0u : ph_rnd(NSTEP);
        for (g = (uint32_t)ph_ri(4, 8), k = ph_ch(30) ? 2u : 1u; g; g--, p += k)
            m |= 1u << (p % NSTEP);
        m |= ph_ch(60) ? 1u << ph_rnd(NSTEP) : 0u;
        *acc = 1u << i | (m & 0x1111u);
        turn = 0;
        break;
    case SH_STRAIGHT:
        m = 0xFFFFu;
        for (k = (uint32_t)ph_ri(0, 4); k; k--)
            m &= ~(1u << ph_rnd(NSTEP));
        turn = 0;
        break;
    case SH_OFFBEAT:
        m = ph_ch(60) ? 0x4444u : ph_ch(50) ? 0xAAAAu : 0x2222u;
        m |= ph_ch(50) ? ph_euclid((uint32_t)ph_ri(1, 4), ph_rnd(NSTEP)) : 0u;
        *acc = m & 0x4444u;
        turn = 0;
        break;
    case SH_WALK:
        for (p = ph_rnd(3); p < NSTEP; p += GAP[ph_rnd(role == RO_HAT || role == RO_NOISE ? 5u : 8u)])
            m |= 1u << p;
        break;
    default:                                     /* GAPS */
        m = 0xFFFFu & ~ph_euclid((uint32_t)ph_ri(4, 8), ph_rnd(NSTEP));
        turn = 0;
        break;
    }
    if (!*acc)
        *acc = m & 0x1111u;
    if (turn) {
        k = ph_rnd(NSTEP);
        m = ph_rot(m, k);
        *acc = ph_rot(*acc, k);
    }
    return m & 0xFFFFu;
}
/* the steps the other lanes play (those whose sound's role is in rm: bits by RO_*) */
static uint32_t ph_others(uint32_t rm)
{
    uint32_t l, i, m = 0;
    for (l = 0; l < NLANE; l++)
        if (l != ph_t && ln_on[l] && (rm >> phrase_role(lane_snd(l)) & 1u))
            for (i = 0; i < NSTEP; i++)
                m |= (seq_note[l][i] != 0) << i;
    return m;
}
#define RB(r) (1u << (r))
/* a role's shape, at random; then, now and then, fitted to the other tracks: in the kick's and snare's gaps, a bass
 * on the kick's steps (sh: the shape's) */
static uint32_t ph_pick(uint32_t role, uint32_t *acc, uint32_t *sh)
{
    uint32_t m, o;
    *sh = RSH[role][ph_rnd(10)];
    m = ph_shape(*sh, role, acc);
    switch (role) {
    case RO_KICK:
        if (ph_ch(75))
            m |= 1u, *acc |= 1u;                 /* (the one, mostly) */
        if (ph_ch(40))
            m &= ~(ph_others(RB(RO_SNARE)) & ~1u);
        break;
    case RO_HAT:
        if (ph_ch(25))
            m &= ~ph_others(RB(RO_KICK));
        break;
    case RO_PERC:
    case RO_NOISE:
        if (ph_ch(35)) {
            o = ph_others(RB(RO_KICK) | RB(RO_SNARE));
            m &= ~o;
            if (ph_nbits(m) < 3u)
                m |= ph_euclid((uint32_t)ph_ri(3, 6), ph_rnd(NSTEP)) & ~o;
        }
        break;
    case RO_BASS:
        o = ph_others(RB(RO_KICK));
        if (o && ph_ch(20))
            m = o | (m & ph_euclid((uint32_t)ph_ri(1, 3), ph_rnd(NSTEP))), *acc = o;   /* (with the kick) */
        else if (ph_ch(20))
            m &= ~o;                             /* (between its kicks) */
        break;
    case RO_LEAD:
    case RO_STAB:
        if (ph_ch(20))
            m &= ~ph_others(RB(RO_KICK) | RB(RO_SNARE));
        break;
    default: break;
    }
    if (!(m & 0xFFFFu))
        m = 1u << ph_rnd(NSTEP);
    *acc &= m;
    return m & 0xFFFFu;
}

/* -------------------------------------------------------------- the roles --- */
static void ph_kick(void)
{
    uint32_t i, acc, sh, m = ph_pick(RO_KICK, &acc, &sh);
    for (i = 0; i < NSTEP; i++) {
        int on1 = (int)(acc >> i & 1u);
        if (!(m >> i & 1u))
            continue;
        ph_step(i, on1 ? 0 : (ph_ch(25) ? -1 : 0), on1 ? ph_ri(110, 127) : ph_ri(60, 95), (uint32_t)ph_ri(1, 5),
                on1 || sh == SH_FOUR || ph_ch(45) ? CD_NONE : ph_cond_thin());
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
/* the snare's feel: a backbeat (steps 5 and 13), displaced, half time (9), double (3, 7, 11, 15), broken (groups of
 * 2, 3, 4), or none: a shape of its own */
static void ph_snare(void)
{
    uint32_t i, f = ph_rnd(10), acc = 0, sh, m, g;
    if (f < 3)
        m = 1u << 4 | 1u << 12;
    else if (f < 4)
        m = 1u << (ph_ch(50) ? 4 : 3) | 1u << (ph_ch(50) ? 11 : 13);
    else if (f < 5)
        m = 1u << 8 | (ph_ch(30) ? 1u << 15 : 0u);
    else if (f < 6)
        m = 0x4444u;
    else if (f < 7) {
        m = ph_add(&acc) & ~1u;
        m = acc & m ? acc & m : m;
    } else
        m = ph_pick(RO_SNARE, &acc, &sh);
    g = ph_ch(60) ? ph_euclid((uint32_t)ph_ri(2, 5), ph_rnd(NSTEP)) : ph_shape(ph_ch(50) ? SH_CELL : SH_CLUSTER,
                                                                            RO_SNARE, &acc);
    g &= ~m;
    if (ph_nbits(g) > 6u)
        g &= ph_euclid(8, ph_rnd(2));            /* (the ghosts not too many) */
    if (ph_ch(40))
        g &= ~ph_others(RB(RO_KICK));
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
    if (ph_ch(40)) {                             /* a roll into the next bar, every other time: rising or falling */
        uint32_t from = (uint32_t)ph_ri(11, 14), c = ph_ab(2, 2, 0), up = ph_ch(70), n = NSTEP - from;
        for (i = from; i < NSTEP; i++) {
            uint32_t k = up ? i - from : NSTEP - 1u - i;
            ph_step(i, (int32_t)k, 60 + (int32_t)(k * 60u / n), 1, c);
            ph_lock(i, LK_FREQ, ph_base(LK_FREQ) - 20 + (int32_t)k * 15);
            ph_move(i, LK_ADEC, -50, -20);
        }
    }
}
static void ph_hat(void)
{
    uint32_t i, acc, sh, m = ph_pick(RO_HAT, &acc, &sh), pan = (uint32_t)ph_ri(30, 98);
    if (sh == SH_STRAIGHT || sh == SH_EUCLID || sh == SH_WALK)
        acc = m & 0x4444u;                       /* (the offbeats) */
    for (i = 0; i < NSTEP; i++) {
        int a = (int)(acc >> i & 1u);
        if (!(m >> i & 1u))
            continue;
        ph_step(i, ph_ch(15) ? ph_ri(-1, 1) : 0, a ? ph_ri(95, 127) : ph_ri(45, 85), a && ph_ch(30) ? 6u : 1u,
                a || ph_ch(40) ? CD_NONE : ph_prob(50, 95));
        pan = (uint32_t)clampi((int32_t)pan + ph_ri(-18, 18), 10, 117);   /* (the pan wanders) */
        if (ph_ch(60))
            ph_lock(i, LK_PAN, (int32_t)pan);
        if (a && ph_ch(40))
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
    uint32_t i, acc, sh, m = ph_pick(RO_PERC, &acc, &sh), pair = ph_ch(60);
    for (i = 0; i < NSTEP; i++) {
        uint32_t cond = CD_NONE;
        if (!(m >> i & 1u))
            continue;
        if (pair && ph_ch(40) && !(m >> ((i + 1u) % NSTEP) & 1u)) {   /* one of two: a 50 % step, !PRE after it */
            cond = ph_prob(50, 50);
            ph_step(i + 1u, ph_ri(-3, 5), ph_ri(60, 110), (uint32_t)ph_ri(0, 4), CD_NPRE);
            ph_lock(i + 1u, LK_PAN, ph_ri(0, 127));
        } else if (!(acc >> i & 1u) && ph_ch(35))
            cond = ph_cond_thin();
        ph_step(i, ph_ri(-3, 5), acc >> i & 1u ? ph_ri(95, 125) : ph_ri(60, 110), (uint32_t)ph_ri(0, 5), cond);
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
    uint32_t i, acc, sh, m = ph_pick(RO_NOISE, &acc, &sh);
    for (i = 0; i < NSTEP; i++)
        if (m >> i & 1u)
            ph_step(i, ph_ri(-2, 4), acc >> i & 1u ? ph_ri(100, 127) : ph_ri(70, 120),
                    (uint32_t)(ph_ch(30) ? ph_ri(6, 9) : ph_ri(0, 3)), ph_ch(40) ? ph_prob(30, 75) : CD_NONE);
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
    uint32_t i, acc, sh, m = ph_pick(RO_BASS, &acc, &sh), legato = ph_ch(25);
    deg[0] = deg[1] = deg[2] = 0;
    deg[3] = -n;                                 /* (the octave below) */
    deg[4] = n;
    deg[5] = ph_ri(1, n - 1);
    for (i = 0; i < NSTEP; i++) {
        int a;
        if (!(m >> i & 1u))
            continue;
        a = (int)(acc >> i & 1u) || ph_ch(20);
        ph_step(i, deg[ph_rnd(6)], a ? ph_ri(100, 127) : ph_ri(55, 90),
                legato ? (uint32_t)ph_ri(5, 8) : (uint32_t)ph_ri(1, 5), a || ph_ch(70) ? CD_NONE : ph_cond_thin());
        ph_move(i, LK_FREQ, a ? 0 : -30, a ? 35 : 5);   /* (the filter, step by step) */
        if (a && ph_ch(50))
            ph_move(i, LK_RESO, 10, 40);
        if (a && ph_ch(40))
            ph_move(i, LK_OD, 15, 55);
        if (ph_ch(25))
            ph_move(i, LK_FENV, -20, 30);
    }
}
static void ph_lead(int stab)
{
    int32_t d = 0;
    uint32_t i, acc, sh, m = ph_pick(stab ? RO_STAB : RO_LEAD, &acc, &sh);
    if (stab && sh == SH_EUCLID && ph_ch(50))
        m |= 0x0404u;
    for (i = 0; i < NSTEP; i++) {
        if (!(m >> i & 1u))
            continue;
        d = stab ? (ph_ch(70) ? 0 : ph_ri(-2, 2)) : clampi(d + ph_ri(-3, 3), -5, 9);   /* (a walk in the scale) */
        ph_step(i, d, acc >> i & 1u ? ph_ri(95, 125) : ph_ri(70, 110),
                stab ? (uint32_t)ph_ri(1, 4) : (uint32_t)ph_ri(4, 9),
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
 * in LK_* order: FREQ RESO ENV DECAY VOL DRIVE BITS SRR DELAY REVERB CHORUS PAN DESTROY SLICE); the notes, velocities,
 * lengths
 * and conditions as they were */
static const uint8_t RL_P[NRO][NLK] = {
    {25, 0, 10, 30, 0, 35, 20, 10, 0, 10, 0, 0, 10, 0},        /* KICK */
    {25, 10, 10, 25, 10, 20, 25, 30, 20, 25, 0, 30, 15, 5},    /* SNARE */
    {35, 10, 0, 35, 15, 0, 12, 10, 10, 5, 0, 60, 15, 10},       /* HAT */
    {50, 20, 10, 20, 15, 15, 25, 30, 30, 15, 10, 90, 25, 10},   /* PERC */
    {70, 40, 20, 20, 20, 40, 45, 50, 25, 15, 10, 90, 45, 25},   /* NOISE */
    {90, 40, 25, 15, 0, 35, 10, 5, 5, 5, 0, 0, 10, 15},         /* BASS */
    {40, 15, 15, 10, 0, 10, 10, 5, 35, 20, 15, 30, 10, 15},     /* LEAD */
    {40, 10, 10, 15, 0, 15, 20, 10, 35, 20, 10, 25, 15, 20},    /* STAB */
    {30, 10, 10, 0, 0, 5, 5, 5, 10, 50, 40, 20, 5, 40},        /* DRONE */
};
/* a lock of target l on step i, as its role's would be (acc: on the beat; pan: the walk the pans take) */
static void ph_lockval(uint32_t i, uint32_t l, int acc, int32_t *pan)
{
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
    case LK_DST: ph_lock(i, l, ph_ri(20, 110)); break;
    case LK_SLICE: ph_lock(i, l, ph_ri(1, 16)); break;
    default:                                     /* (the pan: hard sides now and then, else a walk) */
        *pan = ph_ch(25) ? (ph_ch(50) ? 0 : 127) : clampi(*pan + ph_ri(-30, 30), 0, 127);
        ph_lock(i, l, *pan);
        break;
    }
}
/* over the pattern's room: track t's locks go, at random; then the other tracks'; then t's softest notes */
static void ph_trim(uint32_t t)
{
    while (pat_bytes() > SEQ_PX) {
        uint32_t u, i, j, k = ph_rnd(NSTEP * NPL), found = 0;
        for (u = 0; u < NLANE && !found; u++) {  /* (every lane: the subtracks' too) */
            uint32_t tt = (t + u) % NLANE;
            for (j = 0; j < NSTEP * NPL && !found; j++) {
                uint32_t q = (k + j) % (NSTEP * NPL);
                if (seq_lid[tt][q / NPL][q % NPL] != PL_NONE) {
                    seq_lid[tt][q / NPL][q % NPL] = PL_NONE;
                    found = 1;
                }
            }
        }
        if (found)
            continue;
        for (i = 0, k = NSTEP; i < NSTEP; i++)
            if (seq_note[t][i] && (k == NSTEP || seq_vel[t][i] < seq_vel[t][k]))
                k = i;
        if (k == NSTEP)
            break;
        seq_note[t][k] = 0;
    }
}
/* COMPLEXITY's share of a chance: x 0 at 0, as drawn at 50, x 2 at 100 */
static uint32_t ph_cxp(uint32_t p, uint32_t cx) { return p * cx / 50u; }
static void phrase_relock(uint32_t t)
{
    uint32_t i, l, role, cx = ui.gen_cx[t];
    int32_t pan = 64;
    ph_seed = next_seed();
    ph_t = t;
    ph_s = lane_snd(t);
    role = phrase_role(ph_s);
    for (i = 0; i < NSTEP; i++) {
        pl_clear(t, i);
        if (!seq_note[t][i])
            continue;
        for (l = 0; l < NLK; l++)
            if (ph_ch(ph_cxp(RL_P[role][l], cx)))
                ph_lockval(i, l, (i & 3u) == 0u, &pan);
    }
    ph_trim(t);
    pat_dirty = 1;
}

/* DENSITY, after the role's phrase (50: as it drew it): below, steps thinned out (the strong ones -- on the beat,
 * loud -- last); above, ghosts added between them: soft, short, often on a condition, near the notes around them */
static void ph_density(uint32_t role, uint32_t den)
{
    uint32_t i;
    int32_t last = ph_ref;                       /* (the note before: a ghost lands near it) */
    if (den < 50u) {
        uint32_t p = (50u - den) * 9u / 5u;      /* (0: 90 % of them gone) */
        for (i = 0; i < NSTEP; i++) {
            int strong = (i & 3u) == 0u && seq_vel[ph_t][i] >= 100u;
            if (seq_note[ph_t][i] && ph_ch(strong ? p / 3u : p)) {
                seq_note[ph_t][i] = 0;
                pl_clear(ph_t, i);
            }
        }
    } else if (den > 50u && role != RO_DRONE) {
        uint32_t p = (den - 50u) * 7u / 5u;      /* (100: 70 % of the empty steps) */
        for (i = 0; i < NSTEP; i++) {
            if (seq_note[ph_t][i]) {
                last = seq_note[ph_t][i];
                continue;
            }
            if (!ph_ch(p))
                continue;
            ph_step(i, 0, ph_ri(35, 75), (uint32_t)ph_ri(0, 2), ph_ch(40) ? ph_cond_thin() : CD_NONE);
            if (role > RO_PERC || ph_ch(20))     /* (drums mostly on their own note; the rest near the note before) */
                seq_note[ph_t][i] = (uint8_t)scale_move(role > RO_PERC ? last : ph_ref, ph_ri(-1, 1));
        }
    }
}
/* COMPLEXITY, after it (50: as the role drew it): below, locks taken away; above, more of them, as its role locks;
 * and from 40 up, ratchets now and then -- rolls on hats, snares, percussion and noise most */
static void ph_complexity(uint32_t role, uint32_t cx)
{
    static const uint8_t RT[NRO] = {15, 40, 50, 40, 60, 20, 10, 20, 0};   /* (a role's ratchets, at 100, percent) */
    uint32_t i, l, j;
    int32_t pan = 64;
    for (i = 0; i < NSTEP; i++) {
        if (!seq_note[ph_t][i])
            continue;
        if (cx < 50u) {
            for (j = 0; j < NPL; j++)
                if (seq_lid[ph_t][i][j] != PL_NONE && ph_ch((50u - cx) * 2u))
                    seq_lid[ph_t][i][j] = PL_NONE;
        } else if (cx > 50u)
            for (l = 0; l < NLK; l++)
                if (ph_ch(RL_P[role][l] * (cx - 50u) / 50u))
                    ph_lockval(i, l, (i & 3u) == 0u, &pan);
        if (cx > 50u && role != RO_DRONE && ph_ch((cx - 50u) * 3u / 10u)) {   /* (MUTATE: now and then, every few
                                                                             * times) */
            seq_mut[ph_t][i] = (int8_t)(ph_ch(60) ? MUT_RND : ph_ri(-3, 3));
            seq_mcy[ph_t][i] = (uint8_t)ph_ri(1, 3);
        }
        if (cx > 50u && ph_ch((cx - 50u) * 3u / 10u)) {   /* (a MIDI FX: now and then, every few times) */
            static const uint8_t FXP[NRO][4] = {{MFX_FLAM, MFX_STUT, MFX_DROP, MFX_LATE},
                                                {MFX_FLAM, MFX_STUT, MFX_ECHO, MFX_DROP},
                                                {MFX_STUT, MFX_DROP, MFX_FLAM, MFX_LATE},
                                                {MFX_ECHO, MFX_STUT, MFX_GRACE, MFX_DROP},
                                                {MFX_STUT, MFX_ECHODN, MFX_DROP, MFX_FLAM},
                                                {MFX_OCTDN, MFX_GRACE, MFX_LATE, MFX_ECHO},
                                                {MFX_ECHOUP, MFX_FIFTH, MFX_GRACE, MFX_ECHODN},
                                                {MFX_TRIAD, MFX_FIFTH, MFX_ECHO, MFX_OCTUP},
                                                {MFX_FIFTH, MFX_OCTDN, MFX_ECHO, MFX_TRIAD}};
            seq_mfx[ph_t][i] = FXP[role][ph_rnd(4)];
            seq_fcy[ph_t][i] = (uint8_t)ph_ri(1, 3);
        }
        if (cx > 40u && ph_ch(RT[role] * (cx - 40u) / 60u)) {
            static const uint8_t PICK[8] = {1, 1, 2, 2, 3, 3, 4, 5};   /* (2 and 3 most, 6 and 8 rarely) */
            seq_rtg[ph_t][i] = PICK[ph_rnd(role == RO_NOISE || role == RO_HAT ? 8 : 6)];
        }
    }
}

/* (build 94) the bar's velocities, now and then shaped: rising to the bar's end, falling, every other hit softer,
 * or loose (each a little off) */
static void ph_contour(void)
{
    uint32_t i, n = 0, c = ph_rnd(10);
    if (c >= 4u)
        return;
    for (i = 0; i < NSTEP; i++) {
        int32_t v = seq_vel[ph_t][i];
        if (!seq_note[ph_t][i])
            continue;
        v = c == 0u ? v * (55 + (int32_t)i * 3) / 100 : c == 1u ? v * (100 - (int32_t)i * 3) / 100
            : c == 2u ? (n++ & 1u ? v * 65 / 100 : v) : v + ph_ri(-18, 12);
        seq_vel[ph_t][i] = (uint8_t)clampi(v, 20, 127);
    }
}
/* a step added at i like the one played before it (its note, length; a little softer; a pitched role's note moved
 * a step of the scale now and then), on condition cond */
static void ph_like(uint32_t i, uint32_t cond, uint32_t role, int32_t dv)
{
    uint32_t j = i, k;
    for (k = 1; k < NSTEP; k++) {
        j = (i + NSTEP - k) % NSTEP;
        if (seq_note[ph_t][j])
            break;
    }
    if (k == NSTEP || seq_note[ph_t][i])
        return;
    seq_note[ph_t][i] = (uint8_t)(role >= RO_BASS && ph_ch(40) ? scale_move(seq_note[ph_t][j], ph_ri(-2, 2))
                                                                : seq_note[ph_t][j]);
    seq_vel[ph_t][i] = (uint8_t)clampi((int32_t)seq_vel[ph_t][j] + dv, 25, 127);
    seq_glen[ph_t][i] = seq_glen[ph_t][j];
    seq_cond[ph_t][i] = (uint8_t)cond;
}
/* (build 94) the phrase's form over bars: as drawn (one bar over and over), or
 *   a fill    every 4th time (or 8th, or 2nd) the bar's end is another: a run, every other step, hits drawn at random,
 *             or silence
 *   2 or 3    bars: the bar's second (third) time through a variant -- turned a step or two, a few hits moved, or
 *             the role's shape drawn anew
 *   halves    every other time, the bar's second half its first again (or the first backwards)
 * the steps written by A:B: the variant's own on B:B, the bar's own missing from it on !B:B (a step with a
 * condition of its own keeps it) */
static void ph_form(uint32_t role)
{
    uint32_t i, a = 0, b, B, r = ph_rnd(100), from = 0, acc, sh;
    for (i = 0; i < NSTEP; i++)
        a |= (uint32_t)ph_on(i) << i;
    if (!a || r < 35u)
        return;
    if (r < 60u) {
        static const uint8_t FROM[6] = {8, 10, 12, 12, 13, 14};
        uint32_t f = ph_rnd(4);
        from = FROM[ph_rnd(6)];
        B = ph_ch(65) ? 4u : ph_ch(50) ? 8u : 2u;
        b = f == 0u ? 0xFFFFu : f == 1u ? 0x5555u << (from & 1u) : f == 2u ? ph_hits(from, NSTEP - 1u, 3) : 0u;
        b = (a & ((1u << from) - 1u)) | (b & (0xFFFFu << from));
    } else if (r < 85u) {
        uint32_t v = ph_rnd(3);
        B = ph_ch(65) ? 2u : 3u;
        if (v == 0u)
            b = ph_rot(a, (uint32_t)ph_ri(1, 2) * (ph_ch(50) ? 1u : NSTEP - 1u));
        else if (v == 1u)
            for (b = a, i = (uint32_t)ph_ri(1, 3); i; i--)
                b ^= 1u << ph_rnd(NSTEP);
        else
            b = ph_pick(role, &acc, &sh);
    } else {
        uint32_t h = a & 0xFFu, k;
        B = 2u;
        if (ph_ch(50))
            for (i = 0, k = h, h = 0; i < 8u; i++)
                h |= (k >> i & 1u) << (7u - i);
        b = (a & 0xFFu) | h << 8;
    }
    for (i = from; i < NSTEP; i++) {
        uint32_t ia = a >> i & 1u, ib = b >> i & 1u;
        if (ia && !ib && seq_cond[ph_t][i] == CD_NONE)
            seq_cond[ph_t][i] = (uint8_t)ph_ab(B, B, 1);
        else if (ib && !ia)
            ph_like(i, ph_ab(B, B, 0), role, from ? (int32_t)(i - from) * 6 - 10 : -12);
    }
}

/* the chosen track's phrase, new; the pattern changed */
static void phrase_new(uint32_t t)
{
    sound_t *s = lane_snd(t);
    uint32_t i, role;
    ph_seed = next_seed();
    ph_t = t;
    ph_s = s;
    ph_ref = ui.last_note[t] ? ui.last_note[t] : (int32_t)key_note(0, ui.oct[t]);
    role = phrase_role(s);
    steps_clear(t);                              /* (the phrase's notes: single) */
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
    if (role != RO_DRONE) {
        ph_contour();
        ph_form(role);
    }
    ph_density(role, ui.gen_den[t]);
    ph_complexity(role, ui.gen_cx[t]);
    for (i = 0; i < NSTEP; i++)                  /* (no locks left on a step that holds no note) */
        if (!seq_note[t][i])
            pl_clear(t, i);
    ph_trim(t);
    pat_dirty = 1;
}
