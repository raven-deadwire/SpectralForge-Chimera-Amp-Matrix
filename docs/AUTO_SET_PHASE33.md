# Auto Set — phase 33 (amp gain and tone of both Dual Blend rigs)

Phase 33 extends phase 8 (`AUTO_SET_PHASE8.md`, the Classic head's GAIN, BASS,
MIDDLE and TREBLE) to the two heads of a **Dual Blend**. Earlier phases are
described in `AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE32.md`; locks and strength
in `AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has an **Amp gain and tone of both
Dual Blend rigs** step, under **Amp gain and tone**. It is **off by default**,
like the other steps added since phase 24, until it has had a DI and listening
check.

- **When it runs:** a pass of its own per rig, after the Classic amp pass's
  place and before the cabinet passes. **Each pass adds listening time** (as
  phase 8's pass does): two passes in a Dual Blend.
- **What it sets:** each rig's head GAIN and tone knobs, with phase 8's rule,
  from that rig heard alone.
- **Each rig is set toward the style's drive and tone as if it played alone.**
  The step does not know that a blend is meant as, say, a clean rig under a
  driven one: it would pull both toward the same target. Leave it off for such
  a blend, or lock the amp.
- **What it leaves alone:** the heads' channels, input trim, PRESENCE and
  RESONANCE (phases 20, 24 and 12 stay Classic-only), the cabinets, the blend.
- **Kept, with the reason:** locked (no pass); the Classic routing (the Amp
  gain and tone step sets the head); Dual Crossover and the Matrix routing
  (the rigs or lanes play separate bands); a rig whose amp is off or on the
  classic amp engine (the other rig is still set); the routing, Dual type,
  amp, channel or input changed while listening; and phase 8's own reasons.
- **The results:** a **Rig 1 amp** and a **Rig 2 amp** row after the amp rows:
  "GAIN 0.05 > 0.11, BASS 0.50 > 0.20, MIDDLE 0.50 > 0.65, TREBLE 0.50 > 0.80;
  drive 0% > 46% (target 45%)". SHOW opens that rig's head.
- **Strength:** scales each knob's move, as phase 8.
- **Lock → Amp:** now also keeps the two Dual rigs' GAIN and tone knobs. There
  is no new lock.
- **UNDO AUTO:** restores them.

## Rules

Phase 8's (`analyseAmp`, `AutoSetAmp.h`), unchanged, called once per rig with
that rig's head, the signal entering the amps and that rig's own output.

## Engine

The rig tap (`Tap::cab`) heard the whole rig after the post-rig gates. The
processor now has `setAutoSetRigLane`: -1 (the default) keeps that behaviour;
0 or 1 feeds the tap from that Dual rig alone (its lane output: after its
cabinet, level and polarity), in the Dual routing only. `processBlock` reads
one atomic; the lane capture it reuses is phase 13's. The controller sets the
rig at the start of each pass and returns it to -1 when it stops listening.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --dual-amp`):

- **Capture (`dualAmpCapture`, real processor):** the rig tap is fed with the
  whole rig by default and with a Dual rig alone on request; a rig request
  feeds nothing in the Classic or Matrix routing.
- **Controller (`controllerDualAmp`, real processor, the DI and each rig's
  rendered head fed to the taps):** off by default; a pass per rig; a rig at
  GAIN 0.05 is raised and a rig at 0.95 is not raised (drive 0% > 46% and 88% >
  38%, target 45%); 50% strength moves less; the Amp lock; Dual Crossover,
  Matrix and Classic kept without a pass; an amp that is off, a rig on the
  classic engine (the other still set), the Dual type edited while listening;
  UNDO AUTO.

## Not yet validated

No real DI or listening test. The controller tests feed each rig's rendered
head to the rig tap, without a cabinet; that the engine's lane output is what
phase 8's rule expects of a rig heard alone is not measured through the real
engine. The audio-thread change has no realtime measurement of its own beyond
the existing realtime tests.
