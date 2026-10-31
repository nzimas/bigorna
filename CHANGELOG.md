# Changelog

## Build 80

Changed
- A sidechain's source is one subtrack, not the whole track. SC SOURCE lists a track with subtracks as each of them
  (T1-A, T1-B, ...), and a track without them as before (T2). By default, and for a session or pattern that names
  only the track, the source is the track's kick: the first of its subtracks whose sound is a kick (none: ST-A). A
  source subtrack given up falls back to the kick. Build 79 summed all of a track's subtracks.
- Sessions keep the source's subtrack. Sessions from build 79 and earlier take the kick.

## Build 79

Added
- Subtracks, after Titan Haze's lanes: a track on DRUM has subtracks ST-A to ST-D, each a track in its own right (its
  own sound, steps, length, clock, direction, phrase, octave, transposition, level, pan, sends and MANGLE), all
  playing through the track's mute, sidechain and MIDI channel. Hold the track's button and press white key 1 to 4 to
  choose one; SHIFT with it gives one past ST-A up. Three subtracks past ST-A are shared by all the tracks: one track
  with four, or several with two or three. A new subtrack gets the kit's next piece after the track's own.
- The keys, MIDI notes, EDIT, the step view, the TRACK and PHRASE pages, SELECT, ALGORITHM and OCT act on the
  subtrack chosen. The screen shows it (ST-A to ST-D) under the pattern; the TRACK page's title reads 1B.
- Patterns keep each subtrack's steps and settings; sessions keep its sound, level and pan; the looper notes the
  subtrack of each note. A track off DRUM puts its subtracks away until it is on DRUM again.

Compatibility
- Sessions, patterns and loops from earlier builds load as before, with no subtracks.

Changed
- A track that is ducked follows its sidechain source's subtracks too: their hits are summed before they are followed.

## Build 78

Added
- MUTE QT, a session setting on the session's page (YES / NO, saved with the session). With YES, MUTE mode's mutes,
  unmutes and solos wait for the start of the pattern's next cycle, the moment the chosen track begins its steps again
  (as a pattern switch does); the track's button blinks until then. With the sequencer stopped they happen at once,
  and choosing a pattern drops mutes still waiting. NO (the default, and every session saved before) works as before.

Removed
- ROOT and SCALE from the session's page: the SCALE page (ARP) sets them.

## Build 77

Added
- SHIFT + ARP draws a scale and a root at random and shows them on the SCALE page for 1.5 s, over any page, as
  SHIFT + the 1st black key does in Tonitor (the 1st black key is CHORD mode here).
- GLOBAL SETUP > START: how the FM-1 starts. NEW (the default): with no session loaded and a new one made, as Tonitor
  starts — new sounds on every track, a scale and root drawn at random, the tempo 120. LAST: with the last session
  loaded or saved, as before. A setup saved by build 76 or earlier reads as NEW, so after this update the FM-1 starts
  with a new session until START is set to LAST; the MIDI settings and MIDI LEARN bindings are kept.

Changed
- The start and a new session (SAVE on an empty slot) draw a scale and root at random. Before, both were MINOR
  PENTA from C.
- GLOBAL SETUP opens on its first entry, MIDI.
- A value being set in GLOBAL SETUP has a white field as wide as the value.

## Build 76

Changed
- The patterns, the sessions and GLOBAL SETUP are reached as in Tonitor, with Bigorna's patterns in place of
  Tonitor's snapshots:
  - SAVE opens the PATTERNS grid; SAVE again takes it down. Before, ARP opened it.
  - SAVE held 0.8 s saves the session, as before. On the PATTERNS grid it saves the pattern playing into the slot
    under the cursor, as a white key held does.
  - SHIFT + SAVE opens the sessions' list; SHIFT + SAVE again takes it down. Before, SAVE opened it.
  - SHIFT + SAVE held 0.8 s opens GLOBAL SETUP. Before, SHIFT + SAVE opened it at once.
  - On the sessions' pages and in GLOBAL SETUP, SAVE and SHIFT + SAVE work as before.
- ARP opens the SCALE page, as in Tonitor: SELECT sets the root, ALGORITHM the scale, and ARP again goes back. The
  root and scale can still be set on the session's page.

Removed
- ARP held no longer saves the pattern playing, and ARP on the grid no longer loads the pattern under the cursor. On
  the grid, SAVE held saves the pattern playing into the slot under the cursor, and a white key loads its pattern.

Fixed
- The manual's text for builds 72 to 74 (patterns, loops, sessions, the dimmed root keys) was written into the
  website's manual page only, so publishing build 74 put the older text back. It is now in the README, which the page
  is made from.

## Build 75

Changed
- The splash screen shows the Bigorna logo: the anvil over the lettering, in steel on black, with the build under
  them. It is drawn from the website logo's shapes (the anvil's outline, BIGORNA in Michroma), made into bitmaps when
  the firmware is built (tools/gen_logo.py); they take 9.5 KB of flash and no RAM.

## Build 74

Fixed
- A session saved by builds 38 to 72, loaded by build 73, had its loops only in the pattern it was saved playing, so
  a pattern that was a copy of it played no loops. Now each of its loops goes to every saved pattern, as they played
  before; empty patterns get none. If the loop memory runs out, LOADED, LOOPS LOST shows and the remaining patterns
  get none.

Compatibility
- A session already saved again by build 73 keeps the loops as build 73 placed them: to give a copied pattern the
  loops back, load the pattern that has them and hold the copy's white key on the PATTERNS grid.

## Build 73

Changed
- Loops belong to a pattern, as its steps do. Before, the loops belonged to the session and played in every pattern,
  an empty one too. Now another pattern does not play them; an empty pattern plays nothing. When a pattern comes in,
  its loops start from their first step with its steps, and the loops of the pattern it replaces stop.
- Saving the pattern playing into another slot, pasting a pattern and saving a break copy its loops with it.
  Emptying a pattern removes its loops.
- All the patterns' loops share one memory: 40 loops and 4608 notes in all (the 18 loops of 256 notes there were
  before still fit). When it is full, arming a loop shows NO ROOM; saving a pattern into another slot saves its steps
  and shows LOOPS LOST for the loops that did not fit.
- The transposition moves the pattern playing's loops only.

Removed
- A pattern that unmutes a track no longer keeps that track's loops silent until their first step: every loop of the
  pattern starts with it.

Compatibility
- A session's loops are saved with their pattern. A session saved by builds 38 to 72 loads with its loops in the
  pattern it was saved playing. Loaded by build 72 or earlier, a session saved by this build puts the loops of every
  pattern together, and loops in the same place on a track in different patterns overwrite each other.

## Build 72

Fixed
- A pattern saved with one track locked on many steps lost the steps of the tracks after it. The steps were written
  track by track, each with all its locks, and once the pattern's room was full the remaining tracks got nothing.
  Now every step with a note on every track is always saved (all 96 fit); the room left goes to chords, then MOD
  settings, then locks, shared one per step in turn across all six tracks.

Changed
- The PATTERNS grid: a white key held 0.8 s saves the pattern playing into that slot, empty or not, as Tonitor saves
  its snapshots. Copy and paste moved to SHIFT + a white key held. With the 3rd black key held and the sequencer
  stopped, a white key held saves nothing.
- The PATTERNS grid no longer draws a green frame around the pattern playing. The cursor (the white square) is the
  only highlight; the pattern playing still has its number in green (orange with changes not saved).

Added
- Out of the step view, the keys that play the root of the scale are lit dimly.

Compatibility
- Nothing saved changes. Patterns saved by earlier builds load as before.

## Build 71

Fixed
- PING-PONG stopped the track on one step when chosen while the sequencer played, unless the track had been started
  in PING-PONG. Its way up or down was set only when the sequencer started, so a track started in another direction
  moved by nothing: it stayed on its step, and a pattern waiting to come in never did. Now it goes up from the step
  it is on.

Compatibility
- Nothing saved changes.

## Build 70

Changed
- The step view: the steps are silent. Pressing or holding a step, or turning its note, no longer plays it. The notes
  are heard from the keys with the step view off.
- 14 more scales, from Tonitor's list: MINOR MELODIC, ENIGMATIC, LYDIAN, MIXOLYDIAN, LOCRIAN, BLUES, HUNGARIAN MINOR,
  DOUBLE HARMONIC, PHRYGIAN DOMINANT, JAPANESE HIRAJOSHI, JAPANESE IWATO, PELOG, OCTATONIC, PROMETHEUS (24 in all).
  They come after the ten there were, so the scales already chosen keep their numbers.

Added
- SHIFT + STEP (the 3rd black key from the right) empties the chosen track's 16 steps: notes, velocities, lengths,
  conditions, ratchets, MOD settings and locks. The track's length, clock, direction and transposition, its loops and
  the other tracks are kept. The step view stays on or off as it was.

Compatibility
- Nothing saved changes. A session saved with one of the new scales, loaded by build 69 or earlier, plays CHROMATIC.

## Build 69

Fixed
- PATTERNS grid: after a session was loaded at a pattern other than 1, the grid's cursor (the white square) stayed on
  pattern 1 while the pattern playing had only its green frame. The cursor now opens on the pattern playing (or the
  one waiting to play) and follows the pattern playing when it changes.

Compatibility
- Nothing saved changes.

## Build 68

Fixed
- A pattern with another transposition, switched to while playing: its loops played the new transposition from
  their second note, not the first. The loop notes were moved by the main loop after the sequencer had already played
  the first one. Now the sequencer applies the new transposition to the loops at the step the pattern comes in on,
  including when the track's clock runs a little behind the tempo's 16ths.

Changed
- Loops keep their notes as recorded in RAM and are transposed as they play (by the track's transposition less the
  one they were recorded at), instead of being rewritten in place.
- At the step a waiting pattern comes in on, the sequencer takes the pattern in before playing that step's loop notes.

Compatibility
- Nothing saved changes. A saved session still holds its loops as they play in the pattern it was saved playing.

## Build 67

Fixed
- Loops did not follow a pattern's transposition. Transposing moves a track's loop notes as well as its steps, but
  the loops belong to the session, so switching to a pattern with another transposition (or none) left the loops
  transposed. Now, when a pattern comes in (switched to, playing or stopped, or emptied while playing), each track's
  loops move by the difference between the new pattern's transposition and the previous one. Loading a session
  leaves its loops as saved.

Compatibility
- Nothing saved changes. Loops saved in a session are taken to be at the transposition of the pattern the session
  was saved playing, as before.

## Build 66

Added
- PATTERNS grid: SHIFT and a white key held 0.8 s saves the pattern playing, with its changes and its tracks' state,
  into that slot, empty or not. The pattern playing plays on. A slot waiting to play takes the saved pattern.

Changed
- PATTERNS grid: emptying a pattern is now SHIFT and the 1st black key held, then the white key pressed (before:
  SHIFT and the white key held 0.8 s, which now saves).

Compatibility
- Nothing saved changes.

## Build 65

Added
- A pattern change that unmutes a track with loops: each of the track's loops stays silent until it comes round to
  its first step, then plays on in its place, in time with the sequencer. The track's sequencer steps play at once.
  A loop at its first step as the pattern comes in plays it at once. Track unmutes by hand or by MIDI are unchanged.

Changed
- While the sequencer plays, a pattern's saved mutes are applied by the sequencer at the moment the pattern comes in
  (before: by the main loop, a moment later). A track the pattern unmutes has the notes it played while muted cut,
  so no tail of them is heard as it opens.

Compatibility
- Nothing saved changes.

## Build 64

Added
- BREAK on the PATTERNS grid: the 3rd black key (SHIFT and it: every track) breaks up the tracks as in the track's
  view, with the step view on or off; the grid's title shows the way playing.
- A break saved as a pattern: on the grid, with the 3rd black key held and the sequencer playing, a white key held
  0.8 s saves the break into that slot. Each broken track's steps become the steps the break plays at each position,
  with one draw of its ratchets, octave leaps, flams (MIDI FX FLAM), accents and gap; other tracks and drones are
  saved as written; the tracks' state is saved as by ARP held. Saving over the pattern playing replaces its steps at
  the end of the chosen track's time through.

Changed
- A pattern's track state (build 62) is packed into 54 bytes instead of 78, leaving 590 bytes for steps instead of
  566. With 566, a pattern with all 96 steps holding a note lost its last notes when saved.
- On the grid, while the 3rd black key is held, a white key held 0.8 s saves the break instead of copying or emptying
  the pattern.

Compatibility
- Patterns saved by builds 62 and 63 load with their state (their layout is still read). Patterns saved by build 64
  load in earlier builds with their steps; builds 62 and 63 ignore the new state layout, as build 61 does.

## Build 63

Added
- MUTE mode on the PATTERNS grid. It stays on when the grid opens from the track's view, and the 2nd black key turns
  it on or off on the grid (with the step view on or off). While it is on, the track buttons mute and solo tracks and
  the grid stays up; the grid's title shows the six tracks, in their colour playing and dark grey muted, and the 2nd
  black key is lit.

Changed
- On the grid with MUTE mode on, a track button no longer goes back to the track's view; turn MUTE mode off first,
  or press ARP to load the pattern chosen.

Compatibility
- Nothing saved changes.

## Build 62

Added
- A pattern holds each track's state when it is saved: muted or not, machine, octave, level, pan, sidechain (SC
  SOURCE and amount), CHORD mode and form, and the arpeggiator (on or off and its eight settings). Loading or
  switching to the pattern gives the state back; while the sequencer plays, when the pattern starts. ARP held, SAVE
  of the session, and copying a pattern take the state with the steps.

Changed
- Changing any of those settings marks the pattern playing as changed (its number orange), as editing a step does;
  the change is kept in RAM with the pattern while another plays, until the session is saved or loaded.
- The steps of a pattern have 566 bytes instead of 644: the state takes the last 78 bytes of the pattern's step
  area. The phrase generator trims to the smaller room. The flash layout, the session format and RAM use are
  unchanged.

Compatibility
- Patterns saved by earlier builds load unchanged. They have no state: loading one leaves the state as it is, and a
  session's own saved mutes and settings apply when it loads, as before.
- Patterns saved by build 62 load in earlier builds with their steps; those builds ignore the state. A pattern from
  an earlier build whose steps take more than 566 bytes loses its last steps' locks when it is saved again.

## Build 61

Added
- FX: OD TYPE (knob D, the slot that was empty): the kind of OVERDRIVE, VALVE (as before), RAT or FOLD. Lockable per
  step. After the LXR firmware's master distortion (Erica Synths, GPL-3.0-only), in fixed point on the one knob:
  - RAT: a 60 Hz high-pass, an op-amp gain of x 1 .. x 2305 (OVERDRIVE, on an exponential curve) whose bandwidth
    falls with the gain (a 1.1 MHz gain-bandwidth: 16 kHz down to ~480 Hz), its slew rate and +-4.2 V rails, then
    LXR's 1N914 diode curve; OVERDRIVE's low-pass after.
  - FOLD: gain x 1 .. x 8 and a bias of 0 .. 0.3 (OVERDRIVE), OVERDRIVE's low-pass before the fold, folded back at
    full scale (a triangle fold, any number of times), a DC blocker after.
  - The level is brought back by a curve of the amount, measured as VALVE's (out/in RMS about 1 on a sine, FM and
    noise).
- HOME: the grit pass after a rhythmic family's recipe, when the sound has OVERDRIVE, sometimes sets OD TYPE to RAT
  or FOLD.

Compatibility
- Sessions from earlier builds load unchanged: OD TYPE was an unused slot, 0 (VALVE) in every saved sound.

## Build 60

Added
- DRUM: AB NOISE (SYN 3, knob E), 0 .. 127. Mixes operators A and B's waveforms towards a sample-and-hold noise
  that takes a new random value at the start of each cycle of the operator, so the noise steps at the operator's
  frequency and follows the note, the pitch sweep and RATIO A or B. It can be locked per step and is an LFO
  destination. NOISE RST on gives every note the same values. After the noise oscillator of the LXR firmware
  (Erica Synths, GPL-3.0-only), which its hihat uses as a modulator.
- HOME: METAL HAT, OPEN HAT, RIDE, CYMBAL, CLANG, RATTLE, STATIC BURST and SCREECH sometimes draw AB NOISE; the grit
  pass after a rhythmic DRUM family sometimes raises it.

Fixed
- GRAIN FIELD (WAVY) could draw its noise band (NS BASE) far above its band-pass filter (FREQ); with the band's LFO
  moving it further up, some notes were nearly or fully silent (about 1 in 70). NS BASE is now kept 30 below FREQ.

Compatibility
- Sessions from earlier builds load unchanged: AB NOISE was an unused slot, 0 (off) in every saved sound. Sounds
  drawn by HOME from a rhythmic DRUM family differ from earlier builds' draws for the same seed.

## Build 59

Fixed
- The knobs' dial in the track's view: its name and the track's number (DELAY SEND 6, REVERB SEND 6, CHORUS SEND 6,
  RESONANCE 4 ..) ran past the screen's left and right edges. It now takes the largest of the three sizes that fits.

## Build 58

Added
- SHIFT + SEQ: the chosen track's three LFOs thrown anew (LFO RANDOM), after Tonitor's MOD RANDOM. Two or three
  LFOs on, each to a parameter of the machine's pages that moves smoothly (NUM, BIP and HOLD parameters: no
  switches, lists or FM ratios), none to the same one twice; LFO 2 or 3 sometimes on an earlier LFO's SPEED or
  DEPTH; any wave, speed and mode, synced to the tempo most of the time, a fade now and then, a start, a depth up to
  56 either way. The other tracks are not changed. From the track's view and the effects' and LFO pages (not the
  sessions' and patterns' pages).

## Build 57

Changed
- SAVE on an empty slot's page, while a saved session is playing, now starts a new blank session there and saves
  it, instead of saving a copy of the session playing into it: each track's machine as at the start with new sounds
  drawn, the start's levels, pans, octaves, effects, compressor, tempo, root and scale; no patterns, loops,
  chords, mutes, transposition or arpeggiator; the sequencer stopped. NEW, SESSION n shows. With changes not
  saved, NEW SESSION? is asked first. With nothing saved yet (the start, or a session just deleted), SAVE on an
  empty slot saves what is playing, as before. COPY still copies a session into another slot.

## Build 56

Added
- A battery meter at the top right of the track's view: a small battery, three bars green, two green, one amber,
  one red when low; a white bolt on a USB host's power. Read from the battery's divider (ADC channel 3) in the
  main loop, smoothed; its levels use MELODEE's thresholds (531, 561, 591 of 1023).

Changed
- The track's view, top right: the battery, then the octave; the transposition (T+2) under them; REC or LOOP
  under that (one row lower than before).

## Build 55

Added
- A Backup page on the website (backup.html, after Tonitor's): it saves the sessions on the FM-1 -- each with its
  patterns and loops -- and GLOBAL SETUP to a JSON file, and restores them, over Web MIDI. The firmware answers
  Tonitor's backup commands in the installer's framing (0x50 list, 0x51 read, 0x52 / 0x53 / 0x54 write and
  commit, 256-byte pieces; firmware/src/backup.c). A record is a 4 KB sector as the flash holds it: a session's
  own, its pattern sectors, the loop sectors it holds, GLOBAL SETUP's.
- A restore checks each record whole (its head, length, body CRC and number) before writing it. A session's loops
  go first, into free sectors under a new generation; then its patterns; then the session, pointed at that
  generation, so a restore cut off leaves the session before it as it was. Sessions saved by older builds are
  restored as they are and converted when loaded, as from the flash. RESTORED S3 or RESTORED SETUP shows on the
  screen.
- The update loader is unchanged (its binary identical): the commands are answered by the firmware only.

## Build 54

Changed
- Transposition takes the loops too: in the step view OCT− and OCT+ move the chosen track's loops with its steps,
  and SHIFT + OCT− or OCT+ every track's, each note one step of the scale (CHROMATIC: a semitone), as the steps'.
  A loop's notes are moved in place and saved with the session; the loop being recorded is left alone. The TRN
  count shown stays the pattern's.

## Build 53

Changed
- The arpeggiator's page: holding the 5th black key 0.8 s opens it, or closes it, in place of SHIFT + the key. A
  short press still turns the arpeggiator on or off, now as the key is let go.
- BREAK: one way per press. The way drawn when the key is pressed (REPEAT, HALF TIME, DOUBLE, REVERSE, CHOP, ROLL)
  plays for as long as it is held; until now another was drawn every two bars held. Within the way, its ratchets,
  flams, leaps and gap are as before. The next press draws another, never the same twice running.

## Build 52

Added
- An arpeggiator for each track: the 5th black key (the track's view, the step view off) turns it on or off (lit
  while on; ARP at the top left). The keys held, and MIDI notes for the track, go to it; it plays them one at a time
  on the tempo's grid -- the sequencer's 16ths while it plays, its own clock at the tempo while stopped, the first
  note of a chord at once. It runs in the audio interrupt with the sequencer.
- SHIFT + the 5th black key: the ARP page, per track. MODE (UP, DOWN, UP DOWN, DOWN UP, ORDER, RANDOM, CONVERGE,
  DIVERGE, PINKY, THUMB), RATE (1/32, 1/16T, 1/16, 1/8T, 1/8, 1/4), RANGE (1 to 4 octaves), GATE, LATCH, DENSITY
  (the chance each note plays), OCT RND (the chance a note jumps an octave), BASS (every 2 to 8 notes, the lowest
  held an octave down). DENSITY, OCT RND and BASS follow the NGEN's ARPER generator's ideas.
- Recording: REC puts the arpeggio into the steps, each note at the step it was played on (where the sequencer was
  as it played, not where the main loop found it), with its velocity and length; SHIFT + REC records it into a
  loop. Armed, its first note starts the sequencer and is step 1.
- Each track's arpeggiator and its settings are saved with the session (9 bytes a track at the end of the session's
  body). Sessions from builds 50 and 51, and earlier, load with it off.

Changed
- SAVED, SESSION 16 and the like: the second line in the largest size that fits, no longer squeezed past the
  screen's edges.
- In CHROMATIC the 5th black key no longer plays a note out of the step view.

## Build 51

Added
- Transposition. In the step view, OCT− and OCT+ transpose the chosen track's steps; SHIFT + OCT− or OCT+ (in the
  track's view or the step view) transposes every track's. Each press moves each note one step of the scale and
  root chosen (CHROMATIC: a semitone); a note out of the scale moves to the scale's next. In the track's view, OCT−
  and OCT+ alone still move the keys' octave. The loops are not transposed.
- The transposition shows at the top right in the track's colour (T+2), up to 24 steps each way, and TRN with its
  value as it changes; the OCT keys' lights follow it in the step view.
- It is part of the pattern: the notes are moved in place and the count is kept in the pattern (bytes 26 to 31 of
  its steps, empty in every pattern before), so it is saved with the session and goes with a copied pattern.
  Patterns from earlier builds read as TRN 0.

## Build 50

Added
- LFOs: three per track, on the pages LFO 1, LFO 2 and LFO 3 after REVERB (SEQ), in the chosen track's colour.
  The parameters: SPEED, MULT (x1 to x2048 with the tempo, .1 to .2048 at 120 BPM), FADE, DEST, WAVE
  (TRI, SINE, SQR, SAW, EXP, RAMP, RND), START, MODE (FREE, TRIG, HOLD, ONE, HALF), DEPTH. A cycle takes
  2048 / (|SPEED| × MULT) 16ths. DEST is a parameter of the sound's pages, or an earlier LFO's SPEED or DEPTH.
  FADE fades in or out over |FADE| 16ths from each note; DEPTH is the swing each way as a share of the destination's
  range (±64 the whole).
- An LFO runs once per track, all its voices following it (not one per voice). The LFOs are worked out every 4 blocks (2.9 ms); then the track's sound is cooked again with the LFOs'
  values, one sound in turn every 4 blocks (tracks with LFOs on, and voices with step locks).
- The cost, measured on the FM-1 with three LFOs on each of six tracks: 2.0 % of the CPU on average (the steps
  0.75 %, the cooks 1.28 %); the stress arrangement's mean load 73.3 % -> 74.9 %, no half late. Done every block,
  it was 8.1 %: a cook in the audio interrupt takes about three times what it takes in the main loop.
- HOME: the recipes' LFOs from SynthSeeder (the drone, pad and texture families, FM LEAD; those on PAN left out,
  the sound has none an LFO reaches), and its sample-and-hold LFO on the filter's FREQ for the other families but
  the kicks (held per hit on percussion, a slow drift on sustained sounds; its swing down kept above FREQ 30).
- MIDI LEARN: the LFOs' rows learn per track; DEST cannot be learned.

Changed
- Sessions hold each sound's LFOs (24 bytes a sound). Sessions saved by earlier builds load with the LFOs off,
  including those of builds 38 to 49, whose loops stay theirs.

## Build 49

Added
- USB audio: the FM-1 is a class-compliant USB audio input (FM-1 Bigorna) carrying its master output, stereo,
  16-bit, 44.1 kHz -- as it is rendered, with no resampling. It is Felucca's and SLOOP's USB audio, switched on, fed
  by the render (each block as it is made) and served at every 1 ms USB frame, also while a half buffer renders; its
  ring is sized for Bigorna's 256-frame halves. The stream is the master before the MASTER knob (the knob sets the
  jack's level only). Streaming costs 1.8 % of the CPU (measured: idle 24.1 % -> 25.9 %; six tracks of eighth notes
  peak at 99.7 %, no half late, no frame repeated). SLOOP's 48 kHz option is left out: its resampler cost 6.7 %, and
  under heavy playing it made halves late. MIDI goes on as before on the same cable; the update loader is unchanged.

## Build 48

Added
- MIDI LEARN: the 4th black key (the step view off) turns it on or off (lit while on; MIDI LEARN in the name
  band). The EDIT pages and the effects' pages then show, where each value goes, the CC learned for it, in amber,
  or --; the title shows CC. PRESETS moves through the rows A to H; OCT− and OCT+ turn the pages. SAVE: the row
  waits for a CC (WAIT), and the next one in, from USB or the jack, any channel, is learned for it. SHIFT + SAVE
  forgets it. A learned CC turns its parameter (a sound's on the track it was learned on, an effect's for all)
  before the CC map. Up to 64, kept in the flash with GLOBAL SETUP when MIDI LEARN is turned off; settings from
  builds 46 and 47 are read as they were.
- BACK at the top of each level below the top of GLOBAL SETUP, and of a session's page: SAVE on it goes back a
  level. A level opens on the entry below BACK.

## Build 47

Added
- MIDI IN on the FM-1's 3.5 mm TRS jack: notes, CCs, program changes, the clock, START, CONTINUE, STOP and the song
  position, routed as USB's (not SysEx). The parser is MELODEE 1.4.5's (its RX verified on an FM-1): UART1 at
  31250 baud into a DMA ring, read by content, polled at 2 kHz from the timer interrupt, nested in the audio
  render, so a clock byte is timed as it comes.
- GLOBAL SETUP > MIDI > PORTS: USB IN, TRS IN (both on). Settings saved by build 46 are read as they were.

## Build 46

Added
- GLOBAL SETUP: SHIFT + SAVE (outside the sessions' pages) opens the machine's own settings, kept in the flash
  (0xFD000) and read at every start. PRESETS moves, SAVE goes in or sets, SHIFT + SAVE goes back.
- MIDI over USB, set in GLOBAL SETUP > MIDI after the Model:Cycles' MIDI CONFIG:
  - SYNC: CLK IN, CLK OUT (the clock with START, CONTINUE, STOP and the song position), PCH IN, PCH OUT.
  - IN CHAN: each track's channel (1 to 6 at first), AUTO (the chosen track; 16), PCH.
  - OUT CHAN: each track's channel (1 to 6), PCH.
  - RECEIVE: NOTES, CC (on). SEND: NOTES, CC (off).
- Clock in: the tempo is the average of the last 24 pulses, and the sequencer is held to the pulses as they come
  (to where the master is, block by block, never past the next pulse before it arrives), so it does not drift.
  START plays from step 1 on the next pulse, CONTINUE from the song position, STOP stops; PLAY starts on the next
  pulse. The TEMPO page shows EXT TEMPO. No pulse for 0.5 s: the FM-1's own tempo again.
- Clock out: 24 pulses a beat from the sequencer, to within an audio block (0.73 ms), stopped too; SPP 0, START
  and the first pulse with step 1; STOP.
- Notes out: every note a track plays, on its OUT channel with its velocity and note off; none from a muted track.
- CCs in and out: level 7, pan 10, mute 94, and each page's eight parameters (the table in the manual).
- Program changes 0 to 15: the session's patterns 1 to 16, in and out.

Changed
- USB MIDI notes no longer play the chosen track on every channel: a track's IN channel plays it, the AUTO
  channel (16) the chosen track.
- The update loader is unchanged.

## Build 45

Changed
- BREAK leaves the loops alone again, as before build 41: they play on as recorded while the key is held, and the
  pattern falls back in with them when it is let go. What builds 43 and 44 did to the loops at release goes with
  it.
- BREAK stays on the pattern's beat:
  - A track goes into the break on its next beat (its next half bar for REPEAT 8), playing as written until then.
  - REPEAT windows are 1, 2, 4 or 8 steps (3 is gone: it cycled against the beat).
  - REVERSE keeps each window's first step in place (0 3 2 1): the beat no longer moves to the step before the next.
  - CHOP shuffles the bar's four beats (no longer 2-step slices, which put beats on the offbeat).
  - A new plan, after two bars held, is drawn on the next bar line, not mid-bar.
- Let go of BREAK: the ratchets and echoes it had started stop at once, so the pattern is back as the key comes up.

## Build 44

Fixed
- After BREAK, a loop's notes no longer sound off the beat. Build 43 struck the loop's held notes again at the step
  the key was let go on (a chord from beat 1 heard on step 3, say). Now nothing is struck at that point: the notes
  the break left sounding stop, the loop carries on from its next note on its own step, and a note held over the
  next bar line (a chord, a pad) comes back on that downbeat for the rest of its length.
- With the sequencer stopped after more than 32 bars of playing, a loop armed and started from the first note
  ended at once, on that note: the looper measured it from the last run's step count. The count now resets when
  the sequencer stops.

## Build 43

Fixed
- After BREAK, a loop sounded out of place until its next notes came round: the notes the break had started (a
  chord from another bar, held for its full length) kept sounding, and the loop's own notes that should have been
  sounding at that point did not come back. At the first step after the key is let go, the notes the break left
  sounding on its tracks are now let go (its echoes and ratchets dropped), and each loop's notes due to be sounding
  then start again for what is left of their length.

## Build 42

Changed
- REC ends a loop at the nearest whole number of bars (16 steps each; one bar at least, 32 at most), so a loop
  never drifts against a 16-step pattern. The loop's first step stays where its first note was played, and it
  carries on from where it is in its length: ended short of the bar line, it plays its last steps and then comes
  round; ended past it, the notes past the length fold onto the loop's first steps. LOOP n and the bars show
  (LOOP 1  4 BARS).

## Build 41

Changed
- BREAK takes in the loops of the tracks under it. A loop is broken up as its track's steps are: the same way,
  from the same beat, with the same gap before the bar's end, so loop and pattern stay together while the key is
  held. Each loop's own position runs on underneath, as the steps' does: let go, both carry on in step. A drone's
  loops, like its steps, are left alone.

## Build 40

Fixed
- Sessions saved by builds 35 to 37 are found again. Builds 38 and 39 looked for them by the length of their data
  without the 2 bytes of padding those builds saved after it (2778 bytes, not 2780), so their slots showed as
  empty. The sessions were not changed in the flash; a slot saved into since build 38 holds the new session.

## Build 39

Fixed
- On the sessions' pages SAVE acts as it is pressed again, as before build 37. Since build 37 it acted when let go,
  and a press of 0.8 s or more counted as SAVE held: on LOAD it saved the session in place instead of loading the
  one chosen. SAVE held now saves only from the other pages.

## Build 38

Added
- A session now holds its loops: SAVE stores every loop of every track (its notes, length and mute), and LOAD
  gives them back, starting at the next bar if the sequencer is playing. A session with no loops, or saved by an
  earlier build, empties the loops when loaded.
- The loops are kept in a loop store of 31 sectors of flash shared by the 16 sessions; a session takes as many as
  its loops need (up to 7, every loop full). If the store has no room, nothing is saved and NO ROOM FOR LOOPS
  shows. New loops are written before the session that points at them, so a save cut off by power loss leaves
  the session as it was.
- The FM-1 starts with the last session loaded or saved (SESSION n shows). After a start that failed, it starts
  empty instead.

Changed
- A session also keeps the track chosen, and loads at the pattern it was saved playing (was: pattern 1).
- COPY copies a session's loops with it.

## Build 37

Added
- BREAK: in the track's view (the step view off), the 3rd black key held breaks up the chosen track's pattern;
  SHIFT and the key, every track. The steps are not changed: each track's position runs on underneath, and let go,
  the pattern continues from where it would have been. Each press draws a way of playing the steps (REPEAT 1, 2,
  3, 4 or 8, HALF TIME, DOUBLE, REVERSE 4 or 8, CHOP, ROLL), shown in the name band, and another every two bars
  held. Drawn with it: ratchets that get faster the longer the key is held, flams on drums, octave leaps on melodic
  tracks, a gap before the bar's end, accents. A kick keeps the bar's first step; a drone is left alone. After the
  break key of MOTOR:CYCLES.
- SAVE held 0.8 s saves the session at once. A session never saved: the first empty slot's page opens with SAVE
  NEW? and YES chosen; SAVE saves it there. All 16 slots taken: NO FREE SLOT. HOME held does the same.
- Out of the step view the 3rd black key no longer plays a note in CHROMATIC (it is BREAK's).

Changed
- The looper: a loop up to 32 bars (was 16) and 256 notes (was 128); a note can be held for the whole loop.

## Build 36

Changed
- MUTE mode: the soloed track's button held 1 s again ends the solo -- the mutes as they were before it. Another
  track's button held while one is soloed solos that one instead (ending it then still gives back the mutes from
  before the first). A track muted or played by a short press ends the solo there (nothing to give back).
- The CHORD key is lit dimly in the track's view on every track (DRUM's too, where it says NO CHORDS), so it shows
  it has a use there.

## Build 35

Added
- MUTE mode: in the track's view, the step view off, the 2nd black key turns it on or off (lit while on, dim off;
  MUTE MODE in the name's band while on). The tracks' buttons then mute rather than choose: lit, a track plays;
  dim, it is muted. A button let go soon mutes its track or plays it again; held 1 s, its track is soloed (every
  other muted). Off, the mutes stay as they were set, and the buttons choose tracks again. Leaving the track's
  view (or the step view on) turns MUTE mode off.
  - A muted track goes silent over ~9 ms (no click), its sends with it; its sequencer plays on (it comes back in
    time). A muted track ducks no other (SC SOURCE).
  - The chosen track's number is dark grey while it is muted.
  - The mutes are kept with the session; sessions from before load with none muted.
- Out of the step view the 2nd black key no longer plays a note in CHROMATIC (it is MUTE mode's).

## Build 34

Changed
- The machines renamed: FM TONE is 4-OP, FM DRUM is DRUM, WAVETONE is WAVY, SWARMER is HIVE.
- No CHORD mode on DRUM: the chord key says NO CHORDS (its light dark), a white key plays its note alone, and a
  chord step plays its root alone on a DRUM track.
- 1 CORE on the screen at start-up when CPU1 is held (after a fault on it, until power-off; or after a failed start),
  and when CPU1 is retired as it plays: the voices then have half the room.

Fixed
- Clicks and pops with a pad's chords and a kick (heard on build 33): the FM-1 was rendering on one core -- CPU1
  held since the fault test of build 32, until a power-off. Measured on one core (a kick each beat, a HIVE drone's
  chord each beat): 65 late halves in 20 s (the audio short: clicks), the worst half 118 %. On two cores: none late.
- Two chords with a note in common, played legato: letting go the first could let go the second's copy of that
  note (the first one found), leaving the first's ringing. A note let go is now the oldest held of that note.
- A voice taken for a new note (all twelve sounding) is the quietest of those releasing (one already fading first),
  no longer the oldest: often a loud pad's tail, cut in 9 ms.
- The overload guard fades a tail over ~12 ms (6 before).

## Build 33

Added
- CHORD mode (after TONITOR's): in the track's view, the step view off, the 1st black key turns it on or off for
  the chosen track (lit while on, dim off). With it on, each white key plays a triad from its note, in the scale
  chosen: TRIAD, INV 1, INV 2, SUS2, SUS4, OPEN or FOURTHS (in CHROMATIC, as semitones: a major triad and so on).
  SHIFT and the key: the next form (and on). The form shows at the top left, under the pattern, in the track's
  colour.
  - A step put in (the step view) takes the chord last played on the track; REC records each chord into its step
    (its root and form); the looper records a chord's three notes. A chord step plays its three notes, with its
    ratchets, MUTATE (the root moved, the chord with it) and the MIDI FX's later notes.
  - Each track's CHORD mode and form are kept with the session; chord steps with the pattern (a byte more a
    chord step). Sessions and patterns from before load with CHORD mode off and single notes.

Changed
- A track's button on the effects' pages (CHORUS, COMP, DELAY, REVERB) goes back to the track's view. SHIFT and
  a track's button chooses the track and keeps the page (for COMP's SC SOURCE and SC LEVEL, the chosen track's).
- The 1st black key no longer plays a note in CHROMATIC out of the step view (it is CHORD mode's).

Fixed
- The octave at the top right (-1, +2 ..) was drawn again only when the track changed: OCT- / OCT+ now show at
  once. (Track 1 starts an octave down, as before: -1.)

## Build 32

Changed
- The FM-1's second core (CPU1) renders half the voices and all the effects (after Melodee 1.4.5, keremimo/melodee:
  its CPU1 start, hold, fault handling, job mailbox and the stock supply setup; LICENSING.md). Each block, after
  the events and the sequencer, the sounding voices are dealt to the two cores by their cost, the dearest first;
  each renders its voices' machines and chains into buffers of their own; the tracks then fade, level and mix
  them on CPU0 as before. Measured on the FM-1:

  |                                                         | build 31            | build 32            |
  |---------------------------------------------------------|---------------------|---------------------|
  | kick + 3 SWARMER notes                                  | 70 % mean, 77 % peak | 43 % mean, 47 % peak |
  | kick, a 3-note SWARMER pad, METAL HAT 16ths, FM LEAD and FM BASS 8ths | to ~133 %, the pad cut, late audio | 69 % mean, 83 % peak, nothing late |

- The effects (chorus, delay, reverb) run a block behind the tracks, from the sends they made the block before: on
  CPU1 beside its voices. Their returns come 0.73 ms later than before (a pre-delay of 32 samples); otherwise the
  same. Alone, CPU0 runs them in the same order: the output is the same with one core or two.
- At start-up, the stock firmware's supply setup (V15's, as Melodee applies it): the core rail at 1.38 V, VDC14
  1.60 V, VDDIO 3.2 V. CPU1 starts only if it held, never after a failed start, and never once CPU1 was retired.
- A CPU1 fault, stall or misread retires CPU1 until power-off: it is held, CPU0 renders everything from then on
  (as in build 31), and the audio goes on (tested: a fault forced on CPU1; no reset, USB answering). A bus fault
  of CPU1's (an unmapped access, which the debug unit names no core for) is told from CPU0's own by CPU1 saying
  so in the handler.
- FM TONE and FM DRUM run one copy of their operator loop for each algorithm, its wiring constant (the tests
  for which operator modulates which, each sample, gone): FM TONE's machine about 3 x faster (in the
  arrangement above, 32 % of a block to 11 %), FM DRUM's ~30 %. The same output, bit for bit.
- The chain's drive and multimode filter, the operators' phases and feedback, and the tracks' DC blocker keep
  their state in registers across a block; a track's sends at 0 are skipped. The same output, bit for bit.
- The overload guard counts a new voice at half its cost while CPU1 is on.
- The code RAM grows to 0x01C07F00 (the debug area below crt0's STATUS, as Melodee uses it); the chorus line
  moves to ordinary RAM.

Not changed
- The update loader is the same, byte for byte (the CPU1 code is the app's only: FM1_CORE1).

Testing
- After installing, switch the FM-1 off and on: a CPU1 retired on this unit (the fault test) stays held until
  power-off.
- `tools/build.py --telemetry` and `tools/telelog.py status | pads | arr | crash1 | crashrec`: CPU1's state
  (on, its jobs, misreads, faults, its share of the time), the block's phases, and a forced CPU1 fault.

## Build 31

Fixed
- A SWARMER triad against a kick cut the kick to a dry transient: the FM-1 ran out of time with four voices and
  the voice budget's guard let the kick's tail go. Kick + three pad notes (DRIVEN KICK, SWARM DRONE) took up to
  106 % of a block's time; it now takes ~70 % on average, ~77 % at most, and nothing is cut. Measured on the FM-1
  (a diagnostic build, `tools/build.py --telemetry`, read with `tools/telelog.py`): where a block's time went,
  per 0.73 ms block:

  |                      | build 30 | build 31 |
  |----------------------|---------:|---------:|
  | reverb               | 20.9 %   | 11.1 %   |
  | FM DRUM kick (voice) | 13.9 %   | 12.7 %   |
  | SWARMER, each note   | 10.5 %   | 9.9 %    |
  | kick + 3 pad notes   | 83 % mean, 106 % peak | 70 % mean, 77 % peak |

  - The reverb's tank runs at half the rate (22.05 kHz): its input filtered at 44.1 kHz, two samples averaged a
    step, its output brought back up between steps; the modulation's LFOs once a block, a line between. The same
    lengths, decay and shelf (its coefficient converted for the rate); it differs from the old by ~2 % of its
    energy, above ~11 kHz.
  - SWARMER sums its six oscillators per wave and crossfades the two sums once a sample, not each oscillator
    (the same output within rounding, 73 dB down).
  - FM DRUM skips its noise once the noise's envelope is at 0, and the transient once past its longest (20 ms):
    the same output, bit for bit.
  - The audio's hot code (the machines, the chain, the tracks' block, the chorus, delay and compressor) runs
    from RAM, and the short per-sample loops are unrolled by 4: fewer of the slow peaks (a block's worst case
    107 % to 86 %).
- The update loader is always built at -Os: an -O2 loader never answered, and an install stalled in update mode.

Not yet
- A fuller arrangement (kick, a pad triad, hats, an FM lead, an FM bass at once) still overloads: up to ~130 % of
  a block, the pad's notes cut. Voices cost ~10 to 14 % of a block each, the effects ~17 %.

Tools
- `tools/build_install.sh`: build; install only when the build printed its package line and no FAIL; then check
  that the FM-1 answers over USB.
- `tools/telelog.py pads | arr | bench`: the diagnostic build's reports (a kick and 1 .. 4 pad notes; the
  arrangement above; the chip's costs measured at start-up).

## Build 30

Fixed
- Sidechain ducking was barely audible, and SC LEVEL 50 and 100 sounded alike:
  - A ducked track's reverb, delay and chorus were not ducked. Only what it sent into them was, so the shared reverb
    kept ringing through every dip. Each effect's return is now ducked by the tracks that feed it, weighted by how
    much each one sends (followed over ~0.7 s, the length of a tail). A return fed only by the ducked track ducks
    fully; a return the source also feeds ducks by the ducked track's share.
  - The duck let go about 30 ms after the hit, at the compressor's RELEASE, while a kick's body still sounded for
    200 ms or more, so the dip sat under the kick and was masked. The duck now follows the source's level after the
    hit: all of SC LEVEL at the hit's peak, less as the source falls, none once it is 12 dB below the peak (RELEASE
    is the fastest it can let go). With a kick each beat at SC LEVEL 100, a pad is held down through the kick's body
    and swells back in before the next beat; at 50 it is held about 20 dB down.
- Host duck: the demo kick no longer has a reverb lock (KICKREV=1 puts it back); KICKMUTE=1 renders the pad alone.

## Build 29

Changed
- Sidechaining is now each track's own. On the COMP page's second half (PRESETS right), SC SOURCE and SC LEVEL are the
  chosen track's, shown in its colour; a track's button on the effects' pages chooses the track and keeps the page.
  - SC SOURCE: OFF (the track is never ducked), or T1 to T6: the track whose hits duck it (not itself).
  - SC LEVEL: 0 to 100, how deep: up to ~45 dB on each hit at 100, near a gate -- the hit slices the track -- and
    about 10, 20, 27 dB at 25, 50, 70 (host duck renders a kick and a SWARMER drone at any level).
  - A hit is each note the source track strikes (the sequencer's, the keys', MIDI's): the duck comes down at the
    compressor's ATTACK as the source sounds its first ~30 ms, then lets go at its RELEASE whatever the source still
    sounds -- one duck a hit, never a second as a kick's body swells.
  - The duck works on the track itself and on what it sends to the delay and the reverb (a tail already in the
    reverb rings on: the reverb is all the tracks').
  - Kept with the session; sessions saved before load with every track OFF (SC LEVEL 50).
- The master compressor no longer has a sidechain: it listens to the whole mix. COMP page: THRESHOLD, ATTACK,
  RELEASE, RATIO | MAKEUP, MIX, SC SOURCE, SC LEVEL.

Removed
- SC FILTER (it did nothing worth hearing) and the compressor's own SC SOURCE.

## Build 28

Fixed
- A kick still lost its body, with a pop, as a second note was added to a pad on another track: the FM-1 ran short of
  time and the voice budget's guard let the kick go (its gate over, its body counted as a tail) in 2 ms. Each voice
  cost too much, and most of it was not the machine but its chain:
  - OVERDRIVE ran at twice the sample rate (since build 16) -- half the chain's time, even at a drive barely heard. It
    runs at the sample rate now (the same two stages, emphasis, bias and tone; the level curve measured again, within
    about 1 dB), and below 2 it is off.
  - LOWPASS 4 solved its feedback exactly each sample; it now takes it from the sample before, as a classic ladder
    (the feedback eased as the cutoff nears the top, where that delay would let it run away). Its resonance moves a
    little.
  - mulq16, the multiply most of the filters use, is always inlined now, as the others were.
  A pad voice's chain now takes ~40 % less time (on the host: a SWARM DRONE voice 1.1 to 0.7 us a block; a kick and
  two pad notes 4.4 to 3.3 us with the effects).
- The guard lets tails go only past 90 % of the block's time (82 % before: there was room), and over ~6 ms, not 2 (no
  click).

## Build 27

Fixed
- The sidechain still did not pump: with a heavy kick as SC SOURCE the rest of the mix was held down ~12 dB nearly the
  whole beat -- the compressor followed the kick's long body (whose swells, as FM beats, kept striking it again) --
  so a pad sounded steady, only quieter; and the source track, left out of the compression, also missed the MAKEUP,
  so choosing it made the kick ~3 dB quieter. Now a track as the source is followed for its hits: a hit strikes the
  level (its first ~30 ms followed up to its peak), which then falls at RELEASE whatever the track still sounds; the
  next hit counts once the track has fallen 15.5 dB (a kick's swelling body does not), or at once if twice as loud.
  The source track gets the makeup. A heavy kick each beat now ducks the rest by ~9 dB on the hit, back to nothing
  ~200 ms later (at 120 BPM the pad is back for the last ~300 ms of each beat): a pump. The whole mix as the source
  works as before.

Note
- The source track's own reverb and delay returns are part of the mix the compressor works on, so they duck too.

## Build 26

Fixed
- A kick lost its body (only its transient left) as soon as a second pad note was held. Each kick took a new voice
  while the kicks before still rang, so a kick each beat filled all 12 voices with tails; the load rose, and the
  voice budget's guard let go of the released voices -- a sequenced kick's gate ends early, so its body counted as a
  tail and went. Now:
  - an FM DRUM track chokes, as a drum machine's: its last hit fades out in ~2 ms as the next one strikes (a kick each
    beat and a pad: 2 to 4 voices sounding, it was 12);
  - the guard lets go of the quietest tails first, and of a voice struck in the last ~150 ms only if there is no other.
- The compressor's sidechain still barely pumped: the compressor sat before the delay's and the reverb's returns, so
  a pad's reverb and delay tails -- much of what is heard of a pad -- never ducked. It is now the master's, after
  the returns (the source track's own dry sound still left out of it). A kick each beat as the
  source now takes a held pad, tails and all, from about -12 dBFS down to about -26 dBFS on each hit, swelling back
  over the beat (host duck); before, the pad as heard hardly moved.

## Build 25

Fixed
- The compressor's sidechain hardly pumped: with a kick on a track as SC SOURCE it took 4 dB off at most, and only
  ~90 ms after the hit, while MAKEUP lifted the rest back up. Now:
  - the level is caught at its peaks at once (falling at RELEASE), and ATTACK sets how fast the gain comes down;
  - a track as SC SOURCE is listened to 12 dB louder (one track is quieter than the whole mix), and that track is
    left out of the compression: the rest of the mix ducks under it, the kick itself keeps its punch.
  A kick on track 1 each beat now ducks a pad by about 12 dB, back between the beats (host duck).
- The compressor's start THRESHOLD is -9 dB (it was -12): with the peaks caught at once, the whole mix as the source
  takes 1 to 3 dB off on the whole, 5 to 6 dB on the hits. Sessions keep the values they were saved with.

## Build 24

Added
- COMP, a master compressor, on the effects' pages (SEQ): the pages now run
  CHORUS, COMP, DELAY, REVERB. It works on the mix with the chorus's return in it, before the delay's and the
  reverb's returns are added. A-D: THRESHOLD (-60 to 0 dB), ATTACK (0.03 to 30 ms), RELEASE (10 ms to 2 s), RATIO
  (1.5, 2, 3, 4, 6, 8, 16, 20). E-H (PRESETS right): MAKEUP (0 to +24 dB), SC FILTER (bipolar: to the left a
  low-pass on what it listens to -- the bass, so it pumps with the kicks; to the right a high-pass), SC SOURCE (MIX,
  or a track T1 to T6: the mix ducked under that track), MIX (dry to wet). Stereo, linked.
  Its start, for pounding drums: -12 dB, 10 ms (the kick's attack through), 100 ms, 4:1, +3 dB, listening below
  about 200 Hz, fully wet; on the phrase renders it takes 1 to 3 dB off on the whole and 4 to 6 dB on the hits.
  Kept with the session; sessions saved before load with the start values.

Changed
- The effects' pages' order: CHORUS, COMP, DELAY, REVERB (it was REVERB, DELAY, CHORUS).

## Build 23

Added
- MOD, on the step view's fourth black key from the left (lit while on; TRIG goes off with it, and the other way
  round): a step held shows its MOD page.
  - KNOB 1 MUTATE: the step's note moved -N to +N steps of the scale (N its notes in an octave), OFF in the middle,
    or RND (at the right end): a step of the scale drawn at random, up to an octave either way.
  - KNOB 2 MUT EVERY: 1 to 16, how often MUTATE happens, counted in the times the step itself plays (its condition
    met), from PLAY.
  - KNOB 3 MIDI FX (after TONITOR's looper VARY and BRICKLAYER): ECHO (two echoes, quieter), ECHO UP and ECHO DN
    (each a step of the scale higher or lower), STUTTER (three quick repeats), FLAM, LATE (a quarter of a step
    late), GRACE (a step of the scale below just before it), FIFTH and TRIAD (with it), OCT UP, OCT DN, DROP (not
    played).
  - KNOB 4 FX EVERY: 1 to 16, counted the same way.
  MOD is kept with the pattern and copied with the step. COMPLEXITY above 50 also draws MUTATE and MIDI FX now and
  then, by role.

Changed
- The track's directions (REVERSE, PING-PONG, RANDOM) moved to the sixth, seventh and eighth black keys from the
  left; the fifth is free.

## Build 22

Fixed
- Leaving a pattern lost what had been done in it since it was last saved: copying pattern 1 (never saved) and
  pasting it into pattern 2 left pattern 1 empty. Every pattern now keeps its changes when another is loaded (in the
  session's memory; P1 shows orange while they are not saved). ARP held saves the pattern playing into the flash;
  a session's SAVE saves every pattern.

Changed
- Switching patterns while the sequencer plays waits for the chosen track to come round to its first step; then every
  track starts the new pattern from its first step together. Meanwhile the pattern waiting blinks on its white key
  (PATTERNS grid) and in turn with the one playing at the top left. Choosing the pattern playing again, or another,
  replaces the one waiting. Stopped, a switch is at once.

## Build 21

Added
- Copying a pattern: on the PATTERNS grid (ARP), a white key held 0.8 s copies its pattern (the one playing as it
  plays, its changes too). COPY P5 shows above the grid, blinking, and the key blinks. Another white key held 0.8 s
  pastes it there: saved into the session (if it has a slot), loaded, PASTED shows. The same key held again, or
  leaving the grid, lets the copy go.
- Copying a step: in the step view, SHIFT and a step held 0.8 s copies it (note, velocity, length, condition, ratchet,
  locks). COPY S5 shows, blinking, and the key blinks. SHIFT and another step held 0.8 s pastes it there, on the track
  chosen then (another track too); PASTED shows. The same step again, or the step view off, lets the copy go.
- The TEMPO page: PLAY held 0.8 s shows the session's tempo, large; PRESETS sets it. PLAY held again, a track's
  button, or another page takes it down.

Changed
- PLAY starts and stops the sequencer as it is let go (so that it can be held).
- On the PATTERNS grid, emptying a pattern is SHIFT and its white key held 0.8 s (held alone now copies).
- In the step view, SHIFT and a step key no longer places or empties the step (SHIFT is for copying).

## Build 20

Added
- The PHRASE page: in the step view, the phrase key (first black key) held 0.8 s brings it up and down (the key lit
  while up). KNOB 1 the track's DENSITY, KNOB 2 its COMPLEXITY, 0 to 100, 50 as the generator drew before. DENSITY
  below 50 thins the phrase out (the strong steps, on the beat and loud, go last); above 50 it adds ghost hits between
  the steps, soft and short, often on a condition. COMPLEXITY below 50 takes locks away; above 50 adds more, as the
  track's role locks; from 40 up it adds ratchets now and then (on hats, snares, percussion and noise most). SHIFT +
  the phrase key (new locks) follows COMPLEXITY too. A short press of the phrase key still makes a new phrase, the
  page staying up. DENSITY and COMPLEXITY are kept with the pattern, a track each.
- Ratchets: a step can play 2, 3, 4, 6 or 8 times, evenly through it, each hit a little softer (on its own clock,
  cut by the next step). Set on TRIG's other half (PRESETS right): RATCHET on KNOB 1.
- The track's direction: in the step view, the fourth black key from the left plays the track REVERSE, the fifth
  PING-PONG (both ends once), the sixth RANDOM; the key lit is the direction, pressing it again goes back to forward.
  A:B and 1ST count the times through in every direction. Kept with the pattern. The seventh and eighth black keys
  are free.

Changed
- TRIG is back, on the third black key from the left: on (lit), a step held shows its own page (NOTE, VEL, LEN, COND;
  PRESETS right: RATCHET). Off, a step held locks what the knobs set on the track's view (FREQ, RESO, the delay and
  reverb sends; SHIFT: chorus, overdrive), as in build 15; the EDIT pages still lock any parameter.
- The phrase key acts when let go (so it can be held for its page).
- A pattern keeps each track's direction and phrase settings and each step's ratchet (patterns saved before still
  load, forward, 50, 50, no ratchets).

Fixed
- A new phrase on a pattern already full of locks on other tracks could hang the generator; it now takes locks from
  the other tracks, then this track's softest notes, until the pattern fits.

## Build 19

Added
- The TRACK page: in the step view, the second black key from the left brings it up and down (lit while up). KNOB 1
  sets the track's LENGTH, 1 to 16 steps; KNOB 2 its CLOCK against the session's tempo, a bipolar row with 1X in the
  middle: 1/8, 1/4, 1/2 to the left, 2X and 4X to the right. Each track runs on its own clock; a step's length
  follows its track's clock. The metronome, the looper and MANGLE's slicer keep the tempo's. LENGTH and CLOCK are
  saved with the pattern (patterns saved before play at 1X). In the step view, the keys past the track's length are
  dark. Notes recorded with REC go to the nearest step of the track's own clock.

Changed
- Ranges: the parameters that ran 0 to 127 show 0 to 100, and the bipolar ones -50 to +50 with 0 in the middle; a
  knob's click is one of those. This covers the EDIT pages, the knobs' dials, locks, the reverb, delay and chorus
  pages, and a step's velocity. The sound keeps its values as before (sessions, patterns and locks are unchanged);
  only what shows and how far a click goes changed. Lists, ratios, notes, the delay's time and the parameters with
  ranges of their own are as they were.
- Every page of rows (EDIT, effects, the step's, TRACK) shows its names in the same small size and its values in the
  next size; a value too wide for its row shows in the names' size.

## Build 18

Added
- MANGLE, a page of its own after FX (in place of GRAIN), working on the track's voices together:
  - DESTROY: digital destruction on the samples as 16-bit words, its amount (0 off) and kind: WRAP (driven up to
    x 16 and left to overflow, wrapping round), XOR (the low bits XORed with a pattern, more of them as it goes up),
    FLIP (one bit flipped, a higher one as it goes up), DROP (samples dropped and held, more often and longer).
  - SLICE: a gate in time with the tempo (the sequencer's steps; stopped, its own clock at the tempo): pattern OFF
    or 1 to 16, SLICE RATE 1/8, 1/16, 1/32 or 1/16T, SLC DEPTH (127: cut to silence), SLC EDGE (0: a click, about
    0.3 ms; 127: about 30 ms), SLC SHIFT (the pattern turned by slices).
  Both cost a few operations a sample and no buffer.
- Parameter locks on any parameter of the sound. In the step view, hold a step and bring up the EDIT pages: what
  the knobs turn there is locked on that step instead of changing the sound. The page shows the step's values (S5
  next to the page number): a row's bar in the track's colour is locked, grey is the sound's own. Up to eight locks a
  step (8 LOCKS shows when a ninth is refused). Machine, filter, amp and FX parameters belong to the note; the sends,
  MANGLE and the pan hold on the track from the step until the track's next step plays.
- The phrase generator's SHIFT + phrase key (new locks) also locks DESTROY and SLICE now, by role (slices on drones
  and noise most).

Changed
- With a step held and the EDIT pages not up, KNOB 1 to 4 set its note, velocity, length and trig condition (the
  page of four rows shows); SELECT and ALGORITHM still lock its level and pan.
- The fourth and fifth black keys no longer choose TRIG and FX pages: they are free.
- A pattern keeps its locks as parameter and value pairs (patterns saved with builds 9 to 17 still load).
- A session keeps MANGLE with each sound; sessions saved before build 18 still load (MANGLE off).
- A title that does not fit beside its page number shows in the smaller size.

Removed
- GRAIN (build 17's granulator), and its 24 KB of buffers.

## Build 17

Added
- GRAIN, a granulator after TONITOR's, on the FX page in SRR ROUTE's place (knob D): -63 to +63, the middle off.
  Left goes towards dark ambient (long overlapping grains, drifting, pitched down now and then, reversed, darkened,
  fed back, freezing now and then); right goes towards scattering (short, fast, jittered grains, reversed,
  stuttered, dropped out, decimated at the far end). It works on the track's voices together, set by the sound the
  track plays, and rings on about 2 s after the last voice. Each track's granulator has its own character. Its
  buffer is 0.19 s a track (TONITOR's 0.56 s), so the dark side's longest grains are about 160 ms.

Changed
- SRR is always before the filter; SRR ROUTE is gone. Sounds saved with SRR ROUTE on POST load with GRAIN at +1.
- The EDIT pages: OCT- / OCT+ move between the pages, PRESETS between a page's two halves (turned right E-H, left
  A-D); SHIFT no longer shows E-H there. The effects' pages work the same (OCT moves between REVERB, DELAY and
  CHORUS, as SEQ does).
- Every EDIT and effects page shows its names in the smaller size (SYN 2 and AMP showed them larger).

## Build 16

Changed
- OVERDRIVE rebuilt after TONITOR's distortion: two gain stages as in a valve preamp (the first symmetric, the
  second clipping sooner on one side, for even harmonics), at twice the sample rate; the highs lifted before them
  and lowered after, so the mids and highs distort more than the bass; the first stage's bias follows the input's
  peak, so the wave stays lopsided however hard it is driven; a low-pass that closes from 16 kHz to 6.5 kHz as the
  amount goes up. The level is brought back by a curve measured on test signals (within about 1 dB from 10 to 100),
  so turning it up adds grit, not loudness. It is still per voice, before or after the filter (ROUTE), and lockable
  per step. It costs more processing than the old one.
- OVERDRIVE runs 0 to 100 (it ran 0 to 127). Sounds and locks with more read as 100; the generated sounds' drive is
  scaled down in proportion.
- PRESETS on the EDIT pages moves through the pages, and on the effects' pages through REVERB, DELAY and CHORUS; it
  changes the machine only on the track's view (it changed it on every page).
- The EDIT, effects and step pages show their names in one size and their values in one size, the largest that fits
  every row of the page (both halves).
- The page indicator at the top right (3/8, A-D) is larger and white.

## Build 15

Added
- Parameter locks by hand, in the step view, on the step held:
  - With no black key chosen, the four knobs lock what they set on the track's view: KNOB 1 FREQ, 2 RESO, 3 the
    delay send, 4 the reverb send (with SHIFT: 3 the chorus send, 4 overdrive). The lock shows as a dial (FREQ S5).
  - SELECT locks the step's level and ALGORITHM its pan, whatever black key is chosen.
  - The fifth black key from the left, FX: KNOB 1 sample rate reduction, 2 bit reduction, 3 overdrive, 4 the amp's
    decay (per-step gating), on a page of four rows. A row's bar is grey while the step plays the sound's own value,
    in the track's colour once it is locked.
  - A lock starts from the sound's own value.
- SHIFT + the phrase key (the first black key, in the step view): new locks on the steps the track holds, in the
  manner of its sound's role; notes, velocities, lengths and conditions are kept. The screen flashes NEW / LOCKS.

Changed
- The step view no longer shows the strip of steps at the bottom: the track's screen stays as it is, level and pan
  included; the white keys show the steps.

## Build 14

Added
- SHIFT + REC held 1.5 s, with no white key touched, empties every loop of the chosen track (EMPTY / LOOPS). The
  loops still show on the white keys from 0.4 s, as before. While a loop is recording, SHIFT + REC ends it and,
  held on, empties the track's loops the same way.
- SHIFT + PRESETS on the track's view: a new sound of the family before or after, on the machine the track plays
  (its family's name shows under the machine). At the machine's first or last family it stays there.

## Build 13

Added
- A looper, after TONITOR's, with up to three loops a track.
  - SHIFT + REC arms a new loop on the chosen track (REC flashes, the metronome counts). The first note played
    starts it, and the sequencer too if it was stopped.
  - Every note played on the track goes into the loop, quantized to the nearest step, with its velocity and its
    length as held; there is no bar limit up to 16 bars (a loop reaching 16 bars ends by itself).
  - REC ends the loop at the nearest step. It plays from there at once, round and round in its own length, over the
    track's steps and in time with them. LOOP shows in red at the top right while recording.
  - SHIFT + REC held shows the track's loops on the first three white keys: lit while playing, dim while muted. A
    key let go soon mutes its loop or plays it again; held 0.8 s, it empties the loop (EMPTY / LOOP n).
  - Loops play while the sequencer runs and start again from their first step on PLAY. They are not saved.
- On the PATTERNS grid (ARP), the white keys are the 16 patterns: a key let go soon loads its pattern and the grid
  stays up; held 0.8 s, it empties the pattern.

Changed
- SHIFT + PLAY turns the metronome off or on (it was SHIFT + REC, now the looper).
- SHIFT + ARP is gone: ARP alone shows the patterns on the white keys.

Fixed
- Turning PRESETS on the sessions' pages could also change the track's machine. PRESETS, SELECT and ALGORITHM
  are now each read once and go to the page shown only; on the sessions' and patterns' pages SELECT and ALGORITHM
  do nothing (the level and the pan no longer change there either).

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
    - COND: the trig condition: NONE; PRE, !PRE (the last condition on the track was
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
- Measured curves, fitted (tools/gen_tables.py; the tables are generated from the fits):
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
  - FM TONE: operator envelopes 20 s attack and 60 s decay at 127; modulation depth from a fitted LEV
    curve, looked up at LEV x envelope, 2.2 cycles at full; B's LEV split between B1 and B2 by its law; DTUN adds
    up to 0.5 to A's ratio and 0.75 to B2's; feedback up to 2/pi of a cycle, scaled by the operator's own level;
    key tracking from C1 to C7 in Hz; HARM's 26 waves as measured partial sets; algorithm 8's outputs as
    its DSP sums them (X: C and B2, Y: B1).
  - FM DRUM: decays reach e^-6 in 8 (0.1p + 0.9p^4) s, holds 1000 p^2 ms, the sweep falls in 5 (0.1p + 0.9p^4) s
    and starts 16 p^2 times the note above it (linear in Hz); MOD, FDBK, LEV and the transient level p^2, noise
    2 p^3; GRAN as impulse odds 0.0001 + (0.992 - p)^2; the folder 1.12u - 0.672 tanh(4u) at gain p^2 + 0.07;
    the 33 operator waves as measured partial sets.
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
  bandwidth and the WAVETONE wave contents are not measured yet.

## Build 1

First build. Based on Tonitor 0.46 (the HAL, boot, USB, the installer and rescue mode, the voice pool, the screen).

Added
- Six tracks, each playing one of four machines, chosen with PRESETS: FM TONE, FM DRUM,
  WAVETONE, SWARMER. Each track keeps a sound of each machine.
- FM TONE: operators C, A, B1, B2; the eight algorithms (modulation, feedback operator, X and Y outputs
  direct or through the operator envelopes); the ratio lists
  (C and B: 19 values, A: 36; RATIO B as B1 x 19 + B2); HARM, DTUN, FDBK, MIX; A and B envelopes (delay, attack,
  decay, end level, triggered or gated, reset); phase reset; ratio offsets; key tracking.
- FM DRUM: operators C, A, B in seven algorithms (B also heard in 5 to 7); ratios A and B
  linear from 0.001 to 31.75; pitch sweep, wavefolder, body, noise (grain, ring modulation, base-width band) and
  eight transient types.
- WAVETONE: two oscillators on PRIM (sine, triangle, saw, square) or HARM (16 waves), phase distortion, linear
  offsets, ring modulation, fixed ring modulation, hard sync, phase reset, drift; noise with its own envelope,
  GRAIN, TUNED and S&H types and a base-width band.
- SWARMER: a main oscillator and six detuned ones, swarm and main waveforms, detune, mix, main octave, animation
  (three LFOs, one per pair), noise modulation.
- Per voice, in this order: SRR and overdrive (PRE or POST the filter), base-width filter (PRE or
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
- Envelope times, filter frequency and resonance curves are this firmware's own estimates until measured.
