# Auto Set — phase 36 (amp input trim of both Dual Blend rigs)

Phase 36 extends phase 24 (`AUTO_SET_PHASE24.md`, the Classic head's INPUT
TRIM, with OUTPUT LEVEL holding the loudness) to the two heads of a **Dual
Blend**, in the rig passes of phase 33 (`AUTO_SET_PHASE33.md`). With it, every
step of the Classic amp pass has its Dual Blend counterpart: channel (35),
GAIN and tone (33), input trim (36), presence and resonance (34). Locks and
strength are in `AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed.

## Use

The options button next to **AUTO SET** has an **Amp input trim of both Dual
Blend rigs** step, under **Amp input trim past the GAIN range**. It is **off
by default**, like the other steps added since phase 24.

- **When it runs:** in each rig's pass (phase 33), after that rig's GAIN and
  tone step and before its presence step, as phase 24 sits in the Classic amp
  pass. It adds no pass of its own; alone, it still starts the two rig passes.
- **What it sets:** each rig's INPUT TRIM, with phase 24's rule, from that rig
  heard alone, and that rig's OUTPUT LEVEL so the rig stays as loud.
- **Rig 2's OUTPUT LEVEL is its rig level** (the level phase 19 matches
  against rig 1, in the Dual pass that follows). The **Dual rig levels lock**
  keeps it, so with that lock rig 2's trim is kept too, with the reason: a
  trim that could not hold the loudness is not applied. Rig 1 is still set.
- **As phase 33, each rig is judged as if it played alone.**
- **Kept, with the reason:** locked; the Classic routing (the Amp input trim
  step sets the head); Dual Crossover and Matrix; a rig whose amp is off or
  on the classic amp engine; the routing, Dual type, amp, channel or input
  changed while listening; INPUT TRIM or OUTPUT LEVEL edited while listening;
  the Dual rig levels lock (rig 2); and phase 24's own reasons.
- **The results:** a **Rig 1 amp input trim** and a **Rig 2 amp input trim**
  row, each after that rig's amp row: "INPUT TRIM +0.0 dB > +12.0 dB, OUTPUT
  LEVEL +0.0 dB > -10.0 dB; drive 2% > 21% (target 45%, still short at 12 dB),
  loudness held". SHOW opens that rig's head.
- **Strength:** scales the trim, as phase 24; the level holds the loudness of
  the trim applied.
- **Lock → Amp input trim:** now also keeps the two Dual rigs' INPUT TRIM and
  rig 1's OUTPUT LEVEL. There is no new lock.
- **UNDO AUTO:** restores them.

## Rules

Phase 24's (`analyseTrim`, `AutoSetTrim.h`), unchanged. `trimStep` takes the
rig, its result row, its trim and level at the start of its pass, and whether
another lock keeps its level.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --dual-trim`):

- **Controller (`controllerDualTrim`, real processor, the DI and each rig's
  rendered head fed to the taps):** off by default; rock with both rigs on
  RHYTHM at the bottom of GAIN: each rig's trim rises to +12 dB and its level
  drops by 10 dB (drive 2% > 21%), and no amp rows are written; the Classic
  head's trim is not touched; 50% strength trims less; with the Dual rig
  levels lock rig 2's trim and level are kept with the reason and rig 1 is
  still set; with the rigs' amp step the trim row follows the amp row per rig;
  the Amp input trim lock runs no pass; Dual Crossover, Matrix and Classic
  kept without a pass; an amp that is off; a trim edited while listening;
  UNDO AUTO.
- **Locks (`controllerEveryLock`):** the Amp input trim lock's parameter set
  now names the Dual rigs' three.

## Not yet validated

No real DI or listening test. As in phase 33, the tests feed each rig's
rendered head to the rig tap without a cabinet. How the trim's level change
and phase 19's rig level matching settle over repeated runs is not measured.
