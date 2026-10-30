# Náströnd A2 data-quantity diagnostic, 2026-10-26

Question: does more training data reduce A2 error, especially on loud notes?
Fenrir, width 8 (A2), `diagnose_capacity.py` with identical settings. The arms
differ only in training data: 480 s (seed 640500) against 960 s (seeds 640500
and 640509, concatenated). The renderer accepts at most 600 s per capture.
Validation is identical.

| Step | Full ESR 480 s → 960 s | Loud-note ESR 480 s → 960 s | Quieter-note ESR 480 s → 960 s |
|---:|---|---|---|
| 10,000 | 0.0436 → 0.0330 | 0.118 → 0.082 | 0.0231 → 0.0180 |
| 25,000 | 0.0256 → **0.0220** | 0.065 → **0.053** | 0.0163 → **0.0121** |

At 25,000 steps the active median and p95 fell from 0.0162 / 0.100 to 0.0147 /
0.093.

## Reading

- **Twice the data helps everywhere, by a moderate amount.** At 25,000 steps,
  full ESR is 14% lower, loud-note ESR 18% lower and quieter-note ESR 26% lower.
  This is the first change in this series that reduced loud-note error at A2
  width. The gain is about the same size as doubling the width (16 channels:
  loud-note error 0.054).
- Neither arm had converged at 25,000 steps.

## Combined picture (2026-10-24 to 2026-10-26)

| Factor | Effect on loud-note error | Source |
|---|---|---|
| Memory beyond 132 ms | none; the native DSP has none (ESR ≤ 4.6e-7) | history-diagnostic |
| Longer model memory | none | memory-diagnostic |
| Output-amplitude-weighted loss | none | finish-ab |
| 30% loud-input crops | none | loud-crops-ab |
| Width 8 → 16 → 32 | −17% → −22% | capacity-diagnostic |
| Data 480 s → 960 s | −18% | this report |
| Training budget 5k → 50k steps | about −50% overall | train-v2-50k |

No single cause dominates. Loud, heavily saturated notes are the hardest part
of the mapping, and only more capacity, data and training reduce their error,
gradually. Even the widest and best-trained diagnostic, width 32 (not A2),
left active p95 near 0.07, seven times its 0.01 limit.
