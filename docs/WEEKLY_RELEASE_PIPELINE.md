# Weekly release publication

Implementation follow-up to [planning PR #50](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/50).
The October 14, 2026 test build, October 15 candidate and October 16
`v1.3.1-beta.2` publication remain conditional targets. This infrastructure
change grants no release approval and does not publish or relabel beta.1.

## Entry points and identity

The existing Actions entry at `.github/workflows/publish-beta131.yml` is retained
so links and manual workflow selection continue to work. Its display name is
**Review or publish verified Chimera release**. It calls `Tools/publish_release.py`.

| Input | Contract |
| --- | --- |
| `target_version` | Required canonical version, for example `1.3.1-beta.2`, `1.3.1-rc.1` or `1.3.1`; no `v` prefix |
| `source_sha` | Required exact 40-character lowercase commit SHA of the tested product |
| `publish` | Defaults to `false`; review is offline and never creates a release |
| `confirm` | Publication only: `publish-v<TARGET_VERSION>@<FULL_SOURCE_SHA>` |

Versions are source assertions. Passing beta.2 while the selected commit contains
`VERSION=1.3.1` and `RELEASE_CHANNEL=beta.1` fails. The publisher reads these files
from the selected Git commit, checks the controller worktree, and compares the
source release scope, candidate metadata, payload identity, feed, artifact name
and approval receipt. A filename change cannot promote already-built bytes.

`VERSION` retains numeric `major.minor.patch`. `RELEASE_CHANNEL` supports private
`preview`, `beta.N`, `rc.N`, or `stable`. Public versions reject previews, leading
zeros, zero prerelease counters, unknown labels, build metadata, oversized numeric
components and malformed inputs. Private preview CI assembly remains available.

### Product source and publication controller

The product SHA identifies compiled binaries and all required verification runs.
The controller SHA is the exact checkout running the manual publishing workflow.
They may be identical, or the controller may be a descendant whose only file
difference is `Validation/weekly-publication-policy.json`, as a regular file.
Product changes, publisher code changes, workflow changes, deletions, renames,
symlinks, unrelated history and dirty tracked files are rejected.

Publication runs only from the repository's **current default branch**, resolved
from GitHub metadata and checked against the invocation and current branch head.
It has no dependency on `release/open-beta-1.3.1` or another per-version branch.
Review can run from a development branch. A new default-branch commit supersedes
an in-flight controller, including a commit that withdraws approval.

## October 14 candidate preparation

1. Integrate the reviewed infrastructure and intended product changes into the
   candidate source. For beta.2, set `VERSION` to `1.3.1`, `RELEASE_CHANNEL` to
   `beta.2`, and the matching `Validation/release-policy.json.release_scope`
   identity to `1.3.1-beta.2` / `1.3.1` / `beta.2` / `beta_1_3`.
2. Keep `preparation_authorized=true`, retain the beta check inventory and its two
   existing transpose deferrals, and keep source release flags unapproved during
   preparation. Update the actual release notes, including retained host and CAB
   limitations. Do not carry a historical approval into the new receipt.
3. Commit the complete source and run the product/feature CI on that exact SHA.
   Pull-request CI checks out the PR head. Optional manual build version/SHA
   inputs only assert that identity; they cannot select different bytes under a
   workflow run's original `head_sha`.
4. Retrieve `SpectralForge-Chimera-1.3.1-beta.2-Release-Candidate` from that build.
   The candidate records `version`, `tag`, `revision`, `runId`, `runAttempt`, and
   unpublished/signing/notarization status. Review the source-bound packages and
   perform the applicable installation, host and listening acceptance.
5. Record any subsequent actual publication approval as described below. Until
   then, leave the workflow in review mode. A missed date does not satisfy a gate.

The tested SHA must remain an ancestor of the eventual default-branch
controller, with identical product/publisher files. A squash or rebase merge can
create a different, non-ancestral source; in that case run all seven workflows on
the final default-branch product SHA and approve that SHA instead. Several
feature workflows use path filters, so a version/policy-only change may need
explicit manual dispatch of the missing workflows on a ref at that exact SHA.
An older run of unchanged feature code is not substituted for current-source CI.

The build workflow always produces a consolidated report even when acceptance
remains BLOCKED. Successful build jobs alone are not publication authorization.

Example offline review, after selecting an actual beta.2 source:

```bash
python Tools/publish_release.py --dry-run \
  --target-version 1.3.1-beta.2 \
  --source-sha "$TESTED_SOURCE_SHA"
```

For local package inspection, add `--candidate-dir`, `--run-id` and
`--run-attempt`; ZIP inspection also requires the original artifact metadata.
`--gate-archive` recomputes a supplied strict report. These local inputs never
make an offline review publication-ready and are forbidden in publication mode.

## Approval and acceptance

`Validation/weekly-publication-policy.json` is initially unapproved, with no
version, source SHA or policy fingerprints and no inherited gap acceptance.
After reviewing an actual candidate, a separate controller commit can record:

- Exact version, numeric product version, applicable profile and approved SHA.
- SHA-256 fingerprints of the **entire source strict policy** and CI ownership
  map. The validator also freezes the validation rules independently of release
  scope, so recomputing a fingerprint cannot silently remove a required gate.
- Explicit `release_approved` and `publication_authorized` booleans, plus the
  owner's actual instruction, context and scope. Strings such as `"true"` fail.
- A decision mode and its applicable acceptance evidence.

The new receipt is the authorization authority for this weekly path. Historical
`release_scope` flags are retained as source preparation metadata; they cannot
authorize a new release. This separates approval of an already-tested SHA from
the product commit, avoiding a self-referential approval SHA or an unnecessary
binary rebuild. The legacy beta.1 approval file is never imported as permission.

**`STRICT_PASS` is the default.** Every applicable strict check must actually pass.
RC and stable require the `full_release` profile, including the two transpose
checks that the existing beta profile defers. Their missing evidence remains
BLOCKED; support for those version strings does not claim release readiness.

The existing beta exception can be used only through a fresh, explicit
`OWNER_ACCEPTED_BETA_WITH_VALIDATION_GAPS` decision for that beta version and SHA.
It preserves the frozen 21 connected automatic checks and 107 individually
disclosed unexecuted checks, including 42 strict hard gates. Actual failures,
waivers, changed inventories and invented PASS results are rejected. The strict
report remains BLOCKED and is published alongside the separate decision. This
implementation does not grant that exception to beta.2 or any later source.

## Verification and publication

All seven existing required workflows remain mandatory:
`build.yml`, `update-candidate.yml`, `cab-panel.yml`, `cab-realtime.yml`,
`amp-head-controls.yml`, `signal-path.yml`, and `graphical-eq.yml`.
Their existing required jobs and Windows/Linux/macOS coverage are retained.
New publisher regressions also run inside the required product validation job
and in the fast `Chimera release publisher contracts` workflow.

The publisher selects the newest exact-source run of each workflow. Missing,
failed, cancelled or pending newer runs cannot fall back to an older success.
Repository provenance, positive run/attempt identities, required jobs and unique
unexpired artifacts with matching source/run, digest and size are required.
Jobs are fetched from the selected run attempt, and their source SHA, run ID and
attempt must match. The workflow path is checked, and run status/attempt is read
again after job and artifact lookup. The read-only gate rechecks CI identities
and the approval receipt after archive evaluation; a rerun or replacement blocks
its success. The publisher retains its checks before staging and before public
visibility. These are bounded snapshots, not an atomic lock on GitHub Actions.

The complete strict report is recomputed from its per-check JSONs. Embedded
platform manifests and their original raw files are hash-checked, source/run/
attempt checked and used to regenerate the connected automatic checks. Missing
raw evidence cannot be replaced by a summary claiming PASS.

Publication mode runs a dedicated read-only `release-gate` job first
(`publish_release.py --require-gate-pass`), and the `publish` job `needs:` it.
The gate job waits for the same exact-source CI, downloads the consolidated gate
artifact, recomputes it with the approval receipt and fails unless the result
authorizes publication. A BLOCKED, missing, stale, malformed or pending gate
fails the job, so publish never starts. The only BLOCKED result that passes is
an explicit `OWNER_ACCEPTED_BETA_WITH_VALIDATION_GAPS` receipt for that exact
version and SHA; its strict verdict stays BLOCKED and is disclosed. The publish
job repeats every check before staging. The PR/main `consolidate-release-gate`
job still exits 0 for a valid BLOCKED result, because a PR has no external
evidence; it only records the verdict and does not authorize anything.

The approval receipt is trusted repository configuration, not a cryptographic
proof of the owner's identity. Its review/access controls must prevent an
unauthorized writer from fabricating approval or changing the workflow itself.
An unchanged receipt cannot authorize another version, SHA, profile or policy.
No wall-clock evidence TTL is imposed: freshness means the newest qualifying
exact-source run, current attempt, unexpired artifact and matching policy. A
same-source approval can be used with a newer successful CI attempt only after
all evidence is recomputed for that attempt; old-attempt bytes are rejected.

The owner-gap path is not a strict PASS implementation of issue #21 requirement
3. It is the existing separate beta publication policy: 21 automatic PASS and
107 specifically enumerated, unexecuted BLOCKED checks, including 42 hard gates.
Observed failures, reduced inventories, RC/stable releases and arbitrary gaps
cannot use this exception. Issue #21's external acceptance and undefined check
work remains open. See [PR #159 audit](PR159_RELEASE_GATE_AUDIT_20261026.md).

Five binary packages, all checksums, candidate source/attempt, portable payload,
three platform feed entries, exact download URLs, source SHA and packaged notes
must agree. Existing IR distribution restrictions remain in force.

Only after these checks does the transport create or resume a staging draft.
It never moves an existing tag, overwrites a conflicting asset, or edits a
conflicting published release. After all uploads, immediately before the
draft-to-public request, it rechecks the controller, approval and latest CI run/
attempt/artifact identities. Revocation or a rerun blocks that request and leaves
any staged draft unpublished. Publication is followed by unauthenticated feed,
release, download URL and manifest verification. All versions share a publication
concurrency group; the workflow has no scheduled or automatic publication trigger.

## Update feeds and existing installations

| Source channel | Package/tag version | Canonical feed | Compatibility feed | Default receiver |
| --- | --- | --- | --- | --- |
| `beta.2` | `1.3.1-beta.2` / `v1.3.1-beta.2` | `update-beta.json` | Same file | beta, RC and stable successors |
| `rc.1` | `1.3.1-rc.1` / `v1.3.1-rc.1` | `update-rc.json` | `update-beta.json` | RC and stable successors |
| `stable` | `1.3.1` / `v1.3.1` | `update-stable.json` | `update-beta.json` | Stable successors only |

Compatibility feeds carry the same version, source SHA, package names, URLs,
sizes and hashes. Their receiver channel is `beta` for the already-shipped beta
updater. That updater already accepts stable SemVer; the alias therefore lets it
discover a qualified stable successor without falsely marking a stable GitHub
release as a prerelease. New receivers prefer canonical feeds and reject
tag/prerelease disagreement. Previously shipped preview builds retain their
existing SemVer limitation and may need manual installation of a same-base beta.

| Persistent identity | Retained value |
| --- | --- |
| Windows AppId | `SpectralForge.ChimeraAmpMatrix` |
| App/plugin/macOS package identifier | `audio.spectralforge.chimera` |
| JUCE manufacturer / plugin codes | `SpFg` / `Chm1` |
| Product name | `SpectralForge Chimera` |
| Debian package | `chimera-amp-matrix` |
| User preferences, presets and personal IR roots | Existing paths and formats |

Numeric installer/resource versions remain `major.minor.patch`. Full prerelease
versions distinguish download files, application display and updater ordering.
Windows retains its replacement rules for same-base beta files; macOS retains
its bundle upgrade behavior. Source contract and native updater tests do not
replace actual previous-installation upgrade, repair/removal, DAW recall or
personal-data preservation acceptance on the supported platforms.

## Regression checks

```bash
python Tools/test_publish_release.py
python Tools/test_weekly_publication.py
python Tools/test_publication_evidence.py
python Tests/test_packaging_version.py
python Tools/test_release_consolidator.py
python Tools/test_beta_publication131.py
python Tools/test_publish_beta131.py
```

`ChimeraReleaseTests` covers receiver transition and installer URL/name validation
in native C++. Historical beta.1 policies used by regression fixtures are frozen
separately so a legitimate beta.2/RC/stable source bump does not break historical
contracts or silently change the active approval policy.
