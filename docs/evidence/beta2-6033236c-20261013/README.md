# October 14 beta.2 candidate — source 6033236c

## Frozen source

- Product source: `6033236cb81820a43679cbbd34be020804f9658d`
- Product tree: `7b23b503b807db36ec4f51491d6aa5f8c29e7b8c`
- Version: `1.3.1-beta.2`
- Parent: `9375328c30dfe5908e5010342179b5afbc258900`
- Integration PR: [#55](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/55), open Draft against main.
- Verification status: **fresh exact-source CI and installed-product qualification in progress**.

This is an evidence-only branch. Its commits do not replace the product source listed above. No prior source's runtime or CI PASS qualifies this candidate.

## Integration and conflict analysis

The actual two-parent integration is c3e9d436fbdc9b8778b592f333c93037b012b8c5. Its first parent is the live latest #55, 022244e6ce845669665e9ffdfefe25bdee154b75 (the direct child of the user-supplied e7b00cb09c5431d86c03282392eafddbe8edbfdd). Its second parent is #58 at 068f1547face75b34c1760dd7b8c9d34868a5393, which includes #56 at d7450883ddbeba58e44cf217072f325654c16128. Donor history is retained. #56 is incorporated into the integration branch, not main; #58 is closed as incorporated.

The nine common files are Source/FactoryCabVoicing.h, Source/FactoryPresetLevels.h, Source/NiflheimrPresets.h, Tools/check_catalog_contract.py, Validation/catalog-count-contract.json, docs/FACTORY_CAB_VOICING_1_3.md, docs/MANUAL.html, docs/OPEN_BETA_RELEASE_NOTES.md and docs/PRESETS.md. There were zero textual conflicts. The combined edits preserve both the new names and #56's public-name substitutions/scanner; Doom Fuzz retains the Optical wording. #58's installer-only delta does not intersect the new #55 naming delta. No speculative production correction was added.

| Stable ID | Display label | Routing |
|---|---|---|
| original.rig.niflheimr.hrimfaxi.v1 | Metal Clank | Classic (0) |
| original.rig.niflheimr.garmr.v1 | Death Grind | Dual (1) |
| original.rig.niflheimr.nidavellir.v1 | Heavy Pulse | Matrix (2) |
| original.rig.niflheimr.ymir.v1 | Slam Punch | Matrix (2) |
| original.rig.niflheimr.hel.v1 | Doom Fuzz | Classic (0) |

The labels change; stable IDs, authored parameter values, routing, DSP, parameter identities/order/defaults/ranges and state recall remain. Public captions and host-facing display labels intentionally follow #56. The original #58 retirement fingerprints are retained instead of rewriting history to match current docs.

## Two confirmed test-contract problems

1. Fresh initial-integration c3 Windows runs both failed ChimeraUITests. Two stale assertions still expected ampInfo(model).reference, contradicting #56's intended sound-description captions. Commit 9375328c changes only Tests/CorrectionUITests.h and Tests/ChimeraUITests.cpp (8 additions / 4 deletions): retain presence, visibility and exact-caption assertions, require .character, and apply the existing independent safeText public-name check. Existing negative tooltip checks remain.
2. Fresh 9375328c completed 11/11 workflows and 33/33 jobs, yet the original gate was 20 PASS / 108 BLOCKED. A read-only audit of the hashed original LastTest.log reproduced E1.STATE_UI failure: all numeric counts and actual CTests passed, but the emitted “functional captions” phrase differed from the unchanged hash-pinned receipt requirement “short original model captions”. Unchanged strict verification and publication dry-run rejected it. Commit 6033236c changes only Tests/NativeUITests.h (4 additions / 1 deletion): restores that historical machine-readable phrase, adds a separate accurate public-caption message and explains the compatibility requirement. All test assertions and every policy file remain unchanged.

Original failures and raw evidence remain in [c3 evidence](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/tree/b912d14e2b39dc81a8cf67edfbb4b4aed2bf2feb/docs/evidence/beta2-integration-20261013) and [937 evidence](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/tree/20cc17e54737e0a7ff968798b51c6b885b7dfbc7/docs/evidence/beta2-9375328c-20261013). Their successes are historical, not current-source qualification.

## Fresh static source and policy verification

The exact Git commit and root trees were read afresh. Of 17 root entries, 16 are identical to the parent; only Tests changed. The commit diff contains only Tests/NativeUITests.h. Source, Tools, Installer, Validation, .github, docs, CMake and every other root entry have identical Git object IDs. This is current repository identity evidence, not reuse of parent runtime results. Full identity/diff proof is in static/source-identity.json and static/source-commit.json.

Exact-source policy reads confirm preparation authorized, but release approval, publication authorization and owner acceptance remain false. Weekly approval/publication/gap-acceptance flags remain false; waivers are empty. I2.PITCH_LIVE and I2.PITCH_DI remain in the existing transpose follow-up scope, which does not mean PASS or NOT_APPLICABLE. Every other acceptance gate remains.

### Fresh 19-check donor compatibility audit

The full current and three donor Git trees/files were retrieved again and evaluated with 19 explicit JavaScript static assertions; all 19 passed. This is separate from the historical 62-check local audit and is not a local C++/Python run. Raw trees, compared source content, blob IDs and the original current-source validation-contract job log are retained under ci/observations/static.

- All 102 Source paths are accounted for: 81 remain exact latest-#55 blobs, 18 other public-name changes are exact #56 blobs, and three combined files contain the intended label/comment/description changes. Their coefficients, state bodies and numeric behavior remain as in latest #55.
- The 19 directly inspected DSP/parameter/state/PluginProcessor paths are byte-identical to latest #55.
- All 20 non-document changes introduced by #58 are exact donor blobs, including native cleanup, platform packaging/installers, inventory and verification fixtures. The full Installer and workflow trees also match #58.
- Current names, stable IDs, generated filenames, indices 43–47 and routing modes agree. There remain 48 selectable presets and all previous 43 indices are preserved.
- The actual new validation-contract job (115140821449 in Product38350264755) checks out6033236c and reports the catalog plus13 public documents passing the public-name contract. Runtime captions still require the new native test executions.
- Selected approval/gate/CI-map/weekly policy files are exact latest #55. The publisher is exact #56/#58 and retains its added distribution public-name guard.

## Fresh CI

Original attempt-1 run IDs are recorded in ci/run-map.json. All are pinned to the source above. The three document compatibility jobs have completed successfully; final product/installer/CAB results and artifact verification will be recorded when complete. No source qualification is inferred from workflow start or from another SHA.

## Release boundary

No main merge, release, tag, feed publication or deployment has been performed. The source change is prepared for evaluation as an October 14 test candidate. Actual-user DAW/project recall, audio listening, applicable full platform installation/upgrade and signing acceptance, strict gate completion and owner approval must retain their actual statuses. They are not silently deferred merely to meet the date.

The local executor is unavailable during this source's verification. GitHub's original CI is used for native tests and installers. A separately recorded read-only evidence controller will recheck original raw archives using unchanged product verification tools; it will neither change approval policy nor publish. It is a separate audit, not an extra product workflow or an extra installer run.
