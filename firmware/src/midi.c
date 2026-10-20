/* SPDX-License-Identifier: GPL-3.0-only
 * MIDI over USB (build 46), after the Model:Cycles' MIDI CONFIG: the settings (GLOBAL SETUP > MIDI, setup.c; kept in
 * the flash), the clock and the transport in and out, the tracks' notes and CCs out, program changes.
 *
 * CLK IN (on, a clock arriving): each pulse (24 a beat) is counted and timed as usb_poll takes it (TIMER5, 2 kHz).
 * The tempo is the average of the last 24 intervals (a beat, as the Model:Cycles takes it). The sequencer runs at
 * that tempo block by block (0.73 ms), and is held to the count: it may run at most one pulse ahead of the pulses
 * received, and behind it catches up over a few blocks -- so it never drifts from the master, whatever the jitter of
 * the cable. And the transport: START, the sequencer from step 1 on the next pulse; CONTINUE, from the song
 * position last received (SPP); STOP, stopped.
 * CLK OUT: 24 pulses a beat from the sequencer's own position, block by block, stopped too; and the song position and
 * START (CONTINUE from a song position) as it starts, with its first pulse, so a slave starts with step 1; STOP as
 * it stops.
 * Notes out (SEND NOTES): every note a track plays -- its steps, its loops, the keys, notes received -- on the
 * track's OUT channel, with its velocity, and its note off; a muted track sends none. CCs (RECEIVE CC, SEND CC): the
 * sound's parameters, each page's eight (MIDI_CC), the level (7), the pan (10), the mute (94), on the track's
 * channels. Program changes: 0 .. 15, the session's patterns 1 .. 16. */

#define MIDI_TRACKS 6u
typedef struct {
    uint8_t clk_in, clk_out, pc_in, pc_out;      /* (CLK IN, OUT: the clock and the transport, as the Model:Cycles) */
    uint8_t in_ch[MIDI_TRACKS], auto_ch, pc_in_ch;     /* (channels: 0 OFF, 1 .. 16) */
    uint8_t out_ch[MIDI_TRACKS], pc_out_ch;
    uint8_t rcv_note, rcv_cc, send_note, send_cc;
    uint8_t usb_in, trs_in;                      /* PORTS: what is taken from USB, from the TRS jack (build 47; a
                                                  * record from build 46 ends before them: midi_cfg46) */
} midi_cfg_t;
#define MIDI_CFG0 {0, 0, 0, 0, {1, 2, 3, 4, 5, 6}, 16, 16, {1, 2, 3, 4, 5, 6}, 16, 1, 1, 0, 0, 1, 1}
#define MIDI_CFG46 __builtin_offsetof(midi_cfg_t, usb_in)
static midi_cfg_t mcfg = MIDI_CFG0;             /* (the main loop's; the ISRs read single bytes of it) */

/* the CCs of the sound's parameters, page id x 8 + knob (params.c: SYN 1 .. 4, FLTR 1, FLTR 2, AMP, FX, MANGLE):
 * 16 .. 119 in order, past the ones a keyboard or a DAW sends for other things -- 32 (bank LSB), 38 (data entry
 * LSB), 64 .. 69 (pedals), 94 (the mute here), 96 .. 101 (data increment, NRPN, RPN) */
#define MIDI_NPAR 72u
#define CC_LEVEL 7u
#define CC_PAN 10u
#define CC_MUTE 94u
static int midi_cc_free(uint32_t c)
{
    return c != 32u && c != 38u && !(c >= 64u && c <= 69u) && c != CC_MUTE && !(c >= 96u && c <= 101u);
}
static uint8_t MIDI_CC[MIDI_NPAR];              /* (filled by midi_init) */
static uint8_t MIDI_PAR[128];                   /* (and back: a CC's parameter, 0xFF none) */
static void midi_cc_init(void)
{
    uint32_t c = 16, i;
    memset(MIDI_PAR, 0xFF, sizeof MIDI_PAR);
    for (i = 0; i < MIDI_NPAR; i++) {
        while (!midi_cc_free(c))
            c++;
        MIDI_CC[i] = (uint8_t)c;
        MIDI_PAR[c] = (uint8_t)i;
        c++;
    }
}

/* ------------------------------------------------------------ out --- */
/* the audio ISR's: a USB-MIDI packet (usb.c's ring, the one producer); the main loop's go through mq, which the
 * audio ISR empties into it at each block's start */
static void midi_ch_send(uint32_t st, uint32_t d1, uint32_t d2)
{
    midi_out_event((st >> 4) | st << 8 | (d1 & 0x7Fu) << 16 | (d2 & 0x7Fu) << 24);
}
static void midi_rt_send(uint32_t st) { midi_out_event(0xFu | st << 8); }
#define MMQ 32u
static uint32_t mm_q[MMQ];
static volatile uint32_t mm_w, mm_r;
static void midi_main_send(uint32_t st, uint32_t d1, uint32_t d2)   /* the main loop's */
{
    if (mm_w - mm_r < MMQ) {
        mm_q[mm_w % MMQ] = (st >> 4) | st << 8 | (d1 & 0x7Fu) << 16 | (d2 & 0x7Fu) << 24;
        RING_PUBLISH();
        mm_w++;
    }
}
static void midi_main_drain(void)                 /* (the audio ISR's) */
{
    while (mm_r != mm_w) {
        midi_out_event(mm_q[mm_r % MMQ]);
        RING_PUBLISH();
        mm_r++;
    }
}
/* a track's note on (vel > 0) or off, out on its channel; the notes it has sent on, so each off goes once */
static uint8_t midi_held[MIDI_TRACKS][16];
static void midi_note_out(uint32_t t, uint32_t note, uint32_t vel, int muted)
{
    uint32_t ch = mcfg.out_ch[t], b = 1u << (note & 7u);
    uint8_t *h = &midi_held[t][(note & 0x7Fu) >> 3];
    if (!ch || note > 127u)
        return;
    if (vel) {
        if (!mcfg.send_note || muted)
            return;
        if (*h & b)
            midi_ch_send(0x80u | (ch - 1u), note, 0);
        midi_ch_send(0x90u | (ch - 1u), note, vel);
        *h |= (uint8_t)b;
    } else if (*h & b) {
        midi_ch_send(0x80u | (ch - 1u), note, 0);
        *h &= (uint8_t)~b;
    }
}
static void midi_track_off(uint32_t t)           /* (every note a track has sent on: off) */
{
    uint32_t n;
    for (n = 0; n < 128u; n++)
        if (midi_held[t][n >> 3] & (1u << (n & 7u)))
            midi_note_out(t, n, 0, 0);
}

/* -------------------------------------------------- the clock in --- */
/* (usb_poll's, TIMER5: written there, read by the audio ISR) */
#define XK_N 24u
static uint32_t xk_ring[XK_N], xk_sum;
static uint8_t xk_i, xk_fill;
static volatile uint32_t xk_n;                   /* pulses taken, all told */
static volatile uint32_t xk_t;                   /* the last one's time, ticks (24 MHz) */
static volatile uint32_t xk_iv;                  /* the average interval, ticks (0: none yet) */
static volatile uint8_t xk_go;                   /* START (1) or CONTINUE (2) taken: the sequencer starts on the
                                                  * next pulse (CLOCK IN), or at once */
static volatile uint8_t xk_start, xk_stop;       /* (.. for the audio ISR: start (1, 2), stop) */
static volatile uint16_t xk_spp;                 /* the song position, 16ths */
#define XK_LOST (24000u * 500u)                  /* no pulse for 0.5 s: no clock */
static void midi_rt(uint32_t st, uint32_t d)     /* (usb_poll, or the TRS jack's poll -- d's bit 16: a clock,
                                                  * START, CONTINUE, STOP, a song position) */
{
    uint32_t now = fm1_ticks();
    if (!((d >> 16) & 1u ? mcfg.trs_in : mcfg.usb_in))   /* (PORTS: the input left out) */
        return;
    d &= 0x3FFFu;
    if (st == 0xF8u) {
        if (!mcfg.clk_in)
            return;
        if (xk_n && (uint32_t)(now - xk_t) < XK_LOST) {
            uint32_t iv = now - xk_t;
            xk_sum += iv - xk_ring[xk_i];
            xk_ring[xk_i] = iv;
            xk_i = (uint8_t)((xk_i + 1u) % XK_N);
            if (xk_fill < XK_N)
                xk_fill++;
            xk_iv = xk_sum / xk_fill;
        } else {                                 /* (the first, or after a pause: the average starts again) */
            memset(xk_ring, 0, sizeof xk_ring);
            xk_sum = 0;
            xk_i = xk_fill = 0;
        }
        xk_t = now;
        RING_PUBLISH();
        xk_n++;
        if (xk_go) {
            xk_start = xk_go;
            xk_go = 0;
        }
        return;
    }
    if (!mcfg.clk_in)
        return;
    if (st == 0xFAu || st == 0xFBu) {
        if (st == 0xFAu)
            xk_spp = 0;
        xk_go = (uint8_t)(st == 0xFAu ? 1u : 2u);
    } else if (st == 0xFCu) {
        xk_go = 0;
        xk_stop = 1;
    } else if (st == 0xF2u)
        xk_spp = (uint16_t)d;
}
static int midi_slave(void)                      /* following a clock (signed: a pulse timed by usb_poll just after
                                                  * the caller read the time is not a lost clock) */
{
    return mcfg.clk_in && xk_iv && (int32_t)(fm1_ticks() - xk_t) < (int32_t)XK_LOST;
}
static uint32_t midi_ext_bpm10(void) { return xk_iv ? clampi((int32_t)(600000000u / xk_iv), 100, 3000) : 0u; }

/* ------------------------------------------- the block's advance --- */
/* (the audio ISR's) how far the sequencer moves this block, in its units (SEQ_16TH a 16th): CTL x the tempo x 10.
 * Following a clock: to where the master is at the block's end -- the pulses received (xk_owed: received, not yet
 * played through), and of the next one the part its interval (the average) says has gone by since the last, never
 * past it before it comes. So the steps fall on the pulses, a pulse's own jitter apart at most, and nothing adds up:
 * slower or faster than the average, the next pulse puts it right. Far behind (a jump in tempo), it catches up over a
 * few blocks (at most 4 blocks' worth a block) */
#define MIDI_16TH (FS * 15u * 10u)               /* (tracks.c's SEQ_16TH: a 16th, in samples x the tempo x 10) */
#define MIDI_PU (MIDI_16TH / 6u)                 /* a pulse: a 16th is 6 */
#define XK_BT ((uint32_t)CTL * 24000000u / FS)   /* a block, in ticks */
static int32_t xk_owed;
static uint32_t xk_seen;
static uint32_t midi_adv(int running)
{
    uint32_t base, d, f, iv = xk_iv;
    int32_t el, tgt;
    if (!midi_slave()) {
        xk_seen = xk_n;
        xk_owed = 0;
        return CTL * (uint32_t)fx_bpm10;
    }
    base = CTL * midi_ext_bpm10();
    d = xk_n - xk_seen;
    xk_seen += d;
    if (!running) {
        xk_owed = 0;
        return base;
    }
    xk_owed += (int32_t)(d < 64u ? d : 64u) * (int32_t)MIDI_PU;
    el = (int32_t)(fm1_ticks() + XK_BT - xk_t);  /* (since the last pulse, at the block's end) */
    f = el <= 0 ? 0u : (uint32_t)el >= iv ? 1024u : (uint32_t)el * 1024u / iv;   /* (of the next, x 1024) */
    tgt = xk_owed + (int32_t)((MIDI_PU / 4u) * f / 256u);
    if (tgt <= 0)
        return 0;                                /* (ahead: held until the pulses come) */
    if ((uint32_t)tgt > 4u * base)
        tgt = (int32_t)(4u * base);
    xk_owed -= tgt;
    return (uint32_t)tgt;
}

/* -------------------------------------------------- the clock out --- */
static uint32_t mk_acc;                          /* (the audio ISR's: the sequencer's units since the last pulse) */
static void midi_clock_out(uint32_t adv)
{
    if (!mcfg.clk_out)
        return;
    mk_acc += adv;
    while (mk_acc >= MIDI_PU) {
        mk_acc -= MIDI_PU;
        midi_rt_send(0xF8u);
    }
}
static void midi_started(uint32_t spp)           /* (the sequencer started, at song position spp: its first pulse
                                                  * with START, sent in this block) */
{
    if (mcfg.clk_out) {
        midi_out_event(0x3u | 0xF2u << 8 | (spp & 0x7Fu) << 16 | ((spp >> 7) & 0x7Fu) << 24);
        midi_rt_send(spp ? 0xFBu : 0xFAu);
    }
    mk_acc = MIDI_PU;                            /* (a pulse now, with the first step) */
    xk_seen = xk_n;                              /* (following a clock: this pulse is the start) */
    xk_owed = 0;
}
static void midi_stopped(void)                   /* (the sequencer stopped) */
{
    uint32_t t;
    if (mcfg.clk_out)
        midi_rt_send(0xFCu);
    for (t = 0; t < MIDI_TRACKS; t++)
        midi_track_off(t);
}
