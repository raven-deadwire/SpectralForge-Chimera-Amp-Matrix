# Auto Set — phase 42 (amp input trim of the Matrix MID and HIGH lanes)

Phase 42 extends phase 39 (`AUTO_SET_PHASE39.md`, the INPUT TRIM of each Dual
Crossover rig's head, with OUTPUT LEVEL holding the loudness) to the heads of
the **Matrix MID and HIGH lanes**, in the lane passes of phase 40
(`AUTO_SET_PHASE40.md`). With it, each drive step of the amp pass (channel,
GAIN, input trim) has its Matrix counterpart: phases 41, 40 and 42. Locks and
strength are in `AUTO_SET_LOCKS_STRENGTH.md`. Phase 24's rule is unchanged.

## Use

The options button next to **AUTO SET** has an **Amp input trim of the Matrix
MID and HIGH lanes** step, under **Amp input trim of both Dual Crossover
rigs**. It is **off by default**, like the other steps added since phase 24.

- **When it runs:** in each Matrix lane's pass (phase 40), after that lane's
  GAIN step. It adds no pass of its own; alone, it still starts the two lane
  passes.
- **What it hears:** as phase 40: the band that lane's head receives, and that
  lane alone after its cabinet.
- **What it sets:** that lane's INPUT TRIM, with phase 24's rule on that band,
  and that lane's OUTPUT LEVEL so the lane stays as loud. The trim is judged
  at the GAIN the gain step proposed at full strength.
- **A lane's OUTPUT LEVEL is its lane level** (on a native head it replaces
  the lane LEVEL; phase 17 balances the lanes with it, in the Matrix pass that
  follows). The **Matrix levels lock** keeps it, so with that lock both lanes'
  trims are kept too, with the reason.
- **The LOW lane is not set**, as in phase 40.
- **Kept, with the reason:** locked; the Classic routing (the Amp input trim
  step sets the head); a Dual Blend or Crossover (their own steps set the
  heads); a lane whose amp is off or on the classic amp engine; the routing,
  amp, channel or input changed while listening; INPUT TRIM or OUTPUT LEVEL
  edited while listening; the Matrix levels lock; and phase 24's own reasons.
- **The results:** a **MID lane amp input trim** and a **HIGH lane amp input
  trim** row, each after that lane's gain row. SHOW opens that lane's head.
- **Strength:** scales the trim, as phase 24; the level holds the loudness of
  the trim applied.
- **Lock → Amp input trim:** now also keeps the two lanes' INPUT TRIM. Their
  OUTPUT LEVEL stays the Matrix levels lock's. There is no new lock.
- **UNDO AUTO:** restores them.

## Rules

Phase 24's (`analyseTrim`, `AutoSetTrim.h`), unchanged. `trimStep` takes the
reason it reports when another lock keeps the level. No audio-thread code is
changed in this phase; the capture is phase 40's.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --matrix-trim`):

- **Controller (`controllerMatrixTrim`, real processor, the DI and each
  lane's rendered head fed to the taps):** off by default; rock with both
  lanes on RHYTHM at the bottom of GAIN: each lane's trim rises and its level
  drops, with no gain rows; the Classic head's and the LOW lane's trim are
  not touched; 50% strength trims less; with the Matrix levels lock both
  lanes' trim and level are kept with the reason; with the gain step the trim
  row follows the gain row per lane and the trim is smaller; the Amp input
  trim lock runs no pass; Classic, Blend and Crossover kept without a pass; an
  amp that is off; a trim edited while listening; UNDO AUTO.

## Not yet validated

No real DI or listening test. As in phase 40, the controller test feeds the
whole DI as each lane's band, without a crossover or cabinet, and muted or
soloed lanes are not handled apart or tested. How the trim's level change and
phase 17's lane balance settle over repeated runs is not measured.
