# Auto Set — phase 30 (Dual and Matrix cabinet mics)

Phase 30 extends phase 9 (`AUTO_SET_PHASE9.md`, the Classic cabinet's modeled
mic: position and distance) to the other routings: the modeled mic of each
**Dual rig** and each **Matrix lane**. Earlier phases are described in
`AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE29.md`; locks and strength in
`AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has a **Dual and Matrix cabinet mic
placement** step, listed before the lane IR and cut steps. It is **off by
default**, like the other steps added since phase 24, until it has had a DI
and listening check.

- **When it runs:** in the Dual pass (after lane polarity, rig levels and the
  blend) or the Matrix pass (after lane polarity, lane levels and the
  crossovers), before the steps of phases 27 to 29. It adds no listening time;
  it alone also starts the pass. In the Classic routing it is kept: the
  Cabinet mic placement step (phase 9) places rig 1's mic.
- **What it sets:** the heard modeled mic's position and distance on each rig
  or lane (mic A or B, whichever the A/B blend is fully on), and that mic's
  level, so the lane stays as loud as it was.
- **What it leaves alone:** captured IRs and filters (phase 29 chooses a
  factory IR), blended mics, the cuts, the lanes' levels, polarities, blend and
  crossovers.
- **With the IR and cut steps (phases 27 to 29):** those are judged from the
  pass's captures. After a mic move the captures are stale, so their rows say
  "kept (a cabinet mic was moved in this run: run Auto Set again to judge the
  IRs)" or "… the cuts)". Without a move they are judged as before.
- **The results:** a **Lane cabinet mics** row before the IR and cut rows:
  "rig 1 mic A position 0.50 > 0.15, distance 10.0 > 5.0 cm, level -2.0 >
  -4.2 dB; balance off by 5.7 > 3.9 dB (predicted from the mix heard)", or why
  it kept them. Its SHOW button opens rig 1's cabinet.
- **Strength:** scales the move of position and distance; the level then holds
  the loudness of the placement applied. A Strength placement that would add a
  peak or notch is not applied, and the row says so. Below 100% the row's
  balance is the full-strength prediction and says "at full strength".
- **Lock → Cabinet mic:** now keeps the modeled mics of all three rigs
  (position, distance and level of mic A and B; it kept rig 1's). There is no
  new lock.
- **UNDO AUTO:** restores placement and level.

The Cabinet mic step (phase 9) in a Dual or Matrix routing now says that this
step places those mics.

## Rules (`analyseLaneMics`, `AutoSetLaneMic.h`)

- **Heard:** the pass's lane captures, at most 6 s, weighted as the engine
  mixes them (as phase 27).
- **Model:** the modeled cabinet's IR is generated from its authored response
  (`cabLayout::response`, the propagation delays included), so a candidate
  placement changes its lane by the complex ratio of the candidate's response
  to the current one. The ratio is smooth, so it is taken per 1/96 octave:
  the lanes' spectra are reduced once to their cross-powers in each such step
  (split at the tone bands' edges), and a candidate costs one small sum per
  step. The lanes add coherently. A mic moved away arrives later, and that
  counts in the sum.
- **Level:** a candidate is judged with the gain that brings its lane's power
  back to its power now. The gain applied is computed for the placement
  actually applied, from the IRs the engine plays (`cabLevelChangeDb`), as in
  phase 9.
- **Candidates and limits, as phase 9:** distances from half to twice the
  current one (nine steps, 0.5 cm grid, 2–60 cm), positions within 0.35 of the
  current one in steps of 0.05; a placement rougher than the current one by
  more than 1 dB, or one that adds a local peak or notch of more than 3 dB, is
  ruled out; a move only when the mix's balance (the amp step's tone error)
  improves by more than 0.5 dB.
- **Order:** lanes are judged one after another (rig 1 / LOW first), each with
  the placements already chosen. A rig alone in the mix is placed as phase 9
  places it.
- **A lane is judged only** when its cabinet is on, one mic is heard and that
  mic is a modeled mic.
- **Kept, with the reason:** locked; the Classic routing; the routing, Dual
  type or a lane's head changed while listening; a cabinet, mic or mic level
  changed while listening; the Matrix crossovers moved in this run; no playing
  heard; and per lane: cabinet off, two mics blended, a captured IR or filters,
  silent, already near the target, a Strength placement that adds a peak or
  notch.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --lane-mic`):

- **Rules (`laneMicRules`, 44.1, 48 and 96 kHz; lanes rendered through the
  generated model IRs):** a dark and a bright rig move their mic and the mix
  comes closer; a rig alone in the mix gets phase 9's placement; the moves stay
  within phase 9's candidates and limits; the same source in both rigs
  (correlated lanes) and an inverted rig are predicted for fixed moves; with
  both mics free, rig 2 is judged from rig 1's choice; a Matrix lane; a silent
  lane and silence are kept. **Prediction against the rendered mix:** worst
  0.024 dB; the level-holding gain misses the rendered lane by at most
  0.002 dB.
- **Controller (`controllerLaneMic`, real processor, lanes fed to the lane
  taps):** off by default; Dual mics are moved and their level held; 50%
  strength; the Cabinet mic lock; cabinet off, blended mics, a mic that is not
  modeled, a silent rig, a placement or a level edited while listening, the
  Classic routing; the IR and cut rows wait after a move, in row order; the
  Matrix HIGH lane; UNDO AUTO.
  - **Real heads (measured):** notes through the processor's default Dual
    heads, both rigs through modeled mics at 0.5 / 10 cm: rig 1's mic moved to
    0.2 / 16 cm is predicted at 7.610 dB and re-rendered by the engine at
    7.603 dB; the level gain is 1.798 dB modelled and 1.803 dB applied, and
    the engine's rig 1 level changes by 0.004 dB (asserted within 0.5 dB).
- **Locks (`controllerEveryLock`):** the Cabinet mic lock now owns the
  eighteen placement and level parameters.

## Not yet validated

No real DI or listening test. The real-head check is Dual, 48 kHz, mic A; the
Matrix routing, mic B and other session rates are checked on rendered lanes
or by the controller with fed lanes only.
