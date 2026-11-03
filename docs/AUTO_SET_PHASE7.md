# Auto Set — phase 7 (Final EQ and output level)

Phase 7 adds a last listening pass, at the very end of the chain. It makes
small Final EQ corrections and sets the output trim so the plug-in leaves at a
working loudness, with its peaks below the limiter ceiling. Earlier phases are
described in `AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE6.md`; locks and strength in
`AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has a **Final EQ and output level**
step, on by default. It adds one pass of about 6 seconds after the de-esser and
limiter pass (the last one when that step is off). Keep playing as you
normally would, at your usual dynamics, so the loudness is representative.

- **The results** get a **Final EQ + output** row: the Final EQ moves, then
  the output trim with the loudness heard. Its SHOW button opens the Final EQ.
- **Strength** scales the Final EQ gains and the trim change (as for input
  gain: a bounded loudness correction × strength, in 0.5 dB steps). The high
  pass is on or off. Peak-protective attenuation is not reduced by strength.
- **Lock → Final EQ** and **Lock → Output level** keep each part. Locking
  both skips the pass.
- **UNDO AUTO** restores the Final EQ and the output trim.

## Measurement

A new sample tap, `Tap::master`, is placed at the limiter input. This is after
the Final EQ and the output trim, at the same point as the window tap the
limiter rule uses. Everything before it has been set by the earlier passes,
so this pass hears the finished sound. The first 300 ms after the previous
pass are skipped.

- **Spectrum:** the third-octave spectrum of the playing (as for the Tone EQ),
  compared with the style target aligned to 630 Hz – 2 kHz. This is above the
  low-mid region the pass judges, so a low-mid bump does not make the top
  look dark.
- **Loudness:** ITU-R BS.1770 integrated loudness. The signal is K-weighted
  (the standard's filters, redesigned for the session rate),
  read in 400 ms blocks with 75% overlap, and gated at −70 LUFS absolute and
  −10 LU relative. Each actual output channel is filtered and squared before
  summing its energy. Mono contributes one channel; stereo contributes two.
  Polarity, panning and unequal channel levels cannot cancel the measurement.
  The meter accepts any complete gated block. Auto Set additionally requires
  detected playing and at least 10 gated blocks before changing loudness;
  shorter attacks can still trigger peak-protective attenuation.
- **Peak:** the largest sample.

## Rules (`analyseOutput`)

| Move | Rule |
|---|---|
| Rumble high pass (guitar only) | 50–63 Hz ≥ 3 dB over the target → 12 dB/oct high pass at 70 Hz |
| Low-mid bell, 315 Hz, Q 1 | −½ × (250–400 Hz excess − 630 Hz–1 kHz excess) |
| Air shelf, 8 kHz (bass 5 kHz) | −½ × the top band excess (guitar 6.3–10 kHz, bass 4–6.3 kHz) |

- Gains are limited to ±3 dB, set in 0.5 dB steps, and moves below 1 dB are
  dropped. Bands more than 45 dB under the strongest band are ignored.
- The Tone EQ already did the main balancing, so these are half-way
  corrections of what remains after the dynamics.

**Output trim:**
- The change aims at −18 LUFS. The controller first selects the moves it can
  actually apply (locks, bypass, free bands and strength included), then renders
  those new static filters on the captured channels and remeasures their LUFS
  and sample peak. Unapplied EQ never contributes to output compensation.
- The desired loudness change is limited to ±12 dB. Changes below 1 dB
  after 0.5 dB quantisation are left alone for stability. Strength then scales
  this change, rounded to a 0.5 dB increment from the existing trim.
- Peak protection is applied **after** strength, quantisation and this deadband.
  When needed, the trim is rounded toward attenuation in 0.5 dB steps to keep
  the predicted captured sample peak at or below −1 dBFS.
- The ±12 dB per-run limit and the parameter range (−36 to +12 dB) still apply.
  If either prevents enough attenuation, or the output is locked, the result
  explicitly reports an unmet peak ceiling. A locked output is never changed;
  new EQ is omitted if it would worsen an already unsafe peak.
- These are predictions for the captured signal at settled static settings,
  not a guarantee for harder future playing, EQ switching transients, Dynamic
  EQ interactions or intersample true peaks. The limiter remains necessary.

**Where the moves go:**
- On Final EQ bands that are off and have no Dynamic EQ slot, after the last
  active Dynamic EQ band. Existing curves and dynamic-filter order are kept.
- The measurement is taken after the Final EQ, so a re-run sees the earlier
  run's moves and finds little left to correct.
- A user curve behind a bypassed Final EQ is never made audible. The Final EQ
  part is then kept, and the trim still applies.

## Testing

`ChimeraAutoSetTests`:
- **Rules (`levelRules`), at 44.1, 48 and 96 kHz:**
  - Silence reports no playing.
  - A mono 997 Hz sine at −20 dBFS reads approximately −23.01 LUFS;
    two identical output channels read approximately −20 LUFS.
  - On-target, quiet noise gets no Final EQ move, and the trim reaches −18
    LUFS.
  - Loud noise is trimmed down.
  - Quiet noise with sharp clicks is trimmed only until the peaks reach the
    ceiling.
  - A +8 dB bright top gets an air cut and a −8 dB dark top an air lift (2–3
    dB). A +8 dB low-mid bump gets a 315 Hz cut.
  - Rumble under a guitar gets the 70 Hz high pass; a bass never does.
- **Controller (`controllerLevel`), real processor, fixtures fed to the
  capture taps:**
  - The pass runs last. A bright, quiet output gets an air shelf on a free
    Final EQ band, matching the analysis, and the trim moves by the analysis.
  - Strength scales the shelf gain and the requested loudness change; trim
    is recomputed for the actual legal EQ values, then peak-limited.
  - The output lock keeps the trim and the Final EQ lock keeps the Final EQ.
    Locking both skips the pass.
  - A bypassed user curve stays bypassed.
  - UNDO AUTO restores owned writes; newer manual/host edits are retained.
- **Full run (`controllerIntegration`):** six passes and eight result rows.
  The trim and the Final EQ match the analysis.
- **Locks (`controllerEveryLock`):** all ten locks at four strengths.

## Not yet validated

The follow-up to #226 adds an independent libebur128 comparison and rendered
regressions; see [the validation report](verification/auto-set-phase7/REPORT.md).

The −18 LUFS target, rumble threshold, half-way corrections, band edges and
±3 dB EQ limit remain listening hypotheses. Real guitar/bass DI through actual
DAW mono/stereo buses, a second end-to-end external meter, true-peak checks,
Dynamic EQ interactions and listening tests remain required for release
qualification. Synthetic and reference-meter agreement do not establish
musical quality or real-host routing correctness.
