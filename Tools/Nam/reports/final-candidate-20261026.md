# Náströnd A2 final candidate, 2026-10-26

This candidate is A2-Full, the TONE3000-compatible width of 8 channels, under
profile `nastrond-tone3000-fidelity-v2`. **It fails the profile on all five
channels.** Fimbulvetr passes the full-ESR limit on both validation and the
held-out test, and the active median on validation only. No release approval.

## How it was made

- Data: 1,920 s of training audio from four 480 s captures of main `4ef21c16`
  (seeds 640500, 640509, 640518 and 640527), merged with
  `combine_captures.py`. Validation, test and audition are unchanged.
- Training: a warm start from the best previous models (mean full ESR 0.0239),
  then 30,000 steps with the LR falling from 0.002 to 0.00015 (half-life
  8,000). Legacy recipe, 10% tail crops, centering, level-gated selection on
  the full validation signal. About 3.9 hours on 4 CPU threads.
- The starting LR was too high. Every channel got worse until about step
  15,000, recovered as the LR decayed, and ended better than it started.

## Results

| Channel | Validation full ESR | Test full ESR | Test active median / p95 / worst | Test quiet RMS / peak dBFS | Test level RMS / peak dB |
|---|---:|---:|---|---|---|
| Fenrir | 0.0229 | 0.0250 | 0.013 / 0.123 / 0.267 | −88.8 / −81.9 | −0.09 / +0.33 |
| Surtr | 0.0219 | 0.0228 | 0.014 / 0.082 / 0.312 | −76.8 / −69.6 | −0.05 / +0.97 |
| Nidhoggr | 0.0249 | 0.0299 | 0.019 / 0.095 / 0.478 | −77.6 / −70.7 | −0.15 / −0.15 |
| Fimbulvetr | **0.0084** | **0.0095** | 0.0069 / 0.031 / 0.085 | −82.0 / −75.5 | −0.05 / +0.86 |
| Ragnarok | 0.0259 | 0.0251 | 0.018 / 0.094 / 0.478 | −70.2 / −63.8 | −0.07 / **+2.31** |
| **Mean** | **0.0208** | **0.0225** | | | |

On validation, Fimbulvetr's active median is 0.0047, which passes the 0.005
limit; on the test split it is 0.0069. The test set was never used for
training or selection, and its numbers are close to validation, so the
models generalize to unseen excitation.

## Path from the restart

| Stage | Mean full ESR (validation) |
|---|---:|
| First v2 run, 5,000 steps | 0.098 |
| 50,000 steps | 0.032 |
| Low-LR finish | 0.025 |
| Continuation on 480 s | 0.024 |
| This candidate, 1,920 s | **0.021** |

## What remains

- **The active p95 and worst limits are the gap that remains:** p95 is 3–12
  times its limit and worst 4–24 times. The diagnostics found no single cause.
  Wider models and more data each reduce it gradually, and even a width-32
  model (not A2) stays near 0.07 p95. The current limits look out of reach
  for any practical NAM on this DSP and need an owner decision informed by
  listening.
- **Ragnarok's peak error stays above +2 dB.** It comes from a few attack
  samples.
- `Nastrond-A2-final-ABX.zip` (8 blind trials per channel) is ready for that
  decision; the answer key is kept outside the repository.
