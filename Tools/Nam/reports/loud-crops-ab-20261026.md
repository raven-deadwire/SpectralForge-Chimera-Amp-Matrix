# Náströnd A2 loud-input crops A/B, 2026-10-26

Both arms warm-start from the final-step models of the finishing run
(`--warm-start-final`, mean full ESR 0.0249). Each runs 10,000 steps with the
LR falling from 0.0003 to below 0.0001 (half-life 5,000), selects on the full
validation signal, and uses the same 480 s capture. **All five channels still
FAIL.** No release approval.

| Channel | Control: full ESR / active median | Loud crops 30%: full ESR / active median | Loud-note ESR, control → loud |
|---|---|---|---|
| Fenrir | 0.0265 / 0.0134 | 0.0276 / 0.0137 (step 0) | 0.082 → 0.082 |
| Surtr | 0.0261 / 0.0162 | 0.0268 / 0.0173 | 0.055 → 0.053 |
| Nidhoggr | 0.0288 / 0.0170 | 0.0298 / 0.0185 (step 0) | 0.075 → 0.076 |
| Fimbulvetr | **0.0096** / 0.0056 | **0.0096** / 0.0058 | 0.020 → 0.019 |
| Ragnarok | 0.0285 / 0.0197 | 0.0298 / 0.0205 (step 0) | 0.075 → 0.079 |
| **Mean full ESR** | **0.0239** | 0.0247 | |

"Loud notes" are the validation notes played at −6 or −3 dBFS peak. "Step 0"
means no checkpoint beat the starting model.

## Reading

- **Fimbulvetr now passes the full-ESR limit**, 0.0096 against <0.01: the first
  limit any channel has passed. It still fails the active median (0.0056
  against ≤0.005) and p95 (0.035 against ≤0.01).
- **Loud-input crops did not help.** Training on 30% loud crops left the loud-note
  error unchanged on every channel, and three channels kept their starting
  model. Loud-note ESR has stayed near 0.08 for Fenrir across the last three
  runs while quieter notes improved. That points to a limit of the model's
  capacity for heavily driven inputs, not to too little exposure to them. In
  TONE3000's A2 that capacity is fixed: 8 channels is the largest width the
  player accepts.
- The control arm's low-LR continuation still improved, but only slightly:
  mean full ESR 0.0249 → 0.0239.
