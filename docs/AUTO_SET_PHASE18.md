# Auto Set — phase 18 (Cabinet mic alignment and blend)

Phase 18 lets Auto Set set how the Classic cabinet's two mics combine when
both are heard. Two mics on one speaker reach the cabinet output at slightly
different times. Blended, they comb: some frequencies add up and others
cancel. Phase 9 places a single modeled mic and leaves two blended mics alone.
This step lines the two up and then sets their blend. Earlier phases are
described in `AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE17.md`; locks and strength
in `AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in the earlier
phases.

## Use

The options button next to **AUTO SET** has a **Cabinet mic alignment and
blend** step, on by default.

- **When it runs:** only when, as Auto Set starts, the routing is Classic, the
  cabinet is on and both mics are heard (the A/B blend between 0 and 100%). It
  adds one pass of about 6 seconds, after the cabinet mic pass and before the
  Dual step's place. Otherwise there is no pass and the row says why:
  "one mic heard (mic A)", "cabinet off", or "Dual and Matrix routing".
- **What it sets:** mic A's and mic B's DELAY, mic B's POLARITY, and the A/B
  BLEND.
- **What it leaves alone:** each mic's level, source, IR or model, and cuts,
  plus the modeled mic placement (phase 9).
- **The results:** a **Cabinet mic blend** row shows the delay and polarity
  moves, with the cancellation they remove; then the blend, the Tone EQ fit
  before and after, and the offset and correlation it measured. The row's
  SHOW button opens the cabinet.
- **Strength:** scales the blend change only. The alignment is applied whole,
  because a part-way delay would leave the mics combing.
- **Lock → Cabinet mic blend:** keeps the four settings and skips the pass.
- **UNDO AUTO:** restores them.

## Measurement

The pass hears each mic on its own:

- **What the taps carry:** `Tap::micA` is mic A after its level, polarity and
  delay. `Tap::micB` is mic B after its own. Both are taken before the A/B
  blend and the cabinet's on/off fade.
- **Rings:** `micA` uses the sample ring; `micB` shares the Matrix HIGH lane's
  ring, which no other pass listens to at the same time.
- **How they are filled:** the cabinet fills them only while Auto Set listens,
  in Classic routing, for blocks in which mic B renders.

If the routing, the cabinet, the mics' sources or the blend (to one mic)
change while the pass listens, nothing is changed. A delay, polarity or blend
the player edits while the pass listens is kept.

## Rules (`analyseMics`, `AutoSetCabMics.h`)

The first 4 seconds of the captures are used.

- **Offset:** the cross-correlation of mic A and mic B (FFT), searched within
  ±5 ms and refined to a fraction of a sample. Its peak gives how much later
  mic B arrives. The sign at the peak gives whether the mics are opposed.
- **Coherence:** the normalised correlation at the peak. Mics below 0.3 are
  too different to align, for example two different cabinets' IRs.
- **Alignment:**
  - The mic that arrives first is delayed by the offset. If the later mic
    already has a delay, that delay comes down instead. Delays stay within
    0–20 ms.
  - Mic B's polarity flips when the mics are opposed.
  - It is applied only when the aligned mix cancels at least 1 dB less than
    the mix now, in its worst third-octave band (80 Hz – 6.3 kHz, within 30 dB
    of the strongest band, against the blend-weighted mics' power sum).
- **Prediction:** a delay change is a fractional shift of the captured mic,
  with linear interpolation as the cabinet's own delay line uses. The mix is
  computed sample by sample.
- **Blend:** on the mics as they will be, in 5% steps up to 30% either way,
  kept within 5–95% so both mics stay heard, toward the Tone EQ target. The
  fit is the one phases 8, 9, 13 and 17 use. It moves only when the fit
  improves by more than 0.5 dB.
- **Silent mic:** a mic that is not heard keeps everything, and the row names
  the mic.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --mics`):

- **Rules (`micsRules`), 44.1, 48 and 96 kHz.** The fixtures are one
  on-target noise source as mic A, and as mic B the same source, later and
  4 dB duller above 4 kHz.
  - Mic B 0.6 ms late is found within 0.03 ms (0.61 ms at 48 kHz, correlation
    0.996). Mic A is delayed by it, and the worst-band cancellation falls
    (16.4 → 0 dB at 48 kHz).
  - Running again on the aligned mics moves nothing.
  - With mic B already delayed 2 ms, mic B's delay comes down instead.
  - Mic B late and opposed: its polarity flips and the offset is removed
    (16.4 → 0 dB).
  - Unrelated mics (correlation 0.06) are not aligned.
  - With aligned mics and a dark mic A, the blend moves toward mic B
    (30 → 60%), within 5–95%.
  - A silent mic B keeps everything, and silence reports no playing.
- **Capture (`micsCapture`):**
  - In the cabinet, the captured mic A (after its level, polarity and delay)
    and mic B sum, with the blend weights, to the cabinet output.
  - The processor feeds the taps in Classic routing with both mics heard
    only: not for one mic, not in Dual.
- **Controller (`controllerMics`), real processor, fixtures fed to the mic
  taps:**
  - The pass runs alone. Mic A's delay and mic B's polarity match the
    analysis (5.5 → 0.7 dB tone error).
  - 50% strength moves the blend half-way and the alignment whole.
  - The lock keeps everything and skips the pass.
  - One mic heard, and Dual routing, have no pass and say why.
  - The blend going to one mic while listening voids the pass; a delay edited
    while listening is kept.
  - UNDO AUTO restores the settings.
- **Full run (`controllerIntegration`, Classic, one mic):** nineteen result
  rows, with the mic blend row (kept, one mic heard) after the cuts row.
- **Locks (`controllerEveryLock`):** all twenty-one locks at four strengths.

## Not yet validated

The 0.3 coherence, the 1 dB alignment threshold, the ±5 ms search and the 30%
blend limit are starting values. They have been checked on synthetic noise
with a delayed, filtered copy as the second mic, not on real two-mic cabinet
IRs, a real DI or listening tests. Real mics on one cabinet also differ in
frequency response. One delay cannot line them up at every frequency, so the
alignment removes the broad comb, not every notch.
