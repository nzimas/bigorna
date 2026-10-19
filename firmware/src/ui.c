/* SPDX-License-Identifier: GPL-3.0-only
 * The panel and the screen (after TONITOR's ui.c: its panel map, its bands, its dial, its scale page).
 *
 *   FX SEL ENV LFO EDIT GLO   the top row: tracks 1 to 6 (lit: the chosen one; the others glow while they sound)
 *   PRESETS (knob)            the track's machine: FM TONE, FM DRUM, WAVETONE, SWARMER (each track keeps a sound of
 *                             each, so turning back finds it)
 *   HOME                      a new sound for the track, on its machine (gen.c); SHIFT + HOME: of the same family
 *   OCT- / OCT+               the track's octave, -3 .. +3; on the EDIT page, the page before / after
 *   the white keys            play the track in the scale chosen (the first key the root, each the next degree)
 *   the rightmost black key   SHIFT
 *   the 3rd black key from the right   STEP: the step view, on or off (lit while on). The white keys are the track's
 *                             16 steps: a key on an empty step puts the track's last note played there; a key on a
 *                             step that holds a note, let go soon (0.4 s) and nothing changed, empties it. In the
 *                             view the black keys, lit dimly:
 *                               the 1st   a new phrase for the track (phrase.c): its steps, conditions and locks
 *                               the 4th   TRIG: the knobs on the step held: KNOB 1 the note (in the scale), 2 its
 *                                         velocity, 3 its length, 4 its trig condition
 *   PLAY                      the sequencer: plays, or stops (and recording ends, a loop being recorded is dropped)
 *   SHIFT + PLAY              the metronome off or on (it clicks while REC or the looper is armed or recording)
 *   REC                       recording into the chosen track's steps. Stopped: armed (REC flashes, the metronome
 *                             counts), the first note played starts the sequencer and is step 1. Playing: recording
 *                             at once (punch in). Each note played goes to the nearest step, its velocity and its
 *                             length (as held) with it, over what the step held; round and round until REC again
 *                             (it plays on)
 *   SHIFT + REC               the looper (after TONITOR's): a new loop on the chosen track, armed; the first note
 *                             played starts it (and the sequencer, if stopped); every note played after is in it, at
 *                             the nearest step, with its velocity and length, until REC ends it (at the nearest step):
 *                             it plays from there, round and round in its own length (up to 16 bars), over the
 *                             track's steps. Three loops a track
 *   SHIFT + REC held          the track's loops on the first three white keys (lit: playing; dim: muted): a key let
 *                             go soon mutes its loop or plays it again; held 0.8 s, empties it. Held 1.5 s with no
 *                             white key touched: every loop of the track emptied
 *   SHIFT + PRESETS           the track's sound: a new one of the family before / after, on the machine it plays
 *   the black key before it   EDIT: the parameter pages of the track's sound -- SYN 1 .. 4, FLTR 1, FLTR 2, AMP,
 *                             FX -- KNOB 1 .. 4 the page's A .. D, with SHIFT held E .. H
 *   ARP                       the patterns (projects.c)
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
#define KEY_STEP 20u                 /* (the 3rd black key from the right) */
#define KEY_GEN 1u                   /* (the 1st black key from the left: in the step view, a new phrase) */
#define KEY_NONE 0xFFu

static const uint16_t TRACK_COL[NTRACK] = {RGB(255, 214, 0), RGB(0, 214, 255), RGB(255, 92, 210),
                                           RGB(96, 255, 96), RGB(255, 140, 0), RGB(190, 160, 255)};
enum { PG_MAIN, PG_EDIT, PG_FXP, PG_PRJ, PG_PRJD, PG_PAT };   /* (the sessions' list and page, the patterns' grid:
                                                              * projects.c) */

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
    uint8_t last_note[NTRACK];   /* the note last played on each track (0: none yet): what a step is given */
    uint8_t step_key;            /* SHIFT: the key of the step held (KEY_NONE none) */
    uint8_t step_new, step_edit;     /* .. its note put there as it was pressed; changed since */
    uint32_t step_t0;            /* .. pressed at */
    uint8_t sv;                  /* the step view on (STEP) */
    uint32_t gen_until;          /* the phrase key lit fully until */
    uint8_t smode;               /* the step view's knobs: SM_* (the black key chosen) */
    uint8_t step_heard;          /* .. the note it sounds as it is held (0 none) */
    uint8_t qk, qk_long;         /* the PATTERNS grid: the white key held (KEY_NONE none); held 0.8 s, its pattern emptied */
    uint8_t rec;                 /* REC_* */
    uint8_t click;               /* the metronome while recording */
    uint8_t lrec;                /* the looper: 0 off, 1 armed, 2 recording */
    uint8_t lt, ll;              /* .. the track and the loop it records */
    uint32_t lstart;             /* .. the step its first note went to (seq_gcount's count) */
    uint8_t lrec_down, lview;    /* SHIFT + REC held: on release, armed -- unless the loops were shown (lview) */
    uint8_t lv_any;              /* .. a white key touched, or the loops emptied, while it was held */
    uint32_t lrec_t0;
    uint8_t lv_k, lv_long;       /* the loops shown: the white key held, held 0.8 s */
    uint32_t lv_t0;
    uint16_t lk_ev[NKEY];        /* a key held as the looper recorded it: its note in the loop (0xFFFF none) .. */
    uint32_t lk_t0[NKEY];        /* .. and when it was pressed */
    uint8_t rec_step[NKEY], rec_tr[NKEY];   /* a key held as it was recorded: its step (0xFF none) and track .. */
    uint32_t rec_t0[NKEY];       /* .. and when it was pressed */
    uint32_t qk_t0;
    uint8_t midi_track[128];
    uint32_t keys;
    char msg[16];
    uint32_t msg_until, home_until;
    uint32_t home_t0, arp_t0;    /* HOME, ARP pressed at (held 0.8 s: a save; let go sooner: a press) */
    uint8_t home_down, home_long, arp_down, arp_long;
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
    int16_t shown_prj;           /* (the pattern shown at the top left, and its changes) */
    int32_t shown_steps;         /* (the steps' strip, folded into one number; -1 not shown) */
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
static uint8_t prj_dirty, pat_cur, pat_dirty;
static int8_t prj_cur;
static void prj_press(int sh);
static void prj_preset(int32_t s);
static void prj_draw(void);
static void prj_quick_save(void);
static void pat_quick_save(void);
static void pat_switch(uint32_t k);
static void pat_clear(uint32_t k);
static void phrase_new(uint32_t t);              /* (phrase.c) */
static int pg_any(void) { return ui.page >= PG_PRJ; }   /* a projects' or patterns' page */

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
static int key_plays(uint32_t k)
{
    return k != KEY_SHIFT && k != KEY_EDIT && k != KEY_STEP && (ui.scale == 0 || white_ix(k) >= 0);
}
/* a note in the scale (and the root); the next one up or down from n, d times */
static int in_scale(int32_t n)
{
    uint32_t i, d = (uint32_t)(n - ui.root + 120) % 12u;
    for (i = 0; i < SCALE_N[ui.scale]; i++)
        if (SCALE_STEP[ui.scale][i] == d)
            return 1;
    return 0;
}
static int32_t scale_move(int32_t n, int32_t d)
{
    int32_t u = d > 0 ? 1 : -1, m;
    for (; d; d -= u) {
        for (m = n + u; m >= 12 && m <= 120 && !in_scale(m); m += u)
            ;
        if (m < 12 || m > 120)
            break;
        n = m;
    }
    return n;
}
/* the step view's black keys, from the fourth on: what each makes the knobs do to the step held */
enum { SM_NONE, SM_TRIG, NSM };
static const uint8_t SM_KEY[NSM] = {KEY_NONE, 8};   /* (the fourth black key from the left: C#) */
static uint32_t sm_of(uint32_t k)
{
    uint32_t m;
    for (m = 1; m < NSM; m++)
        if (SM_KEY[m] == k)
            return m;
    return SM_NONE;
}
static int step_view(void) { return ui.sv; }
static int step_page(void) { return ui.smode && ui.step_key != KEY_NONE; }   /* (the step's knobs on the screen) */
static void note_text(char *b, uint32_t n)     /* C#3 */
{
    str_cpy(b, NOTE_NAME[n % 12u], 4);
    fmt_int(b + str_len(b), (int32_t)(n / 12u) - 1);
}

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

/* ----------------------------------------------------------- recording --- */
enum { REC_OFF, REC_ARMED, REC_ON };
static void rec_set(uint32_t r)
{
    ui.rec = (uint8_t)r;
    met_on = (uint8_t)((r != REC_OFF || ui.lrec) && ui.click);
    ui.force = 1;
}

/* the looper (the loops themselves: tracks.c). The step a note goes to: the nearest, in seq_gcount's count */
static uint32_t loop_qnow(void)
{
    uint32_t g = seq_gcount, acc = seq_acc_now;
    return g == 0xFFFFFFFFu ? 0u : g + (acc >= SEQ_16TH / 2u);
}
static uint32_t loop_len8(uint32_t ms)           /* a time held, in eighths of a step */
{
    uint32_t e = ((ms > 60000u ? 60000u : ms) * (uint32_t)fx_bpm10 * 8u + 75000u) / 150000u;   /* (rounded) */
    return e ? (e < 2048u ? e : 2048u) : 1u;
}
static void loop_arm(void)
{
    uint32_t t = ui.sel, l;
    for (l = 0; l < NLOOP && loops[t][l].on; l++)
        ;
    if (l == NLOOP) {
        say("3 LOOPS", 1200);
        return;
    }
    loops[t][l].n = 0;
    ui.lt = (uint8_t)t;
    ui.ll = (uint8_t)l;
    ui.lrec = 1;
    memset(ui.lk_ev, 0xFF, sizeof ui.lk_ev);
    rec_set(REC_OFF);                            /* (REC's recording off: one at a time) */
    say("LOOP ARMED", 900);
}
static void loop_drop(void)                      /* the loop being recorded, dropped */
{
    loops[ui.lt][ui.ll].n = 0;
    ui.lrec = 0;
    rec_set(ui.rec);
}
/* a note played with the looper armed or recording (k: its key, NKEY none) */
static void loop_note(uint32_t note, uint32_t vel, uint32_t k)
{
    loop_t *p = &loops[ui.lt][ui.ll];
    uint32_t q;
    if (!ui.lrec || ui.sel != ui.lt)
        return;
    if (!seq_run) {                              /* (stopped: the sequencer starts on it) */
        seq_run = 1;
        q = 0;
    } else
        q = loop_qnow();
    if (ui.lrec == 1) {
        ui.lstart = q;
        ui.lrec = 2;
        ui.force = 1;
    }
    if (q - ui.lstart >= LOOP_MAX || p->n >= LOOP_EV)
        return;
    p->ev[p->n].step = (uint16_t)(q - ui.lstart);
    p->ev[p->n].len8 = 4;
    p->ev[p->n].note = (uint8_t)note;
    p->ev[p->n].vel = (uint8_t)clampi((int32_t)vel, 1, 127);
    if (k < NKEY) {
        ui.lk_ev[k] = p->n;
        ui.lk_t0[k] = fm1_ms;
    }
    p->n++;
}
static void loop_release(uint32_t k)             /* a key the looper recorded let go: its note's length */
{
    if (ui.lrec == 2 && ui.lk_ev[k] < loops[ui.lt][ui.ll].n)
        loops[ui.lt][ui.ll].ev[ui.lk_ev[k]].len8 = (uint16_t)loop_len8(fm1_ms - ui.lk_t0[k]);
    ui.lk_ev[k] = 0xFFFF;
}
/* REC: the loop ends at the nearest step and plays from there (armed, nothing played: not armed) */
static void loop_end(void)
{
    loop_t *p = &loops[ui.lt][ui.ll];
    uint32_t q = loop_qnow(), len, i, j;
    char m[8] = "LOOP ";
    if (ui.lrec != 2 || !p->n) {
        loop_drop();
        say("LOOP OFF", 900);
        return;
    }
    len = q - ui.lstart;
    len = len < 1u ? 1u : len > LOOP_MAX ? LOOP_MAX : len;
    for (i = 0; i < NKEY; i++)                   /* (keys still held: their notes to here) */
        if (ui.lk_ev[i] != 0xFFFFu)
            loop_release(i);
    for (i = 0; i < p->n; i++)                   /* (a note at the end is the loop's first) */
        p->ev[i].step = (uint16_t)(p->ev[i].step % len);
    for (i = 1; i < p->n; i++) {                 /* (in order of steps) */
        lev_t e = p->ev[i];
        for (j = i; j > 0u && p->ev[j - 1u].step > e.step; j--)
            p->ev[j] = p->ev[j - 1u];
        p->ev[j] = e;
    }
    p->len = (uint16_t)len;
    p->anchor = q;
    p->mute = 0;
    p->cur = 0;
    p->kick = (uint8_t)(seq_gcount != 0xFFFFFFFFu && q <= seq_gcount);   /* (its first step gone by: played now) */
    RING_PUBLISH();
    p->on = 1;
    ui.lrec = 0;
    rec_set(ui.rec);
    fmt_int(m + 5, ui.ll + 1);
    say(m, 1200);
}
static uint32_t loops_n(uint32_t t)
{
    uint32_t l, n = 0;
    for (l = 0; l < NLOOP; l++)
        n += loops[t][l].on;
    return n;
}
/* a note played while recording (k: its key, NKEY none): into the chosen track's nearest step; the step the
 * sequencer comes to next is not played that once (the key has) */
static void rec_note(uint32_t note, uint32_t vel, uint32_t k)
{
    uint32_t t = ui.sel, i, l;
    if (ui.rec == REC_ARMED) {                   /* the first note: the sequencer starts on it, step 1 */
        seq_skip[t][0] = 1;
        seq_run = 1;
        rec_set(REC_ON);
        i = 0;
    } else if (ui.rec != REC_ON || !seq_run)
        return;
    else {
        uint32_t pos = seq_pos[t], acc = seq_acc_now, len = seq_len[t] ? seq_len[t] : NSTEP;
        if (pos >= len) {                        /* (just started: step 1 to come) */
            i = 0;
            seq_skip[t][0] = 1;
        } else if (acc < SEQ_16TH / 2u)
            i = pos;
        else {
            i = (pos + 1u) % len;
            seq_skip[t][i] = 1;
        }
    }
    seq_vel[t][i] = (uint8_t)clampi((int32_t)vel, 1, 127);
    seq_glen[t][i] = GLEN_DEF;
    seq_cond[t][i] = CD_NONE;
    for (l = 0; l < NLK; l++)
        seq_lock[t][i][l] = LK_NONE;
    seq_note[t][i] = (uint8_t)note;
    pat_dirty = 1;
    if (k < NKEY) {
        ui.rec_step[k] = (uint8_t)i;
        ui.rec_tr[k] = (uint8_t)t;
        ui.rec_t0[k] = fm1_ms;
    }
}
/* a key recorded let go: its step's length, as it was held (the nearest of the lengths, up to 16 steps) */
static void rec_release(uint32_t k)
{
    uint32_t i = ui.rec_step[k], t = ui.rec_tr[k], ms = fm1_ms - ui.rec_t0[k], e, g, best = 0;
    ui.rec_step[k] = 0xFF;
    if (i >= NSTEP)
        return;
    if (ms > 60000u)
        ms = 60000u;
    e = ms * (uint32_t)fx_bpm10 * 8u / 150000u;  /* (eighths of a step: a step is 15000 / BPM ms) */
    for (g = 1; g < GLEN_INF; g++)
        if (absi((int32_t)GLEN_8TH[g] - (int32_t)e) < absi((int32_t)GLEN_8TH[best] - (int32_t)e))
            best = g;
    seq_glen[t][i] = (uint8_t)best;
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
            ui.last_note[ui.sel] = (uint8_t)note;
            ev_post(EV_ON, ui.sel, note, vel);
            rec_note(note, vel, NKEY);
            loop_note(note, vel, NKEY);
        } else if ((cin == 8u || cin == 9u) && ui.midi_track[note] != 0xFFu) {
            ev_post(EV_OFF, ui.midi_track[note], note, 0);
            ui.midi_track[note] = 0xFFu;
        }
    }
}

/* SHIFT and a white key: step i of the chosen track. Pressed on an empty step, the track's last note put there; let go
 * without PRESETS turned, a step that held a note emptied. Held, the step's note sounds */
static void step_say(uint32_t i, uint32_t n)
{
    char m[16] = "STEP ";
    fmt_int(m + 5, (int32_t)i + 1);
    str_cpy(m + str_len(m), "  ", 3);
    if (n)
        note_text(m + str_len(m), n);
    else
        str_cpy(m + str_len(m), "OFF", 4);
    say(m, 1500);
}
static void step_hear(uint32_t n)
{
    if (ui.step_heard)
        ev_post(EV_OFF, ui.sel, ui.step_heard, 0);
    ui.step_heard = (uint8_t)n;
    if (n)
        ev_post(EV_ON, ui.sel, n, seq_vel[ui.sel][white_ix(ui.step_key)]);
}
static void step_down(uint32_t k)
{
    uint32_t i = (uint32_t)white_ix(k), n = seq_note[ui.sel][i];
    ui.step_key = (uint8_t)k;
    ui.step_edit = 0;
    ui.step_new = !n;
    ui.step_t0 = fm1_ms;
    if (!n) {
        n = ui.last_note[ui.sel] ? ui.last_note[ui.sel] : key_note(0, ui.oct[ui.sel]);
        seq_vel[ui.sel][i] = VEL_DEF;
        seq_glen[ui.sel][i] = GLEN_DEF;
        seq_cond[ui.sel][i] = CD_NONE;
        seq_note[ui.sel][i] = (uint8_t)n;
        pat_dirty = 1;
    }
    if (!ui.smode)
        step_say(i, n);
    ui.force = 1;
    step_hear(n);
}
/* the step view's knobs on the step held: TRIG: the note, the velocity, the length, the condition */
static void step_knob(uint32_t j, int32_t s)
{
    uint32_t t = ui.sel, i = (uint32_t)white_ix(ui.step_key);
    if (j == 0u) {
        seq_note[t][i] = (uint8_t)scale_move((int32_t)seq_note[t][i], s);
        step_hear(seq_note[t][i]);
    } else if (j == 1u)
        seq_vel[t][i] = (uint8_t)clampi(seq_vel[t][i] + s, 1, 127);
    else if (j == 2u)
        seq_glen[t][i] = (uint8_t)clampi(seq_glen[t][i] + s, 0, NGLEN - 1);
    else
        seq_cond[t][i] = (uint8_t)clampi(seq_cond[t][i] + s, 0, NCOND - 1);
    ui.step_edit = 1;
    ui.touched = (int8_t)j;
    ui.touch_until = fm1_ms + 1500u;
    pat_dirty = 1;
}
static void step_up(void)
{
    uint32_t i = (uint32_t)white_ix(ui.step_key);
    step_hear(0);
    if (!ui.step_new && !ui.step_edit && (uint32_t)(fm1_ms - ui.step_t0) < 400u) {
        seq_note[ui.sel][i] = 0;
        pat_dirty = 1;
        step_say(i, 0);
    }
    ui.step_key = KEY_NONE;
    ui.force = 1;
}

static void ui_input(void)
{
    uint32_t rel, press = fm1_input_edges(&rel), keys, ch, k, t, j;
    int32_t s, e_pre, e_sel, e_alg;
    int sh = (int)((fm1_in.notes >> KEY_SHIFT) & 1u);
    (void)rel;
    for (t = 0; t < NTRACK; t++)
        if (btn_bit(press, TRACK_BTN[t])) {
            ui.sel = (uint8_t)t;
            if (ui.page == PG_FXP || pg_any())
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
    /* HOME: let go soon, a new sound (SHIFT: of the same family); held 0.8 s, the project saved. ARP: let go soon, the
     * PATTERNS page up or down; held, the pattern saved */
    if (btn_bit(press, B_HOME)) {
        ui.home_down = 1;
        ui.home_long = 0;
        ui.home_t0 = fm1_ms;
    }
    if (ui.home_down) {
        if (!btn_bit(fm1_in.buttons, B_HOME)) {
            ui.home_down = 0;
            if (!ui.home_long)
                new_sound(sh);
        } else if (!ui.home_long && (uint32_t)(fm1_ms - ui.home_t0) >= 800u) {
            ui.home_long = 1;
            prj_quick_save();
        }
    }
    if (btn_bit(press, B_ARP)) {
        ui.arp_down = 1;
        ui.arp_long = 0;
        ui.arp_t0 = fm1_ms;
    }
    if (ui.arp_down) {
        if (!btn_bit(fm1_in.buttons, B_ARP)) {
            ui.arp_down = 0;
            if (!ui.arp_long) {                      /* (the grid up; on it, the pattern chosen loaded) */
                if (ui.page == PG_PAT)
                    prj_press(0);
                else
                    ui.page = PG_PAT;
                ui.force = 1;
            }
        } else if (!ui.arp_long && (uint32_t)(fm1_ms - ui.arp_t0) >= 800u) {
            ui.arp_long = 1;
            pat_quick_save();
        }
    }
    ui.half = (uint8_t)sh;
    if (step_page()) {                               /* a step held in the step view: the knobs its */
        for (j = 0; j < 4u; j++)
            if ((s = enc_take(KNOB[j])) != 0)
                step_knob(j, s);
    } else if (ui.page == PG_EDIT) {                        /* the page's parameters */
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
    if (btn_bit(press, B_PLAY) && sh) {              /* SHIFT + PLAY: the metronome */
        ui.click = (uint8_t)!ui.click;
        rec_set(ui.rec);
        say(ui.click ? "CLICK ON" : "CLICK OFF", 900);
    } else if (btn_bit(press, B_PLAY)) {             /* the sequencer (stopped: recording ends, a loop being
                                                      * recorded dropped; armed: it plays, recording) */
        seq_run = (uint8_t)!seq_run;
        if (!seq_run && ui.lrec)
            loop_drop();
        if (!seq_run && ui.rec)
            rec_set(REC_OFF);
        else if (seq_run && ui.rec == REC_ARMED)
            rec_set(REC_ON);
        say(seq_run ? "PLAY" : "STOP", 700);
    }
    if (btn_bit(press, B_REC) && ui.lrec) {          /* the looper armed or recording: REC ends the loop (SHIFT
                                                      * and held on: emptied, as below) */
        loop_end();
        if (sh) {
            ui.lrec_down = ui.lview = 1;
            ui.lv_any = 0;
            ui.lrec_t0 = fm1_ms;
        }
    } else if (btn_bit(press, B_REC) && sh) {        /* SHIFT + REC: the looper (on release; held: its loops) */
        ui.lrec_down = 1;
        ui.lview = ui.lv_any = 0;
        ui.lrec_t0 = fm1_ms;
    } else if (btn_bit(press, B_REC)) {              /* REC: armed (playing: recording), or off */
        rec_set(ui.rec ? REC_OFF : seq_run ? REC_ON : REC_ARMED);
        say(ui.rec == REC_ON ? "RECORDING" : ui.rec == REC_ARMED ? "REC ARMED" : "REC OFF", 900);
    }
    if (ui.lrec_down) {
        if (!btn_bit(fm1_in.buttons, B_REC)) {
            ui.lrec_down = 0;
            if (!ui.lview)
                loop_arm();
            ui.lview = 0;
            ui.lv_k = KEY_NONE;
            ui.force = 1;
        } else if (!ui.lv_any && sh && (uint32_t)(fm1_ms - ui.lrec_t0) >= 1500u) {   /* (held on: all emptied) */
            uint32_t l;
            for (l = 0; l < NLOOP; l++) {
                loops[ui.sel][l].on = 0;
                RING_PUBLISH();
                loops[ui.sel][l].n = loops[ui.sel][l].len = 0;
            }
            ui.lv_any = ui.lview = 1;
            flash("EMPTY", "LOOPS", C_BAD);
        } else if (!ui.lview && (uint32_t)(fm1_ms - ui.lrec_t0) >= 400u) {
            char m[12] = "LOOPS ";
            ui.lview = 1;
            fmt_int(m + 6, (int32_t)loops_n(ui.sel));
            say(m, 1500);
        }
    }
    if (ui.lrec == 2 && loop_qnow() - ui.lstart >= LOOP_MAX)   /* (16 bars: the loop ends by itself) */
        loop_end();
    /* PRESETS, SELECT, ALGORITHM: each read once, then to the page's own use (on the sessions' and patterns' pages
     * PRESETS is theirs, SELECT and ALGORITHM do nothing) */
    e_pre = enc_take(EN_PRESET);
    e_sel = enc_take(EN_SELECT);
    e_alg = enc_take(EN_ALGO);
    if (pg_any()) {
        if (e_pre)
            prj_preset(e_pre);
        e_pre = e_sel = e_alg = 0;
    }
    if ((s = e_sel) != 0) {                          /* the level */
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
    if ((s = e_alg) != 0) {                          /* the pan */
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
    if ((s = e_pre) != 0 && sh) {                    /* SHIFT + PRESETS: the family before / after, a new sound */
        sound_t *sn = cur();
        uint32_t m = ui.eng[ui.sel];
        int32_t f = sn->family, n = (int32_t)NRECIPE, d = s > 0 ? 1 : -1, i;
        if (f < 0 || f >= n || RECIPES[f].mach != m)
            f = d > 0 ? -1 : n;                      /* (not of this machine: from its first, or its last) */
        for (; s; s -= d) {
            for (i = f + d; i >= 0 && i < n && RECIPES[i].mach != m; i += d)
                ;
            if (i < 0 || i >= n)
                break;                           /* (the machine's first or last family: it stays) */
            f = i;
        }
        if (f >= 0 && f < n && RECIPES[f].mach == m) {
            gen_sound(sn, m, f, -1, next_seed());
            mark(ui.sel);
            ui.home_until = fm1_ms + 300u;
        }
    } else if ((s = e_pre) != 0) {                   /* the machine */
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
    if (ui.lrec_down) {                              /* SHIFT + REC held: the loops on the first three white keys */
        for (k = 0; k < NKEY; k++)
            if (((ch & keys) >> k & 1u) && white_ix(k) >= 0 && white_ix(k) < (int32_t)NLOOP && ui.lv_k == KEY_NONE) {
                ui.lview = ui.lv_any = 1;
                ui.lv_k = (uint8_t)k;
                ui.lv_long = 0;
                ui.lv_t0 = fm1_ms;
            }
        if (ui.lv_k != KEY_NONE) {
            loop_t *p = &loops[ui.sel][white_ix(ui.lv_k)];
            char m[12] = "LOOP ";
            fmt_int(m + 5, white_ix(ui.lv_k) + 1);
            if (!((keys >> ui.lv_k) & 1u)) {
                if (!ui.lv_long && p->on) {     /* (let go soon: muted, or playing again) */
                    p->mute = (uint8_t)!p->mute;
                    str_cpy(m + str_len(m), p->mute ? " MUTE" : " ON", 6);
                    say(m, 900);
                }
                ui.lv_k = KEY_NONE;
            } else if (!ui.lv_long && (uint32_t)(fm1_ms - ui.lv_t0) >= 800u) {
                ui.lv_long = 1;                  /* (held: emptied) */
                p->on = 0;
                RING_PUBLISH();
                p->n = 0;
                p->len = 0;
                flash("EMPTY", m, C_BAD);
            }
        }
        ui.keys = keys;
        midi_take();
        return;
    }
    ui.lv_k = KEY_NONE;
    if (ui.page == PG_PAT) {                         /* the PATTERNS grid: a white key let go soon, its pattern
                                                      * loaded; held 0.8 s, emptied */
        for (k = 0; k < NKEY; k++) {
            if (((ch & ~keys) >> k & 1u) && ui.key_track[k] != KEY_NONE) {   /* (a note held from before: let go) */
                ev_post(EV_OFF, ui.key_track[k], ui.key_note[k], 0);
                ui.key_track[k] = KEY_NONE;
            }
            if (((ch & keys) >> k & 1u) && white_ix(k) >= 0 && ui.qk == KEY_NONE) {
                ui.qk = (uint8_t)k;
                ui.qk_long = 0;
                ui.qk_t0 = fm1_ms;
                ui.key_track[k] = KEY_NONE;
            }
        }
        if (ui.qk != KEY_NONE) {
            if (!((keys >> ui.qk) & 1u)) {
                if (!ui.qk_long)
                    pat_switch((uint32_t)white_ix(ui.qk));
                ui.qk = KEY_NONE;
            } else if (!ui.qk_long && (uint32_t)(fm1_ms - ui.qk_t0) >= 800u) {
                ui.qk_long = 1;
                pat_clear((uint32_t)white_ix(ui.qk));
            }
        }
        ui.keys = keys;
        midi_take();
        return;
    }
    ui.qk = KEY_NONE;
    if ((ch & keys) >> KEY_STEP & 1u) {              /* STEP: the step view on or off */
        ui.sv = (uint8_t)!ui.sv;
        if (!ui.sv && ui.step_key != KEY_NONE)
            step_up();
        ui.force = 1;
    }
    for (k = 0; ch && k < NKEY; k++) {
        if (((ch & keys) >> k & 1u) && step_view() && k == KEY_GEN) {   /* a new phrase for the track */
            phrase_new(ui.sel);
            ui.gen_until = fm1_ms + 400u;
            flash("NEW", "PHRASE", TRACK_COL[ui.sel]);
            continue;
        }
        if (((ch & keys) >> k & 1u) && step_view() && sm_of(k)) {   /* the step view: a black key, its knobs (again:
                                                                      * none) */
            ui.smode = (uint8_t)(ui.smode == sm_of(k) ? SM_NONE : sm_of(k));
            say(ui.smode == SM_TRIG ? "TRIG" : "KNOBS", 700);
            ui.force = 1;
            continue;
        }
        if (!((ch >> k) & 1u) || !key_plays(k))
            continue;
        if ((keys >> k) & 1u) {
            uint32_t note = key_note(k, ui.oct[ui.sel]);
            ui.key_track[k] = KEY_NONE;
            if (step_view()) {                       /* the step view: the white keys are the steps */
                if (white_ix(k) >= 0 && ui.step_key == KEY_NONE)
                    step_down(k);
                continue;
            }
            ui.key_track[k] = ui.sel;
            ui.key_note[k] = (uint8_t)note;
            ui.last_note[ui.sel] = (uint8_t)note;
            ev_post(EV_ON, ui.sel, note, 127);
            rec_note(note, 127, k);
            loop_note(note, 127, k);
        } else if (k == ui.step_key)
            step_up();
        else if (ui.key_track[k] != KEY_NONE) {
            ev_post(EV_OFF, ui.key_track[k], ui.key_note[k], 0);
            ui.key_track[k] = KEY_NONE;
            if (ui.rec_step[k] != 0xFFu)
                rec_release(k);
            if (ui.lk_ev[k] != 0xFFFFu)
                loop_release(k);
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
    fm1_led_key(PANEL_BTN[B_ARP], ui.page == PG_PAT);
    fm1_led_key(PANEL_BTN[B_SAVE], ui.page == PG_PRJ || ui.page == PG_PRJD);
    fm1_led_key(PANEL_BTN[B_OCTDN], ui.page == PG_EDIT || ui.oct[ui.sel] < 0);
    fm1_led_key(PANEL_BTN[B_OCTUP], ui.page == PG_EDIT || ui.oct[ui.sel] > 0);
    fm1_led_key(PANEL_BTN[B_REC], ui.rec == REC_ON || ui.lrec == 2 ||
                                      ((ui.rec == REC_ARMED || ui.lrec == 1) && (fm1_ms / 250u) & 1u));
    fm1_led_key(PANEL_BTN[B_PLAY], seq_run && (seq_pos[ui.sel] & 3u) == 0u);   /* (the beat, while it plays) */
    for (k = 0; k < NKEY; k++) {
        if (ui.lrec_down && white_ix(k) >= 0) {      /* (the loops: playing lit, muted dim) */
            int32_t w = white_ix(k);
            const loop_t *p = &loops[ui.sel][w < (int32_t)NLOOP ? w : 0];
            fm1_led_key(14u + k, w < (int32_t)NLOOP && p->on && !p->mute);
            led_dim_key(14u + k, w < (int32_t)NLOOP && p->on && p->mute);
            continue;
        }
        if (ui.page == PG_PAT && white_ix(k) >= 0) {   /* (the PATTERNS grid: every white key a pattern, dim; the playing
                                                        * one lit) */
            fm1_led_key(14u + k, white_ix(k) == (int32_t)pat_cur);
            led_dim_key(14u + k, 1);
            continue;
        }
        if (step_view() && k == KEY_GEN) {           /* (the phrase key: dim, lit as it makes one) */
            fm1_led_key(14u + k, (int32_t)(ui.gen_until - fm1_ms) > 0);
            led_dim_key(14u + k, 1);
            continue;
        }
        if (step_view() && sm_of(k)) {               /* (the step view's black keys: dim, the one chosen lit) */
            fm1_led_key(14u + k, ui.smode == sm_of(k));
            led_dim_key(14u + k, 1);
            continue;
        }
        if (step_view() && white_ix(k) >= 0) {       /* (the step view: with a note lit, empty dim, the one playing
                                                      * the other way round) */
            uint32_t i = (uint32_t)white_ix(k);
            int on = seq_note[ui.sel][i] != 0;
            if (seq_run && seq_pos[ui.sel] == i)
                on = !on;
            fm1_led_key(14u + k, on || k == ui.step_key);
            led_dim_key(14u + k, 1);
            continue;
        }
        led_dim_key(14u + k, 0);
        fm1_led_key(14u + k, key_plays(k) && ((ui.keys >> k) & 1u) && ui.key_track[k] == ui.sel);
    }
    fm1_led_key(14u + KEY_SHIFT, (ui.keys >> KEY_SHIFT) & 1u);
    fm1_led_key(14u + KEY_STEP, ui.sv);
    led_dim_key(14u + KEY_STEP, 1);
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
        {   /* top left: the pattern playing (P3; orange with changes not saved) */
            char pn[8] = "P";
            fmt_int(pn + 1, pat_cur + 1);
            cv_text(4, 2, &FONT_S, pn, pat_dirty ? RGB(255, 140, 0) : C_WHITE);
            if (ui.rec || ui.lrec) {                 /* (recording, right, under the octave: REC or LOOP in red;
                                                      * armed: ARM) */
                const char *r = ui.lrec == 2 ? "LOOP" : ui.rec == REC_ON ? "REC" : "ARM";
                cv_text(236 - text_w(&FONT_S, r), 6 + FONT_S.h, &FONT_S, r,
                        ui.lrec == 2 || ui.rec == REC_ON ? C_BAD : RGB(255, 140, 0));
            }
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

/* SHIFT held: the chosen track's 16 steps, four groups of four; with a note in the track's colour, empty a grey frame,
 * the one playing white, the one held framed white */
static void draw_steps(void)
{
    uint32_t i, t = ui.sel, pos = seq_run ? seq_pos[t] : 0xFFu, len = seq_len[t] ? seq_len[t] : NSTEP;
    uint32_t h = 0;
    int32_t key;
    for (i = 0; i < NSTEP; i++)
        h = h << 1 | (seq_note[t][i] != 0);
    key = (int32_t)((h << 14 | t << 11 | (pos & 31u) << 5 | (ui.step_key != KEY_NONE ? (uint32_t)white_ix(ui.step_key) + 1u : 0u)) & 0x3FFFFFFFu);
    if (!ui.force && key == ui.shown_steps)
        return;
    cv_begin(240, 240 - BAND_M, C_BLACK);
    for (i = 0; i < NSTEP; i++) {
        int32_t x = 4 + (int32_t)i * 14 + (int32_t)(i / 4u) * 4;
        int held = ui.step_key != KEY_NONE && (uint32_t)white_ix(ui.step_key) == i;
        uint16_t c = i >= len ? RGB(30, 30, 30) : i == pos ? C_WHITE : seq_note[t][i] ? TRACK_COL[t] : 0;
        if (c)
            cv_rect(x, 6, 12, 36, c);
        else
            cv_frame(x, 6, 12, 36, 2, C_GREY);
        if (held)
            cv_frame(x - 1, 2, 14, 44, 2, C_WHITE);
    }
    cv_blit(0, BAND_M);
    ui.shown_steps = key;
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

/* the step held, on the step view's TRIG knobs: NOTE, VEL, LEN, COND as an EDIT page's rows */
static void cond_text(char *b, uint32_t c)
{
    static const char *const CN[CD_PROB] = {"NONE", "PRE", "!PRE", "NEI", "!NEI", "1ST", "!1ST"};
    uint32_t a, n;
    if (c < CD_PROB)
        str_cpy(b, CN[c], 8);
    else if (c < CD_AB) {
        fmt_int(b, PROB[c - CD_PROB]);
        str_cpy(b + str_len(b), "%", 2);
    } else {
        cond_ab(c, &a, &n);
        str_cpy(b, (c - CD_AB) & 1u ? "!" : "", 2);
        fmt_int(b + str_len(b), (int32_t)a);
        str_cpy(b + str_len(b), ":", 2);
        fmt_int(b + str_len(b), (int32_t)n);
    }
}
static void draw_step_page(void)
{
    static const char *const NAME[4] = {"NOTE", "VEL", "LEN", "COND"};
    uint32_t t = ui.sel, i = (uint32_t)white_ix(ui.step_key), j;
    int32_t tc = (int32_t)(ui.touch_until - fm1_ms) > 0 ? ui.touched : -1;
    int full = ui.force || ui.shown_page != 20u || ui.shown_sub != i;
    if (full) {
        char t1[12] = "STEP ";
        fmt_int(t1 + 5, (int32_t)i + 1);
        uint8_t h = ui.half;
        ui.half = 0;                             /* (no halves here: the line under the title as A .. D's) */
        draw_rows_title(t1, "TRIG", TRACK_COL[t]);
        ui.half = h;
    }
    for (j = 0; j < 4u; j++) {
        int32_t v = j == 0u ? seq_note[t][i] : j == 1u ? seq_vel[t][i] : j == 2u ? seq_glen[t][i] : seq_cond[t][i];
        int32_t fr = j == 0u ? (v - 12) * 1000 / 108 : j == 1u ? v * 1000 / 127 : j == 2u ? v * 1000 / (NGLEN - 1)
                   : v * 1000 / (NCOND - 1);
        char b[16];
        if (!full && ui.shown_rows[j] == v && ((int32_t)j == tc) == ((int32_t)j == ui.shown_touch))
            continue;
        if (j == 0u)
            note_text(b, (uint32_t)v);
        else if (j == 1u)
            fmt_int(b, v);
        else if (j == 2u)
            str_cpy(b, GLEN_NAME[v], sizeof b);
        else
            cond_text(b, (uint32_t)v);
        draw_row(j, NAME[j], b, fr, 0, (int32_t)j == tc, TRACK_COL[t]);
        ui.shown_rows[j] = v;
    }
    ui.shown_touch = (int8_t)tc;
    ui.shown_sub = (uint8_t)i;
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
    if (step_page()) {                               /* a step held, the step view's knobs on it */
        draw_step_page();
        ui.shown_page = 20;
        ui.force = 0;
        return;
    }
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
    if (pg_any()) {
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
    if (ui.force || ui.shown_sel != ui.sel || ui.shown_prj != pat_cur * 32 + pat_dirty * 16 + ui.rec * 4 + ui.lrec) {
        ui.shown_prj = (int16_t)(pat_cur * 32 + pat_dirty * 16 + ui.rec * 4 + ui.lrec);
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
    if (step_view()) {                               /* the step view: the steps where the meter and the pan were */
        draw_steps();
        ui.force = 0;
        return;
    }
    if (ui.shown_steps >= 0) {
        ui.shown_steps = -1;
        ui.shown_level = ui.shown_pan = -100;
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
    ui.step_key = ui.qk = KEY_NONE;
    memset(ui.rec_step, 0xFF, sizeof ui.rec_step);
    memset(ui.lk_ev, 0xFF, sizeof ui.lk_ev);
    ui.lv_k = KEY_NONE;
    ui.click = 1;
    ui.shown_steps = -1;
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
