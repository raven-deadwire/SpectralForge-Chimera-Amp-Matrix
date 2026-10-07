# Exact-source Beta 1.2 release evidence

`build.yml` now produces the publisher's `Chimera-A-stage-release-gate`
artifact **after** the three platform builds and release candidate assembly.
The artifact name is retained for publisher compatibility; its intended profile
is `beta_1_2`, including the applicable A, B, E and I acceptance stages.
A successful build does not imply release eligibility.

## Evidence ownership and transport

| Evidence | Existing producer / raw output | Final ownership |
|---|---|---|
| CTest inventory and results | `ctest --show-only=json-v1`, `ctest --output-junit`; `build/ctest-inventory.json`, `build/ctest-results.xml` | A12.01–07, I1.PITCH/GATE/STATE_GAIN_GR require all three platforms; I1.LIVE_REMOVE requires Windows |
| Full matrix and DSP log | CTest plus `build/Testing/Temporary/LastTest.log` | A13.01 Windows, A13.02 Linux, A13.03 macOS; every configured test must appear and pass |
| Platform packaging | `package_release.py`; macOS/Linux verification text | Required alongside the corresponding matrix receipt; does not attest real hardware or DAW acceptance |
| Windows install/repair/uninstall | `Verify-WindowsInstaller.ps1`; `InstallerVerification.json` and `.txt` | A13.04; successful step and report, exact source/run/version |
| MSIX deployment/activation | `Verify-WindowsMSIX.ps1`; `MSIXVerification.txt` | A13.05; successful step and completion marker |
| Defender inspection | `Scan-WindowsArtifacts.ps1`; `WindowsSecurity.txt`, `FileInventory.json`, `DefenderStatus.json` | A13.06; successful step, source identity, completed scan and metadata |
| Candidate assembly | `assemble-candidate`; `candidate-source.json`, `update-beta.json`, `SHA256SUMS.txt` | A13.07; exact source/run/version, five package identities and successful assembly/source/upload steps |
| E670FE implementation | Same hashed CTest inventory/JUnit/full log; reviewed `ctest-contract:E1.*` sub-suite predicates | E1.DSP requires all three platforms; E1.STATE_UI requires Windows; E1.LEGACY requires portable parameter contract on all platforms plus Windows native-state/legacy UI coverage |
| Existing check reports and signature snapshots | `produce_ctest_validation.py`; integration-validation artifacts | Kept for diagnostics. Seeded per-platform gate summaries are never merged or promoted |

`produce_release_evidence.py` preserves raw inputs in four separate artifacts:
`Chimera-release-evidence-{windows,linux,macos,assembly}`. Each manifest binds the
full checked-out source SHA, workflow run ID, run attempt, policy version and
profile, records actual step **outcomes**, and hashes every included raw file.
The checked-out SHA is used on PRs; `GITHUB_SHA` can be a synthetic merge SHA.
This also corrects the Defender report's previous merge-SHA label.

`consolidate_release_gate.py` downloads these artifacts from the same run without
merging their files. It validates identities/hashes and regenerates automatic
check assertions from the raw results using `ci-check-map.json` ownership.
A passing Windows result cannot overwrite a Linux/macOS failure. Duplicate,
missing, stale, malformed or conflicting inputs remain BLOCKED. Invalid input
also fails the consolidation job, after writing its BLOCKED artifact. A valid
BLOCKED verdict due to missing acceptance evidence is a successful computation.

The initial baseline is now `Chimera-validation-seed`; only the final consolidator
uploads `Chimera-A-stage-release-gate`. Run attempts must match: after a partial
rerun that leaves older platform receipts, rerun **all jobs** for fresh evidence.
There is no fallback to a previous run, previous attempt, or candidate workflow.
The separate `update-candidate.yml` remains an additional publisher prerequisite.

## What is still BLOCKED

The 18 A12/A13/I1 checks and three explicit E1 implementation/compatibility
contracts are claimed here. Other A/E checks remain unexecuted until their
original per-ID assertions and concrete producers exist.
No general CTest success is treated as same-DI calibration, musical listening,
commercial DAW acceptance, UI/audio timing acceptance, or human approval.
The B stages, E2 and I2 retain their existing policy requirements. The two
previously deferred transpose goals and all other release policy gates are
unchanged. Replaying run `37429791345` with these ownership additions yields
**21 PASS / 107 BLOCKED**, with **21 / 63 hard gates PASS** (delta +3).
This is a computed release verdict, not a release approval.

The detailed [110-check audit](RELEASE_GATE_OWNERSHIP_AUDIT_20261006.md)
distinguishes 86 undefined numbered A assertions from 21 checks requiring
external inputs/reference/acceptance. They are not all manual tests. Existing
installer and synthetic IR receipts cannot be arbitrarily assigned to unnamed
A subchecks. B/E2/I2 remain blocked in their entirety.

The E1 producer requires both a complete passing configured CTest matrix and
the relevant passing case, then checks every declared marker inside that case's
unique, completed `LastTest.log` section. JUnit output in the audited run was
truncated at 1,024 bytes, so JUnit text alone is insufficient. Global log marker
searches, duplicate sections, missing markers or contradictory failures do not
pass. The actual legacy-parameter manifest hash may differ on ARM/x86: the
existing C++ harness checks its frozen fixture hash and every semantic field
with its pre-existing tolerance; the producer validates that completed contract,
not byte equality between platform-specific floating-point string dumps.

No workflow transport change is necessary: all inputs are already hashed in
the four bundles. The per-platform diagnostic `produce_ctest_validation.py`
does not claim the new composite contracts; the final consolidator owns them.
Historical replay is an audit estimate only. The patch's new source SHA still
requires a fresh exact-source CI run before its publication gate can be used.

Future acceptance producers must supply source-bound raw evidence and explicit
check ownership before replacing these baseline reports. This change deliberately
adds no workflow input that accepts an arbitrary caller-supplied PASS report.

## Publisher contract

`publish_beta12.py` requires a successful final consolidation job and one
nonexpired gate artifact from the selected exact-source build run. It verifies
the downloaded artifact SHA-256, source/run/attempt/profile/policy identity, the
complete required check inventory, and recomputes the gate using the publishing
source's policy. Missing assertions or exit codes cannot become PASS. Summary
fields must equal the recomputed result, which must be PASS/ready with zero
blocked checks/hard gates and no blockers, before candidate download or any
publication mutation. Candidate provenance independently uses the same source
SHA and build run ID.

## Regression coverage

Run:

```sh
python Tools/test_release_consolidator.py
python Tools/test_publish_beta12.py
python Tools/test_release_gate.py
python Tools/test_ctest_validation.py
python Tools/test_release_scope.py
python Tools/test_validation_producer.py
python Tools/check_ci_validation_map.py
```

The integration fixtures cover successful automatic receipts, all-platform AND
semantics, Windows-only ownership, missing artifacts, stale SHA/run/attempt,
wrong profile/policy, hash mismatch, duplicate inventory/JSON/bundles, malformed
JSON/XML, skipped/failed execution, absent package/assembly receipts, candidate
source mismatch and forged PASS summaries. A test-only policy fixture exercises
a full producer-to-publisher PASS path; it never alters production acceptance
requirements or represents real listening evidence.

E1 tests also remove each required sub-suite marker, change platform runtime
digests, remove/fail/skip required cases, duplicate or move log sections, inject
contradictory failures and omit completion markers. The positive log fixture is
a fixed excerpt from the audited run, independent of the ownership predicates.
The production-policy regression fixes the maximum current result at 21/107 and
requires every unowned check to remain unexecuted/BLOCKED.
