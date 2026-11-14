# Auto Set — phase 40 (amp gain of the Matrix MID and HIGH lanes)

Phase 37 (`AUTO_SET_PHASE37.md`) sets the GAIN of each Dual Crossover rig's
head from the band that head receives. Phase 40 does the same for the
**MID** and **HIGH** lanes of the **Matrix** routing, which the amp steps so
far kept ("the lanes play separate bands; set those amps in the full
editor"). Locks and strength are in `AUTO_SET_LOCKS_STRENGTH.md`. Phase 8's
rule is unchanged.

## Use

The options button next to **AUTO SET** has an **Amp gain of the Matrix MID
and HIGH lanes** step, under **Amp gain of both Dual Crossover rigs**. It is
**off by default**, like the other steps added since phase 24.

- **When it runs:** in the Matrix routing, in a listening pass per lane (MID,
  then HIGH), where the Dual rig passes run in a Dual routing. Each pass takes
  as long as the amp pass.
- **What it hears:** the band that lane's head receives (after the crossover
  and the lane's band tone) and that lane alone after its cabinet.
- **What it sets:** that head's GAIN, with phase 8's drive rule, on that band.
- **The tone knobs are kept**, as in phase 37: a lane that plays one band has
  no tone target of its own.
- **The LOW lane is not set.** It blends its amp with the DI (LOW AMP MIX)
  after a compressor, so what is heard after it is not that head alone.
- **Kept, with the reason:** locked; the Classic routing (the Amp gain and
  tone step sets the head); a Dual Blend or Crossover (their own steps set the
  heads); a lane whose amp is off or on the classic amp engine; the routing,
  amp, channel or input changed while listening; no playing heard; no GAIN
  knob on the channel; drive already near the target.
- **The results:** a **MID lane amp gain** and a **HIGH lane amp gain** row.
  SHOW opens that lane's head.
- **Strength:** scales the GAIN move, as for the amp step.
- **Lock → Amp:** now also keeps GAIN of the Matrix MID and HIGH lanes'
  heads. There is no new lock.
- **UNDO AUTO:** restores them.

## Capture

`setAutoSetRigLane` now takes 0 to 2. In the Matrix routing it feeds that lane
alone (after its cabinet, level and polarity) to the rig tap, and the band
that lane's amp receives (`Engine::laneInput`, `ChimeraDSP.h`) to the amp tap,
from the same block, as phase 37 does in a Dual Crossover. A lane the routing
does not have (lane 2 in a Dual routing) feeds nothing. With no lane asked
for, both taps hear what they did before. The copies are made only while such
a pass listens.

A muted lane, or one not soloed while another is, is silent on the rig tap:
its pass then reports "no playing heard" or sets GAIN from the band alone.
This is not handled apart.

## Rules

Phase 8's (`analyseAmp`, `AutoSetAmp.h`), with only the GAIN knob offered.
Each lane is judged as if it played alone.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --matrix-gain`):

- **Capture (`matrixGainCapture`, real processor):** in the Matrix with a
  lane asked for, the amp tap and the rig tap fill in step, the amp tap hears
  a different part of the input per lane and the rig tap a different lane;
  lane 2 asked for in a Dual routing feeds nothing.
- **Controller (`controllerMatrixGain`, real processor, the DI and each
  lane's rendered head fed to the taps):** off by default; a pass per lane;
  the MID lane's low GAIN raised and the HIGH lane's high GAIN not raised; no
  parameter but the two GAIN knobs written; 50% strength moves less; the Amp
  lock runs no pass; Classic, Blend and Crossover kept without a pass; an amp
  that is off; the routing edited while listening; UNDO AUTO.
- **`dualAmpCapture` (phase 33)** now expects the Matrix to feed its lane to
  the rig tap (it expected nothing).

## Not yet validated

No real DI or listening test. The controller test feeds the whole DI as each
lane's band, without a crossover or cabinet; the capture test checks the band
on the real engine but not a full run on it. Muted and soloed lanes are not
tested. Whether the style's drive target suits a single band is not measured.
