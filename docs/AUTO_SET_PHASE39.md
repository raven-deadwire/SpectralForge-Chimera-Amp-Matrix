# Auto Set — phase 39 (amp input trim of both Dual Crossover rigs)

Phase 39 extends phase 36 (`AUTO_SET_PHASE36.md`, the INPUT TRIM of each Dual
Blend rig's head, with OUTPUT LEVEL holding the loudness) to the two heads of
a Dual **Crossover**, in the rig passes of phase 37 (`AUTO_SET_PHASE37.md`).
With it, each drive step of the amp pass (channel, GAIN, input trim) has its
Dual Crossover counterpart: phases 38, 37 and 39. Locks and strength are in
`AUTO_SET_LOCKS_STRENGTH.md`. Phase 24's rule is unchanged.

## Use

The options button next to **AUTO SET** has an **Amp input trim of both Dual
Crossover rigs** step, under **Amp input trim of both Dual Blend rigs**. It is
**off by default**, like the other steps added since phase 24.

- **When it runs:** in each Crossover rig's pass (phase 37), after that rig's
  GAIN step, as phase 36 follows phase 33. It adds no pass of its own; alone,
  it still starts the two rig passes.
- **What it hears:** as phase 37: the band that rig's head receives, and that
  rig alone after its cabinet.
- **What it sets:** that rig's INPUT TRIM, with phase 24's rule on that band,
  and that rig's OUTPUT LEVEL so the rig stays as loud. The trim is judged at
  the GAIN the gain step proposed at full strength, so the trim only takes the
  drive past GAIN's reach.
- **Rig 2's OUTPUT LEVEL is its rig level.** The **Dual rig levels lock**
  keeps it, so with that lock rig 2's trim is kept too, with the reason. Rig 1
  is still set.
- **Kept, with the reason:** locked; the Classic routing (the Amp input trim
  step sets the head); a Dual Blend (phase 36's step sets the heads); Matrix;
  a rig whose amp is off or on the classic amp engine; the routing, Dual type,
  amp, channel or input changed while listening; INPUT TRIM or OUTPUT LEVEL
  edited while listening; the Dual rig levels lock (rig 2); and phase 24's own
  reasons.
- **The results:** a **Rig 1 amp input trim (Crossover)** and a **Rig 2 amp
  input trim (Crossover)** row, each after that rig's gain row. SHOW opens
  that rig's head.
- **Strength:** scales the trim, as phase 24; the level holds the loudness of
  the trim applied.
- **Lock → Amp input trim:** keeps the two rigs' INPUT TRIM and rig 1's OUTPUT
  LEVEL (it already owned them, since phase 36). There is no new lock.
- **UNDO AUTO:** restores them.

## Rules

Phase 24's (`analyseTrim`, `AutoSetTrim.h`), unchanged. No audio-thread code
is changed in this phase; the capture is phase 37's.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --cross-trim`):

- **Controller (`controllerCrossTrim`, real processor, the DI and each rig's
  rendered head fed to the taps):** off by default; rock with both rigs on
  RHYTHM at the bottom of GAIN: each rig's trim rises to +12 dB and its level
  drops by 10 dB (drive 2% > 21%), with no gain rows; the Classic head's trim
  is not touched; 50% strength trims less; with the Dual rig levels lock rig
  2's trim and level are kept with the reason and rig 1 is still set; with the
  gain step the trim row follows the gain row per rig and the trim is smaller
  (0 dB here: GAIN reaches the target); the Amp input trim lock runs no pass;
  Blend, Matrix and Classic kept without a pass; an amp that is off; a trim
  edited while listening; UNDO AUTO.

## Not yet validated

No real DI or listening test. As in phase 37, the controller test feeds the
whole DI as each rig's band, without a crossover or cabinet. How the trim's
level change and the rig level matching settle over repeated runs is not
measured.
