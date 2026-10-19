/* SPDX-License-Identifier: GPL-3.0-only
 * The panel and the screen (after TONITOR's ui.c: its panel map, its bands, its dial, its scale page).
 *
 *   FX SEL ENV LFO EDIT GLO   the top row: tracks 1 to 6 (lit: the chosen one; the others glow while they sound)
 *   PRESETS (knob)            the track's machine: FM TONE, FM DRUM, WAVETONE, SWARMER (each track keeps a sound of
 *                             each, so turning back finds it)
 *   HOME                      a new sound for the track, on its machine (gen.c); SHIFT + HOME: of the same family
 *   OCT- / OCT+               the track's octave, -3 .. +3; on the EDIT page, the page before / after
 *   the white keys            play the track in the scale chosen (the first key the root, each the next degree)
 *   the rightmost black key   SHIFT: keys held latch when it is pressed; SHIFT and a latched key let it go
 *   the black key before it   EDIT: the parameter pages of the track's sound -- SYN 1 .. 4, FLTR 1, FLTR 2, AMP,
 *                             FX -- KNOB 1 .. 4 the page's A .. D, with SHIFT held E .. H
 *   ARP                       the SCALE page: SELECT the root, ALGORITHM the scale
 *   SEQ                       the send effects' pages: REVERB, DELAY, CHORUS in turn, then back; KNOB 1 .. 4 the
 *                             first four of the page, with SHIFT held the next four
 *   SELECT (knob)             the track's level; ALGORITHM (knob) its pan
 *   KNOB 1 .. 4               the track's filter FREQ and RESO, its delay and reverb sends; with SHIFT, KNOB 3 the
 *                             chorus send and KNOB 4 the overdrive. While one turns (and a second after) the whole
 *                             screen is its dial
 *   USB MIDI notes            the chosen track, as the keys
 *   MASTER                    the level
 * The screen is for low vision: few words, very large, high contrast, each track its own colour. */

enum { B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC, B_OCTDN, B_OCTUP, NB };
enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE };
static const uint8_t PANEL_BTN[NB] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 1};
static const uint8_t PANEL_ENC[NE] = {0, 1, 6, 2, 3, 4, 5};
static const uint8_t TRACK_BTN[NTRACK] = {B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO};
static const uint8_t KNOB[4] = {EN_K1, EN_K2, EN_K3, EN_K4};
#define NKEY 27u
#define KEY_SHIFT 25u
#define KEY_EDIT 22u
#define KEY_NONE 0xFFu

static const uint16_t TRACK_COL[NTRACK] = {RGB(255, 214, 0), RGB(0, 214, 255), RGB(255, 92, 210),
                                           RGB(96, 255, 96), RGB(255, 140, 0), RGB(190, 160, 255)};
enum { PG_MAIN, PG_EDIT, PG_FXP, PG_PRJ, PG_PRJD };   /* (the projects' list and a project's page: projects.c) */

static struct {
    uint8_t sel;
    uint8_t page;
    uint8_t edit_pg;             /* the EDIT page shown (0 .. pages_n - 1) */
    uint8_t fx_pg;               /* the effect shown: 0 REVERB, 1 DELAY, 2 CHORUS */
    uint8_t half;                /* the half of the page shown: 0 A .. D, 1 E .. H (SHIFT held) */
    int8_t oct[NTRACK];
    uint8_t eng[NTRACK];
    sound_t snd[NTRACK][NENG];
    uint8_t dirty[NTRACK];       /* the sound changed: to be published */
    uint8_t key_track[NKEY], key_note[NKEY];
    uint32_t latch[NTRACK];
    uint8_t latch_note[NTRACK][NKEY];
    uint8_t midi_track[128];
    uint32_t keys;
    char msg[16];
    uint32_t msg_until, home_until;
    uint32_t seed;
    uint8_t scale, root;
    int16_t tempo;               /* the project's, BPM */
    int8_t level[NTRACK], pan[NTRACK];
    int8_t touched;              /* the row just turned (-1 none), until touch_until */
    uint32_t touch_until;
    uint8_t dial_pg, dial_j;     /* the dial: the parameter it shows (page id, index) */
    uint32_t dial_until, dial_drawn_at;
    int32_t shown_dial;
    uint32_t flash_t0;
    uint8_t flash_on, flash_phase;
    uint16_t flash_col;
    char flash_l1[12], flash_l2[12];
    /* the screen as last drawn */
    int32_t shown_sel, shown_eng, shown_msg, shown_meter, shown_level, shown_pan;
    char shown_text[16];
    int32_t shown_rows[4];
    int8_t shown_touch;
    uint8_t shown_page, shown_half, shown_sub;
    int16_t shown_prj;           /* (the project shown at the top left, and its changes) */
    uint32_t meter_at;
    uint8_t force;
} ui;

static const char *const RVN[NRV] = {"PRE", "DECAY", "FREQ", "GAIN", "HPF", "LPF", "VOLUME"};
static const char *const DLN[NDL] = {"TIME", "PING PONG", "WIDTH", "FEEDBACK", "HPF", "LPF", "REVERB", "VOLUME"};
static const char *const CHN[NCH] = {"DEPTH", "SPEED", "HPF", "WIDTH", "DELAY", "REVERB", "VOLUME"};
static const char *const FXT[3] = {"REVERB", "DELAY", "CHORUS"};
#define C_SEND RGB(140, 190, 255)
#define C_OK RGB(60, 220, 90)
#define C_BAD RGB(255, 40, 40)
/* projects.c (after this file) */
static uint8_t prj_dirty;
static int8_t prj_cur;
static void prj_press(int sh);
static void prj_preset(int32_t s);
static void prj_draw(void);

static void say(const char *s, uint32_t ms)
{
    str_cpy(ui.msg, s, sizeof ui.msg);
    ui.msg_until = fm1_ms + ms;
}
static void flash(const char *l1, const char *l2, uint16_t c)
{
    str_cpy(ui.flash_l1, l1, sizeof ui.flash_l1);
    str_cpy(ui.flash_l2, l2, sizeof ui.flash_l2);
    ui.flash_col = c;
    ui.flash_t0 = fm1_ms;
    ui.flash_on = 1;
    ui.flash_phase = 0xFF;
}
static sound_t *cur(void) { return &ui.snd[ui.sel][ui.eng[ui.sel]]; }

/* ------------------------------------------------------------ notes --- */
#define LEVEL_MAX 23
#define LEVEL_UNITY 20
#define PAN_MAX 20
static const int8_t PAN_DEF[NTRACK] = {0, 6, -6, 10, -10, 3};
#define NSCALE 10
static const char *const SCALE_L1[NSCALE] = {"CHROMATIC", "MAJOR", "MINOR", "MINOR", "DORIAN", "PHRYGIAN", "MINOR",
                                             "MAJOR", "WHOLE", "JAPANESE"};
static const char *const SCALE_L2[NSCALE] = {"", "", "NATURAL", "HARMONIC", "", "", "PENTA", "PENTA", "TONE", "IN"};
static const uint8_t SCALE_N[NSCALE] = {12, 7, 7, 7, 7, 7, 5, 5, 6, 5};
static const uint8_t SCALE_STEP[NSCALE][12] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
    {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 8, 11}, {0, 2, 3, 5, 7, 9, 10},
    {0, 1, 3, 5, 7, 8, 10}, {0, 3, 5, 7, 10}, {0, 2, 4, 7, 9}, {0, 2, 4, 6, 8, 10}, {0, 1, 5, 7, 8},
};
static const char *const NOTE_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
#define SCALE_BASE 48
static int32_t white_ix(uint32_t k)
{
    static const int8_t W[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
    uint32_t n = 53u + k, o = n / 12u;
    int32_t w = W[n % 12u];
    if (w < 0)
        return -1;
    return (int32_t)o * 7 + w - (4 * 7 + 3);
}
/* the note key k plays: in CHROMATIC, the key's own (moved by the root); else white key w the scale's degree */
static uint32_t key_note(uint32_t k, int32_t oct)
{
    int32_t w, n, q;
    if (ui.scale == 0)
        return (uint32_t)clampi(53 + (int32_t)k + ui.root - 5 + 12 * oct, 12, 120);
    w = white_ix(k);
    n = SCALE_N[ui.scale];
    q = w >= 0 ? w / n : -((n - 1 - w) / n);
    return (uint32_t)clampi(SCALE_BASE + ui.root + 12 * oct + 12 * q + SCALE_STEP[ui.scale][w - q * n], 12, 120);
}
static int key_plays(uint32_t k) { return k != KEY_SHIFT && k != KEY_EDIT && (ui.scale == 0 || white_ix(k) >= 0); }

static void level_pan_apply(uint32_t t)
{
    int32_t lv = ui.level[t], ph;
    trk_gain[t] = lv <= 0 ? 0 : (int32_t)(exp2_q16((lv - LEVEL_UNITY) * 16328) >> 2);
    ph = (int32_t)((uint32_t)(ui.pan[t] + PAN_MAX) * (0x40000000u / (2u * PAN_MAX)));
    trk_pl[t] = sin_ph((uint32_t)ph + 0x40000000u);
    trk_pr[t] = sin_ph((uint32_t)ph);
}
static uint32_t next_seed(void)
{
    ui.seed = ui.seed * 1664525u + 1013904223u + fm1_ticks();
    return ui.seed;
}
static void mark(uint32_t t)
{
    ui.dirty[t] = 1;
    prj_dirty = 1;                               /* (the project playing has changed) */
}
/* the main loop: each changed track's sound cooked and published (the ISR takes it at its next block) */
static void ui_publish(void)
{
    uint32_t t;
    for (t = 0; t < NTRACK; t++)
        if (ui.dirty[t] && track_publish(t, &ui.snd[t][ui.eng[t]]))
            ui.dirty[t] = 0;
}

/* HOME: a new sound for the chosen track (same: of the family it plays) */
static void new_sound(int same)
{
    sound_t *s = cur();
    uint32_t m = ui.eng[ui.sel];
    int32_t fam = s->family;
    gen_sound(s, m, same ? fam : -1, same ? -1 : fam, next_seed());
    mark(ui.sel);
    say(same ? "SAME FAMILY" : "NEW", 700);
    ui.home_until = fm1_ms + 400u;
}

/* ------------------------------------------------------------ input --- */
static int32_t enc_take(uint32_t role) { return fm1_enc_take(PANEL_ENC[role]); }
static int btn_bit(uint32_t mask, uint32_t label) { return (int)((mask >> PANEL_BTN[label]) & 1u); }

static void par_turn(uint32_t id, uint32_t j, int32_t s)   /* a parameter of the chosen track's sound turned */
{
    sound_t *snd = cur();
    par_t p;
    int32_t v;
    par_get(snd, id, j, &p);
    if (p.kind == PK_NONE)
        return;
    v = clampi(par_val(snd, id, j) + s, p.lo, p.hi);
    par_put(snd, id, j, v);
    if (id == PG_FLT2 && j == 6u)                    /* (another FLTR machine: its knob G within its range) */
        snd->flt[F_TYPE] = (uint8_t)(snd->fmach == FLT_LEG ? snd->flt[F_TYPE] != 0 : snd->flt[F_TYPE]);
    mark(ui.sel);
}
static uint8_t *fx_vals(void) { return ui.fx_pg == 0 ? fx_rv : ui.fx_pg == 1 ? fx_dl : fx_ch; }
static uint32_t fx_n(void) { return ui.fx_pg == 0 ? NRV : ui.fx_pg == 1 ? NDL : NCH; }
static void fx_turn(uint32_t j, int32_t s)
{
    uint8_t *v = fx_vals();
    int32_t hi = ui.fx_pg == 1 && j == DL_X ? 1 : 127;
    if (j >= fx_n())
        return;
    v[j] = (uint8_t)clampi(v[j] + s, 0, hi);
    fx_cook_all();
    prj_dirty = 1;
}
static void dial(uint32_t id, uint32_t j)
{
    ui.dial_pg = (uint8_t)id;
    ui.dial_j = (uint8_t)j;
    ui.dial_until = fm1_ms + 1000u;
}

static void midi_take(void)
{
    while (mi_r != mi_w) {
        uint32_t pkt = midi_in_q[mi_r % MQ], cin = pkt & 15u, note = (pkt >> 16) & 0x7Fu, vel = pkt >> 24 & 0x7Fu;
        RING_PUBLISH();
        mi_r++;
        if (cin == 9u && vel) {
            if (ui.midi_track[note] != 0xFFu)
                ev_post(EV_OFF, ui.midi_track[note], note, 0);
            ui.midi_track[note] = ui.sel;
            ev_post(EV_ON, ui.sel, note, vel);
        } else if ((cin == 8u || cin == 9u) && ui.midi_track[note] != 0xFFu) {
            ev_post(EV_OFF, ui.midi_track[note], note, 0);
            ui.midi_track[note] = 0xFFu;
        }
    }
}

static void ui_input(void)
{
    uint32_t rel, press = fm1_input_edges(&rel), keys, ch, k, t, j;
    int32_t s;
    int sh = (int)((fm1_in.notes >> KEY_SHIFT) & 1u);
    (void)rel;
    for (t = 0; t < NTRACK; t++)
        if (btn_bit(press, TRACK_BTN[t])) {
            ui.sel = (uint8_t)t;
            if (ui.page == PG_FXP || ui.page == PG_PRJ || ui.page == PG_PRJD)
                ui.page = PG_MAIN;
            if (ui.page == PG_EDIT && ui.edit_pg >= pages_n(cur()))
                ui.edit_pg = 0;
            ui.force = 1;
        }
    if (btn_bit(press, B_SEQ)) {                     /* REVERB, DELAY, CHORUS, back */
        if (ui.page != PG_FXP) {
            ui.page = PG_FXP;
            ui.fx_pg = 0;
        } else if (++ui.fx_pg >= 3u)
            ui.page = PG_MAIN;
        ui.force = 1;
    }
    if (btn_bit(press, B_SAVE)) {                    /* the projects (projects.c) */
        if (ui.page == PG_PRJ || ui.page == PG_PRJD)
            prj_press(sh);
        else if (!sh) {
            ui.page = PG_PRJ;
            ui.force = 1;
        }
    }
    if (btn_bit(press, B_HOME))
        new_sound(sh);
    ui.half = (uint8_t)sh;
    if (ui.page == PG_EDIT) {                        /* the page's parameters */
        uint32_t id = page_id(cur(), ui.edit_pg);
        for (j = 0; j < 4u; j++)
            if ((s = enc_take(KNOB[j])) != 0) {
                par_turn(id, j + 4u * ui.half, s);
                ui.touched = (int8_t)j;
                ui.touch_until = fm1_ms + 1500u;
            }
    } else if (ui.page == PG_FXP) {
        for (j = 0; j < 4u; j++)
            if ((s = enc_take(KNOB[j])) != 0) {
                fx_turn(j + 4u * ui.half, s);
                ui.touched = (int8_t)j;
                ui.touch_until = fm1_ms + 1500u;
            }
    } else {                                         /* the knobs' own: FREQ, RESO, the sends (SHIFT: chorus, drive) */
        static const uint8_t ID[2][4] = {{PG_FLT1, PG_FLT1, PG_FX, PG_FX}, {PG_FLT1, PG_FLT1, PG_FX, PG_FX}};
        static const uint8_t J[2][4] = {{4, 5, 4, 5}, {4, 5, 6, 1}};
        for (j = 0; j < 4u; j++)
            if ((s = enc_take(KNOB[j])) != 0) {
                par_turn(ID[sh][j], J[sh][j], s);
                dial(ID[sh][j], J[sh][j]);
            }
    }
    if (ui.page == PG_PRJ || ui.page == PG_PRJD) {   /* PRESETS is the projects'; SELECT does nothing there */
        if ((s = enc_take(EN_PRESET)) != 0)
            prj_preset(s);
        (void)enc_take(EN_SELECT);
    }
    if ((s = enc_take(EN_SELECT)) != 0) {            /* the level */
        char m[12] = "VOL ";
        int32_t lv = clampi(ui.level[ui.sel] + s, 0, LEVEL_MAX);
        ui.level[ui.sel] = (int8_t)lv;
        level_pan_apply(ui.sel);
        prj_dirty = 1;
        if (!lv)
            str_cpy(m + 4, "OFF", 8);
        else {
            fmt_int(m + 4, (lv * 100 + LEVEL_MAX / 2) / LEVEL_MAX);
            str_cpy(m + str_len(m), "%", 2);
        }
        say(m, 1200);
    }
    if ((s = enc_take(EN_ALGO)) != 0) {              /* the pan */
        char m[12] = "PAN ";
        int32_t pn = clampi(ui.pan[ui.sel] + s, -PAN_MAX, PAN_MAX);
        ui.pan[ui.sel] = (int8_t)pn;
        level_pan_apply(ui.sel);
        prj_dirty = 1;
        if (!pn)
            str_cpy(m + 4, "C", 8);
        else {
            m[4] = pn < 0 ? 'L' : 'R';
            m[5] = ' ';
            fmt_int(m + 6, (pn < 0 ? -pn : pn) * 100 / PAN_MAX);
        }
        say(m, 1200);
    }
    if (btn_bit(press, B_OCTDN) || btn_bit(press, B_OCTUP)) {
        int d = btn_bit(press, B_OCTUP) ? 1 : -1;
        if (ui.page == PG_EDIT) {                    /* the page before / after */
            uint32_t n = pages_n(cur());
            ui.edit_pg = (uint8_t)((ui.edit_pg + n + (uint32_t)(d + (int)n)) % n);
            ui.force = 1;
        } else {
            char m[8] = "OCT  ";
            int32_t o = clampi(ui.oct[ui.sel] + d, -3, 3);
            ui.oct[ui.sel] = (int8_t)o;
            prj_dirty = 1;
            m[4] = o < 0 ? '-' : o > 0 ? '+' : ' ';
            m[5] = (char)('0' + (o < 0 ? -o : o));
            m[6] = 0;
            say(m, 1200);
        }
    }
    if ((s = enc_take(EN_PRESET)) != 0) {            /* the machine */
        int32_t e = clampi((int32_t)ui.eng[ui.sel] + (s > 0 ? 1 : -1), 0, NENG - 1);
        if (e != ui.eng[ui.sel]) {
            ui.eng[ui.sel] = (uint8_t)e;
            mark(ui.sel);
            if (ui.page == PG_EDIT && ui.edit_pg >= pages_n(cur()))
                ui.edit_pg = 0;
            ui.force = 1;
        }
    }
    keys = fm1_in.notes;
    ch = keys ^ ui.keys;
    if ((ch & keys) >> KEY_EDIT & 1u) {              /* EDIT: the parameter pages, up or down */
        ui.page = ui.page == PG_EDIT ? PG_MAIN : PG_EDIT;
        if (ui.edit_pg >= pages_n(cur()))
            ui.edit_pg = 0;
        ui.force = 1;
    }
    if ((ch & keys) >> KEY_SHIFT & 1u)               /* SHIFT pressed: the keys held latch */
        for (k = 0; k < NKEY; k++)
            if (((ui.keys & keys) >> k & 1u) && key_plays(k) && ui.key_track[k] != KEY_NONE) {
                t = ui.key_track[k];
                ui.latch[t] |= 1u << k;
                ui.latch_note[t][k] = ui.key_note[k];
                ui.key_track[k] = KEY_NONE;
            }
    for (k = 0; ch && k < NKEY; k++) {
        if (!((ch >> k) & 1u) || !key_plays(k))
            continue;
        if ((keys >> k) & 1u) {
            uint32_t note = key_note(k, ui.oct[ui.sel]), bit = 1u << k;
            ui.key_track[k] = KEY_NONE;
            if (ui.latch[ui.sel] & bit) {            /* a latched key: SHIFT and it let it go */
                if (sh) {
                    ui.latch[ui.sel] &= ~bit;
                    ev_post(EV_OFF, ui.sel, ui.latch_note[ui.sel][k], 0);
                }
            } else {
                ui.key_track[k] = ui.sel;
                ui.key_note[k] = (uint8_t)note;
                ev_post(EV_ON, ui.sel, note, 127);
            }
        } else if (ui.key_track[k] != KEY_NONE) {
            ev_post(EV_OFF, ui.key_track[k], ui.key_note[k], 0);
            ui.key_track[k] = KEY_NONE;
        }
    }
    ui.keys = keys;
    midi_take();
}

/* ------------------------------------------------------------- LEDs --- */
static void led_dim_key(uint32_t id, int on)
{
    uint32_t p, r;
    for (p = 0; p < FM1_NCOL; p++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][p] == (int8_t)id) {
                if (on)
                    fm1_led_dim[p] |= (uint8_t)(1u << r);
                else
                    fm1_led_dim[p] &= (uint8_t)~(1u << r);
            }
}
static void ui_leds(void)
{
    uint32_t t, k;
    for (t = 0; t < NTRACK; t++) {
        fm1_led_key(PANEL_BTN[TRACK_BTN[t]], t == ui.sel);
        led_dim_key(PANEL_BTN[TRACK_BTN[t]], track_sounding(t));
    }
    fm1_led_key(PANEL_BTN[B_HOME], (int32_t)(ui.home_until - fm1_ms) > 0);
    fm1_led_key(PANEL_BTN[B_SEQ], ui.page == PG_FXP);
    fm1_led_key(PANEL_BTN[B_ARP], 0);
    fm1_led_key(PANEL_BTN[B_SAVE], ui.page == PG_PRJ || ui.page == PG_PRJD);
    fm1_led_key(PANEL_BTN[B_OCTDN], ui.page == PG_EDIT || ui.oct[ui.sel] < 0);
    fm1_led_key(PANEL_BTN[B_OCTUP], ui.page == PG_EDIT || ui.oct[ui.sel] > 0);
    fm1_led_key(PANEL_BTN[B_REC], 0);
    fm1_led_key(PANEL_BTN[B_PLAY], 0);
    for (k = 0; k < NKEY; k++)
        fm1_led_key(14u + k, key_plays(k) && ((((ui.keys >> k) & 1u) && ui.key_track[k] == ui.sel) ||
                                             ((ui.latch[ui.sel] >> k) & 1u)));
    fm1_led_key(14u + KEY_SHIFT, (ui.keys >> KEY_SHIFT) & 1u);
    fm1_led_key(14u + KEY_EDIT, ui.page == PG_EDIT);
    led_dim_key(14u + KEY_EDIT, 1);
}

/* ----------------------------------------------------------- screen --- */
#define BAND_A 0
#define BAND_B 98
#define BAND_C 146
#define BAND_M 192
#define BAND_D 216

static void draw_number(void)
{
    char d[2] = {(char)('1' + ui.sel), 0};
    CV_AREA(0, BAND_A, 240, BAND_B - BAND_A, C_BLACK) {
        cv_text_c(3, &FONT_XL, d, TRACK_COL[ui.sel]);
        if (prj_cur >= 0) {                          /* the project loaded, top left: P7 -- orange with changes not
                                                      * saved */
            char pn[8] = "P";
            fmt_int(pn + 1, prj_cur + 1);
            cv_text(4, 2, &FONT_S, pn, prj_dirty ? RGB(255, 170, 60) : C_GREY);
        }
        if (ui.oct[ui.sel]) {                         /* the octave, top right: +1, -2 */
            char o[4] = {ui.oct[ui.sel] < 0 ? '-' : '+', (char)('0' + (ui.oct[ui.sel] < 0 ? -ui.oct[ui.sel] : ui.oct[ui.sel])), 0, 0};
            cv_text(236 - text_w(&FONT_S, o), 2, &FONT_S, o, C_WHITE);
        }
    }
}
static void draw_engine(void)
{
    const char *e = ENG_NAME[ui.eng[ui.sel]];
    cv_begin(240, 48, C_BLACK);
    if (text_w(&FONT_L, e) <= (int32_t)cv_w - 4)
        cv_text_c(2, &FONT_L, e, C_WHITE);
    else
        cv_text_fit((48 - FONT_M.h) / 2, &FONT_M, e, C_WHITE);
    cv_blit(0, BAND_B);
}
static const font_t *band_font(const char *s)
{
    return text_w(&FONT_M, s) - 6 * ((int32_t)str_len(s) - 1) <= 236 ? &FONT_M : &FONT_S;
}
static void draw_sound(const char *name, int message)
{
    uint16_t fg = message ? C_BLACK : TRACK_COL[ui.sel];
    const font_t *f = band_font(name);
    cv_begin(240, 44, message ? C_WHITE : C_BLACK);
    cv_text_fit((44 - f->h) / 2, f, name, fg);
    cv_blit(0, BAND_C);
}
#define METER_X 6
#define METER_LEN 228
static int32_t meter_px_db10(int32_t db10) { return clampi((db10 + 480) * METER_LEN / 540, 0, METER_LEN); }
static int32_t meter_px(int32_t peak)
{
    if (peak < 130)
        return 0;
    return meter_px_db10((int32_t)(((int64_t)(log2_q16((uint32_t)peak) - (15 << 16)) * 602) >> 16) / 10);
}
static void draw_meter(int32_t px)
{
    int32_t lv = ui.level[ui.sel];
    cv_begin(240, BAND_D - BAND_M - 2, C_BLACK);
    cv_frame(METER_X - 2, 3, METER_LEN + 4, 16, 2, C_GREY);
    if (px)
        cv_rect(METER_X, 5, px, 12, TRACK_COL[ui.sel]);
    if (lv)
        cv_rect(METER_X + meter_px_db10((lv - LEVEL_UNITY) * 15 - 150) - 2, 0, 5, 22, C_WHITE);
    else
        cv_rect(METER_X - 2, 0, 5, 22, RGB(255, 30, 30));
    cv_blit(0, BAND_M);
    ui.shown_meter = px;
    ui.shown_level = lv;
}
static void draw_pan(void)
{
    int32_t pn = ui.pan[ui.sel], x = 120 + pn * 104 / PAN_MAX;
    cv_begin(240, 240 - BAND_D, C_BLACK);
    cv_rect(6, 10, 228, 4, C_GREY);
    cv_rect(6, 4, 3, 16, C_GREY);
    cv_rect(231, 4, 3, 16, C_GREY);
    cv_rect(119, 2, 3, 20, C_WHITE);
    cv_rect(x - 9, 0, 18, 24, C_WHITE);
    cv_rect(x - 6, 3, 12, 18, TRACK_COL[ui.sel]);
    cv_blit(0, BAND_D);
    ui.shown_pan = pn;
}

/* a page of four rows (EDIT, the effects): a title, then each row its name and value, a bar under them; the
 * row just turned black on white */
static void draw_rows_title(const char *t1, const char *t2, uint16_t col)
{
    cv_begin(240, 52, C_BLACK);
    cv_text(4, 4, &FONT_M, t1, col);                  /* the page, large; where it is (2/8, A-D), small, right */
    cv_text(236 - text_w(&FONT_XS, t2), 12, &FONT_XS, t2, C_GREY);
    cv_rect(4, 46, 232, 2, RGB(60, 60, 60));
    cv_rect(ui.half ? 120 : 4, 46, 116, 4, col);      /* (which half: A .. D left, E .. H right) */
    cv_blit(0, 0);
}
/* a row: the name left, the value right, each in the largest size that leaves them apart */
static void draw_row(uint32_t r, const char *name, const char *val, int32_t frac, int bip, int touched, uint16_t col)
{
    static const font_t *const F[3] = {&FONT_M, &FONT_S, &FONT_XS};
    uint16_t bg = touched ? C_WHITE : C_BLACK, fg = touched ? C_BLACK : C_WHITE;
    const font_t *fn = &FONT_M, *fv = &FONT_M;
    uint32_t i;
    cv_begin(240, 47, bg);
    if (name[0]) {
        for (i = 0; i < 6u; i++) {                    /* (the name smaller first, then the value) */
            fn = F[i < 3u ? i : 2u];
            fv = F[i < 3u ? 0u : i - 2u > 2u ? 2u : i - 2u];
            if (text_w(fn, name) + text_w(fv, val) + 12 <= 228)
                break;
        }
        cv_text(6, 1 + (FONT_M.h - fn->h) / 2, fn, name, touched ? C_BLACK : RGB(200, 200, 200));
        cv_text(234 - text_w(fv, val), 1 + (FONT_M.h - fv->h) / 2, fv, val, fg);
        cv_rect(6, 38, 228, 6, touched ? C_GREY : RGB(60, 60, 60));
        if (bip) {
            int32_t x0 = 120, x1 = 6 + frac * 228 / 1000;
            cv_rect(x1 < x0 ? x1 : x0, 38, absi(x1 - x0) + 2, 6, touched ? C_BLACK : col);
        } else
            cv_rect(6, 38, frac * 228 / 1000, 6, touched ? C_BLACK : col);
    }
    cv_blit(0, 52 + 47 * r);
}
static void draw_edit_page(void)
{
    const sound_t *s = cur();
    uint32_t id = page_id(s, ui.edit_pg), j;
    int32_t t = (int32_t)(ui.touch_until - fm1_ms) > 0 ? ui.touched : -1;
    int full = ui.force || ui.shown_page != PG_EDIT || ui.shown_half != ui.half || ui.shown_sub != ui.edit_pg;
    if (full) {
        char t1[16], t2[16];
        page_title(s, id, t1);
        fmt_int(t2, ui.edit_pg + 1);                  /* 3/8 */
        str_cpy(t2 + str_len(t2), "/", 2);
        fmt_int(t2 + str_len(t2), (int32_t)pages_n(s));
        draw_rows_title(t1, t2, TRACK_COL[ui.sel]);
    }
    for (j = 0; j < 4u; j++) {
        par_t p;
        int32_t v;
        char b[16];
        par_get(s, id, j + 4u * ui.half, &p);
        v = par_val(s, id, j + 4u * ui.half);
        if (!full && ui.shown_rows[j] == v && ((int32_t)j == t) == ((int32_t)j == ui.shown_touch))
            continue;
        par_text(&p, v, b);
        draw_row(j, p.kind == PK_NONE ? "" : p.name, b, p.hi > p.lo ? (v - p.lo) * 1000 / (p.hi - p.lo) : 0,
                 p.kind == PK_BIP, (int32_t)j == t, TRACK_COL[ui.sel]);
        ui.shown_rows[j] = v;
    }
    ui.shown_touch = (int8_t)t;
    ui.shown_half = ui.half;
    ui.shown_sub = ui.edit_pg;
}
static void dl_time_text(char *b, int32_t v)   /* TIME as 128th notes: 1 .. 128 */
{
    fmt_int(b, v + 1);
}
static void draw_fx_page(void)
{
    const uint8_t *v = fx_vals();
    uint32_t j;
    int32_t t = (int32_t)(ui.touch_until - fm1_ms) > 0 ? ui.touched : -1;
    int full = ui.force || ui.shown_page != PG_FXP || ui.shown_half != ui.half || ui.shown_sub != ui.fx_pg + 100u;
    if (full)
        draw_rows_title(FXT[ui.fx_pg], ui.half ? "E-H" : "A-D", C_SEND);
    for (j = 0; j < 4u; j++) {
        uint32_t k = j + 4u * ui.half;
        char b[16];
        const char *name = k < fx_n() ? (ui.fx_pg == 0 ? RVN[k] : ui.fx_pg == 1 ? DLN[k] : CHN[k]) : "";
        int32_t x = k < fx_n() ? v[k] : 0;
        if (!full && ui.shown_rows[j] == x && ((int32_t)j == t) == ((int32_t)j == ui.shown_touch))
            continue;
        if (ui.fx_pg == 1 && k == DL_TIME)
            dl_time_text(b, x);
        else if (ui.fx_pg == 1 && k == DL_X)
            str_cpy(b, x ? "ON" : "OFF", 4);
        else if (ui.fx_pg == 1 && k == DL_FDBK) {
            fmt_int(b, x * 100 / 64);
            str_cpy(b + str_len(b), "%", 2);
        } else if ((ui.fx_pg == 0 && k == RV_DEC && x >= 127))
            str_cpy(b, "INF", 4);
        else if (ui.fx_pg == 1 && k == DL_WID) {
            if (x > 64)
                str_cpy(b, "+", 2);
            else
                b[0] = 0;
            fmt_int(b + str_len(b), x - 64);
        } else
            fmt_int(b, x);
        draw_row(j, name, b, (ui.fx_pg == 1 && k == DL_X ? x * 127 : x) * 1000 / 127, ui.fx_pg == 1 && k == DL_WID,
                 (int32_t)j == t, C_SEND);
        ui.shown_rows[j] = x;
    }
    ui.shown_touch = (int8_t)t;
    ui.shown_half = ui.half;
    ui.shown_sub = (uint8_t)(ui.fx_pg + 100u);
}

/* a knob's parameter as a dial over the whole screen: its name and the track; a dial from 7 o'clock to 5 (bipolar
 * ones from 12 o'clock either way); its value under it */
#define DIAL_CX 120
#define DIAL_CY 124
#define DIAL_R 76
static void dial_dot(int32_t oy, int32_t x, int32_t y, int32_t r, uint16_t c) { cv_rect(x - r, y - r - oy, 2 * r + 1, 2 * r + 1, c); }
static void dial_at(uint32_t a, int32_t r, int32_t *x, int32_t *y)
{
    *x = DIAL_CX + mulq15(r, sin_ph(a));
    *y = DIAL_CY - mulq15(r, sin_ph(a + 0x40000000u));
}
static void draw_dial(void)
{
    static const int32_t BAND[3][2] = {{0, 112}, {112, 112}, {224, 16}};
    const sound_t *s = cur();
    par_t p;
    int32_t v, b, k, from, to, fr;
    uint32_t av;
    char t[16], m[16];
    uint16_t col = TRACK_COL[ui.sel];
    const font_t *tf;
    par_get(s, ui.dial_pg, ui.dial_j, &p);
    v = par_val(s, ui.dial_pg, ui.dial_j);
    str_cpy(t, p.name, sizeof t - 2);
    str_cpy(t + str_len(t), " ", 2);
    t[str_len(t) + 1] = 0;
    t[str_len(t)] = (char)('1' + ui.sel);
    tf = text_w(&FONT_M, t) <= 236 ? &FONT_M : &FONT_S;
    par_text(&p, v, m);
    fr = p.hi > p.lo ? (v - p.lo) * 1000 / (p.hi - p.lo) : 0;   /* 0 .. 1000 */
    if (p.kind == PK_BIP) {
        from = 0;
        to = (fr - 500) * 45 / 500;
        av = (uint32_t)((fr - 500) * (int32_t)(0x60000000u / 500u));
    } else {
        from = -45;
        to = -45 + fr * 90 / 1000;
        av = 0xA0000000u + (0xC0000000u / 1000u) * (uint32_t)fr;
    }
    if (to < from) {
        int32_t x_ = to;
        to = from;
        from = x_;
    }
    for (b = 0; b < 3; b++) {
        int32_t oy = BAND[b][0], x, y;
        CV_AREA(0, (uint32_t)oy, 240, (uint32_t)BAND[b][1], C_BLACK) {
            cv_text(((int32_t)cv_w - text_w(tf, t)) / 2, 2 + ((int32_t)FONT_M.h - (int32_t)tf->h) / 2 - oy, tf, t, col);
            for (k = -45; k <= 45; k++) {
                dial_at((uint32_t)(k * 35791394), DIAL_R, &x, &y);
                dial_dot(oy, x, y, 4, C_GREY);
            }
            for (k = from; k <= to; k++) {
                dial_at((uint32_t)(k * 35791394), DIAL_R, &x, &y);
                dial_dot(oy, x, y, 7, col);
            }
            for (k = 0; k <= DIAL_R - 16; k += 4) {
                dial_at(av, k, &x, &y);
                dial_dot(oy, x, y, 4, C_WHITE);
            }
            dial_dot(oy, DIAL_CX, DIAL_CY, 10, C_WHITE);
            if (b >= 1)
                cv_text_fit(200 - oy, &FONT_M, m, C_WHITE);
        }
    }
    ui.shown_dial = v + ui.dial_pg * 1024 + ui.dial_j * 8192;
    ui.dial_drawn_at = fm1_ms;
}

static int draw_flash(void)
{
    uint32_t t = fm1_ms - ui.flash_t0, ph = t / 250u, on;
    uint16_t bg, fg;
    if (!ui.flash_on)
        return 0;
    if (t >= 1500u) {
        ui.flash_on = 0;
        ui.force = 1;
        return 0;
    }
    if (ph == ui.flash_phase)
        return 1;
    ui.flash_phase = (uint8_t)ph;
    on = !(ph & 1u);
    bg = on ? ui.flash_col : C_BLACK;
    fg = on ? C_BLACK : ui.flash_col;
    CV_AREA(0, 0, 240, 100, bg)
        cv_text_c(ui.flash_l2[0] ? 40 : 52, &FONT_L, ui.flash_l1, fg);
    CV_AREA(0, 100, 240, 140, bg)
        cv_text_fit(10, &FONT_L, ui.flash_l2, fg);
    return 1;
}

static void ui_draw(void)
{
    int msg = (int32_t)(ui.msg_until - fm1_ms) > 0;
    const char *c = msg ? ui.msg : family_name(cur());
    int32_t px;
    if (draw_flash())
        return;
    if ((int32_t)(ui.dial_until - fm1_ms) > 0 && ui.page == PG_MAIN) {
        int32_t key = par_val(cur(), ui.dial_pg, ui.dial_j) + ui.dial_pg * 1024 + ui.dial_j * 8192;
        if (ui.shown_page != 4 || (ui.shown_dial != key && (uint32_t)(fm1_ms - ui.dial_drawn_at) >= 40u))
            draw_dial();
        ui.shown_page = 4;
        return;
    }
    if (ui.page == PG_EDIT) {
        draw_edit_page();
        ui.shown_page = PG_EDIT;
        ui.force = 0;
        return;
    }
    if (ui.page == PG_FXP) {
        draw_fx_page();
        ui.shown_page = PG_FXP;
        ui.force = 0;
        return;
    }
    if (ui.page == PG_PRJ || ui.page == PG_PRJD) {
        prj_draw();
        ui.shown_page = ui.page;
        ui.force = 0;
        return;
    }
    if (ui.shown_page != PG_MAIN) {
        ui.shown_page = PG_MAIN;
        ui.force = 1;
        lcd_fill(0, 0, 240, 240, C_BLACK);           /* (the gaps between the bands, from the page before) */
    }
    if (ui.force || ui.shown_sel != ui.sel || ui.shown_prj != prj_cur * 2 + prj_dirty) {
        ui.shown_prj = (int16_t)(prj_cur * 2 + prj_dirty);
        draw_number();
        ui.shown_sel = ui.sel;
        ui.shown_eng = ui.shown_msg = ui.shown_pan = ui.shown_level = -100;
    }
    if (ui.shown_eng != ui.eng[ui.sel]) {
        draw_engine();
        ui.shown_eng = ui.eng[ui.sel];
        ui.shown_msg = -1;
    }
    if (ui.shown_msg != msg || !str_eq(c, ui.shown_text)) {
        draw_sound(c, msg);
        str_cpy(ui.shown_text, c, sizeof ui.shown_text);
        ui.shown_msg = msg;
    }
    px = meter_px(trk[ui.sel].meter);
    if (ui.shown_level != ui.level[ui.sel] || ((uint32_t)(fm1_ms - ui.meter_at) >= 30u && px != ui.shown_meter)) {
        draw_meter(px);
        ui.meter_at = fm1_ms;
    }
    if (ui.shown_pan != ui.pan[ui.sel])
        draw_pan();
    ui.force = 0;
}

/* the start: on each track a sound of each machine; tracks 1 .. 3 on FM DRUM (a kick, a snare, a hat), 4 FM TONE,
 * 5 WAVETONE, 6 SWARMER */
static void ui_init(void)
{
    static const uint8_t ENG0[NTRACK] = {ENG_FMD, ENG_FMD, ENG_FMD, ENG_FMT, ENG_WAV, ENG_SWM};
    static const int8_t FAM0[NTRACK] = {0, 4, 8, -1, -1, -1};
    uint32_t t, e;
    memset(&ui, 0, sizeof ui);
    memset(ui.midi_track, 0xFF, sizeof ui.midi_track);
    memset(ui.key_track, 0xFF, sizeof ui.key_track);
    ui.seed = fm1_ticks() ^ 0x5EEDu;
    ui.scale = 6;                                    /* minor pentatonic, from C */
    ui.tempo = 120;
    for (t = 0; t < NTRACK; t++) {
        ui.eng[t] = ENG0[t];
        for (e = 0; e < NENG; e++)
            gen_sound(&ui.snd[t][e], e, e == ENG0[t] ? FAM0[t] : -1, -1, next_seed());
        ui.level[t] = LEVEL_UNITY;
        ui.pan[t] = PAN_DEF[t];
        level_pan_apply(t);
        ck_cook(&ck_buf[t][0], &ui.snd[t][ui.eng[t]]);
        ck_buf[t][1] = ck_buf[t][0];
        ck_pub[t] = ck_use[t] = 0;
    }
    ui.oct[0] = -1;
    ui.touched = -1;
    ui.shown_sel = ui.shown_eng = ui.shown_msg = -1;
    ui.force = 1;
}
