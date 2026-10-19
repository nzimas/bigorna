/* SPDX-License-Identifier: GPL-3.0-only
 * Projects: 32 slots in the FM-1's flash, one 4 KB sector each (the user data area TONITOR kept its sessions in; the
 * sectors after the 32nd are left for patterns). A project holds every track's machine, its sound on each machine,
 * octave, level and pan; the send effects; the tempo, the root and the scale.
 *
 *   SAVE                the PROJECTS page: the slots PROJECT 1 .. 32, five at a time, PRESETS moves through them
 *   SAVE (on a slot)    its page: LOAD, SAVE, COPY, DELETE, TEMPO, ROOT, SCALE, five at a time; PRESETS moves through
 *                       them, SAVE runs LOAD, SAVE, COPY or DELETE, SELECT changes TEMPO (10 .. 300 BPM), ROOT and
 *                       SCALE -- the project playing's (they are saved with it)
 *   SHIFT + SAVE        back a page; a track's button, back to playing
 * LOAD with changes not saved, SAVE over another project, DELETE and COPY over a project ask first: NO / YES on
 * PRESETS, SAVE answers. COPY asks for the slot to copy to (PRESETS), then SAVE.
 *
 * A record: a head (magic, format, slot, length, the body's CRC, the head's own), then the body. A sector whose head
 * does not check (empty, erased, another firmware's) is an empty slot. */

#define PRJ_LO 0x00097000u
#define PRJ_SECT 4096u
#define PRJ_SLOTS 32u
#define PRJ_MAGIC 0x314E4742u   /* "BGN1" */
#define PRJ_FORMAT 1u
#define TEMPO_MIN 10
#define TEMPO_MAX 300

typedef struct { uint32_t magic; uint16_t format, slot; uint32_t len; uint16_t body_crc, head_crc; } prj_head_t;
typedef struct {
    uint16_t tempo;
    uint8_t root, scale;
    uint8_t eng[NTRACK];
    int8_t oct[NTRACK], level[NTRACK], pan[NTRACK];
    uint8_t dl[NDL], rv[NRV], ch[NCH];
    sound_t snd[NTRACK][NENG];
} prj_body_t;
typedef char prj_fits[sizeof(prj_head_t) + sizeof(prj_body_t) <= PRJ_SECT ? 1 : -1];

static int prj_flash_read(uint32_t off, void *p, uint32_t n);    /* (main.c: the area's own, bounded) */
static int prj_flash_erase(uint32_t off);
static int prj_flash_prog(uint32_t off, const void *p, uint32_t n);

static uint8_t prj_buf[PRJ_SECT] __attribute__((aligned(4)));
static uint8_t prj_has[PRJ_SLOTS];      /* a project in the slot */
static int8_t prj_cur = -1;             /* the slot of the project playing, -1 none (never saved) */
static uint8_t prj_dirty;               /* changes since it was loaded or saved */
static uint8_t prj_ok;                  /* the flash answered */

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
static int prj_head_ok(const prj_head_t *h, uint32_t slot)
{
    return h->magic == PRJ_MAGIC && h->format == PRJ_FORMAT && h->slot == slot && h->len == sizeof(prj_body_t) &&
           h->head_crc == prj_crc((const uint8_t *)h, __builtin_offsetof(prj_head_t, head_crc));
}
static void prj_scan(int flash_ok)
{
    uint32_t k;
    prj_ok = (uint8_t)flash_ok;
    for (k = 0; k < PRJ_SLOTS; k++) {
        prj_head_t h;
        prj_has[k] = 0;
        if (prj_ok && !prj_flash_read(PRJ_LO + k * PRJ_SECT, &h, sizeof h))
            prj_has[k] = (uint8_t)prj_head_ok(&h, k);
    }
}
/* slot k's record into prj_buf; 0 if it is whole */
static int prj_read(uint32_t k)
{
    const prj_head_t *h = (const prj_head_t *)(void *)prj_buf;
    if (!prj_ok || k >= PRJ_SLOTS || prj_flash_read(PRJ_LO + k * PRJ_SECT, prj_buf, sizeof(prj_head_t) + sizeof(prj_body_t)))
        return -1;
    if (!prj_head_ok(h, k) || h->body_crc != prj_crc(prj_buf + sizeof(prj_head_t), sizeof(prj_body_t)))
        return -2;
    return 0;
}
/* prj_buf (its body filled) into slot k: erased, written, read back */
static int prj_write(uint32_t k)
{
    prj_head_t *h = (prj_head_t *)(void *)prj_buf;
    uint32_t n = sizeof(prj_head_t) + sizeof(prj_body_t);
    if (!prj_ok || k >= PRJ_SLOTS)
        return -1;
    h->magic = PRJ_MAGIC;
    h->format = PRJ_FORMAT;
    h->slot = (uint16_t)k;
    h->len = sizeof(prj_body_t);
    h->body_crc = prj_crc(prj_buf + sizeof(prj_head_t), sizeof(prj_body_t));
    h->head_crc = prj_crc(prj_buf, __builtin_offsetof(prj_head_t, head_crc));
    if (prj_flash_erase(PRJ_LO + k * PRJ_SECT) || prj_flash_prog(PRJ_LO + k * PRJ_SECT, prj_buf, n))
        return -2;
    {   /* (read back: the head and the body's CRC) */
        uint16_t c = h->body_crc;
        if (prj_read(k) || ((prj_head_t *)(void *)prj_buf)->body_crc != c)
            return -3;
    }
    prj_has[k] = 1;
    return 0;
}

/* the state playing, into a body .. */
static void prj_take(prj_body_t *b)
{
    uint32_t t;
    memset(b, 0, sizeof *b);
    b->tempo = (uint16_t)ui.tempo;
    b->root = ui.root;
    b->scale = ui.scale;
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
/* .. and back: every note let go, every track's sound published again */
static void prj_give(const prj_body_t *b)
{
    uint32_t t;
    for (t = 0; t < NTRACK; t++) {
        ev_post(EV_STOP, t, 0, 0);
        ui.latch[t] = 0;
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
    fx_bpm10 = ui.tempo * 10;
    fx_cook_all();
}

static int prj_save(uint32_t k)
{
    int rc;
    prj_take((prj_body_t *)(void *)(prj_buf + sizeof(prj_head_t)));
    rc = prj_write(k);
    if (!rc) {
        prj_cur = (int8_t)k;
        prj_dirty = 0;
    }
    return rc;
}
static int prj_load(uint32_t k)
{
    int rc = prj_read(k);
    if (rc)
        return rc;
    prj_give((const prj_body_t *)(const void *)(prj_buf + sizeof(prj_head_t)));
    prj_cur = (int8_t)k;
    prj_dirty = 0;
    return 0;
}
static int prj_copy(uint32_t from, uint32_t to)
{
    int rc = prj_read(from);
    if (rc)
        return rc;
    return prj_write(to);                        /* (the head is made again for the slot it goes to) */
}
static int prj_delete(uint32_t k)
{
    uint32_t zero = 0;
    if (!prj_ok || k >= PRJ_SLOTS)
        return -1;
    if (prj_flash_prog(PRJ_LO + k * PRJ_SECT, &zero, 4u))   /* (the magic cleared: no erase, no pause) */
        return -2;
    prj_has[k] = 0;
    if (prj_cur == (int8_t)k)
        prj_cur = -1;                            /* (what plays stays, no longer saved anywhere) */
    return 0;
}

/* -------------------------------------------------------------- the pages --- */
enum { PR_LOAD, PR_SAVE, PR_COPY, PR_DELETE, PR_TEMPO, PR_ROOT, PR_SCALE, PR_N };
static const char *const PR_NAME[PR_N] = {"LOAD", "SAVE", "COPY", "DELETE", "TEMPO", "ROOT", "SCALE"};
enum { PQ_NONE, PQ_LOAD, PQ_SAVE, PQ_DELETE, PQ_COPYTO, PQ_COPYOVER };
static struct {
    uint8_t slot;                /* the slot chosen on the list, 0 .. 31 */
    uint8_t row;                 /* the row chosen on its page */
    uint8_t ask, yes;            /* a question up (PQ_*), and its answer */
    uint8_t to;                  /* COPY: the slot it goes to */
    int32_t shown;               /* (what was drawn, folded into one number) */
    uint32_t scrolled;           /* when the scale's row was last drawn scrolling */
} pj;
#define C_PRJ RGB(120, 255, 200)
#define C_DIRTY RGB(255, 170, 60)

static void prj_done(int rc, const char *what, uint32_t k)
{
    char n[12] = "PROJECT ";
    fmt_int(n + 8, (int32_t)k + 1);
    if (!rc)
        flash(what, n, C_OK);
    else
        flash(prj_ok ? "FAILED" : "NO FLASH", "", C_BAD);
    ui.force = 1;
}
/* SAVE pressed on a project page */
static void prj_press(int sh)
{
    if (sh) {                                    /* SHIFT + SAVE: back a page */
        if (ui.page == PG_PRJD) {
            if (pj.ask)
                pj.ask = PQ_NONE;
            else
                ui.page = PG_PRJ;
        } else
            ui.page = PG_MAIN;
        ui.force = 1;
        return;
    }
    if (ui.page == PG_PRJ) {                     /* a slot: its page */
        ui.page = PG_PRJD;
        pj.row = 0;
        pj.ask = PQ_NONE;
        ui.force = 1;
        return;
    }
    if (pj.ask == PQ_COPYTO) {                   /* the slot to copy to, chosen */
        if (pj.to == pj.slot)
            return;
        if (prj_has[pj.to]) {
            pj.ask = PQ_COPYOVER;
            pj.yes = 0;
        } else {
            pj.ask = PQ_NONE;
            flash("COPYING", "", C_PRJ);
            ui_draw();
            prj_done(prj_copy(pj.slot, pj.to), "COPIED", pj.to);
        }
        ui.force = 1;
        return;
    }
    if (pj.ask) {                                /* a question answered */
        uint8_t q = pj.ask;
        pj.ask = PQ_NONE;
        ui.force = 1;
        if (!pj.yes) {
            say("NO", 700);
            return;
        }
        if (q == PQ_LOAD) {
            flash("LOADING", "", C_PRJ);
            ui_draw();
            prj_done(prj_load(pj.slot), "LOADED", pj.slot);
            ui.page = PG_MAIN;
        } else if (q == PQ_SAVE) {
            flash("SAVING", "", C_PRJ);
            ui_draw();
            prj_done(prj_save(pj.slot), "SAVED", pj.slot);
        } else if (q == PQ_DELETE)
            prj_done(prj_delete(pj.slot), "DELETED", pj.slot);
        else if (q == PQ_COPYOVER) {
            flash("COPYING", "", C_PRJ);
            ui_draw();
            prj_done(prj_copy(pj.slot, pj.to), "COPIED", pj.to);
        }
        return;
    }
    switch (pj.row) {
    case PR_LOAD:
        if (!prj_has[pj.slot]) {
            say("EMPTY", 900);
            return;
        }
        if (prj_dirty) {                         /* (what plays has changes not saved: asked first) */
            pj.ask = PQ_LOAD;
            pj.yes = 0;
        } else {
            flash("LOADING", "", C_PRJ);
            ui_draw();
            prj_done(prj_load(pj.slot), "LOADED", pj.slot);
            ui.page = PG_MAIN;
        }
        break;
    case PR_SAVE:
        if (prj_has[pj.slot] && prj_cur != (int8_t)pj.slot) {   /* (over another project: asked first) */
            pj.ask = PQ_SAVE;
            pj.yes = 0;
        } else {
            flash("SAVING", "", C_PRJ);
            ui_draw();
            prj_done(prj_save(pj.slot), "SAVED", pj.slot);
        }
        break;
    case PR_COPY:
        if (!prj_has[pj.slot]) {
            say("EMPTY", 900);
            return;
        }
        pj.ask = PQ_COPYTO;
        pj.to = (uint8_t)((pj.slot + 1u) % PRJ_SLOTS);
        break;
    case PR_DELETE:
        if (!prj_has[pj.slot]) {
            say("EMPTY", 900);
            return;
        }
        pj.ask = PQ_DELETE;
        pj.yes = 0;
        break;
    default:
        say("TURN SELECT", 900);                 /* (the values: SELECT turns them) */
        break;
    }
    ui.force = 1;
}
/* PRESETS on a project page: through the slots, the rows, the slot to copy to, or NO / YES */
static void prj_preset(int32_t s)
{
    if (ui.page == PG_PRJ)
        pj.slot = (uint8_t)clampi((int32_t)pj.slot + s, 0, PRJ_SLOTS - 1);
    else if (pj.ask == PQ_COPYTO)
        pj.to = (uint8_t)clampi((int32_t)pj.to + s, 0, PRJ_SLOTS - 1);
    else if (pj.ask)
        pj.yes = (uint8_t)(s > 0);
    else
        pj.row = (uint8_t)clampi((int32_t)pj.row + s, 0, PR_N - 1);
}
/* SELECT on a project page: the value of the row chosen (the project playing's) */
static void prj_select(int32_t s)
{
    if (ui.page != PG_PRJD || pj.ask)
        return;
    if (pj.row == PR_TEMPO) {
        ui.tempo = (int16_t)clampi(ui.tempo + s, TEMPO_MIN, TEMPO_MAX);
        fx_bpm10 = ui.tempo * 10;
        fx_cook_all();                           /* (the delay's time follows the tempo) */
    } else if (pj.row == PR_ROOT)
        ui.root = (uint8_t)(((int32_t)ui.root + s % 12 + 12) % 12);
    else if (pj.row == PR_SCALE)
        ui.scale = (uint8_t)clampi((int32_t)ui.scale + s, 0, NSCALE - 1);
    else
        return;
    prj_dirty = 1;
}

/* the pages: a title; five rows, the one chosen in black on white; a question in place of the rows. A project's
 * state is its colour: loaded green (orange with changes not saved), saved white, empty grey */
static uint16_t prj_col(uint32_t k)
{
    return prj_cur == (int8_t)k ? (prj_dirty ? C_DIRTY : C_OK) : prj_has[k] ? C_WHITE : C_GREY;
}
static void prj_row(uint32_t r, const char *name, const char *val, uint16_t fg, uint16_t vfg, int chosen, uint16_t mark)
{
    static const font_t *const F[3] = {&FONT_M, &FONT_S, &FONT_XS};
    const font_t *fn = &FONT_M, *fv = &FONT_M;
    uint32_t i;
    cv_begin(240, 37, chosen ? C_WHITE : C_BLACK);
    for (i = 0; i < 6u; i++) {                   /* (the name smaller first, then the value) */
        fn = F[i < 3u ? i : 2u];
        fv = F[i < 3u ? 0u : i - 2u > 2u ? 2u : i - 2u];
        if (text_w(fn, name) + (val[0] ? text_w(fv, val) + 40 : 0) <= 230)   /* (a clear gap between them) */
            break;
    }
    cv_text(6, (37 - (int32_t)fn->h) / 2, fn, name, chosen ? C_BLACK : fg);
    if (val[0])
        cv_text(234 - text_w(fv, val), (37 - (int32_t)fv->h) / 2, fv, val, chosen ? C_BLACK : vfg);
    if (mark)                                    /* (the project loaded: a block at the right, its colour) */
        cv_rect(212, 7, 22, 22, mark);
    cv_rect(0, 36, 240, 1, RGB(50, 50, 50));
    cv_blit(0, 55 + 37 * r);
}
/* a project page's row: every name and value in the one size (S); the value in a field of its own at the right
 * (FIELD_X .. 232), scrolling round when it is longer than the field (a scale's name) */
#define FIELD_X 138                /* (past SCALE, TEMPO and ROOT, with a gap) */
#define FIELD_W (232 - FIELD_X)
static int prj_scrolls(const char *val) { return text_w(&FONT_S, val) > FIELD_W; }
static void prj_field(uint32_t r, const char *name, const char *val, uint16_t fg, int chosen)
{
    int32_t y = (37 - (int32_t)FONT_S.h) / 2, w = text_w(&FONT_S, val);
    uint16_t bg = chosen ? C_WHITE : C_BLACK, vf = chosen ? C_BLACK : C_PRJ;
    cv_begin(240, 37, bg);
    if (val[0] && w <= FIELD_W)
        cv_text(232 - w, y, &FONT_S, val, vf);
    else if (val[0]) {                           /* (round and round: the text, a gap, the text again, 40 px a second) */
        int32_t span = w + 40, x = FIELD_X - (int32_t)((fm1_ms / 25u) % (uint32_t)span);
        cv_text(x, y, &FONT_S, val, vf);
        cv_text(x + span, y, &FONT_S, val, vf);
        cv_rect(0, 0, FIELD_X, 37, bg);           /* (the field's left edge: what scrolled past it hidden) */
        cv_rect(234, 0, 6, 37, bg);
    }
    cv_text(8, y, &FONT_S, name, chosen ? C_BLACK : fg);
    cv_rect(0, 36, 240, 1, RGB(50, 50, 50));
    cv_blit(0, 55 + 37 * r);
}
static void prj_title(const char *t, uint16_t col)
{
    cv_begin(240, 55, C_BLACK);
    cv_text_fit((52 - (int32_t)FONT_M.h) / 2, &FONT_M, t, col);
    cv_rect(0, 52, 240, 3, C_PRJ);
    cv_blit(0, 0);
}
static void prj_slot_name(char *b, uint32_t k)
{
    str_cpy(b, "PROJECT ", 12);
    fmt_int(b + 8, (int32_t)k + 1);
}
static void draw_prj_list(void)
{
    uint32_t top = pj.slot < 2u ? 0u : pj.slot > PRJ_SLOTS - 3u ? PRJ_SLOTS - 5u : pj.slot - 2u, r;
    prj_title("PROJECTS", C_PRJ);
    for (r = 0; r < 5u; r++) {
        uint32_t k = top + r;
        char n[16];
        prj_slot_name(n, k);
        prj_row(r, n, "", prj_col(k), prj_col(k), k == pj.slot, prj_cur == (int8_t)k ? prj_col(k) : 0);
    }
}
static void draw_noyes_rows(const char *q1, const char *q2, uint16_t col)
{
    uint32_t r;
    for (r = 0; r < 5u; r++) {
        cv_begin(240, 37, C_BLACK);
        if (r == 0u)
            cv_text_fit((37 - (int32_t)FONT_M.h) / 2, &FONT_M, q1, col);
        else if (r == 1u && q2[0])
            cv_text_fit((37 - (int32_t)FONT_XS.h) / 2, &FONT_XS, q2, C_GREY);
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
static void draw_prj_page(void)
{
    char t[16], v[20];
    uint32_t r, top;
    prj_slot_name(t, pj.slot);
    prj_title(t, prj_col(pj.slot));
    if (pj.ask == PQ_COPYTO) {                   /* the slot to copy to, very large */
        char n[8];
        fmt_int(n, pj.to + 1);
        CV_AREA(0, 55, 240, 185, C_BLACK) {
            cv_text_c(4, &FONT_S, "COPY TO", C_PRJ);
            cv_text_c(36, &FONT_XL, n, pj.to == pj.slot ? C_GREY : prj_has[pj.to] ? C_BAD : C_WHITE);
            cv_text_fit(150, &FONT_XS, pj.to == pj.slot ? "THIS ONE" : prj_has[pj.to] ? "HOLDS A PROJECT" : "EMPTY", C_GREY);
        }
        return;
    }
    if (pj.ask) {
        draw_noyes_rows(pj.ask == PQ_LOAD ? "LOAD?" : pj.ask == PQ_SAVE ? "OVERWRITE?" : pj.ask == PQ_DELETE ? "DELETE?" : "OVERWRITE?",
                        pj.ask == PQ_LOAD ? "CHANGES NOT SAVED" : pj.ask == PQ_DELETE ? "IT CANNOT BE UNDONE" : "", pj.ask == PQ_LOAD ? C_DIRTY : C_BAD);
        return;
    }
    top = pj.row < 2u ? 0u : pj.row > PR_N - 3u ? PR_N - 5u : pj.row - 2u;
    for (r = 0; r < 5u; r++) {
        uint32_t k = top + r;
        v[0] = 0;
        if (k == PR_TEMPO) {
            fmt_int(v, ui.tempo);
        } else if (k == PR_ROOT)
            str_cpy(v, NOTE_NAME[ui.root], sizeof v);
        else if (k == PR_SCALE) {
            str_cpy(v, SCALE_L1[ui.scale], sizeof v);
            if (SCALE_L2[ui.scale][0]) {
                str_cpy(v + str_len(v), " ", 2);
                str_cpy(v + str_len(v), SCALE_L2[ui.scale], sizeof v - str_len(v));
            }
        }
        prj_field(r, PR_NAME[k], v, k < PR_TEMPO && !prj_has[pj.slot] && k != PR_SAVE ? C_GREY : C_WHITE, k == pj.row);
    }
}
/* the page, drawn again when what it shows has changed */
static void prj_draw(void)
{
    int32_t key = ui.page * 1000000 + pj.slot * 10000 + pj.row * 1000 + pj.ask * 100 + pj.yes * 50 + pj.to +
                  ui.tempo * 7 + ui.root * 131 + ui.scale * 977 + prj_dirty * 3 + prj_cur * 13;
    if (!ui.force && ui.shown_page == ui.page && key == pj.shown) {
        uint32_t top = pj.row < 2u ? 0u : pj.row > PR_N - 3u ? PR_N - 5u : pj.row - 2u;
        char v[20];
        if (ui.page != PG_PRJD || pj.ask || PR_SCALE < top || PR_SCALE >= top + 5u)
            return;
        str_cpy(v, SCALE_L1[ui.scale], sizeof v);   /* (the scale's name scrolling: its row again, ~25 a second) */
        if (SCALE_L2[ui.scale][0]) {
            str_cpy(v + str_len(v), " ", 2);
            str_cpy(v + str_len(v), SCALE_L2[ui.scale], sizeof v - str_len(v));
        }
        if (prj_scrolls(v) && (uint32_t)(fm1_ms - pj.scrolled) >= 40u) {
            prj_field(PR_SCALE - top, PR_NAME[PR_SCALE], v, C_WHITE, pj.row == PR_SCALE);
            pj.scrolled = fm1_ms;
        }
        return;
    }
    if (ui.page == PG_PRJ)
        draw_prj_list();
    else
        draw_prj_page();
    pj.shown = key;
}
