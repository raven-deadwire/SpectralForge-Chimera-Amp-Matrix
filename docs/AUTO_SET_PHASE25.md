# Auto Set — phase 25 (Matrix crossovers)

Phase 25 lets Auto Set move the Matrix routing's crossovers **X1** (LOW/MID)
and **X2** (MID/HIGH). Phase 17 (`AUTO_SET_PHASE17.md`) sets the lanes'
levels; this step then judges, from the same listening, whether moving the
splits fits the Tone EQ target better. Earlier phases are described in
`AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE24.md`; locks and strength in
`AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has a **Matrix crossovers X1/X2**
step, listed after **Matrix lane levels**. It is **off by default**: the
split happens before the lanes' amps, so a moved crossover changes what each
amp is driven with, and the step can only predict that linearly (see below).
Turn it on in the options to let Auto Set move X1 and X2.

- **When it runs:** in the Matrix pass, after the lane polarity and lane
  level steps. It adds no listening time; it alone also starts the pass. Only
  in the Matrix routing.
- **What it sets:** X1 and X2.
- **What it leaves alone:** the lane levels (phase 17), polarities (phase
  21), the heads, the Dual crossover (the Dual routing has its own).
- **The results:** a **Matrix crossovers** row after the **Matrix lane
  levels** row: "X1 150 Hz > 119 Hz, X2 1.20 kHz > 952 Hz; tone error 4.7 >
  4.5 dB (predicted from the lanes heard; approximate for driven lanes)", or
  why it kept them. Its SHOW button opens the split.
- **Strength:** scales the move in octaves (each to the nearest 1 Hz).
- **Lock → Matrix crossovers:** keeps X1 and X2. Locking Matrix levels, Lane
  polarity and Matrix crossovers skips the Matrix pass.
- **UNDO AUTO:** restores both.

## Rules (`analyseCrossover`, `AutoSetCrossover.h`)

- **Heard:** the three lane captures of the Matrix pass (after each lane's
  level and polarity, so they sum to the engine output), their first 4 s as
  phase 17, and their sum.
- **Model:** each lane is taken as a gain per third-octave band behind the
  engine's fourth-order Linkwitz-Riley split (LOW = LP(X1) AP(X2), MID =
  HP(X1) LP(X2), HIGH = HP(X1) HP(X2), which sum flat); the gain is the
  lane's band level over its split response there. The lanes add coherently,
  with their polarities and the level changes phase 17 has just made. The mix
  as heard moves by the modelled change, so the starting error is the heard
  one. A band where a lane's response is more than 60 dB down keeps that
  lane's part as it is (what plays there is its amp's harmonics, not the
  split).
- **Candidates:** X1 and X2 each within a third of an octave of where they
  are, in sixth-octave steps (25 splits), within the controls' ranges (X1 60
  to 350 Hz, X2 500 Hz to 4 kHz), with X2 at least an octave above X1. Ties
  keep the smaller move.
- A move is made only when the predicted Tone EQ fit improves by more than
  **0.1 dB**: a crossover reshapes about an octave
  around it, so the whole-spectrum fit moves far less than for the broad
  moves of the other steps (the four out-of-balance lane sets below improve by
  0.16 to 0.57 dB).
- **Kept, with the reason:** locked; not the Matrix routing; the routing or
  a lane's head engine changed while listening; X1 or X2 edited while
  listening; no playing heard; a lane silent (muted or soloed away); no split
  fits better; the strength too low to move either.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --crossover`):

- **Rules (`crossoverRules`, 44.1, 48 and 96 kHz):**
  - The modelled lanes sum flat and cross at -6 dB.
  - One source split as the engine splits it (the test's Linkwitz-Riley
    filters), lanes at a gain: the flat split stays; HIGH +9 dB, HIGH -9 dB,
    LOW +9 dB and MID -9 dB each move (48 kHz: 119/952, 168/1512, 119/1512,
    189/952 Hz), and the lanes re-rendered at the new split fit as predicted
    within 0.1 dB.
  - Over the 25 candidate splits, the prediction against the re-rendered
    lanes: worst 0.012 dB for linear lanes; 0.045 dB for lanes driven
    through a soft clip (tanh, 4x).
  - The grid, the ranges and the octave spacing hold; phase 17's level
    change is applied first (HIGH +9 dB with -9 dB already decided is the
    flat split, which stays); an inverted lane's heard error; a silent lane
    and silence are kept.
- **Controller (`controllerCrossover`, real processor, Matrix routing, lanes
  fed to the lane taps):**
  - The step is off by default (controller and options panel).
  - It runs alone in the Matrix pass (HIGH +9 dB: X1 150 > 119 Hz, X2 1.20
    kHz > 952 Hz); 50% strength moves half the octaves; with the levels step,
    the levels row comes first and the crossovers judge the levelled lanes.
  - The lock, the Classic routing, X2 edited while listening, the routing
    changed while listening and a silent MID lane are kept. UNDO AUTO
    restores both.
  - **Real heads (measured, not asserted):** played notes through the
    processor's default Matrix heads, the HIGH lane +9 dB, re-rendered at the
    proposed split: 150/1200 > 119/952 Hz, error 11.1 > 9.2 dB predicted and
    10.6 dB re-rendered. The direction held; the prediction overstated the
    gain by 1.4 dB, because each head is driven by the new split.
- **Locks (`controllerEveryLock`):** all twenty-eight locks at four strengths.

## Not yet validated

Checked on synthetic lanes and on renders through the default heads, not
with a real DI or a listening test. For driven heads the prediction is
approximate (above): the step is off by default for that reason. Each run
moves at most a third of an octave; the 0.1 dB minimum gain is a starting
value.
