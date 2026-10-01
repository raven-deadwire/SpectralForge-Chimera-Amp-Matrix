# Chimera validation status and release gates

Chimera validation uses three check states:

- **PASS** — the check ran against the current commit and active policy and its assertion passed.
- **BLOCKED** — the requirement is not proven: dependency, execution, environment, input, artifact, stale evidence, regression or test failure.
- **NOT_APPLICABLE** — the check is outside the active scope and has an approved waiver. Missing tools or an unavailable DAW are BLOCKED, not N/A.

Hard gates accept **PASS only**. A hard gate can never be waived to N/A.

## Files

- `Validation/status-schema.json` — check report contract.
- `Validation/release-policy.json` — stages and pull_request / windows_rc / full_release profiles.
- `Validation/waivers.json` — explicit scoped N/A approvals. Empty by default.
- `Tools/evaluate_release_gate.py` — status/dependency/stage/release evaluator.
- `Tools/test_release_gate.py` — regression tests preventing N/A and stale-evidence bypass.
- `Validation/signature-benchmark-policy.json` — frozen Deadwire master-relative metrics and gates.
- `Tools/evaluate_signature_benchmark.py` — Signature PASS/REVISE/INVALID evaluator.

Evidence is current only when its source commit and policy version match the evaluator inputs. Fixture-sensitive checks additionally pin the fixture SHA-256. A previous green result therefore cannot silently certify a newer commit or changed threshold.

The release evaluator exits 0 for PASS, 1 for BLOCKED, and 2 when the validation infrastructure or input schema is invalid. The top-level release verdict is never NOT_APPLICABLE.

## Deadwire Signature IR integrity

Reference-catalog targets such as `DYN 421.wav` and Raven G12-100 captures retain catalog SHA verification. The two personal-pack Signature defaults additionally pin the exact WAV bytes:

- Wild Hunt MID: `Hartke HyDrive 410 _ SM57.wav` — SHA-256 `caa3be009191b2cd6c2ad1a711fe0f7eddd74c9115e991ab2032159405699d0b`
- Azhi Dahaka HIGH: `Marshall G12 1 SM57 3.wav` — SHA-256 `d55582bb1f6ed27e0ee03e4ea71006968c2eb36cba93b55338f86b45d42c5cb1`

Approved ZIP import writes `audio_sha256`, `source_pack_sha256` and `approved_pack` into each local sidecar. Signature recall requires the pinned filename and WAV hash; a same-name replacement is not silently accepted. Missing or mismatched targets fall back to Filters only.

## Signature benchmark verdict

The benchmark compares the three renders as a relative shape, not as exact source-track reconstruction. It computes raw deltas, per-metric common bias, centered deltas, shape RMSE, pairwise order and gap errors, then applies the frozen weighted score. All four hard gates must pass, at least three of four secondary gates must pass, overall score must be at least 85, and every critical metric score must be at least 0.70. DSP-invalid renders return INVALID; otherwise a failed target returns REVISE rather than PASS.
