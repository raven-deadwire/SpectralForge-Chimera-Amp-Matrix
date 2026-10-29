# Náströnd A2 Fenrir-only retrain, 2026-10-29

Goal: make Fenrir pass the v3 quiet limit on validation (worst quiet residual
peak ≤ −60 dBFS), which fails on the low-level release after the input stops
([faded-heldout-20261029](faded-heldout-20261029.md)).

## Run

Warm start from the channel-loss models; only Fenrir is trained
(`--train-channels Fenrir`). New loss term `--block-norm-samples 512`
(weight 0.5): the error is also normalized per 512-sample block by that
block's reference energy, floored 50 dB below the channel's mean energy, so a
quiet decay inside a loud crop is weighted. Also `--tail-all-parts`, tail
share 25%, `--target-limits Fenrir=quiet`, `--keep-passed`; 20,000 steps, LR
warmup to 0.0004 (twice earlier runs), half-life 8,000.

## Result

**No step passed.** Fenrir keeps the packaged model (validation full ESR
0.0171, quiet peak −53.3 dBFS).

| | Previous run (tail crops) | This run (block-normalized loss) |
|---|---|---|
| Worst quiet residual peak, range over 40 evaluations | −52.2 to −55.3 dBFS | −53.1 to −58.9 dBFS |
| Closest to the −60 dBFS limit | −55.3 | −58.9 (step 10,500), −58.8 (16,000) |
| Worst quiet residual RMS | about −70, often failing | −70.2 to −76.2, always passing |
| Validation full ESR | 0.0184–0.0199 | 0.0177–0.0227 |

The block-normalized loss moved the quiet peak about 4 dB toward the limit and
kept the RMS inside it, but the peak never crossed −60 dBFS and every step's
full ESR was worse than the packaged model's.

Notes:

- The other four channels received no loss gradient, but Adam's weight decay
  still shrank their weights (validation ESR near 0.9 by step 1,000).
  Selection kept their starting states, so the package is unaffected. A future
  single-channel run should optimize only the trained channel's parameters.
- Training slowed from about 3 steps/s to under 1 step/s after step 13,000 on
  this machine; the run took about 5.5 hours.
