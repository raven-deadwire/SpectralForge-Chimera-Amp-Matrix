# Auto Set — phase 41 (amp channel of the Matrix MID and HIGH lanes)

Phase 41 extends phase 38 (`AUTO_SET_PHASE38.md`, the channel of each Dual
Crossover rig's head) to the heads of the **Matrix MID and HIGH lanes**, in
the lane passes of phase 40 (`AUTO_SET_PHASE40.md`). Locks and strength are in
`AUTO_SET_LOCKS_STRENGTH.md`. Phase 20's rule is unchanged.

## Use

The options button next to **AUTO SET** has an **Amp channel selection of the
Matrix MID and HIGH lanes** step, under **Amp channel selection of both Dual
Crossover rigs**. It is **off by default**, like the other steps added since
phase 24.

- **When it runs:** in each Matrix lane's pass (phase 40), before that lane's
  GAIN step. It adds no pass of its own; alone, it still starts the two lane
  passes.
- **What it hears:** as phase 40: the band that lane's head receives, and that
  lane alone after its cabinet.
- **What it sets:** that head's channel, with phase 20's rule: the channel
  moves only when the current one cannot reach the style's drive on that band
  and another comes closer. The GAIN step then sets GAIN on the new channel.
- **Judged by what the GAIN step will do:** GAIN's reach when phase 40's step
  is on and not locked; the channel at its knobs otherwise.
- **The LOW lane is not set**, as in phase 40.
- **Kept, with the reason:** locked; the Classic routing (the Amp channel
  selection step sets the head); a Dual Blend or Crossover (their own steps
  set the heads); a lane whose amp is off or on the classic amp engine; the
  routing, amp, channel or input changed while listening; and phase 20's own
  reasons (one channel, no playing, steady playing).
- **The results:** a **MID lane amp channel** and a **HIGH lane amp channel**
  row, each before that lane's gain row. SHOW opens that lane's head.
- **Strength:** not applied: a channel is switched or kept.
- **Lock → Amp channel:** now also keeps the channels of the Matrix MID and
  HIGH lanes' heads. There is no new lock.
- **UNDO AUTO:** restores them.

## Rules

Phase 20's (`analyseChannel`, `AutoSetAmp.h`), unchanged. No audio-thread
code is changed in this phase; the capture is phase 40's.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --matrix-channel`):

- **Controller (`controllerMatrixChannel`, real processor, the DI and each
  lane's rendered head fed to the taps):** off by default; clean with both
  lanes on LEAD: each moves to RHYTHM in its own pass, judged at its knobs,
  with no gain rows; a lane already on RHYTHM stays while the other is
  switched; with the gain step the channel row comes first per lane and the
  channel is judged by GAIN's reach; with the Amp lock the channel step still
  runs; the Amp channel lock runs no pass; Classic, Blend and Crossover kept
  without a pass; an amp that is off; UNDO AUTO.

## Not yet validated

No real DI or listening test. As in phase 40, the controller test feeds the
whole DI as each lane's band, without a crossover or cabinet, and muted or
soloed lanes are not handled apart or tested.
