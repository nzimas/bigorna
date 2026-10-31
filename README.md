<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/logo-dark.svg">
    <img src="docs/assets/logo-light.svg" alt="Bigorna" width="560">
  </picture>
</p>

# Bigorna

A firmware for the M-VAVE FM-1, for rhythmic noise, power noise, industrial and IDM.

Bigorna takes the Elektron Digitone II as its model. It is not a copy of its machines: it tries, as far as the FM-1
allows, to bring the same grit and harshness the Digitone II is known for, and the same range of tones. Six tracks,
four machines (4-OP, DRUM, WAVY, HIVE), the Digitone II's page layout, a 16-step sequencer with parameter locks, a
looper, and send effects. *Bigorna* is Portuguese for anvil.

Website, browser installer and manual: <https://nzimas.github.io/bigorna/>

> [!WARNING]
> Bigorna is unofficial. It is not made, endorsed or supported by M-VAVE or Elektron, and installing it may void
> any warranty that applies to your FM-1. See the [disclaimer](#disclaimer).

## Install

From the browser: open the [installer](https://nzimas.github.io/bigorna/install.html) in a desktop Chrome, Edge or
Opera, connect the FM-1 with a USB data cable and press Install. After installing, switch the FM-1 off and on once.

From the command line (macOS or Linux): `./setup.sh` once, then `./install.sh` (builds, then installs) or
`./install.sh FILE.fwsc` (installs a package). `./install.sh --info` says what the connected FM-1 runs.

# Manual

## Playing it

| Control | What it does |
| --- | --- |
| **The six top buttons** | choose track **1–6** (lit: chosen; glowing dimly: sounding). From any page, back to the track. In [MUTE mode](#mute-mode): mute and solo. |
| **PRESETS** knob | the chosen track's machine: **4-OP**, **DRUM**, **WAVY**, **HIVE**. Each track keeps one sound per machine. |
| **SHIFT + PRESETS** | a new sound of the family before or after. |
| **HOME** | a new sound for the chosen track ([Sounds](#sounds)). |
| **SHIFT + HOME** | a new sound of the same family. |
| **HOME held** (0.8 s) | save the session, as SAVE held. |
| **OCT− / OCT+** | the chosen track's octave, −3 to +3. In the step view: the track's [transposition](#transposition). |
| **SHIFT + OCT− / OCT+** | every track's transposition. |
| the white keys | play the chosen track in the scale; the keys that play the root glow dimly. In the step view: the track's 16 steps. On the PATTERNS grid: the 16 patterns. |
| black key 1 | [CHORD mode](#chord-mode) on or off. **SHIFT +**: the next chord form. |
| black key 2 | [MUTE mode](#mute-mode) on or off. |
| black key 3 | **held**: [BREAK](#break) on the chosen track. **SHIFT +**: on every track. |
| black key 4 | [MIDI LEARN](#midi-learn) on or off. |
| black key 5 | the chosen track's [arpeggiator](#arpeggiator) on or off. **Held** (0.8 s): its page. |
| black key 9 (3rd from the right) | **STEP**: the [step view](#the-step-view). **SHIFT +**: empty the chosen track's steps. |
| black key 10 | **EDIT**: the [sound's pages](#the-sounds-pages-edit). |
| black key 11 (the last) | **SHIFT**. |
| **SELECT** knob | the chosen track's level. On the SCALE page: the root. |
| **ALGORITHM** knob | the chosen track's pan. On the SCALE page: the scale. |
| **KNOB 1–4** | the chosen track's filter FREQ and RESO, delay send, reverb send. On a page: its rows A–D. |
| **SHIFT + KNOB 3 / KNOB 4** | the chosen track's chorus send / overdrive. |
| **PLAY** | start or stop the [sequencer](#the-sequencer). **Held** (0.8 s): the [TEMPO](#tempo) page. |
| **SHIFT + PLAY** | the metronome on or off. |
| **REC** | [record](#recording) the keys into the chosen track's steps. |
| **SHIFT + REC** | the [looper](#the-looper): arm a loop. **Held**: the track's loops on the white keys. |
| **ARP** | the SCALE page ([Scales](#scales)); ARP again leaves it. |
| **SHIFT + ARP** | a new scale and root, drawn at random; the SCALE page shows them for 1.5 s. |
| **SEQ** | the [effects' pages](#the-effects-seq), then the chosen track's [LFOs](#lfos), in turn. |
| **SHIFT + SEQ** | the chosen track's LFOs drawn anew. |
| **SAVE** | the [PATTERNS](#patterns) grid; SAVE again leaves it. |
| **SAVE held** (0.8 s) | save the session. On the PATTERNS grid: save the pattern playing into the slot under the cursor. |
| **SHIFT + SAVE** | the [SESSIONS](#sessions) list; SHIFT + SAVE again leaves it. |
| **SHIFT + SAVE held** (0.8 s) | [GLOBAL SETUP](#global-setup): START and MIDI. |
| **MASTER** | the output level. |
| MIDI in (USB and TRS) | notes and CCs for each track ([MIDI](#midi)). |

While a knob turns, the screen shows its value as a large dial for a second.

## Tracks and sounds

Six tracks, each with its own machine, sound, octave, level, pan, steps and loops. Each track keeps a sound for each
of the four machines, so turning PRESETS back finds the sound it had. The tracks share twelve voices; when all twelve
sound, a new note takes the quietest one. DRUM tracks choke: each hit fades the track's previous one.

The FM-1's second processor core plays half the voices and the effects. If it ever faults, the first core carries on
alone until the FM-1 is switched off, and the screen says **1 CORE**.

### Sounds

**HOME** draws a new sound for the chosen track from a recipe of its machine: a family such as TIGHT KICK, METAL HAT,
FM BASS or SWARM DRONE, leaning each time towards industrial, IDM or abstract. **SHIFT + HOME** draws another from the
same family; **SHIFT + PRESETS** moves to the family before or after. The screen shows the machine and the family.
There are 56 families: 29 for DRUM, 9 for 4-OP, 10 for WAVY, 8 for HIVE. Drone, pad and texture families come with
LFOs already set.

## The sound's pages (EDIT)

Black key 10 (**EDIT**) shows the sound's pages: SYN 1 to 4 (the machine's), FLTR 1, FLTR 2, AMP, FX and MANGLE. Each
page has eight parameters: **KNOB 1–4** set A to D; **PRESETS** turned right shows E to H, left goes back. **OCT− /
OCT+** move between the pages. EDIT again leaves.

### 4-OP

Four-operator FM after the Digitone II's FM TONE: operator C and the modulators A, B1 and B2.

| Page | Parameters |
| --- | --- |
| SYN 1 | ALGO (eight algorithms), RATIO C, RATIO A, RATIO B, HARM, DETUNE, FEEDBACK, MIX |
| SYN 2 | the A and B envelopes: ATK, DEC, END, LEV for each |
| SYN 3 | DELAY, TRIG and RESET for A and for B, PHASE RST |
| SYN 4 | OFS C, OFS A, OFS B1, OFS B2 (fine offsets), KEYTRK A, KEYTRK B1, KEYTRK B2 |

### DRUM

Three-operator FM percussion after FM DRUM.

| Page | Parameters |
| --- | --- |
| SYN 1 | TUNE, SWP TIME, SWP DEPTH (the pitch sweep), ALGO (seven), OP C WAVE, OP AB WAVE, FEEDBACK, FOLD |
| SYN 2 | RATIO, DEC, END and MOD for A and for B |
| SYN 3 | HOLD, DECAY (the body), PHASE C, LEVEL, AB NOISE, NOISE RST, NOISE RM |
| SYN 4 | NS HOLD, NS DECAY, TRANS (the transient's kind), TRANS LEV, NS BASE, NS WIDTH, NS GRAIN, NS LEVEL |

**AB NOISE** mixes operators A and B towards a noise that follows their pitch: at 0 the waveform only, at 127 the
noise only. Modulating C, it gives metallic, grainy hits. With **NOISE RST** on, every hit gets the same noise.

### WAVY

Two wavetable oscillators with phase distortion, and a noise generator.

| Page | Parameters |
| --- | --- |
| SYN 1 | TUNE, WAVE, PD and LEVEL for each oscillator |
| SYN 2 | OFS 1, TABLE 1, MOD (OFF, RING, RING FIX, SYNC), RESET, OFS 2, TABLE 2, DRIFT |
| SYN 3 | NS ATK, NS HOLD, NS DECAY, NS LEVEL, NS BASE, NS WIDTH, NS TYPE (GRAIN, TUNED, S+H), NS CHAR |

### HIVE

A swarm of six detuned oscillators and a main oscillator.

| Page | Parameters |
| --- | --- |
| SYN 1 | TUNE, SWRM WAVE, DETUNE, MIX, MAIN OCT (−2, −1, 0), MAIN WAVE, ANIMATION, NOISE MOD |

### FLTR, AMP, FX and MANGLE

These pages are the same on every machine.

| Page | Parameters |
| --- | --- |
| FLTR 1 | the filter's envelope (ATTACK, DECAY, SUSTAIN, RELEASE), FREQ, RESONANCE, the filter's own parameter, ENV DEPTH |
| FLTR 2 | FLT DELAY, KEYTRACK, BASE and WIDTH (a base-width filter), BW ROUTE (before or after), ENV RESET, FILTER |
| AMP | ATTACK, HOLD, DECAY, SUSTAIN, RELEASE, RESET, MODE (AHD or ADSR), VOLUME |
| FX | BIT REDUCE, OVERDRIVE, SRR (sample-rate reduction), OD TYPE, the DELAY, REVERB and CHORUS sends, OD ROUTE |
| MANGLE | DESTROY and DST MODE, SLICE, SLICE RATE, SLC DEPTH, SLC EDGE, SLC SHIFT |

- **FILTER**: MULTI (multimode), LP4 (4-pole low-pass), LEGACY (low- or high-pass), COMB−, COMB+, EQ.
- **OD TYPE**: VALVE (two valve-style stages), RAT (a RAT-style clipper, darker as it rises), FOLD (a wavefolder,
  which bites only once the signal is loud). RAT and FOLD come from the LXR firmware.
- **DESTROY** breaks the sound digitally; DST MODE chooses how: WRAP, XOR, FLIP, DROP.
- **SLICE** is a gate in time with the tempo: sixteen patterns at 1/8, 1/16, 1/32 or 1/16 triplets, with its depth,
  edge and shift.

## Keys and scales

The white keys play the chosen track in the scale: the first key is the root, each key the next degree. The keys
that play the root glow dimly. USB and TRS MIDI notes play the chosen track as the keys do ([MIDI](#midi)).

### Scales

**ARP** opens the SCALE page: **SELECT** sets the root, **ALGORITHM** the scale. ARP again, or a track button, leaves
it. **SHIFT + ARP** draws a scale and a root at random and shows them for 1.5 s over any page. The scale and root
belong to the session; they can also be set on the session's page. A new session draws them at random.

The scales: CHROMATIC (the black keys play too, except the first five and the last three), MAJOR, MINOR NATURAL,
MINOR HARMONIC, DORIAN, PHRYGIAN, MINOR PENTA, MAJOR PENTA, WHOLE TONE, JAPANESE IN, MINOR MELODIC, ENIGMATIC,
LYDIAN, MIXOLYDIAN, LOCRIAN, BLUES, HUNGARIAN MINOR, DOUBLE HARMONIC, PHRYGIAN DOMINANT, JAPANESE HIRAJOSHI,
JAPANESE IWATO, PELOG, OCTATONIC, PROMETHEUS.

### Transposition

In the step view, **OCT− / OCT+** move the chosen track's steps and loops one step of the scale down or up (in
CHROMATIC, a semitone). **SHIFT + OCT− / OCT+**, in the track's view or the step view, move every track's. Notes stay
in the scale. The transposition shows at the top right, under the octave (T+2, T−1), up to 24 steps each way. It is
part of the pattern: saved with it, and copied with it. It moves only the loops of the pattern playing; a loop being
recorded is left as it is. The keys' octave does not change.

### CHORD mode

Black key 1 turns CHORD mode on or off for the chosen track (lit while on). Each white key then plays a chord from its
note, in the scale. **SHIFT +** black key 1 chooses the next form: TRIAD, INV 1, INV 2, SUS2, SUS4, OPEN, FOURTHS.
The form shows at the top left. Steps, recording and the looper take the chords. DRUM tracks play no chords.

### Arpeggiator

Black key 5 turns the chosen track's arpeggiator on or off (lit while on; ARP shows at the top left). The keys held,
and MIDI notes for the track, then play one at a time on the tempo's grid: with the sequencer's 16ths while it plays,
on their own at the tempo while it is stopped. With CHORD mode on, each key gives it the chord's notes.

**Hold black key 5** (0.8 s) to open or close the ARP page (KNOB 1–4 rows A to D; PRESETS right, E to H):

| Row | What it sets |
| --- | --- |
| MODE | UP, DOWN, UP DOWN, DOWN UP, ORDER (as played), RANDOM, CONVERGE (from the ends in), DIVERGE (from the middle out), PINKY (the top note between each of the others), THUMB (the lowest between each) |
| RATE | 1/32, 1/16T, 1/16, 1/8T, 1/8, 1/4 |
| RANGE | 1 to 4 octaves |
| GATE | each note's length, 0 to 100 |
| LATCH | on: the notes keep playing with the keys up, until the next chord |
| DENSITY | the chance that each note plays, 0 to 100 |
| OCT RND | the chance that a note jumps an octave, 0 to 100 |
| BASS | OFF, or EVERY 2 to EVERY 8: every so many notes, the lowest an octave down |

REC records the arpeggio into the steps and SHIFT + REC into a loop; with either armed, its first note starts the
sequencer. The arpeggiator and its settings are saved with the session and the pattern.

### MUTE mode

Black key 2 turns MUTE mode on or off (lit while on; the screen reads MUTE MODE). The track buttons then mute instead
of choosing:

| To | Do this |
| --- | --- |
| Mute a track, or bring it back | press its button (lit: playing; dim: muted) |
| Solo a track | hold its button 1 s |
| End the solo | hold it 1 s again: the mutes come back as they were |

A muted track fades out, its sends with it; its steps keep running silently. The mutes stay when MUTE mode is
turned off. They are saved with the pattern. MUTE mode also works on the PATTERNS grid.

### BREAK

While the sequencer plays, **hold black key 3** to break up the chosen track's pattern; **SHIFT +** it, every track's.
Nothing is written into the steps: when the key is let go, the pattern carries on from where it would have been.

Each press draws one way of playing the steps, shown on the screen, and keeps it while the key is held. The break
starts on the track's next beat, and what is on a beat stays on a beat:

| Way | What plays |
| --- | --- |
| REPEAT 1, 2, 4 or 8 | a window of that many steps, played over and over |
| HALF TIME | the steps at half speed |
| DOUBLE | the steps at double speed |
| REVERSE 4 or 8 | each window of 4 or 8 steps backwards, its first step in place |
| CHOP | the bar's four beats shuffled, the first in place |
| ROLL | the steps as written, with more ratchets |

On top of the way, a break may add ratchets (faster the longer the key is held), flams on drums, octave leaps on
melodic tracks, a gap at the end of each bar, and accents. A kick keeps the bar's first step and takes no leaps; drones
are left alone; loops play on as recorded.

**Saving a break.** BREAK also works on the PATTERNS grid. There, with black key 3 held and the sequencer playing, hold
a white key 0.8 s: the break, as it plays, is saved into that pattern slot (BREAK SAVED, PATTERN n). The pattern
playing goes on.

## The effects (SEQ)

**SEQ** shows CHORUS, COMP, DELAY and REVERB, then the chosen track's LFO 1, LFO 2 and LFO 3, in turn, then goes back.
**OCT− / OCT+** move between them. **KNOB 1–4** set the first four rows; **PRESETS** turned right shows the next four.
A track button goes back to the track; **SHIFT +** a track button chooses another track and stays on the page.

| Page | Rows |
| --- | --- |
| CHORUS | DEPTH, SPEED, HPF, WIDTH, DELAY (its send to the delay), REVERB, VOLUME |
| COMP | the master compressor: THRESHOLD, ATTACK, RELEASE, RATIO, MAKEUP, MIX; then the chosen track's SC SOURCE and SC LEVEL |
| DELAY | TIME (in time with the tempo), PING PONG, WIDTH, FEEDBACK, HPF, LPF, REVERB, VOLUME |
| REVERB | PRE, DECAY, FREQ, GAIN (the high shelf), HPF, LPF, VOLUME |

### Sidechain

On the COMP page, **SC SOURCE** chooses the track whose hits duck the chosen track (OFF: none) and **SC LEVEL** how
deep, 0 to 100; at 100 the track is all but silenced on each hit. The duck lets go at the compressor's RELEASE, and the
track's effect sends duck with it.

### LFOs

Each sound has three LFOs, on the pages LFO 1 to 3. They belong to the sound, so each machine's sound on a track has
its own, and they are saved with the session. All of a track's voices follow the same LFO.

| Row | What it sets |
| --- | --- |
| SPEED | −64 to +63; negative runs the wave backwards. SPEED +32 with MULT x4 is one bar |
| MULT | x1 to x2048 follow the tempo; .1 to .2048 run as at 120 BPM whatever the tempo |
| FADE | 0 none; negative fades the LFO in, positive fades it out, from each note |
| DEST | NONE, or a parameter of the sound's pages (S1–S4, F1, F2, AMP, FX, MNG). LFO 2 can also move LFO 1's SPEED or DEPTH, and LFO 3 those of LFO 1 or 2 |
| WAVE | TRI, SINE, SQR, SAW, EXP, RAMP, RND (a new random value each cycle) |
| START | where the wave starts on a note. For RND: how smoothly it moves to each new value |
| MODE | FREE runs on; TRIG starts again on each note; HOLD holds its value from each note to the next; ONE runs one cycle from each note; HALF half a cycle |
| DEPTH | −64 to +63, how far each way; negative turns the wave upside down; 0 off |

**SHIFT + SEQ** draws the chosen track's three LFOs anew (LFO RANDOM): two or three of them on, each to a different
parameter, with any wave, speed, mode and depth. It takes risks on purpose: press it again for another. LFO settings
cannot be step-locked.

## The sequencer

Each track has 16 steps, with its own length (1 to 16), clock (1/8, 1/4, 1/2, 1X, 2X, 4X) and direction. **PLAY**
starts and stops it.

### The step view

Black key 9 (**STEP**) turns the step view on or off. The white keys are then the chosen track's 16 steps: lit, a note;
dim, empty; the step playing the other way round; past the track's length, dark. Steps are silent while you edit them.

| To | Do this |
| --- | --- |
| Put a note in | press an empty step: the track's last note (or chord) goes there |
| Empty a step | press a step with a note, and let go |
| Lock a value on a step | hold the step and turn KNOB 1–4 (filter, sends), SELECT (level) or ALGORITHM (pan) |
| Lock a sound parameter | hold the step with the EDIT pages up and turn a knob; up to eight locks a step |
| Copy a step | SHIFT + hold it 0.8 s (COPY blinks) |
| Paste it | SHIFT + hold another step 0.8 s, on this track or another |
| Empty the whole track | SHIFT + STEP (with the step view on or off): its steps and locks; the rest of the track is kept |

The black keys, lit dimly, do this in the step view:

| Key | What it does |
| --- | --- |
| black key 1 | a new [phrase](#phrases) for the track. **SHIFT +**: new locks on its steps. **Held** (0.8 s): the PHRASE page |
| black key 2 | the TRACK page: LENGTH and CLOCK |
| black key 3 | TRIG on or off: a step held shows its own page |
| black key 4 | MOD on or off: a step held shows its MOD page |
| black keys 6, 7, 8 | the track's direction: REVERSE, PING-PONG, RANDOM; pressed again, forwards |

### A step's own pages

**TRIG**: KNOB 1 the step's note (in the scale), 2 its velocity, 3 its length, 4 its trig condition; with PRESETS turned
right, KNOB 1 its RATCHET (the step played 2 to 8 times). The conditions are the Digitone II's: PRE and !PRE (as the
last condition was met or not), NEI and !NEI (as the track before met its condition), 1ST and !1ST (the first time
through), probabilities, and A:B (the A-th time of every B).

**MOD**: KNOB 1 MUTATE (the note moved by up to N steps of the scale, or RND), 2 MUT EVERY (how often), 3 MIDI FX
(ECHO, ECHO UP, ECHO DN, STUTTER, FLAM, LATE, GRACE, FIFTH, TRIAD, OCT UP, OCT DN, DROP), 4 FX EVERY.

### Phrases

In the step view, black key 1 writes a new phrase for the chosen track: steps, notes, velocities, lengths, conditions,
ratchets and locks, made for the role of its sound (KICK, SNARE, HAT, PERC, NOISE, BASS, LEAD, STAB, DRONE). On the
PHRASE page, DENSITY and COMPLEXITY (0 to 100) thin it out or fill it.

## Recording

**REC** records the keys into the chosen track's steps, each note with its velocity and length, until REC again.
Stopped, REC arms (it flashes, the metronome counts in) and the first note starts the sequencer. **SHIFT + PLAY**
turns the metronome on or off.

## The looper

**SHIFT + REC** arms a loop on the chosen track; the first note starts it (and the sequencer, if stopped). **REC** ends
it. Its length is rounded to whole bars (1 to 32), so it stays in time with the steps (LOOP 1  4 BARS shows). Three
loops a track, up to 256 notes each.

| To | Do this |
| --- | --- |
| See the track's loops | hold SHIFT + REC: they show on the first three white keys (lit: playing; dim: muted) |
| Mute a loop, or bring it back | with SHIFT + REC held, press its key |
| Empty one loop | with SHIFT + REC held, hold its key 0.8 s |
| Empty all the track's loops | hold SHIFT + REC 1.5 s without touching a key |

A loop belongs to the pattern it was recorded in. Another pattern does not play it, and an empty pattern plays
nothing. When a pattern comes in, its loops start with its steps. Saving, copying or pasting a pattern takes its loops
along; emptying it removes them. All patterns share room for 40 loops and 4608 notes: when it is full, arming a loop
shows NO ROOM, and a pattern saved into another slot shows LOOPS LOST for the loops that did not fit.

## Tempo

**Hold PLAY** (0.8 s) for the TEMPO page: **PRESETS** sets the session's tempo, 10 to 300 BPM. A track button leaves
it.

## Patterns

A session has 16 patterns. A pattern holds every track's steps (with their length, clock, direction and
transposition), its loops, and each track's mute, machine, octave, level, pan, sidechain, CHORD mode and arpeggiator.
The sounds, the effects, the tempo, the scale and the root belong to the session.

**SAVE** opens the PATTERNS grid; SAVE again leaves it. The white keys are the 16 patterns, the one playing brighter.
On the screen the pattern playing is green (orange with changes not saved), saved patterns white, empty ones grey, and
the white square is the cursor: it opens on the pattern playing, and **PRESETS** moves it.

| To | Do this |
| --- | --- |
| Play a pattern | press and release its white key. While the sequencer plays, it starts at the end of the chosen track's steps |
| Save the pattern playing into a slot | hold the slot's white key 0.8 s, or move the cursor there and hold SAVE; the slot may be empty or not, and the pattern playing goes on |
| Save its changes in its own slot | hold its own white key 0.8 s |
| Copy a pattern | SHIFT + hold its key 0.8 s |
| Paste it | SHIFT + hold another key 0.8 s |
| Empty a pattern | hold SHIFT and black key 1, and press its key |
| Leave the grid | SAVE, or a track button |

A changed pattern keeps its changes when you switch away, until the session is saved or loaded. A pattern that
unmutes a track starts that track cleanly, without notes left over from the muted time. Every note is always saved;
on a pattern crowded with locks, the last locks may be left out. MUTE mode and BREAK work on the grid too, and MIDI
program changes 0 to 15 choose patterns 1 to 16 ([MIDI](#midi)).

## Sessions

16 sessions are kept in the FM-1's flash. A session holds everything: every track's sounds and settings, the effects,
the tempo, the scale and root, the 16 patterns with their loops, and the pattern playing.

**SHIFT + SAVE** opens the SESSIONS list; SHIFT + SAVE again leaves it. **PRESETS** chooses a slot and **SAVE** opens
its page: BACK, LOAD, SAVE, COPY, DELETE, TEMPO, ROOT, SCALE. SAVE runs the row chosen, or moves to its value (PRESETS
changes it, SAVE sets it, SHIFT + SAVE leaves it as it was). Loading over changes not saved, saving over another
session, copying over one and deleting ask first: NO or YES on PRESETS.

| To | Do this |
| --- | --- |
| Save the session playing | hold SAVE (or HOME) 0.8 s, away from the SESSIONS pages and the PATTERNS grid. Never saved, it offers the first empty slot (SAVE NEW?) |
| Load a session | its page, LOAD |
| Start a new session | SAVE on an empty slot's page: new sounds, a scale and root at random, tempo 120, no patterns or loops. It asks first if the session playing has changes not saved. With nothing saved yet, it saves what is playing there instead |
| Copy a session into another slot | its page, COPY, then the slot |
| Delete a session | its page, DELETE |

Loading a session brings all of it back; while the sequencer plays, its loops start at the next bar. Loops take room
in a store shared by all the sessions: if it is full, NO ROOM FOR LOOPS shows and nothing is saved; deleting a session
frees room. Sessions saved by older builds load as they played. Changes not saved before the FM-1 is switched off are
lost.

How the FM-1 starts is set in [GLOBAL SETUP](#global-setup) > START: with a new session (NEW) or the last session
loaded or saved (LAST).

### Backup

The website's [Backup page](https://nzimas.github.io/bigorna/backup.html) (desktop Chrome, Edge or Opera, over USB
MIDI) copies every session, with its patterns and loops, and GLOBAL SETUP to a file on the computer, and writes them
back. Use it before installing another firmware: Bigorna keeps its sessions where Tonitor keeps its own, so either may
overwrite the other's. A restore puts each session back into its own slot and leaves the others; RESTORED S3 (or
SETUP) shows as each lands.

## GLOBAL SETUP

**SHIFT + SAVE held** (0.8 s) opens GLOBAL SETUP: the FM-1's own settings, kept apart from the sessions. **PRESETS**
moves through the entries; **SAVE** opens one, or moves to a value and sets it; **SHIFT + SAVE** leaves a value as it
was or goes back a level. A track button leaves. Changes are saved as you leave.

**START**: how the FM-1 starts. **NEW** (at first): with a new session, as Tonitor starts: new sounds on every track, a
scale and root at random, tempo 120. Hold SAVE to keep it. **LAST**: with the last session loaded or saved.

### MIDI

MIDI goes over the USB cable both ways (the FM-1 shows up as FM-1 Bigorna) and comes in through the 3.5 mm MIDI IN jack
(TRS Type A). The settings follow the Model:Cycles' MIDI CONFIG:

| Part | Settings | At the start |
| --- | --- | --- |
| SYNC | CLK IN: follow a MIDI clock and its transport; CLK OUT: send them; PCH IN: a program change chooses the pattern; PCH OUT: send one when a pattern is chosen (ON / OFF) | all OFF |
| IN CHAN | TRACK 1 to 6: the channel each track listens on; AUTO: the channel that plays the chosen track; PCH: the program changes' channel | 1 to 6; AUTO 16; PCH 16 |
| OUT CHAN | TRACK 1 to 6: the channel each track sends on; PCH | 1 to 6; PCH 16 |
| RECEIVE | NOTES, CC (ON / OFF) | ON, ON |
| SEND | NOTES, CC (ON / OFF) | OFF, OFF |
| PORTS | USB IN, TRS IN: take MIDI from USB, from the jack (ON / OFF) | ON, ON |

**Clock in.** With CLK IN on, the FM-1 follows the clock that arrives: its tempo, START (from step 1), CONTINUE (from
the song position) and STOP. The sending device must send its transport as well as its clock (on Elektron machines,
CLOCK SEND and TRANSPORT SEND). The TEMPO page shows EXT TEMPO. If the clock stops for half a second, the FM-1 goes back
to its own tempo.

**Clock out.** With CLK OUT on, the FM-1 sends its clock all the time, so another device can lock to it before it
starts; PLAY sends START, stopping sends STOP.

**Notes.** A note on a track's IN channel plays that track; on the AUTO channel, the chosen track. Notes on the chosen
track are recorded as the keys are. With SEND NOTES on, every note a track plays (steps, loops, keys, notes received)
goes out on its OUT channel; a muted track sends nothing.

**CCs.** Each track answers to, and with SEND CC sends, these CCs on its channel: level 7, pan 10, mute 94 (64 and up
mutes), and each page's eight parameters (A to H), whatever the machine:

| Page | A | B | C | D | E | F | G | H |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| SYN 1 | 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 |
| SYN 2 | 24 | 25 | 26 | 27 | 28 | 29 | 30 | 31 |
| SYN 3 | 33 | 34 | 35 | 36 | 37 | 39 | 40 | 41 |
| SYN 4 | 42 | 43 | 44 | 45 | 46 | 47 | 48 | 49 |
| FLTR 1 | 50 | 51 | 52 | 53 | 54 | 55 | 56 | 57 |
| FLTR 2 | 58 | 59 | 60 | 61 | 62 | 63 | 70 | 71 |
| AMP | 72 | 73 | 74 | 75 | 76 | 77 | 78 | 79 |
| FX | 80 | 81 | 82 | 83 | 84 | 85 | 86 | 87 |
| MANGLE | 88 | 89 | 90 | 91 | 92 | 93 | 95 | 102 |

**Program changes** 0 to 15 choose patterns 1 to 16, in time as the PATTERNS grid does.

The jack takes everything USB does except SysEx. With both inputs on they are merged; a clock should come from only
one of them. A controller cannot be plugged straight into the FM-1's USB-C port (both are USB devices and need a
computer between them); one with a 5-pin MIDI OUT goes into the jack.

**Tested with:**

- **Elektron Digitone II, into the TRS jack** (a Roland TRS-to-DIN cable, Type A): the FM-1 follows its clock, starts
  and stops with it, and its keyboard plays the FM-1's tracks by channel. On the Digitone II, in SETTINGS > MIDI CONFIG:
  SYNC > CLOCK SEND and TRANSPORT SEND on; PORT CONFIG > OUTPUT TO MIDI (or MIDI+USB), TRIG KEY DEST INT + EXT, OUTPUT
  CH TRK CH (or AUTO CH). On the FM-1: GLOBAL SETUP > MIDI > SYNC > CLK IN on.
- **A computer over USB** (macOS): notes, CCs, and MIDI LEARN with an M-VAVE SMC Mixer in MIDI mode.

### MIDI LEARN

Black key 4 turns MIDI LEARN on or off (lit while on; the screen reads MIDI LEARN). The EDIT pages and the effects'
pages then show, in each row, the CC learned for it, or -- if none. **PRESETS** chooses the row; **OCT− / OCT+** turn
the pages.

| To | Do this |
| --- | --- |
| Learn a CC | SAVE (WAIT shows), then move the control: the next CC to arrive, on any channel, is learned |
| Stop waiting | SAVE again |
| Forget the row's CC | SHIFT + SAVE |

A learned sound parameter moves on the track it was learned on; an effect's moves for all. Each CC and channel
controls one parameter, and each parameter has one CC. Up to 64 are kept, with GLOBAL SETUP. RECEIVE CC must be on.

## USB audio

Plugged into a computer, a phone or a tablet, the FM-1 is also a USB audio input named FM-1 Bigorna: its master output
in stereo, 16-bit, 44.1 kHz. Record or monitor it like any class-compliant audio interface; nothing to install. The
headphone jack plays on as usual. The USB sound does not follow the MASTER knob, so a recording keeps its level
whatever the headphones are set to. MIDI goes on alongside on the same cable.

The other end must be a USB host: a computer, or a phone or tablet that takes USB audio devices (on Android, most do
through USB-C). A host that works at 48 kHz converts the sound itself.

**Tested with:**

- **POCO X7 Pro (HyperOS), over USB-C:** the phone's camera app records video with the FM-1's sound. Switch on OTG
  first (Settings > Additional settings > OTG), with nothing plugged into the phone; HyperOS turns it off again after
  10 minutes with no device.
- **MacBook Air M5 (macOS), over USB-C:** FM-1 Bigorna, 2 input channels, 44,100 Hz, every packet in order with six
  tracks playing.

## The screen

Few words, very large type, high contrast, and a colour for each track. The track's view shows, from the top: the
pattern playing (orange with changes not saved) and the chord form (or ARP) at the top left; the battery, the octave
and the transposition (T+2) at the top right, with REC or LOOP while recording; the track's number, its machine and
its sound's family.

The battery shows three green bars when full, then two green, one amber, and one red when it runs low. On USB power it
shows a white bolt instead.

## Rescue

Hold **OCT−** while switching the FM-1 on for the rescue screen, from which the installer can run again; two failed
starts in a row also lead there. If the FM-1 starts but no longer answers over USB, hold **OCT−** and **OCT+** for 5
seconds while it runs: the chip's own update mode, from which a firmware can be restored with
[Melodee's recovery tool](https://github.com/keremimo/melodee/blob/main/docs/RECOVERY.md).

# Building from source

Needs macOS or Linux, Python 3, and on macOS Docker Desktop (JieLi's toolchain is Linux x86-64 only).

```
./setup.sh                  # once: a Python environment, JieLi's toolchain, three files of the AC79 SDK
./build.sh                  # writes build/bigorna.fwsc
./install.sh --no-build     # installs it on the FM-1 connected over USB
tests/run_tests.sh          # the host tests: no FM-1 or toolchain needed
```

The host tests build the firmware for the computer and render every sound recipe through the real mixer (checked
for silence, clipping, DC and runaway tails), run the sessions, patterns and controls through their paths, and draw
the screens to `build/screens`. Versions are build numbers: `BUILD` in `tools/build.py`.

`tools/build.py --telemetry` builds a diagnostic version that reports the audio's timing over USB MIDI;
`tools/telelog.py` reads it.

# Credits

- Built on **Tonitor** (Nuno Zimas), itself built on **Felucca** (Leo Kuroshita, Hügelton Instruments) and **SLOOP**
  (isod89): the hardware layer, the USB stack, the update loader and the installer.
- The dual-core start, its fault handling and the stock supply setup are from **Melodee** (Kerem Kilic, Ellic
  Studio).
- The sound recipes are ported from **SynthSeeder**, the same author's patch generator for the Digitone II.
- The website and installer page follow Tonitor's.

# Licence

The firmware is GPL-3.0-only (`LICENSE`); the website and the logo are MIT. See `LICENSING.md` for each part's
origin and licence.

# Disclaimer

Bigorna is an unofficial firmware. It is not made, endorsed or supported by M-VAVE or Elektron. M-VAVE and FM-1 are
trademarks of their owner; Elektron and Digitone are trademarks of Elektron Music Machines. They are used only to say
which device Bigorna runs on and which instrument it takes as its model. No Elektron code or data is in this
firmware.

Installing it replaces the FM-1's firmware and may void any warranty that applies to your device. An interrupted
update can leave the FM-1 unable to start until it is recovered. The software is provided as is, without warranty
of any kind; you install and use it entirely at your own risk.
