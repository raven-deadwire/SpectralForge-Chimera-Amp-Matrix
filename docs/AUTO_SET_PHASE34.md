# Auto Set — phase 34 (presence and resonance of both Dual Blend rigs)

Phase 34 extends phase 12 (`AUTO_SET_PHASE12.md`, the Classic head's PRESENCE
and RESONANCE or DEPTH) to the two heads of a **Dual Blend**, in the rig
passes of phase 33 (`AUTO_SET_PHASE33.md`). Locks and strength are in
`AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has a **Presence and resonance of both
Dual Blend rigs** step, under the rigs' amp step. It is **off by default**,
like the other steps added since phase 24.

- **When it runs:** in each rig's pass (phase 33), after that rig's GAIN and
  tone step, on the head as that step left it, as phase 12 follows phase 8.
  It adds no pass of its own; alone, it still starts the two rig passes.
- **What it sets:** each rig's PRESENCE and RESONANCE (or DEPTH) with phase
  12's rule, from that rig heard alone. A knob the channel does not have is
  left alone.
- **As phase 33, each rig is judged as if it played alone.**
- **Kept, with the reason:** locked; the Classic routing (the Presence and
  resonance step sets the head); Dual Crossover and Matrix; a rig whose amp is
  off or on the classic amp engine (the other rig is still set); the routing,
  Dual type, amp, channel or input changed while listening; and phase 12's own
  reasons.
- **The results:** a **Rig 1 presence + resonance** and a **Rig 2 presence +
  resonance** row, each after that rig's amp row: "PRESENCE 1.00 > 0.70,
  RESONANCE 0.50 > 0.20; bite +4.1 > +2.1 dB, low end +11.4 > +8.0 dB from the
  target". SHOW opens that rig's head.
- **Strength:** scales each knob's move, as phase 12.
- **Lock → Presence + resonance:** now also keeps the two Dual rigs' knobs.
  There is no new lock. The Amp lock and this lock each keep their own step.
- **UNDO AUTO:** restores them.

## Rules

Phase 12's (`analysePower`, `AutoSetPower.h`), unchanged. `powerStep` takes the
rig's head and result row; nothing else in it changed.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --dual-power`):

- **Controller (`controllerDualPower`, real processor, the DI and each rig's
  rendered head fed to the taps):** off by default; alone, the step runs in
  both rig passes and writes no amp rows; a bright head's PRESENCE at maximum
  comes down on both rigs (1.00 > 0.70; bite +4.1 > +2.1 dB); 50% strength
  moves less; with the rigs' amp step the rows are amp then presence per rig;
  each of the two locks keeps its own step; the Presence + resonance lock
  alone runs no pass; Dual Crossover, Matrix and Classic kept without a pass;
  an amp that is off keeps that rig and the other is still set; UNDO AUTO.

## Not yet validated

No real DI or listening test. As in phase 33, the tests feed each rig's
rendered head to the rig tap without a cabinet, so the heads sound bright
there; the moves measured are for that fixture.
