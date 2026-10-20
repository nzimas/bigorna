/* SPDX-License-Identifier: GPL-3.0-only
 * The panel and the screen (after TONITOR's ui.c: its panel map, its bands, its dial, its scale page).
 *
 *   FX SEL ENV LFO EDIT GLO   the top row: tracks 1 to 6 (lit: the chosen one; the others glow while they sound)
 *   PRESETS (knob)            the track's machine: 4-OP, DRUM, WAVY, HIVE (each track keeps a sound of
 *                             each, so turning back finds it) -- on the track's view only: on the EDIT pages it moves
 *                             through them, on the effects' pages the half shown
 *   HOME                      a new sound for the track, on its machine (gen.c); SHIFT + HOME: of the same family
 *   OCT- / OCT+               the track's octave, -3 .. +3; on the EDIT page, the page before / after
 *   the white keys            play the track in the scale chosen (the first key the root, each the next degree)
 *   the 1st black key         (the track's view, the step view off) CHORD mode on or off for the track (lit while
 *                             on, dim off): each white key then plays a triad from its note, in the scale (after
 *                             TONITOR's: TRIAD, INV 1, INV 2, SUS2, SUS4, OPEN, FOURTHS; in CHROMATIC as semitones);
 *                             SHIFT and it, the next form (and on). The form shows at the top left. A step put in
 *                             takes the chord last played; REC records chords into the steps, the looper their notes
 *   the 2nd black key         (the track's view, the step view off) MUTE mode on or off (lit while on): the tracks'
 *                             buttons, lit while playing and dim muted, mute or play a track as they are let go soon;
 *                             held 1 s, that track alone plays (solo; held 1 s again, the mutes as they were before
 *                             it). Off, the mutes stay (and the buttons choose
 *                             tracks again); a muted track's number is dark. Kept with the session
 *   the 3rd black key         (the step view off) BREAK while held (tracks.c): the chosen track's pattern broken up
 *                             -- other steps played, ratchets, flams, leaps, a gap -- its position kept; SHIFT and
 *                             it, every track. Let go, each plays on from where it is. The way shows in the band
 *   the rightmost black key   SHIFT
 *   the 3rd black key from the right   STEP: the step view, on or off (lit while on). The white keys are the track's
 *                             16 steps: a key on an empty step puts the track's last note played there; a key on a
 *                             step that holds a note, let go soon (0.4 s) and nothing changed, empties it. In the
 *                             view the black keys, lit dimly:
 *                               the 1st   a new phrase for the track (phrase.c): its steps, conditions and locks;
 *                                         SHIFT and it: new locks on the steps the track holds, nothing else changed
 *                                         Held 0.8 s: the PHRASE page, up or down: KNOB 1 the track's DENSITY, 2 its
 *                                         COMPLEXITY (0 .. 100: how full its phrases; how many locks, conditions,
 *                                         ratchets)
 *                               the 2nd   the TRACK page, up or down: KNOB 1 the track's LENGTH (1 .. 16 steps),
 *                                         KNOB 2 its CLOCK against the tempo's 16ths (1/8, 1/4, 1/2, 1X, 2X, 4X)
 *                               the 3rd   TRIG on or off: on, a step held shows its own page -- KNOB 1 its note (in
 *                                         the scale), 2 its velocity, 3 its length, 4 its trig condition; PRESETS
 *                                         right, its other half: KNOB 1 its RATCHET (the step played 2 .. 8 times)
 *                               the 4th   MOD on or off (TRIG off): on, a step held shows its MOD page -- KNOB 1
 *                                         MUTATE (its note moved -N .. +N steps of the scale, or RND: at random up to
 *                                         an octave either way), 2 MUT EVERY (every so many times the step plays), 3
 *                                         MIDI FX, 4 FX EVERY
 *                               the 6th .. 8th   the track's direction: REVERSE, PING-PONG, RANDOM (again: forward)
 *                             A step held, TRIG off: the knobs lock what they set on the track's view on it (FREQ,
 *                             RESO, the delay and reverb sends; SHIFT: chorus, overdrive), as a dial; SELECT locks its
 *                             level, ALGORITHM its pan. With the EDIT pages up, the knobs lock what they turn on the
 *                             step held -- any parameter of the sound, eight locks a step (the page shows the step's
 *                             values: locked in the track's colour, the sound's own grey)
 *                             SHIFT and a step held 0.8 s: the step copied (its note, velocity, length, condition,
 *                             ratchet, locks; COPY S5 shows, its key blinks); SHIFT and another held 0.8 s: pasted
 *                             there (this track or another); the same again, or the step view off: let go
 *   PLAY                      the sequencer, as it is let go: plays, or stops (and recording ends, a loop being
 *                             recorded is dropped). Held 0.8 s: the TEMPO page, up or down -- the session's tempo,
 *                             large; PRESETS sets it (a track's button, or another page, takes it down too)
 *   SHIFT + PLAY              the metronome off or on (it clicks while REC or the looper is armed or recording)
 *   REC                       recording into the chosen track's steps. Stopped: armed (REC flashes, the metronome
 *                             counts), the first note played starts the sequencer and is step 1. Playing: recording
 *                             at once (punch in). Each note played goes to the nearest step, its velocity and its
 *                             length (as held) with it, over what the step held; round and round until REC again
 *                             (it plays on)
 *   SHIFT + REC               the looper (after TONITOR's): a new loop on the chosen track, armed; the first note
 *                             played starts it (and the sequencer, if stopped); every note played after is in it, at
 *                             the nearest step, with its velocity and length, until REC ends it (at the nearest step):
 *                             it plays from there, round and round in its own length (up to 32 bars), over the
 *                             track's steps. Three loops a track
 *   SHIFT + REC held          the track's loops on the first three white keys (lit: playing; dim: muted): a key let
 *                             go soon mutes its loop or plays it again; held 0.8 s, empties it. Held 1.5 s with no
 *                             white key touched: every loop of the track emptied
 *   SHIFT + PRESETS           the track's sound: a new one of the family before / after, on the machine it plays
 *   the black key before it   EDIT: the parameter pages of the track's sound -- SYN 1 .. 4, FLTR 1, FLTR 2, AMP,
 *                             FX -- eight parameters a page: KNOB 1 .. 4 its A .. D, or (PRESETS turned right; left
 *                             back) its E .. H; OCT- / OCT+ the page before / after
 *   ARP                       the patterns (projects.c)
 *   SEQ                       the effects' pages: CHORUS, COMP (the master compressor), DELAY, REVERB in turn, then
 *                             back (OCT- / OCT+ too; a track's button: back to its view; SHIFT and it, the track
 *                             chosen, the page kept -- COMP's SC SOURCE and SC LEVEL are the chosen track's);
 *                             KNOB 1 .. 4 the first four of the page, or (PRESETS turned right; left back) the next
 *   SAVE                      the sessions (projects.c); held 0.8 s, the session saved -- never saved, the first
 *                             empty slot's page, SAVE NEW? (YES chosen)
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
#define KEY_TRK 3u                   /* (the 2nd: in the step view, the TRACK page) */
#define KEY_TRIG 5u                  /* (the 3rd: TRIG, the step held's own page) */
#define KEY_MOD 8u                   /* (the 4th: MOD, the step held's MUTATE and MIDI FX; the 5th free) */
static const uint8_t KEY_DIR[3] = {13, 15, 17};  /* (the 6th .. 8th: REVERSE, PING-PONG, RANDOM) */
static const char *const DIR_NAME[NDIR] = {"FORWARD", "REVERSE", "PING-PONG", "RANDOM"};
#define SHOWN_DIAL 30u               /* (ui.shown_page: the dial up; the step's own page) */
#define SHOWN_STEP 31u
#define KEY_NONE 0xFFu

static const uint16_t TRACK_COL[NTRACK] = {RGB(255, 214, 0), RGB(0, 214, 255), RGB(255, 92, 210),
                                           RGB(96, 255, 96), RGB(255, 140, 0), RGB(190, 160, 255)};
enum { PG_MAIN, PG_EDIT, PG_FXP, PG_TRK, PG_GEN, PG_TEMPO, PG_PRJ, PG_PRJD, PG_PAT };   /* (the sessions' list and page, the patterns' grid:
                                                              * projects.c) */

static struct {
    uint8_t sel;
    uint8_t page;
    uint8_t edit_pg;             /* the EDIT page shown (0 .. pages_n - 1) */
    uint8_t fx_pg;               /* the effect shown: FXP_* (CHORUS, COMP, DELAY, REVERB) */
    uint8_t half;                /* the half of the page shown: 0 A .. D, 1 E .. H (the pages': ehalf; else SHIFT held) */
    uint8_t ehalf;               /* the EDIT and effects pages' half, set by PRESETS */
    int8_t oct[NTRACK];
    uint8_t eng[NTRACK];
    sound_t snd[NTRACK][NENG];
    uint8_t dirty[NTRACK];       /* the sound changed: to be published */
    uint8_t key_track[NKEY], key_note[NKEY];
    uint8_t key_cn[NKEY][3];     /* a key down: the notes it sounds (its chord's three; a note: it alone) */
    uint8_t last_note[NTRACK];   /* the note last played on each track (0: none yet): what a step is given */
    uint8_t last_chf[NTRACK];    /* .. and its chord (form + 1; 0: a note alone) */
    uint8_t chord_on[NTRACK], chord_form[NTRACK];   /* each track's CHORD mode, and its form (CH_*) */
    uint8_t mute_mode;           /* MUTE mode (the 2nd black key): the tracks' buttons mute, held 1 s solo (build 35) */
    uint8_t brk, brk_shown;      /* BREAK (the 3rd black key held): the tracks under it; the way last said (build 37) */
    uint8_t tb_down, tb_long;    /* .. a track's button held (a bit each), held long enough: soloed */
    uint8_t solo;                /* .. the track soloed (0xFF none), and the mutes before it (a bit each: held again,
                                  * they come back) */
    uint8_t pre_solo;
    uint32_t tb_t0[NTRACK];
    uint8_t step_key;            /* SHIFT: the key of the step held (KEY_NONE none) */
    uint8_t step_new, step_edit;     /* .. its note put there as it was pressed; changed since */
    uint32_t step_t0;            /* .. pressed at */
    uint8_t sv;                  /* the step view on (STEP) */
    uint32_t gen_until;          /* the phrase key lit fully until */
    uint8_t gen_down, gen_long, gen_sh;   /* the phrase key held: since gen_t0; held 0.8 s (its page); SHIFT with it */
    uint32_t gen_t0;
    uint8_t gen_den[NTRACK], gen_cx[NTRACK];   /* the phrase generator's DENSITY and COMPLEXITY, 0 .. 100, a track's */
    uint8_t trig;                /* TRIG on (the 3rd black key): a step held shows its own page */
    uint8_t mod;                 /* MOD on (the 4th): a step held shows its MOD page */
    uint8_t play_down, play_long;   /* PLAY held: since play_t0; held 0.8 s (the TEMPO page) */
    uint32_t play_t0;
    uint8_t sk, sk_long;         /* SHIFT and a step key held (KEY_NONE none): since sk_t0; held 0.8 s */
    uint32_t sk_t0;
    struct {                     /* a step copied (SHIFT + a step held): from where, and all it held */
        uint8_t on, t, i, note, vel, glen, cond, rtg, lid[NPL], mcy, mfx, fcy, chf;
        int8_t mut;
        int16_t lv[NPL];
    } sclip;
    uint8_t step_heard;          /* .. the note it sounds as it is held (0 none) */
    uint8_t step_hn[3];          /* .. the notes (its chord's) */
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
    uint8_t lk_n[NKEY];          /* .. how many (a chord: three, one after the other) */
    uint32_t lk_t0[NKEY];        /* .. and when it was pressed */
    uint8_t rec_step[NKEY], rec_tr[NKEY];   /* a key held as it was recorded: its step (0xFF none) and track .. */
    uint32_t rec_t0[NKEY];       /* .. and when it was pressed */
    uint32_t qk_t0;
    uint8_t midi_track[128];
    uint32_t keys;
    char msg[16];
    uint32_t msg_until, home_until;
    uint32_t home_t0, arp_t0, save_t0;   /* HOME, ARP, SAVE pressed at (held 0.8 s: a save; let go sooner: a press) */
    uint8_t home_down, home_long, arp_down, arp_long, save_down, save_long;
    uint32_t seed;
    uint8_t scale, root;
    int16_t tempo;               /* the project's, BPM */
    int8_t level[NTRACK], pan[NTRACK];
    int8_t touched;              /* the row just turned (-1 none), until touch_until */
    uint32_t touch_until;
    uint8_t dial_pg, dial_j;     /* the dial: the parameter it shows (page id, index) */
    uint8_t dial_lk, dial_step;  /* .. or a step's lock (its id + 1, 0 none) and the step */
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
    uint8_t shown_step;          /* (the EDIT page: the step held it showed, 0xFF none) */
    int8_t shown_touch;
    uint8_t shown_page, shown_half, shown_sub;
    int32_t shown_prj;           /* (the pattern shown at the top left, its changes, the octave and chord) */
    uint32_t meter_at;
    uint8_t force;
} ui;

static const char *const RVN[NRV] = {"PRE", "DECAY", "FREQ", "GAIN", "HPF", "LPF", "VOLUME"};
static const char *const DLN[NDL] = {"TIME", "PING PONG", "WIDTH", "FEEDBACK", "HPF", "LPF", "REVERB", "VOLUME"};
static const char *const CHN[NCH] = {"DEPTH", "SPEED", "HPF", "WIDTH", "DELAY", "REVERB", "VOLUME"};
/* the COMP page's rows: the compressor's six, then the chosen track's sidechain (SC SOURCE, SC LEVEL: in its colour) */
static const char *const CPN[NCP] = {"THRESHOLD", "ATTACK", "RELEASE", "RATIO", "MAKEUP", "MIX", "SC SOURCE", "SC LEVEL"};
#define CMP_SRC 0xFEu
#define CMP_LVL 0xFFu
static const uint8_t CMP_SLOT[NCP] = {CP_THR, CP_ATK, CP_REL, CP_RAT, CP_MUP, CP_MIX, CMP_SRC, CMP_LVL};
static uint16_t row_tint;                        /* (draw_row: a row's name and value in this colour, 0 none) */
enum { FXP_CHR, FXP_CMP, FXP_DEL, FXP_REV, NFXP };   /* (the effects' pages, in turn) */
static const char *const FXT[NFXP] = {"CHORUS", "COMP", "DELAY", "REVERB"};
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
static void phrase_brk_roles(void);   /* (phrase.c: BREAK's kicks, drums and drones, from the tracks' sounds) */
static void pat_quick_save(void);
static void pat_switch(uint32_t k);
static void pat_copy_hold(uint32_t k);
static void pat_poll(void);
static uint8_t seq_pend_k;                       /* (projects.c: the pattern waiting to play) */
static void tempo_apply(void);
static int8_t pat_clip_from;                     /* (projects.c: the pattern copied, -1 none) */
static void pat_clear(uint32_t k);
static void phrase_new(uint32_t t);              /* (phrase.c) */
static void phrase_relock(uint32_t t);
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
static int step_view(void);
static int key_plays(uint32_t k)
{
    return k != KEY_SHIFT && k != KEY_EDIT && k != KEY_STEP &&
           ((k != KEY_GEN && k != KEY_TRK && k != KEY_TRIG) || step_view()) &&
           /* (out of the step view the 1st black key is CHORD mode's, the 2nd MUTE mode's, the 3rd BREAK) */
           (ui.scale == 0 || white_ix(k) >= 0);
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
static int step_view(void) { return ui.sv; }
static int step_held(void) { return ui.sv && ui.step_key != KEY_NONE; }
/* the step's own page (NOTE, VEL, LEN, COND), while a step is held and the EDIT pages are not up */
static int step_page(void)
{
    return step_held() && (ui.trig || ui.mod) && ui.page != PG_EDIT && ui.page != PG_TRK && ui.page != PG_GEN;
}

/* the steps' locks (the sequencer's, tracks.c): a lock is a parameter (page x 8 + knob; PL_PAN) and its value */
static int pl_find(uint32_t t, uint32_t i, uint32_t id)
{
    uint32_t j;
    for (j = 0; j < NPL; j++)
        if (seq_lid[t][i][j] == id)
            return (int)j;
    return -1;
}
static int pl_set(uint32_t t, uint32_t i, uint32_t id, int32_t v)   /* 0; -1 the step's eight places taken */
{
    int j = pl_find(t, i, id);
    if (j < 0)
        j = pl_find(t, i, PL_NONE);
    if (j < 0)
        return -1;
    seq_lv[t][i][j] = (int16_t)v;
    RING_PUBLISH();
    seq_lid[t][i][j] = (uint8_t)id;
    return 0;
}
static void pl_clear(uint32_t t, uint32_t i)
{
    uint32_t j;
    for (j = 0; j < NPL; j++)
        seq_lid[t][i][j] = PL_NONE;
}
/* a parameter's definition (the pan's, as a bipolar 0 .. 127), and the sound's own value of it */
static void pl_par(const sound_t *s, uint32_t id, par_t *p)
{
    static const par_t PAN = {"PAN", 0, 127, 64, PK_BIP, 0};
    if (id == PL_PAN)
        *p = PAN;
    else
        par_get(s, id >> 3, id & 7u, p);
}
static int32_t pl_base(const sound_t *s, uint32_t id) { return id == PL_PAN ? 64 : par_val(s, id >> 3, id & 7u); }
static int32_t pl_val(uint32_t t, uint32_t i, uint32_t id)   /* step i's value of a parameter: locked, or the sound's */
{
    int j = pl_find(t, i, id);
    return j >= 0 ? seq_lv[t][i][j] : pl_base(&ui.snd[t][ui.eng[t]], id);
}

/* the phrase generator's targets (phrase.c), as the parameters they lock */
enum { LK_FREQ, LK_RESO, LK_FENV, LK_ADEC, LK_VOL, LK_OD, LK_BR, LK_SRR, LK_DEL, LK_REV, LK_CHR, LK_PAN, LK_DST, LK_SLICE,
       NLK };
static const uint8_t LK_PID[NLK] = {PG_FLT1 * 8 + 4, PG_FLT1 * 8 + 5, PG_FLT1 * 8 + 7, PG_AMP * 8 + 2, PG_AMP * 8 + 7,
                                    PG_FX * 8 + 1, PG_FX * 8 + 0, PG_FX * 8 + 2, PL_DEL, PL_REV, PL_CHR, PL_PAN,
                                    PG_MNG * 8 + M_DST, PG_MNG * 8 + M_SLICE};
static int32_t lk_base(const sound_t *s, uint32_t lk) { return pl_base(s, LK_PID[lk]); }
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
    v = par_step(&p, par_val(snd, id, j), s);
    par_put(snd, id, j, v);
    if (id == PG_FLT2 && j == 6u)                    /* (another FLTR machine: its knob G within its range) */
        snd->flt[F_TYPE] = (uint8_t)(snd->fmach == FLT_LEG ? snd->flt[F_TYPE] != 0 : snd->flt[F_TYPE]);
    mark(ui.sel);
}
static uint8_t *fx_vals(void)
{
    return ui.fx_pg == FXP_REV ? fx_rv : ui.fx_pg == FXP_DEL ? fx_dl : ui.fx_pg == FXP_CMP ? fx_cp : fx_ch;
}
static int32_t fx_get(uint32_t k)                /* an effect's parameter k (COMP: its row, CMP_SLOT) */
{
    if (ui.fx_pg == FXP_CMP)
        return CMP_SLOT[k] == CMP_SRC ? sc_src[ui.sel] : CMP_SLOT[k] == CMP_LVL ? sc_lvl[ui.sel] : fx_cp[CMP_SLOT[k]];
    return fx_vals()[k];
}
static void fx_put(uint32_t k, int32_t v)
{
    if (ui.fx_pg == FXP_CMP && CMP_SLOT[k] == CMP_SRC)
        sc_src[ui.sel] = (uint8_t)v;
    else if (ui.fx_pg == FXP_CMP && CMP_SLOT[k] == CMP_LVL)
        sc_lvl[ui.sel] = (uint8_t)v;
    else if (ui.fx_pg == FXP_CMP)
        fx_cp[CMP_SLOT[k]] = (uint8_t)v;
    else
        fx_vals()[k] = (uint8_t)v;
}
static uint32_t fx_n(void) { return ui.fx_pg == FXP_REV ? NRV : ui.fx_pg == FXP_DEL ? NDL : ui.fx_pg == FXP_CMP ? NCP : NCH; }
/* an effect's parameter: 0 shown as it is (DELAY's TIME, PING PONG), 1 as 0 .. 100, 2 as -50 .. +50 (DELAY's WIDTH),
 * 3 a list (COMP's RATIO, SC SOURCE) */
static uint32_t fx_kind(uint32_t j)
{
    if (ui.fx_pg == FXP_DEL && (j == DL_TIME || j == DL_X))
        return 0;
    if (ui.fx_pg == FXP_CMP && (CMP_SLOT[j] == CP_RAT || CMP_SLOT[j] == CMP_SRC))
        return 3u;
    return ui.fx_pg == FXP_DEL && j == DL_WID ? 2u : 1u;
}
static uint32_t fx_list_n(uint32_t j) { return CMP_SLOT[j] == CP_RAT ? NCP_RAT : NCP_SCS; }
static void fx_turn(uint32_t j, int32_t s)
{
    int32_t v;
    uint32_t kd;
    if (j >= fx_n())
        return;
    kd = fx_kind(j);
    v = fx_get(j);
    fx_put(j, kd == 1u ? pct_raw(pct_disp(v) + s) : kd == 2u ? bip_raw(bip_disp(v) + s)
              : kd == 3u ? clampi(v + s, 0, (int32_t)fx_list_n(j) - 1)
              : clampi(v + s, 0, ui.fx_pg == FXP_DEL && j == DL_X ? 1 : 127));
    fx_cook_all();
    prj_dirty = 1;
}
static void dial(uint32_t id, uint32_t j)
{
    ui.dial_pg = (uint8_t)id;
    ui.dial_j = (uint8_t)j;
    ui.dial_lk = 0;
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
    uint32_t e = ((ms > 900000u ? 900000u : ms) * (uint32_t)fx_bpm10 + 9375u) / 18750u;   /* (x 8 / 150000; < 2^32) */
    return e ? (e < 4096u ? e : 4096u) : 1u;     /* (rounded; up to the loop's whole 32 bars) */
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
static void loop_note(const uint8_t *nn, uint32_t cnt, uint32_t vel, uint32_t k)   /* (a chord: its three) */
{
    loop_t *p = &loops[ui.lt][ui.ll];
    uint32_t q, j;
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
    if (q - ui.lstart >= LOOP_MAX || p->n + cnt > LOOP_EV)
        return;
    if (k < NKEY) {
        ui.lk_ev[k] = p->n;
        ui.lk_n[k] = (uint8_t)cnt;
        ui.lk_t0[k] = fm1_ms;
    }
    for (j = 0; j < cnt; j++) {
        p->ev[p->n].step = (uint16_t)(q - ui.lstart);
        p->ev[p->n].len8 = 4;
        p->ev[p->n].note = nn[j];
        p->ev[p->n].vel = (uint8_t)clampi((int32_t)vel, 1, 127);
        p->n++;
    }
}
static void loop_release(uint32_t k)             /* a key the looper recorded let go: its notes' length */
{
    uint32_t j;
    for (j = 0; ui.lrec == 2 && j < ui.lk_n[k] && ui.lk_ev[k] + j < loops[ui.lt][ui.ll].n; j++)
        loops[ui.lt][ui.ll].ev[ui.lk_ev[k] + j].len8 = (uint16_t)loop_len8(fm1_ms - ui.lk_t0[k]);
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
static void rec_note(uint32_t note, uint32_t vel, uint32_t k, uint32_t f1)   /* (f1: a chord's form + 1, or 0) */
{
    uint32_t t = ui.sel, i;
    if (ui.rec == REC_ARMED) {                   /* the first note: the sequencer starts on it, step 1 */
        seq_skip[t][0] = 1;
        seq_run = 1;
        rec_set(REC_ON);
        i = 0;
    } else if (ui.rec != REC_ON || !seq_run)
        return;
    else {
        uint32_t pos = seq_pos[t], acc = seq_tacc_now[t], len = seq_len[t] ? seq_len[t] : NSTEP;
        if (pos >= len) {                        /* (just started: step 1 to come) */
            i = 0;
            seq_skip[t][0] = 1;
        } else if (acc < SEQ_STEP8 / 2u)         /* (the track's own steps, on its own clock) */
            i = pos;
        else {
            i = (pos + 1u) % len;
            seq_skip[t][i] = 1;
        }
    }
    seq_vel[t][i] = (uint8_t)clampi((int32_t)vel, 1, 127);
    seq_glen[t][i] = GLEN_DEF;
    seq_cond[t][i] = CD_NONE;
    seq_rtg[t][i] = 0;
    seq_mut[t][i] = 0;
    seq_mcy[t][i] = seq_mfx[t][i] = seq_fcy[t][i] = 0;
    pl_clear(t, i);
    seq_chf[t][i] = (uint8_t)f1;
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
#if TELEMETRY
    if (tl_ready) {                              /* (a diagnosis: a recipe on a track, or its level) */
        uint32_t t = tl_cmd[1] % NTRACK;
        if (tl_cmd[0] == 0x56u && tl_cmd[2] < NENG) {
            gen_sound(&ui.snd[t][tl_cmd[2]], tl_cmd[2], tl_cmd[3], -1, 7u + t);
            ui.eng[t] = tl_cmd[2];
        } else if (tl_cmd[0] == 0x57u) {
            ui.level[t] = tl_cmd[2];
            level_pan_apply(t);
        } else if (tl_cmd[0] == 0x58u)
            tl_c1_crash = 1;                     /* (a test: CPU1's next job writes to 0, a fault: CPU0 goes on alone) */
        mark(t);
        ui.force = 1;
        RING_PUBLISH();
        tl_ready = 0;
    }
#endif
    while (mi_r != mi_w) {
        uint32_t pkt = midi_in_q[mi_r % MQ], cin = pkt & 15u, note = (pkt >> 16) & 0x7Fu, vel = pkt >> 24 & 0x7Fu;
#if TELEMETRY
        uint32_t tt = (pkt >> 8 & 15u) < NTRACK ? (pkt >> 8 & 15u) : ui.sel;   /* (channel 1 .. 6: track 1 .. 6) */
#else
        uint32_t tt = ui.sel;
#endif
        RING_PUBLISH();
        mi_r++;
        if (cin == 9u && vel) {
            if (ui.midi_track[note] != 0xFFu)
                ev_post(EV_OFF, ui.midi_track[note], note, 0);
            ui.midi_track[note] = (uint8_t)tt;
            ui.last_note[tt] = (uint8_t)note;
            ui.last_chf[tt] = 0;
            ev_post(EV_ON, tt, note, vel);
            rec_note(note, vel, NKEY, 0);
            {
                uint8_t n8 = (uint8_t)note;
                loop_note(&n8, 1, vel, NKEY);
            }
        } else if ((cin == 8u || cin == 9u) && ui.midi_track[note] != 0xFFu) {
            ev_post(EV_OFF, ui.midi_track[note], note, 0);
            ui.midi_track[note] = 0xFFu;
        }
    }
}

/* a key let go: the notes it sounds (a chord: its three) */
static void key_off(uint32_t k)
{
    uint32_t j;
    for (j = 0; j < 3u; j++)
        if (ui.key_cn[k][j])
            ev_post(EV_OFF, ui.key_track[k], ui.key_cn[k][j], 0);
    ui.key_cn[k][0] = ui.key_cn[k][1] = ui.key_cn[k][2] = 0;
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
static void step_hear(uint32_t n)                /* (the step held: its note, or its chord) */
{
    uint32_t j;
    if (ui.step_heard)
        for (j = 0; j < 3u; j++)
            if (ui.step_hn[j])
                ev_post(EV_OFF, ui.sel, ui.step_hn[j], 0);
    ui.step_heard = (uint8_t)n;
    ui.step_hn[0] = ui.step_hn[1] = ui.step_hn[2] = 0;
    if (n) {
        uint32_t i = (uint32_t)white_ix(ui.step_key), f1 = ui.eng[ui.sel] == ENG_FMD ? 0u : seq_chf[ui.sel][i];
        int32_t c[3] = {(int32_t)n, 0, 0};
        if (f1)
            chord_of((int32_t)n, f1 - 1u, c);
        for (j = 0; j < (f1 ? 3u : 1u); j++) {
            ui.step_hn[j] = (uint8_t)c[j];
            ev_post(EV_ON, ui.sel, (uint32_t)c[j], seq_vel[ui.sel][i]);
        }
    }
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
        seq_rtg[ui.sel][i] = 0;
        seq_mut[ui.sel][i] = 0;
        seq_mcy[ui.sel][i] = seq_mfx[ui.sel][i] = seq_fcy[ui.sel][i] = 0;
        seq_chf[ui.sel][i] = ui.last_note[ui.sel] ? ui.last_chf[ui.sel] : 0u;   /* (the chord last played, too) */
        seq_note[ui.sel][i] = (uint8_t)n;
        pat_dirty = 1;
    }
    step_say(i, n);
    ui.force = 1;
    step_hear(n);
}
/* a parameter of the step held turned: locked (from the sound's value, if it had none) */
static void pl_turn(uint32_t id, int32_t s, int show)
{
    uint32_t t = ui.sel, i = (uint32_t)white_ix(ui.step_key);
    par_t p;
    pl_par(cur(), id, &p);
    if (p.kind == PK_NONE)
        return;
    if (pl_set(t, i, id, par_step(&p, pl_val(t, i, id), s))) {
        say("8 LOCKS", 900);                     /* (the step's places all taken) */
        return;
    }
    ui.step_edit = 1;
    pat_dirty = 1;
    if (show) {                                  /* (as a dial) */
        ui.dial_lk = (uint8_t)(id + 1u);
        ui.dial_step = (uint8_t)i;
        ui.dial_until = fm1_ms + 1000u;
    }
}
/* the knobs on the step held (EDIT not up): the note, the velocity, the length, the condition */
static void step_knob(uint32_t j, int32_t s)
{
    uint32_t t = ui.sel, i = (uint32_t)white_ix(ui.step_key);
    if (ui.mod) {                                /* MOD: MUTATE (-N .. -1, OFF, +1 .. +N, RND), its EVERY, MIDI FX, its
                                                  * EVERY */
        if (j == 0u) {
            int32_t n = SCALE_N[ui.scale], v = seq_mut[t][i], x = v == MUT_RND ? 2 * n + 1 : clampi(v, -n, n) + n;
            x = clampi(x + s, 0, 2 * n + 1);
            seq_mut[t][i] = (int8_t)(x == 2 * n + 1 ? MUT_RND : x - n);
        } else if (j == 1u)
            seq_mcy[t][i] = (uint8_t)clampi((int32_t)seq_mcy[t][i] + s, 0, 15);
        else if (j == 2u)
            seq_mfx[t][i] = (uint8_t)clampi((int32_t)seq_mfx[t][i] + s, 0, NMFX - 1);
        else if (j == 3u)
            seq_fcy[t][i] = (uint8_t)clampi((int32_t)seq_fcy[t][i] + s, 0, 15);
        else
            return;
    } else if (j >= 4u) {                               /* (the other half: E the ratchet) */
        if (j == 4u)
            seq_rtg[t][i] = (uint8_t)clampi((int32_t)seq_rtg[t][i] + s, 0, NRTG - 1);
        else
            return;
    } else if (j == 0u) {
        seq_note[t][i] = (uint8_t)scale_move((int32_t)seq_note[t][i], s);
        step_hear(seq_note[t][i]);
    } else if (j == 1u)
        seq_vel[t][i] = (uint8_t)clampi(pct_raw(pct_disp(seq_vel[t][i]) + s), 1, 127);   /* (shown 1 .. 100) */
    else if (j == 2u)
        seq_glen[t][i] = (uint8_t)clampi(seq_glen[t][i] + s, 0, NGLEN - 1);
    else
        seq_cond[t][i] = (uint8_t)clampi(seq_cond[t][i] + s, 0, NCOND - 1);
    ui.step_edit = 1;
    ui.touched = (int8_t)(j & 3u);
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

/* SHIFT and step i held: copied (the same again: let go); a step copied, pasted into i, on the chosen track */
static void step_copy_hold(uint32_t i)
{
    uint32_t t = ui.sel, j;
    char m[12] = "STEP ";
    fmt_int(m + 5, (int32_t)i + 1);
    if (ui.sclip.on && ui.sclip.t == t && ui.sclip.i == i) {
        ui.sclip.on = 0;
        say("COPY OFF", 900);
        ui.force = 1;
        return;
    }
    if (ui.sclip.on) {                           /* pasted */
        seq_note[t][i] = 0;                      /* (silent while it changes) */
        seq_vel[t][i] = ui.sclip.vel;
        seq_glen[t][i] = ui.sclip.glen;
        seq_cond[t][i] = ui.sclip.cond;
        seq_rtg[t][i] = ui.sclip.rtg;
        seq_mut[t][i] = ui.sclip.mut;
        seq_mcy[t][i] = ui.sclip.mcy;
        seq_mfx[t][i] = ui.sclip.mfx;
        seq_fcy[t][i] = ui.sclip.fcy;
        seq_chf[t][i] = ui.sclip.chf;
        pl_clear(t, i);
        for (j = 0; j < NPL; j++)
            if (ui.sclip.lid[j] != PL_NONE)
                (void)pl_set(t, i, ui.sclip.lid[j], ui.sclip.lv[j]);
        seq_note[t][i] = ui.sclip.note;
        ui.sclip.on = 0;
        pat_dirty = 1;
        flash("PASTED", m, C_OK);
        return;
    }
    if (!seq_note[t][i]) {
        say("EMPTY", 900);
        return;
    }
    ui.sclip.t = (uint8_t)t;                     /* copied */
    ui.sclip.i = (uint8_t)i;
    ui.sclip.note = seq_note[t][i];
    ui.sclip.vel = seq_vel[t][i];
    ui.sclip.glen = seq_glen[t][i];
    ui.sclip.cond = seq_cond[t][i];
    ui.sclip.rtg = seq_rtg[t][i];
    ui.sclip.mut = seq_mut[t][i];
    ui.sclip.mcy = seq_mcy[t][i];
    ui.sclip.mfx = seq_mfx[t][i];
    ui.sclip.fcy = seq_fcy[t][i];
    ui.sclip.chf = seq_chf[t][i];
    for (j = 0; j < NPL; j++) {
        ui.sclip.lid[j] = seq_lid[t][i][j];
        ui.sclip.lv[j] = seq_lv[t][i][j];
    }
    ui.sclip.on = 1;
    ui.force = 1;
}

static void ui_input(void)
{
    uint32_t rel, press = fm1_input_edges(&rel), keys, ch, k, t, j;
    int32_t s, e_pre, e_sel, e_alg;
    int sh = (int)((fm1_in.notes >> KEY_SHIFT) & 1u);
    (void)rel;
    pat_poll();                                      /* (a pattern switched as the sequencer took it in) */
    {   /* CPU1 retired as it plays (a fault, a stall, a misread): said, once */
        static uint8_t was_on;
        if (was_on && !audio_worker_online)
            say("1 CORE", 4000);
        was_on = (uint8_t)audio_worker_online;
    }
    {   /* the scale, for the sequencer's MUTATE and MIDI FX */
        uint32_t m = 0, q;
        for (q = 0; q < SCALE_N[ui.scale]; q++)
            m |= 1u << SCALE_STEP[ui.scale][q];
        seq_smask = (uint16_t)m;
        seq_sroot = ui.root;
    }
    if (ui.mute_mode && (ui.page != PG_MAIN || step_view()))
        ui.mute_mode = 0;                            /* (MUTE mode: the track's view only) */
    for (t = 0; ui.mute_mode && t < NTRACK; t++) {   /* MUTE mode: a track's button let go soon, muted or not; held
                                                      * 1 s, soloed (every other muted) */
        uint32_t b = 1u << t;
        if (btn_bit(press, TRACK_BTN[t])) {
            ui.tb_down |= (uint8_t)b;
            ui.tb_long &= (uint8_t)~b;
            ui.tb_t0[t] = fm1_ms;
        } else if ((ui.tb_down & b) && !btn_bit(fm1_in.buttons, TRACK_BTN[t])) {
            char m[10] = "MUTE  ";
            ui.tb_down &= (uint8_t)~b;
            if (!(ui.tb_long & b)) {
                ui.solo = 0xFF;                  /* (a mute changed by hand: no solo to undo) */
                trk_mute[t] = (uint8_t)!trk_mute[t];
                if (!trk_mute[t])
                    str_cpy(m, "PLAY  ", 7);
                m[5] = (char)('1' + t);
                say(m, 900);
                prj_dirty = 1;
                ui.force = 1;
            }
        } else if ((ui.tb_down & b) && !(ui.tb_long & b) && (uint32_t)(fm1_ms - ui.tb_t0[t]) >= 1000u) {
            char m[10] = "SOLO  ";
            uint32_t j;
            ui.tb_long |= (uint8_t)b;
            if (ui.solo == t) {                  /* (the soloed track held again: the mutes as they were before) */
                for (j = 0; j < NTRACK; j++)
                    trk_mute[j] = (uint8_t)((ui.pre_solo >> j) & 1u);
                ui.solo = 0xFF;
                say("SOLO OFF", 1200);
            } else {
                if (ui.solo == 0xFFu)            /* (another soloed after one: the mutes before the first kept) */
                    for (j = 0, ui.pre_solo = 0; j < NTRACK; j++)
                        ui.pre_solo |= (uint8_t)((trk_mute[j] != 0) << j);
                ui.solo = (uint8_t)t;
                for (j = 0; j < NTRACK; j++)
                    trk_mute[j] = (uint8_t)(j != t);
                m[5] = (char)('1' + t);
                say(m, 1200);
            }
            prj_dirty = 1;
            ui.force = 1;
        }
    }
    for (t = 0; !ui.mute_mode && t < NTRACK; t++)
        if (btn_bit(press, TRACK_BTN[t])) {
            ui.sel = (uint8_t)t;
            if (ui.page == PG_TEMPO || pg_any() || (ui.page == PG_FXP && !sh))   /* (the effects' pages: back to the
                                                      * track; SHIFT: they stay, COMP's sidechain the chosen track's) */
                ui.page = PG_MAIN;
            if (ui.page == PG_EDIT && ui.edit_pg >= pages_n(cur()))
                ui.edit_pg = 0;
            ui.force = 1;
        }
    if (btn_bit(press, B_SEQ)) {                     /* CHORUS, COMP, DELAY, REVERB, back */
        if (ui.page != PG_FXP) {
            ui.page = PG_FXP;
            ui.fx_pg = 0;
        } else if (++ui.fx_pg >= NFXP)
            ui.page = PG_MAIN;
        ui.ehalf = 0;
        ui.force = 1;
    }
    /* SAVE: let go soon, the projects (projects.c); held 0.8 s, the session saved (none: the first empty slot, asked) */
    if (btn_bit(press, B_SAVE)) {
        ui.save_down = 1;
        ui.save_long = 0;
        ui.save_t0 = fm1_ms;
    }
    if (ui.save_down) {
        if (!btn_bit(fm1_in.buttons, B_SAVE)) {
            ui.save_down = 0;
            if (ui.save_long)
                ;
            else if (ui.page == PG_PRJ || ui.page == PG_PRJD)
                prj_press(sh);
            else if (!sh) {
                ui.page = PG_PRJ;
                ui.force = 1;
            }
        } else if (!ui.save_long && (uint32_t)(fm1_ms - ui.save_t0) >= 800u) {
            ui.save_long = 1;
            prj_quick_save();
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
    ui.half = ui.page == PG_EDIT || ui.page == PG_FXP || step_page() ? ui.ehalf : (uint8_t)sh;   /* (the pages' half:
                                                                                              * PRESETS's) */
    if (step_page()) {                               /* a step held in the step view, TRIG on: the knobs its */
        for (j = 0; j < 4u; j++)
            if ((s = enc_take(KNOB[j])) != 0)
                step_knob(j + 4u * ui.half, s);
    } else if (step_held() && ui.page != PG_EDIT && ui.page != PG_TRK && ui.page != PG_GEN) {   /* .. TRIG off: the
                                                                                   * knobs' own, locked on it */
        static const uint8_t LKK[2][4] = {{PG_FLT1 * 8 + 4, PG_FLT1 * 8 + 5, PL_DEL, PL_REV},
                                          {PG_FLT1 * 8 + 4, PG_FLT1 * 8 + 5, PL_CHR, PG_FX * 8 + 1}};
        for (j = 0; j < 4u; j++)
            if ((s = enc_take(KNOB[j])) != 0)
                pl_turn(LKK[sh][j], s, 1);
    } else if (step_held() && ui.page == PG_EDIT) {  /* .. with the EDIT pages up: what they turn, locked on it */
        uint32_t id = page_id(cur(), ui.edit_pg);
        for (j = 0; j < 4u; j++)
            if ((s = enc_take(KNOB[j])) != 0) {
                pl_turn(id * 8u + j + 4u * ui.half, s, 0);
                ui.touched = (int8_t)j;
                ui.touch_until = fm1_ms + 1500u;
            }
    } else if (ui.page == PG_EDIT) {                        /* the page's parameters */
        uint32_t id = page_id(cur(), ui.edit_pg);
        for (j = 0; j < 4u; j++)
            if ((s = enc_take(KNOB[j])) != 0) {
                par_turn(id, j + 4u * ui.half, s);
                ui.touched = (int8_t)j;
                ui.touch_until = fm1_ms + 1500u;
            }
    } else if (ui.page == PG_GEN) {                  /* PHRASE: KNOB 1 DENSITY, 2 COMPLEXITY */
        for (j = 0; j < 4u; j++)
            if ((s = enc_take(KNOB[j])) != 0 && j < 2u) {
                uint8_t *v = j ? &ui.gen_cx[ui.sel] : &ui.gen_den[ui.sel];
                *v = (uint8_t)clampi(*v + s, 0, 100);
                pat_dirty = 1;
                ui.touched = (int8_t)j;
                ui.touch_until = fm1_ms + 1500u;
            }
    } else if (ui.page == PG_TRK) {                  /* TRACK: KNOB 1 the length, 2 the clock */
        for (j = 0; j < 4u; j++)
            if ((s = enc_take(KNOB[j])) != 0 && j < 2u) {
                uint32_t t = ui.sel;
                if (j == 0u)
                    seq_len[t] = (uint8_t)clampi((seq_len[t] ? seq_len[t] : (int32_t)NSTEP) + s, 1, NSTEP);
                else
                    seq_div[t] = (uint8_t)clampi((int32_t)seq_div[t] + s, 0, NDIV - 1);
                pat_dirty = 1;
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
    } else if (btn_bit(press, B_PLAY)) {             /* PLAY: as it is let go (held: the TEMPO page) */
        ui.play_down = 1;
        ui.play_long = 0;
        ui.play_t0 = fm1_ms;
    }
    if (ui.play_down) {
        if (!btn_bit(fm1_in.buttons, B_PLAY)) {
            ui.play_down = 0;
            if (!ui.play_long) {                     /* the sequencer (stopped: recording ends, a loop being recorded
                                                      * dropped; armed: it plays, recording) */
                seq_run = (uint8_t)!seq_run;
                if (!seq_run && ui.lrec)
                    loop_drop();
                if (!seq_run && ui.rec)
                    rec_set(REC_OFF);
                else if (seq_run && ui.rec == REC_ARMED)
                    rec_set(REC_ON);
                say(seq_run ? "PLAY" : "STOP", 700);
            }
        } else if (!ui.play_long && (uint32_t)(fm1_ms - ui.play_t0) >= 800u) {
            ui.play_long = 1;
            ui.page = ui.page == PG_TEMPO ? PG_MAIN : PG_TEMPO;
            ui.force = 1;
        }
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
    if (ui.lrec == 2 && loop_qnow() - ui.lstart >= LOOP_MAX)   /* (32 bars: the loop ends by itself) */
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
    if (step_view() && ui.step_key != KEY_NONE) {   /* a step held: SELECT its level, ALGORITHM its pan, locked */
        if (e_sel)
            pl_turn(PG_AMP * 8u + 7u, e_sel, 1);   /* (VOLUME) */
        if (e_alg)
            pl_turn(PL_PAN, e_alg, 1);
        e_sel = e_alg = 0;
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
        if (ui.page == PG_EDIT) {                    /* the page before / after (its A .. D) */
            uint32_t n = pages_n(cur());
            ui.edit_pg = (uint8_t)((ui.edit_pg + n + (uint32_t)(d + (int)n)) % n);
            ui.ehalf = 0;
            ui.force = 1;
        } else if (ui.page == PG_FXP) {              /* CHORUS, COMP, DELAY, REVERB */
            ui.fx_pg = (uint8_t)((ui.fx_pg + NFXP + (uint32_t)(d + (int)NFXP)) % NFXP);
            ui.ehalf = 0;
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
    if ((s = e_pre) != 0 && step_page() && ui.mod) {   /* (MOD: one page, no halves) */
        ;
    } else if ((s = e_pre) != 0 && ui.page == PG_TEMPO) {   /* TEMPO: PRESETS the session's tempo */
        ui.tempo = (int16_t)clampi(ui.tempo + s, 10, 300);
        tempo_apply();
        prj_dirty = 1;
        ui.force = 1;
    } else if ((s = e_pre) != 0 && (ui.page == PG_EDIT || ui.page == PG_FXP || step_page())) {   /* EDIT, the effects: PRESETS the page's
                                                                          * half, right E .. H, left A .. D (never the
                                                                          * machine) */
        ui.ehalf = (uint8_t)(s > 0);
        ui.force = 1;
    } else if (s && ui.page != PG_MAIN) {
        ;                                            /* (elsewhere PRESETS does nothing: only the track's view picks the
                                                      * machine or the family) */
    } else if (s && sh) {                            /* SHIFT + PRESETS: the family before / after, a new sound */
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
    } else if (s) {                                  /* the machine */
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
    /* BREAK: the 3rd black key held (the step view off), the chosen track broken up (tracks.c); SHIFT and it, every
     * track. Let go, each plays on from where it is */
    if (!ui.brk && ((ch & keys) >> KEY_TRIG & 1u) && !step_view() && ui.page != PG_PAT && !ui.lrec_down) {
        phrase_brk_roles();
        brk_seed = next_seed() | 1u;
        ui.brk = (uint8_t)(sh ? (1u << NTRACK) - 1u : 1u << ui.sel);
        ui.brk_shown = 0xFF;
        brk_mask = ui.brk;
        if (!seq_run)
            say("NOT PLAYING", 900);
        else if (brk_keep >> ui.sel & 1u && !sh)
            say("DRONE: KEPT", 900);
        ui.force = 1;
    } else if (ui.brk && (!((keys >> KEY_TRIG) & 1u) || step_view())) {
        ui.brk = 0;
        brk_mask = 0;
        if (ui.brk_shown != 0xFFu)
            ui.msg_until = fm1_ms;               /* (the way's name: gone with it) */
        ui.force = 1;
    }
    if (ui.brk && brk_shown != 0xFFu && (uint8_t)(brk_shown * 16u + brk_shown_w) != ui.brk_shown) {
        char m[16];                              /* the way playing, for as long as it plays */
        uint32_t r = brk_shown;
        ui.brk_shown = (uint8_t)(r * 16u + brk_shown_w);
        str_cpy(m, BR_NAME[r % NBR], sizeof m);
        if (r == BR_REPEAT || r == BR_REVERSE) {
            str_cpy(m + str_len(m), " ", 2);
            fmt_int(m + str_len(m), brk_shown_w);
        }
        say(m, 60000);
    }
    if ((ch & keys) >> KEY_EDIT & 1u) {              /* EDIT: the parameter pages, up or down */
        ui.page = ui.page == PG_EDIT ? PG_MAIN : PG_EDIT;
        ui.ehalf = 0;
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
                                                      * loaded; held 0.8 s, copied -- or, a pattern copied, pasted there
                                                      * (projects.c); SHIFT and held, emptied */
        for (k = 0; k < NKEY; k++) {
            if (((ch & ~keys) >> k & 1u) && ui.key_track[k] != KEY_NONE) {   /* (a note held from before: let go) */
                key_off(k);
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
                if (sh)
                    pat_clear((uint32_t)white_ix(ui.qk));
                else
                    pat_copy_hold((uint32_t)white_ix(ui.qk));
            }
        }
        ui.keys = keys;
        midi_take();
        return;
    }
    ui.qk = KEY_NONE;
    pat_clip_from = -1;                              /* (off the grid: a pattern copied let go) */
    if (ui.sk != KEY_NONE) {                         /* SHIFT and a step held 0.8 s: copied, or pasted */
        if (!((keys >> ui.sk) & 1u))
            ui.sk = KEY_NONE;
        else if (!ui.sk_long && (uint32_t)(fm1_ms - ui.sk_t0) >= 800u) {
            ui.sk_long = 1;
            step_copy_hold((uint32_t)white_ix(ui.sk));
        }
    }
    if (ui.gen_down) {                               /* the phrase key: let go soon, a new phrase (SHIFT: new locks);
                                                      * held 0.8 s, the PHRASE page up or down */
        if (!((keys >> KEY_GEN) & 1u)) {
            ui.gen_down = 0;
            if (!ui.gen_long && step_view()) {
                if (ui.gen_sh)
                    phrase_relock(ui.sel);
                else
                    phrase_new(ui.sel);
                ui.gen_until = fm1_ms + 400u;
                flash("NEW", ui.gen_sh ? "LOCKS" : "PHRASE", TRACK_COL[ui.sel]);
            }
        } else if (!ui.gen_long && (uint32_t)(fm1_ms - ui.gen_t0) >= 800u) {
            ui.gen_long = 1;
            ui.page = ui.page == PG_GEN ? PG_MAIN : PG_GEN;
            ui.force = 1;
        }
    }
    if ((ch & keys) >> KEY_STEP & 1u) {              /* STEP: the step view on or off */
        ui.sv = (uint8_t)!ui.sv;
        if (!ui.sv && ui.step_key != KEY_NONE)
            step_up();
        if (!ui.sv)
            ui.sclip.on = 0;
        if (!ui.sv && (ui.page == PG_TRK || ui.page == PG_GEN))
            ui.page = PG_MAIN;
        ui.force = 1;
    }
    for (k = 0; ch && k < NKEY; k++) {
        if (((ch & keys) >> k & 1u) && !step_view() && k == KEY_GEN && ui.page == PG_MAIN) {   /* CHORD mode on or
                                                                         * off; SHIFT: the next form (and on) */
            if (ui.eng[ui.sel] == ENG_FMD)
                say("NO CHORDS", 1000);          /* (DRUM: none) */
            else if (sh) {
                if (ui.chord_on[ui.sel])
                    ui.chord_form[ui.sel] = (uint8_t)((ui.chord_form[ui.sel] + 1u) % CHF_N);
                ui.chord_on[ui.sel] = 1;
                say(CH_NAME[ui.chord_form[ui.sel]], 1200);
            } else {
                ui.chord_on[ui.sel] = (uint8_t)!ui.chord_on[ui.sel];
                say(ui.chord_on[ui.sel] ? CH_NAME[ui.chord_form[ui.sel]] : "CHORDS OFF", 1200);
            }
            prj_dirty = 1;
            ui.force = 1;
            continue;
        }
        if (((ch & keys) >> k & 1u) && !step_view() && k == KEY_TRK && ui.page == PG_MAIN) {   /* MUTE mode on or
                                                                         * off (the mutes stay as they are) */
            ui.mute_mode = (uint8_t)!ui.mute_mode;
            ui.tb_down = ui.tb_long = 0;
            say(ui.mute_mode ? "MUTE MODE" : "MUTE OFF", 1000);
            ui.force = 1;
            continue;
        }
        if (((ch & keys) >> k & 1u) && step_view() && k == KEY_TRK) {   /* the TRACK page, up or down */
            ui.page = ui.page == PG_TRK ? PG_MAIN : PG_TRK;
            ui.force = 1;
            continue;
        }
        if (((ch & keys) >> k & 1u) && step_view() && k == KEY_GEN) {   /* the phrase key: on its release or held */
            ui.gen_down = 1;
            ui.gen_long = 0;
            ui.gen_sh = (uint8_t)sh;
            ui.gen_t0 = fm1_ms;
            continue;
        }
        if (((ch & keys) >> k & 1u) && step_view() && k == KEY_MOD) {   /* MOD on or off (TRIG off) */
            ui.mod = (uint8_t)!ui.mod;
            ui.trig = 0;
            ui.ehalf = 0;
            say(ui.mod ? "MOD ON" : "MOD OFF", 700);
            ui.force = 1;
            continue;
        }
        if (((ch & keys) >> k & 1u) && step_view() && k == KEY_TRIG) {   /* TRIG on or off (MOD off) */
            ui.trig = (uint8_t)!ui.trig;
            ui.mod = 0;
            ui.ehalf = 0;
            say(ui.trig ? "TRIG ON" : "TRIG OFF", 700);
            ui.force = 1;
            continue;
        }
        if (((ch & keys) >> k & 1u) && step_view() && (k == KEY_DIR[0] || k == KEY_DIR[1] || k == KEY_DIR[2])) {
            uint32_t d = k == KEY_DIR[0] ? DIR_REV : k == KEY_DIR[1] ? DIR_PING : DIR_RND;   /* (the track's direction;
                                                                                         * again: forward) */
            seq_dir[ui.sel] = (uint8_t)(seq_dir[ui.sel] == d ? DIR_FWD : d);
            pat_dirty = 1;
            say(DIR_NAME[seq_dir[ui.sel]], 900);
            continue;
        }
        if (!((ch >> k) & 1u) || !key_plays(k))
            continue;
        if ((keys >> k) & 1u) {
            uint32_t note = key_note(k, ui.oct[ui.sel]);
            ui.key_track[k] = KEY_NONE;
            if (step_view()) {                       /* the step view: the white keys are the steps (SHIFT: copy, paste) */
                if (white_ix(k) >= 0 && sh && ui.sk == KEY_NONE) {
                    ui.sk = (uint8_t)k;
                    ui.sk_long = 0;
                    ui.sk_t0 = fm1_ms;
                } else if (white_ix(k) >= 0 && !sh && ui.step_key == KEY_NONE)
                    step_down(k);
                continue;
            }
            {   /* the note, or with CHORD mode on the track its chord (the form chosen), from the key's note */
                uint32_t f1 = ui.chord_on[ui.sel] && ui.eng[ui.sel] != ENG_FMD ? ui.chord_form[ui.sel] + 1u : 0u, j,
                         nn = f1 ? 3u : 1u;   /* (DRUM: never chords) */
                int32_t c[3] = {(int32_t)note, 0, 0};
                if (f1)
                    chord_of((int32_t)note, f1 - 1u, c);
                ui.key_track[k] = ui.sel;
                ui.key_note[k] = (uint8_t)note;
                ui.last_note[ui.sel] = (uint8_t)note;
                ui.last_chf[ui.sel] = (uint8_t)f1;
                ui.key_cn[k][0] = ui.key_cn[k][1] = ui.key_cn[k][2] = 0;
                for (j = 0; j < nn; j++) {
                    ui.key_cn[k][j] = (uint8_t)c[j];
                    ev_post(EV_ON, ui.sel, (uint32_t)c[j], 127);
                }
                rec_note(note, 127, k, f1);      /* (a step holds the chord as its root and form) */
                loop_note(ui.key_cn[k], nn, 127, k);   /* (a loop: its notes) */
            }
        } else if (k == ui.step_key)
            step_up();
        else if (ui.key_track[k] != KEY_NONE) {
            key_off(k);
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
        if (ui.mute_mode) {                          /* (MUTE mode: playing lit, muted dim) */
            fm1_led_key(PANEL_BTN[TRACK_BTN[t]], !trk_mute[t]);
            led_dim_key(PANEL_BTN[TRACK_BTN[t]], trk_mute[t]);
            continue;
        }
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
            fm1_led_key(14u + k, white_ix(k) == pat_clip_from || (seq_pend == 1u && white_ix(k) == seq_pend_k)
                                     ? (fm1_ms / 250u) & 1u   /* (the one copied, the one waiting to play: blink) */
                                     : white_ix(k) == (int32_t)pat_cur && seq_pend != 1u);
            led_dim_key(14u + k, 1);
            continue;
        }
        if (!step_view() && k == KEY_TRIG) {    /* (the BREAK key: lit while held, dim) */
            fm1_led_key(14u + k, ui.brk != 0);
            led_dim_key(14u + k, 1);
            continue;
        }
        if (!step_view() && k == KEY_TRK && ui.page == PG_MAIN) {   /* (the MUTE key: lit while on, dim off) */
            fm1_led_key(14u + k, ui.mute_mode);
            led_dim_key(14u + k, 1);
            continue;
        }
        if (!step_view() && k == KEY_GEN && ui.page == PG_MAIN) {   /* (the CHORD key: lit while on, dim off) */
            fm1_led_key(14u + k, ui.chord_on[ui.sel] && ui.eng[ui.sel] != ENG_FMD);
            led_dim_key(14u + k, 1);                 /* (dim on every track: the key has a use here) */
            continue;
        }
        if (step_view() && k == KEY_GEN) {           /* (the phrase key: dim, lit as it makes one) */
            fm1_led_key(14u + k, (int32_t)(ui.gen_until - fm1_ms) > 0 || ui.page == PG_GEN);
            led_dim_key(14u + k, 1);
            continue;
        }
        if (step_view() && k == KEY_MOD) {           /* (MOD: dim, lit while on) */
            fm1_led_key(14u + k, ui.mod);
            led_dim_key(14u + k, 1);
            continue;
        }
        if (step_view() && k == KEY_TRIG) {          /* (TRIG: dim, lit while on) */
            fm1_led_key(14u + k, ui.trig);
            led_dim_key(14u + k, 1);
            continue;
        }
        if (step_view() && (k == KEY_DIR[0] || k == KEY_DIR[1] || k == KEY_DIR[2])) {   /* (the directions: the one
                                                                                     * the track goes lit) */
            uint32_t d = k == KEY_DIR[0] ? DIR_REV : k == KEY_DIR[1] ? DIR_PING : DIR_RND;
            fm1_led_key(14u + k, seq_dir[ui.sel] == d);
            led_dim_key(14u + k, 1);
            continue;
        }
        if (step_view() && k == KEY_TRK) {           /* (the TRACK key: dim, lit while its page is up) */
            fm1_led_key(14u + k, ui.page == PG_TRK);
            led_dim_key(14u + k, 1);
            continue;
        }
        if (step_view() && white_ix(k) >= 0) {       /* (the step view: with a note lit, empty dim, the one playing
                                                      * the other way round; past the track's length, dark) */
            uint32_t i = (uint32_t)white_ix(k), len = seq_len[ui.sel] ? seq_len[ui.sel] : NSTEP;
            int on = seq_note[ui.sel][i] != 0;
            if (seq_run && seq_pos[ui.sel] == i)
                on = !on;
            if (ui.sclip.on && ui.sclip.t == ui.sel && ui.sclip.i == i)
                on = (fm1_ms / 250u) & 1u;           /* (the step copied: blinks) */
            fm1_led_key(14u + k, i < len && (on || k == ui.step_key));
            led_dim_key(14u + k, i < len);
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
        cv_text_c(3, &FONT_XL, d, trk_mute[ui.sel] ? RGB(80, 80, 80) : TRACK_COL[ui.sel]);   /* (muted: dark grey) */
        {   /* top left: the pattern playing (P3; orange with changes not saved); one waiting to play: it and the one
             * playing in turn */
            char pn[8] = "P";
            fmt_int(pn + 1, (seq_pend == 1u && (fm1_ms / 300u) & 1u ? seq_pend_k : pat_cur) + 1);
            cv_text(4, 2, &FONT_S, pn, pat_dirty ? RGB(255, 140, 0) : C_WHITE);
            if (ui.chord_on[ui.sel] && ui.eng[ui.sel] != ENG_FMD)   /* (CHORD mode: its form, left, under the
                                                                      * pattern; DRUM: none) */
                cv_text(4, 6 + FONT_S.h, &FONT_S, CH_ABBR[ui.chord_form[ui.sel] % CHF_N], TRACK_COL[ui.sel]);
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

/* a page of four rows (EDIT, the effects): a title, then each row its name and value, a bar under them; the
 * row just turned black on white */
static void draw_rows_title(const char *t1, const char *t2, uint16_t col)
{
    cv_begin(240, 52, C_BLACK);
    if (text_w(&FONT_M, t1) + text_w(&FONT_S, t2) + 16 <= 232)   /* the page, large; where it is (2/8, A-D), right */
        cv_text(4, 4, &FONT_M, t1, col);
    else                                              /* (both not fitting: the page smaller) */
        cv_text(4, 4 + ((int32_t)FONT_M.h - (int32_t)FONT_S.h) / 2, &FONT_S, t1, col);
    cv_text(236 - text_w(&FONT_S, t2), 4 + ((int32_t)FONT_M.h - (int32_t)FONT_S.h) / 2, &FONT_S, t2, C_WHITE);
    cv_rect(4, 46, 232, 2, RGB(60, 60, 60));
    cv_rect(ui.half ? 120 : 4, 46, 116, 4, col);      /* (which half: A .. D left, E .. H right) */
    cv_blit(0, 0);
}
/* a row: the name left, the value right -- every page's names in one size (XS), the values in the next (S), a value
 * too wide beside its name in the names' size (the pages' rows the same everywhere) */
static void draw_row(uint32_t r, const char *name, const char *val, int32_t frac, int bip, int touched, uint16_t col)
{
    uint16_t bg = touched ? C_WHITE : C_BLACK, fg = touched ? C_BLACK : C_WHITE;
    const font_t *fn = &FONT_XS, *fv = text_w(&FONT_XS, name) + text_w(&FONT_S, val) + 12 <= 228 ? &FONT_S : &FONT_XS;
    cv_begin(240, 47, bg);
    if (name[0]) {
        cv_text(6, 1 + (FONT_M.h - fn->h) / 2, fn, name, touched ? C_BLACK : row_tint ? row_tint : RGB(200, 200, 200));
        cv_text(234 - text_w(fv, val), 1 + (FONT_M.h - fv->h) / 2, fv, val, touched || !row_tint ? fg : row_tint);
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
    uint32_t id = page_id(s, ui.edit_pg), j, st = step_held() ? (uint32_t)white_ix(ui.step_key) : 0xFFu;
    int32_t t = (int32_t)(ui.touch_until - fm1_ms) > 0 ? ui.touched : -1;
    int full = ui.force || ui.shown_page != PG_EDIT || ui.shown_half != ui.half || ui.shown_sub != ui.edit_pg ||
               ui.shown_step != st;
    ui.shown_step = (uint8_t)st;
    if (full) {
        char t1[16], t2[16];
        page_title(s, id, t1);
        t2[0] = 0;
        if (st < NSTEP) {                            /* (a step held: S5 3/8, its values) */
            str_cpy(t2, "S", 2);
            fmt_int(t2 + 1, (int32_t)st + 1);
            str_cpy(t2 + str_len(t2), " ", 2);
        }
        fmt_int(t2 + str_len(t2), ui.edit_pg + 1);  /* 3/8 */
        str_cpy(t2 + str_len(t2), "/", 2);
        fmt_int(t2 + str_len(t2), (int32_t)pages_n(s));
        draw_rows_title(t1, t2, TRACK_COL[ui.sel]);
    }
    for (j = 0; j < 4u; j++) {
        par_t p;
        int32_t v;
        char b[16];
        uint32_t pid = id * 8u + j + 4u * ui.half;
        int locked = st < NSTEP && pl_find(ui.sel, st, pid) >= 0;
        par_get(s, id, j + 4u * ui.half, &p);
        v = st < NSTEP ? pl_val(ui.sel, st, pid) : par_val(s, id, j + 4u * ui.half);
        if (!full && ui.shown_rows[j] == v + locked * 100000 && ((int32_t)j == t) == ((int32_t)j == ui.shown_touch))
            continue;
        par_text(&p, v, b);
        draw_row(j, p.kind == PK_NONE ? "" : p.name, b, p.hi > p.lo ? (v - p.lo) * 1000 / (p.hi - p.lo) : 0,
                 p.kind == PK_BIP, (int32_t)j == t, st < NSTEP && !locked ? C_GREY : TRACK_COL[ui.sel]);
        ui.shown_rows[j] = v + locked * 100000;   /* (a step held: its bar grey where it plays the sound's own) */
    }
    ui.shown_touch = (int8_t)t;
    ui.shown_half = ui.half;
    ui.shown_sub = ui.edit_pg;
}
static void dl_time_text(char *b, int32_t v)   /* TIME as 128th notes: 1 .. 128 */
{
    fmt_int(b, v + 1);
}
static void fx_text(uint32_t k, int32_t x, char *b)   /* an effect's parameter k, as it reads */
{
    if (ui.fx_pg == FXP_CMP && fx_kind(k) == 3u)
        str_cpy(b, CMP_SLOT[k] == CP_RAT ? CP_RATN[x % NCP_RAT] : SC_SRCN[x % NCP_SCS], 8);
    else if (ui.fx_pg == FXP_DEL && k == DL_TIME)
        dl_time_text(b, x);
    else if (ui.fx_pg == FXP_DEL && k == DL_X)
        str_cpy(b, x ? "ON" : "OFF", 4);
    else if (ui.fx_pg == FXP_DEL && k == DL_FDBK) {   /* (FEEDBACK: as the share it feeds back, 64 the whole) */
        fmt_int(b, x * 100 / 64);
        str_cpy(b + str_len(b), "%", 2);
    } else if ((ui.fx_pg == FXP_REV && k == RV_DEC && x >= 127))
        str_cpy(b, "INF", 4);
    else if (fx_kind(k) == 2u) {
        int32_t d = bip_disp(x);
        str_cpy(b, d > 0 ? "+" : "", 2);
        fmt_int(b + str_len(b), d);
    } else
        fmt_int(b, pct_disp(x));
}
static const char *fx_name(uint32_t k)
{
    return ui.fx_pg == FXP_REV ? RVN[k] : ui.fx_pg == FXP_DEL ? DLN[k] : ui.fx_pg == FXP_CMP ? CPN[k] : CHN[k];
}
static void draw_fx_page(void)
{
    uint32_t j;
    int32_t t = (int32_t)(ui.touch_until - fm1_ms) > 0 ? ui.touched : -1;
    uint32_t sub = ui.fx_pg + 100u + (ui.fx_pg == FXP_CMP ? 10u * ui.sel : 0u);   /* (COMP: the chosen track's too) */
    int full = ui.force || ui.shown_page != PG_FXP || ui.shown_half != ui.half || ui.shown_sub != sub;
    if (full)
        draw_rows_title(FXT[ui.fx_pg], ui.half ? "E-H" : "A-D", C_SEND);
    for (j = 0; j < 4u; j++) {
        uint32_t k = j + 4u * ui.half;
        char b[16];
        const char *name = k < fx_n() ? fx_name(k) : "";
        int32_t x = k < fx_n() ? fx_get(k) : 0;
        if (!full && ui.shown_rows[j] == x && ((int32_t)j == t) == ((int32_t)j == ui.shown_touch))
            continue;
        fx_text(k, x, b);
        row_tint = ui.fx_pg == FXP_CMP && k < fx_n() && CMP_SLOT[k] >= CMP_SRC ? TRACK_COL[ui.sel] : 0;   /* (the
                                                                                    * chosen track's, in its colour) */
        draw_row(j, name, b,
                 k < fx_n() && fx_kind(k) == 3u ? x * 1000 / (int32_t)(fx_list_n(k) - 1u)
                 : (ui.fx_pg == FXP_DEL && k == DL_X ? x * 127 : x) * 1000 / 127,
                 k < fx_n() && fx_kind(k) == 2u, (int32_t)j == t, row_tint ? row_tint : C_SEND);
        row_tint = 0;
        ui.shown_rows[j] = x;
    }
    ui.shown_touch = (int8_t)t;
    ui.shown_half = ui.half;
    ui.shown_sub = (uint8_t)sub;
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
    int full = ui.force || ui.shown_page != SHOWN_STEP || ui.shown_sub != (ui.mod ? i + 48u : i + 16u * ui.half);
    if (full) {
        char t1[12] = "STEP ";
        fmt_int(t1 + 5, (int32_t)i + 1);
        draw_rows_title(t1, ui.mod ? "MOD" : "TRIG", TRACK_COL[t]);   /* (its half under the title: A .. D, E .. H) */
    }
    if (ui.mod) {                                /* MOD: MUTATE, MUT EVERY, MIDI FX, FX EVERY */
        static const char *const MN[4] = {"MUTATE", "MUT EVERY", "MIDI FX", "FX EVERY"};
        int32_t n = SCALE_N[ui.scale];
        for (j = 0; j < 4u; j++) {
            int32_t v = j == 0u ? seq_mut[t][i] : j == 1u ? seq_mcy[t][i] : j == 2u ? seq_mfx[t][i] : seq_fcy[t][i];
            int32_t fr, bip = 0;
            char b[12];
            if (!full && ui.shown_rows[j] == v && ((int32_t)j == tc) == ((int32_t)j == ui.shown_touch))
                continue;
            if (j == 0u) {
                if (v == MUT_RND)
                    str_cpy(b, "RND", 4);
                else if (!v)
                    str_cpy(b, "OFF", 4);
                else {
                    str_cpy(b, v > 0 ? "+" : "", 2);
                    fmt_int(b + str_len(b), v);
                }
                fr = v == MUT_RND ? 1000 : (clampi(v, -n, n) + n) * 1000 / (2 * n + 1);
                bip = v != MUT_RND;
            } else if (j == 2u) {
                str_cpy(b, MFX_NAME[v % NMFX], sizeof b);
                fr = v * 1000 / (NMFX - 1);
            } else {
                fmt_int(b, v + 1);
                fr = v * 1000 / 15;
            }
            draw_row(j, MN[j], b, fr, bip, (int32_t)j == tc, TRACK_COL[t]);
            ui.shown_rows[j] = v;
        }
        ui.shown_touch = (int8_t)tc;
        ui.shown_sub = (uint8_t)(i + 48u);
        return;
    }
    if (ui.half) {                               /* E .. H: the ratchet */
        for (j = 0; j < 4u; j++) {
            int32_t v = j == 0u ? seq_rtg[t][i] : 0;
            if (!full && ui.shown_rows[j] == v && ((int32_t)j == tc) == ((int32_t)j == ui.shown_touch))
                continue;
            if (j == 0u)
                draw_row(j, "RATCHET", RTG_NAME[v % NRTG], v * 1000 / (NRTG - 1), 0, (int32_t)j == tc, TRACK_COL[t]);
            else
                draw_row(j, "", "", 0, 0, 0, TRACK_COL[t]);
            ui.shown_rows[j] = v;
        }
        ui.shown_touch = (int8_t)tc;
        ui.shown_sub = (uint8_t)(i + 16u);
        return;
    }
    for (j = 0; j < 4u; j++) {
        int32_t v, fr, key;
        char b[16];
        v = j == 0u ? seq_note[t][i] : j == 1u ? seq_vel[t][i] : j == 2u ? seq_glen[t][i] : seq_cond[t][i];
        fr = j == 0u ? (v - 12) * 1000 / 108 : j == 1u ? v * 1000 / 127 : j == 2u ? v * 1000 / (NGLEN - 1)
             : v * 1000 / (NCOND - 1);
        key = v;
        if (!full && ui.shown_rows[j] == key && ((int32_t)j == tc) == ((int32_t)j == ui.shown_touch))
            continue;
        if (j == 0u)
            note_text(b, (uint32_t)v);
        else if (j == 1u)
            fmt_int(b, pct_disp(v) ? pct_disp(v) : 1);
        else if (j == 2u)
            str_cpy(b, GLEN_NAME[v], sizeof b);
        else
            cond_text(b, (uint32_t)v);
        draw_row(j, NAME[j], b, fr, 0, (int32_t)j == tc, TRACK_COL[t]);
        ui.shown_rows[j] = key;
    }
    ui.shown_touch = (int8_t)tc;
    ui.shown_sub = (uint8_t)i;
}

/* the TEMPO page (PLAY held): the session's tempo, as large as the screen takes it */
static void draw_tempo_page(void)
{
    char n[8];
    if (!ui.force && ui.shown_page == PG_TEMPO && ui.shown_rows[0] == ui.tempo)
        return;
    fmt_int(n, ui.tempo);
    CV_AREA(0, 0, 240, 60, C_BLACK)
        cv_text_c(14, &FONT_M, "TEMPO", C_WHITE);
    CV_AREA(0, 60, 240, 150, C_BLACK)
        cv_text_c(0, &FONT_XL, n, RGB(255, 210, 120));
    CV_AREA(0, 210, 240, 30, C_BLACK)
        cv_text_c(2, &FONT_S, "BPM", C_GREY);
    ui.shown_rows[0] = ui.tempo;
}

/* the PHRASE page (the phrase key held): the track's DENSITY and COMPLEXITY, as rows */
static void draw_gen_page(void)
{
    uint32_t t = ui.sel, j;
    int32_t tc = (int32_t)(ui.touch_until - fm1_ms) > 0 ? ui.touched : -1;
    int full = ui.force || ui.shown_page != PG_GEN || ui.shown_sub != t;
    if (full) {
        char t1[12] = "PHRASE ";
        uint8_t h = ui.half;
        fmt_int(t1 + 7, (int32_t)t + 1);
        ui.half = 0;
        draw_rows_title(t1, "", TRACK_COL[t]);
        ui.half = h;
    }
    for (j = 0; j < 4u; j++) {
        int32_t v = j == 0u ? ui.gen_den[t] : j == 1u ? ui.gen_cx[t] : 0;
        char b[8];
        if (!full && ui.shown_rows[j] == v && ((int32_t)j == tc) == ((int32_t)j == ui.shown_touch))
            continue;
        fmt_int(b, v);
        if (j < 2u)
            draw_row(j, j ? "COMPLEXITY" : "DENSITY", b, v * 10, 0, (int32_t)j == tc, TRACK_COL[t]);
        else
            draw_row(j, "", "", 0, 0, 0, TRACK_COL[t]);
        ui.shown_rows[j] = v;
    }
    ui.shown_touch = (int8_t)tc;
    ui.shown_sub = (uint8_t)t;
}

/* the TRACK page (the step view's 2nd black key): the track's LENGTH and CLOCK, as rows */
static void draw_trk_page(void)
{
    uint32_t t = ui.sel, j;
    int32_t tc = (int32_t)(ui.touch_until - fm1_ms) > 0 ? ui.touched : -1;
    int full = ui.force || ui.shown_page != PG_TRK || ui.shown_sub != t;
    if (full) {
        char t1[12] = "TRACK ";
        uint8_t h = ui.half;
        fmt_int(t1 + 6, (int32_t)t + 1);
        ui.half = 0;
        draw_rows_title(t1, "", TRACK_COL[t]);
        ui.half = h;
    }
    for (j = 0; j < 4u; j++) {
        int32_t v = j == 0u ? (seq_len[t] ? seq_len[t] : (int32_t)NSTEP) : j == 1u ? seq_div[t] : 0;
        char b[8];
        if (!full && ui.shown_rows[j] == v && ((int32_t)j == tc) == ((int32_t)j == ui.shown_touch))
            continue;
        if (j == 0u) {
            fmt_int(b, v);
            draw_row(j, "LENGTH", b, (v - 1) * 1000 / 15, 0, (int32_t)j == tc, TRACK_COL[t]);
        } else if (j == 1u)                      /* (bipolar: 1X in the middle) */
            draw_row(j, "CLOCK", DIV_NAME[v % NDIV], v <= (int32_t)SEQ_DIV1 ? v * 500 / (int32_t)SEQ_DIV1
                     : 500 + (v - (int32_t)SEQ_DIV1) * 500 / (int32_t)(NDIV - 1u - SEQ_DIV1), 1, (int32_t)j == tc, TRACK_COL[t]);
        else
            draw_row(j, "", "", 0, 0, 0, TRACK_COL[t]);
        ui.shown_rows[j] = v;
    }
    ui.shown_touch = (int8_t)tc;
    ui.shown_sub = (uint8_t)t;
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
static int32_t dial_key(void)                    /* (what the dial shows, folded into one number) */
{
    if (ui.dial_lk)
        return pl_val(ui.sel, ui.dial_step, ui.dial_lk - 1u) + ui.dial_lk * 1024 + ui.dial_step * 131072 + (1 << 24);
    return par_val(cur(), ui.dial_pg, ui.dial_j) + ui.dial_pg * 1024 + ui.dial_j * 8192;
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
    if (ui.dial_lk) {                                /* a step's lock: REVERB S5 */
        uint32_t id = ui.dial_lk - 1u;
        pl_par(s, id, &p);
        v = pl_val(ui.sel, ui.dial_step, id);
        str_cpy(t, p.name, sizeof t - 4);
        str_cpy(t + str_len(t), " S", 3);
        fmt_int(t + str_len(t), ui.dial_step + 1);
    } else {
        par_get(s, ui.dial_pg, ui.dial_j, &p);
        v = par_val(s, ui.dial_pg, ui.dial_j);
        str_cpy(t, p.name, sizeof t - 2);
        str_cpy(t + str_len(t), " ", 2);
        t[str_len(t) + 1] = 0;
        t[str_len(t)] = (char)('1' + ui.sel);
    }
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
    ui.shown_dial = dial_key();
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
    char cp[12] = "COPY S";
    if (ui.sclip.on && step_view() && !msg) {        /* (a step copied: shown, blinking, until it is pasted) */
        fmt_int(cp + 6, ui.sclip.i + 1);
        c = cp;
        msg = (int)((fm1_ms / 400u) & 1u);
    }
    if (ui.mute_mode && !msg) {                      /* (MUTE mode: said in the band while it is on) */
        c = "MUTE MODE";
        msg = 1;
    }
    int32_t px;
    if (draw_flash())
        return;
    if ((int32_t)(ui.dial_until - fm1_ms) > 0 && (ui.page == PG_MAIN || ui.dial_lk)) {   /* (a step's lock: on any
                                                                                      * page, the step's own too) */
        int32_t key = dial_key();
        if (ui.shown_page != SHOWN_DIAL || (ui.shown_dial != key && (uint32_t)(fm1_ms - ui.dial_drawn_at) >= 40u))
            draw_dial();
        ui.shown_page = SHOWN_DIAL;
        return;
    }
    if (step_page()) {                               /* a step held, the step view's knobs on it */
        draw_step_page();
        ui.shown_page = SHOWN_STEP;
        ui.force = 0;
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
    if (ui.page == PG_TEMPO) {
        draw_tempo_page();
        ui.shown_page = PG_TEMPO;
        ui.force = 0;
        return;
    }
    if (ui.page == PG_GEN) {
        draw_gen_page();
        ui.shown_page = PG_GEN;
        ui.force = 0;
        return;
    }
    if (ui.page == PG_TRK) {
        draw_trk_page();
        ui.shown_page = PG_TRK;
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
    {
        int32_t pk = pat_cur * 32 + pat_dirty * 16 + ui.rec * 4 + ui.lrec +
                     (seq_pend == 1u ? 1024 + (int32_t)((fm1_ms / 300u) & 1u) * 2048 : 0) +
                     (ui.oct[ui.sel] + 3) * 4096 +
                     (ui.chord_on[ui.sel] && ui.eng[ui.sel] != ENG_FMD ? (ui.chord_form[ui.sel] + 1) * 32768 : 0) +
                     trk_mute[ui.sel] * (1 << 20);   /* (the
                                                      * octave and the chord shown: drawn again as they change) */
        if (ui.force || ui.shown_sel != ui.sel || ui.shown_prj != pk) {
            ui.shown_prj = pk;
            draw_number();
            ui.shown_sel = ui.sel;
            ui.shown_eng = ui.shown_msg = ui.shown_pan = ui.shown_level = -100;
        }
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

/* the start: on each track a sound of each machine; tracks 1 .. 3 on DRUM (a kick, a snare, a hat), 4 4-OP,
 * 5 WAVY, 6 HIVE */
static void ui_init(void)
{
    static const uint8_t ENG0[NTRACK] = {ENG_FMD, ENG_FMD, ENG_FMD, ENG_FMT, ENG_WAV, ENG_SWM};
    static const int8_t FAM0[NTRACK] = {0, 4, 8, -1, -1, -1};
    uint32_t t, e;
    memset(&ui, 0, sizeof ui);
    memset(ui.midi_track, 0xFF, sizeof ui.midi_track);
    memset(ui.key_track, 0xFF, sizeof ui.key_track);
    ui.step_key = ui.qk = ui.sk = KEY_NONE;
    memset(ui.rec_step, 0xFF, sizeof ui.rec_step);
    memset(ui.lk_ev, 0xFF, sizeof ui.lk_ev);
    ui.lv_k = KEY_NONE;
    memset(ui.gen_den, 50, sizeof ui.gen_den);
    memset(ui.gen_cx, 50, sizeof ui.gen_cx);
    ui.click = 1;
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
    ui.solo = 0xFF;
    ui.touched = -1;
    ui.shown_sel = ui.shown_eng = ui.shown_msg = -1;
    ui.force = 1;
}
