# Náströnd head + cab: guitar-range qualification (pre-registered 2026-11-04)

Written and committed before the signals it names were generated. No model
has seen or been scored on them.

## Why

Náströnd is a guitar amp. The shared synthetic generator (`common.signal`)
also plucks 27.5, 30.9 and 41.2 Hz fundamentals so that the same signal can
qualify the Niflheimr bass amp. After the preset cabinet lifts the lows, these
bass-range notes dominate the head + cab error. On the original 24 s held-out
signals, every Náströnd head + cab window over the v3 limits had a sounding
fundamental below 46 Hz. Windows whose lowest note is 46 Hz or higher were
within the v3 limits on both splits for all five channels (refine5 models).
The released head NAM convolved with the same cabinet IR fails the same
sub-46 Hz windows, so this is not specific to head + cab training. The
training set (7,680 s) already holds about 1,000 such notes, so more of the
same data is not expected to help.

That split was measured on signals we had already scored. This document
therefore fixes the rule first and qualifies on new signals.

## Rule

- **Scope:** Náströnd head + cab captures only. Niflheimr (bass) and the
  Náströnd head pack keep the full-range signals.
- **Input range:** guitar range, plucked fundamentals of 46 Hz and above
  (`common.GUITAR_NOTES`: 55 to 329.6 Hz; the lowest note of an 8-string
  guitar, F#1, is 46.2 Hz). Everything else in the generator is unchanged,
  including the 30 Hz to 14 kHz noise section, silence and decay tail.
- **Signals:** new validation and test signals, 120 s each (the old ones were
  24 s), seeds 26110401 (validation) and 26110402 (test). Each is captured
  through the same frozen renderer and the same preset lane cabinet IRs as
  `ndc-data`.
- **Limits:** release profile v3, unchanged, with the frozen `<Channel>-Cab`
  anchors. Both splits must pass.
- **Model choice:** for each channel, among the existing candidate exports
  (`ndc-full`, `ndc-refine` to `ndc-refine5`, `ndc-refine6`,
  `ndc-refine6-final`), choose on the **new validation signal only**: the
  lowest full-signal ESR among candidates that pass v3 there. If none pass,
  choose the lowest full-signal ESR. The new test signal is then evaluated
  once, on the chosen models, and is not used for any choice.
- **Listening:** release still needs the owner's instrument-DI listening.

## Disclosure

The pack description must say that the head + cab models were qualified on
guitar-range input (fundamentals 46 Hz and above), and that very low
bass-range input is outside that qualification.
