# Náströnd A2 refinement, 2026-10-11

PR #22. Numerical accuracy remains FAIL; release approval remains false.

## Completed experiment

Both recipes received 1,000 additional updates from the same archived legacy checkpoint, batch 2, 4,096 output frames, two threads, LR 0.001, 10% terminal-decay crops and export-consistent centering. Optimizers and crop RNGs were reset. Recipe-specific objectives and LR schedules remain explicit. This bundled refinement does not isolate the contribution of each change.

Validation mean ESR: legacy 0.100126; official-a2 0.101804. Both 0/5 fully passing. Legacy was selected on validation before generating the new seed 26101122. No test-driven weight, level, EQ or alignment adjustment followed.

| Channel | Fresh baseline ESR | Fresh candidate ESR | Reduction | Level | Quiet tail |
|---|---:|---:|---:|---|---|
| Fenrir | 0.095057 | 0.085300 | 10.26% | FAIL | PASS |
| Surtr | 0.105351 | 0.091362 | 13.28% | PASS | FAIL |
| Nidhoggr | 0.108063 | 0.099453 | 7.97% | PASS | FAIL |
| Fimbulvetr | 0.051078 | 0.043812 | 14.23% | PASS | FAIL |
| Ragnarok | 0.137977 | 0.120449 | 12.70% | FAIL | PASS |

Mean fresh ESR 0.099505 → 0.088075, an error reduction of 11.49%. This is not a tone-similarity percentage. All five still fail full ESR <0.01 and active median/p95/worst 0.005/0.01/0.02.

## Hard blocker under the unchanged quiet policy

Fimbulvetr has a quiet window starting at sample 1,132,747 (23.5988958 s). Of its 4,096 samples, 2,497 have an entirely zero 6,347-sample input receptive field. Any bare A2 must output the same constant there. Even allowing an independently optimal constant for this window and perfect prediction everywhere else, residual RMS is bounded below by **−68.033258 dBFS**, above the required **−70 dBFS**. This is a proof for this captured input and frozen scale, not an estimate from one poorly trained network.

All five real target-engine outputs have exactly zero peak-to-peak variation on the zero-history subset, corroborating the finite-memory premise. The current five-channel aggregate cannot pass this test under the bare A2 contract merely by training longer. Thresholds, quiet classification and native DSP were not changed. Longer memory or an explicitly validated NAM-plus-recursive-filter chain needs separate architecture/target support work. The proof does not establish a floor for active passages or for those alternative systems.

Surtr and Nidhoggr also fail quiet limits on this seed, but this bound does not prove their gates impossible. Their almost unchanged tail measurements must not be called improved.

Ragnarok has a material peak regression: fresh-test peak error +2.053 dB → +2.361 dB; validation +3.018 dB → +3.732 dB. Its fresh RMS error is −0.650 dB. Lower ESR alone does not qualify this candidate.

## Passing and blocked checks

- 50 local test executions PASS, including numerical bound arithmetic and zero-history boundary tests. Some suites are imported by the CI entry point as well as discovered separately.
- Every native validation sample reproduced bit-for-bit for all five channels. Channel controls, eight frozen DSP headers, architecture and quality policy unchanged.
- Both five-model arms: official NAM export roundtrip maximum error 0.
- Selected models: actual TONE3000 A2-Full import PASS; engine parity −138.59 to −149.61 dBFS against the −80 dBFS limit; preset restore maximum error 0 for all five.
- Steady silence −153.63 to −170.02 dBFS, below the unchanged −80 dBFS limit. This does not imply decaying tails pass.
- [Core implementation smoke CI](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38112794082) SUCCESS at `0dfaafc182c96d58e61bdd66b44b23c850811d4b`; this is compatibility, not fidelity approval. Later audit/guard/report changes have local regression evidence and require their own head-specific CI status.
- Numerical fidelity FAIL, real-instrument DI listening BLOCKED, channel-role/macro listening BLOCKED, actual TONE3000 GUI and DAW reopen/close/exit BLOCKED. PR remains Draft; no merge or release.

## Reproduce and evidence

See `../README.md` for paired refinement and fresh-test commands. Then run:

```sh
python Tools/Nam/audit_quiet_memory.py --evaluation /path/to/fresh-test \
  --models /path/to/refinement/legacy --out /path/to/memory-bound.json
```

`refinement-20261011.json` contains the unchanged policy, per-channel full/active/quiet/level metrics, model hashes, test identity and the memory witness. The accompanying research evidence archive retains checkpoints, both recipe outputs, split arrays and the fresh-test audio arrays. Raw models/presets use native capture units and disabled EQ/Normalize; they are diagnostic artifacts, not approved production downloads.
