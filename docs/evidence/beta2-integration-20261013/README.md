# Historical c3 integration verification — superseded after a demonstrated test failure

## Product source identity

- Product commit: `c3e9d436fbdc9b8778b592f333c93037b012b8c5`
- Product tree: `9f147ca042491a56af6373592707bf2f6179b4c1`
- Product version: `1.3.1-beta.2`
- First parent, latest PR #55: `022244e6ce845669665e9ffdfefe25bdee154b75`
- Second parent, PR #58: `068f1547face75b34c1760dd7b8c9d34868a5393`
- Included PR #56: `d7450883ddbeba58e44cf217072f325654c16128`
- Shared ancestor of the naming branches: `559d0cb4aa0fa70ababdb1f29b71ea022a27bc4c`

The live PR #55 head had advanced beyond the user-supplied `e7b00cb09c5431d86c03282392eafddbe8edbfdd` to `022244e6ce845669665e9ffdfefe25bdee154b75`, which already contained the requested Metal Clank / Heavy Pulse labels. This integration used that latest source, preserving both donor histories in an actual two-parent merge. All tests documented here were rerun on the integrated product SHA. Parent-source passes were not reused.

This evidence branch is separate from the product branch. An evidence-only commit is not a new tested product source. PR #55 remains the open Draft integration PR; PR #56 was incorporated into its integration branch, and PR #58 was closed as incorporated. Main was not changed.

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

## Fresh local verification

| Check | Result | Scope |
|---|---|---|
| Independent static compatibility audit | 62/62 PASS | Source identity, IDs, DSP/state boundaries, donor preservation, generator reproducibility, policy |
| Existing build workflow Python contract blocks | 293 PASS, 3 Windows-only SKIP; 296 unique cases in 14 distinct suites | Both original workflow blocks, packaging/version/approval/provenance contracts; repeated executions are not double-counted |
| Public presentation naming unit tests | 8/8 PASS | Current public-name policy |
| Native processor/state CTests | 3/3 PASS | Native state, gate processor, five Niflheimr presets |
| Native helper unit tests | 41 PASS, 1 local UID/GID SKIP | Skip executed and passed in fresh Ubuntu CI |
| Windows document-rule unit tests | 11/11 PASS | Cleanup rule generation |
| Current document preservation scenarios | PASS | Document-only Linux native-helper scenarios using 12 exact source-authored public files; generated Windows/Linux/macOS rules, edited document preservation and personal-file sentinels. Product installers/binaries and build-generated documents are excluded. |

The native processor run checks the released 3,686-parameter compatibility contract, actual 4,967 host display names and 613 choice strings, binary/A-B state, automation/migration, native banks, and the five preset recalls. It does not establish DAW project or listening acceptance.

### Local failures and environment repairs retained

The first Python packaging-contract attempt could not locate CMake in PATH. Only the environment PATH was corrected, and the unchanged source's complete workflow blocks then passed. The final `source-contract-results.json` is authoritative; the first attempt's raw failure is retained in the archive for traceability.

The first native link encountered a demonstrated zero-byte generated `BinaryData209.cpp.o`. The invalid generated object was retained as evidence, removed from the build output, and rebuilt from the same source successfully. No source or threshold was changed. The desktop wrapper then failed before CTest because Xvfb could not bind local/unix sockets in this container. The three nongraphical processor/state tests were run directly and passed; no local native-window/UI pass is claimed.

## Completed c3 CI and disposition

**This product source failed required CI and is superseded by test-only correction `9375328c30dfe5908e5010342179b5afbc258900`. Nothing in this directory qualifies that later source.**

All 11 fresh workflows completed: 9 success, 2 failure. Of 33 jobs, 29 succeeded, 3 failed, and candidate assembly was skipped. All 32 actually executed jobs checked out c3; the skipped job had no checkout. Every run used attempt 1; no retry, waiver or threshold change was used.

Product Linux passed 45/45 configured CTests and macOS 44/44. Each Windows matrix (Product run 38335147294 and update run 38335147371) passed 47/48 but failed `ChimeraUITests`. Two existing tests required the original amplifier equipment name while the correct #56 production caption emits the public sound description. The missed expectations are `Tests/CorrectionUITests.h` and `Tests/ChimeraUITests.cpp`; `NativeUITests.h` already checked the correct public caption. Both failed files were unchanged through #55/#56/#58, explaining the integration gap. The follow-up changes only these two test files, adding the existing independent forbidden-name check; production behavior remains unchanged.

Windows staging, installer creation, actual install/repair/uninstall, and previous-public-beta.1 product upgrade **did not execute**. The always-run installer upload step logged no files and created no installer-verification artifact. There is no candidate artifact. The declared gate is BLOCKED: 2 PASS / 126 BLOCKED, with 61 blocked hard gates and missing assembly input. The unchanged publisher rejects the incomplete gate, and unchanged `verify_ci` rejects the actual recorded API responses because the latest exact-source workflow did not pass.

The separate document-upgrade workflow did pass on all three OSes: Ubuntu native-helper 42/42; Windows 12 real Inno fixture invocations / 8 groups; macOS arm64 4 real Apple PKG fixture invocations / 4 lifecycle groups. These use historical-document fixtures, not a prior full product installation. Product macOS/Linux packaging evidence verifies universal slices/ad-hoc seals/expanded PKG bytes or DEB extraction/dependencies/TAR construction respectively, without installing the generated product or executing full old-to-new product upgrades.

Fresh CAB performance evidence passed 15 CTests and all 198 p99 routes, with 78 individual callback deadline overruns across 316,800 callbacks. p99 PASS is not a zero-overrun claim or proof that historical timing failures are resolved. The original earlier 022244e6 macOS failure and c3 raw callback measurements are preserved separately.

All beta.2 publication/owner approvals remain false and waivers empty. No main merge, release, tag, feed update or deployment occurred. New CI and installer validation must complete on 9375328c before any new-source qualification can be stated; real DAW/listening acceptance and existing release gates remain required. Deferred transpose checks remain deferred, not PASS.

## Evidence files

`local-evidence.zip` contains the original local reports/logs, both initial failures, final successful tests, compatibility audit, and scripts. `evidence-manifest.json` binds the archive and each included file by size and SHA-256. Readable summaries are provided beside it. Signed download URLs and transfer credentials are excluded.

`ci-evidence.zip` and `ci-evidence-manifest.json` preserve the original c3 API metadata, both failing Windows job logs, unchanged verifier rejection, incomplete gate ZIP, document fixtures, platform package reports, and CAB proof. Readable final CI and NOT_RUN summaries are beside the archive.
