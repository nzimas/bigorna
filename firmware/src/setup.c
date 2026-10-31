/* SPDX-License-Identifier: GPL-3.0-only
 * GLOBAL SETUP (build 46): the settings of the machine, not of a session, kept in the flash (SET_SECT). SHIFT + SAVE
 * held opens it (outside the sessions' pages); a track's button leaves it. Its sections -- MIDI, for now -- then each
 * one's parts, then their rows, as the sessions' pages go: PRESETS through them, SAVE into one; on a row SAVE moves
 * the focus to its value, PRESETS changes it, SAVE sets it; SHIFT + SAVE leaves a value as it was, or goes back a
 * level (from the top, out). Each level below the top begins with BACK (build 48): SAVE on it goes back a level; a
 * level opens on the entry below it. Left, what changed is written to the flash.
 *
 *   MIDI   SYNC       CLK IN, CLK OUT (the clock and the transport), PCH IN, PCH OUT (program changes)   ON / OFF
 *          IN CHAN    TRACK 1 .. 6, AUTO (the chosen track), PCH                               OFF, 1 .. 16
 *          OUT CHAN   TRACK 1 .. 6, PCH                                                        OFF, 1 .. 16
 *          RECEIVE    NOTES, CC                                                                ON / OFF
 *          SEND       NOTES, CC                                                                ON / OFF
 *          PORTS      USB IN, TRS IN (the 3.5 mm MIDI IN jack; build 47)                       ON / OFF
 * (the Model:Cycles' names, short enough for the pages' type; midi.c does what they say)
 *   START  NEW (a new session, its sounds, scale and root drawn at random, as Tonitor starts) / LAST (the last session
 *          loaded or saved); a value at the top level, set as a row is (build 77) */

#define SET_SECT 0x000FD000u                     /* (its record: the MIDI settings, then MIDI LEARN's bindings -- from
                                                  * build 48; builds 46 and 47 wrote the settings alone) */
#define SET_LEN48 (sizeof(midi_cfg_t) + sizeof mlearn)
#define SET_LEN (SET_LEN48 + 1u)                 /* (then START: build 77; before, a record ends at SET_LEN48) */
#define SET_MAGIC 0x31434742u   /* "BGC1" */
enum { SK_ONOFF, SK_CHAN, SK_CHAN1 };            /* (a value: ON / OFF; OFF, 1 .. 16; 1 .. 16) */
typedef struct { const char *name; uint8_t off, kind; } set_row_t;
#define SR(n, f, k) {n, (uint8_t)__builtin_offsetof(midi_cfg_t, f), k}
static const set_row_t SET_SYNC[] = {SR("CLK IN", clk_in, SK_ONOFF), SR("CLK OUT", clk_out, SK_ONOFF),
                                     SR("PCH IN", pc_in, SK_ONOFF), SR("PCH OUT", pc_out, SK_ONOFF)};
static const set_row_t SET_IN[] = {SR("TRACK 1", in_ch[0], SK_CHAN), SR("TRACK 2", in_ch[1], SK_CHAN),
                                   SR("TRACK 3", in_ch[2], SK_CHAN), SR("TRACK 4", in_ch[3], SK_CHAN),
                                   SR("TRACK 5", in_ch[4], SK_CHAN), SR("TRACK 6", in_ch[5], SK_CHAN),
                                   SR("AUTO", auto_ch, SK_CHAN), SR("PCH", pc_in_ch, SK_CHAN1)};
static const set_row_t SET_OUT[] = {SR("TRACK 1", out_ch[0], SK_CHAN), SR("TRACK 2", out_ch[1], SK_CHAN),
                                    SR("TRACK 3", out_ch[2], SK_CHAN), SR("TRACK 4", out_ch[3], SK_CHAN),
                                    SR("TRACK 5", out_ch[4], SK_CHAN), SR("TRACK 6", out_ch[5], SK_CHAN),
                                    SR("PCH", pc_out_ch, SK_CHAN1)};
static const set_row_t SET_RCV[] = {SR("NOTES", rcv_note, SK_ONOFF), SR("CC", rcv_cc, SK_ONOFF)};
static const set_row_t SET_SND[] = {SR("NOTES", send_note, SK_ONOFF), SR("CC", send_cc, SK_ONOFF)};
static const set_row_t SET_PORT[] = {SR("USB IN", usb_in, SK_ONOFF), SR("TRS IN", trs_in, SK_ONOFF)};
#define SET_NPART 6u
static const struct { const char *name; const set_row_t *rows; uint8_t n; } SET_PART[SET_NPART] = {
    {"SYNC", SET_SYNC, NELEM(SET_SYNC)}, {"IN CHAN", SET_IN, NELEM(SET_IN)}, {"OUT CHAN", SET_OUT, NELEM(SET_OUT)},
    {"RECEIVE", SET_RCV, NELEM(SET_RCV)}, {"SEND", SET_SND, NELEM(SET_SND)}, {"PORTS", SET_PORT, NELEM(SET_PORT)}};
static const char *const SET_SECTION[2] = {"MIDI", "START"};   /* (START: a value, not a section) */
#define SET_START 1u
static uint8_t set_boot;                         /* START: 0 NEW (the default), 1 LAST */
#define C_SET RGB(150, 200, 255)

static struct {
    uint8_t lvl;                 /* 0 the sections, 1 a section's parts, 2 a part's rows */
    uint8_t sec, part, row;      /* (part: the part entered; on its list pc, 0 BACK; row: 0 BACK, then the rows) */
    uint8_t pc;
    uint8_t edit;                /* the focus on the row's value .. */
    int16_t val;                 /* .. as it is being set */
    uint8_t dirty;               /* changed since read from the flash */
    uint8_t was;                 /* (the page was up) */
    int32_t shown;
} set;

/* ------------------------------------------------------------ flash --- */
static void setup_write(void)
{
    if (!prj_ok)
        return;
    memset(prj_buf, 0, sizeof(prj_head_t) + SET_LEN);
    memcpy(prj_buf + sizeof(prj_head_t), &mcfg, sizeof mcfg);
    memcpy(prj_buf + sizeof(prj_head_t) + sizeof mcfg, mlearn, sizeof mlearn);
    prj_buf[sizeof(prj_head_t) + SET_LEN48] = set_boot;
    prj_head_fill(SET_MAGIC, 0, SET_LEN);
    if (!prj_flash_erase(SET_SECT) && !prj_flash_prog(SET_SECT, prj_buf, sizeof(prj_head_t) + SET_LEN))
        set.dirty = 0;
}
static void setup_load(void)                     /* (the start: the settings as last left, or the defaults) */
{
    static const midi_cfg_t D = MIDI_CFG0;
    midi_cfg_t c = D;
    const prj_head_t *h = (const prj_head_t *)(void *)prj_buf;
    uint32_t i, len;
    mcfg = D;
    memset(mlearn, 0, sizeof mlearn);
    set_boot = 0;
    if (!prj_ok || prj_flash_read(SET_SECT, prj_buf, sizeof(prj_head_t) + SET_LEN))
        return;
    len = prj_head_ok(h, SET_MAGIC, 0, SET_LEN) ? SET_LEN
          : prj_head_ok(h, SET_MAGIC, 0, SET_LEN48) ? SET_LEN48   /* (builds 48 .. 76's: no START -- NEW) */
          : prj_head_ok(h, SET_MAGIC, 0, sizeof(midi_cfg_t)) ? sizeof(midi_cfg_t)   /* (build 47's: no bindings) */
          : prj_head_ok(h, SET_MAGIC, 0, MIDI_CFG46) ? MIDI_CFG46 : 0u;   /* (build 46's: no PORTS -- both on) */
    if (!len || h->body_crc != prj_crc(prj_buf + sizeof(prj_head_t), len))
        return;
    memcpy(&c, prj_buf + sizeof(prj_head_t), len < sizeof c ? len : sizeof c);
    if (len == SET_LEN)
        set_boot = (uint8_t)(prj_buf[sizeof(prj_head_t) + SET_LEN48] == 1u);
    if (len >= SET_LEN48) {                      /* (the bindings: each whole, or none) */
        memcpy(mlearn, prj_buf + sizeof(prj_head_t) + sizeof c, sizeof mlearn);
        for (i = 0; i < NLEARN; i++) {
            learn_t *l = &mlearn[i];
            if (l->kind > LK_FX || !l->ch || l->ch > 16u || l->cc > 127u || l->track >= NTRACK ||
                l->id >= (l->kind == LK_FX ? NFXP * 8u : MIDI_NPAR))
                l->kind = LK_NONE;
        }
    }
    for (i = 0; i < SET_NPART; i++) {            /* (each value in its range, or the default) */
        uint32_t r;
        for (r = 0; r < SET_PART[i].n; r++) {
            uint8_t *v = (uint8_t *)&c + SET_PART[i].rows[r].off;
            uint32_t k = SET_PART[i].rows[r].kind;
            if (k == SK_ONOFF ? *v > 1u : k == SK_CHAN ? *v > 16u : (*v < 1u || *v > 16u))
                *v = *((const uint8_t *)&D + SET_PART[i].rows[r].off);
        }
    }
    mcfg = c;
}

/* ------------------------------------------------------------ pages --- */
static uint8_t *set_ptr(const set_row_t *r) { return (uint8_t *)&mcfg + r->off; }
static int32_t set_lo(const set_row_t *r) { return r->kind == SK_CHAN1 ? 1 : 0; }
static int32_t set_hi(const set_row_t *r) { return r->kind == SK_ONOFF ? 1 : 16; }
static void set_text(const set_row_t *r, int32_t v, char *b)
{
    if (r->kind == SK_ONOFF)
        str_cpy(b, v ? "ON" : "OFF", 4);
    else if (!v)
        str_cpy(b, "OFF", 4);
    else
        fmt_int(b, v);
}
static void setup_open(void)
{
    ui.page = PG_SET;
    set.lvl = 0;
    set.sec = 0;                                 /* (it opens on MIDI, the first entry) */
    set.edit = 0;
    ui.force = 1;
}
static void setup_leave(void)
{
    ui.page = PG_MAIN;
    ui.force = 1;
}
/* SAVE (SHIFT: back) on the page */
static void setup_press(int sh)
{
    ui.force = 1;
    if (sh) {
        if (set.edit)
            set.edit = 0;                        /* (the value as it was) */
        else if (set.lvl)
            set.lvl--;
        else
            setup_leave();
        return;
    }
    if (set.lvl == 0u && set.sec == SET_START) { /* START: the focus on its value, or the value set */
        if (!set.edit) {
            set.edit = 1;
            set.val = set_boot;
        } else {
            set.edit = 0;
            if (set_boot != (uint8_t)set.val) {
                set_boot = (uint8_t)set.val;
                set.dirty = 1;
            }
        }
    } else if (set.lvl == 0u) {
        set.lvl = 1;
        set.pc = 1;
    } else if (set.lvl == 1u && !set.pc)         /* (BACK) */
        set.lvl = 0;
    else if (set.lvl == 1u) {
        set.lvl = 2;
        set.part = (uint8_t)(set.pc - 1u);
        set.row = 1;
        set.edit = 0;
    } else if (!set.edit && !set.row)            /* (BACK) */
        set.lvl = 1;
    else if (!set.edit) {
        set.edit = 1;
        set.val = *set_ptr(&SET_PART[set.part].rows[set.row - 1u]);
    } else {
        uint8_t *p = set_ptr(&SET_PART[set.part].rows[set.row - 1u]);
        set.edit = 0;
        if (*p != (uint8_t)set.val) {
            *p = (uint8_t)set.val;
            set.dirty = 1;
        }
    }
}
static void setup_preset(int32_t s)
{
    if (set.lvl == 0u && set.edit)
        set.val = (int16_t)(s > 0);
    else if (set.lvl == 0u)
        set.sec = (uint8_t)clampi((int32_t)set.sec + s, 0, (int32_t)NELEM(SET_SECTION) - 1);
    else if (set.lvl == 1u)
        set.pc = (uint8_t)clampi((int32_t)set.pc + s, 0, (int32_t)SET_NPART);
    else if (!set.edit)
        set.row = (uint8_t)clampi((int32_t)set.row + s, 0, (int32_t)SET_PART[set.part].n);
    else {
        const set_row_t *r = &SET_PART[set.part].rows[set.row - 1u];
        set.val = (int16_t)clampi(set.val + s, set_lo(r), set_hi(r));
    }
}
/* each pass: the page left (a track's button, SHIFT + SAVE), what changed written */
static void setup_poll(void)
{
    if (ui.page == PG_SET)
        set.was = 1;
    else if (set.was) {
        set.was = 0;
        set.edit = 0;
        if (set.dirty)
            setup_write();
    }
}
static void set_title(const char *t)
{
    cv_begin(240, 55, C_BLACK);
    cv_text_fit((50 - (int32_t)FONT_S.h) / 2, &FONT_S, t, C_SET);   /* (the smaller type: GLOBAL SETUP fits) */
    cv_rect(0, 52, 240, 3, C_SET);
    cv_blit(0, 0);
}
/* row r of five: a name, and a value at the right (the value being set in a white field) */
static void set_line(uint32_t r, const char *name, const char *val, int chosen, int focus)
{
    int32_t y = (37 - (int32_t)FONT_S.h) / 2;
    cv_begin(240, 37, chosen && !focus ? C_WHITE : C_BLACK);
    if (focus) {                                 /* (the field as wide as its value takes) */
        int32_t x = val ? clampi(224 - text_w(&FONT_S, val), 0, 160) : 160;
        cv_rect((uint32_t)x, 1, 240u - (uint32_t)x, 35, C_WHITE);
    }
    if (name)
        cv_text(8, y, &FONT_S, name, chosen && !focus ? C_BLACK : C_WHITE);
    if (val && val[0])
        cv_text(232 - text_w(&FONT_S, val), y, &FONT_S, val, chosen || focus ? C_BLACK : C_SET);
    cv_rect(0, 36, 240, 1, RGB(50, 50, 50));
    cv_blit(0, 55 + 37 * r);
}
static void setup_draw(void)
{
    uint32_t n, sel, top, r;
    int32_t key = set.lvl * 1000000 + set.sec * 100000 + set.part * 10000 + set.pc * 1000 + set.row * 100 +
                  set.edit * 50 + set.val;
    char t[20];
    if (!ui.force && ui.shown_page == PG_SET && key == set.shown)
        return;
    set.shown = key;
    if (set.lvl == 0u) {
        set_title("GLOBAL SETUP");
        n = NELEM(SET_SECTION);
        sel = set.sec;
    } else if (set.lvl == 1u) {
        set_title(SET_SECTION[set.sec]);
        n = SET_NPART + 1u;                      /* (BACK, then the parts) */
        sel = set.pc;
    } else {
        str_cpy(t, "MIDI ", sizeof t);
        str_cpy(t + 5, SET_PART[set.part].name, sizeof t - 5);
        set_title(t);
        n = SET_PART[set.part].n + 1u;           /* (BACK, then the rows) */
        sel = set.row;
    }
    top = n <= 5u || sel < 2u ? 0u : sel > n - 3u ? n - 5u : sel - 2u;
    for (r = 0; r < 5u; r++) {
        uint32_t i = top + r;
        char v[8];
        if (i >= n) {
            set_line(r, 0, 0, 0, 0);
            continue;
        }
        if (set.lvl == 0u && i == SET_START)
            set_line(r, SET_SECTION[i], (set.edit ? set.val : set_boot) ? "LAST" : "NEW", i == sel, set.edit);
        else if (set.lvl == 0u)
            set_line(r, SET_SECTION[i], 0, i == sel, 0);
        else if (!i)
            set_line(r, "BACK", 0, i == sel, 0);
        else if (set.lvl == 1u)
            set_line(r, SET_PART[i - 1u].name, 0, i == sel, 0);
        else {
            const set_row_t *w = &SET_PART[set.part].rows[i - 1u];
            set_text(w, set.edit && i == sel ? set.val : *set_ptr(w), v);
            set_line(r, w->name, v, i == sel, set.edit && i == sel);
        }
    }
}
