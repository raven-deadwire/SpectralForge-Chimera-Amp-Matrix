# Auto Set — locks and strength

Auto Set (phases 1–4: `AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE4.md`) can now
keep chosen settings as they are and apply its results at less than full
strength. Both are in the options button next to **AUTO SET**; the button
shows the strength when it is below 100% and `*` when anything is locked.

## Locks

**Lock (Auto Set keeps these)** lists Input gain, Gate, Tone EQ, Compressor,
Multiband, De-esser, Limiter, Dynamic EQ (phase 6, `AUTO_SET_PHASE6.md`), Final
EQ and Output level (phase 7, `AUTO_SET_PHASE7.md`), Amp (phase 8,
`AUTO_SET_PHASE8.md`: the head's GAIN, BASS, MIDDLE and TREBLE) and Cabinet
mic (phase 9, `AUTO_SET_PHASE9.md`: the modeled mic's position, distance and
level), Boost pedal (phase 10, `AUTO_SET_PHASE10.md`: the Green Drive it
places on the board), Delay + reverb (phase 11, `AUTO_SET_PHASE11.md`),
Presence + resonance (phase 12, `AUTO_SET_PHASE12.md`: the head's PRESENCE
and RESONANCE or DEPTH), Dual blend (phase 13, `AUTO_SET_PHASE13.md`: the
Dual BLEND and CROSSOVER), Stereo doubler (phase 14, `AUTO_SET_PHASE14.md`:
DOUBLER and its spread), Cabinet cuts (phase 15, `AUTO_SET_PHASE15.md`:
the Classic rig's LOW and HIGH cuts), Bus compressor (phase 16,
`AUTO_SET_PHASE16.md`: the POST Console VCA) Matrix levels (phase 17,
`AUTO_SET_PHASE17.md`: the Matrix MID and HIGH lane levels), Cabinet mic
blend (phase 18, `AUTO_SET_PHASE18.md`: the Classic cabinet's mic delays,
mic B polarity and A/B blend), Dual rig levels (phase 19,
`AUTO_SET_PHASE19.md`: the Dual rig 2 LEVEL or its native head's OUTPUT
LEVEL), Amp channel (phase 20, `AUTO_SET_PHASE20.md`: the Classic head's
channel), Lane polarity (phase 21, `AUTO_SET_PHASE21.md`: the polarity of
Dual rig 2 and of the Matrix MID and HIGH lanes), Delay tempo sync (phase 22,
`AUTO_SET_PHASE22.md`: DELAY SYNC and Follow host tempo), Cabinet IR (phase 23,
`AUTO_SET_PHASE23.md`: the factory IR of the Classic cabinet's mic A or mic B), Amp
input trim (phase 24, `AUTO_SET_PHASE24.md`: the Classic head's INPUT TRIM and OUTPUT LEVEL), Matrix
crossovers (phase 25, `AUTO_SET_PHASE25.md`: X1 and X2) and Room ambience (phase 26,
`AUTO_SET_PHASE26.md`: the Room Core switch, size, absorption, return and sends). LEVEL 2 is both the Dual rig 2 level and the Matrix MID lane level,
so either the Dual rig levels or the Matrix levels lock keeps it. A locked setting is not part of the run's
snapshot, so Auto Set never writes it and **UNDO AUTO** never restores it: a
later edit to a locked setting survives undo. A lock selected after the run
also protects its stage from undo; unlocking an originally locked stage does
not add an undo entry.

- A pass whose outputs are all locked is skipped: locking Tone EQ removes the
  Tone EQ pass (and the Dynamic EQ step, whose nodes are Tone EQ bands), locking Compressor removes the compressor pass, locking both
  De-esser and Limiter (and Bus compressor, when that step is on) removes the de-esser and limiter pass, locking Amp,
  Presence + resonance and Amp channel removes the amp pass (it serves the three steps), locking both Cabinet mic and Cabinet cuts removes the cabinet mic pass (it serves both steps), locking Boost pedal removes the boost pass, locking Dual blend, Dual rig levels and Lane polarity removes the Dual pass (it serves the three steps), locking Matrix levels and Lane polarity removes the Matrix pass (it serves both steps), locking Cabinet mic blend removes the cabinet mics pass. The input pass runs unless
  both Input gain and Gate are locked.
- Skipped passes appear in the results as "kept (locked)", in signal order.
  A partly locked pass says which part it kept ("input kept (locked)",
  "limiter kept (locked)").
- When every chosen step is locked, AUTO SET changes nothing and says so.
- Skipping the Tone EQ pass also skips its instrument estimate; with
  instrument set to Detect, later passes then use their undetected defaults.
  Choose Guitar or Bass explicitly when Tone EQ is locked.

The gate is placed for the input gain actually applied. The analysis
proposes a threshold for its proposed gain; when the input is locked or
scaled by strength, the threshold moves by the difference, since the gate
detects after input gain. If input gain is edited during listening, keep the
gate unchanged: a capture with multiple gain settings cannot be corrected
safely by one shift. Rerun after settling on an input setting.

## Strength

**Strength** (100, 75, 50, 25%) scales how far each result moves the sound.
For an undisturbed run, 100% keeps the previous suggested values. Edit-history
protection and mixed-gain capture abstention apply at every strength.

| Stage | What strength scales |
|---|---|
| Input gain | The change: before + strength × (result − before), in 0.5 dB steps |
| Gate | Not scaled (threshold follows the applied input gain) |
| Tone EQ | Every band gain; frequency, Q and type unchanged |
| Dynamic EQ | Maximum cut; frequency, threshold, ratio and times unchanged |
| Final EQ | Every band gain; the high pass is on or off |
| Output level | The trim change, as for input gain |
| Amp | Each knob change: before + strength × (result − before) |
| Presence + resonance | Each knob change, as for Amp |
| Amp channel | Not scaled: a channel is switched or kept. The choice counts only the part of the GAIN move the amp step makes at this Strength (none when Amp gain and tone is off or locked). Off by default |
| Lane polarity | Not scaled: a lane is flipped or kept |
| Delay tempo sync | Not scaled: sync is turned on or kept |
| Cabinet IR | Not scaled: an IR is switched or kept |
| Amp input trim | The INPUT TRIM change in dB, as for Amp; OUTPUT LEVEL holds the loudness for the trim applied |
| Dual blend | The BLEND or CROSSOVER change, as for Amp |
| Dual rig levels | The rig 2 level change in dB, as for Amp; the blend step then hears rig 2 at the applied level |
| Matrix levels | Each lane level change in dB, as for Amp |
| Matrix crossovers | The X1 and X2 moves in octaves (each to the nearest 1 Hz) |
| Room ambience | A room that is on: its return change in dB, as for Amp. A room that is off is opened with its return lowered by 20 log10(strength), as a mix scales (25%: -12 dB); below the return's -36 dB it stays off |
| Cabinet mic blend | The BLEND change, as for Amp; the mic delays and polarity are applied whole |
| Stereo doubler | A spread change of a doubler that is on, as for Amp; switching it on is not scaled |
| Cabinet cuts | Each cut's change in Hz, as for Amp |
| Bus compressor | The glue target (gain reduction on loud passages): strength × the style's target |
| Cabinet mic | The position and distance changes, as for Amp; the level holds the loudness of the applied placement |
| Boost pedal | OVER DRIVE and the LEVEL change in dB (LEVEL 0.5 is unity); TONE unchanged |
| Delay + reverb | The mix changes: before + strength × (result − before); time, feedback, size and damping unchanged |
| Compressor | Mix = strength (parallel); threshold, ratio and times unchanged |
| Multiband | Mix = strength (parallel); bands unchanged |
| De-esser | Range (maximum reduction) |
| Limiter | Not scaled: it protects the output |

The results row and the summary report the applied values, the mix and the
strength ("Applied (rock, 50% strength): …").

Locks and strength are session options like instrument and style: they last
while the editor is open and are not saved with the project. Execution options
are frozen at start; changes during a run apply to the next run. Newly selected
locks additionally protect undo of an earlier run.

## Tests

`Tests/AutoSetTests.cpp` (`controllerLocksAndStrength`) feeds identical
fixtures straight into the capture taps, so runs compare exactly:

- 50% against 100%: input change halved, gate threshold shifted by the input
  difference, Tone EQ gains halved at the same frequencies, compressor and
  multiband mix 50%, de-esser range halved, limiter identical.
- Input, Tone EQ, Compressor and Limiter locked: those parameters unchanged,
  passes reduced to input/multiband/finish, locked rows in signal order, the
  unlocked stages identical to the reference run, and UNDO AUTO keeps a later
  edit to the locked input while restoring the gate.
- Everything locked: refused without writing; out-of-range and non-finite
  strength values are clamped.

Listening acceptance with real DI recordings is still pending, as for the
earlier phases.

Expanded regression matrix and integration boundaries: [review](evidence/auto-set-integration-20261026/LOCKS_STRENGTH.md).
