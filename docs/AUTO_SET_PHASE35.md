# Auto Set — phase 35 (amp channel of both Dual Blend rigs)

Phase 35 extends phase 20 (`AUTO_SET_PHASE20.md`, the Classic head's channel)
to the two heads of a **Dual Blend**, in the rig passes of phase 33
(`AUTO_SET_PHASE33.md`). Locks and strength are in
`AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has an **Amp channel selection of both
Dual Blend rigs** step, under **Amp channel selection**. It is **off by
default**, like the other steps added since phase 24.

- **When it runs:** first in each rig's pass (phase 33), as phase 20 leads the
  Classic amp pass. The rig's GAIN and tone step (phase 33) and its presence
  step (phase 34) then work on the channel it chose, against the head as it
  played. It adds no pass of its own; alone, it still starts the two rig
  passes.
- **What it sets:** each rig's head channel, with phase 20's rule, from that
  rig heard alone. A head with one channel is kept.
- **How a channel is judged:** as phase 20. With that rig's amp step running,
  by the drive its GAIN can reach on each channel; without it, each channel at
  its own knobs.
- **As phase 33, each rig is judged as if it played alone.**
- **Kept, with the reason:** locked; the Classic routing (the Amp channel
  selection step sets the head); Dual Crossover and Matrix; a rig whose amp is
  off or on the classic amp engine (the other rig is still judged); the
  routing, Dual type, amp, channel or input changed while listening; and phase
  20's own reasons.
- **The results:** a **Rig 1 amp channel** and a **Rig 2 amp channel** row,
  each before that rig's amp row: "CHANNEL LEAD > RHYTHM; LEAD at its knobs
  92%, RHYTHM at its knobs 39%, drive target 15% (still short: the closest
  channel); judged at the knobs (the amp step is off or locked)". SHOW opens
  that rig's head.
- **Strength:** a switch is not scaled, as phase 20.
- **Lock → Amp channel:** now also keeps the two Dual rigs' channels. There
  is no new lock.
- **UNDO AUTO:** restores them.

## Rules

Phase 20's (`analyseChannel`, `AutoSetChannel.h`), unchanged. `channelStep`
takes the rig, its result row and whether that rig's own GAIN step follows.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --dual-channel`):

- **Controller (`controllerDualChannel`, real processor, the DI and each rig's
  rendered head fed to the taps):** off by default; clean style with both rigs
  on LEAD: each moves to RHYTHM in its own pass and no amp rows are written; a
  rig already on RHYTHM stays while the other is switched; with the rigs' amp
  step the channel row comes before the amp row per rig; the Amp channel lock
  runs no pass; Dual Crossover, Matrix and Classic kept without a pass; an amp
  that is off keeps that rig and the other is still switched; UNDO AUTO.

## Not yet validated

No real DI or listening test. The controller test uses one head (the one the
Classic channel test uses) on both rigs.
