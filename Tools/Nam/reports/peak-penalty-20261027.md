# Náströnd A2 crop peak penalty, 2026-10-27

Ragnarok's peak error stayed at +1.5 to +2.0 dB through every earlier run,
outside the profile's ±1 dB. An earlier overshoot penalty averaged the excess
over all samples. Only a few attack samples set the peak, so that average was
nearly zero and the peak did not move
([finish-ab-20261025](finish-ab-20261025.md)).

`--peak-weight Name=w` instead penalizes, per training crop, how far the
output's peak exceeds the target's peak, as a squared fraction of the target
peak. On loud crops of the starting models, Ragnarok's output overshot in 66%
of crops (mean term 0.011 against a crop ESR of 0.056), Fimbulvetr's in 47%
(0.002 against 0.024).

This run warm-starts from [continued-16320-20261027](continued-16320-20261027.md)
on the same 16,320 s of data. It uses weights Ragnarok 5 and Fimbulvetr 2, and
a quarter of its crops contain input at or above −6 dBFS. It runs 20,000 steps
with an LR warmup to 0.0002 and a half-life of 8,000, and keeps passed limits
(`--keep-passed`).

| Channel | Validation full ESR, start → selected | Validation level RMS / peak dB | Test full ESR, start → selected | Test level RMS / peak dB, start → selected | Test active p95 / worst | Step |
|---|---|---|---|---|---|---:|
| Ragnarok | 0.0197 → 0.0217 | −0.10 / −0.00 | 0.0187 → 0.0190 | −0.05 / **+1.52** → −0.13 / **−0.22** | 0.079 / 0.270 → 0.074 / 0.225 | 18500 |
| Fimbulvetr | 0.00678 → 0.00678 | −0.02 / +0.69 | 0.0082 → 0.0082 | −0.04 / +0.90 (unchanged) | 0.026 / 0.088 | start |
| Others | unchanged | | unchanged | | | start |

## Reading

- **Ragnarok now passes level on both splits.** Its peak fell inside ±1 dB
  within 500 steps and stayed there for the whole run, near 0 dB. On test, the
  peak went from +1.52 to −0.22 dB for a 1% rise in full ESR (0.01874 to
  0.01895), and its active p95 and worst fell. On validation the ESR cost is
  10% (0.0197 to 0.0217). It still fails full ESR and the active limits.
- **Fimbulvetr keeps its start.** Its peak error under the penalty sat at +0.2
  to +0.4 dB rather than +0.7 to +1.2, but no step beat the start's ESR: the
  best was 0.00679 at step 18,500 against 0.00678. The starting model already
  passes level on both splits.
- The other three channels trained without a penalty and no step beat their
  starting models.
- A weight of 5 holds Ragnarok's peak near 0 dB, well inside the limit. A
  smaller weight might hold it near +0.5 dB at less ESR cost.

## Adopted set

Fenrir, Surtr, Nidhoggr and Fimbulvetr from continued-16320-20261027;
Ragnarok from this run. Mean test full ESR 0.0174, mean validation 0.0165.
All five channels still FAIL the profile (full ESR for four channels; active
p95 and worst for all). No release approval.
