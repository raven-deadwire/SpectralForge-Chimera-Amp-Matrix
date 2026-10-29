# Náströnd A2 Ragnarok peak penalty weight, 2026-10-27

A rerun of [peak-penalty-20261027](peak-penalty-20261027.md) with the same
settings and starting models, but a peak weight of 1.5 for Ragnarok only (5
before) and none for Fimbulvetr. The question was whether a weaker penalty
keeps the peak inside ±1 dB at less cost in ESR.

| Ragnarok | Start | Weight 5, step 18,500 | Weight 1.5, step 19,000 |
|---|---|---|---|
| Validation full ESR | 0.0197 | 0.0217 | **0.0214** |
| Validation level RMS / peak dB | | −0.10 / −0.00 | −0.13 / +0.28 |
| Test full ESR | 0.0187 | **0.0190** | 0.0192 |
| Test level RMS / peak dB | −0.05 / +1.52 | −0.13 / −0.22 | −0.15 / +0.00 |
| Test active median / p95 / worst | 0.0138 / 0.079 / 0.270 | 0.0145 / 0.074 / 0.225 | 0.0148 / 0.074 / 0.253 |

## Reading

- **The weight barely matters.** At 1.5 the peak sat at +0.1 to +0.6 dB
  rather than 0 to +0.3 dB, but the ESR cost was nearly the same. Validation
  favors weight 1.5 by 1.5% and test favors weight 5 by 1.5%; both are within
  run-to-run noise. Most of the ESR cost comes from bringing the attack peaks
  down at all, not from the penalty's strength.
- Weight 1.5 is adopted because selection uses validation only.
- **Fimbulvetr without a penalty overshoots again.** Its later steps reached a
  validation ESR of 0.0065 but with a peak of +1.11 to +1.16 dB. Under weight
  2 in the earlier run its peak stayed at +0.2 to +0.4 dB but its ESR did not
  beat the start. Its starting model is kept.
- All five channels still FAIL the profile. No release approval.
