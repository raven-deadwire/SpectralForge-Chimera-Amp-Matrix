# Náströnd A2 memory diagnostic, 2026-10-24

Question: does the A2 receptive field (6,347 samples, 132 ms) limit accuracy?
Some channels' supply-sag recovery is longer than that: Surtr 160 ms,
Fimbulvetr 300 ms. **Answer at this training budget: no.** A five-times longer
memory did not improve accuracy. Neither model is A2; both are diagnostic only
and are not to be shipped.

## Design

Two arms, identical except for three layers appended to the A2 stack (kernel 6):

| Arm | Extra dilations | Receptive field | Purpose |
|---|---|---:|---|
| short | 1, 3, 7 | 6,402 (133 ms) | Capacity control: same layer count, about the same memory |
| long | 571, 1361, 3229 | 32,152 (670 ms) | Covers the longest sag time constant |

Both arms use the same 480 s capture (main `4ef21c16`) and train from scratch
with seed 6405. Each runs 5,000 steps, batch 2, 4,096-frame crops, LR 0.004
with a 2,000-step half-life, the legacy recipe, 10% tail crops and centering,
with level-gated selection (`3f08fe0`).

## Results (validation, profile v2)

| Channel | Full ESR short → long | Active median short → long | Quiet RMS dBFS short → long |
|---|---|---|---|
| Fenrir | 0.0665 → 0.0667 | 0.046 → 0.045 | −65.4 → −61.0 |
| Surtr | 0.0839 → 0.0899 | 0.077 → 0.083 | −77.5 → −68.0 |
| Nidhoggr | 0.0811 → 0.0909 | 0.064 → 0.073 | −74.7 → −65.9 |
| Fimbulvetr | 0.0409 → 0.0568 | 0.030 → 0.044 | −77.5 → −63.6 |
| Ragnarok | 0.1069 → 0.0916 | 0.104 → 0.088 | −75.9 → −68.5 |
| **Mean** | **0.0759 → 0.0792** | | |

The final training loss was 0.0594 for the short arm and 0.0539 for the long
arm. The long arm took 3.6 times as long: 120 minutes against 34.

## Reading

- **Longer memory did not reduce error.** Mean full ESR went from 0.076 to
  0.079. Fimbulvetr, the channel with the longest sag (300 ms), got worse:
  0.041 → 0.057. Only Ragnarok improved, and only modestly.
- The long arm fit its training data slightly better (0.054 against 0.059) but
  did not generalize better. Its quiet residuals also worsened, by 4–14 dB.
- **Both arms underfit.** Their training loss is similar to their validation
  ESR. At this budget the limit is the amount of training, not memory or model
  size. A2 models see about 14 minutes of audio, roughly 1.8 passes over the
  480 s capture.
- This doesn't prove memory never matters. A larger model may need more
  training to use its extra memory, so the test should be repeated if a
  long-budget A2 run plateaus.

## Next

Train the A2 model itself for an order of magnitude longer, with a slower LR
decay. For example, 50,000 steps with a 20,000-step half-life, about 5–6
hours on this 4-core CPU. Keep the level-gated selection.
