# Náströnd A2 Fenrir quiet window, 2026-10-28

Fenrir fails release profile v3 on one validation quiet window
([release-profile-v3-20261028](release-profile-v3-20261028.md)). This run tried
to fix it by training, then located the cause.

## Run

Warm start from the channel-loss models, 20,000 steps, LR warmup to 0.0002,
half-life 8,000, `--keep-passed`, Ragnarok peak weight 1.5, a quarter of the
crops loud. New: `--tail-all-parts` draws tail crops from the final decay of
each of the 27 capture parts instead of only the last one, and the tail share
rises from 10% to 25%. `--target-limits Fenrir=quiet` ranks steps that pass
Fenrir's quiet limit first.

**No step passed.** Fenrir's worst quiet residual peak stayed between −52.2 and
−55.3 dBFS at all 40 evaluations (limit −60); its RMS moved around the −70
limit. Its validation full ESR was 0.0184–0.0199, worse than the start's 0.0171,
so Fenrir keeps its start. Ragnarok's best step (18,000) improved validation by
1.3% (0.0213 → 0.0210) but worsened test by 4.7% (0.0186 → 0.0194); that is
within run-to-run noise, so the packaged Ragnarok is kept. No channel changes.

## Cause

The failing window starts at 23.51 s of the validation signal. The largest
error, −53.3 dBFS, is 655 samples (14 ms) after the input's last nonzero sample.
`common.signal` overwrites its last 0.5 s with digital zero, so the final note,
still sounding at −55 dBFS RMS, stops within one sample. After that step the
native DSP keeps ringing for about 11 ms at about −54 dBFS, most likely its
oversampling or other filters answering the discontinuity. The A2 model barely
reproduces this ringing: its error (−53.3 dBFS) is as large as the reference
(−53.8 dBFS).

The test signal has the same hard cut, but its final note is at −19.5 dBFS, so
that window counts as active and is judged by active ESR instead of the quiet
limit. Test passes the quiet limit by window classification, not by better
modeling.

A real instrument DI does not drop from a sounding note to digital zero within
one sample. The failure comes from the synthetic measurement signal rather than
from playing. Whether to fade the signal's end, add an exclusion to the
profile, or keep the failure is an owner decision.
