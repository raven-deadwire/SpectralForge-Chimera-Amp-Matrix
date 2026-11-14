# Auto Set — phase 38 (amp channel of both Dual Crossover rigs)

Phase 38 extends phase 35 (`AUTO_SET_PHASE35.md`, the channel of each Dual
Blend rig's head) to the two heads of a Dual **Crossover**, in the rig passes
of phase 37 (`AUTO_SET_PHASE37.md`). Locks and strength are in
`AUTO_SET_LOCKS_STRENGTH.md`. Phase 20's rule is unchanged.

## Use

The options button next to **AUTO SET** has an **Amp channel selection of
both Dual Crossover rigs** step, under **Amp channel selection of both Dual
Blend rigs**. It is **off by default**, like the other steps added since
phase 24.

- **When it runs:** in each Crossover rig's pass (phase 37), before that rig's
  GAIN step, as phase 35 leads a Dual Blend rig's pass. It adds no pass of its
  own; alone, it still starts the two rig passes.
- **What it hears:** as phase 37: the band that rig's head receives, and that
  rig alone after its cabinet.
- **What it sets:** that head's channel, with phase 20's rule: the channel
  moves only when the current one cannot reach the style's drive on that band
  and another comes closer. The GAIN step then sets GAIN on the new channel.
- **Judged by what the GAIN step will do:** GAIN's reach when phase 37's step
  is on and not locked; the channel at its knobs otherwise.
- **Kept, with the reason:** locked; the Classic routing (the Amp channel
  selection step sets the head); a Dual Blend (phase 35's step sets the
  heads); Matrix; a rig whose amp is off or on the classic amp engine; the
  routing, Dual type, amp, channel or input changed while listening; and
  phase 20's own reasons (one channel, no playing, steady playing).
- **The results:** a **Rig 1 amp channel (Crossover)** and a **Rig 2 amp
  channel (Crossover)** row, each before that rig's gain row. SHOW opens that
  rig's head.
- **Strength:** not applied: a channel is switched or kept.
- **Lock → Amp channel:** keeps both rigs' channels (it already owned the
  Dual heads' channels). There is no new lock.
- **UNDO AUTO:** restores them.

## Rules

Phase 20's (`analyseChannel`, `AutoSetAmp.h`), unchanged. No audio-thread
code is changed in this phase; the capture is phase 37's.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --cross-channel`):

- **Controller (`controllerCrossChannel`, real processor, the DI and each
  rig's rendered head fed to the taps):** off by default; clean with both rigs
  on LEAD: each moves to RHYTHM in its own pass, judged at its knobs, with no
  gain rows; a rig already on RHYTHM stays while the other is switched; with
  the gain step the channel row comes first per rig and the channel is judged
  by GAIN's reach; with the Amp lock the channel step still runs; the Amp
  channel lock runs no pass; Blend, Matrix and Classic kept without a pass; an
  amp that is off; UNDO AUTO.

## Not yet validated

No real DI or listening test. As in phase 37, the controller test feeds the
whole DI as each rig's band, without a crossover or cabinet.
