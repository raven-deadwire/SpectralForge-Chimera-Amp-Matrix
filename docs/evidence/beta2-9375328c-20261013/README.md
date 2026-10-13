# October 14 beta.2 candidate — source-specific verification

## Candidate identity and dependency disposition

- Product source: `9375328c30dfe5908e5010342179b5afbc258900`
- Product tree: `b02780be13bb594074bf51b269f804b4c861e5a4`
- Product version: `1.3.1-beta.2`
- Direct parent: `c3e9d436fbdc9b8778b592f333c93037b012b8c5`, the actual two-parent integration.
- Integration first parent: latest #55 `022244e6ce845669665e9ffdfefe25bdee154b75`.
- Integration second parent: #58 `068f1547face75b34c1760dd7b8c9d34868a5393`, containing #56 `d7450883ddbeba58e44cf217072f325654c16128`.
- Original shared naming ancestor: `559d0cb4aa0fa70ababdb1f29b71ea022a27bc4c`.

The live #55 head had advanced beyond the user-supplied verified `e7b00cb09c5431d86c03282392eafddbe8edbfdd` to its direct child `022244e6`, which already implemented the requested Metal Clank / Heavy Pulse labels. That latest source was integrated with #56/#58 without discarding donor history. The subsequent 9375328c commit fixes only two test files after concrete failures; no production file changes were added beyond the reviewed integration.

#55 remains the open Draft integration PR against main. #56 is recorded by GitHub as incorporated into the integration branch (not main). #58 was closed as incorporated. Main remains outside this integration. No release, tag, feed publication or deployment is part of this work. This evidence-only branch is distinct from the tested product source.

## Conflict analysis and retained behavior

Nine files overlapped between the latest #55 naming changes and #56:

1. `Source/FactoryCabVoicing.h`
2. `Source/FactoryPresetLevels.h`
3. `Source/NiflheimrPresets.h`
4. `Tools/check_catalog_contract.py`
5. `Validation/catalog-count-contract.json`
6. `docs/FACTORY_CAB_VOICING_1_3.md`
7. `docs/MANUAL.html`
8. `docs/OPEN_BETA_RELEASE_NOTES.md`
9. `docs/PRESETS.md`

There were zero textual merge conflicts. Semantic inspection found the preset-name edits and the public-name substitutions independent. The catalog contract retains both the new name expectations and the public-name scan. The Doom Fuzz description retains #56's Optical substitution. The #58-only installer delta has no overlap with the latest #55 naming delta. No speculative production correction was added.

| Stable preset ID | Final display name | Routing |
|---|---|---|
| `original.rig.niflheimr.hrimfaxi.v1` | Metal Clank | Classic (0) |
| `original.rig.niflheimr.garmr.v1` | Death Grind | Dual (1) |
| `original.rig.niflheimr.nidavellir.v1` | Heavy Pulse | Matrix (2) |
| `original.rig.niflheimr.ymir.v1` | Slam Punch | Matrix (2) |
| `original.rig.niflheimr.hel.v1` | Doom Fuzz | Classic (0) |

Static comparison confirms 81 other production Source files remain byte-identical to the latest #55; the 18 non-overlapping naming Source files remain byte-identical to #56. DSP processing, authored audio parameter values, routing modes, recall implementations, parameter IDs/order/defaults/ranges, numeric mappings, serialized identities, and release policy remain unchanged. Public captions, host display names, choice labels, and descriptive metadata change as intended by #56. Fifteen #58 production/inventory files remain byte-identical to the donor. Historical cleanup fingerprints were not rewritten to fit the current names.

## Confirmed integration problem and exact correction

The first integrated source c3 was fully tested and failed. Both independently built Windows matrices passed 47/48 CTests but failed `ChimeraUITests`. `Tests/CorrectionUITests.h` and `Tests/ChimeraUITests.cpp` still required `ampInfo(model).reference` in visible selected amplifier captions. #56 correctly changed the production editor to `amp.character`; `NativeUITests.h` already enforced that intended behavior. The two missed assertions were byte-identical through #55/#56/#58 and contradicted the non-disclosure policy. This was a deterministic stale test contract, not an installer or DSP regression.

The 9375328c correction changes exactly those two test files (8 additions / 4 deletions): captions must remain present, visible, and equal to the selected sound description; the existing independent `publicNameTests::safeText` policy is applied at both sites; and the obsolete success diagnostic is corrected. Existing negative tooltip checks remain. No test is bypassed, no tolerance is widened, and no product, installer, DSP, parameter, state or release-policy file changes in this correction.

Windows staging and all actual product installer operations did not run on c3. Its gate declared BLOCKED (2 PASS / 126 BLOCKED, 61 blocked hard gates) and had a missing assembly input. The unchanged publisher rejected incomplete acceptance; the unchanged CI verifier rejected the failed exact-source run. Full historical original evidence is retained at [commit b912d14e](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/tree/b912d14e2b39dc81a8cf67edfbb4b4aed2bf2feb/docs/evidence/beta2-integration-20261013).

Those failures, and any c3 passes, qualify neither this new source nor a later source. All final results below must bind 9375328c.

## Fresh local verification at 9375328c

| Check | Result | Scope |
|---|---|---|
| Static compatibility and catalog reproducibility | 62/62 PASS | DSP/state/IDs/policy boundaries, donor preservation, three generator outputs |
| Public-name Python unit suite | 8/8 PASS | Current forbidden-name policy, executed afresh |
| Existing workflow Python contract blocks | 293 PASS + 3 Windows-only SKIP; 296 unique cases in 14 suites | Both unchanged build workflow blocks, version and publisher offline dry-run; repeated suites are not double-counted |
| Native helper unit tests | 41 PASS + 1 local UID/GID mapping SKIP | Current source; platform CI coverage recorded below |
| Windows cleanup-rule unit tests | 11/11 PASS | Current source |
| Current-document preservation probe | PASS | Document-only Linux native-helper fresh/repair/remove, 12 exact source-authored public files; generated rules for all three platforms |
| Processor/state native CTests | 3/3 PASS, 41.18 seconds | Fresh configure/rebuild stamping 9375328c; NativeState, GateProcessor, NiflheimrPreset |

The document-only local probe preserves modified Manual/Presets files and personal preset/IR/project sentinels. It excludes product installers, application/plugin binaries, and build-generated Verification/README/payload-manifest files. Historical cleanup fingerprints remain byte-identical to #58; they were not rewritten to match new documentation. Platform line endings can change raw document bytes, so an LF Git hash is not asserted to be a Windows-installed CRLF hash.

The earlier container Xvfb socket-binding failure is an environment boundary; native Windows full UI execution is required below. Earlier environment failures and old-source measurements are retained in the historical archive rather than represented as new-source passes.

## Fresh CI and installer qualification

Verification is in progress. This section will be completed only from original 9375328c workflow metadata, checkout logs, reports and artifact bytes.

## Release and October 14 decision

Publication and owner-acceptance flags remain false, waivers remain empty, and the source's release-policy files are unchanged. Any deferred transpose live/DI checks remain deferred, not PASS. The final source-specific gate and acceptance decision will be recorded after verification. No unverified deployment will proceed.

## Evidence

Final original reports, logs, scripts and archive digests will be supplied beside this README. Historical c3 material remains separately identified. Signed transfer URLs and credentials are excluded.
