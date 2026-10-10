# Open Beta 1.3 preparation and publication pipeline

The CAB release uses product version `1.3.0`, package version
`1.3.0-beta.1` and the intended tag `v1.3.0-beta.1`. These identify a new
candidate. The earlier `1.2.0-preview.40d5e8a5ea` installer remains a historical
test build and must not be renamed or promoted as a 1.3 binary.

## Authorization and current state

The owner explicitly requested fixing the cabinet issues, publishing version
1.3 and updating the RavenForge homepage after publication on 2026-10-10 KST.
The current `release_scope` records `publication_authorized=true` and
`release_approved=false`. Publication authorization does not certify
unexecuted instrument-DI, commercial DAW or historical acceptance checks.

The current cabinet follow-up preserves native grille apertures during
minification, fits the IR equipment stage to the selected captures with a
shared physical scale, and labels generic cabinet/microphone illustrations as
example images. Known metadata retains its documented configuration and model;
examples never change the IR or audio parameters. Exact-source CI must verify
these changes before the candidate can be accepted.

`publish-beta13.yml` has only `workflow_dispatch`. Its default is a read-only
review. The publication job requires the explicit publication option, the full
source SHA, the exact confirmation `publish-v1.3.0-beta.1` and branch
`release/open-beta-1.3.0`. The Python publisher independently checks the same
identities and current source-specific release scope before making an API call.
An existing `RELEASE_REVIEW_PENDING` file also blocks publication.

The preparation workflow can finish and deliver test packages while product
acceptance remains BLOCKED. A valid BLOCKED acceptance verdict is not relabeled
as a failed build or as completed listening/DAW acceptance.

## Exact-source prerequisites

| Workflow or input | Required result before publication |
| --- | --- |
| `build.yml` | Latest run for the accepted source succeeds, including validation contracts, Windows/Linux/macOS builds, candidate assembly and final consolidation. |
| `update-candidate.yml` | Latest run for the same source succeeds for preparation contracts and the Windows candidate. |
| `cab-panel.yml` | Latest run for the same source succeeds on Windows, Linux and macOS. A former CAB result cannot cover a later camera, DSP or version change. |
| Candidate artifact | Exactly one nonexpired `SpectralForge-Chimera-1.3.0-beta.1-Release-Candidate` from that build, with matching source/run and a verified archive SHA-256. |
| Acceptance artifact | Exactly one nonexpired `Chimera-A-stage-release-gate` from the same build; full source, run, attempt, policy and `beta_1_3` profile agree. |
| Publication scope | Current 1.3 publication authorization and release acceptance explicitly recorded; no pending review marker. |

The latest pending, cancelled or failed run blocks publication. There is no
fallback to an older successful run. The branch and selected run/attempt/artifact
identities are checked again after downloads, before the first release mutation.

## Concrete evidence production

`produce_release_evidence.py` records four separate bundles:
`Chimera-release-evidence-windows`, `-linux`, `-macos` and `-assembly`.
Each bundle binds the checked-out source SHA, workflow run, attempt, policy and
profile, hashes the raw receipts, and records actual step outcomes.

`consolidate_release_gate.py` reads these bundles without merging their files.
It verifies source identity and hashes, then derives automatic assertions from
the configured CTest inventory, JUnit, complete test log, platform packaging,
Windows installer/MSIX/Defender records and candidate assembly receipts.
Common tests require the relevant evidence from all three platforms.

Only the final consolidator uses the `Chimera-A-stage-release-gate` artifact name.
The earlier baseline is called `Chimera-validation-seed`; seeded summaries never
override concrete evidence. Missing, malformed, duplicate, stale or modified
bundles produce a BLOCKED artifact and an infrastructure failure. A partial job
rerun cannot reuse receipts from an earlier attempt as current evidence.

The publisher recomputes the complete gate from its underlying check reports
using the current policy and waivers. The artifact's summary must exactly match
the recomputed result. Missing assertions, missing exit codes, malformed JSON,
disabled source binding or a summary-only PASS cannot authorize publication.

## Existing acceptance work remains visible

The reviewed ownership map covers 21 automatic checks: A12/A13, I1 and three
E1 implementation/compatibility contracts. The unchanged beta profile contains
128 required checks. With complete fixture receipts the producer therefore
reports **21 PASS / 107 BLOCKED**. This count is a contract fixture result,
not a claim about the new source's CI execution.

The remaining 107 comprise 86 historical numbered A assertions whose original
definitions/producers are absent and 21 checks requiring external inputs,
reference comparison or manual acceptance. They cannot all be completed by a
single listening confirmation. Future evidence needs explicit check ownership
and a source-bound receipt before replacing an unexecuted baseline.

Actual guitar/bass DI listening, Studio One/commercial DAW lifecycle and
target-hardware acceptance remain separate from synthetic CI. The two existing
transpose goals stay deferred under their existing follow-up profile. No
acceptance threshold, waiver, required stage or manual result is weakened.

## Offline review

The default invocation performs no network reads or writes and reports missing
candidate/evidence inputs explicitly:

```sh
python Tools/publish_beta13.py --dry-run --source-sha FULL_SOURCE_SHA
```

To inspect an already extracted candidate:

```sh
python Tools/publish_beta13.py --dry-run --source-sha FULL_SOURCE_SHA \
  --candidate-dir review/candidate --run-id BUILD_RUN_ID
```

For an intact Actions ZIP, replace `--candidate-dir` with
`--candidate-zip review/candidate.zip --artifact-json review/artifact.json`.
The JSON must be the artifact metadata matching that source and build; its size
and digest are checked against the ZIP. `--gate-archive review/release-gate.zip
--run-attempt BUILD_RUN_ATTEMPT` additionally recomputes a consolidated gate.
Offline results never claim that live GitHub provenance or publication was
verified, and never call the publishing transport.

Five public binary packages, their exact checksums and sidecars, the three
platform update manifest, portable payload source/version, packaged release
notes and source documentation are checked. Personal IR/NAM captures remain
excluded. The established draft/upload/hash/public-discovery transport refuses
to replace mismatched assets or move existing tags.

## Selective integration provenance

The evidence producer, strict evaluator semantics and three E1 ownership
contracts are adapted from reviewed
[PR #23 at `4d3d2ec`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/23),
with the intended profile changed to `beta_1_3` and the existing CAB/Linux
workflow paths preserved. This is a selective integration of release support;
it does not merge the unrelated NAM training, live signing or Node 24 branches.
Existing source-level implementations and their previous evidence retain their
own provenance.

Run `Tools/test_publish_beta13.py` and `Tools/test_release_consolidator.py` for
the publication-negative and raw-evidence contracts. Their positive paths use
explicit test fixtures and never represent product acceptance. Fresh CI for the
final integrated 1.3 source remains required after every source change.
