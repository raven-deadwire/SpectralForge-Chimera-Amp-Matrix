# Auto Set — phase 27 (Dual and Matrix lane cabinet cuts)

Phase 27 extends phase 15 (`AUTO_SET_PHASE15.md`, the Classic rig's cabinet
LOW and HIGH cuts) to the other rigs: the cuts of **Dual rig 2** and of the
**Matrix MID and HIGH lanes** (`cablow2`/`cabhigh2`, `cablow3`/`cabhigh3`).
Earlier phases are described in `AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE26.md`;
locks and strength in `AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in
the earlier phases.

## Use

The options button next to **AUTO SET** has a **Dual and Matrix lane cabinet
cuts** step, listed after the cabinet steps. It is **off by default**, like
the other steps added since phase 24, until it has had a DI and listening
check; the prediction itself is exact (below).

- **When it runs:** in the Dual pass (after lane polarity, rig levels and the
  blend) or the Matrix pass (after lane polarity, lane levels and the
  crossovers), so it judges the rigs as those steps left them mixed. It adds
  no listening time; it alone also starts the pass. In the Classic routing it
  is kept: the Cabinet cuts step (phase 15) sets rig 1.
- **What it sets:** the LOW and HIGH cuts of Dual rig 2, or of the Matrix MID
  and HIGH lanes.
- **What it leaves alone:** rig 1 / the LOW lane (its cuts are phase 15's,
  in Classic), the lanes' levels, polarities, blend and crossovers.
- **The results:** a **Lane cabinet cuts** row after the routing rows: "rig 2
  LOW 70 Hz > 35 Hz, rig 2 HIGH 14.0 kHz > 7.0 kHz; rig 2 low end off by 2.9 >
  1.2 dB, rig 2 top off by 7.0 > 3.6 dB (predicted from the mix heard)", or why
  it kept them. Its SHOW button opens rig 2's cabinet.
- **Strength:** scales each cut's change in Hz.
- **Lock → Lane cabinet cuts:** keeps the four cuts.
- **UNDO AUTO:** restores them.

The Auto Set lock menu ids now start at 101 (the thirtieth lock would have
taken id 70, the cabinet mic blend step).

## Rules (`analyseLaneCuts`, `AutoSetLaneCuts.h`)

- **Heard:** the pass's lane captures (after each lane's cabinet, cuts, level
  and polarity), at most 6 s, weighted as the engine mixes them: the Dual
  blend (or 1 and 1 for the Dual crossover), and the level and polarity changes
  the earlier steps of the pass have just made.
- **Model:** a cut is a second-order Butterworth filter after the cabinet
  (`Cab::setCuts`), so a moved cut changes its lane exactly by the new
  filter's response over the current one. Each lane is transformed once (one
  FFT over the whole capture); a candidate re-weights a lane's bins by that
  ratio, the lanes are added coherently, and each band of the mix as heard
  moves by the modelled change.
- **Fit, candidates and limits as phase 15:** the low end (guitar 50-160 Hz,
  bass 50-100 Hz) and the top (5-10 kHz) against the aligned Tone EQ target;
  half to twice the cut in nine steps (LOW on a 5 Hz grid, at most the Drop A
  ceiling of 55 Hz for guitar and 25 Hz for bass; HIGH on a 100 Hz grid, from
  4 kHz for guitar and 2.5 kHz for bass); a move only when its region improves
  by more than 0.5 dB. Lanes are judged one after another (MID before HIGH),
  each with the cuts already chosen for the earlier one.
- **Kept, with the reason:** locked; the Classic routing; the routing, Dual
  type or a lane's head changed while listening; a lane cut edited while
  listening; a lane's cabinet off (it plays no cuts); a lane silent; no
  playing heard; the Matrix crossovers moved in this run (the lanes then play
  a different band: run again); already near the target.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --lane-cuts`):

- **Rules (`laneCutsRules`, 44.1, 48 and 96 kHz):** Dual rig 2 with a fizzy
  top lowers its HIGH cut, with a boomy low end raises its LOW cut (within the
  Drop A ceiling); re-run at its own result, rig 2 settles; an inverted capture
  with a -1 weight chooses the same; the Matrix HIGH lane's fizz lowers its
  HIGH cut; a silent lane and silence are kept. **Prediction against the lanes
  re-rendered at the chosen cuts:** worst 0.054 dB.
- **Controller (`controllerLaneCuts`, real processor, lanes fed to the lane
  taps):** off by default (controller and options panel); Dual rig 2's cuts
  (HIGH 14 > 7 kHz, LOW 70 > 35 Hz) and the Matrix HIGH lane's HIGH cut move;
  50% strength moves half the Hz; the lock, rig 2's cabinet off, a cut edited
  while listening, a silent rig 2 and the Classic routing are kept; with the
  Matrix levels step, the levels row comes first; UNDO AUTO restores the cuts.
  - **Real heads (measured):** notes through the processor's default Dual
    heads (rig 2 driven at 1.0, rig 1 HIGH at 5 kHz, rig 2 at 70 Hz / 16 kHz):
    the step keeps the cuts (the heads' cabinet IRs already roll the top off);
    a fixed move to 140 Hz / 6 kHz is predicted at 12.41 / 13.25 dB and
    re-renders at 12.31 / 13.22 dB (low end / top), within 0.5 dB as asserted.
- **Locks (`controllerEveryLock`):** all thirty locks at four strengths.

## Not yet validated

Checked on synthetic lanes and on renders through the default heads, not with
a real DI or a listening test. Rig 1 / the LOW lane in Dual and Matrix is
phase 28's step (`AUTO_SET_PHASE28.md`). With factory cabinet IRs, which roll
the top off near 5 kHz, a HIGH cut above that often changes little.
