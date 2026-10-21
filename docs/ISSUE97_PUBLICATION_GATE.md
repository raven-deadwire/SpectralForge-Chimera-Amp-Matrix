# Issue #97: candidate-bound P0 publication gate

Status, 2026-10-21 KST: **beta.2 BLOCKED / schedule re-estimation required**.
The existing evaluator and publisher path is implemented and reused; no P0
criterion, approval, expiry or required report is relaxed by this refresh.
October 22/23 remain blocked review checkpoints, not a qualified candidate or
committed release. The integrated beta.2 product SHA remains **unselected**.

## Current source comparison

[Current audit, source/CI/package evidence and schedule decision](evidence/beta2-readiness-20261021/README_KO.md).
Machine-readable `readiness-review.json` maps all four P0s and common requirements
to the existing 27 report IDs. It is a planning reference, **not** an acceptance
manifest. `current-readiness.json` is the output of this existing gate: BLOCKED.
The 2026-10-20 snapshot is historical and remains preserved in its original folder.

| Work | Latest inspected source | Implemented scope | Remaining qualification |
| --- | --- | --- | --- |
| #93 | `153f1a6a0400ad38287bbd59030e06dd794fb5cb` | Existing P0/calendar/publisher path; publisher jobs 5/5 | Full build validation-contract failed on temporary Git cleanup; build/assembly skipped, strict 0/128. This refresh isolates fixture maintenance; new-source CI is separate |
| #112 on #100 | `650feaa8955281232fc8c840bd82afdeda5df7e0` | Independent corr40-split research, production Source unchanged | Small high-band attacks B0/A0/A1 2.750/2.688/2.667ms; low band up to 16.877ms, upward attacks 37.687ms, tiny A1 misses 6/16; full <=10ms target NOT met, fixed PDC/real DI/reference/DAW missing |
| #110 | `3a942a125d652a234d0f481fca3af2bdf6897635` | Six spectrum editors and independent 4-band DSP on an unmerged product branch; full build complete, Windows/macOS/Linux 67/59/60 CTests pass | Separate macOS tuner and CAB-panel FAIL; original FabFilter comparison, approved tolerances, installed-host/listening acceptance missing; beta.1 packages, strict 21/107 |
| #111, rejected experiment | `0123668f74c7e61c84f209d91eaa54df864786b6` | Offline allpass experiment implemented at `1669a805f67c55acf4e3600172befd456455c7e0` | REJECT_CANDIDATE, +6.547dB low-band overshoot, 32/288 failures |
| #111, newer Source fix | `ffab682bef3468001f98624bda3051796341ab51` | Actual MultibandDynamicsCore/CommonDynamicsPanel changes since `63a72f4ac3551b76d4eb204c699469d23c1bde4e`, unmerged | Own numerical/native workflow 39529062371 pending/in progress at snapshot; previous experiment's verdict is not this source's result; no beta.2/host approval |
| #98 | `b7923ea8b7a0f85cfbfafb0e0338238adc09b181` | Countdown/lifecycle implementation, lifecycle CI 7/7 jobs SUCCESS | beta.2-specific expiry/policy/packages and installed DAW expiry/export/update restore missing; separate signal-path FAIL |
| #101 incl. #106 | `8ff4dbf59639f54c751232999da6e78055691703` | AAX target, host bypass PDC and recovery fixes; both native builds, CTest 5/5 each | Only AAX-UNSIGNED-DEVELOPMENT beta.1 packages; PACE/final signatures, retail Pro Tools on Windows/Intel/Apple Silicon, same-source four-P0 integration missing |

The latest main is `4aeb3033d15e04dbaa785811f4faf95db72882a9` (preset
maintenance), not the selected beta.2 source. Existing beta.1 and separate preset
maintenance publication do not transfer approval. No branch's historical PASS
is evidence for a different SHA. In particular, #111's historical experiment did not change production Source,
while its new implementation does. Its PR body was also updated during this review;
reported local numerical PASS remains separate from pending native/host acceptance.

The full report records four downloaded, digest-verified evidence archives,
actual strict reports and package metadata, source fingerprints, complete
SHA-filtered workflow inventory, and the remaining manual evidence gaps.

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

## October 22/23 review and remaining decisions

Keep the blocked review checkpoints and re-estimate development/publication timing.
Do not automatically roll over or consume another beta number. Select an integrated source only after feature
owners supply compatible changes. Run the existing native CI and collect its
actual artifacts on that source, preserving tuner/CAB failures. Run original
same-DI and versioned FabFilter comparisons; capture installed DAW and signed
AAX/retail Pro Tools acceptance on the required platforms. Review the comparison
protocol and beta.2-specific expiry separately; then review the evidence and
public approval. Missing DI/reference/host/signing access stays explicit BLOCKED.
Do not omit a P0, weaken a time budget, or publish to meet October 23.
