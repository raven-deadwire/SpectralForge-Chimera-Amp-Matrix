# Náströnd A2 capacity diagnostic, 2026-10-26

Question: does the A2 width (8 channels per layer, the most TONE3000's player
accepts) limit accuracy, especially on heavily driven notes? Only widths 8 and
smaller are A2. Wider models are diagnostic only and are not exported.

## Design

`diagnose_capacity.py` trains Fenrir, the channel with the worst loud-note
error, from scratch. It keeps the A2 layer layout and receptive field
(6,347 samples) and changes only the width. Every arm runs 25,000 steps at
batch 4 with 4,096-frame crops, LR 0.004 with a 6,000-step half-life, seed
6405, and 10% tail crops, on the 480 s capture from main `4ef21c16`. Loud notes
are validation notes played at −6 or −3 dBFS peak.

## Results at 25,000 steps (full validation)

| Width | Parameters | Full ESR | Active median / p95 | Loud-note ESR | Quieter-note ESR | Train loss |
|---:|---:|---:|---|---:|---:|---:|
| 8 (A2) | 12,145 | 0.0256 | 0.0162 / 0.100 | 0.065 | 0.0163 | 0.0206 |
| 16 | 47,201 | 0.0168 | 0.0075 / 0.081 | 0.054 | 0.0084 | 0.0120 |
| 32 | 186,049 | 0.0147 | 0.0061 / 0.069 | 0.051 | 0.0077 | 0.0095 |

## Cabinet check

The A2 models from the loud-crops control arm were measured amp-only and
through the repository's V30 + SM57 IR:

| Channel | Full ESR, amp only | Full ESR, through cab | Loud-note error above 5 kHz |
|---|---:|---:|---:|
| Fenrir | 0.0265 | 0.0269 | 18% |
| Surtr | 0.0261 | 0.0368 | 21% |
| Nidhoggr | 0.0288 | 0.0422 | 24% |
| Fimbulvetr | 0.0096 | 0.0148 | 24% |
| Ragnarok | 0.0285 | 0.0636 | 22% |

## Reading

- **A2's width does limit overall accuracy.** Doubling it to 16 channels cut
  full ESR by 34% and halved both the active median and the quieter-note error.
  32 channels gained a further 13%, so the returns diminish.
- **Width barely helps loud notes.** Loud-note ESR fell only from 0.065 to
  0.051 with 15 times the parameters, and the active p95 stays about 7 times
  its limit even at width 32. Together with the earlier results (memory length,
  amplitude-weighted loss and loud-input crops all left it unchanged), the
  loud-note error is explained by neither capacity, memory, nor exposure.
- **It is not cabinet-filtered fizz.** About 80% of the loud-note error energy
  lies below 5 kHz. Through a cabinet the ESR rises (to 0.064 for Ragnarok),
  because the cab removes high-frequency reference energy while the error sits
  in the mids it passes. The remaining error is in the audible band.
- Wider models generalize less well: at width 32, training loss is 0.0095 and
  validation ESR 0.0147. More, or more varied, training data would matter for
  any larger model.
