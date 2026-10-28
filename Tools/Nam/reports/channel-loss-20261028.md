# Náströnd A2 channel-normalized loss, 2026-10-28

Full ESR is the error energy over the whole signal divided by the reference
energy, so loud passages dominate it. The training loss so far normalized each
4,096-sample crop by its own energy, which weights quiet and loud crops
equally. This run uses `--loss-normalization channel`: each crop's error is
divided by the channel's mean energy, which matches full ESR.

It warm-starts from the packaged set ([tone3000-package-20261028](tone3000-package-20261028.md))
with otherwise the same settings as [peak-weight-20261027](peak-weight-20261027.md):
20,000 steps, LR warmup to 0.0002, half-life 8,000, a quarter of the crops loud,
Ragnarok peak weight 1.5, and `--keep-passed`.

| Channel | Validation full ESR, start → best | Test full ESR | Test active p95 / worst | Test level RMS / peak dB | Adopted |
|---|---|---|---|---|---|
| Fenrir | 0.0171 → 0.0175 (no step better) | 0.0184 | | | start |
| Surtr | 0.0166 → 0.0174 (no step better) | 0.0190 | | | start |
| Nidhoggr | 0.0202 → 0.0204 (no step better) | 0.0224 | | | start |
| Fimbulvetr | 0.00678 → 0.00653 (step 11,500) | 0.00821 → 0.00822 | 0.026 / 0.088 → 0.028 / 0.084 | −0.08 / **+1.07** | **no**: fails level on test |
| Ragnarok | 0.0214 → 0.0213 (step 19,000) | 0.0192 → **0.0186** | 0.074 / 0.253 → 0.069 / 0.212 | −0.10 / +0.01 | yes |

## Reading

- **Matching the loss to full ESR does not move it.** Three channels found no
  step better than their start, and the two that did improved by 0.4% and 4%
  on validation. Mean test full ESR moves from 0.0175 to 0.0174. The gap to
  the 0.01 limit is not a loss-weighting problem.
- **Fimbulvetr's gain does not hold on test.** Its validation ESR fell 4%, but
  test ESR is unchanged and its test peak rose to +1.07 dB, outside ±1 dB,
  where the packaged model passes. It is not adopted.
- **Ragnarok improves on test** by 3.5%, with a lower active p95 and worst, and
  stays inside the level limit.
- With this, every lever available at A2 width has been tried: data (now
  diminishing to −3.6% per doubling), training budget, LR schedule, memory,
  loud crops, amplitude and peak weighting, and loss normalization. Full ESR
  below 0.01 for Fenrir, Surtr, Nidhoggr and Ragnarok would need a wider model
  than A2 allows, or a different criterion.
