# Náströnd A2 Fenrir passes v3; all five channels pass the v3 numbers, 2026-10-29

Continues [fenrir-retrain-20261029](fenrir-retrain-20261029.md) from its final
Fenrir state (`--warm-start-final`) with the block-normalized loss weight
raised from 0.5 to 1.5, 25,000 steps, LR warmup to 0.0002 and half-life
10,000. Only Fenrir is trained; weight decay is off for the untrained channels
(they kept their weights exactly), and denormals are flushed to zero.

## Fenrir

Three of 50 evaluations passed the quiet limit (steps 2,000, 3,500 and
16,000). Selection, which ranks the quiet limit first, chose step 16,000:

| | Packaged (before) | Step 16,000 (adopted) |
|---|---|---|
| Validation full ESR | 0.0171 | 0.0184 |
| Validation quiet residual RMS / peak | −70.8 / **−53.3** dBFS | −76.3 / **−60.2** dBFS |
| Test full ESR | 0.0184 | 0.0198 |
| Test active median / p95 / worst | 0.0109 / 0.087 / 0.206 | 0.0113 / 0.093 / 0.198 |
| Test level RMS / peak dB | −0.07 / +0.46 | −0.12 / −0.16 |
| v3, validation / test | fail (quiet) / pass | **pass / pass** |

The quiet margin is small (0.2 dB). Passing it cost about 8% in full ESR on
both splits, which stays inside v3's 0.025 for high-gain channels.

## Package

The TONE3000 test package is rebuilt with this Fenrir; the other channels are
unchanged. The same checks pass: A2-Full through the player's gate, preset
restore exact, Normalize off. **Every channel passes the release profile v3
numbers on validation and test.** v2 (reference fidelity) still fails, as
before. The README gains a v3 section. Release still needs the owner's
instrument-DI listening acceptance, which has not been done, so
`release_approved` stays false.

## Speed fixes found during this work

- Single-channel runs read only the trained channel's data (549a451). The
  arrays exceed RAM.
- Denormals are flushed to zero (5f22e23). Weights that the earlier run had
  decayed toward 1e-30 made each step about 4.5 times slower; the fix brought
  it back to about 52 s per 100 steps.
