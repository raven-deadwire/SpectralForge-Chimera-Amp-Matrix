# Issue #97: candidate-bound P0 publication gate

Status, 2026-10-20 KST: **beta.2 BLOCKED**. This change implements an additive
verification path, not feature completion, an expiry decision, a passing native
candidate, or permission to publish. October 21/22/23 remain conditional targets.

## Current source comparison

The live API snapshot is in
`docs/evidence/issue97-gate-20261020/source-status.json`.

| Work | Latest inspected source | Reuse | Still required for the integrated candidate |
| --- | --- | --- | --- |
| #93 | `7f3a838f5bf1a50bc5949fc3e31141a7da231d40` | Calendar, approval/controller, archive and publisher contracts; immutable historical policy | This additive gate, reviewed evidence, actual packages, explicit expiry and public approval |
| #98 | `b7923ea8b7a0f85cfbfafb0e0338238adc09b181` | Countdown/lifecycle implementation, runtime and historical restore test procedures; lifecycle workflow SUCCESS | New source/package policy identity; installed DAW expiration/export/update restore. Separate signal-path workflow FAIL remains |
| #100 | `6ec78c475311827ccfd1949c9ae9ce364f79c0b4` | Research fixtures, render/latency/CPU measurement tools and negative cases | Production integration; original Neural capture; Azhi Dahaka/Low B/-2 same DI and level; +/-1,+/-2 at 48k <=10ms; weak attack maximum 34.60ms remains a failure. Research build workflow FAIL remains |
| #101 | `8ff4dbf59639f54c751232999da6e78055691703` | Common AAX target, bypass/PDC and installer recovery tests; latest AAX development workflow SUCCESS | Final signed install packages, PACE and platform/installed signature checks; retail Pro Tools on Windows, Intel and Apple Silicon; same P0/policy. Separate signal-path workflow FAIL remains |
| #110 | `3a942a125d652a234d0f481fca3af2bdf6897635` | Six spectrum workspaces, independent four-band DSP and existing UI/core/compatibility suites; dedicated dynamics/lifecycle/graphical-EQ workflows SUCCESS | Original FabFilter comparisons, reviewed tolerances, actual candidate DAW/listening checks. Signal-path and CAB-panel workflows FAIL; full build IN_PROGRESS at inspection |

No branch's historical success is evidence for a new source. Procedures and
fixtures are reusable; their PASS verdicts are not. #110's documented global
multiband bypass transition losses remain an unresolved acceptance item. A
successful standalone CAB realtime workflow does not cancel CAB-panel failures.

## Publication path

1. Select one integrated product SHA containing the reviewed feature changes and
   gate code. Do not merge feature Drafts merely to generate a date-driven build.
   Existing feature owners integrate their code; this change edits no DSP, AAX,
   parameter/state, version or beta-policy files.
2. Build/test the exact source. Keep the existing candidate artifact, all five
   native distribution packages and the existing strict gate archive immutable.
   The Windows installer and macOS pkg must actually include the final AAX
   payloads. A development AAX ZIP is not a substitute.
3. Collect 27 required feature/platform reports (run `--inventory` below). Native
   CI reports contain actual measured output, existing test logs/captures, the
   installed executable hashes, package hashes, source/version and run/attempt.
   Adapt existing native test output into reports; do not rerun unrelated DSP
   development or convert an entire green job into a fabricated report.
4. Collect installed-host/operator attestations separately: native DAW and
   representative system reports on four architectures; retail Pro Tools and
   signature reports on Windows x64/macOS Intel/macOS Apple Silicon. Attach raw
   logs, original captures/results, exact host/OS and executable identity. Manual
   results remain reviewed operator attestations, never automatic CI execution.
5. Package the existing evidence into a **separate** `issue97` artifact. This
   avoids pretending that a build's earlier immutable strict gate already
   contained later DAW acceptance. Upload through a repository workflow running
   on that exact product SHA; the manifest records that run and attempt. The
   reviewed approval identifies its artifact ID, digest and exact manifest hash.
6. `weekly_publication.evaluate()` requires both the unchanged strict baseline
   and a recomputed P0 PASS. `publish_release.py` downloads the actual packages
   and independent P0 artifact, validates live API provenance and producer
   artifact bytes, and rechecks before staging and before making a draft public.
   Original policy fingerprints, empty waivers, calendar, source/controller,
   version-specific approval and confirmation remain mandatory.

The new inventory is additive. Legacy gap acceptance is forbidden for successors
of beta.1 even when their old 128 checks pass. The old beta.1 publication contract
is preserved. Missing evidence still yields a failing exit status. No local
CLI command here publishes or writes to GitHub.

## Evidence contract

P0 artifact layout: `issue97/manifest.json`, referenced report JSON and referenced
raw evidence. Each reference is `{ "path": "issue97/...", "sha256": "..." }`.
Paths are not extracted, duplicate JSON/ZIP entries and oversized archives fail.
The P0 archive is capped at 64 MiB, each member at 16 MiB; the existing strict
archive's independent 16 MiB limit is unchanged.

Manifest fields:

- `schema`: `spectralforge.chimera.issue97-evidence`; `schema_version`: 1.
- `version`, `source_sha`, `run_id`, `run_attempt`, `requirements_sha256`.
- `packages`: every actual distributed binary package name -> SHA-256.
- `binaries`: keys `windows-x64`, `macos-x64`, `macos-arm64`, `linux-x64`;
  each lists `platform`, installed `path`, executable `sha256`, `format`,
  containing `package`. Required formats: Standalone/VST3 everywhere, AAX on
  Windows/macOS, AU on macOS. Every report must use the same platform inventory.
- `expiry_policy`: exact `version`, `expires_at_utc`, `approved_by`, `approved_at`,
  and hash of the actual source policy bytes in `policy_sha256`. The source
  policy and actual packaged lifecycle JSON must agree; beta.1's date cannot be
  inherited. No beta.2 date is supplied by this change.
- `comparison_protocol`: reviewer, approval timestamp, rationale and seven
  numerical error limits (`response_db`, `timing_ms`, `phase_degrees`,
  `latency_ms`, `meter_db`, `analyzer_db`, `bypass_transition_db`). Unknown limits
  are BLOCKED. This code does not invent sonic-equivalence tolerances. Review
  the protocol against the original reference before accepting measurements.
- `reports`: exact inventory from `--inventory`; each has `report` file/hash.
  Automatic rows additionally have `ci` with integer `run_id`, `run_attempt`,
  `artifact_id`, GitHub artifact ZIP `artifact_sha256`, and `report_member`.

Each report uses schema `spectralforge.chimera.issue97-evidence.report`, exact
`id`, `version`, `source_sha`, `kind`, `synthetic:false`, execution timestamp,
`packages`, `installed_binaries`, `raw` references, and every required assertion.
Each assertion has `passed:true`, a concrete `method`, and a `raw_path` from
those attached references. CI reports additionally record integer run/attempt.

Numeric checks do not accept bools, NaN or infinity. Transpose requires four
48kHz +/-1,+/-2 cases with separately recorded algorithmic, weak-attack maximum,
host-reported and roundtrip times. Algorithmic and weak-attack maximum must
both be 0..10ms. Same-DI/reference capture hashes must match actual raw evidence;
level alignment and the original product version are required. FabFilter
requires versioned actual captures for Pro-Q 4/Pro-C 3/Pro-MB/Pro-G/Pro-L 2/Pro-DS,
all six editors and actual instance spectra, independent multiband and measured
errors against the reviewed protocol. No true-peak/Dynamic Phase equivalence is
implied by a four-band implementation or a sample-peak limiter.

Manual rows include reviewer, host name/version, OS, and confirmation that an
actual release package was installed. AAX requires retail `Pro Tools`,
`developer_host:false`, and raw successful PACE/platform/installed signature
commands. System reports include the full CPU/RAM/OS/DAW/format/interface/driver/
sample-rate/buffer/oversampling/preset/power/GUI environment and measured 30-minute
48kHz, 128/256, 4x results: callback p99/peak, deadline misses, memory growth/peak.
Nonzero misses or callback values exceeding the block budget fail. These are
representative conditions, not certification of all configurations.

A reviewed weekly approval adds `issue97` with `schema`, `requirements_sha256`,
`manifest_sha256`, and `ci` identifying the P0 artifact and member
`issue97/manifest.json`. This fingerprint only preflights the scope; it is never
itself a P0 PASS. All public-approval fields stay false until separately granted.

Live verification requires the same repository/source/run attempt and actual
report bytes inside downloaded, digest-verified producer artifacts. It checks
the latest required build, lifecycle, signal-path, graphical-EQ, CAB-panel,
CAB-realtime, dynamics and AAX workflow runs. Missing, queued, skipped, cancelled
or failed work cannot be replaced by an older success. Native report producers
and manual evidence must exist before this path can pass; this change does not
claim to have supplied those missing real-world results.

## Executable preparation and verification

```bash
python Tools/issue97_gate.py --inventory
python Tools/test_issue97_gate.py
python Tools/test_weekly_publication.py
python Tools/test_publish_release.py
```

Pack **existing** complete evidence (never creates a PASS report or approval):

```bash
python Tools/issue97_gate.py --pack /path/to/evidence-root --output /path/to/new-evidence.zip
```

Read-only P0 verification after a source/candidate/evidence review is available:

```bash
python Tools/issue97_gate.py --version 1.3.2-beta.2 --source-sha FULL_PRODUCT_SHA \
  --bundle /path/to/issue97-artifact.zip --approval /path/to/reviewed-receipt.json \
  --candidate-dir /path/to/extracted-candidate --live
```

`--live` uses `GH_TOKEN` only for GETs. Offline evidence is diagnostic and cannot
PASS provenance. Even a P0 PASS returns `publication_allowed:false`: the full
publisher must independently pass the baseline, approval, package and calendar
contracts. With no candidate/evidence, the command exits 1 and prints BLOCKED.

## October 21 execution and remaining decisions

Run the gate contracts now. Select an integrated source only after feature
owners supply compatible changes. Run the existing native CI and collect its
actual artifacts on that source, preserving tuner/CAB failures. Run original
same-DI and versioned FabFilter comparisons; capture installed DAW and signed
AAX/retail Pro Tools acceptance on the required platforms. Review the comparison
protocol and beta.2-specific expiry separately; then review the evidence and
public approval. Missing DI/reference/host/signing access stays explicit BLOCKED.
Do not omit a P0, weaken a time budget, or publish to meet October 23.
