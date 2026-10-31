/* SPDX-License-Identifier: GPL-3.0-only
 * Sessions and their patterns, in the FM-1's flash (the user data area TONITOR kept its sessions in).
 *
 * 16 sessions, four 4 KB sectors each: the session (every track's machine, its sound on each machine, octave, level
 * and pan; the send effects; the tempo, root and scale), then its 16 patterns, six a sector (a pattern, 680 bytes:
 * each track's 16 steps and its length; its tempo, kept for later, GLOBAL -- the session's -- for now; from build 62
 * the tracks' state as it was saved -- muted, machine, octave, level, pan, sidechain, CHORD mode, arpeggiator --
 * given back when it plays).
 * The session playing is kept in RAM whole, its 16 patterns with it; the pattern playing is one of them, its steps
 * in the sequencer's (tracks.c) as they are edited.
 * (In the code a session is a "project".)
 *
 *   (the gestures as Tonitor's, build 76: its snapshots are the patterns here)
 *   SHIFT + SAVE      the SESSIONS page: SESSION 1 .. 16; PRESETS moves through them, SAVE opens one's page. On the
 *                     page SAVE runs LOAD, SAVE, COPY or DELETE, or moves the focus to a value: PRESETS changes it,
 *                     SAVE sets it, SHIFT + SAVE leaves it as it was; SHIFT + SAVE goes back a page (from the list:
 *                     out). SHIFT + SAVE held 0.8 s: GLOBAL SETUP (setup.c)
 *   SAVE              the PATTERNS grid, 1 .. 16 in four rows of four (SAVE again: out): PRESETS moves the cursor,
 *                     SAVE held saves the pattern playing into the slot under it. The white keys are the 16 patterns there, lit dimly (the one playing brighter): a white
 *                     key let go soon loads its pattern (the grid stays); held 0.8 s, the pattern playing saved there,
 *                     empty or not (build 72); SHIFT and held 0.8 s, copies it (COPY P5 above the grid, blinking, and
 *                     its key) -- then SHIFT and another held 0.8 s pastes it there, saved and loaded (the same again,
 *                     or off the grid: let go); SHIFT and the 1st black key held, pressed, empties it
 *   SAVE held, HOME held   the session playing saved, into its slot
 *   a track's button  back to playing
 * A session loads at the pattern it was saved playing; the start, at the last session's (or pattern 1).
 * LOAD with changes not saved, SAVE over another, DELETE, and COPY over another ask first: NO / YES on PRESETS, SAVE
 * answers. COPY asks for the slot to copy to (PRESETS), then SAVE.
 * A session's SAVE stores the pattern playing into its slot too, then the whole session -- its loops with it (in the
 * loop store, below), and the track chosen; LOAD gives all of it back, at the pattern it was saved playing.
 * The last session loaded or saved is noted (LST_SECT): the FM-1 starts with it.
 *
 * Each sector begins with a head: magic, format, what it is (the project, or which four patterns), the body's length
 * and CRC, the head's own CRC. A sector whose head does not check (empty, erased, another firmware's) is empty. */

#define PRJ_LO 0x00097000u
#define PRJ_SECT 4096u
#define PRJ_SLOTS 16u
#define PRJ_SECTS 4u             /* a project's sectors: itself, then its patterns six a sector */
#define PAT_SLOTS 16u
#define PAT_PER 6u
#define PAT_SECTS 3u             /* (the last holds four: the store is two longer, 18) */
#define PRJ_MAGIC 0x334E4742u   /* "BGN3" */
#define PAT_MAGIC 0x33504742u   /* "BGP3" */
#define PRJ_FORMAT 3u
#define TEMPO_MIN 10
#define TEMPO_MAX 300

typedef struct { uint32_t magic; uint16_t format, what; uint32_t len; uint16_t body_crc, head_crc; } prj_head_t;
/* the sound as builds 18 .. 49 saved it: no LFOs (prj_up makes a session of now of it) */
typedef struct {
    uint8_t mach, fmach;
    int16_t syn[32];
    uint8_t flt[NF], amp[NA], fx[NX], family;
    uint8_t mg[NM];
    uint32_t seed;
} sound49_t;
typedef char snd49_fits[__builtin_offsetof(sound_t, lfo) == sizeof(sound49_t) ? 1 : -1];   /* (sound_t: sound49_t, then
                                                                                          * the LFOs) */
/* a session as builds 18 .. 49 saved it (their lengths below, each of its builds') */
typedef struct {
    uint16_t tempo;
    uint8_t root, scale;
    uint8_t eng[NTRACK];
    int8_t oct[NTRACK], level[NTRACK], pan[NTRACK];
    uint8_t dl[NDL], rv[NRV], ch[NCH];
    uint8_t pat;
    sound49_t snd[NTRACK][NENG];
    uint8_t cp[NCP];
    uint8_t sc_src[NTRACK], sc_lvl[NTRACK];
    uint8_t chord_on[NTRACK], chord_form[NTRACK];
    uint8_t mute[NTRACK];
    uint8_t sel;
    uint8_t loop_nsec;
    uint32_t loop_gen;
} prj_body49_t;
typedef struct {
    uint16_t tempo;
    uint8_t root, scale;
    uint8_t eng[NTRACK];
    int8_t oct[NTRACK], level[NTRACK], pan[NTRACK];
    uint8_t dl[NDL], rv[NRV], ch[NCH];
    uint8_t pat;                 /* the pattern playing as it was saved */
    sound_t snd[NTRACK][NENG];
    uint8_t cp[NCP];             /* the master compressor (from build 24: sessions before end at snd, PRJ_LEN23) */
    uint8_t sc_src[NTRACK], sc_lvl[NTRACK];   /* each track's sidechain (from build 29: before, PRJ_LEN28) */
    uint8_t chord_on[NTRACK], chord_form[NTRACK];   /* each track's CHORD mode and form (from build 33: PRJ_LEN32) */
    uint8_t mute[NTRACK];        /* each track muted (from build 35: before, PRJ_LEN34) */
    uint8_t sel;                 /* the track chosen (from build 38: before, PRJ_LEN37) */
    uint8_t loop_nsec;           /* the loops: their sectors in the loop store (0: none) .. */
    uint32_t loop_gen;           /* .. and the generation they were written with */
    uint8_t arp_on[NTRACK];      /* each track's arpeggiator on (from build 52: before, PRJ_LEN50) .. */
    uint8_t arp[NTRACK][NARP];   /* .. and its settings (tracks.c's AR_*) */
} prj_body_t;
#define PRJ_LEN23 __builtin_offsetof(prj_body49_t, cp)   /* (the lengths builds 18 .. 49 saved: the sound without LFOs) */
#define PRJ_LEN28 __builtin_offsetof(prj_body49_t, sc_src)
#define PRJ_LEN32 __builtin_offsetof(prj_body49_t, chord_on)
#define PRJ_LEN34 __builtin_offsetof(prj_body49_t, mute)
#define PRJ_LEN49 sizeof(prj_body49_t)
#define PRJ_LEN50 __builtin_offsetof(prj_body_t, arp_on)   /* (builds 50 and 51: the sounds with LFOs, no arpeggiator) */
typedef char prj_len50[PRJ_LEN50 % 4u == 0u ? 1 : -1];   /* (their size as saved: no padding after loop_gen) */
/* (builds 35 .. 37: the body ended at mute, and its size -- what they saved -- was rounded up to the struct's
 * alignment, 4: two bytes of padding past mute. Build 38 took the unrounded length and found none of their sessions
 * (build 40)) */
#define PRJ_LEN37 ((__builtin_offsetof(prj_body49_t, sel) + 3u) & ~3u)
typedef char prj_len37[PRJ_LEN37 == 2780u && PRJ_LEN34 == 2772u && PRJ_LEN32 == 2760u ? 1 : -1];   /* (as saved) */
static void prj_ar0(prj_body_t *b)               /* (a session from before build 52: the arpeggiators off) */
{
    uint32_t t;
    for (t = 0; t < NTRACK; t++) {
        b->arp_on[t] = 0;
        memcpy(b->arp[t], AR_DEF, NARP);
    }
}
static void prj_lp0(prj_body_t *b)               /* (a session from before build 38: track 1, no loops) */
{
    b->sel = 0;
    b->loop_nsec = 0;
    b->loop_gen = 0;
}
static void prj_mu0(prj_body_t *b)               /* (a session from before build 35: no track muted) */
{
    memset(b->mute, 0, NTRACK);
    prj_lp0(b);
}
static void prj_ch0(prj_body_t *b)               /* (a session from before build 33: CHORD mode off) */
{
    memset(b->chord_on, 0, NTRACK);
    memset(b->chord_form, 0, NTRACK);
    prj_mu0(b);
}
static void prj_sc0(prj_body_t *b)               /* (a session from before build 29: no track ducked) */
{
    memset(b->sc_src, 0, NTRACK);
    memset(b->sc_lvl, 64, NTRACK);
    prj_ch0(b);
}
typedef struct {
    uint8_t used;                /* saved */
    uint8_t st;                  /* PX_ON: the tracks' state at the end of seq (from build 62; 0 before) */
    uint16_t tempo;              /* 0: GLOBAL (the project's) */
    uint8_t seq[676];            /* the steps (below) */
} pat_t;
/* the steps, from build 20: seq[t] track t's length (0: 16; low 5 bits) and its clock (top 3 bits: (division + 3)
 * mod 6, so that 0 -- every pattern before build 19 -- is 1X); seq[6] SEQ_V4; seq[7] the records; seq[8 + t] track
 * t's direction, seq[14 + t] its phrase DENSITY, seq[20 + t] its COMPLEXITY; from seq[32], a record for each step that
 * holds a note: its place (16 t + i), its note, velocity, its length (low 4 bits) and ratchet (top 4), its condition,
 * the number of its locks (from build 23, its top bit: MOD's three bytes follow -- MUTATE, the two EVERYs (low 4
 * bits MUT, top 4 FX, each less one), MIDI FX), then each lock: its parameter (tracks.c's ids) and its value, low byte
 * first.
 * (Build 18 and 19, SEQ_V3: the records from seq[8], the length a byte of its own, no direction, settings, ratchet.)
 * (Builds 11 .. 17, SEQ_V2: the record's length shared a byte with the top of a mask of 12 locks -- the phrase
 * generator's, LK_* -- its low byte where the count is now, then a byte for each lock. Builds 9 and 10 kept every
 * step at a fixed place: SEQ_OLD_*) */
#define SEQ_LEN 0u
#define SEQ_FMT 6u
#define SEQ_V2 2u
#define SEQ_V3 3u
#define SEQ_V4 4u
#define SEQ_DIR 8u
#define SEQ_DEN 14u
#define SEQ_CX 20u
#define SEQ_TRN 26u              /* (build 51: seq[26 + t] track t's transposition, steps of the scale, signed -- 0 in
                                  * every pattern before: pat_encode clears them) */
#define SEQ_REC4 32u
#define SEQ_NREC 7u
#define SEQ_REC 8u
#define PAT_SEQ_ROOM 676u
/* (build 62) the tracks' state as the pattern was saved, at the end of seq (the records end before it, so that an
 * earlier build reads the steps as ever and leaves the state alone). As it plays (px_take), 13 bytes a track: muted,
 * CHORD mode on (bit 1), the arpeggiator on (bit 2), the chord's form (bits 3 .. 5); the machine (bits 0, 1), the
 * octave + 3 (bits 2 .. 4), the SC SOURCE (bits 5 .. 7); the level, the pan, the SC amount; the arpeggiator's
 * settings. Builds 62 and 63 kept it so, its last 78 bytes (st PX_ON1); from build 64 packed, 9 bytes a track, the
 * last 54 (st PX_ON: PX_W bits of each in turn, the pan + PAN_MAX), so that 96 steps of notes fit */
#define PX_ON1 1u
#define PX_ON 2u
#define PX_T 13u
#define PX_P 9u
#define SEQ_PX1 (PAT_SEQ_ROOM - NTRACK * PX_T)
#define SEQ_PX (PAT_SEQ_ROOM - NTRACK * PX_P)
static const uint8_t PX_W[PX_T] = {6, 8, 5, 6, 7, 4, 3, 2, 7, 1, 7, 7, 3};   /* (66 bits of 72) */
typedef char px_fits[NENG <= 4 && CHF_N <= 8 && NCP_SCS <= 8 && NARP == 8 && LEVEL_MAX < 32 && 2 * PAN_MAX < 64 &&
                     NAM <= 16 && NAR_RATE <= 8 && NAR_RNG <= 4 && NAR_BASS <= 8 && SEQ_REC4 + NTRACK * NSTEP * 6u <= SEQ_PX
                     ? 1 : -1];
#define SEQ_OLD_NOTE 8u          /* seq[8 + 16 t + i]: track t's step i, its note (0: empty) */
#define SEQ_OLD_VEL 104u         /* .. its velocity (0: VEL_DEF, as build 9 left it) */
#define SEQ_OLD_GLEN 200u        /* .. its length + 1 (0: GLEN_DEF) */
#define SEQ_OLD_COND 296u        /* .. its trig condition */
typedef char prj_fits[sizeof(prj_head_t) + sizeof(prj_body_t) <= PRJ_SECT ? 1 : -1];
/* a session as builds 7 .. 17 saved it: the sound without MANGLE (read, and made a session of now, by prj_read) */
typedef struct {
    uint8_t mach, fmach;
    int16_t syn[32];
    uint8_t flt[NF], amp[NA], fx[NX], family;
    uint32_t seed;
} sound17_t;
typedef struct {
    uint16_t tempo;
    uint8_t root, scale;
    uint8_t eng[NTRACK];
    int8_t oct[NTRACK], level[NTRACK], pan[NTRACK];
    uint8_t dl[NDL], rv[NRV], ch[NCH];
    uint8_t pat;
    sound17_t snd[NTRACK][NENG];
} prj_body17_t;
typedef char prj17_head[__builtin_offsetof(prj_body17_t, snd) == __builtin_offsetof(prj_body_t, snd) ? 1 : -1];
typedef char pat_fits[sizeof(prj_head_t) + PAT_PER * sizeof(pat_t) <= PRJ_SECT ? 1 : -1];

static int prj_flash_read(uint32_t off, void *p, uint32_t n);    /* (main.c: the area's own, bounded) */
static int prj_flash_erase(uint32_t off);
static int prj_flash_prog(uint32_t off, const void *p, uint32_t n);

static uint8_t prj_buf[PRJ_SECT] __attribute__((aligned(4)));
static uint8_t prj_has[PRJ_SLOTS];      /* a project in the slot */
static int8_t prj_cur = -1;             /* the slot of the project playing, -1 none (never saved) */
static uint8_t prj_dirty;               /* changes since it was loaded or saved */
static uint8_t prj_ok;                  /* the flash answered */
static pat_t pat_store[PAT_SECTS * PAT_PER];   /* the project playing's patterns (16, and two never used) */
static uint8_t pat_cur;                 /* the pattern playing */
static uint16_t pat_tempo;              /* .. its tempo as it plays (0: GLOBAL) */
static uint8_t pat_dirty;               /* .. changed since it was loaded or saved */

static uint32_t prj_sector(uint32_t k, uint32_t s) { return PRJ_LO + (k * PRJ_SECTS + s) * PRJ_SECT; }
static uint16_t prj_crc(const uint8_t *p, uint32_t n)   /* CRC16, poly 0x1021 */
{
    uint32_t c = 0xFFFFu, i, k;
    for (i = 0; i < n; i++) {
        c ^= (uint32_t)p[i] << 8;
        for (k = 0; k < 8u; k++)
            c = c & 0x8000u ? (c << 1) ^ 0x1021u : c << 1;
    }
    return (uint16_t)c;
}
static int prj_head_ok(const prj_head_t *h, uint32_t magic, uint32_t what, uint32_t len)
{
    return h->magic == magic && h->format == PRJ_FORMAT && h->what == what && h->len == len &&
           h->head_crc == prj_crc((const uint8_t *)h, __builtin_offsetof(prj_head_t, head_crc));
}
static void prj_head_fill(uint32_t magic, uint32_t what, uint32_t len)
{
    prj_head_t *h = (prj_head_t *)(void *)prj_buf;
    h->magic = magic;
    h->format = PRJ_FORMAT;
    h->what = (uint16_t)what;
    h->len = len;
    h->body_crc = prj_crc(prj_buf + sizeof(prj_head_t), len);
    h->head_crc = prj_crc(prj_buf, __builtin_offsetof(prj_head_t, head_crc));
}
/* sector s of project k into prj_buf; 0 if it is whole */
/* a session as builds 18 .. 49 saved it (len bytes of it), in prj_buf, made one of now in place (build 50): the part
 * after the sounds moved up first, then each sound to its new place, the last first (each lands at or after where it
 * was), its LFOs off; what a build before had not, its start values; its head as if saved so */
static void prj_up(uint32_t len)
{
    uint8_t *b = prj_buf + sizeof(prj_head_t);
    prj_body_t *nb = (prj_body_t *)(void *)b;
    const uint32_t so = __builtin_offsetof(prj_body_t, snd), o49 = PRJ_LEN23, o = __builtin_offsetof(prj_body_t, cp);
    int32_t n;
    if (len > o49)
        memmove(b + o, b + o49, len - o49);
    for (n = NTRACK * NENG - 1; n >= 0; n--) {
        memmove(b + so + (uint32_t)n * sizeof(sound_t), b + so + (uint32_t)n * sizeof(sound49_t), sizeof(sound49_t));
        lfos_off(&nb->snd[0][0] + n);
    }
    if (len <= PRJ_LEN23) {                      /* (builds 18 .. 23: no compressor -- its start values) */
        memcpy(nb->cp, CP0, NCP);
        prj_sc0(nb);
    } else if (len <= PRJ_LEN28)                 /* (builds 24 .. 28: no track's sidechain) */
        prj_sc0(nb);
    else if (len <= PRJ_LEN32)                   /* (builds 29 .. 32: no CHORD mode) */
        prj_ch0(nb);
    else if (len <= PRJ_LEN34)                   /* (builds 33 .. 34: no mutes) */
        prj_mu0(nb);
    else if (len <= PRJ_LEN37)                   /* (builds 35 .. 37: no loops) */
        prj_lp0(nb);
    prj_ar0(nb);                                 /* (every build before 52: no arpeggiator) */
    prj_head_fill(PRJ_MAGIC, ((prj_head_t *)(void *)prj_buf)->what, sizeof(prj_body_t));
}
/* a session saved before build 18, in prj_buf: each sound made one of builds 18 .. 49 in place (MANGLE all off), the
 * last first; then as theirs (prj_up) */
static void prj_from17(void)
{
    uint8_t *b = prj_buf + sizeof(prj_head_t);
    const sound17_t *o = (const sound17_t *)(const void *)(b + __builtin_offsetof(prj_body_t, snd));
    sound49_t *d0 = (sound49_t *)(void *)(b + __builtin_offsetof(prj_body_t, snd));
    int32_t n;
    for (n = NTRACK * NENG - 1; n >= 0; n--) {
        sound17_t a = o[n];
        sound49_t *d = d0 + n;
        memset(d, 0, sizeof *d);
        d->mach = a.mach;
        d->fmach = a.fmach;
        memcpy(d->syn, a.syn, sizeof d->syn);
        memcpy(d->flt, a.flt, NF);
        memcpy(d->amp, a.amp, NA);
        memcpy(d->fx, a.fx, NX);
        d->fx[X_ODT] = 0;
        d->family = a.family;
        d->seed = a.seed;
    }
    prj_up(PRJ_LEN23);
}
/* sector s of project k into prj_buf; 0 if it is whole (a session saved by an earlier build: made one of now) */
static int prj_read(uint32_t k, uint32_t s)
{
    static const uint16_t OLD[8] = {sizeof(prj_body17_t), PRJ_LEN23, PRJ_LEN28, PRJ_LEN32, PRJ_LEN34, PRJ_LEN37,
                                    PRJ_LEN49, PRJ_LEN50};
    uint32_t magic = s ? PAT_MAGIC : PRJ_MAGIC, len = s ? PAT_PER * sizeof(pat_t) : sizeof(prj_body_t), i;
    const prj_head_t *h = (const prj_head_t *)(void *)prj_buf;
    if (!prj_ok || k >= PRJ_SLOTS || prj_flash_read(prj_sector(k, s), prj_buf, sizeof(prj_head_t) + len))
        return -1;
    for (i = 0; !s && i < 8u; i++)
        if (prj_head_ok(h, magic, k * 16u, OLD[i]) && h->body_crc == prj_crc(prj_buf + sizeof(prj_head_t), OLD[i])) {
            if (i == 0u)
                prj_from17();
            else if (OLD[i] == PRJ_LEN50) {      /* (builds 50, 51: as now, the arpeggiators added) */
                prj_ar0((prj_body_t *)(void *)(prj_buf + sizeof(prj_head_t)));
                prj_head_fill(PRJ_MAGIC, h->what, sizeof(prj_body_t));
            }
            else
                prj_up(OLD[i]);
            return 0;
        }
    if (!prj_head_ok(h, magic, k * 16u + s, len) || h->body_crc != prj_crc(prj_buf + sizeof(prj_head_t), len))
        return -2;
    return 0;
}
/* prj_buf (its body filled) into sector s of project k: erased, written, read back */
static int prj_write(uint32_t k, uint32_t s)
{
    uint32_t len = s ? PAT_PER * sizeof(pat_t) : sizeof(prj_body_t);
    uint16_t c;
    if (!prj_ok || k >= PRJ_SLOTS)
        return -1;
    prj_head_fill(s ? PAT_MAGIC : PRJ_MAGIC, k * 16u + s, len);
    c = ((prj_head_t *)(void *)prj_buf)->body_crc;
    if (prj_flash_erase(prj_sector(k, s)) || prj_flash_prog(prj_sector(k, s), prj_buf, sizeof(prj_head_t) + len))
        return -2;
    if (prj_read(k, s) || ((prj_head_t *)(void *)prj_buf)->body_crc != c)
        return -3;
    return 0;
}
/* ------------------------------------------------------------ the loops --- */
/* The loop store (build 38): 31 sectors the FM-1's flash has free (beyond the sessions' 64, apart from the update's
 * staging area), shared by the sessions. A session's loops are a stream -- how many loops, then each: its track,
 * its number, muted, its length, its notes as they are in RAM -- cut into as many sectors as it takes (up to 7: 18
 * loops of 256 notes), each sector marked with the session's slot, its place in the stream, and a generation. A
 * sector is in use while a session in its slot holds that generation; any other is free. SAVE writes the loops into
 * free sectors first, then the session that points at them: a save cut off leaves the session before it whole. */
#define LOP_MAGIC 0x334C4742u   /* "BGL3" */
#define LST_MAGIC 0x31534742u   /* "BGS1": the last session */
#define LOP_N 31u
#define LST_SECT 0x000FC000u
typedef struct { uint32_t gen; uint8_t slot, idx, nsec, pad; uint32_t bytes; } lop_meta_t;
#define LOP_BODY (PRJ_SECT - (uint32_t)sizeof(prj_head_t))
#define LOP_DATA (LOP_BODY - (uint32_t)sizeof(lop_meta_t))
#define LOP_WHAT 0x1000u        /* (+ slot x 16 + its place) */
static uint32_t prj_lgen[PRJ_SLOTS];    /* each slot's session: its loops' generation (0: none) */
static uint32_t lop_gmax;               /* the highest generation in the store */
static int8_t lst_slot = -1;            /* the last session, as noted */
static uint32_t lop_sector(uint32_t i)  /* 0xD7000 .. 0xDF000, 0xE5000 .. 0xFA000 (not 0xE0000 .. 0xE4FFF: the
                                         * update's staging) */
{
    return i < 9u ? 0x000D7000u + i * PRJ_SECT : 0x000E5000u + (i - 9u) * PRJ_SECT;
}
static lop_meta_t *lop_meta(void) { return (lop_meta_t *)(void *)(prj_buf + sizeof(prj_head_t)); }
static uint8_t *lop_data(void) { return prj_buf + sizeof(prj_head_t) + sizeof(lop_meta_t); }
/* sector i's head and mark (not its body): 0 if it is a loop sector */
static int lop_peek(uint32_t i, lop_meta_t *m)
{
    struct { prj_head_t h; lop_meta_t m; } x;
    if (prj_flash_read(lop_sector(i), &x, sizeof x) || x.h.magic != LOP_MAGIC || x.h.format != PRJ_FORMAT ||
        x.h.len != LOP_BODY || x.h.head_crc != prj_crc((const uint8_t *)&x.h, __builtin_offsetof(prj_head_t, head_crc)) ||
        x.m.slot >= PRJ_SLOTS || x.h.what != LOP_WHAT + x.m.slot * 16u + x.m.idx)
        return -1;
    *m = x.m;
    return 0;
}
static int lop_live(uint32_t i)          /* in use: a session's */
{
    lop_meta_t m;
    return !lop_peek(i, &m) && prj_has[m.slot] && m.gen && prj_lgen[m.slot] == m.gen;
}
static int lop_read(uint32_t i)          /* sector i into prj_buf, whole */
{
    const prj_head_t *h = (const prj_head_t *)(void *)prj_buf;
    if (prj_flash_read(lop_sector(i), prj_buf, PRJ_SECT) || h->magic != LOP_MAGIC || h->len != LOP_BODY ||
        h->body_crc != prj_crc(prj_buf + sizeof(prj_head_t), LOP_BODY))
        return -1;
    return 0;
}
static int lop_write(uint32_t i)         /* prj_buf (its mark and data filled) into sector i: erased, written, read back */
{
    lop_meta_t m = *lop_meta();
    uint16_t c;
    prj_head_fill(LOP_MAGIC, LOP_WHAT + m.slot * 16u + m.idx, LOP_BODY);
    c = ((prj_head_t *)(void *)prj_buf)->body_crc;
    if (prj_flash_erase(lop_sector(i)) || prj_flash_prog(lop_sector(i), prj_buf, PRJ_SECT))
        return -2;
    if (lop_read(i) || ((prj_head_t *)(void *)prj_buf)->body_crc != c)
        return -3;
    return 0;
}
/* the free sectors for session k: nsec of them into map (k's own in use last, only if the others are too few) */
static int lop_alloc(uint32_t k, uint32_t nsec, uint8_t *map)
{
    uint32_t i, n = 0, pass;
    for (pass = 0; pass < 2u && n < nsec; pass++)
        for (i = 0; i < LOP_N && n < nsec; i++) {
            lop_meta_t m;
            int live = lop_live(i), own = live && !lop_peek(i, &m) && m.slot == k;
            if (pass == 0u ? !live : own)
                map[n++] = (uint8_t)i;
        }
    return n == nsec ? 0 : -4;
}
/* the stream, written: a byte at a time into prj_buf, a sector written as it fills */
static struct {
    uint32_t pos, j, nsec, slot, gen, bytes;
    uint8_t map[8];
    int err;
} lw;
static void lw_flush(void)
{
    lop_meta_t *m = lop_meta();
    if (lw.err || lw.j >= lw.nsec)
        return;
    m->gen = lw.gen;
    m->slot = (uint8_t)lw.slot;
    m->idx = (uint8_t)lw.j;
    m->nsec = (uint8_t)lw.nsec;
    m->pad = 0;
    m->bytes = lw.bytes;
    if (lop_write(lw.map[lw.j]))
        lw.err = -2;
    lw.j++;
    lw.pos = 0;
    memset(lop_data(), 0, LOP_DATA);
}
static void lw_put(const void *p, uint32_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--) {
        if (lw.pos == LOP_DATA)
            lw_flush();
        lop_data()[lw.pos++] = *b++;
    }
}
static uint32_t lop_bytes(void)          /* the stream's length (0: no loop) */
{
    uint32_t i, n = 0, b = 1;
    for (i = 0; i < NLP; i++)
        if (lps[i].pat != LP_FREE && lps[i].on && lps[i].n) {
            n++;
            b += 8u + lps[i].n * (uint32_t)sizeof(lev_t);
        }
    return n ? b : 0u;
}
/* (build 73) pattern k's transposition on track t (the one playing: as it plays) */
static int32_t pat_trn(uint32_t k, uint32_t t)
{
    const pat_t *p = &pat_store[k % PAT_SLOTS];
    if (k == pat_cur)
        return ui.trn[t];
    return p->used && p->seq[SEQ_FMT] == SEQ_V4 ? clampi((int8_t)p->seq[SEQ_TRN + t], -TRN_MAX, TRN_MAX) : 0;
}
/* every pattern's loops into the store, for session k: their generation and sectors (0, 0: none). Each loop: its
 * track, its place, muted, its pattern + 1 (build 73; 0 before: the session's, in every pattern), its length, its
 * notes as they play in its pattern */
static int lop_save(uint32_t k, uint32_t *gen, uint8_t *nsec)
{
    uint32_t i, bytes = lop_bytes();
    uint8_t cnt = 0;
    *gen = 0;
    *nsec = 0;
    if (!bytes)
        return 0;
    memset(&lw, 0, sizeof lw);
    lw.nsec = (bytes + LOP_DATA - 1u) / LOP_DATA;
    if (lw.nsec > sizeof lw.map || lop_alloc(k, lw.nsec, lw.map))
        return -4;
    lw.slot = k;
    lw.gen = ++lop_gmax;
    lw.bytes = bytes;
    memset(lop_data(), 0, LOP_DATA);
    for (i = 0; i < NLP; i++)
        cnt += (uint8_t)(lps[i].pat != LP_FREE && lps[i].on && lps[i].n);
    lw_put(&cnt, 1);
    for (i = 0; i < NLP; i++) {
        const loop_t *p = &lps[i];
        uint8_t h[8];
        if (p->pat == LP_FREE || !p->on || !p->n)
            continue;
        h[0] = p->t;
        h[1] = p->l;
        h[2] = p->mute;
        h[3] = (uint8_t)(p->pat + 1u);
        h[4] = (uint8_t)p->len;
        h[5] = (uint8_t)(p->len >> 8);
        h[6] = (uint8_t)p->n;
        h[7] = (uint8_t)(p->n >> 8);
        lw_put(h, 8);
        {                                        /* (its notes as they play: the transposition in them, build 68) */
            uint32_t e;
            int32_t d = pat_trn(p->pat, p->t) - p->ref;
            for (e = 0; e < p->n; e++) {
                lev_t v = p->ev[e];
                v.note = (uint8_t)(d ? scale_move((int32_t)v.note, d) : v.note);
                lw_put(&v, sizeof v);
            }
        }
    }
    lw_flush();                                  /* (the last, part full) */
    if (lw.err || lw.j != lw.nsec)
        return lw.err ? lw.err : -3;
    *gen = lw.gen;
    *nsec = (uint8_t)lw.nsec;
    return 0;
}
/* every pattern's loops emptied (none playing, none recording) */
static void lop_clear(void)
{
    uint32_t i;
    if (ui.lrec)
        loop_drop();
    for (i = 0; i < NLP; i++)
        lps[i].on = 0;
    RING_PUBLISH();
    for (i = 0; i < NLP; i++) {
        lps[i].pat = LP_FREE;
        lps[i].cap = lps[i].n = lps[i].len = 0;
    }
}
/* the stream read back: a byte at a time from the sectors in map, each read as the one before runs out */
static struct {
    uint32_t pos, j, nsec, left;
    uint8_t map[8];
    int err;
} lr;
static void lr_get(void *p, uint32_t n)
{
    uint8_t *b = (uint8_t *)p;
    while (n--) {
        if (lr.err || !lr.left) {
            lr.err = -1;
            *b++ = 0;
            continue;
        }
        if (lr.pos == LOP_DATA) {
            if (++lr.j >= lr.nsec || lop_read(lr.map[lr.j]))
                lr.err = -2;
            lr.pos = 0;
        }
        *b++ = lop_data()[lr.pos++];
        lr.left--;
    }
}
static int lp_copy_pat(uint32_t from, uint32_t to);
/* session k's loops (generation gen, nsec sectors) given back, each to its pattern; the loops emptied first, and left
 * empty if they do not read whole. Playing, they start at the next bar. A session saved before build 73 (its loops
 * the session's, playing in every pattern): its loops to pat, the pattern it was saved playing, and copied to every
 * other pattern saved, as they played there (-6: not all of them fitted) */
static int lop_load(uint32_t k, uint32_t gen, uint32_t nsec, uint32_t pat)
{
    uint32_t i, j, a, old = 0;
    uint8_t cnt;
    lop_clear();
    if (!gen || !nsec)
        return 0;
    if (nsec > sizeof lr.map)
        return -2;
    memset(&lr, 0, sizeof lr);
    lr.nsec = nsec;
    for (j = 0; j < nsec; j++) {                 /* (each place in the stream: the sector that holds it) */
        lop_meta_t m;
        for (i = 0; i < LOP_N; i++)
            if (!lop_peek(i, &m) && m.slot == k && m.gen == gen && m.idx == j && m.nsec == nsec)
                break;
        if (i == LOP_N)
            return -2;
        lr.map[j] = (uint8_t)i;
    }
    if (lop_read(lr.map[0]))
        return -2;
    lr.left = lop_meta()->bytes;
    a = seq_run && seq_gcount != 0xFFFFFFFFu ? (seq_gcount / 16u + 1u) * 16u : 0u;
    lr_get(&cnt, 1);
    for (i = 0; i < cnt && !lr.err; i++) {
        uint8_t h[8];
        loop_t *p;
        uint32_t n, len, pk;
        lr_get(h, 8);
        n = h[6] | (uint32_t)h[7] << 8;
        len = h[4] | (uint32_t)h[5] << 8;
        pk = h[3] ? h[3] - 1u : pat;
        old |= !h[3];
        if (lr.err || h[0] >= NTRACK || h[1] >= NLOOP || pk >= PAT_SLOTS || !n || n > LOOP_EV || !len ||
            len > LOOP_MAX)
            break;
        p = lp_new(pk, h[0], h[1], n);
        if (!p || p->cap < n)
            break;
        lr_get(p->ev, n * (uint32_t)sizeof(lev_t));
        p->n = (uint16_t)n;
        p->len = (uint16_t)len;
        p->mute = (uint8_t)(h[2] != 0);
        p->ref = (int8_t)pat_trn(pk, h[0]);      /* (its notes as they play in its pattern) */
        p->anchor = a;
        p->cur = 0;
        p->kick = 0;
    }
    if (lr.err || i != cnt) {
        lop_clear();
        return -2;
    }
    RING_PUBLISH();
    for (i = 0; i < NLP; i++)
        if (lps[i].pat != LP_FREE && lps[i].n)
            lps[i].on = 1;
    if (old && pat < PAT_SLOTS) {                /* (build 74) */
        int rc = 0;
        for (i = 0; i < PAT_SLOTS; i++)
            if (i != pat && pat_store[i].used && lp_copy_pat(pat, i))
                rc = -6;
        for (i = 0; i < NLP; i++)                /* (each from the next bar, as the one playing) */
            if (lps[i].pat != LP_FREE)
                lps[i].anchor = a;
        return rc;
    }
    return 0;
}
/* session k's loops copied for session to: their new generation and sectors */
static int lop_copy(uint32_t k, uint32_t gen, uint32_t nsec, uint32_t to, uint32_t *ngen)
{
    uint32_t i, j;
    uint8_t from[8], dst[8];
    *ngen = 0;
    if (!gen || !nsec)
        return 0;
    if (nsec > sizeof from || lop_alloc(to, nsec, dst))
        return -4;
    for (j = 0; j < nsec; j++) {
        lop_meta_t m;
        for (i = 0; i < LOP_N; i++)
            if (!lop_peek(i, &m) && m.slot == k && m.gen == gen && m.idx == j)
                break;
        if (i == LOP_N)
            return -2;
        from[j] = (uint8_t)i;
    }
    *ngen = ++lop_gmax;
    for (j = 0; j < nsec; j++) {
        lop_meta_t *m;
        if (lop_read(from[j]))
            return -2;
        m = lop_meta();
        m->slot = (uint8_t)to;
        m->gen = *ngen;
        if (lop_write(dst[j]))
            return -2;
    }
    return 0;
}
/* the last session, noted (only when it changes) */
static void lst_note(uint32_t k)
{
    if (!prj_ok || lst_slot == (int8_t)k)
        return;
    memset(prj_buf, 0, sizeof(prj_head_t) + 4u);
    prj_buf[sizeof(prj_head_t)] = (uint8_t)k;
    prj_head_fill(LST_MAGIC, 0, 4u);
    if (!prj_flash_erase(LST_SECT) && !prj_flash_prog(LST_SECT, prj_buf, sizeof(prj_head_t) + 4u))
        lst_slot = (int8_t)k;
}

static void prj_scan(int flash_ok)
{
    uint32_t k, i;
    prj_ok = (uint8_t)flash_ok;
    lop_gmax = 0;
    for (k = 0; k < PRJ_SLOTS; k++) {
        prj_head_t h;
        prj_has[k] = 0;
        prj_lgen[k] = 0;
        if (prj_ok && !prj_flash_read(prj_sector(k, 0), &h, sizeof h)) {
            prj_has[k] = (uint8_t)(prj_head_ok(&h, PRJ_MAGIC, k * 16u, sizeof(prj_body_t)) ||
                                   prj_head_ok(&h, PRJ_MAGIC, k * 16u, sizeof(prj_body17_t)) ||
                                   prj_head_ok(&h, PRJ_MAGIC, k * 16u, PRJ_LEN23) ||
                                   prj_head_ok(&h, PRJ_MAGIC, k * 16u, PRJ_LEN28) ||
                                   prj_head_ok(&h, PRJ_MAGIC, k * 16u, PRJ_LEN32) ||
                                   prj_head_ok(&h, PRJ_MAGIC, k * 16u, PRJ_LEN34) ||
                                   prj_head_ok(&h, PRJ_MAGIC, k * 16u, PRJ_LEN37) ||
                                   prj_head_ok(&h, PRJ_MAGIC, k * 16u, PRJ_LEN49) ||
                                   prj_head_ok(&h, PRJ_MAGIC, k * 16u, PRJ_LEN50));
            if (prj_head_ok(&h, PRJ_MAGIC, k * 16u, sizeof(prj_body_t)) ||
                prj_head_ok(&h, PRJ_MAGIC, k * 16u, PRJ_LEN50))   /* (its loops' generation: builds 50 on, here) */
                prj_flash_read(prj_sector(k, 0) + sizeof h + __builtin_offsetof(prj_body_t, loop_gen), &prj_lgen[k], 4u);
            else if (prj_head_ok(&h, PRJ_MAGIC, k * 16u, PRJ_LEN49))   /* (builds 38 .. 49: at their place -- else their
                                                  * loops' sectors would count as free; build 50) */
                prj_flash_read(prj_sector(k, 0) + sizeof h + __builtin_offsetof(prj_body49_t, loop_gen), &prj_lgen[k], 4u);
        }
        if (prj_lgen[k] > lop_gmax)
            lop_gmax = prj_lgen[k];
    }
    for (i = 0; prj_ok && i < LOP_N; i++) {      /* (a generation never used twice: past every one written) */
        lop_meta_t m;
        if (!lop_peek(i, &m) && m.gen > lop_gmax)
            lop_gmax = m.gen;
    }
    lst_slot = -1;
    if (prj_ok && !prj_flash_read(LST_SECT, prj_buf, sizeof(prj_head_t) + 4u) &&
        prj_head_ok((const prj_head_t *)(void *)prj_buf, LST_MAGIC, 0, 4u) &&
        ((const prj_head_t *)(void *)prj_buf)->body_crc == prj_crc(prj_buf + sizeof(prj_head_t), 4u) &&
        prj_buf[sizeof(prj_head_t)] < PRJ_SLOTS)
        lst_slot = (int8_t)prj_buf[sizeof(prj_head_t)];
}

/* ---------------------------------------------------------------- patterns --- */
/* the tempo playing: the pattern's, or (GLOBAL) the project's; the delay follows it */
static void tempo_apply(void)
{
    fx_bpm10 = (pat_tempo ? pat_tempo : ui.tempo) * 10;
    fx_cook_all();
}
static uint32_t pl_n(uint32_t t, uint32_t i)    /* step i of track t: its locks */
{
    uint32_t n = 0, j;
    for (j = 0; j < NPL; j++)
        n += seq_lid[t][i][j] != PL_NONE;
    return n;
}
/* the bytes the steps playing take in a pattern */
static uint32_t pat_bytes(void)
{
    uint32_t t, i, n = SEQ_REC4;
    for (t = 0; t < NTRACK; t++)
        for (i = 0; i < NSTEP; i++)
            if (seq_note[t][i])
                n += 6u + 3u * pl_n(t, i) + (seq_mut[t][i] || seq_mcy[t][i] || seq_mfx[t][i] || seq_fcy[t][i] ? 3u : 0u) +
                     (seq_chf[t][i] ? 1u : 0u);
    return n;
}
static int mod_on(const seqbuf_t *b, uint32_t t, uint32_t i)
{
    return b->mut[t][i] || b->mcy[t][i] || b->mfx[t][i] || b->fcy[t][i];
}
static uint32_t pl_nb(const seqbuf_t *b, uint32_t t, uint32_t i)
{
    uint32_t n = 0, j;
    for (j = 0; j < NPL; j++)
        n += b->lid[t][i][j] != PL_NONE;
    return n;
}
/* (build 62) the tracks' state playing, as a pattern holds it (SEQ_PX) */
static int16_t px_mu_over = -1;                  /* (build 65: the mutes px_take takes, if not those playing -- the
                                                  * pattern left, its own, the switch having unmuted) */
static void px_take(uint8_t *x)
{
    uint32_t t;
    for (t = 0; t < NTRACK; t++, x += PX_T) {
        uint32_t mu = px_mu_over >= 0 ? (uint32_t)px_mu_over >> t & 1u : trk_mute[t] != 0;
        x[0] = (uint8_t)(mu | (ui.chord_on[t] != 0) << 1 | (arp_on[t] != 0) << 2 |
                         (ui.chord_form[t] % CHF_N) << 3);
        x[1] = (uint8_t)(ui.eng[t] % NENG | (uint32_t)(clampi(ui.oct[t], -3, 3) + 3) << 2 | (sc_src[t] % NCP_SCS) << 5);
        x[2] = (uint8_t)ui.level[t];
        x[3] = (uint8_t)ui.pan[t];
        x[4] = (uint8_t)(sc_lvl[t] & 127u);
        memcpy(x + 5, (const void *)arp_p[t], NARP);
    }
}
static uint8_t px_base[NTRACK * PX_T];           /* the state as the pattern playing was loaded or saved */
/* .. and back: what differs from what plays, each in its range (an arpeggiator changed: its notes let go). Not the
 * session changed: the pattern's state, as its steps */
static void px_give(const uint8_t *x)
{
    uint32_t t, solo = 0;
    for (t = 0; t < NTRACK; t++, x += PX_T) {
        uint32_t e = x[1] & 3u, sc = x[1] >> 5;
        int32_t o = (int32_t)(x[1] >> 2 & 7u) - 3, lv = clampi((int8_t)x[2], 0, LEVEL_MAX),
                pn = clampi((int8_t)x[3], -PAN_MAX, PAN_MAX);
        if ((x[0] & 1u) != (trk_mute[t] != 0)) {
            if (trk_mute[t])
                loop_wait_set(t);                /* (unmuted: its loops from their first step, build 65) */
            trk_mute[t] = (uint8_t)(x[0] & 1u);
            solo = 1;
        }
        ui.chord_on[t] = (uint8_t)(x[0] >> 1 & 1u);
        ui.chord_form[t] = (uint8_t)((x[0] >> 3 & 7u) % CHF_N);
        if (e < NENG && e != ui.eng[t]) {
            ui.eng[t] = (uint8_t)e;
            ui.dirty[t] = 1;                     /* (its sound on that machine published) */
        }
        ui.oct[t] = (int8_t)clampi(o, -3, 3);
        if (lv != ui.level[t] || pn != ui.pan[t]) {
            ui.level[t] = (int8_t)lv;
            ui.pan[t] = (int8_t)pn;
            level_pan_apply(t);
        }
        sc_src[t] = (uint8_t)(sc < NCP_SCS ? sc : 0u);
        sc_lvl[t] = (uint8_t)(x[4] & 127u);
        if (arp_on[t] != (x[0] >> 2 & 1u) || memcmp((const void *)arp_p[t], x + 5, NARP))
            arp_set(t, x[0] >> 2 & 1u, x + 5);
    }
    if (solo)
        ui.solo = 0xFF;                          /* (the mutes changed: no solo to undo) */
    if (ui.page == PG_EDIT && ui.edit_pg >= pages_n(cur()))
        ui.edit_pg = 0;
    ui.force = 1;
}
/* the state as it plays, packed (PX_ON) .. */
static void px_pack(uint8_t *d, const uint8_t *x)
{
    uint32_t t, i, acc, nb, o;
    memset(d, 0, NTRACK * PX_P);
    for (t = 0; t < NTRACK; t++, x += PX_T, d += PX_P)
        for (i = 0, acc = 0, nb = 0, o = 0; i < PX_T; i++) {
            uint32_t v = i == 3u ? (uint32_t)((int8_t)x[3] + PAN_MAX) : x[i];
            acc |= (v & ((1u << PX_W[i]) - 1u)) << nb;
            for (nb += PX_W[i]; nb >= 8u; nb -= 8u, acc >>= 8)
                d[o++] = (uint8_t)acc;
            if (i == PX_T - 1u && nb)
                d[o] = (uint8_t)acc;
        }
}
static void px_unpack(uint8_t *x, const uint8_t *d)   /* .. and back */
{
    uint32_t t, i, acc, nb, o;
    for (t = 0; t < NTRACK; t++, x += PX_T, d += PX_P)
        for (i = 0, acc = 0, nb = 0, o = 0; i < PX_T; i++) {
            for (; nb < PX_W[i]; nb += 8u)
                acc |= (uint32_t)d[o++] << nb;
            x[i] = (uint8_t)(acc & ((1u << PX_W[i]) - 1u));
            if (i == 3u)
                x[3] = (uint8_t)(int8_t)((int32_t)x[3] - PAN_MAX);
            acc >>= PX_W[i];
            nb -= PX_W[i];
        }
}
/* (build 65) a pattern's mutes, a bit a track, bit 7 set; 0 if it has none (saved before build 62, or empty) */
static uint32_t px_mutes_of(const pat_t *p)
{
    uint8_t x[NTRACK * PX_T];
    const uint8_t *s = x;
    uint32_t t, m = 0x80u;
    if (!p->used || (p->st != PX_ON && p->st != PX_ON1))
        return 0;
    if (p->st == PX_ON)
        px_unpack(x, p->seq + SEQ_PX);
    else
        s = p->seq + SEQ_PX1;
    for (t = 0; t < NTRACK; t++)
        m |= (uint32_t)(s[t * PX_T] & 1u) << t;
    return m;
}
/* the pattern's state: given (its own, or -- one saved before build 62, or empty -- what plays, left so), noted */
static void px_load(const pat_t *p)
{
    uint8_t x[NTRACK * PX_T];
    if (p->used && p->st == PX_ON) {
        px_unpack(x, p->seq + SEQ_PX);
        px_give(x);
    } else if (p->used && p->st == PX_ON1)       /* (builds 62, 63) */
        px_give(p->seq + SEQ_PX1);
    px_take(px_base);
}
/* the state changed since the pattern playing was loaded or saved: the pattern changed (each pass of the main loop) */
static void px_note(void)
{
    uint8_t x[NTRACK * PX_T];
    if (pat_dirty)
        return;
    px_take(x);
    if (memcmp(x, px_base, sizeof x))
        pat_dirty = 1;
}
/* steps b, and the tracks' state playing, as a pattern. Every step with a note fits (96 of 6 bytes: 576 of the
 * 590); the room left goes to the chords, then the MOD, then the locks, one a step in turn across every track
 * (build 72: before, the tracks in order -- a track locked on every step left the tracks after it without steps) */
static void pat_encode(pat_t *p, const seqbuf_t *b)
{
    static uint8_t md[NTRACK][NSTEP], ch[NTRACK][NSTEP], nl[NTRACK][NSTEP];
    uint32_t t, i, r, more, o = SEQ_REC4, n = 0, room = SEQ_PX - SEQ_REC4;
    memset(p, 0, sizeof *p);
    p->used = 1;
    p->tempo = pat_tempo;
    p->seq[SEQ_FMT] = SEQ_V4;
    {
        uint8_t x[NTRACK * PX_T];
        px_take(x);
        p->st = PX_ON;
        px_pack(p->seq + SEQ_PX, x);
    }
    memset(md, 0, sizeof md);
    memset(ch, 0, sizeof ch);
    memset(nl, 0, sizeof nl);
    for (t = 0; t < NTRACK; t++)
        for (i = 0; i < NSTEP; i++)
            if (b->note[t][i])
                room -= 6u;
    for (t = 0; t < NTRACK; t++)
        for (i = 0; i < NSTEP; i++)
            if (b->note[t][i] && b->chf[t][i] && room >= 1u) {
                ch[t][i] = 1;
                room -= 1u;
            }
    for (t = 0; t < NTRACK; t++)
        for (i = 0; i < NSTEP; i++)
            if (b->note[t][i] && mod_on(b, t, i) && room >= 3u) {
                md[t][i] = 1;
                room -= 3u;
            }
    for (r = 0, more = 1; more && room >= 3u; r++)
        for (t = 0, more = 0; t < NTRACK; t++)
            for (i = 0; i < NSTEP; i++)
                if (b->note[t][i] && pl_nb(b, t, i) > r && room >= 3u) {
                    nl[t][i]++;
                    room -= 3u;
                    more = 1;
                }
    for (t = 0; t < NTRACK; t++) {
        p->seq[SEQ_LEN + t] = (uint8_t)(b->len[t] | ((b->div[t] + SEQ_DIV1) % NDIV) << 5);
        p->seq[SEQ_DIR + t] = b->dir[t];
        p->seq[SEQ_DEN + t] = b->den[t];
        p->seq[SEQ_CX + t] = b->cx[t];
        p->seq[SEQ_TRN + t] = (uint8_t)b->trn[t];
        for (i = 0; i < NSTEP; i++) {
            uint32_t j, at;
            if (!b->note[t][i])
                continue;
            p->seq[o++] = (uint8_t)(t * NSTEP + i);
            p->seq[o++] = b->note[t][i];
            p->seq[o++] = b->vel[t][i];
            p->seq[o++] = (uint8_t)(b->glen[t][i] | b->rtg[t][i] << 4);
            p->seq[o++] = b->cond[t][i];
            at = o++;
            p->seq[at] = 0;
            if (md[t][i]) {                      /* (MOD) */
                p->seq[o++] = (uint8_t)b->mut[t][i];
                p->seq[o++] = (uint8_t)(b->mcy[t][i] | b->fcy[t][i] << 4);
                p->seq[o++] = b->mfx[t][i];
            }
            if (ch[t][i])                        /* (a chord: its form + 1; bit 6 of the locks' count, build 33) */
                p->seq[o++] = b->chf[t][i];
            for (j = 0; j < NPL && (p->seq[at] & 63u) < nl[t][i]; j++) {
                uint32_t id = b->lid[t][i][j];
                int32_t v = b->lv[t][i][j];
                if (id == PL_NONE)
                    continue;
                p->seq[o++] = (uint8_t)id;
                p->seq[o++] = (uint8_t)v;
                p->seq[o++] = (uint8_t)((uint32_t)v >> 8);
                p->seq[at]++;
            }
            if (md[t][i])
                p->seq[at] |= 0x80u;
            if (ch[t][i])
                p->seq[at] |= 0x40u;
            n++;
        }
    }
    p->seq[SEQ_NREC] = (uint8_t)n;
}
static seqbuf_t sb_tmp;                           /* (the main loop's) */
static uint8_t pat_dirtyv[PAT_SLOTS];             /* a pattern changed since saved (kept in the store, not the flash) */
static uint8_t seq_pend_k;                        /* (the pattern waiting) */
static void pat_take(pat_t *p)                   /* the pattern playing, as a pattern */
{
    seq_copy_out(&sb_tmp);
    memcpy(sb_tmp.den, ui.gen_den, NTRACK);
    memcpy(sb_tmp.cx, ui.gen_cx, NTRACK);
    memcpy(sb_tmp.trn, ui.trn, NTRACK);
    pat_encode(p, &sb_tmp);
}
/* a pattern as steps, into b (empty: none) */
static void pat_decode(const pat_t *p, seqbuf_t *b)
{
    uint32_t t, i;
    memset(b->note, 0, sizeof b->note);
    memset(b->vel, VEL_DEF, sizeof b->vel);
    memset(b->glen, GLEN_DEF, sizeof b->glen);
    memset(b->cond, CD_NONE, sizeof b->cond);
    memset(b->lid, PL_NONE, sizeof b->lid);
    memset(b->rtg, 0, sizeof b->rtg);
    memset(b->mut, 0, sizeof b->mut);
    memset(b->mcy, 0, sizeof b->mcy);
    memset(b->mfx, 0, sizeof b->mfx);
    memset(b->fcy, 0, sizeof b->fcy);
    memset(b->chf, 0, sizeof b->chf);
    for (t = 0; t < NTRACK; t++) {
        uint32_t bb = p->used ? p->seq[SEQ_LEN + t] : 0u, c = bb >> 5, v4 = p->used && p->seq[SEQ_FMT] == SEQ_V4;
        b->len[t] = (uint8_t)((bb & 31u) <= NSTEP ? bb & 31u : 0u);
        b->div[t] = (uint8_t)(c < NDIV ? (c + SEQ_DIV1) % NDIV : SEQ_DIV1);
        b->dir[t] = (uint8_t)(v4 && p->seq[SEQ_DIR + t] < NDIR ? p->seq[SEQ_DIR + t] : DIR_FWD);
        b->den[t] = (uint8_t)(v4 && p->seq[SEQ_DEN + t] <= 100u ? p->seq[SEQ_DEN + t] : 50u);
        b->cx[t] = (uint8_t)(v4 && p->seq[SEQ_CX + t] <= 100u ? p->seq[SEQ_CX + t] : 50u);
        b->trn[t] = (int8_t)(v4 ? clampi((int8_t)p->seq[SEQ_TRN + t], -TRN_MAX, TRN_MAX) : 0);
    }
    if (p->used && (p->seq[SEQ_FMT] == SEQ_V3 || p->seq[SEQ_FMT] == SEQ_V4)) {
        uint32_t v4 = p->seq[SEQ_FMT] == SEQ_V4, o = v4 ? SEQ_REC4 : SEQ_REC, r;
        for (r = 0; r < p->seq[SEQ_NREC] && o + 6u <= PAT_SEQ_ROOM; r++) {
            uint32_t id = p->seq[o], nl = p->seq[o + 5u] & 63u, md = p->seq[o + 5u] >> 7, ch = p->seq[o + 5u] >> 6 & 1u, l;
            if (id >= NTRACK * NSTEP)
                break;
            t = id / NSTEP;
            i = id % NSTEP;
            b->vel[t][i] = (uint8_t)clampi(p->seq[o + 2u], 1, 127);
            {
                uint32_t g = v4 ? p->seq[o + 3u] & 15u : p->seq[o + 3u], rt = v4 ? p->seq[o + 3u] >> 4 : 0u;
                b->glen[t][i] = (uint8_t)(g < NGLEN ? g : GLEN_DEF);
                b->rtg[t][i] = (uint8_t)(rt < NRTG ? rt : 0u);
            }
            b->cond[t][i] = (uint8_t)(p->seq[o + 4u] < NCOND ? p->seq[o + 4u] : CD_NONE);
            b->note[t][i] = p->seq[o + 1u];
            o += 6u;
            if (md && o + 3u <= PAT_SEQ_ROOM) {  /* (MOD) */
                int32_t mu = (int8_t)p->seq[o];
                b->mut[t][i] = (int8_t)(mu == MUT_RND || (mu >= -12 && mu <= 12) ? mu : 0);
                b->mcy[t][i] = (uint8_t)(p->seq[o + 1u] & 15u);
                b->fcy[t][i] = (uint8_t)(p->seq[o + 1u] >> 4);
                b->mfx[t][i] = (uint8_t)(p->seq[o + 2u] < NMFX ? p->seq[o + 2u] : 0u);
                o += 3u;
            }
            if (ch && o + 1u <= PAT_SEQ_ROOM) {  /* (a chord) */
                b->chf[t][i] = (uint8_t)(p->seq[o] <= CHF_N ? p->seq[o] : 0u);
                o += 1u;
            }
            for (l = 0; l < nl && o + 3u <= PAT_SEQ_ROOM; l++, o += 3u)
                if (p->seq[o] <= PL_PAN && l < NPL) {
                    b->lv[t][i][l] = (int16_t)(uint16_t)(p->seq[o + 1u] | p->seq[o + 2u] << 8);
                    b->lid[t][i][l] = p->seq[o];
                }
        }
    } else if (p->used && p->seq[SEQ_FMT] == SEQ_V2) {   /* (builds 11 .. 17) */
        uint32_t o = SEQ_REC, r;
        for (r = 0; r < p->seq[SEQ_NREC] && o + 6u <= PAT_SEQ_ROOM; r++) {
            uint32_t id = p->seq[o], m = (uint32_t)(p->seq[o + 3u] >> 4) << 8 | p->seq[o + 5u], l, j = 0;
            if (id >= NTRACK * NSTEP)
                break;
            t = id / NSTEP;
            i = id % NSTEP;
            b->vel[t][i] = (uint8_t)clampi(p->seq[o + 2u], 1, 127);
            b->glen[t][i] = (uint8_t)((p->seq[o + 3u] & 15u) < NGLEN ? p->seq[o + 3u] & 15u : GLEN_DEF);
            b->cond[t][i] = (uint8_t)(p->seq[o + 4u] < NCOND ? p->seq[o + 4u] : CD_NONE);
            b->note[t][i] = p->seq[o + 1u];
            o += 6u;
            for (l = 0; l < 12u && o < PAT_SEQ_ROOM; l++)
                if (m >> l & 1u) {
                    int32_t v = p->seq[o++] & 127u;
                    if (j < NPL) {               /* (the 12 then: LK_FREQ .. LK_PAN, in the order they still have) */
                        b->lv[t][i][j] = (int16_t)v;
                        b->lid[t][i][j++] = LK_PID[l];
                    }
                }
        }
    } else if (p->used)                          /* (as builds 9 and 10 kept them) */
        for (t = 0; t < NTRACK; t++)
            for (i = 0; i < NSTEP; i++) {
                uint32_t o = t * NSTEP + i, g = p->seq[SEQ_OLD_GLEN + o];
                b->vel[t][i] = (uint8_t)(p->seq[SEQ_OLD_VEL + o] ? p->seq[SEQ_OLD_VEL + o] : VEL_DEF);
                b->glen[t][i] = (uint8_t)(g && g <= NGLEN ? g - 1u : GLEN_DEF);
                b->cond[t][i] = (uint8_t)(p->seq[SEQ_OLD_COND + o] < NCOND ? p->seq[SEQ_OLD_COND + o] : CD_NONE);
                b->note[t][i] = p->seq[SEQ_OLD_NOTE + o];
            }
}
/* slot k of the store, as the pattern playing, at once, its loops with it (follow: unused since build 73 -- each
 * loop is its own pattern's) */
static void pat_give(uint32_t k, int follow)
{
    const pat_t *p = &pat_store[k];
    uint32_t i;
    (void)follow;
    seq_pend = 0;                                /* (a pattern waiting: not now) */
    pat_decode(p, &sb_tmp);
    lp_follow(sb_tmp.trn);
    if (k != seq_lpat) {                         /* (its loops from their first step: the next bar, playing) */
        uint32_t a = seq_run && seq_gcount != 0xFFFFFFFFu ? (seq_gcount / 16u + 1u) * 16u : 0u;
        for (i = 0; i < NLP; i++)
            if (lps[i].pat == k) {
                lps[i].cur = 0;
                lps[i].kick = 0;
                lps[i].anchor = a;
            }
        memset((void *)loop_wait, 0, sizeof loop_wait);
        RING_PUBLISH();
        seq_lpat = (uint8_t)k;
    }
    seq_copy_in(&sb_tmp);
    memcpy(ui.gen_den, sb_tmp.den, NTRACK);
    memcpy(ui.gen_cx, sb_tmp.cx, NTRACK);
    memcpy(ui.trn, sb_tmp.trn, NTRACK);
    pat_cur = (uint8_t)k;
    pat_tempo = p->used ? p->tempo : 0;
    pat_dirty = pat_dirtyv[k];
    px_load(p);
    tempo_apply();
}
/* the pattern playing, changed: kept in its slot of the store as it is (the flash, when it is saved) */
static void pat_stash(void)
{
    px_note();
    if (!pat_dirty)
        return;
    pat_take(&pat_store[pat_cur]);
    pat_dirtyv[pat_cur] = 1;
}
/* the store's patterns k (and the three beside it in its sector) into the flash, if the project has a slot */
static int pat_flush(uint32_t k)
{
    uint32_t s = k / PAT_PER;
    if (prj_cur < 0)
        return 0;                                /* (no slot yet: kept in RAM until the project is saved) */
    memcpy(prj_buf + sizeof(prj_head_t), &pat_store[s * PAT_PER], PAT_PER * sizeof(pat_t));
    return prj_write((uint32_t)prj_cur, 1u + s);
}
static int pat_save(uint32_t k)
{
    pat_take(&pat_store[k]);
    px_take(px_base);
    pat_cur = (uint8_t)k;
    pat_dirty = 0;
    pat_dirtyv[k] = 0;
    prj_dirty = 1;                               /* (the project's pattern changed: saved with it) */
    return pat_flush(k);
}
/* (build 73) pattern k's loops emptied (the one being recorded in it: dropped) */
static void lp_clear_pat(uint32_t k)
{
    uint32_t i;
    if (ui.lrec && lrp && lrp->pat == k)
        loop_drop();
    for (i = 0; i < NLP; i++)
        if (lps[i].pat == k)
            lp_free(&lps[i]);
}
/* (build 73) pattern from's loops copied as pattern to's (its own emptied first; the one being recorded left out):
 * 0, or -1 if some did not fit (the table or the notes full) */
static int lp_copy_pat(uint32_t from, uint32_t to)
{
    uint32_t i;
    int rc = 0;
    if (from == to)
        return 0;
    lp_clear_pat(to);
    for (i = 0; i < NLP; i++) {
        const loop_t *p = &lps[i];
        loop_t *q;
        if (p->pat != from || !p->on || p == lrp)
            continue;
        q = lp_new(to, p->t, p->l, p->n);
        if (!q || q->cap < p->n) {
            lp_free(q);
            rc = -1;
            continue;
        }
        memcpy(q->ev, p->ev, p->n * sizeof(lev_t));
        q->n = p->n;
        q->len = p->len;
        q->mute = p->mute;
        q->ref = p->ref;
        RING_PUBLISH();
        q->on = 1;
    }
    return rc;
}
/* the PATTERNS grid and a white key held: pattern k emptied (the one playing too: its steps gone) */
static void pat_clear(uint32_t k)
{
    char n[12] = "PATTERN ";
    int rc;
    if (k >= PAT_SLOTS)
        return;
    memset(&pat_store[k], 0, sizeof pat_store[k]);
    pat_dirtyv[k] = 0;
    lp_clear_pat(k);                             /* (its loops too: build 73) */
    if (seq_pend == 1u && seq_pend_k == k)
        seq_pend = 0;                            /* (it was waiting to play: not now) */
    if (k == pat_cur)
        pat_give(k, 1);
    rc = pat_flush(k);
    fmt_int(n + 8, (int32_t)k + 1);
    flash(rc ? "FAILED" : "EMPTY", n, rc ? C_BAD : RGB(255, 210, 120));
    ui.force = 1;
}

/* (build 66; build 72 without SHIFT) the PATTERNS grid, a white key held: the pattern playing (its changes too, its state) saved as
 * pattern k, empty or not; the one playing plays on (k itself: saved in its own slot) */
static void pat_save_as(uint32_t k)
{
    char n[12] = "PATTERN ";
    int rc, lr = 0;
    if (k >= PAT_SLOTS)
        return;
    if (k == pat_cur)
        rc = pat_save(k);
    else {
        lr = lp_copy_pat(pat_cur, k);            /* (its loops too: build 73) */
        pat_take(&pat_store[k]);
        pat_dirtyv[k] = 0;
        if (seq_pend == 1u && seq_pend_k == k) {   /* (waiting to play: as saved now) */
            seq_pend = 0;
            RING_PUBLISH();
            pat_decode(&pat_store[k], &sb_next);
            seq_pend_mu = (uint8_t)px_mutes_of(&pat_store[k]);
            RING_PUBLISH();
            seq_pend = 1;
        }
        prj_dirty = 1;
        rc = pat_flush(k);
    }
    fmt_int(n + 8, (int32_t)k + 1);
    flash(rc ? "FAILED" : "SAVED", rc ? "" : lr ? "LOOPS LOST" : n, rc || lr ? C_BAD : C_OK);
    ui.force = 1;
}
/* the PATTERNS grid, SHIFT and a white key held: pattern k copied (the one playing as it plays, its changes too); a pattern
 * copied, pasted into k, saved there and loaded; the one copied again: let go */
static pat_t pat_clip;
static int8_t pat_clip_from = -1;
static void pat_copy_hold(uint32_t k)
{
    char n[12] = "PATTERN ";
    int rc;
    if (k >= PAT_SLOTS)
        return;
    fmt_int(n + 8, (int32_t)k + 1);
    ui.force = 1;
    if (pat_clip_from == (int8_t)k) {
        pat_clip_from = -1;
        say("COPY OFF", 900);
        return;
    }
    if (pat_clip_from >= 0) {                    /* pasted (its loops too: build 73) */
        int lr = lp_copy_pat((uint32_t)pat_clip_from, k);
        pat_store[k] = pat_clip;
        pat_dirtyv[k] = 0;
        rc = pat_flush(k);
        pat_switch(k);
        pat_clip_from = -1;
        prj_dirty = 1;
        flash(rc ? "FAILED" : "PASTED", rc ? "" : lr ? "LOOPS LOST" : n, rc || lr ? C_BAD : C_OK);
        return;
    }
    if (k == pat_cur)
        pat_take(&pat_clip);
    else if (pat_store[k].used)
        pat_clip = pat_store[k];
    else {
        say("EMPTY", 900);
        return;
    }
    pat_clip_from = (int8_t)k;
}

/* (build 64) the PATTERNS grid, the 3rd black key held (a break playing) and a white key held 0.8 s: the break saved
 * as pattern k. Each track under it takes, at each of its steps, the step the break plays there (its plan: the way,
 * from where the track went into it), with one throw of its layers -- a ratchet, an octave leap, a flam (MIDI FX
 * FLAM), an accent, the gap emptying the bar's last steps; the other tracks, and the drones, as written. The pattern
 * holds the tracks' state playing, as SAVE held on the grid does; the one playing saved over: the break's steps play at the end
 * of the chosen track's time through */
static uint32_t rtg_of(uint32_t n)               /* (the ratchet nearest n hits, not more) */
{
    uint32_t r = 0, i;
    for (i = 0; i < NRTG; i++)
        if (RTG_N[i] <= n)
            r = i;
    return r;
}
static void brk_render(seqbuf_t *b, uint32_t t, uint32_t seed)
{
    uint32_t len = seq_len[t] ? seq_len[t] : NSTEP, kick = brk_kick >> t & 1u, drum = brk_drum >> t & 1u, p,
             a = brk_fresh >> t & 1u ? 0u : brk_anchor[t], r = seed | 1u;
#define BRND(n) (r = r * 1664525u + 1013904223u, (r >> 16) % (n))
    for (p = 0; p < len; p++) {
        uint32_t src = brk_map(a, p, len, kick), ro;
        b->note[t][p] = seq_note[t][src];
        b->vel[t][p] = seq_vel[t][src];
        b->glen[t][p] = seq_glen[t][src];
        b->cond[t][p] = seq_cond[t][src];
        b->rtg[t][p] = seq_rtg[t][src];
        b->mut[t][p] = seq_mut[t][src];
        b->mcy[t][p] = seq_mcy[t][src];
        b->mfx[t][p] = seq_mfx[t][src];
        b->fcy[t][p] = seq_fcy[t][src];
        b->chf[t][p] = seq_chf[t][src];
        memcpy(b->lid[t][p], (const void *)seq_lid[t][src], NPL);
        memcpy(b->lv[t][p], (const void *)seq_lv[t][src], sizeof b->lv[t][p]);
        if ((brk_lay & BL_GAP) && !kick && p % 16u >= brk_gap) {   /* (the gap: the kick alone) */
            b->note[t][p] = 0;
            continue;
        }
        if (!b->note[t][p])
            continue;
        ro = brk_rec == BR_ROLL ? (kick ? 10u : drum ? 70u : 35u) : (kick ? 5u : drum ? 35u : 15u);
        if ((brk_lay & BL_RAT) && !b->rtg[t][p] && BRND(100u) < ro) {
            uint32_t rate = 2u + (brk_rec == BR_ROLL) + brk_n16 / 16u + BRND(2u);
            b->rtg[t][p] = (uint8_t)rtg_of(rate < 6u ? rate : 6u);
        }
        if ((brk_lay & BL_OCT) && !kick && !drum && BRND(100u) < 30u) {
            int32_t n = (int32_t)b->note[t][p] + (BRND(2u) ? 12 : -12);
            if (n >= 24 && n <= 108)
                b->note[t][p] = (uint8_t)n;
        }
        if ((brk_lay & BL_FLAM) && !kick && drum && !b->mfx[t][p] && BRND(100u) < 25u) {
            b->mfx[t][p] = MFX_FLAM;
            b->fcy[t][p] = 0;
        }
        if (brk_lay & BL_ACC) {
            uint32_t vp = p % 4u == 0u ? 100u : 55u + BRND(40u), v = b->vel[t][p] ? b->vel[t][p] : VEL_DEF;
            b->vel[t][p] = (uint8_t)clampi((int32_t)(v * vp / 100u), 1, 127);
        }
    }
#undef BRND
}
static void pat_brk_save(uint32_t k)
{
    char n[12] = "PATTERN ";
    uint32_t t;
    int rc, lr = 0;
    if (k >= PAT_SLOTS)
        return;
    ui.force = 1;
    if (!seq_run || brk_shown == 0xFFu) {
        say("NOT PLAYING", 900);
        return;
    }
    seq_copy_out(&sb_tmp);
    memcpy(sb_tmp.den, ui.gen_den, NTRACK);
    memcpy(sb_tmp.cx, ui.gen_cx, NTRACK);
    memcpy(sb_tmp.trn, ui.trn, NTRACK);
    for (t = 0; t < NTRACK; t++)
        if ((ui.brk >> t & 1u) && !(brk_keep >> t & 1u))
            brk_render(&sb_tmp, t, brk_seed + t * 977u);
    pat_encode(&pat_store[k], &sb_tmp);
    pat_dirtyv[k] = 0;
    lr = lp_copy_pat(pat_cur, k);                /* (the loops playing with it: build 73) */
    if (seq_pend == 1u && seq_pend_k == k)
        seq_pend = 0;                            /* (it was waiting to play: as it was, not now) */
    if (k == pat_cur) {                          /* (the one playing: its steps the break's, at the end of the chosen
                                                  * track's time through) */
        px_take(px_base);
        pat_dirty = 0;
        seq_pend = 0;
        RING_PUBLISH();
        pat_decode(&pat_store[k], &sb_next);
        seq_pend_k = (uint8_t)k;
        seq_pend_lp = (uint8_t)k;
        seq_pend_t = ui.sel;
        seq_pend_mu = 0;
        RING_PUBLISH();
        seq_pend = 1;
    }
    rc = pat_flush(k);
    prj_dirty = 1;
    fmt_int(n + 8, (int32_t)k + 1);
    flash(rc ? "FAILED" : "BREAK SAVED", rc ? "" : lr ? "LOOPS LOST" : n, rc || lr ? C_BAD : C_OK);
}

/* ---------------------------------------------------------------- projects --- */
static void prj_take(prj_body_t *b)          /* the state playing, into a body .. */
{
    uint32_t t;
    memset(b, 0, sizeof *b);
    b->tempo = (uint16_t)ui.tempo;
    b->root = ui.root;
    b->scale = ui.scale;
    b->pat = pat_cur;
    for (t = 0; t < NTRACK; t++) {
        b->eng[t] = ui.eng[t];
        b->oct[t] = ui.oct[t];
        b->level[t] = ui.level[t];
        b->pan[t] = ui.pan[t];
    }
    memcpy(b->dl, fx_dl, NDL);
    memcpy(b->rv, fx_rv, NRV);
    memcpy(b->ch, fx_ch, NCH);
    memcpy(b->cp, fx_cp, NCP);
    memcpy(b->sc_src, (const void *)sc_src, NTRACK);
    memcpy(b->sc_lvl, (const void *)sc_lvl, NTRACK);
    memcpy(b->chord_on, ui.chord_on, NTRACK);
    memcpy(b->chord_form, ui.chord_form, NTRACK);
    memcpy(b->mute, (const void *)trk_mute, NTRACK);
    memcpy(b->arp_on, (const void *)arp_on, NTRACK);
    memcpy(b->arp, (const void *)arp_p, sizeof b->arp);
    memcpy(b->snd, ui.snd, sizeof b->snd);
    b->sel = ui.sel;
}
static void prj_give(const prj_body_t *b)    /* .. and back: every note let go, every track's sound published again */
{
    uint32_t t;
    for (t = 0; t < NTRACK; t++) {
        ev_post(EV_STOP, t, 0, 0);
        ui.eng[t] = (uint8_t)(b->eng[t] % NENG);
        ui.oct[t] = (int8_t)clampi(b->oct[t], -3, 3);
        ui.level[t] = (int8_t)clampi(b->level[t], 0, LEVEL_MAX);
        ui.pan[t] = (int8_t)clampi(b->pan[t], -PAN_MAX, PAN_MAX);
        level_pan_apply(t);
        mark(t);
    }
    memset(ui.key_track, 0xFF, sizeof ui.key_track);
    memcpy(ui.snd, b->snd, sizeof ui.snd);
    memcpy(fx_dl, b->dl, NDL);
    memcpy(fx_rv, b->rv, NRV);
    memcpy(fx_ch, b->ch, NCH);
    memcpy(fx_cp, b->cp, NCP);
    for (t = 0; t < NTRACK; t++) {
        sc_src[t] = (uint8_t)(b->sc_src[t] < NCP_SCS ? b->sc_src[t] : 0u);
        sc_lvl[t] = (uint8_t)(b->sc_lvl[t] & 127u);
        ui.chord_on[t] = (uint8_t)(b->chord_on[t] != 0);
        ui.chord_form[t] = (uint8_t)(b->chord_form[t] % CHF_N);
        trk_mute[t] = (uint8_t)(b->mute[t] != 0);
        arp_set(t, b->arp_on[t], b->arp[t]);
    }
    memset(ui.key_arp, 0, sizeof ui.key_arp);
    ui.solo = 0xFF;                              /* (the mutes loaded: no solo to undo) */
    ui.tempo = (int16_t)clampi(b->tempo, TEMPO_MIN, TEMPO_MAX);
    ui.root = (uint8_t)(b->root % 12u);
    ui.scale = (uint8_t)(b->scale < NSCALE ? b->scale : 0);
    ui.sel = (uint8_t)(b->sel < NTRACK ? b->sel : 0);
}
static int prj_save(uint32_t k)              /* the loops into the store, the pattern playing into its slot, then the
                                             * whole project */
{
    uint32_t s, gen;
    uint8_t nsec;
    int rc;
    if (!prj_ok || k >= PRJ_SLOTS)
        return -1;
    if ((rc = lop_save(k, &gen, &nsec)) != 0)
        return rc;
    pat_take(&pat_store[pat_cur]);
    px_take(px_base);
    pat_dirty = 0;
    prj_take((prj_body_t *)(void *)(prj_buf + sizeof(prj_head_t)));
    ((prj_body_t *)(void *)(prj_buf + sizeof(prj_head_t)))->loop_gen = gen;
    ((prj_body_t *)(void *)(prj_buf + sizeof(prj_head_t)))->loop_nsec = nsec;
    if ((rc = prj_write(k, 0)) != 0)
        return rc;
    prj_has[k] = 1;
    prj_lgen[k] = gen;
    for (s = 0; s < PAT_SECTS; s++) {
        memcpy(prj_buf + sizeof(prj_head_t), &pat_store[s * PAT_PER], PAT_PER * sizeof(pat_t));
        if ((rc = prj_write(k, 1u + s)) != 0)
            return rc;
    }
    prj_cur = (int8_t)k;
    prj_dirty = 0;
    memset(pat_dirtyv, 0, sizeof pat_dirtyv);    /* (every pattern in the flash now) */
    lst_note(k);
    return 0;
}
/* a new session (build 57), as the FM-1 starts with none: each track's machine the start's, its sounds drawn anew,
 * the start's levels, pans, octaves, effects, compressor and scale; no sidechain, chords, mutes, arpeggiators,
 * patterns or loops; the sequencer stopped. Not saved anywhere yet */
static void prj_fresh(void)
{
    static prj_body_t b POOL;                    /* (not on the stack, not in RAM: 3 KB, in the pool) */
    uint32_t t, e;
    memset(&b, 0, sizeof b);
    b.tempo = 120;
    b.scale = 6;
    for (t = 0; t < NTRACK; t++) {
        b.eng[t] = ENG0[t];
        b.oct[t] = (int8_t)(t ? 0 : -1);
        b.level[t] = LEVEL_UNITY;
        b.pan[t] = PAN_DEF[t];
        for (e = 0; e < NENG; e++)
            gen_sound(&b.snd[t][e], e, e == ENG0[t] ? FAM0[t] : -1, -1, next_seed());
        b.sc_lvl[t] = 64;
        memcpy(b.arp[t], AR_DEF, NARP);
    }
    memcpy(b.dl, DL0, NDL);
    memcpy(b.rv, RV0, NRV);
    memcpy(b.ch, CH0, NCH);
    memcpy(b.cp, CP0, NCP);
    seq_run = 0;
    rec_set(REC_OFF);
    lop_clear();
    ui.mute_mode = 0;
    prj_give(&b);
    memset(pat_store, 0, sizeof pat_store);
    memset(pat_dirtyv, 0, sizeof pat_dirtyv);
    pat_give(0, 0);
    prj_cur = -1;
    prj_dirty = 0;
    pat_dirty = 0;
}
static int prj_load(uint32_t k)
{
    uint32_t s;
    int rc = prj_read(k, 0);
    prj_body_t b;
    if (rc)
        return rc;
    memcpy(&b, prj_buf + sizeof(prj_head_t), sizeof b);
    for (s = 0; s < PAT_SECTS; s++) {            /* (a pattern sector that does not check: its six empty) */
        if (prj_read(k, 1u + s))
            memset(&pat_store[s * PAT_PER], 0, PAT_PER * sizeof(pat_t));
        else
            memcpy(&pat_store[s * PAT_PER], prj_buf + sizeof(prj_head_t), PAT_PER * sizeof(pat_t));
    }
    prj_give(&b);
    prj_cur = (int8_t)k;
    memset(pat_dirtyv, 0, sizeof pat_dirtyv);
    pat_give(b.pat < PAT_SLOTS ? b.pat : 0u, 0);    /* (the pattern it was saved playing) */
    prj_dirty = 0;
    rc = lop_load(k, b.loop_gen, b.loop_nsec, b.pat < PAT_SLOTS ? b.pat : 0u);
    lst_note(k);
    return rc ? -5 : 0;                          /* (-5: loaded, but its loops did not read whole) */
}
/* at the start: the last session loaded or saved, loaded again (none, or gone: the start as it is) */
static void prj_boot(void)
{
    char n[12] = "SESSION ";
    if (lst_slot < 0 || !prj_has[lst_slot])
        return;
    if (prj_load((uint32_t)lst_slot) && prj_cur != lst_slot)
        return;
    fmt_int(n + 8, lst_slot + 1);
    say(n, 1500);
}
static int prj_copy(uint32_t from, uint32_t to)
{
    uint32_t s, gen;
    int rc;
    prj_body_t b;
    if ((rc = prj_read(from, 0)) != 0)           /* the loops first, copied for the slot they go to */
        return rc;
    memcpy(&b, prj_buf + sizeof(prj_head_t), sizeof b);
    if ((rc = lop_copy(from, b.loop_gen, b.loop_nsec, to, &gen)) != 0)
        return rc;
    b.loop_gen = gen;
    memcpy(prj_buf + sizeof(prj_head_t), &b, sizeof b);
    if ((rc = prj_write(to, 0)) != 0)
        return rc;
    prj_has[to] = 1;
    prj_lgen[to] = gen;
    for (s = 1; s < PRJ_SECTS; s++)              /* (each sector read and written again, made for the slot it goes to) */
        if ((rc = prj_read(from, s)) != 0)
            continue;                            /* (a pattern sector never saved: left empty there too) */
        else if ((rc = prj_write(to, s)) != 0)
            return rc;
    return 0;
}
static int prj_delete(uint32_t k)
{
    uint32_t zero = 0;
    if (!prj_ok || k >= PRJ_SLOTS)
        return -1;
    if (prj_flash_prog(prj_sector(k, 0), &zero, 4u))   /* (the project's magic cleared: no erase, no pause) */
        return -2;
    prj_has[k] = 0;
    prj_lgen[k] = 0;                             /* (its loops' sectors free) */
    if (prj_cur == (int8_t)k)
        prj_cur = -1;                            /* (what plays stays, no longer saved anywhere) */
    return 0;
}

/* -------------------------------------------------------------- the pages --- */
/* the rows of a session's page: the operations, then the values */
enum { PR_BACK, PR_LOAD, PR_SAVE, PR_COPY, PR_DELETE, PR_TEMPO, PR_ROOT, PR_SCALE, PR_N };   /* (BACK at the top:
                                                  * SAVE on it, the list again; build 48) */
static const char *const PR_NAME[PR_N] = {"BACK", "LOAD", "SAVE", "COPY", "DELETE", "TEMPO", "ROOT", "SCALE"};
enum { PQ_NONE, PQ_LOAD, PQ_SAVE, PQ_DELETE, PQ_COPYTO, PQ_COPYOVER, PQ_SAVENEW, PQ_NEW };
static struct {
    uint8_t slot[2];             /* the slot chosen on each list (projects, patterns) */
    uint8_t row;                 /* the row chosen on the page */
    uint8_t ask, yes;            /* a question up (PQ_*), and its answer */
    uint8_t to;                  /* COPY: the slot it goes to */
    uint8_t edit;                /* the focus on the value of the row chosen */
    int16_t val;                 /* .. the value as it is being set */
    int32_t shown;               /* (what was drawn, folded into one number) */
    uint32_t scrolled;           /* when the scale's row was last drawn scrolling */
} pj;
#define C_PRJ RGB(120, 255, 200)
#define C_PAT RGB(255, 210, 120)
#define C_DIRTY RGB(255, 170, 60)

static int pg_pat(void) { return ui.page == PG_PAT; }
static uint32_t px_mutes(void)                   /* (the tracks muted, a bit each) */
{
    uint32_t t, m = 0;
    for (t = 0; t < NTRACK; t++)
        m |= (uint32_t)(trk_mute[t] != 0) << t;
    return m;
}   /* (the patterns' grid) */
static uint32_t pg_kind(void) { return (uint32_t)pg_pat(); }
static uint32_t pg_rows(void) { return PR_N; }
static uint32_t pg_slots(void) { return pg_pat() ? PAT_SLOTS : PRJ_SLOTS; }
static int pg_has(uint32_t k) { return pg_pat() ? pat_store[k].used : prj_has[k]; }
static int pg_cur(uint32_t k) { return pg_pat() ? pat_cur == k : prj_cur == (int8_t)k; }
static int pg_dirty(void) { return pg_pat() ? pat_dirty : prj_dirty; }

static void prj_done(int rc, const char *what, uint32_t k)
{
    char n[12];
    str_cpy(n, pg_pat() ? "PATTERN " : "SESSION ", 12);
    fmt_int(n + 8, (int32_t)k + 1);
    if (!rc)
        flash(what, n, C_OK);
    else if (rc == -4)
        flash("NO ROOM", "FOR LOOPS", C_BAD);    /* (the loop store full: nothing saved) */
    else if (rc == -5)
        flash(what, "LOOPS LOST", C_BAD);        /* (loaded, its loops not) */
    else
        flash(prj_ok ? "FAILED" : "NO FLASH", "", C_BAD);
    ui.force = 1;
}
static void do_load(uint32_t k)
{
    flash("LOADING", "", C_PRJ);
    ui_draw();
    prj_done(prj_load(k), "LOADED", k);
}
static void do_save(uint32_t k)
{
    flash("SAVING", "", C_PRJ);
    ui_draw();
    prj_done(prj_save(k), "SAVED", k);
}
static void do_copy(uint32_t from, uint32_t to)
{
    flash("COPYING", "", C_PRJ);
    ui_draw();
    prj_done(prj_copy(from, to), "COPIED", to);
}
static void do_delete(uint32_t k) { prj_done(prj_delete(k), "DELETED", k); }
static void do_new(uint32_t k)                   /* (an empty slot: a new session there, saved) */
{
    prj_fresh();
    flash("SAVING", "", C_PRJ);
    ui_draw();
    prj_done(prj_save(k), "NEW", k);
    ui.page = PG_MAIN;
}

/* SAVE pressed on a sessions' page */
static void prj_press(int sh)
{
    uint32_t kind = pg_kind(), k = pj.slot[kind];
    int list = ui.page == PG_PRJ;
    if (sh) {                                    /* SHIFT + SAVE: back a page (or a value left as it was) */
        if (list)
            ui.page = PG_MAIN;
        else if (pj.edit)
            pj.edit = 0;
        else if (pj.ask)
            pj.ask = PQ_NONE;
        else
            ui.page = PG_PRJ;
        ui.force = 1;
        return;
    }
    ui.force = 1;
    if (list) {                                  /* a slot: its page */
        ui.page = PG_PRJD;
        pj.row = PR_LOAD;                        /* (the page opens on LOAD, BACK above it) */
        pj.ask = PQ_NONE;
        pj.edit = 0;
        return;
    }
    if (pj.edit) {                               /* the value set: the focus back on the rows */
        pj.edit = 0;
        if (pj.row == PR_TEMPO)
            ui.tempo = pj.val;
        else if (pj.row == PR_ROOT)
            ui.root = (uint8_t)pj.val;
        else
            ui.scale = (uint8_t)pj.val;
        prj_dirty = 1;
        tempo_apply();
        return;
    }
    if (pj.ask == PQ_COPYTO) {                   /* the slot to copy to, chosen */
        if (pj.to == k)
            return;
        if (pg_has(pj.to)) {
            pj.ask = PQ_COPYOVER;
            pj.yes = 0;
        } else {
            pj.ask = PQ_NONE;
            do_copy(k, pj.to);
        }
        return;
    }
    if (pj.ask) {                                /* a question answered */
        uint8_t q = pj.ask;
        pj.ask = PQ_NONE;
        if (!pj.yes) {
            say("NO", 700);
            return;
        }
        if (q == PQ_LOAD) {
            do_load(k);
            ui.page = PG_MAIN;
        } else if (q == PQ_SAVE)
            do_save(k);
        else if (q == PQ_SAVENEW) {
            do_save(k);
            ui.page = PG_MAIN;
        } else if (q == PQ_NEW)
            do_new(k);
        else if (q == PQ_DELETE)
            do_delete(k);
        else if (q == PQ_COPYOVER)
            do_copy(k, pj.to);
        return;
    }
    switch (pj.row) {
    case PR_BACK:
        ui.page = PG_PRJ;
        break;
    case PR_LOAD:
        if (!pg_has(k)) {
            say("EMPTY", 900);
            return;
        }
        if (prj_dirty || pat_dirty) {            /* (changes not saved would go: asked first) */
            pj.ask = PQ_LOAD;
            pj.yes = 0;
        } else {
            do_load(k);
            ui.page = PG_MAIN;
        }
        break;
    case PR_SAVE:
        if (pg_has(k) && !pg_cur(k)) {           /* (over another: asked first) */
            pj.ask = PQ_SAVE;
            pj.yes = 0;
        } else if (!pg_has(k) && prj_cur >= 0) { /* an empty slot, a saved session playing: a new one there, blank
                                                  * (build 57; changes not saved would go: asked first) */
            if (prj_dirty || pat_dirty) {
                pj.ask = PQ_NEW;
                pj.yes = 0;
            } else
                do_new(k);
        } else                                   /* (nothing saved yet: what plays, into it) */
            do_save(k);
        break;
    case PR_COPY:
        if (!pg_has(k)) {
            say("EMPTY", 900);
            return;
        }
        pj.ask = PQ_COPYTO;
        pj.to = (uint8_t)((k + 1u) % pg_slots());
        break;
    case PR_DELETE:
        if (!pg_has(k)) {
            say("EMPTY", 900);
            return;
        }
        pj.ask = PQ_DELETE;
        pj.yes = 0;
        break;
    default:                                     /* TEMPO, ROOT, SCALE: the focus on the value */
        pj.edit = 1;
        pj.val = pj.row == PR_TEMPO ? ui.tempo : pj.row == PR_ROOT ? ui.root : ui.scale;
        break;
    }
}
/* PRESETS on a page: through the slots, the rows, the slot to copy to, NO / YES, or the value */
static void prj_preset(int32_t s)
{
    uint32_t kind = pg_kind();
    if (ui.page == PG_PRJ || ui.page == PG_PAT)
        pj.slot[kind] = (uint8_t)clampi((int32_t)pj.slot[kind] + s, 0, (int32_t)pg_slots() - 1);
    else if (pj.ask == PQ_COPYTO)
        pj.to = (uint8_t)clampi((int32_t)pj.to + s, 0, (int32_t)pg_slots() - 1);
    else if (pj.ask)
        pj.yes = (uint8_t)(s > 0);
    else if (pj.edit)
        pj.val = (int16_t)(pj.row == PR_TEMPO ? clampi(pj.val + s, TEMPO_MIN, TEMPO_MAX)
                           : pj.row == PR_ROOT ? (pj.val + s % 12 + 12) % 12 : clampi(pj.val + s, 0, NSCALE - 1));
    else
        pj.row = (uint8_t)clampi((int32_t)pj.row + s, 0, (int32_t)pg_rows() - 1);
}

/* the PATTERNS grid and a white key: pattern k playing (an empty one too: GLOBAL, nothing in it) */
static void pat_switch(uint32_t k)
{
    if (k >= PAT_SLOTS)
        return;
    if (mcfg.pc_out)                             /* (PROG CH OUT: the pattern asked for, 0 .. 15; build 46) */
        midi_main_send(0xC0u | (mcfg.pc_out_ch - 1u), k, 0);
    pj.slot[1] = (uint8_t)k;
    if (!seq_run) {                              /* stopped: at once (the one left kept as it was) */
        pat_stash();
        pat_give(k, 1);
        return;
    }
    seq_pend = 0;
    RING_PUBLISH();
    if (k == pat_cur)
        return;                                  /* (the one playing: the one waiting, not now) */
    pat_decode(&pat_store[k], &sb_next);         /* playing: at the end of the chosen track's time through */
    seq_pend_k = (uint8_t)k;
    seq_pend_lp = (uint8_t)k;
    seq_pend_t = ui.sel;
    seq_pend_mu = (uint8_t)px_mutes_of(&pat_store[k]);   /* (its mutes, as it comes in: build 65) */
    RING_PUBLISH();
    seq_pend = 1;
}
/* each pass of the main loop: a pattern taken in by the sequencer, its switch finished (the one it left kept in the
 * store as it was, changes and all); stopped with one waiting, at once */
static uint8_t pat_noted = 0xFF;                 /* (the pattern playing as the grid's cursor last followed it) */
/* (build 76) the PATTERNS grid, SAVE held: the pattern playing saved into the slot under the cursor, as a white key
 * held saves it (Tonitor's SAVE held on its SNAPSHOTS page) */
static void pat_save_cursor(void) { pat_save_as(pj.slot[1]); }
/* (build 69) the PATTERNS grid opened: its cursor (white) on the pattern playing -- or the one waiting to */
static void pat_grid_open(void)
{
    pj.slot[1] = (uint8_t)(seq_pend == 1u ? seq_pend_k : pat_cur);
}
static void pat_poll(void)
{
    uint32_t sp = seq_pend, was = sp == 2u ? seq_mu_was : 0u;
    if (pat_cur != pat_noted) {                  /* (another playing -- a session loaded at it, a program change, a
                                                  * switch done: the grid's cursor with it, build 69) */
        pat_noted = pat_cur;
        pj.slot[1] = pat_cur;
    }
    if (was & 0x80u)                             /* (taken in, its mutes the sequencer's already: the one left noted and
                                                  * kept with its own, build 65) */
        px_mu_over = (int16_t)(was & 0x3Fu);
    px_note();
    if (sp == 1u && !seq_run) {
        seq_pend_mu = 0;                         /* (stopped: given here, its mutes too) */
        pat_stash();
        pat_give(seq_pend_k, 1);
        ui.force = 1;
        return;
    }
    if (sp != 2u)
        return;
    if (pat_dirty && seq_pend_k != pat_cur) {   /* (the same: a break saved over it, kept) */
        memcpy(sb_old.den, ui.gen_den, NTRACK);
        memcpy(sb_old.cx, ui.gen_cx, NTRACK);
        memcpy(sb_old.trn, ui.trn, NTRACK);
        pat_encode(&pat_store[pat_cur], &sb_old);
        pat_dirtyv[pat_cur] = 1;
    }
    memcpy(ui.gen_den, sb_next.den, NTRACK);
    memcpy(ui.gen_cx, sb_next.cx, NTRACK);
    memcpy(ui.trn, sb_next.trn, NTRACK);
    pat_cur = seq_pend_k;
    pat_tempo = pat_store[pat_cur].used ? pat_store[pat_cur].tempo : 0;
    pat_dirty = pat_dirtyv[pat_cur];
    px_mu_over = -1;
    if (was & 0x80u)
        ui.solo = 0xFF;                          /* (the mutes the pattern's: no solo to undo) */
    seq_mu_was = 0;
    seq_pend_mu = 0;
    seq_pend = 0;
    px_load(&pat_store[pat_cur]);
    tempo_apply();
    ui.force = 1;
}
/* SAVE or HOME held: the session playing saved into its slot -- never saved, the first empty slot's page with SAVE
 * NEW? (YES chosen; SAVE answers) */
static void prj_quick_save(void)
{
    if (ui.page == PG_PRJD && pj.ask)            /* (a question up: answered by SAVE let go, not held) */
        return;
    if (prj_cur < 0) {
        uint32_t k;
        for (k = 0; k < PRJ_SLOTS && prj_has[k]; k++)
            ;
        if (k == PRJ_SLOTS) {
            say("NO FREE SLOT", 1500);
            return;
        }
        pj.slot[0] = (uint8_t)k;
        pj.row = PR_SAVE;
        pj.edit = 0;
        pj.ask = PQ_SAVENEW;
        pj.yes = 1;
        ui.page = PG_PRJD;
        ui.force = 1;
        return;
    }
    flash("SAVING", "", C_PRJ);
    ui_draw();
    prj_done(prj_save((uint32_t)prj_cur), "SAVED", (uint32_t)prj_cur);
}

/* ------------------------------------------------------------------ drawing --- */
/* a slot's state is its colour: loaded green (orange with changes not saved), saved white, empty grey */
static uint16_t pg_col(uint32_t k) { return pg_cur(k) ? (pg_dirty() ? C_DIRTY : C_OK) : pg_has(k) ? C_WHITE : C_GREY; }
/* a page's title, centred (squeezed to fit); the line under it, its colour (half: which page of two, its half thick) */
static void prj_title(const char *t, int half, uint16_t col, uint16_t line)
{
    cv_begin(240, 55, C_BLACK);
    cv_text_fit((50 - (int32_t)FONT_M.h) / 2, &FONT_M, t, col);
    cv_rect(0, 52, 240, 3, line);
    if (half)
        cv_rect(half == 1 ? 0 : 120, 47, 120, 8, line);
    cv_blit(0, 0);
}
/* the SESSIONS list: a row a slot, five shown, the one loaded with a block at its right */
static void draw_prj_list(void)
{
    uint32_t sel = pj.slot[0], top = sel < 2u ? 0u : sel > PRJ_SLOTS - 3u ? PRJ_SLOTS - 5u : sel - 2u, r;
    prj_title("SESSIONS", 0, C_PRJ, C_PRJ);
    for (r = 0; r < 5u; r++) {
        uint32_t k = top + r;
        char n[16];
        uint16_t c = pg_col(k);
        str_cpy(n, "SESSION ", 12);
        fmt_int(n + 8, (int32_t)k + 1);
        cv_begin(240, 37, k == sel ? C_WHITE : C_BLACK);
        cv_text(8, (37 - (int32_t)FONT_S.h) / 2, &FONT_S, n, k == sel ? C_BLACK : c);   /* (the pages' size) */
        if (pg_cur(k))
            cv_rect(212, 7, 22, 22, c);
        cv_rect(0, 36, 240, 1, RGB(50, 50, 50));
        cv_blit(0, 55 + 37 * r);
    }
}
/* the PATTERNS grid: the sixteen in four rows of four */
static void draw_pat_grid(void)
{
    uint32_t sel = pj.slot[1], page = 0, r, c;
    if (pat_clip_from >= 0) {                    /* (a pattern copied: COPY P5, blinking, until it is pasted) */
        char t[12] = "COPY P";
        int on = (int)((fm1_ms / 400u) & 1u);
        fmt_int(t + 6, pat_clip_from + 1);
        prj_title(t, 0, on ? C_BLACK : C_DIRTY, C_DIRTY);
        if (on) {
            cv_begin(240, 52, C_DIRTY);
            cv_text_fit((50 - (int32_t)FONT_M.h) / 2, &FONT_M, t, C_BLACK);
            cv_blit(0, 0);
        }
    } else if (ui.brk && ui.brk_shown != 0xFFu) {   /* (a break playing: its way, as the name band says it; build 64) */
        char m[16];
        uint32_t w = ui.brk_shown % 16u, r = ui.brk_shown / 16u;
        str_cpy(m, BR_NAME[r % NBR], sizeof m);
        if (r == BR_REPEAT || r == BR_REVERSE) {
            str_cpy(m + str_len(m), " ", 2);
            fmt_int(m + str_len(m), (int32_t)w);
        }
        prj_title(m, 0, C_DIRTY, C_PAT);
    } else if (ui.mute_mode) {                   /* (MUTE mode: the tracks in the title, playing in their colour, muted
                                                  * dark grey; build 63) */
        uint32_t t;
        cv_begin(240, 55, C_BLACK);
        for (t = 0; t < NTRACK; t++) {
            char n[2] = {(char)('1' + t), 0};
            cv_text((int32_t)t * 40 + 20 - text_w(&FONT_M, n) / 2, (50 - (int32_t)FONT_M.h) / 2, &FONT_M, n,
                    trk_mute[t] ? RGB(80, 80, 80) : TRACK_COL[t]);
        }
        cv_rect(0, 52, 240, 3, C_PAT);
        cv_blit(0, 0);
    } else
        prj_title("PATTERNS", 0, C_PAT, C_PAT);
    for (r = 0; r < 4u; r++) {
        cv_begin(240, 46, C_BLACK);
        for (c = 0; c < 4u; c++) {
            uint32_t k = page * 16u + r * 4u + c;
            int32_t x = (int32_t)c * 60;
            char n[4];
            uint16_t col = pg_col(k);
            fmt_int(n, (int32_t)k + 1);
            if (k == sel)                        /* (the one chosen: white; build 72, no frame on the one playing --
                                                  * its number green, or orange with changes) */
                cv_rect(x + 2, 2, 56, 42, C_WHITE);
            cv_text(x + 30 - text_w(&FONT_S, n) / 2, 23 - (int32_t)FONT_S.h / 2, &FONT_S, n, k == sel ? C_BLACK : col);
        }
        cv_blit(0, 55 + 46 * r);
    }
    cv_begin(240, 240 - 55 - 4 * 46, C_BLACK);
    cv_blit(0, 55 + 4 * 46);
}
/* a page's row: every name and value in the one size (S); the value in a field of its own at the right, scrolling
 * round when longer than the field (a scale's name). The operations above, the values below, a line between; DELETE
 * in red */
#define FIELD_X 138                /* (past SCALE, TEMPO and ROOT, with a gap) */
#define FIELD_W (232 - FIELD_X)
static int prj_scrolls(const char *val) { return text_w(&FONT_S, val) > FIELD_W; }
static void prj_field(uint32_t r, uint32_t k, const char *val, uint16_t fg, int chosen, int focus)
{
    int32_t y = (37 - (int32_t)FONT_S.h) / 2, w = text_w(&FONT_S, val);
    uint16_t bg = chosen && !focus ? C_WHITE : C_BLACK, vf = focus || chosen ? C_BLACK : pg_pat() ? C_PAT : C_PRJ;
    cv_begin(240, 37, bg);
    if (focus) {                                 /* (the value being set: its field in white) */
        cv_rect(FIELD_X - 6, 1, 240 - FIELD_X + 6, 35, C_WHITE);
        bg = C_WHITE;
    }
    if (val[0] && w <= FIELD_W)
        cv_text(232 - w, y, &FONT_S, val, vf);
    else if (val[0]) {                           /* (round and round: the text, a gap, the text again, 40 px a second) */
        int32_t span = w + 40, x = FIELD_X - (int32_t)((fm1_ms / 25u) % (uint32_t)span);
        cv_text(x, y, &FONT_S, val, vf);
        cv_text(x + span, y, &FONT_S, val, vf);
        cv_rect(0, 0, FIELD_X, 37, focus ? C_BLACK : bg);
        if (focus)
            cv_rect(FIELD_X - 6, 1, 6, 35, C_WHITE);
        cv_rect(234, 0, 6, 37, bg);
    }
    cv_text(8, y, &FONT_S, PR_NAME[k], chosen && !focus ? (k == PR_DELETE ? C_BAD : C_BLACK) : fg);
    cv_rect(0, 36, 240, 1, RGB(50, 50, 50));
    if (k == PR_DELETE)                          /* (the operations' section ends: a line across) */
        cv_rect(0, 33, 240, 4, pg_pat() ? C_PAT : C_PRJ);
    cv_blit(0, 55 + 37 * r);
}
/* row k's value as it reads (the value being set, while the focus is on it) */
static void prj_value(uint32_t k, char *v, uint32_t n)
{
    int32_t x = pj.edit && pj.row == k ? pj.val
                : k == PR_TEMPO ? ui.tempo : k == PR_ROOT ? ui.root : ui.scale;
    v[0] = 0;
    if (k == PR_TEMPO)
        fmt_int(v, x);
    else if (k == PR_ROOT)
        str_cpy(v, NOTE_NAME[x % 12], n);
    else if (k == PR_SCALE) {
        str_cpy(v, SCALE_L1[x % NSCALE], n);
        if (SCALE_L2[x % NSCALE][0]) {
            str_cpy(v + str_len(v), " ", 2);
            str_cpy(v + str_len(v), SCALE_L2[x % NSCALE], n - str_len(v));
        }
    }
}
static void draw_noyes_rows(const char *q, uint16_t col)
{
    uint32_t r;
    for (r = 0; r < 5u; r++) {
        cv_begin(240, 37, C_BLACK);
        if (r == 1u)
            cv_text_fit((37 - (int32_t)FONT_M.h) / 2, &FONT_M, q, col);
        else if (r == 3u) {                      /* NO | YES, the answer chosen in white */
            cv_rect(10, 0, 100, 37, pj.yes ? C_BLACK : C_WHITE);
            cv_rect(130, 0, 100, 37, pj.yes ? C_WHITE : C_BLACK);
            cv_frame(10, 0, 100, 37, 2, C_GREY);
            cv_frame(130, 0, 100, 37, 2, C_GREY);
            cv_text(60 - text_w(&FONT_M, "NO") / 2, (37 - (int32_t)FONT_M.h) / 2, &FONT_M, "NO", pj.yes ? C_GREY : C_BLACK);
            cv_text(180 - text_w(&FONT_M, "YES") / 2, (37 - (int32_t)FONT_M.h) / 2, &FONT_M, "YES", pj.yes ? C_BLACK : C_GREY);
        }
        cv_blit(0, 55 + 37 * r);
    }
}
static uint32_t prj_top(void)
{
    uint32_t n = pg_rows();
    return n <= 5u || pj.row < 2u ? 0u : pj.row > n - 3u ? n - 5u : pj.row - 2u;
}
static void draw_prj_page(void)
{
    uint32_t k = pj.slot[pg_kind()], r, top = prj_top();
    char t[16], v[20];
    uint16_t line = pg_pat() ? C_PAT : C_PRJ;
    str_cpy(t, "SESSION ", 12);
    fmt_int(t + 8, (int32_t)k + 1);
    prj_title(t, 0, pg_col(k), line);
    if (pj.ask == PQ_COPYTO) {                   /* the slot to copy to, very large */
        char n[8];
        fmt_int(n, pj.to + 1);
        CV_AREA(0, 55, 240, 185, C_BLACK) {
            cv_text_c(4, &FONT_S, "COPY TO", line);
            cv_text_c(36, &FONT_XL, n, pj.to == k ? C_GREY : pg_has(pj.to) ? C_BAD : C_WHITE);
        }
        return;
    }
    if (pj.ask) {
        draw_noyes_rows(pj.ask == PQ_LOAD ? "LOAD?" : pj.ask == PQ_DELETE ? "DELETE?" : pj.ask == PQ_SAVENEW ? "SAVE NEW?"
                        : pj.ask == PQ_NEW ? "NEW SESSION?" : "OVERWRITE?",
                        pj.ask == PQ_LOAD || pj.ask == PQ_NEW ? C_DIRTY : pj.ask == PQ_SAVENEW ? C_PRJ : C_BAD);
        return;
    }
    for (r = 0; r < 5u; r++) {
        uint32_t i = top + r;
        if (i >= pg_rows()) {
            cv_begin(240, 37, C_BLACK);
            cv_blit(0, 55 + 37 * r);
            continue;
        }
        prj_value(i, v, sizeof v);
        prj_field(r, i, v, i == PR_DELETE ? C_BAD : i < PR_TEMPO && !pg_has(k) && i != PR_SAVE && i != PR_BACK ? C_GREY : C_WHITE,
                  i == pj.row, pj.edit && i == pj.row);
    }
}
/* the page, drawn again when what it shows has changed (and the scale's name, scrolling, ~25 times a second) */
static void prj_draw(void)
{
    uint32_t kind = pg_kind();
    int32_t key = ui.page * 1000000 + pj.slot[kind] * 10000 + pj.row * 1000 + pj.ask * 100 + pj.yes * 50 + pj.to +
                  ui.tempo * 7 + ui.root * 131 + ui.scale * 977 + prj_dirty * 3 + prj_cur * 13 + pj.edit * 5000000 +
                  pj.val * 31 + pat_cur * 17 + pat_tempo * 11 + pat_dirty * 5 + pat_store[pj.slot[1]].used * 23 +
                  (pat_clip_from + 1) * 41 + (pat_clip_from >= 0 ? (int32_t)((fm1_ms / 400u) & 1u) * 7777 : 0) +
                  (ui.page == PG_PAT && ui.mute_mode ? (int32_t)(64u + px_mutes()) * 100003 : 0) +
                  (ui.page == PG_PAT && ui.brk ? (int32_t)ui.brk_shown * 3001 + 7 : 0);
    if (!ui.force && ui.shown_page == ui.page && key == pj.shown) {
        uint32_t top = prj_top();
        char v[20];
        if (ui.page != PG_PRJD || pj.ask || PR_SCALE < top || PR_SCALE >= top + 5u)
            return;
        prj_value(PR_SCALE, v, sizeof v);
        if (prj_scrolls(v) && (uint32_t)(fm1_ms - pj.scrolled) >= 40u) {
            prj_field(PR_SCALE - top, PR_SCALE, v, C_WHITE, pj.row == PR_SCALE, pj.edit && pj.row == PR_SCALE);
            pj.scrolled = fm1_ms;
        }
        return;
    }
    if (ui.page == PG_PRJ)
        draw_prj_list();
    else if (ui.page == PG_PAT)
        draw_pat_grid();
    else
        draw_prj_page();
    pj.shown = key;
}
