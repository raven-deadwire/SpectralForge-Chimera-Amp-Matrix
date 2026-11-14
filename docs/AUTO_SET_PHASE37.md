# Auto Set — phase 37 (amp gain of both Dual Crossover rigs)

Phase 33 (`AUTO_SET_PHASE33.md`) sets the GAIN and tone of the two heads of a
Dual **Blend**. In a Dual **Crossover** it kept them, because each rig plays
one band of the input and the amp tap heard the whole input. Phase 37 hears
the band each rig's head receives and sets that head's **GAIN**. Locks and
strength are in `AUTO_SET_LOCKS_STRENGTH.md`. Phase 8's rule is unchanged.

## Use

The options button next to **AUTO SET** has an **Amp gain of both Dual
Crossover rigs** step, under **Amp gain and tone of both Dual Blend rigs**. It
is **off by default**, like the other steps added since phase 24.

- **When it runs:** in a Dual Crossover, in a listening pass per rig (rig 1,
  then rig 2), where phase 33's passes run in a Dual Blend. Each pass takes
  as long as the amp pass.
- **What it hears:** the band that rig's head receives (after the crossover
  and the rig's band tone) and that rig alone after its cabinet.
- **What it sets:** that head's GAIN, with phase 8's drive rule, on that band.
- **The tone knobs are kept.** A rig that plays one band has no tone target of
  its own: the balance of the two bands is the crossover's and the rig
  levels' job.
- **Kept, with the reason:** locked; the Classic routing (the Amp gain and
  tone step sets the head); a Dual Blend (phase 33's step sets the heads);
  Matrix; a rig whose amp is off or on the classic amp engine; the routing,
  Dual type, amp, channel or input changed while listening; no playing heard;
  no GAIN knob on the channel; drive already near the target.
- **The results:** a **Rig 1 amp gain (Crossover)** and a **Rig 2 amp gain
  (Crossover)** row: "GAIN 0.50 > 6.20; drive 2% > 38% (target 45%)". SHOW
  opens that rig's head.
- **Strength:** scales the GAIN move, as for the amp step.
- **Lock → Amp:** keeps both rigs' GAIN (it already owned the Dual heads'
  knobs). There is no new lock.
- **UNDO AUTO:** restores them.
- With phase 33's step also on, a Blend runs phase 33's passes and a Crossover
  this step's; the other step's rows say why they are kept.

## Capture

`setAutoSetRigLane(0 or 1)` (phase 33) feeds one Dual rig alone to the rig
tap. In a Dual Crossover, the amp tap now gets, from the same block, the band
that rig's amp receives (`Engine::laneInput`, `ChimeraDSP.h`), so the two taps stay in
step; a block that did not play the Crossover feeds neither. With no rig asked
for, and in every other routing, the amp tap hears what goes into the amps, as
before. The copy is made only while such a pass listens.

## Rules

Phase 8's (`analyseAmp`, `AutoSetAmp.h`), with only the GAIN knob offered.
Each rig is judged as if it played alone, as in phase 33.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --cross-gain`):

- **Capture (`crossGainCapture`, real processor):** in a Dual Crossover with a
  rig asked for, the amp tap and the rig tap fill in step and the amp tap
  hears a different part of the input per rig; with no rig asked for, in a
  Dual Blend and in the Classic routing it hears the whole input.
- **Controller (`controllerCrossGain`, real processor, the DI and each rig's
  rendered head fed to the taps):** off by default; a pass per rig; rig 1's
  low GAIN raised and rig 2's high GAIN not raised; no parameter but the two
  GAIN knobs written; 50% strength moves less; the Amp lock runs no pass;
  Blend, Matrix and Classic kept without a pass; with phase 33's step also on,
  each routing runs its own passes; an amp that is off; the Dual type edited
  while listening; UNDO AUTO.

## Not yet validated

No real DI or listening test. The controller test feeds the whole DI as each
rig's band, without a crossover or cabinet; the capture test checks the band
on the real engine but not a full run on it. Whether the style's drive target
suits a low band on its own (a bass-range rig) is not measured.
