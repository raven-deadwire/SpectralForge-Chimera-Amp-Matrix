# A/B level match

A louder sound usually seems better. **MATCH**, next to **A / B / COPY**,
makes the A/B comparison about tone rather than volume: while you play, it
measures the output loudness of each slot and turns the louder slot down to
the quieter one.

## Use

1. Set up two sounds in A and B (or COPY one to the other and edit it).
2. Press **MATCH**.
3. Play a few seconds on A, then a few seconds on B. Once both have been
   heard (1.5 s of playing each), the louder one is turned down. While it
   cuts, the button shows the cut in dB (for example `-6.0`).
4. Press **MATCH** again to hear the real levels.

## Behaviour

- **Never boosts.** Only the louder slot is turned down, so MATCH cannot
  clip the output or push the limiter. The cut is at most 24 dB and ramps
  over 50 ms.
- **Measurement:** ITU-R BS.1770 K-weighting on the final output (after the
  limiter), about 3 s short-term, a running mean until the first 3 s are
  heard. Blocks below -60 LUFS (silence, pauses) are not counted. The first
  250 ms after an A/B switch is skipped so the switch transient is not
  measured.
- Each slot's measurement keeps following its own sound while it plays, so
  edits inside a slot are tracked within a few seconds. Measuring continues
  with MATCH off, so turning it on matches at once.
- **COPY** gives the target slot the copied slot's measurement (they now
  sound the same). Loading a project or reference file forgets both
  measurements.
- **Monitoring only.** MATCH is not a parameter, is not part of A or B, is
  not automated and is not saved: a project always opens with it off. While
  on, it does lower what you record or bounce from the louder slot, so turn
  it off before printing.
- With MATCH off (or nothing to cut) the output is bit-identical to a build
  without the feature.

Code: `Source/ABLevelMatch.h`, applied in `ChimeraProcessor::process` after
the live limiter, before the output meter (the meter shows what you hear).

## Tests

`Tests/ABLevelMatchTests.cpp` (`ChimeraABLevelMatchTests`):

- Meter: a full-scale 997 Hz sine reads -3.01 LUFS (BS.1770 reference); a
  slot not yet heard has no reading; 30 Hz reads lower, 4 kHz higher.
- Match: two sines 12 dB apart end within 0.2 dB; the quieter one is
  untouched sample for sample; MATCH off is bit-identical; silence does not
  move a reading; the cut stops at 24 dB; COPY and project load behave as
  above.
- Processor: the same preset 6 dB louder on B is matched to within 0.5 dB on
  the real output; a saved and restored project has MATCH off; loading a
  project forgets the readings; MATCH toggled on and off leaves the output
  bit-identical to an instance that never used it.
