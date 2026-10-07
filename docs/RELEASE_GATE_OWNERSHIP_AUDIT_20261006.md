# PR #23: audit of 110 BLOCKED checks

Source `47c7bf7412baa41de66f5b4ae41bafc410bde5ca`, build run
[37429791345](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37429791345),
attempt 1; gate artifact `11398430615`.
The downloaded ZIP SHA-256 is
`51d5368622a13d8537444b13a81030162e962012371c11b6eee3844e74235f22`,
matching GitHub's artifact digest. All four inner bundle identities/hashes were
validated by the production consolidator. No newly generated DSP or acceptance
evidence was added to this replay.

| Measure | Published CI artifact | Historical replay with patch | Delta |
|---|---:|---:|---:|
| Required checks PASS | 18/128 | 21/128 | +3 |
| Required checks BLOCKED | 110 | 107 | -3 |
| Hard gates PASS | 18/63 | 21/63 | +3 |
| Required stages PASS | 3/30 | 4/30 | +1 (E1) |
| Verdict / ready | BLOCKED / false | BLOCKED / false | unchanged |

The policy file, thresholds, waivers and deferred-check membership are unchanged.
This replay estimates the ownership delta on historical evidence. It does not
certify the patch's new SHA or replace the original GitHub artifact. Fresh CI
must collect new evidence for that SHA.

## Classification

| Classification | Checks | Treatment |
|---|---:|---|
| Existing raw evidence can satisfy the documented assertion | 3 | Connect E1.DSP / E1.STATE_UI / E1.LEGACY only |
| Numbered A assertion definition unavailable | 86 | Keep BLOCKED; retain useful receipts, recover original definitions before assigning IDs |
| External input, reference or acceptance required | 21 | Keep all B, E2 and I2 checks BLOCKED |

The complete one-row-per-check inventory is
[blocked-audit.csv](evidence/pr23-ownership-audit-20261006/blocked-audit.csv).
It lists all 110 original BLOCKED IDs, hard-gate membership, existing relevant
evidence, missing requirement and next action. The machine-readable counts,
changed IDs, source/policy digest and four bundle hashes are in
[delta.json](evidence/pr23-ownership-audit-20261006/delta.json).

## Safe connections

| ID | Required evidence | Assertion scope |
|---|---|---|
| E1.DSP | All three platforms: configured and passing `ChimeraAmpNativeTests`; scoped full-log markers | Model 23 gain tests; all 393 controls; 168 rate/channel routes; channel isolation; 100 integrated oversampling paths; no runtime allocation |
| E1.STATE_UI | Windows: configured/passing `ChimeraUITests`; native-state and native-panel completion markers | `NativeStateTests.h` executes six contexts × five E670FE channels, 32 controls, inactive return, binary recall and A/B; `NativeUITests.h` visits all serialized panels/channels and callbacks |
| E1.LEGACY | All three platforms: `ChimeraGateProcessorTests` parameter contract; additionally Windows `ChimeraUITests` native-state and legacy-selection markers | Frozen released parameter structure, approved existing defaults/tolerances, append-only E670FE ordering, Ironball recall and A/B, legacy selections/state migration |

The requirements come from `docs/E670FE_IMPLEMENTATION.md`,
`Tests/AmpNativeTests.cpp`, `Tests/NativeStateTests.h`, `Tests/NativeUITests.h`,
`Tests/AmpSelectionStateTests.h` and `Tests/GateProcessorTests.cpp`. Successful
completion markers follow the relevant assertions in those exact-source tests.
E1 is implementation/compatibility evidence; it makes no hardware emulation,
T.D. EQ, calibrated capture, musical quality or commercial host claim.

Use the already-bound `LastTest.log`: CTest truncated JUnit `system-out` at
1,024 bytes in this run. Match markers only inside the named test's one complete
section, with both its JUnit status and log completion passing. Missing or
duplicated sections, borrowed markers, nonexecution or failures remain blocked.

## Why other available evidence does not automatically clear A checks

The current policy and original commits `c499d4c` / `11ee241` contain stage
names, required numeric IDs and hard-gate membership, but no assertion text for
the 86 unowned A IDs. Current repository documentation and retrieved prior
design records did not recover the numbered definitions. Inventing an ordinal
assignment from a stage title would change the effective acceptance contract.

| Stage | Count | Existing evidence and remaining gap |
|---|---:|---|
| A1 | 6 | Installer version/source/run, install/repair/uninstall, MSIX, Defender, package and assembly receipts exist and already own A13.04–07. None defines which predicate belongs to A1.01–06. |
| A2 | 8 | Structural signature assertions exist; numbered definition and exact coverage mapping do not. |
| A3 | 9 | Actual Processor/APVTS recall and synthetic snapshots exist; unlabelled Bass/Guitar scope cannot be inferred per ID. |
| A4 | 5 | Factory host-ID/range, recall and compatibility tests execute; numeric assertions are undefined. |
| A5 / A6 | 8 + 8 | Metadata, hash rejection and companion fixtures execute. Original approved Hartke/Marshall ZIP import is not established by these synthetic/companion fixtures. |
| A7 | 7 | Integrity-field/hash rejection tests execute. Full original-pack provenance and the seven numbered predicates are not established. |
| A8 | 4 | Windows tests assert missing/hash-mismatched Signature IR falls back to Filters only. Four original numbered cases are still undefined. |
| A9 / A10 / A11 | 9 + 6 + 9 | Benchmark code and synthetic evaluator tests exist. Per-ID assertions and complete bound calculator/actual benchmark receipts are missing; evaluator correctness is separate from real-DI acceptance. |
| A14 | 7 | Embedded manual presence and catalog checking do not establish complete content/language synchronization. Seven numbered assertions remain undefined. |

This is not a claim that all 86 need human testing. Many are likely automatable
once their exact existing definitions and required raw receipts are restored.
No projected PASS count is assigned to those undefined contracts.

## External evidence boundary

All 15 B checks, three E2 checks and three I2 checks stay BLOCKED.

- B1–B6 and B10 can use automated hashing/rendering/measurement/evaluation after
  the required real-DI fixtures and bound audio evidence exist. Synthetic
  snapshots cannot provide those inputs.
- B8 needs listening acceptance; B11/B12 need commercial host/Studio One 8
  acceptance; B13 needs the actual 30-minute soak. B14 is complete candidate
  acceptance, not a duplicate of the A13.07 assembly receipt.
- E2.TD_EQ, E2.CALIBRATION and E2.SAME_DI require the missing reference,
  calibration and sound evidence. E1 completion does not close them.
- I2 requires real-input Gate acceptance, controlled workstation UI/audio timing
  and actual commercial-host live removal. A synthetic harness is insufficient.

## Patch and regression validation

Only ownership, the scoped raw-evidence predicate, map validation, tests and this
audit change. Existing four-bundle transport and publisher remain unchanged.
No DSP, released parameter, factory preset, policy, waiver or threshold changes.

The suite now has 24 consolidator tests, including removal of every E1 marker,
missing/failed/skipped platform evidence, stale identity, tampering, duplicate
sections, wrong case names, failed completion, empty contracts and protected
checks staying unexecuted. The ten Beta 1.2 publisher and six Beta 1.1.2
compatibility tests also pass, together with evaluator, scope, CTest producer,
validation producer and ownership-map checks. The publisher still rejects the
production-policy 21/107 result.

`Tools/fixtures/e1-ctest.log` is a fixed, 12,413-byte excerpt of the Windows
`LastTest.log` in the above artifact, containing the three relevant test
sections. Tests do not synthesize success markers from the ownership map.
Production evidence never comes from this fixture.

To reproduce the historical delta after downloading and digest-verifying the
original artifact, extract its four `evidence/<kind>` directories into a fresh
input directory as `Chimera-release-evidence-<kind>`. Invoke the Python
`consolidate_release_gate.consolidate` function with that input, a fresh output,
`release_evidence.identity('47c7bf7412baa41de66f5b4ae41bafc410bde5ca',
'37429791345', '1')`, and profile `beta_1_2`. This direct function call is for
historical audit only; the production CLI continues to require checked-out HEAD
to match the evidence source exactly.
