# 1.3 validation definition and evidence recovery

## Finding and scope

The 86 historical A IDs are **still BLOCKED**. Their original numbered
assertions, original process ownership and per-ID evidence producers were not
found in the inspected history. This is not a claim that they need 86 manual
tests, nor permission to replace them with newly invented assertions.

The audit inspected 393 reachable Git commits across the saved refs, original
policy/numbering commits, PR #23/#35/#39 descriptions and discussions, matching
issue searches and a targeted prior-context search. Reproducible search commands,
ref snapshot, match classifications and policy comparisons are in
[the historical audit](evidence/validation-recovery-20261010/history-audit.json).
Deleted/unreachable commits and unavailable original external design documents
are outside that search scope.

| Reviewed source | Profile | Required | Hard gates | Owned automatic | Undefined A | External |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| #23 `4d3d2ec944f8a2699451943a9da66102c2f21ea2` | beta_1_2 | 128 | 63 | 21 | 86 | 21 |
| #35 `b7e4f18efa295f0babf2278ca63117fbccb54143` | beta_1_3 | 128 | 63 | 21 | 86 | 21 |
| #39 `aaa1330dd5cf7d51faf1c45a7cef633b9b118458` | beta_1_3 | 128 | 63 | 21 | 86 | 21 |

The required ID sequences and applicable hard-gate memberships match across
all three. #35 changes product/profile scope to 1.3 and extends E1 coverage for
the expanded product. #39 selectively integrates #23's concrete producers and
records publication authorization; it does not add the missing numbered
definitions. `release_approved` remains false.

`c499d4cfcda82d87b57023757536e79287232d7e` introduced stage names.
`11ee2419faeecab5bce919965051deef2391a88f` added numeric IDs and hard-gate
membership in a policy-only commit. The only other matching A-ID changes were
generic producer fixtures, generated unexecuted reports and #23's missing-definition
audit. A fixture named `A2.01` is not the original Signature assertion.

## Recovered purpose and unresolved numbering

`Validation/legacy-check-recovery.json` preserves stage purposes, historical
commits, relevant current files, proposed investigation processes and explicit
gaps. Original owners/producers/assertions remain null where unconfirmed.
The generated JSON/CSV inventory has one row for **every one of the 128 IDs**,
including all 86 unresolved A checks. Stage-level research leads never become
ordinal assignments.

| IDs | Count | Recovered stage evidence / remaining requirement |
| --- | ---: | --- |
| A1.01–06 | 6 | Installer/version/repair/removal receipts exist; recover six exact predicates. Existing A13 ownership cannot be reassigned by assertion count. |
| A2.01–08 | 8 | `4fd060e`/`211cce7`: Signature Matrix/PRE/LOW, metadata, crossover and amp structure. Eight-number mapping absent. |
| A3.01–09 | 9 | APVTS/binary/A-B and integrated state tests exist; recover original Bass/Guitar and snapshot scope per ID. |
| A4.01–05 | 5 | Factory compatibility tests exist; recover original five predicates and baseline. |
| A5.01–08 | 8 | `ca0c976`: Hartke metadata fixture, not proof of original ZIP import. Need original numbered definition/private fixture and scope reconciliation. |
| A6.01–08 | 8 | Same limitation for Marshall metadata fixture. |
| A7.01–07 | 7 | `0259cb7`: audio/pack hashes and sidecar stamping; original seven predicates/provenance remain absent. |
| A8.01–04 | 4 | Historical missing/hash-mismatched private Signature IR fallback; four predicates absent and 1.3 factory path changed. |
| A9.01–09 | 9 | `156fcf2`/`ec6a13b`: raw delta, common bias, centered delta, RMSE, pair order/gaps and weighted score. Equations recovered, numbering not recovered. |
| A10.01–06 | 6 | H1–H4, >=3/4 secondary, >=85 score and >=0.70 critical gates survive. Six-number mapping absent. |
| A11.01–09 | 9 | Benchmark input/output schema and numeric measurements survive. Nine exact predicates and provenance requirements absent. |
| A14.01–07 | 7 | Catalog/manual checks exist; seven content/language/version predicates absent. Presence is not complete translation review. |

### Deliberate 1.3 IR scope change

`4c7688d78869e113a041cb7316f4d23d5ea1a02c` removes private-pack automatic
resolution/catalog routes and gives factory presets modeled CAB recipes.
Restoring the old Hartke/Marshall import metadata and Filters-only factory
fallback wholesale would conflict with that reviewed 1.3 work. Current manual
user import/hash tests and public two-asset distribution allowlisting remain
relevant supporting evidence, but do not retroactively define A5–A8. No private
audio, old import route, waiver or N/A result is introduced by this recovery.

## Code and producer changes

- `test_signature_calculator_contracts.py`: 13 synthetic calculator tests,
  derived from the surviving frozen policy/evaluator. They check arithmetic,
  ties, minimum directional gaps, each hard gate's boundary, secondary count,
  score/critical boundaries, invalid DSP and JSON/numeric inputs. The three
  original cases in `test_signature_benchmark.py` are retained unchanged.
- A real input bug was reproduced: Python treats JSON booleans as integers.
  `finite()` now rejects booleans as measurements. Numeric formulas, thresholds,
  reference values and audio/DSP behavior are unchanged. The boundary tests use
  copies of synthetic policies; the production policy file is unchanged.
- `produce_recovered_evidence.py`: executes both suites on clean checked-out
  HEAD, records full commit/tree SHA, input hashes, exact commands/return codes,
  per-case outcomes, logs and output SHA-256. Missing/skipped/duplicate cases,
  zero-exit without assertions, dirty/stale source, CI identity mismatch and
  existing output directories fail closed. Local execution is labelled local;
  Actions receipts also bind run ID and attempt.
- The new component receipt has `policy_claims: []`. Its PASS means calculator
  contract execution only. It cannot provide a single A/B/E2/I2 release PASS.
- `validation_inventory.py` writes JSON/CSV alongside the real consolidated
  gate, using that gate's actual statuses and revision. Separately generated
  local inventory is explicitly an unexecuted BLOCKED baseline.
- `build.yml` executes and uploads `Chimera-recovered-component-evidence` in
  validation-contract. It is deliberately outside the four release-evidence
  bundle names and is not consumed as acceptance by the publisher.

## Automatic / external / manual boundary

| Class | Count | Execution requirement |
| --- | ---: | --- |
| Existing owned CI contracts | 21 | Current source/run/attempt platform receipts; all owning platforms must pass. |
| Original numbered definition missing | 86 | Recover the original per-ID assertion/owner/evidence contract before mapping. |
| Automation possible after external inputs | 13 | B1–B6, B10 and E2: real DI/reference/capture inputs, approved protocol and bound outputs; some also need reviewer/listening acceptance. |
| Manual or controlled target-environment acceptance | 7 | B8, B11–B13, I2.GATE_DI, I2.UI_AUDIO_TIMING, I2.LIVE_REMOVE_DAW. |
| Composite release acceptance | 1 | B14: complete candidate review, not just assembly. |

The 21 external checks are not one listening checkbox. Required materials per
ID are in the registry and generated inventory. Their process labels identify
the required role; they do not invent a named original assignee or authorize
synthetic CI to close host/hardware acceptance.

## Reproduction and evidence interpretation

On a clean committed checkout:

```sh
python Tools/test_validation_recovery.py
python Tools/test_release_consolidator.py
python Tools/produce_recovered_evidence.py --commit "$(git rev-parse HEAD)" --output build/recovered-evidence
```

Use a fresh output directory per run. The final `recovery-evidence.json` contains
the actual tested SHA; the SHA in the historical audit is only the researched
base. Its logs and `cases.json` are execution evidence. The registry and
historical comparison are definition research, not a test result.

Complete producer fixtures still yield **21 PASS / 107 BLOCKED**, 21/63 hard
gates PASS. That is a regression fixture expectation, not the new source's
product CI result. Local calculator/recovery tests do not certify native builds,
Windows installers, CAB timing, DAWs or real instruments. The publisher still
rejects a production-policy BLOCKED verdict.

## Conditions still required for 1.3 approval

1. Recover the 86 original numbered definitions or obtain an explicit,
   separately reviewed specification decision where the original is unavailable
   or conflicts with the 1.3 IR design. This patch makes no such policy decision.
2. Bind the subsequently verified producers to those exact definitions and run
   them on the final source; preserve all thresholds and failed evidence.
3. Supply real guitar/bass DI, reference/capture provenance and sound evidence
   for the 13 input-dependent checks; run measurement/rendering contracts.
4. Execute listening, real Windows/Studio One host lifecycle, actual 30-minute
   soak and controlled workstation timing; preserve binary/input/device/session
   hashes and logs. Complete the B14 candidate review.
5. The final source must also pass Product 3OS, Windows Candidate and CAB 3OS,
   packaging/install/security and five-package provenance. Every source change
   needs its own receipts; older successful CAB/CI runs cannot cover this patch.
6. The unchanged beta_1_3 gate must recompute PASS and the release scope must
   record actual source-specific approval. Publication authorization alone is
   insufficient. Keep the two existing transpose deferrals as-is.

Issue #21's separately documented PASS-enforcing job/dependency work is not
declared complete by this definition recovery. Preparation may be green while
acceptance is BLOCKED; current publisher enforcement still rejects that verdict.
