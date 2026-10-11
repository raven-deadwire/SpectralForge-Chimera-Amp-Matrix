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

## Factory CAB and user IR integrity (1.3)

All 48 factory presets use the authored modeled CAB recipes in [FACTORY_CAB_VOICING_1_3.md](FACTORY_CAB_VOICING_1_3.md), including the Deadwire Signatures. Authored dry LOW lanes retain CAB bypass. Factory recall applies these settings without searching for external captures or requiring a private filename or hash.

The public IR loader includes the two attributed factory recordings listed in [IR_DISTRIBUTION.md](IR_DISTRIBUTION.md). The distribution guard checks their exact WAV bytes and attribution against `Validation/ir-distribution-policy.json` before packaging. Development/private reference catalogs and their download prompts are excluded.

Users can select WAV/AIFF files with **OPEN IR** or **ADD FOLDER**. Existing user-file metadata and project-embedded IR audio retain their recall behavior; factory selection does not delete them. Importing audio does not establish redistribution permission or assign private-pack approval. See [EXTERNAL_BASS_IRS.md](EXTERNAL_BASS_IRS.md) for the manual import and sharing contract.

Historical scope: private filename/hash auto-target checks from development source `40d5e8a5ea466eaccc1044ccefce1bd33e6ac222` describe that earlier preset implementation. They do not describe 1.3 factory recall or certify the current release.

## Signature benchmark verdict

The benchmark compares the three renders as a relative shape, not as exact source-track reconstruction. It computes raw deltas, per-metric common bias, centered deltas, shape RMSE, pairwise order and gap errors, then applies the frozen weighted score. All four hard gates must pass, at least three of four secondary gates must pass, overall score must be at least 85, and every critical metric score must be at least 0.70. DSP-invalid renders return INVALID; otherwise a failed target returns REVISE rather than PASS.

## Integrated 1.1.1 policy v4

Policy v4 retains the E670FE and I1 synthetic integration gates
(pitch low-bin regression, combined Gate, full Guitar state/unity/GR, Windows
live removal) plus I2 release-only real-input/host acceptance. For the `beta_1_1_1` profile,
only `I2.PITCH_LIVE` and `I2.PITCH_DI` are deferred to the `transpose_followup`
profile. Gate instrument listening, UI/audio timing and live removal in actual
DAWs remain required hard gates and accept PASS only. Historical v1/v2/v3
artifacts stay historical and cannot certify policy v4.

CTest now emits JUnit and a configured-test inventory. The producer claims only
executed passing cases, rejects missing/skipped/duplicate/unconfigured cases,
and stamps the source HEAD, policy and JUnit SHA. A platform matrix requires
all configured tests. Missing DI/DAW/calibration/installer evidence remains
BLOCKED; successful synthetic CTest cannot satisfy I2, B-host or E2 checks.
