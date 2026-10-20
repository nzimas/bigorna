/* SPDX-License-Identifier: GPL-3.0-only
 * Sessions and their patterns, in the FM-1's flash (the user data area TONITOR kept its sessions in).
 *
 * 16 sessions, four 4 KB sectors each: the session (every track's machine, its sound on each machine, octave, level
 * and pan; the send effects; the tempo, root and scale), then its 16 patterns, six a sector (a pattern, 680 bytes:
 * each track's 16 steps and its length; its tempo, kept for later, GLOBAL -- the session's -- for now).
 * The session playing is kept in RAM whole, its 16 patterns with it; the pattern playing is one of them, its steps
 * in the sequencer's (tracks.c) as they are edited.
 * (In the code a session is a "project".)
 *
 *   SAVE              the SESSIONS page: SESSION 1 .. 16; PRESETS moves through them, SAVE opens one's page. On the
 *                     page SAVE runs LOAD, SAVE, COPY or DELETE, or moves the focus to a value: PRESETS changes it,
 *                     SAVE sets it, SHIFT + SAVE leaves it as it was; SHIFT + SAVE goes back a page
 *   ARP               the PATTERNS grid, 1 .. 16 in four rows of four: PRESETS moves through them, ARP loads the one
 *                     chosen. The white keys are the 16 patterns there, lit dimly (the one playing brighter): a white
 *                     key let go soon loads its pattern (the grid stays); held 0.8 s, copies it (COPY P5 above the
 *                     grid, blinking, and its key) -- then another held 0.8 s pastes it there, saved and loaded (the
 *                     same again, or off the grid: let go); SHIFT and held 0.8 s, empties it (the one playing too)
 *   HOME held         the session playing saved, into its slot
 *   ARP held          the pattern playing saved, into its slot of the session (and into the flash if the session has
 *                     a slot)
 *   a track's button  back to playing
 * A session loads with its pattern 1; so does the start.
 * LOAD with changes not saved, SAVE over another, DELETE, and COPY over another ask first: NO / YES on PRESETS, SAVE
 * answers. COPY asks for the slot to copy to (PRESETS), then SAVE.
 * A session's SAVE stores the pattern playing into its slot too, then the whole session.
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
} prj_body_t;
#define PRJ_LEN23 __builtin_offsetof(prj_body_t, cp)
#define PRJ_LEN28 __builtin_offsetof(prj_body_t, sc_src)
#define PRJ_LEN32 __builtin_offsetof(prj_body_t, chord_on)
static void prj_ch0(prj_body_t *b)               /* (a session from before build 33: CHORD mode off) */
{
    memset(b->chord_on, 0, NTRACK);
    memset(b->chord_form, 0, NTRACK);
}
static void prj_sc0(prj_body_t *b)               /* (a session from before build 29: no track ducked) */
{
    memset(b->sc_src, 0, NTRACK);
    memset(b->sc_lvl, 64, NTRACK);
    prj_ch0(b);
}
typedef struct {
    uint8_t used;                /* saved */
    uint8_t pad;
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
#define SEQ_REC4 32u
#define SEQ_NREC 7u
#define SEQ_REC 8u
#define PAT_SEQ_ROOM 676u
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
/* a session saved before build 18, in prj_buf, made one of now in place: each sound moved up to its new place, the
 * last first (each lands at or after where it was), MANGLE all off; its head as if saved so */
static void prj_from17(void)
{
    prj_body_t *b = (prj_body_t *)(void *)(prj_buf + sizeof(prj_head_t));
    const sound17_t *o = (const sound17_t *)(const void *)((const uint8_t *)b + __builtin_offsetof(prj_body_t, snd));
    int32_t n;
    for (n = NTRACK * NENG - 1; n >= 0; n--) {
        sound17_t a = o[n];
        sound_t *d = &b->snd[0][0] + n;
        memset(d, 0, sizeof *d);
        d->mach = a.mach;
        d->fmach = a.fmach;
        memcpy(d->syn, a.syn, sizeof d->syn);
        memcpy(d->flt, a.flt, NF);
        memcpy(d->amp, a.amp, NA);
        memcpy(d->fx, a.fx, NX);
        d->fx[X_SPARE] = 0;
        d->family = a.family;
        d->seed = a.seed;
    }
    memcpy(((prj_body_t *)(void *)(prj_buf + sizeof(prj_head_t)))->cp, CP0, NCP);
    prj_sc0((prj_body_t *)(void *)(prj_buf + sizeof(prj_head_t)));
    prj_head_fill(PRJ_MAGIC, ((prj_head_t *)(void *)prj_buf)->what, sizeof(prj_body_t));
}
static int prj_read(uint32_t k, uint32_t s)
{
    uint32_t magic = s ? PAT_MAGIC : PRJ_MAGIC, len = s ? PAT_PER * sizeof(pat_t) : sizeof(prj_body_t);
    const prj_head_t *h = (const prj_head_t *)(void *)prj_buf;
    if (!prj_ok || k >= PRJ_SLOTS || prj_flash_read(prj_sector(k, s), prj_buf, sizeof(prj_head_t) + len))
        return -1;
    if (!s && prj_head_ok(h, magic, k * 16u, sizeof(prj_body17_t)) &&
        h->body_crc == prj_crc(prj_buf + sizeof(prj_head_t), sizeof(prj_body17_t))) {
        prj_from17();                            /* (saved before build 18) */
        return 0;
    }
    if (!s && prj_head_ok(h, magic, k * 16u, PRJ_LEN23) &&
        h->body_crc == prj_crc(prj_buf + sizeof(prj_head_t), PRJ_LEN23)) {   /* (builds 18 .. 23: no compressor --
                                                                             * its start values) */
        memcpy(((prj_body_t *)(void *)(prj_buf + sizeof(prj_head_t)))->cp, CP0, NCP);
        prj_sc0((prj_body_t *)(void *)(prj_buf + sizeof(prj_head_t)));
        prj_head_fill(PRJ_MAGIC, k * 16u, sizeof(prj_body_t));
        return 0;
    }
    if (!s && prj_head_ok(h, magic, k * 16u, PRJ_LEN28) &&
        h->body_crc == prj_crc(prj_buf + sizeof(prj_head_t), PRJ_LEN28)) {   /* (builds 24 .. 28: no track's
                                                                             * sidechain) */
        prj_sc0((prj_body_t *)(void *)(prj_buf + sizeof(prj_head_t)));
        prj_head_fill(PRJ_MAGIC, k * 16u, sizeof(prj_body_t));
        return 0;
    }
    if (!s && prj_head_ok(h, magic, k * 16u, PRJ_LEN32) &&
        h->body_crc == prj_crc(prj_buf + sizeof(prj_head_t), PRJ_LEN32)) {   /* (builds 29 .. 32: no CHORD mode) */
        prj_ch0((prj_body_t *)(void *)(prj_buf + sizeof(prj_head_t)));
        prj_head_fill(PRJ_MAGIC, k * 16u, sizeof(prj_body_t));
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
static void prj_scan(int flash_ok)
{
    uint32_t k;
    prj_ok = (uint8_t)flash_ok;
    for (k = 0; k < PRJ_SLOTS; k++) {
        prj_head_t h;
        prj_has[k] = 0;
        if (prj_ok && !prj_flash_read(prj_sector(k, 0), &h, sizeof h))
            prj_has[k] = (uint8_t)(prj_head_ok(&h, PRJ_MAGIC, k * 16u, sizeof(prj_body_t)) ||
                                   prj_head_ok(&h, PRJ_MAGIC, k * 16u, sizeof(prj_body17_t)) ||
                                   prj_head_ok(&h, PRJ_MAGIC, k * 16u, PRJ_LEN23) ||
                                   prj_head_ok(&h, PRJ_MAGIC, k * 16u, PRJ_LEN28) ||
                                   prj_head_ok(&h, PRJ_MAGIC, k * 16u, PRJ_LEN32));
    }
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
/* steps b as a pattern (what will not fit: the last steps' locks, then the steps, left out) */
static void pat_encode(pat_t *p, const seqbuf_t *b)
{
    uint32_t t, i, o = SEQ_REC4, n = 0;
    memset(p, 0, sizeof *p);
    p->used = 1;
    p->tempo = pat_tempo;
    p->seq[SEQ_FMT] = SEQ_V4;
    for (t = 0; t < NTRACK; t++) {
        p->seq[SEQ_LEN + t] = (uint8_t)(b->len[t] | ((b->div[t] + SEQ_DIV1) % NDIV) << 5);
        p->seq[SEQ_DIR + t] = b->dir[t];
        p->seq[SEQ_DEN + t] = b->den[t];
        p->seq[SEQ_CX + t] = b->cx[t];
        for (i = 0; i < NSTEP; i++) {
            uint32_t j, nl = pl_nb(b, t, i), at, md = (uint32_t)mod_on(b, t, i), ch = b->chf[t][i] != 0;
            if (!b->note[t][i] || o + 6u > PAT_SEQ_ROOM)
                continue;
            if (md && o + 9u > PAT_SEQ_ROOM)
                md = 0;
            if (ch && o + 6u + 3u * md + 1u > PAT_SEQ_ROOM)
                ch = 0;
            while (nl && o + 6u + 3u * md + ch + 3u * nl > PAT_SEQ_ROOM)
                nl--;
            p->seq[o++] = (uint8_t)(t * NSTEP + i);
            p->seq[o++] = b->note[t][i];
            p->seq[o++] = b->vel[t][i];
            p->seq[o++] = (uint8_t)(b->glen[t][i] | b->rtg[t][i] << 4);
            p->seq[o++] = b->cond[t][i];
            at = o++;
            p->seq[at] = 0;
            if (md) {                            /* (MOD) */
                p->seq[o++] = (uint8_t)b->mut[t][i];
                p->seq[o++] = (uint8_t)(b->mcy[t][i] | b->fcy[t][i] << 4);
                p->seq[o++] = b->mfx[t][i];
            }
            if (ch)                              /* (a chord: its form + 1; bit 6 of the locks' count, build 33) */
                p->seq[o++] = b->chf[t][i];
            for (j = 0; j < NPL && (p->seq[at] & 63u) < nl; j++) {
                uint32_t id = b->lid[t][i][j];
                int32_t v = b->lv[t][i][j];
                if (id == PL_NONE)
                    continue;
                p->seq[o++] = (uint8_t)id;
                p->seq[o++] = (uint8_t)v;
                p->seq[o++] = (uint8_t)((uint32_t)v >> 8);
                p->seq[at]++;
            }
            if (md)
                p->seq[at] |= 0x80u;
            if (ch)
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
/* slot k of the store, as the pattern playing, at once */
static void pat_give(uint32_t k)
{
    const pat_t *p = &pat_store[k];
    seq_pend = 0;                                /* (a pattern waiting: not now) */
    pat_decode(p, &sb_tmp);
    seq_copy_in(&sb_tmp);
    memcpy(ui.gen_den, sb_tmp.den, NTRACK);
    memcpy(ui.gen_cx, sb_tmp.cx, NTRACK);
    pat_cur = (uint8_t)k;
    pat_tempo = p->used ? p->tempo : 0;
    pat_dirty = pat_dirtyv[k];
    tempo_apply();
}
/* the pattern playing, changed: kept in its slot of the store as it is (the flash, when it is saved) */
static void pat_stash(void)
{
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
    pat_cur = (uint8_t)k;
    pat_dirty = 0;
    pat_dirtyv[k] = 0;
    prj_dirty = 1;                               /* (the project's pattern changed: saved with it) */
    return pat_flush(k);
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
    if (seq_pend == 1u && seq_pend_k == k)
        seq_pend = 0;                            /* (it was waiting to play: not now) */
    if (k == pat_cur)
        pat_give(k);
    rc = pat_flush(k);
    fmt_int(n + 8, (int32_t)k + 1);
    flash(rc ? "FAILED" : "EMPTY", n, rc ? C_BAD : RGB(255, 210, 120));
    ui.force = 1;
}

/* the PATTERNS grid, a white key held: pattern k copied (the one playing as it plays, its changes too); a pattern
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
    if (pat_clip_from >= 0) {                    /* pasted */
        pat_store[k] = pat_clip;
        pat_dirtyv[k] = 0;
        rc = pat_flush(k);
        pat_switch(k);
        pat_clip_from = -1;
        prj_dirty = 1;
        flash(rc ? "FAILED" : "PASTED", rc ? "" : n, rc ? C_BAD : C_OK);
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
    memcpy(b->snd, ui.snd, sizeof b->snd);
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
    }
    ui.tempo = (int16_t)clampi(b->tempo, TEMPO_MIN, TEMPO_MAX);
    ui.root = (uint8_t)(b->root % 12u);
    ui.scale = (uint8_t)(b->scale < NSCALE ? b->scale : 0);
}
static int prj_save(uint32_t k)              /* the pattern playing into its slot, then the whole project */
{
    uint32_t s;
    int rc;
    pat_take(&pat_store[pat_cur]);
    pat_dirty = 0;
    prj_take((prj_body_t *)(void *)(prj_buf + sizeof(prj_head_t)));
    if ((rc = prj_write(k, 0)) != 0)
        return rc;
    for (s = 0; s < PAT_SECTS; s++) {
        memcpy(prj_buf + sizeof(prj_head_t), &pat_store[s * PAT_PER], PAT_PER * sizeof(pat_t));
        if ((rc = prj_write(k, 1u + s)) != 0)
            return rc;
    }
    prj_has[k] = 1;
    prj_cur = (int8_t)k;
    prj_dirty = 0;
    memset(pat_dirtyv, 0, sizeof pat_dirtyv);    /* (every pattern in the flash now) */
    return 0;
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
    pat_give(0);                                 /* (a session starts at its pattern 1) */
    prj_dirty = 0;
    return 0;
}
static int prj_copy(uint32_t from, uint32_t to)
{
    uint32_t s;
    int rc;
    for (s = 0; s < PRJ_SECTS; s++)              /* (each sector read and written again, made for the slot it goes to) */
        if ((rc = prj_read(from, s)) != 0 && s == 0u)
            return rc;
        else if (rc)
            continue;                            /* (a pattern sector never saved: left empty there too) */
        else if ((rc = prj_write(to, s)) != 0)
            return rc;
    prj_has[to] = 1;
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
    if (prj_cur == (int8_t)k)
        prj_cur = -1;                            /* (what plays stays, no longer saved anywhere) */
    return 0;
}

/* -------------------------------------------------------------- the pages --- */
/* the rows of a session's page: the operations, then the values */
enum { PR_LOAD, PR_SAVE, PR_COPY, PR_DELETE, PR_TEMPO, PR_ROOT, PR_SCALE, PR_N };
static const char *const PR_NAME[PR_N] = {"LOAD", "SAVE", "COPY", "DELETE", "TEMPO", "ROOT", "SCALE"};
enum { PQ_NONE, PQ_LOAD, PQ_SAVE, PQ_DELETE, PQ_COPYTO, PQ_COPYOVER };
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

static int pg_pat(void) { return ui.page == PG_PAT; }   /* (the patterns' grid) */
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

/* SAVE pressed on a sessions' page; ARP on the patterns' grid */
static void prj_press(int sh)
{
    uint32_t kind = pg_kind(), k = pj.slot[kind];
    int list = ui.page == PG_PRJ || ui.page == PG_PAT;
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
    if (kind) {                                  /* the patterns' grid: the one chosen loaded */
        char n[12] = "PATTERN ";
        pat_switch(k);
        fmt_int(n + 8, (int32_t)k + 1);
        ui.page = PG_MAIN;
        say(n, 900);
        return;
    }
    if (list) {                                  /* a slot: its page */
        ui.page = PG_PRJD;
        pj.row = 0;
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
        else if (q == PQ_DELETE)
            do_delete(k);
        else if (q == PQ_COPYOVER)
            do_copy(k, pj.to);
        return;
    }
    switch (pj.row) {
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
        } else
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
    pj.slot[1] = (uint8_t)k;
    if (!seq_run) {                              /* stopped: at once (the one left kept as it was) */
        pat_stash();
        pat_give(k);
        return;
    }
    seq_pend = 0;
    RING_PUBLISH();
    if (k == pat_cur)
        return;                                  /* (the one playing: the one waiting, not now) */
    pat_decode(&pat_store[k], &sb_next);         /* playing: at the end of the chosen track's time through */
    seq_pend_k = (uint8_t)k;
    seq_pend_t = ui.sel;
    RING_PUBLISH();
    seq_pend = 1;
}
/* each pass of the main loop: a pattern taken in by the sequencer, its switch finished (the one it left kept in the
 * store as it was, changes and all); stopped with one waiting, at once */
static void pat_poll(void)
{
    if (seq_pend == 1u && !seq_run) {
        pat_stash();
        pat_give(seq_pend_k);
        ui.force = 1;
        return;
    }
    if (seq_pend != 2u)
        return;
    if (pat_dirty) {
        memcpy(sb_old.den, ui.gen_den, NTRACK);
        memcpy(sb_old.cx, ui.gen_cx, NTRACK);
        pat_encode(&pat_store[pat_cur], &sb_old);
        pat_dirtyv[pat_cur] = 1;
    }
    memcpy(ui.gen_den, sb_next.den, NTRACK);
    memcpy(ui.gen_cx, sb_next.cx, NTRACK);
    pat_cur = seq_pend_k;
    pat_tempo = pat_store[pat_cur].used ? pat_store[pat_cur].tempo : 0;
    pat_dirty = pat_dirtyv[pat_cur];
    seq_pend = 0;
    tempo_apply();
    ui.force = 1;
}
/* HOME held: the session playing saved into its slot; ARP held: the pattern playing into its slot */
static void prj_quick_save(void)
{
    if (prj_cur < 0) {
        say("NO SESSION", 1200);                 /* (never saved: its slot chosen on the SESSIONS page) */
        return;
    }
    flash("SAVING", "", C_PRJ);
    ui_draw();
    prj_done(prj_save((uint32_t)prj_cur), "SAVED", (uint32_t)prj_cur);
}
static void pat_quick_save(void)
{
    char n[12] = "PATTERN ";
    int rc = pat_save(pat_cur);
    fmt_int(n + 8, pat_cur + 1);
    flash(rc ? "FAILED" : "SAVED", rc ? "" : n, rc ? C_BAD : C_OK);
    ui.force = 1;
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
            if (k == sel)
                cv_rect(x + 2, 2, 56, 42, C_WHITE);
            else if (pg_cur(k))
                cv_frame(x + 2, 2, 56, 42, 3, col);
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
        draw_noyes_rows(pj.ask == PQ_LOAD ? "LOAD?" : pj.ask == PQ_DELETE ? "DELETE?" : "OVERWRITE?",
                        pj.ask == PQ_LOAD ? C_DIRTY : C_BAD);
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
        prj_field(r, i, v, i == PR_DELETE ? C_BAD : i < PR_TEMPO && !pg_has(k) && i != PR_SAVE ? C_GREY : C_WHITE,
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
                  (pat_clip_from + 1) * 41 + (pat_clip_from >= 0 ? (int32_t)((fm1_ms / 400u) & 1u) * 7777 : 0);
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
