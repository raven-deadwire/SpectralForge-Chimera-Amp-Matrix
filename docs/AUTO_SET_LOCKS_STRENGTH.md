# Auto Set — locks and strength

Auto Set (phases 1–4: `AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE4.md`) can now
keep chosen settings as they are and apply its results at less than full
strength. Both are in the options button next to **AUTO SET**; the button
shows the strength when it is below 100% and `*` when anything is locked.

## Locks

**Lock (Auto Set keeps these)** lists Input gain, Gate, Tone EQ, Compressor,
Multiband, De-esser, Limiter and Dynamic EQ (phase 6, `AUTO_SET_PHASE6.md`). A locked setting is not part of the run's
snapshot, so Auto Set never writes it and **UNDO AUTO** never restores it: a
later edit to a locked setting survives undo. A lock selected after the run
also protects its stage from undo; unlocking an originally locked stage does
not add an undo entry.

- A pass whose outputs are all locked is skipped: locking Tone EQ removes the
  Tone EQ pass (and the Dynamic EQ step, whose nodes are Tone EQ bands), locking Compressor removes the compressor pass, locking both
  De-esser and Limiter removes the last pass. The input pass runs unless
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
