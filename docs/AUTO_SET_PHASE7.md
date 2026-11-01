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
  gain: before + strength × change, in 0.5 dB steps). The high pass is on or
  off.
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
  −10 LU relative. At least 10 gated blocks are needed. The tap holds the mean
  of the two channels, and it is reported as the stereo output it stands for:
  both channels equal, +3 dB. A mono track reads 3 dB high.
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
- The change aims at −18 LUFS. It accounts for the loudness change of the
  Final EQ moves, worked out from the K-weighted band powers through the
  moves' response.
- The trim is held back where the peaks, plus the moves' largest boost on a
  strong band, would pass the −1 dBFS limiter ceiling. The trim sets the
  level; the limiter only catches the occasional peak.
- The change is limited to ±12 dB, set in 0.5 dB steps, and changes below
  1 dB are left alone.

**Where the moves go:**
- On Final EQ bands that are off and have no Dynamic EQ slot, so a curve
  already on the Final EQ is kept.
- The measurement is taken after the Final EQ, so a re-run sees the earlier
  run's moves and finds little left to correct.
- A user curve behind a bypassed Final EQ is never made audible. The Final EQ
  part is then kept, and the trim still applies.

## Testing

`ChimeraAutoSetTests`:
- **Rules (`levelRules`), at 44.1, 48 and 96 kHz:**
  - Silence reports no playing.
  - A 997 Hz sine at −20 dBFS reads −20 LUFS.
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
  - At 50% strength both the shelf and the trim change halve.
  - The output lock keeps the trim and the Final EQ lock keeps the Final EQ.
    Locking both skips the pass.
  - A bypassed user curve stays bypassed.
  - UNDO AUTO restores everything.
- **Full run (`controllerIntegration`):** six passes and eight result rows.
  The trim and the Final EQ match the analysis.
- **Locks (`controllerEveryLock`):** all ten locks at four strengths.

## Not yet validated

The −18 LUFS target, the 3 dB rumble limit, the half-way corrections, the
band edges and the ±3 dB limit are starting values. They have been checked on
synthetic signals, not on real recordings, other loudness meters or listening
tests.
