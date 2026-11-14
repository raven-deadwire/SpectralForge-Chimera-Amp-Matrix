# Auto Set — phase 31 (Dual and Matrix cabinet mic alignment and blend)

Phase 31 extends phase 18 (`AUTO_SET_PHASE18.md`, the Classic cabinet's two
blended mics: their timing, polarity and blend) to the other routings: the two
mics of each **Dual rig's** and each **Matrix lane's** cabinet. Earlier phases
are described in `AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE30.md`; locks and
strength in `AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in the
earlier phases.

## Use

The options button next to **AUTO SET** has a **Dual and Matrix cabinet mic
alignment and blend** step, listed before the lane mic placement step. It is
**off by default**, like the other steps added since phase 24, until it has
had a DI and listening check.

- **When it runs:** a pass of its own for each rig or lane whose cabinet is on
  with both mics heard (the A/B blend between 0 and 1), after the cabinet
  passes and before the Dual or Matrix pass, so the lanes are then judged with
  their mics aligned. **Each pass adds listening time** (as phase 18's pass
  does): up to two passes in Dual, three in Matrix. In the Classic routing it
  is kept: the Cabinet mic alignment and blend step (phase 18) sets rig 1.
- **What it sets, per rig or lane:** mic A's and mic B's delay and mic B's
  polarity (the alignment), and the A/B blend.
- **The blend is judged only where a lane plays the whole instrument:** the
  two rigs of a Dual **Blend**. A Dual **Crossover** rig or a Matrix lane
  plays one band, and its balance alone does not say how to blend; there the
  mics are aligned and the row says "BLEND kept: this lane plays one band, so
  its balance alone does not say how to blend".
- **What it leaves alone:** each mic's level, source, IR or model, the cuts,
  the lanes' levels and polarities.
- **The results:** a row per rig or lane, **Rig 1 / LOW lane mic blend**,
  **Rig 2 / MID lane mic blend**, **HIGH lane mic blend**, after the Classic
  mic row: "mic A DELAY 0.00 ms > 0.61 ms, mic B POLARITY inverted
  (cancellation 12.9 > 0.0 dB); tone error 5.4 > 0.6 dB; mic B 0.61 ms after
  mic A, correlation -1.00", or why it was kept. SHOW opens that rig's
  cabinet.
- **Strength:** scales the blend change. The alignment is applied whole (a
  part-way delay would leave the mics combing), as in phase 18.
- **Lock → Cabinet mics:** now keeps the mic delays, mic B polarity and blend
  of all three rigs (it kept rig 1's). There is no new lock.
- **UNDO AUTO:** restores them.

The Cabinet mic blend step (phase 18) in a Dual or Matrix routing now says
that this step sets those mics.

## Rules

Phase 18's (`analyseMics`, `AutoSetCabMics.h`), applied to one cabinet at a
time: the cross-correlation of the two mics within ±5 ms gives the offset and
relative polarity; the alignment is applied when the mics are coherent
(|correlation| at least 0.3) and the aligned mix cancels at least 1 dB less in
its worst third-octave band; then the blend moves in 5% steps, up to 30% and
within 5–95%, when the fit to the style's target improves by more than 0.5 dB.
`analyseMics` has a new `judgeBlend` argument (default true: phase 18
unchanged); false skips the blend.

**Kept, with the reason:** locked; the Classic routing; cabinet off; one mic
heard; the routing, cabinet or mic sources changed while listening; no playing
heard; a mic silent; mics already aligned (and blended near the target); mics
too different to align.

## Engine

The mic taps (`Tap::micA`, `Tap::micB`) heard the Classic cabinet only. The
processor now has `setAutoSetMicLane`: -1 (the default) keeps that behaviour;
0–2 feeds the taps from that Dual rig's or Matrix lane's cabinet instead, in a
routing that has that lane. `processBlock` reads one atomic and sets each
cabinet's capture flag; the copy of mic A runs only while a mics pass listens,
as before. The controller sets the lane at the start of each pass and returns
it to -1 when it stops listening.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --lane-mics`):

- **Rules (`laneMicsRules`, 44.1, 48 and 96 kHz):** alignment alone gives
  phase 18's alignment and keeps the blend; a pair that only wants a blend
  move is kept when the blend is not judged.
- **Capture (`laneMicsCapture`, real processor):** the taps are fed from the
  chosen Dual rig or the Matrix HIGH lane with both mics heard; not from a rig
  with one mic, a lane the routing lacks, a lane request in Classic, or the
  Classic request in Dual.
- **Controller (`controllerLaneMics`, real processor, a late and opposed mic
  pair fed to the mic taps):** off by default; a pass per Dual rig, both
  aligned; 50% strength scales the blend and not the alignment; Dual Crossover
  and Matrix lanes are aligned with the blend kept; a one-mic lane and a
  cabinet that is off are reported without a pass; the Cabinet mics lock; the
  Classic routing; sources edited while listening; the passes come before the
  Dual pass; UNDO AUTO.
- **Locks (`controllerEveryLock`):** the Cabinet mics lock now owns the
  twelve delay, polarity and blend parameters.
- Phase 18's own tests (`--mics`) are unchanged and pass.

## Not yet validated

No real DI or listening test. The controller tests feed a synthetic mic pair
to the taps; the engine check covers which lane feeds the taps, not a
re-render of an aligned lane (the cabinet's mic capture itself is phase 18's
test). The audio-thread change is small but has no realtime measurement of its
own beyond the existing realtime tests.
