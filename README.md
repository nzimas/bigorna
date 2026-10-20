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
| OCT− / OCT+ | The track's octave, −3 to +3 (shown at the top right) |
| White keys | Play the track in the scale chosen |
| 1st black key | CHORD mode on or off |
| 2nd black key | MUTE mode on or off |
| 3rd black key | BREAK, while held; SHIFT + it: on every track |
| Black key before the last | EDIT: the sound's pages |
| 3rd black key from the right | STEP: the step view |
| Last black key | SHIFT |
| KNOB 1 to 4 | The filter's FREQ and RESO, the delay and reverb sends; with SHIFT, KNOB 3 the chorus send, KNOB 4 the overdrive |
| SELECT (knob) | The track's level |
| ALGORITHM (knob) | The track's pan |
| PLAY | The sequencer plays or stops; held 0.8 s, the TEMPO page; SHIFT + PLAY: the metronome on or off |
| REC | Records the keys into the track's steps; SHIFT + REC: the looper |
| ARP | The patterns |
| SEQ | The effects' pages |
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
screen shows the machine and the family. 56 families: 29 for DRUM, 9 for 4-OP, 10 for WAVY, 8 for HIVE.

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
root are set on the session's page (SAVE): CHROMATIC (the black keys play too, but for the first three),
MAJOR, MINOR NATURAL, MINOR
HARMONIC, DORIAN, PHRYGIAN, MINOR PENTA, MAJOR PENTA, WHOLE TONE, IN. OCT− and OCT+ move the track's octave. USB
MIDI notes play the chosen track as the keys do.

### CHORD mode

In the track's view, the step view off, the 1st black key turns CHORD mode on or off for the track (lit while on,
dim off). Each white key then plays a triad from its note, in the scale: TRIAD, INV 1, INV 2, SUS2, SUS4, OPEN or
FOURTHS (in CHROMATIC, the same shapes in semitones). SHIFT and the key choose the next form. The form shows at the
top left. A step put in takes the chord last played; REC records chords into the steps; the looper records their
notes. DRUM tracks play no chords.

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
plays as written until then. Each press draws a way of playing the steps, shown in the name band; held two bars, a
new one is drawn on the next bar line (never the same twice running). Every way keeps what is on a beat on a beat:

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

SEQ shows CHORUS, COMP, DELAY and REVERB in turn, then goes back (OCT− and OCT+ move between them; a track's button
goes back to the track's view). KNOB 1 to 4 set the first four rows; PRESETS turned right shows the next four.

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
LOAD, SAVE, COPY, DELETE, TEMPO, ROOT, SCALE. SAVE runs the row chosen, or moves the focus to its value (PRESETS
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

## GLOBAL SETUP (SHIFT + SAVE)

SHIFT + SAVE (outside the sessions' pages) opens GLOBAL SETUP: the settings of the machine rather than of a session.
They are kept in the FM-1's flash and read at every start. PRESETS moves through a list and SAVE goes into the entry
chosen; on a setting, SAVE moves the focus to its value, PRESETS changes it and SAVE sets it. SHIFT + SAVE leaves a
value as it was, or goes back a level; from the top it leaves GLOBAL SETUP, as a track's button does. Changes are
written to the flash as it is left.

### MIDI

MIDI goes over the USB cable, both ways (the FM-1 shows up as FM-1 Bigorna). The settings follow the Model:Cycles'
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

Clock in. With CLK IN on and a clock arriving, the FM-1 follows it: the tempo is the average of the last 24 pulses
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

The FM-1's 3.5 mm MIDI IN jack is not used yet.

## The screen

Few words, very large type, high contrast, and a colour for each track. The track's view shows the track's number,
its machine and its sound's family; at the top left the pattern playing (orange with changes not saved) and the
chord form; at the top right the octave and, while recording, REC or LOOP.

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
