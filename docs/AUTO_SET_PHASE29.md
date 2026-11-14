# Auto Set — phase 29 (Dual and Matrix cabinet IRs)

Phase 29 extends phase 23 (`AUTO_SET_PHASE23.md`, the Classic cabinet's
factory IR: **V30 / SM57** or **Jensen / SM57**) to the other routings: the
factory IR of each **Dual rig's** and each **Matrix lane's** heard mic.
Earlier phases are described in `AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE28.md`;
locks and strength in `AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in
the earlier phases.

## Use

The options button next to **AUTO SET** has a **Dual and Matrix cabinet
factory IR choice** step, listed before the lane cabinet cut steps. It is
**off by default**, like the other steps added since phase 24, until it has
had a DI and listening check.

- **When it runs:** in the Dual pass (after lane polarity, rig levels and the
  blend) or the Matrix pass (after lane polarity, lane levels and the
  crossovers), before the cut steps of phases 27 and 28. It adds no listening
  time; it alone also starts the pass. In the Classic routing it is kept: the
  Cabinet factory IR choice step (phase 23) sets rig 1.
- **What it sets:** the cabinet source of each rig's or lane's heard mic, mic
  A (`cabtype1`…`cabtype3`) or mic B (`cabBtype1`…`cabBtype3`), between the two
  factory IRs.
- **What it leaves alone:** the levels (the change is reported; the Final EQ +
  output step sets the level), the other mic, modeled mics, user IRs, the cuts.
- **With the cut steps (phases 27 and 28):** the cuts are judged from the
  pass's captures. After an IR switch those are stale, so the cut rows say
  "kept (a cabinet IR was switched in this run: run Auto Set again to judge
  the cuts)". Without a switch the cuts are judged as before.
- **The results:** a **Lane cabinet IRs** row before the cut rows: "rig 1 mic
  A IR V30 / SM57 > Jensen / SM57; balance off by 1.9 > 0.5 dB; level +1.5 dB
  (predicted from the mix heard; the Final EQ + output step sets the level);
  rig 2 already closest with Jensen / SM57", or why it kept them. Its SHOW
  button opens rig 1's cabinet.
- **Strength:** not applied. An IR is switched or kept.
- **Lock → Cabinet IR:** now keeps the sources of all three rigs (it kept rig
  1's two). There is no new lock.
- **UNDO AUTO:** restores the sources.

The Cabinet IR step (phase 23) in a Dual or Matrix routing now says that this
step chooses those rigs' IRs.

## Rules (`analyseLaneIR`, `AutoSetLaneIR.h`)

- **Heard:** the pass's lane captures, at most 6 s, weighted as the engine
  mixes them (as phase 27).
- **Model:** a lane's IR is a fixed filter in series in that lane, so the
  other IR changes the lane exactly by the two IRs' complex response ratio.
  Each lane and each IR is transformed once on the same bins; a candidate
  re-weights its lane's bins by the ratio, the lanes are added coherently, and
  each band of the mix as heard moves by the modelled change. The IRs' delays
  count: a switch also moves the lane against the others.
  - Each IR is taken as the cabinet loads it (`laneIRLoaded`): its first
    channel, resampled to the session rate, scaled by 0.125 / the square root
    of its energy, not trimmed.
  - A ratio is limited to 30 dB per bin (an IR's nulls) and 24 dB per band.
- **Choice, as phase 23:** the other IR when the mix's balance (the amp step's
  tone error, from 80 Hz) improves by more than 0.5 dB. Lanes are judged one
  after another (rig 1 / LOW first), each with the IRs already chosen.
- **Low end:** a switch that would cut the heard mix's 44.5–70.7 Hz power by
  more than 3 dB and leave its 50–63 Hz bands more than 3 dB under the style's
  target, where the playing has them, is not made. The change is taken at the
  heard bins themselves (the one transform resolves the notes).
- **Bass** is kept: both factory IRs are guitar cabinets.
- **A lane is judged only** when its cabinet is on, one mic is heard (the A/B
  blend fully on one mic), that mic's modeled mic is off and its source is a
  factory IR.
- **Kept, with the reason:** locked; the Classic routing; the routing, Dual
  type or a lane's head changed while listening; a cabinet, mic or source
  changed while listening; the Matrix crossovers moved in this run; no playing
  heard; bass; and per lane: cabinet off, two mics blended, a modeled mic,
  filters only, a user IR, silent, the low end, already closest.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --lane-ir`):

- **Rules (`laneIRRules`, 44.1, 48 and 96 kHz; lanes rendered with
  `juce::dsp::Convolution` as the cabinet loads the IRs):** a rig whose playing
  suits the other IR is switched and a suited one kept; with both rigs on the
  wrong IR, rig 2 is judged from rig 1's choice; the same signal in both rigs
  (correlated lanes) and an inverted rig are predicted; the Matrix HIGH lane is
  switched alone; a silent lane, bass, silence and non-factory sources are
  kept; a switch that would cut the low strings' fundamentals is held.
  **Prediction against the rendered mix:** worst 0.082 dB; level change worst
  0.14 dB.
- **Controller (`controllerLaneIR`, real processor, lanes fed to the lane
  taps):** off by default; Dual rig 1 and the Matrix LOW lane are switched;
  mic B's source when the blend is on B; strength is not applied; the Cabinet
  IR lock; every per-lane reason above; an edit while listening; bass; the
  Classic routing; the cut rows wait after a switch and are judged without
  one, in row order; UNDO AUTO.
  - **Real heads (measured):** notes through the processor's default Dual
    heads, both rigs on the V30: rig 1's switch to the Jensen is predicted at
    9.203 dB and re-rendered by the engine at 9.207 dB (asserted within
    0.5 dB).
- **Locks (`controllerEveryLock`):** the Cabinet IR lock now owns the six
  source parameters.

## Not yet validated

No real DI or listening test. The real-head check is Dual, 48 kHz, mic A; the
Matrix routing and other session rates are checked on rendered lanes only.
