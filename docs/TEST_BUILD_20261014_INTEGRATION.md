# October 14, 2026 test-build integration

This is the source-selection record for a **test candidate**, not a publication
receipt. October 14 testing, October 15 candidate selection and October 16
`v1.3.1-beta.2` publication remain conditional. The candidate revision is the
commit containing this file, as recorded by the build; it must be freshly
configured and tested. Any later product, policy, publisher or workflow change
requires a new candidate revision and matching evidence.

## Sources and disposition

The GitHub state below was inspected on October 12, 2026 (Asia/Seoul). A PR being
open does not establish that its commits are missing from the product.

| Source | Exact revision | Disposition |
| --- | --- | --- |
| `main` | `3817246f779a50f19b5378ec9661a2275e6b91b2` | Shipped beta.1 integration baseline |
| [#40](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/40) | `5712cefa379c38d071ef0a4a6ab97cd3ff1e466a` | Already an ancestor of main; no additional merge |
| [#44](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/44) | `27daebe6d36a257926a4f9b4cb36c9d44eedf1f8` | Already an ancestor of main; no additional merge |
| [#51](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/51) | `aca556081a570765c71044223c1c91c251862407` | Include the implemented weekly publisher unchanged |
| [#54](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/54) | `97d6786ba35fa541f2ff50b621222aac4f007a5b` | Include Classic room, CAB/IR isolation, Niflheimr rigs and their corrective tests/rendering |
| [#52](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/52) | `72e361ba91ea2c6b8d830921a71275fa25cba493` | Exclude; separate EQ preview and October 23 installer acceptance |

#40 is based on `develop/1.3.1`; #44 is stacked on #40. Both heads are already
ancestors of the #48 merge that produced current main. #51, #54 and #52 branch
from main. #51 and #54 have no overlapping changed paths and merge cleanly.
The candidate retains both included heads as ancestors rather than squashing
or relabelling their existing binary artifacts.

The #52 preview CMake option changes display/package identity only. It does
not compile out the new EQ. Excluding that PR is the boundary: turning the
preview option off after merging would still include its new product behavior.
#52 and #54 both change `Source/PluginEditor.cpp`, `CMakeLists.txt` and
`Tests/NativeMessageLoop.mm`; their later integration needs normal review.
[Issue #53](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/53)
tracks the separate EQ acceptance scope.

## Candidate identity correction

The unmodified included heads and main all identify as `1.3.1-beta.1`. Existing
weekly code correctly rejects a beta.2 target for that source. This candidate
changes only the preparation identity and notes in addition to the clean merge:

| Field | Candidate requirement |
| --- | --- |
| `VERSION` | `1.3.1` |
| `RELEASE_CHANNEL` | `beta.2` |
| Strict policy scope | `1.3.1-beta.2` / `1.3.1` / `beta.2` / `beta_1_3` |
| Source preparation | `preparation_authorized=true` |
| Source publication and release flags | `false`; historical beta.1 acceptance is not carried forward |
| Weekly receipt | Original unapproved receipt; null version/source/fingerprints, `STRICT_PASS`, all acceptance flags false |
| Candidate / tag identity | `1.3.1-beta.2` / `v1.3.1-beta.2`; no tag is created by this integration |
| Update feed | Generated `update-beta.json`, same full source SHA and beta.2 package names/hashes |
| Installed identifiers | Existing Windows/App/plugin/package identities and personal-data paths |

`docs/OPEN_BETA_RELEASE_NOTES.md` now describes the actual candidate, the EQ
exclusion and unfinished verification. The historical beta.1 policy fixtures
remain unchanged. No required workflow, validation check, hard gate, timeout,
waiver inventory or existing transpose deferral is changed by these preparation
edits. The weekly publisher is reused, not reimplemented.

## Evidence available before the integration

- #51's nine observed workflows, including all seven publication-required
  workflows, succeeded at `aca5560`. This supersedes the PR body's older queued
  status, but certifies beta.1 source only. [Product run 38207733695](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38207733695)
  has candidate artifact `11730535918`, named
  `SpectralForge-Chimera-1.3.1-beta.1-Release-Candidate`.
- Its [consolidated artifact 11730307462](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38207733695/artifacts/11730307462)
  was downloaded and SHA-256 checked:
  `a4ab54f1994dcc2e310bbab19956a62b7999cd81c04efd57e92b92aca82ef9c6`.
  Recomputing the embedded raw evidence validates its integrity, while the
  strict verdict stays **BLOCKED: 21 PASS, 107 BLOCKED**, including 42 blocked
  hard gates. These are historical source results, not candidate beta.2 results.
- #54's previous `b94055e` macOS CAB run
  [38242325898](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38242325898)
  failed the Classic-room observation deadline; the listener fired only after
  the failure probe. Its macOS audition run
  [38242325688](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38242325688)
  failed an exact raw zero comparison. `97d6786` reduces repeated grille draw
  submissions within the same cache bound and compares the host's canonical
  zero detent without loosening the assertion. Fresh platform results are
  required to establish whether the corrective source passes.
- #52's [targeted EQ run 38243938568](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38243938568)
  succeeded at `72e361b` on Windows, macOS and Linux (six selected tests each).
  That is an independent developer preview, not this beta.2 candidate or an
  accepted October 23 installer. DAW/device acceptance remains unassessed.

Foreign local build folders are not candidate evidence. In particular, an
existing #54 build still recorded `b94055e` and beta.1 after its source checkout
had advanced. Reconfigure in an isolated build directory after committing the
complete candidate; never reuse that build metadata or rename its packages.

## Verification sequence and unresolved conditions

1. Freeze the full candidate and verify its `VERSION`, channel, release scope
   and clean Git tree. Configure fresh; `chimera-build-version.json` must record
   that exact full SHA and `1.3.1-beta.2`.
2. Run the existing publisher, historical-fixture, version/package,
   consolidation, raw-evidence and release-scope regressions. Check the strict
   rules fingerprint against the unchanged source policy rules.
3. Require the newest successful exact-source executions, current attempts and
   matching unexpired artifacts for **all seven** workflows: `build.yml`,
   `update-candidate.yml`, `cab-panel.yml`, `cab-realtime.yml`,
   `amp-head-controls.yml`, `signal-path.yml`, `graphical-eq.yml`.
   Older-source or older-attempt successes do not qualify. The additional
   Niflheimr audition/stock-preset checks must verify the revised rigs on all
   supported platforms. No synthetic test is counted as listening acceptance.
4. Inspect the beta.2 candidate artifact: compiled revision, source identity,
   run ID/attempt, all five binary packages, exact filenames, hashes, release
   notes, portable payload and three feed entries must agree. Do not alter the
   existing public beta.1 feed or tag while preparing this candidate.
5. Before declaring the test installer ready, resolve any failed or missing
   native/platform/package checks, including the macOS Classic-room deadline
   and Niflheimr zero-detent regression. Test actual beta.1-to-beta.2 upgrade,
   repair/removal, user IR/preset preservation and relevant DAW recall.
6. Before any public release, recompute strict acceptance from original raw
   evidence. The new weekly receipt is unapproved. Actual approval must bind
   the selected version, source SHA and policy/map fingerprints; the default
   remains `STRICT_PASS`. No prior beta gap acceptance is inherited.

The tested product SHA must be an ancestor of the eventual default-branch
publication controller. The controller may differ only by the separate weekly
receipt. Changing product/publisher/workflow/policy files, or squashing/rebasing
to a different non-ancestral product SHA, requires fresh validation. Default
branch/controller and latest CI/artifact identity checks remain in force.

**Decision at source preparation:** integration is feasible and reviewable;
the complete beta.2 installer is not yet qualified. October 16 publication
remains blocked until the source-specific CI, acceptance and receipt satisfy
the existing pipeline. A calendar target cannot substitute for any condition.
