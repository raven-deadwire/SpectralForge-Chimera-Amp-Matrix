# Auto Set — phase 28 (Dual rig 1 and Matrix LOW lane cabinet cuts)

Phase 28 closes the gap phase 27 left (`AUTO_SET_PHASE27.md`): the cabinet LOW
and HIGH cuts of **Dual rig 1** and of the **Matrix LOW lane**
(`cablow1`/`cabhigh1`). Phase 15 sets these two cuts in the Classic routing
only; phase 27 sets the other lanes'. Earlier phases are described in
`AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE27.md`; locks and strength in
`AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has a **Dual rig 1 and Matrix LOW lane
cabinet cuts** step, listed before the lane cabinet cuts step. It is **off by
default**, like the other steps added since phase 24, until it has had a DI and
listening check.

- **When it runs:** in the Dual or the Matrix pass, with phase 27's step and
  under the same conditions (after lane polarity, the levels, the blend or the
  crossovers). It adds no listening time; it alone also starts the pass. In
  the Classic routing it is kept: the Cabinet cuts step (phase 15) sets rig 1.
- **What it sets:** the LOW and HIGH cuts of Dual rig 1, or of the Matrix LOW
  lane.
- **With the lane cabinet cuts step:** one analysis judges rig 1 / the LOW
  lane first, then the other lanes with the cuts already chosen for it. Each
  step has its own row.
- **The results:** a **Rig 1 / LOW lane cabinet cuts** row before the lane
  cabinet cuts row: "rig 1 LOW 70 Hz > 35 Hz, rig 1 HIGH 14.0 kHz > 7.0 kHz;
  rig 1 low end off by 3.4 > 1.5 dB, rig 1 top off by 6.7 > 3.4 dB (predicted
  from the mix heard)", or why it kept them. Its SHOW button opens rig 1's
  cabinet.
- **Strength:** scales each cut's change in Hz.
- **Lock → Cabinet cuts:** keeps the two cuts, in every routing (the lock
  already owns `cablow1`/`cabhigh1`; there is no new lock). Lock → Lane
  cabinet cuts still keeps only the other lanes'.
- **UNDO AUTO:** restores them.

The Cabinet cuts step (phase 15) in a Dual or Matrix routing now says that
these two steps set those rigs.

## Rules

Phase 27's (`analyseLaneCuts`, `AutoSetLaneCuts.h`), unchanged: the lane
captures weighted as the engine mixes them, each cut modelled exactly as the
new filter's response over the current one, the fit, candidates and limits of
phase 15. Rig 1 / the LOW lane is passed as the first adjustable lane.

**Kept, with the reason:** locked (Cabinet cuts); the Classic routing; the
routing, Dual type or a lane's head changed while listening; the cuts edited
while listening; rig 1's cabinet off; rig 1 silent; no playing heard; the
Matrix crossovers moved in this run; already near the target.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --rig1-cuts`):

- **Rules (`rig1CutsRules`, 44.1, 48 and 96 kHz):** Dual rig 1 with a fizzy
  top lowers its HIGH cut; with both rigs fizzy and both adjustable, rig 2 is
  judged from rig 1's chosen cut; the Matrix LOW lane with a boomy low end
  raises its LOW cut within the Drop A ceiling; a silent rig 1 is kept.
  **Prediction against the lanes re-rendered at the chosen cuts:** worst
  0.048 dB.
- **Controller (`controllerRig1Cuts`, real processor, lanes fed to the lane
  taps):** off by default (controller and options panel); Dual rig 1's cuts
  move and rig 2's stay; 50% strength moves half the Hz; the Cabinet cuts
  lock, rig 1's cabinet off, a cut edited while listening, a silent rig 1 and
  the Classic routing are kept; with the lane cuts step, rig 1's row comes
  first and both analyses are applied; each of the two locks keeps its own
  rig alone; the Matrix LOW lane's LOW cut rises (30 > 55 Hz); UNDO AUTO
  restores the cuts.

## Not yet validated

Checked on synthetic lanes only, not with a real DI or a listening test; the
real-head prediction check is phase 27's (rig 2). In the Matrix routing the
LOW lane carries little above the first crossover, so its HIGH cut rarely
changes the top.
