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

## The panel

| Control | In the track's view |
| --- | --- |
| The six top buttons | Tracks 1 to 6: the lit one is chosen; the others glow dimly while they sound |
| PRESETS (knob) | The track's machine: 4-OP, DRUM, WAVY, HIVE |
| SHIFT + PRESETS | A new sound of the family before / after |
| HOME | A new sound for the track, on its machine; SHIFT + HOME: of the same family. Held: the session saved, as SAVE held |
| OCT− / OCT+ | The track's octave, −3 to +3 (shown at the top right); in the step view, the track's transposition; SHIFT + OCT− / OCT+: every track's transposition |
| White keys | Play the track in the scale chosen |
| 1st black key | CHORD mode on or off |
| 2nd black key | MUTE mode on or off |
| 3rd black key | BREAK, while held; SHIFT + it: on every track |
| 4th black key | MIDI LEARN on or off |
| 5th black key | The track's arpeggiator on or off; held 0.8 s: the ARP page |
| Black key before the last | EDIT: the sound's pages |
| 3rd black key from the right | STEP: the step view |
| Last black key | SHIFT |
| KNOB 1 to 4 | The filter's FREQ and RESO, the delay and reverb sends; with SHIFT, KNOB 3 the chorus send, KNOB 4 the overdrive |
| SELECT (knob) | The track's level |
| ALGORITHM (knob) | The track's pan |
| PLAY | The sequencer plays or stops; held 0.8 s, the TEMPO page; SHIFT + PLAY: the metronome on or off |
| REC | Records the keys into the track's steps; SHIFT + REC: the looper |
| ARP | The patterns |
| SEQ | The effects' pages, then the track's LFOs |
| SAVE | The sessions; held 0.8 s, the session saved; SHIFT + SAVE: GLOBAL SETUP (MIDI) |
| MASTER | The output level |

While a knob turns, the screen shows its value as a large dial for a second.

## Tracks, machines and voices

Six tracks, each with its own machine, sound, octave, level, pan, steps and loops. Each track keeps a sound for each
of the four machines, so turning PRESETS back finds the sound it had.

Twelve voices are shared by the tracks: a track takes as many as its notes need. When all twelve sound, a new note
takes the quietest one fading out. The FM-1's second processor core renders half the voices and the send effects;
should it ever fault, it is stopped and the first core carries on alone until the FM-1 is switched off (the screen
says **1 CORE**).

DRUM tracks choke: each hit fades the track's previous one, as on a drum machine.

## Sounds

HOME draws a new sound for the track from a recipe of its machine — a family such as TIGHT KICK, METAL HAT, FM
BASS or SWARM DRONE — its values drawn within the recipe's ranges, leaning each time towards industrial, IDM or
abstract. SHIFT + HOME draws again from the same family. SHIFT + PRESETS moves to the family before or after. The
screen shows the machine and the family. 56 families: 29 for DRUM, 9 for 4-OP, 10 for WAVY, 8 for HIVE. The
drone, pad and texture families and FM LEAD set LFOs as SynthSeeder's recipes do (those on PAN left out); the other
families, kicks aside, may get a sample-and-hold LFO on the filter's FREQ.

## The sound's pages (EDIT)

EDIT shows the sound's pages: SYN 1 to 4 (the machine's), FLTR 1, FLTR 2, AMP, FX and MANGLE. Each page has eight
parameters: KNOB 1 to 4 set A to D; PRESETS turned right shows E to H (left: back). OCT− and OCT+ move between
the pages. EDIT again leaves.

### 4-OP

Four-operator FM after the Digitone II's FM TONE: operator C and the modulators A, B1 and B2.

- SYN 1: ALGO (eight algorithms), RATIO C, RATIO A, RATIO B, HARM, DETUNE, FEEDBACK, MIX
- SYN 2: the A and B envelopes — ATK, DEC, END, LEV for each
- SYN 3: DELAY, TRIG and RESET for A and for B, PHASE RST
- SYN 4: OFS C, OFS A, OFS B1, OFS B2 (fine offsets), KEYTRK A, KEYTRK B1, KEYTRK B2

### DRUM

Three-operator FM percussion after FM DRUM.

- SYN 1: TUNE, SWP TIME, SWP DEPTH (the pitch sweep), ALGO (seven), OP C WAVE, OP AB WAVE, FEEDBACK, FOLD
- SYN 2: RATIO, DEC, END and MOD for A and for B
- SYN 3: HOLD, DECAY (the body), PHASE C, LEVEL, NOISE RST, NOISE RM
- SYN 4: NS HOLD, NS DECAY, TRANS (the transient's kind), TRANS LEV, NS BASE, NS WIDTH, NS GRAIN, NS LEVEL

### WAVY

Two wavetable oscillators with phase distortion, and a noise generator.

- SYN 1: TUNE, WAVE, PD and LEVEL for each oscillator
- SYN 2: OFS 1, TABLE 1, MOD (OFF, RING, RING FIX, SYNC), RESET, OFS 2, TABLE 2, DRIFT
- SYN 3: NS ATK, NS HOLD, NS DECAY, NS LEVEL, NS BASE, NS WIDTH, NS TYPE (GRAIN, TUNED, S+H), NS CHAR

### HIVE

A swarm of six detuned oscillators and a main oscillator.

- SYN 1: TUNE, SWRM WAVE, DETUNE, MIX, MAIN OCT (−2, −1, 0), MAIN WAVE, ANIMATION, NOISE MOD

### FLTR 1, FLTR 2

- FLTR 1: the filter's envelope (ATTACK, DECAY, SUSTAIN, RELEASE), FREQ, RESONANCE, the filter's own parameter,
  ENV DEPTH
- FLTR 2: FLT DELAY, KEYTRACK, BASE and WIDTH (the base-width filter), BW ROUTE (before or after), ENV RESET,
  FILTER: MULTI (multimode), LP4 (4-pole low-pass), LEGACY (low- or high-pass), COMB−, COMB+, EQ

### AMP and FX

- AMP: ATTACK, HOLD, DECAY, SUSTAIN, RELEASE, RESET, MODE (AHD or ADSR), VOLUME
- FX: BIT REDUCE, OVERDRIVE, SRR (sample-rate reduction), the DELAY, REVERB and CHORUS sends, OD ROUTE

### MANGLE

Per track, on the track's sound before its level:

- DESTROY and DST MODE: digital destruction — WRAP (the sample wraps round), XOR, FLIP (a bit flipped), DROP
  (samples dropped)
- SLICE, SLICE RATE, SLC DEPTH, SLC EDGE, SLC SHIFT: a slicer gate synced to the tempo — sixteen patterns at 1/8,
  1/16, 1/32 or 1/16 triplets, how deep, how soft its edges, shifted in steps

## Playing

The white keys play the track in the scale chosen: the first key is the root, each the next degree. The scale and
root are set on the session's page (SAVE): CHROMATIC (the black keys play too, but for the first five),
MAJOR, MINOR NATURAL, MINOR
HARMONIC, DORIAN, PHRYGIAN, MINOR PENTA, MAJOR PENTA, WHOLE TONE, IN. OCT− and OCT+ move the track's octave. USB
MIDI notes play the chosen track as the keys do.

### Transposition

In the step view, OCT− and OCT+ transpose the chosen track's steps and its loops; SHIFT + OCT− or OCT+ (in the
track's view or the step view) transposes every track's steps and loops at once. Steps and loops move by the same
amount, each from the notes it plays. Each press moves every note one step of the scale and root
chosen, down or up (in CHROMATIC, a semitone), so the notes stay in the scale; a note that is not in it moves to the
scale's next. The transposition shows at the top right, under the octave, in the track's colour (T+2, T−1), up to 24 steps each way,
and TRN with its value on the screen as it changes. It is part of the pattern: the moved notes and the count are
saved with it and with the session, and a copied pattern takes them along. A loop's notes are moved in place and
saved with the session (loops belong to the session, not to a pattern; a loop being recorded is left as it is). The
keys' octave is left as it is.

### CHORD mode

In the track's view, the step view off, the 1st black key turns CHORD mode on or off for the track (lit while on,
dim off). Each white key then plays a triad from its note, in the scale: TRIAD, INV 1, INV 2, SUS2, SUS4, OPEN or
FOURTHS (in CHROMATIC, the same shapes in semitones). SHIFT and the key choose the next form. The form shows at the
top left. A step put in takes the chord last played; REC records chords into the steps; the looper records their
notes. DRUM tracks play no chords.

### Arpeggiator

In the track's view, the step view off, a short press of the 5th black key turns the track's arpeggiator on or off
(lit while on; ARP shows at the top left). While it is on, the keys held (and USB or TRS MIDI notes for the track) are not played
directly: the arpeggiator plays them one at a time, on the tempo's grid. While the sequencer plays it follows the
sequencer's 16ths; stopped, it runs on its own at the tempo, and the first note of a chord plays at once. With CHORD
mode on, each key gives it the chord's three notes.

Holding the 5th black key for 0.8 s opens the ARP page of the chosen track, or closes it (KNOB 1 to 4 the rows A to
D, PRESETS turned right E to H; a track's button also goes back). The arpeggiator turns on or off as the key is let
go after a short press.

- MODE: UP, DOWN, UP DOWN, DOWN UP (the end notes once), ORDER (as played), RANDOM, CONVERGE (from the ends in),
  DIVERGE (from the middle out), PINKY (the top note between each of the others), THUMB (the lowest between each).
- RATE: 1/32, 1/16T, 1/16, 1/8T, 1/8, 1/4.
- RANGE: 1 to 4 octaves.
- GATE: each note's length, 0 to 100 of the rate's.
- LATCH: on, the notes keep playing with the keys up; the next chord played replaces them. Turning it off lets them
  go.
- DENSITY: 0 to 100, the chance that each note plays (otherwise a rest).
- OCT RND: 0 to 100, the chance that a note jumps an octave up or down.
- BASS: OFF, or EVERY 2 to EVERY 8: every so many notes, the lowest note held an octave down.

The arpeggio is recorded as played notes are: REC puts each note into the step it falls on, with its velocity and
its length; SHIFT + REC records it into a loop. With REC or the looper armed, the arpeggiator's first note starts the
sequencer. Each track's arpeggiator, on or off, and its settings are saved with the session; sessions from earlier
builds load with it off.

### MUTE mode

In the track's view, the step view off, the 2nd black key turns MUTE mode on or off (lit while on; the name band
reads MUTE MODE). The track buttons then mute instead of choosing: lit, a track plays; dim, it is muted. A button
let go soon mutes its track or plays it again. Held 1 s, its track is soloed, every other muted; held 1 s again,
the mutes come back as they were. MUTE mode off, the mutes stay, and the buttons choose tracks again. A muted track
fades out over ~9 ms, its sends with it; its steps keep playing silently. The chosen track's number is dark grey
while it is muted. Mutes are kept with the session.

### BREAK

While the sequencer plays, holding the 3rd black key (the step view off) breaks up the chosen track's pattern;
SHIFT and the key, every track at once. Nothing is written into the steps: each track's position keeps running
underneath, and when the key is let go the pattern carries on from where it would have been; ratchets and echoes
the break had started stop at once. The key is lit while held.

The break stays on the pattern's beat. A track goes into it on its next beat (its next half bar for REPEAT 8) and
plays as written until then. Each press draws one way of playing the steps, shown in the name band, and keeps it
for as long as the key is held; the next press draws another (never the same twice running). Every way keeps what
is on a beat on a beat:

| Way | What plays |
| --- | --- |
| REPEAT 1, 2, 4 or 8 | A window of that many steps, from the beat the break began on, played over and over |
| HALF TIME | The steps from that beat at half speed |
| DOUBLE | The steps from that beat at double speed, wrapping round the track |
| REVERSE 4 or 8 | Each window of 4 or 8 steps backwards after its first step, which stays in place (0 3 2 1) |
| CHOP | The bar's four beats shuffled; the first beat stays in place |
| ROLL | The steps as written, with more ratchets |

Over the way, some of these are drawn too:

- ratchets (2 to 6 hits a step), faster the longer the key is held; not on a step that has its own ratchet;
- flams on the drums;
- octave leaps, up or down, on the melodic tracks;
- a gap: every track but the kick silent for the last 2 or 4 steps of each bar;
- accents: the steps on the beat full, the ones between softer.

The role of each track's sound (as the phrase generator reads it) keeps the result coherent: a kick keeps the
bar's first step and takes no leaps, no gap and few ratchets; a drone (HIVE's pads and drones, dark washes) is
left alone. The looper's loops are not broken up: they play on as recorded, and the pattern falls back in with
them when the key is let go.

## The effects (SEQ)

SEQ shows CHORUS, COMP, DELAY, REVERB, then the chosen track's LFO 1, LFO 2 and LFO 3 in turn, then goes back (OCT−
and OCT+ move between them; a track's button goes back to the track's view). KNOB 1 to 4 set the first four rows; PRESETS turned right shows the next four.

- CHORUS: DEPTH, SPEED, HPF, WIDTH, DELAY (its send to the delay), REVERB, VOLUME
- DELAY: TIME (synced to the tempo), PING PONG, WIDTH, FEEDBACK, HPF, LPF, REVERB, VOLUME
- REVERB: PRE, DECAY, FREQ, GAIN (the high shelf), HPF, LPF, VOLUME
- COMP: the master compressor — THRESHOLD, ATTACK, RELEASE, RATIO, MAKEUP, MIX — then the chosen track's SC SOURCE
  and SC LEVEL

### Sidechain

Any track can be ducked under another's hits: SC SOURCE chooses the track whose notes duck it (OFF: none), SC
LEVEL how deep, 0 to 100 — at 100 the track is all but gated on each hit. The duck follows the source's level and
lets go at the compressor's RELEASE; the track's delay, reverb and chorus returns duck with it. The two rows show
in the chosen track's colour; SHIFT and a track's button choose another track while staying on the page.

### LFOs

Each track's sound has three LFOs, on the pages LFO 1, LFO 2 and LFO 3 after REVERB, shown in the track's colour
(SHIFT and a track's button choose another track while staying on the page). They are part of the sound: each
machine's sound on a track has its own, saved with the session. An LFO runs once for the track, and all the track's
voices follow it (the Digitone II runs one per voice).

- SPEED: −64 to +63; negative runs the wave backwards. One cycle takes 2048 / (|SPEED| × MULT) 16ths: SPEED +32 with
  MULT x4 is one bar.
- MULT: x1 to x2048 follow the tempo; .1 to .2048 run as at 120 BPM whatever the tempo.
- FADE: 0 none; negative fades the LFO in, positive fades it out, over |FADE| 16ths from each note.
- DEST: NONE, or a parameter of the sound's pages, named with its page (S1 to S4 the machine's SYN pages, F1, F2,
  AMP, FX, MNG; the page left out where the name would not fit). LFO 2 can also move LFO 1's SPEED or DEPTH, and
  LFO 3 those of LFO 1 or LFO 2. Parameters the machine does not have are skipped.
- WAVE: TRI, SINE, SQR, SAW, EXP, RAMP, RND (a new random value each cycle).
- START: where the wave starts on a note (TRIG, ONE, HALF), 0 to 127 a whole cycle. For RND: how slowly it moves to
  each new value (below 48 it jumps).
- MODE: FREE runs on; TRIG starts again at START on each note; HOLD runs on, but its value is held from each note to
  the next; ONE runs one cycle from each note and stops; HALF half a cycle.
- DEPTH: −64 to +63, the swing each way as a share of the destination's range (+32: half the range); negative
  turns the wave upside down; 0 off.

The LFOs are worked out every 2.9 ms. The track's sound is then made anew with the LFOs' values, one sound at a
time in turn: with LFOs on several tracks (and voices with step locks), each moves in steps of 2.9 ms times their
number. The LFOs' parameters cannot be step-locked.

## The sequencer

Each track has 16 steps, its own length (1 to 16), clock (1/8, 1/4, 1/2, 1X, 2X, 4X the tempo's sixteenths) and
direction. PLAY starts and stops it.

### The step view

STEP turns the step view on or off. The white keys are the track's 16 steps (lit: a note; dim: empty; the step
playing shows the other way round; past the track's length, dark):

- a key on an empty step puts the track's last note (or chord) there
- a key on a step with a note, let go soon and nothing changed, empties it
- a step held: the knobs lock what they set in the track's view (FREQ, RESO, the sends; SHIFT: chorus,
  overdrive), SELECT locks the level, ALGORITHM the pan
- a step held with the EDIT pages up: the knobs lock any parameter of the sound on the page; up to eight locks a
  step, shown in the track's colour
- SHIFT and a step held 0.8 s: copied (COPY and its number blink); SHIFT and another held 0.8 s: pasted there, on
  this track or another

The black keys, lit dimly, do this in the step view:

| Key | What it does |
| --- | --- |
| 1st | A new phrase for the track; SHIFT and it: new locks on its steps. Held 0.8 s: the PHRASE page (DENSITY, COMPLEXITY) |
| 2nd | The TRACK page: LENGTH and CLOCK |
| 3rd | TRIG on or off: a step held shows its own page |
| 4th | MOD on or off: a step held shows its MOD page |
| 6th, 7th, 8th | The track's direction: REVERSE, PING-PONG, RANDOM (again: forward) |

### A step's own pages

TRIG: KNOB 1 the step's note (in the scale), 2 its velocity, 3 its length, 4 its trig condition; PRESETS right,
KNOB 1 its RATCHET (the step played 2 to 8 times). The conditions, as the Digitone II's: PRE and !PRE (as the last
condition was met or not), NEI and !NEI (as the track before met its condition), 1ST and !1ST (the first time
through), probabilities, and A:B (the A-th time of every B).

MOD: KNOB 1 MUTATE (the note moved −N to +N steps of the scale, or RND), 2 MUT EVERY (every so many times the step
plays), 3 MIDI FX — ECHO, ECHO UP, ECHO DN, STUTTER, FLAM, LATE, GRACE, FIFTH, TRIAD, OCT UP, OCT DN, DROP — 4 FX
EVERY.

### Phrases

The 1st black key in the step view writes a new phrase for the track: steps, notes, velocities, lengths, conditions,
ratchets and locks, from the role of the sound the track plays (KICK, SNARE, HAT, PERC, NOISE, BASS, LEAD, STAB,
DRONE), with displaced accents, ghosts on probabilities and A:B variations. DENSITY and COMPLEXITY (the PHRASE page,
0 to 100) thin it out or fill it, and take locks away or add more.

## Recording

REC records the keys into the chosen track's steps. Stopped: REC arms (it flashes, the metronome counts) and the
first note played starts the sequencer on step 1. Playing: recording starts at once. Each note goes to the nearest
step with its velocity and its length as held, until REC again. SHIFT + PLAY turns the metronome on or off.

## The looper

SHIFT + REC arms a new loop on the chosen track; the first note played starts it (and the sequencer, if stopped).
Every note played after goes into it, at the nearest step, with its velocity and length, until REC ends it. Its
length is then the nearest whole number of bars (16 steps a bar; 1 bar at least, 32 at most), so it never drifts
from the patterns: it plays round in that length over the track's steps, its first step where its first note was,
carrying on from where it is (LOOP 1  4 BARS shows). Three loops a track, up to 256 notes each (a
chord counts its three notes). A note can be held for the whole loop.

SHIFT + REC held shows the track's loops on the first three white keys (lit: playing; dim: muted): a key let go soon
mutes its loop or plays it again; held 0.8 s, empties it. SHIFT + REC held 1.5 s with no key touched empties every
loop of the track.

## Tempo

PLAY held 0.8 s shows the TEMPO page: PRESETS sets the session's tempo (10 to 300 BPM). A track's button or another
page takes it down.

## Patterns (ARP)

A session has 16 patterns: every track's steps, lengths, clocks and directions. ARP shows the PATTERNS grid; the
white keys are the 16 patterns (dim; the one playing brighter). A key let go soon loads its pattern — while the
sequencer plays, at the end of the chosen track's steps. Held 0.8 s, copies it; another held 0.8 s pastes it there;
SHIFT and held 0.8 s empties it. ARP held saves the pattern playing. A pattern keeps its edits when another is
loaded, until the session is saved or loaded.

## Sessions (SAVE)

16 sessions are kept in the FM-1's flash. SAVE shows the list; PRESETS moves through it, SAVE opens a session's page:
BACK, LOAD, SAVE, COPY, DELETE, TEMPO, ROOT, SCALE (it opens on LOAD; SAVE on BACK goes back to the list). SAVE runs the row chosen, or moves the focus to its value (PRESETS
changes it, SAVE sets it, SHIFT + SAVE leaves it). Loading over changes not saved, saving over another session,
copying over one and deleting ask first (NO / YES on PRESETS).

SAVE held 0.8 s (or HOME held), outside the sessions' pages, saves the session playing into its slot at once. If it has never been saved, the
first empty slot's page shows SAVE NEW? with YES chosen: SAVE saves it there and goes back to the track (PRESETS
left: NO). With all 16 slots taken, NO FREE SLOT shows.

A session holds the whole state of the machine: every track's machine, sounds, octave, level, pan, chord mode and
mutes, the track chosen, the effects, the tempo, the root and scale, its 16 patterns and the pattern playing, and
every loop on every track (notes, length, mute). Loading a session gives all of it back; its loops start at the
next bar if the sequencer is playing. A session saved by build 37 or earlier has no loops: loading it empties them.

The loops are kept in a loop store shared by the 16 sessions (31 sectors of flash, about 125 KB). A session takes
what its loops need: up to 7 sectors with all 18 loops full (256 notes each). If the store has no room left,
nothing is saved and NO ROOM FOR LOOPS shows; deleting a session, or saving one with fewer loops, frees room.

The FM-1 starts with the last session loaded or saved (SESSION n shows for a moment). Changes not saved before it
was switched off are not kept.

### Backup

The website's [Backup page](https://nzimas.github.io/bigorna/backup.html) copies the sessions on the FM-1 — each
with its patterns and loops — and GLOBAL SETUP to a file on the computer, over USB MIDI (desktop Chrome, Edge or
Opera), and restores them from it. Use it before installing another firmware: Bigorna keeps its sessions where
Tonitor keeps its own, so either may overwrite the other's. A restore writes each session back into the slot it
came from (the others on the FM-1 are kept), each part checked before it is written; RESTORED S3 (or SETUP) shows
on the screen as each lands. The session playing is not changed: load it to hear what was restored. Build 55 or
later.

## GLOBAL SETUP (SHIFT + SAVE)

SHIFT + SAVE (outside the sessions' pages) opens GLOBAL SETUP: the settings of the machine rather than of a session.
They are kept in the FM-1's flash and read at every start. PRESETS moves through a list and SAVE goes into the entry
chosen; on a setting, SAVE moves the focus to its value, PRESETS changes it and SAVE sets it. SHIFT + SAVE leaves a
value as it was, or goes back a level; from the top it leaves GLOBAL SETUP, as a track's button does. Every level
below the top begins with BACK: SAVE on it goes back a level (a level opens on the entry below BACK). Changes are
written to the flash as it is left.

### MIDI

MIDI goes over the USB cable, both ways (the FM-1 shows up as FM-1 Bigorna), and comes in through the FM-1's 3.5 mm
MIDI IN jack (TRS Type A: a Type A cable or adapter from the other device's 5-pin MIDI OUT; tested with a Roland
TRS-to-DIN cable from a Digitone II). The settings follow the Model:Cycles'
MIDI CONFIG:

| Part | Setting | Values | Start value |
| --- | --- | --- | --- |
| SYNC | CLK IN: follow a MIDI clock, and its START, CONTINUE and STOP | ON / OFF | OFF |
| SYNC | CLK OUT: send a MIDI clock, and START and STOP | ON / OFF | OFF |
| SYNC | PCH IN: a program change chooses the pattern | ON / OFF | OFF |
| SYNC | PCH OUT: send a program change when a pattern is chosen | ON / OFF | OFF |
| IN CHAN | TRACK 1 to 6: the channel each track listens on | OFF, 1 to 16 | 1 to 6 |
| IN CHAN | AUTO: the channel that plays the track chosen | OFF, 1 to 16 | 16 |
| IN CHAN | PCH: the channel program changes are taken on | 1 to 16 | 16 |
| OUT CHAN | TRACK 1 to 6: the channel each track sends on | OFF, 1 to 16 | 1 to 6 |
| OUT CHAN | PCH: the channel program changes are sent on | 1 to 16 | 16 |
| RECEIVE | NOTES, CC: take notes and CCs in | ON / OFF | ON |
| SEND | NOTES, CC: send notes and CCs out | ON / OFF | OFF |
| PORTS | USB IN, TRS IN: take MIDI from USB, from the jack | ON / OFF | ON |

Clock in. The sending device has to send its transport (start and stop) as well as its clock; on Elektron machines
these are two settings, CLOCK SEND and TRANSPORT SEND. With CLK IN on and a clock arriving, the FM-1 follows it: the tempo is the average of the last 24 pulses
(one beat), and the sequencer is held to the pulses themselves, so it never drifts from the master and the jitter of
the cable does not add up. START starts the sequencer from step 1 on the next pulse; CONTINUE starts it from the song
position last received (each track where it would be by then on its clock and length, the loops too); STOP stops
it. PLAY on the FM-1 starts on the master's next pulse. The TEMPO page (PLAY held) shows EXT TEMPO and the
master's. If no pulse comes for half a second, the FM-1 goes back to its own tempo.

Clock out. With CLK OUT on, the FM-1 sends 24 pulses a beat from its sequencer, stopped too, so a slave can lock to
the tempo before it starts. PLAY sends a song position of 0, START and the first pulse together, as step 1 plays;
stopping sends STOP. A pulse is placed to within one audio block (0.73 ms); USB then delivers it in its own 1 ms
frames.

Notes. A note received on a track's IN channel plays that track; on the AUTO channel, the track chosen. Notes on the
chosen track are recorded (REC, the looper) as the keys are. With SEND NOTES on, every note a track plays (its steps,
ratchets and MIDI FX, its loops, the keys, notes received) is sent on its OUT channel with its velocity, and its note
off when it ends. A muted track sends nothing; stopping the sequencer sends the note offs still due.

CCs. Received on a track's channel (or AUTO), and sent on its OUT channel with SEND CC as its knobs turn: the level
is CC 7, the pan CC 10, the mute CC 94 (0 to 63 plays, 64 to 127 mutes), and each page's eight parameters (A to H,
as the knobs show them) these CCs, whichever machine the track plays:

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

The CCs a keyboard or a DAW sends for other things are left out: 32, 38, 64 to 69 (pedals), 96 to 101. A value
from 0 to 127 is spread over the parameter's whole range.

Program changes 0 to 15 choose the session's patterns 1 to 16, in time as the patterns' grid does.

### Tested with

MIDI has been tested on the FM-1 with:

- **Elektron Digitone II, into the TRS MIDI IN jack** (a Roland TRS-to-DIN cable, Type A, from the Digitone II's 5-pin
  MIDI OUT): the FM-1 follows its clock, starts and stops with it, and its keyboard plays the FM-1's tracks by
  channel (track 6 on channel 6). The Digitone II needs, in SETTINGS > MIDI CONFIG: SYNC > CLOCK SEND and TRANSPORT
  SEND on; PORT CONFIG > OUTPUT TO MIDI (or MIDI+USB), TRIG KEY DEST INT + EXT, OUTPUT CH TRK CH (or AUTO CH, which the
  FM-1 takes on its AUTO channel). On the FM-1: GLOBAL SETUP > MIDI > SYNC > CLK IN on.
- **A computer over USB** (macOS): notes, CCs, and MIDI LEARN with CCs as the M-VAVE SMC Mixer sends them in MIDI
  mode.

A controller cannot be plugged straight into the FM-1's USB-C port: both are USB devices, and USB needs a computer
(a host) between them. A controller with a 5-pin MIDI OUT goes into the TRS jack.

This list grows as more gear is tried.

### MIDI LEARN

The 4th black key (the step view off) turns MIDI LEARN on or off; it is lit while on, and MIDI LEARN shows in the
name band. With it on, the EDIT pages and the effects' pages (SEQ) show CC instead of the page count, and in each
row, where the value goes, the CC learned for it in amber, or -- if none. PRESETS moves through the page's rows, A to
H (the half shown follows); OCT− and OCT+ turn the pages as before.

- SAVE: the chosen row waits for a CC (WAIT). The next CC to come in, from USB or the jack, on any channel, is
  learned for it, and its number shows. SAVE again stops waiting.
- SHIFT + SAVE: the chosen row's CC forgotten.

A CC learned for a sound's parameter turns it on the track it was learned on; one learned for an effect's turns it
for all (COMP's SC SOURCE and SC LEVEL, and the LFOs' rows: on the track it was learned on; an LFO's DEST cannot be
learned). A CC and channel are learned for one
parameter at a time, and a parameter has one CC: learning again moves it. A learned CC comes before the CC map
above, on its channel; it needs RECEIVE CC on. Up to 64 are kept, in the flash with GLOBAL SETUP, written when MIDI
LEARN is turned off.

The jack takes notes, CCs, program changes, the clock, START, CONTINUE, STOP and the song position, as USB does
(not SysEx). With both inputs on, the two are merged; a clock should come from one of them only.

## USB audio

The FM-1 is also a USB audio device: plugged into a computer, a phone or a tablet, it shows up as an audio input
named FM-1 Bigorna, carrying its master output in stereo, 16-bit, at 44.1 kHz -- the rate it is made at, sent as it
is. Record it or monitor it like any
class-compliant audio interface; nothing to install. Its MIDI stays as it was, on the same cable. The FM-1's own
output (the headphone jack) plays on as usual. The USB stream does not follow the MASTER knob: the knob sets the jack's
level only, so a recording keeps its level whatever the headphones are set to.

The host has to be the USB host: a computer, or a phone or tablet that takes USB audio devices (on Android, most do
through USB-C). The FM-1 offers 44.1 kHz only; a host or app that works at 48 kHz converts it (Android, macOS and
Windows do). Streaming takes about 2 % of the FM-1's processing time.

### Tested with

- **POCO X7 Pro (HyperOS), over a USB-C cable:** the phone's own camera app records video with the FM-1's sound
  as its audio, as it plays. The phone needs OTG switched on before the FM-1 is plugged in: Settings > Additional
  settings > OTG. The switch is greyed out while anything is plugged into the phone's USB-C port (a charger too):
  unplug, switch it on, then connect the FM-1. HyperOS turns it off again after 10 minutes with no device.
- **MacBook Air M5 (macOS), over a USB-C cable:** the FM-1 shows up as FM-1 Bigorna, 2 input channels, 44,100 Hz,
  next to its MIDI port. Measured with a test tone in place of the music: the FM-1 sends a packet every 1 ms USB
  frame, every frame in order (none repeated, none dropped), under six tracks playing eighth notes; streaming costs
  1.8 % of the FM-1's processing time. Music recorded from it peaked at -3 dBFS, the MASTER knob not changing it.

The FM-1's own MIDI goes on alongside the audio on the same cable.

## The screen

Few words, very large type, high contrast, and a colour for each track. The track's view shows the track's number,
its machine and its sound's family; at the top left the pattern playing (orange with changes not saved) and the
chord form (and ARP while the arpeggiator is on); at the top right the battery, the octave, under them the
track's transposition (T+2) and, while recording, REC or LOOP.

The battery shows three bars in green when full, two in green, one in amber, and one in red when it runs low. On a
computer's or phone's USB power it shows a white bolt instead (the reading is then the supply's, not the charge's).
The levels are read from the battery's voltage, smoothed over a few seconds; they are coarse, not a percentage.

## Rescue

Hold OCT− while switching the FM-1 on: the rescue screen, from which the installer can run again. Two failed starts
in a row also enter it. If the FM-1 starts but no longer answers over USB, hold OCT− and OCT+ for 5 seconds while it
runs: the chip's own update mode, from which a firmware can be restored with
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
