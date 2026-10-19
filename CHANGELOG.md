# Changelog

## Build 12

Added
- Live recording into the chosen track's 16 steps, with REC.
  - Stopped: REC arms (REC flashes, ARM at the top right) and the metronome counts the tempo. The first note
    played starts the sequencer, and that note is step 1.
  - Playing: REC records at once, over what the track already holds (punch in).
  - Each note played goes to the nearest step (the second half of a step goes to the next one), with its velocity
    and its length as held (the nearest length, up to 16 steps); a note on a step that held one replaces it, its
    condition and locks cleared. The note sounds as it is played; the step it went to is not played again that
    time round.
  - Recording goes round the 16 steps until REC is pressed again (REC at the top right, in red, while it is on);
    the sequencer plays on. PLAY stops both.
  - Notes from USB MIDI are recorded too (at the default length).
- A metronome: it clicks each beat, higher on the bar's first, while REC is armed or on. SHIFT + REC turns it off
  or on.

## Build 11

Added
- STEP, the third black key from the right (next to EDIT), turns the step view on and off. It is lit dimly, and
  fully while the view is on. Everything the step view did on SHIFT in build 10 now works the same way under STEP.
- A phrase generator: in the step view, the first black key from the left (lit dimly there) writes a new phrase on
  the chosen track and flashes NEW PHRASE; the key lights fully as it does. A phrase sets the track's steps: their
  notes (in the scale, from the track's last note played), velocities, lengths, trig conditions and parameter locks.
  - The track's sound decides the phrase: its family gives it a role (kick, snare, hat, percussion, noise, bass,
    lead, stab or drone; a sound with no family goes by its machine). Each role has its own ways of making a
    rhythm (straight, broken, Euclidean, sparse, bursts), its own use of conditions (probability ghosts, A:B
    variations, rolls that play every other time, PRE / !PRE pairs where one of two steps plays) and its own locks.
  - Drums lock where they accent (more drive on a kick, a reverb throw on a snare, opened hats); percussion and
    noise lock almost every step (pan, bit and sample rate reduction, filter, delay throws); bass moves its filter
    step by step; leads and stabs throw notes into the delay; drones hold long notes under reverb and chorus.
- Parameter locks, set per step by the phrase generator (they cannot be set by hand yet): FREQ, RESO and the
  envelope depth of the filter, the amp's decay, VOL, overdrive, bit reduction, sample rate reduction, the delay,
  reverb and chorus sends, and pan. The filter, decay, VOL, overdrive, bit and rate locks belong to the note; the
  sends and the pan hold on the track from the step until the track's next step plays, and go back to the sound's
  own when the sequencer stops.

Changed
- SHIFT no longer opens or latches the step view; it is SHIFT again everywhere.
- A pattern now keeps only the steps that hold a note, each with its locks. A pattern holds about 670 bytes of
  steps: a phrase that would not fit loses locks at random until it does; steps placed by hand beyond that room lose
  their locks, then are left out, when the pattern is saved. Patterns saved with builds 9 and 10 still load.

## Build 10

Added
- The step view (SHIFT held) latches when SHIFT is held for 3 seconds; SHIFT blinks while it is latched, and pressing
  SHIFT again lets it go. SHIFT held together with a button or a knob does not latch.
- In the step view, the black keys from the fourth from the left on choose what the four knobs do to the step held.
  They are lit dimly in the step view, and the one chosen is lit fully; pressing it again turns it off.
  - The fourth black key, TRIG: holding a step shows STEP n with four rows, set by KNOB 1 to 4:
    - NOTE: the note, up or down the scale.
    - VEL: the velocity, 1 to 127 (100 for a new step).
    - LEN: the length in steps: 1/8, 1/4, 3/8, 1/2, 3/4, 1, 1.5, 2, 3, 4, 6, 8, 12, 16, INF (1/2 for a new step).
      INF holds until the step plays again or the sequencer stops.
    - COND: the trig condition, as on the Digitone II: NONE; PRE, !PRE (the last condition on the track was
      true, or not); NEI, !NEI (the same on the track before); 1ST, !1ST (the first time through, or not); a
      probability, 1% to 99%; A:B and !A:B for B 2 to 8 (plays on time A of every B times through the track).
- A track's steps keep up to four notes sounding at once, so a long step can ring under the next.
- Velocity, length and condition are saved with the pattern. Patterns saved with build 9 load with the defaults.

Changed
- PRESETS no longer changes the note of the step held (KNOB 1 on TRIG does).
- A step that holds a note is emptied only when it is pressed and let go within 0.4 s without a change; holding a
  step to look at it keeps it.

## Build 9

Added
- A step sequencer: 16 steps a track, a 16th note each at the tempo.
  - While SHIFT is held, the white keys are the chosen track's 16 steps, and the bottom of the screen shows them
    (with a note in the track's colour, empty as a grey frame, the step playing white).
  - A white key on an empty step places the last note played on that track.
  - A white key on a step that holds a note, let go without turning PRESETS, empties it.
  - While a step is held, PRESETS moves its note up or down the scale, and the screen shows the step and its note
    (STEP 5  C#3). The note sounds while the step is held.
  - The white keys' LEDs show the steps while SHIFT is held: lit with a note, dim when empty. While the sequencer
    plays, the step playing shows the other way round.
  - PLAY starts and stops the sequencer; its LED blinks on the beat.
  - Each track's steps are saved with the pattern. Each track also has a length, stored and played but not yet
    settable (16).
- SHIFT + ARP held, a white key held 0.8 s: its pattern is emptied (in the flash too when the session has a slot),
  and the screen flashes EMPTY / PATTERN n. A white key let go sooner loads its pattern, as before.

Changed
- Projects are now called sessions on the screen (SESSIONS, SESSION 3, NO SESSION).
- The top left of the main screen shows only the pattern playing, on one line: P1 to P16, white, or orange with
  changes not saved.
- The patterns have no page of their own: ARP opens the PATTERNS grid, PRESETS moves through it, and ARP loads the
  pattern chosen. Loading a pattern replaces the steps playing; changes not saved are lost.
- A session loads at its pattern 1; so does the start.
- SHIFT no longer latches held notes.

Removed
- The pattern's page (LOAD, SAVE, COPY, DELETE, TEMPO). Patterns play at the session's tempo for now.

## Build 8

Changed
- ARP is to the patterns what SAVE is to the projects: ARP opens the PATTERNS page, ARP on a slot opens its page,
  and on that page ARP runs LOAD, SAVE, COPY or DELETE, or moves the focus to TEMPO and sets it. SAVE is for the
  projects only (on a pattern's page it opens the PROJECTS page).
- A pattern's SAVE makes it the pattern playing.
- 16 projects (was 8), each with 16 patterns (was 32), on one PATTERNS page of four rows of four. A project takes
  four sectors: itself and its patterns, six a sector (680 bytes a pattern). Projects saved with build 7 are not
  read.
- The PROJECTS list in the pages' size (PROJECT 16 fits).

Added
- SHIFT + ARP held: the PATTERNS page shown and the white keys lit dimly, the pattern playing's key bright; a white
  key loads its pattern (an empty one too: GLOBAL, nothing in it). Let go, back to what was shown.

## Build 7

Added
- Patterns: 32 in each project, saved inside it. A pattern holds its tempo (and, to come, its sequence); the sounds
  stay the project's.
  - ARP: the PATTERNS page, 1 to 32, sixteen a page in four rows of four (the page shown by which half of the line
    under the title is thick); PRESETS moves through them, SAVE opens one's page. ARP again goes back.
  - A pattern's page: LOAD, SAVE, COPY, DELETE, then TEMPO: GLOBAL (the project's) or 10 to 300 BPM -- from GLOBAL,
    a click to 10, then 5 BPM a click to 40, then 1 BPM a click. SAVE, PRESETS, SAVE to set it, as on a project's
    page. A pattern's tempo plays over the project's.
  - Saving, copying or deleting a pattern writes it into the project's flash at once when the project has a slot;
    a project never saved keeps its patterns until it is.
  - A project's SAVE stores the pattern playing into its slot too.
  - The main screen shows, at the top left, the project (P3, green) and under it the pattern playing (20, amber),
    each orange with changes not saved.
- HOME held (0.8 s): the project playing saved into its slot (NO PROJECT if it has none yet). HOME's new sound now
  comes when HOME is let go.
- ARP held (0.8 s): the pattern playing saved into its slot.

Changed
- Projects: 8 slots (was 32), nine 4 KB sectors each: the project, then its 32 patterns four a sector. Projects
  saved with builds 3 to 6 are not read.
- A project's and a pattern's pages: the operations (LOAD, SAVE, COPY, DELETE) and the values (TEMPO, ROOT,
  SCALE) are two sections, a line between them; DELETE in red; the DELETE question without a sentence under it.
  The titles are centred.

## Build 6

Changed
- A project's page: SAVE on TEMPO, ROOT or SCALE moves the focus to the value (its field in white); PRESETS changes
  it, SAVE sets it and puts the focus back on the rows, SHIFT + SAVE leaves it as it was. SELECT no longer changes
  them (it does nothing on the projects' pages).

## Build 5

Changed
- The USB product ID is now 0x0005 (it was 0x0004), so a computer that met the earlier builds reads the device's
  name afresh: FM-1 Bigorna.

## Build 4

Changed
- A project's page: every name and value in one size (smaller than before); each value right-aligned in a field of
  its own; a scale's name longer than the field scrolls round in it, in full (MINOR PENTA, CHROMATIC). TEMPO shows
  the number alone.

## Build 3

Added
- Projects: 32 slots in the FM-1's flash, one 4 KB sector each (the user data area TONITOR kept its sessions in;
  the sectors after the 32nd are left for patterns). A project holds every track's machine, its sound on each of the
  four machines, its octave, level and pan; the delay, reverb and chorus; the tempo, the root and the scale.
  - SAVE: the PROJECTS page, PROJECT 1 to 32, five at a time; PRESETS moves through them. A saved project's name is
    white, an empty slot's grey; the project loaded is green, with a block at its right (orange once it has changes
    not saved).
  - SAVE on a slot: its page, LOAD, SAVE, COPY, DELETE, TEMPO, ROOT, SCALE, five at a time; PRESETS moves through them,
    SAVE runs LOAD, SAVE, COPY or DELETE, SELECT changes TEMPO (10 to 300 BPM), ROOT and SCALE.
  - TEMPO, ROOT and SCALE are the project playing's, saved with it. The delay's time follows the tempo.
  - LOAD with changes not saved, SAVE over another project, DELETE, and COPY over a project ask first: NO or YES on
    PRESETS, SAVE answers. COPY asks for the slot to copy to (PRESETS), then SAVE.
  - SHIFT + SAVE goes back a page; a track's button goes back to playing.
  - The main screen shows the project loaded at the top left (P7), orange once it has changes not saved.
  - A slot holding another firmware's data reads as empty; saving into it replaces that data.

Changed
- The firmware is named Bigorna: the splash screen, the USB device name (FM-1 Bigorna), the package (bigorna.fwsc).
- The root and the scale are set on a project's page; ARP no longer opens the SCALE page (ARP has no function for
  now).

## Build 2

Changed
- The Digitone II's own curves, read from its DSP (docs/dt2-curves.md) and fitted (tools/gen_tables.py; the tables
  are generated from the fits, not copied):
  - AMP envelope: attack ~48 ms at 32, ~0.26 s at 64, 1 s at 110, then 1.25 to 30 s; hold up to 10 s; decay and
    release ~1 s at 32 and ~5.5 s at 64 to -60 dB, ~29 s at 126, 127 INF. AHD has no sustain; its release is the
    decay.
  - FLTR envelope: delay up to 3 s, decay and release ~0.36 s at 32, ~1.8 s at 64, ~29 s at 126.
  - FREQ: 5 Hz to 22 kHz (5 x 4400^(v/127)); ENV reaches the whole span; KEY.T tracks an octave an octave at 127,
    from note 48.
  - MULTI-MODE resonance Q 0.5 to 20.5; LEGACY 0.5 to 22.4; LOWPASS 4 feedback 3.5 x^2.
  - COMB- and COMB+: FREQ is a note number (220 x 2^((n - 69)/12) Hz), the envelope moves it 127 semitones, KEY.T
    follows the keys; the low-pass in the loop at 22 kHz x ((LPF/127)^2 + 0.2 f^2).
  - Base-width: one-pole high-pass and low-pass, 5 x 8000^(v/127) Hz.
  - FM TONE: operator envelopes 20 s attack and 60 s decay at 127; modulation depth from the Digitone II's LEV
    curve, looked up at LEV x envelope, 2.2 cycles at full; B's LEV split between B1 and B2 by its law; DTUN adds
    up to 0.5 to A's ratio and 0.75 to B2's; feedback up to 2/pi of a cycle, scaled by the operator's own level;
    key tracking from C1 to C7 in Hz; HARM's 26 waves as the Digitone II's partial sets; algorithm 8's outputs as
    its DSP sums them (X: C and B2, Y: B1).
  - FM DRUM: decays reach e^-6 in 8 (0.1p + 0.9p^4) s, holds 1000 p^2 ms, the sweep falls in 5 (0.1p + 0.9p^4) s
    and starts 16 p^2 times the note above it (linear in Hz); MOD, FDBK, LEV and the transient level p^2, noise
    2 p^3; GRAN as impulse odds 0.0001 + (0.992 - p)^2; the folder 1.12u - 0.672 tanh(4u) at gain p^2 + 0.07;
    the 33 operator waves as the Digitone II's partial sets.
  - WAVETONE: the noise envelope is the AMP envelope (AHD); levels p^2; wave positions v/127 along the tables.
  - SWARMER: the six voices' asymmetric spread (~19 cents for the outermost at 64, two semitones at 127); MIX as
    the main oscillator 0.25 to 0.13 against each swarm voice up to 0.105; animation and noise depth p^2.
  - Bit reduction: 16 bits down to 1.9, a fractional depth crossfading the two whole ones; SRR holds 1 + 100 p^2
    samples.
  - Delay: feedback v/128; HPF and LPF one-poles, the low-pass from HPF up. Reverb: predelay 0.8 to 337 ms, decay
    0.46 s x 1.0259^v (~9 s at 116, ~46 s at 126, 127 held), the shelf from FREQ, HPF and LPF one-poles. Chorus:
    speed 0.28 to 3.1 Hz, HPF 40 Hz to 1 kHz. Sends v^2, returns 2 v^2.
- HOME: the tastes lean industrial and IDM; after a rhythmic family's recipe, a grit pass adds at random drive
  after the filter, bit reduction, sample-rate reduction, more fold (FM DRUM), feedback and detune (FM TONE),
  noise modulation (SWARMER), or a comb in place of the filter. The dark-ambient families (NOISE ENGINE, SCRAPE,
  MACHINE, FB DRONE, WAVE PAD, RING DRONE, GRAIN FIELD, BREATH, RADIO, SWARM PAD, SWARM DRONE, DARK WASH) are left
  as they are.
- Each track's output passes a DC blocker (~10 Hz) before its limiter.

Added
- Recipes: CRUSH KICK, COMB HIT, SCREECH, RATTLE, STATIC BURST, IMPACT (FM DRUM); FM SCREAM, FM GROWL, FM GLITCH,
  FM CLANK (FM TONE); RING SCREAM, SYNC GRIND, DIGITAL HASH, PD BUZZ (WAVETONE); SWARM GRIND, HORNET (SWARMER).
  56 recipes in all.

Removed
- Recipes FM BLIP, FM PAD, FM PLUCK, DATA BURST, SYNC LEAD, SHAKER, NOISE HAT, SAW BASS, WAVE KEYS.

Fixed
- Crackle on sounds sent to the chorus (most audible on SWARMER pads): the chorus's fractional delay was
  interpolated towards the older sample, so its value jumped each time the fraction wrapped under the sweep.
- WAVETONE and SWARMER choose their band-limited wave level from the pitch before drift, detune, animation and
  noise modulation, so it no longer flips at a level boundary.

Not yet
- LFOs, the master compressor, sessions and presets, the sequencer. The track overdrive's drive curve, the EQ's
  bandwidth and the WAVETONE wave contents are not read from the Digitone II yet.

## Build 1

First build. Based on Tonitor 0.46 (the HAL, boot, USB, the installer and rescue mode, the voice pool, the screen).

Added
- Six tracks, each playing one of four machines after the Digitone II's, chosen with PRESETS: FM TONE, FM DRUM,
  WAVETONE, SWARMER. Each track keeps a sound of each machine.
- FM TONE: operators C, A, B1, B2; the eight algorithms as the Digitone II's OS 1.12 draws them (modulation,
  feedback operator, X and Y outputs direct or through the operator envelopes); the Digitone II's ratio lists
  (C and B: 19 values, A: 36; RATIO B as B1 x 19 + B2); HARM, DTUN, FDBK, MIX; A and B envelopes (delay, attack,
  decay, end level, triggered or gated, reset); phase reset; ratio offsets; key tracking.
- FM DRUM: operators C, A, B in the Digitone II's seven algorithms (B also heard in 5 to 7); ratios A and B
  linear from 0.001 to 31.75; pitch sweep, wavefolder, body, noise (grain, ring modulation, base-width band) and
  eight transient types.
- WAVETONE: two oscillators on PRIM (sine, triangle, saw, square) or HARM (16 waves), phase distortion, linear
  offsets, ring modulation, fixed ring modulation, hard sync, phase reset, drift; noise with its own envelope,
  GRAIN, TUNED and S&H types and a base-width band.
- SWARMER: a main oscillator and six detuned ones, swarm and main waveforms, detune, mix, main octave, animation
  (three LFOs, one per pair), noise modulation.
- Per voice, in the Digitone II's order: SRR and overdrive (PRE or POST the filter), base-width filter (PRE or
  POST), the FLTR machine (MULTI-MODE, LOWPASS 4, LEGACY LP/HP, COMB-, COMB+, EQUALIZER) with its envelope and key
  tracking, bit reduction, the AMP envelope (AHD with HOLD fixed or NOTE, or ADSR; 127 on decays and releases is
  INF).
- Send effects: chorus, delay (time in 128th notes at 120 BPM, ping-pong, width, feedback to 198 %, HPF and LPF in
  the repeats, send to the reverb), reverb (predelay, decay with INF, shelving FREQ and GAIN in the tank, HPF and
  LPF on its input). SEQ shows REVERB, DELAY, CHORUS in turn.
- EDIT (the black key before SHIFT): the sound's pages SYN 1 to 4, FLTR 1, FLTR 2 (with the FLTR machine), AMP,
  FX. KNOB 1 to 4 set A to D, with SHIFT held E to H; OCT- and OCT+ move between pages.
- KNOB 1 to 4 on the main screen: filter FREQ and RESO, delay send, reverb send; with SHIFT, KNOB 3 the chorus send
  and KNOB 4 the overdrive. The whole screen is the knob's dial while it turns.
- HOME: a new sound for the track on its machine, from 49 recipes ported from SynthSeeder (kicks, snares, claps,
  hats, toms, metal, glitch, basses, leads, plucks, bells, stabs, pads, drones, textures); SHIFT + HOME: the same
  family again.
- Scales on the white keys (ARP: root and scale), including CHROMATIC on every key; octave per track; latching
  with SHIFT; USB MIDI notes.

Not yet
- LFOs, sessions and presets, the sequencer. SAVE shows NOT YET.
- Envelope times, filter frequency and resonance curves are this firmware's own estimates until the Digitone II's
  are read from its DSP.
