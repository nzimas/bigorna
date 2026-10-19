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
 *                     chosen
 *   SHIFT + ARP held  the PATTERNS grid shown and the white keys lit dimly (the pattern playing's brighter): a white
 *                     key let go soon loads its pattern; held 0.8 s, empties it (the one playing too); let go of
 *                     SHIFT or ARP, back to what was shown
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
} prj_body_t;
typedef struct {
    uint8_t used;                /* saved */
    uint8_t pad;
    uint16_t tempo;              /* 0: GLOBAL (the project's) */
    uint8_t seq[676];            /* the steps: SEQ_LEN, SEQ_NOTE, SEQ_VEL, SEQ_GLEN, SEQ_COND (the rest for later) */
} pat_t;
#define SEQ_LEN 0u               /* seq[t]: track t's length (0: 16) */
#define SEQ_NOTE 8u              /* seq[8 + 16 t + i]: track t's step i, its note (0: empty) */
#define SEQ_VEL 104u             /* .. its velocity (0: VEL_DEF, as build 9 left it) */
#define SEQ_GLEN 200u            /* .. its length + 1 (0: GLEN_DEF) */
#define SEQ_COND 296u            /* .. its trig condition */
typedef char prj_fits[sizeof(prj_head_t) + sizeof(prj_body_t) <= PRJ_SECT ? 1 : -1];
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
static int prj_read(uint32_t k, uint32_t s)
{
    uint32_t magic = s ? PAT_MAGIC : PRJ_MAGIC, len = s ? PAT_PER * sizeof(pat_t) : sizeof(prj_body_t);
    const prj_head_t *h = (const prj_head_t *)(void *)prj_buf;
    if (!prj_ok || k >= PRJ_SLOTS || prj_flash_read(prj_sector(k, s), prj_buf, sizeof(prj_head_t) + len))
        return -1;
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
            prj_has[k] = (uint8_t)prj_head_ok(&h, PRJ_MAGIC, k * 16u, sizeof(prj_body_t));
    }
}

/* ---------------------------------------------------------------- patterns --- */
/* the tempo playing: the pattern's, or (GLOBAL) the project's; the delay follows it */
static void tempo_apply(void)
{
    fx_bpm10 = (pat_tempo ? pat_tempo : ui.tempo) * 10;
    fx_cook_all();
}
static void pat_take(pat_t *p)               /* the pattern playing, into a slot of the store */
{
    uint32_t t, i;
    memset(p, 0, sizeof *p);
    p->used = 1;
    p->tempo = pat_tempo;
    for (t = 0; t < NTRACK; t++) {
        p->seq[SEQ_LEN + t] = seq_len[t];
        for (i = 0; i < NSTEP; i++) {
            uint32_t o = t * NSTEP + i;
            p->seq[SEQ_NOTE + o] = seq_note[t][i];
            p->seq[SEQ_VEL + o] = seq_vel[t][i];
            p->seq[SEQ_GLEN + o] = (uint8_t)(seq_glen[t][i] + 1u);
            p->seq[SEQ_COND + o] = seq_cond[t][i];
        }
    }
}
static void pat_give(uint32_t k)             /* slot k of the store, as the pattern playing (empty: no steps) */
{
    const pat_t *p = &pat_store[k];
    uint32_t t, i;
    pat_cur = (uint8_t)k;
    pat_tempo = p->used ? p->tempo : 0;
    for (t = 0; t < NTRACK; t++) {
        seq_len[t] = p->used && p->seq[SEQ_LEN + t] <= NSTEP ? p->seq[SEQ_LEN + t] : 0;
        for (i = 0; i < NSTEP; i++) {
            uint32_t o = t * NSTEP + i, g = p->used ? p->seq[SEQ_GLEN + o] : 0u;
            seq_vel[t][i] = (uint8_t)(p->used && p->seq[SEQ_VEL + o] ? p->seq[SEQ_VEL + o] : VEL_DEF);
            seq_glen[t][i] = (uint8_t)(g && g <= NGLEN ? g - 1u : GLEN_DEF);
            seq_cond[t][i] = (uint8_t)(p->used && p->seq[SEQ_COND + o] < NCOND ? p->seq[SEQ_COND + o] : CD_NONE);
            seq_note[t][i] = p->used ? p->seq[SEQ_NOTE + o] : 0;
        }
    }
    pat_dirty = 0;
    tempo_apply();
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
    prj_dirty = 1;                               /* (the project's pattern changed: saved with it) */
    return pat_flush(k);
}
/* SHIFT + ARP and a white key held: pattern k emptied (the one playing too: its steps gone) */
static void pat_clear(uint32_t k)
{
    char n[12] = "PATTERN ";
    int rc;
    if (k >= PAT_SLOTS)
        return;
    memset(&pat_store[k], 0, sizeof pat_store[k]);
    if (k == pat_cur)
        pat_give(k);
    rc = pat_flush(k);
    fmt_int(n + 8, (int32_t)k + 1);
    flash(rc ? "FAILED" : "EMPTY", n, rc ? C_BAD : RGB(255, 210, 120));
    ui.force = 1;
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

/* SHIFT + ARP and a white key: pattern k playing (an empty one too: GLOBAL, nothing in it) */
static void pat_switch(uint32_t k)
{
    if (k >= PAT_SLOTS)
        return;
    pat_give(k);
    pj.slot[1] = (uint8_t)k;
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
                  pj.val * 31 + pat_cur * 17 + pat_tempo * 11 + pat_dirty * 5 + pat_store[pj.slot[1]].used * 23;
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
