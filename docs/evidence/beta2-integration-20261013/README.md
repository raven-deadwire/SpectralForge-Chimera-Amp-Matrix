# October 14 beta.2 integration verification

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

Static comparison confirms 81 other production Source files remain byte-identical to the latest #55; the 18 non-overlapping naming Source files remain byte-identical to #56. DSP, recall implementations, parameter IDs/order/defaults/ranges, numeric mappings, serialized identities, and release policy remain unchanged. Fifteen #58 production/inventory files remain byte-identical to the donor. Historical cleanup fingerprints were not rewritten to fit the current names.

## Fresh local verification

| Check | Result | Scope |
|---|---|---|
| Independent static compatibility audit | 62/62 PASS | Source identity, IDs, DSP/state boundaries, donor preservation, generator reproducibility, policy |
| Existing build workflow Python contract blocks | 293 PASS, 3 Windows-only SKIP; 296 unique cases in 14 distinct suites | Both original workflow blocks, packaging/version/approval/provenance contracts; repeated executions are not double-counted |
| Public presentation naming unit tests | 8/8 PASS | Current public-name policy |
| Native processor/state CTests | 3/3 PASS | Native state, gate processor, five Niflheimr presets |
| Native helper unit tests | 41 PASS, 1 local UID/GID SKIP | Skip executed and passed in fresh Ubuntu CI |
| Windows document-rule unit tests | 11/11 PASS | Cleanup rule generation |
| Current document preservation scenarios | PASS | Current 12 public documents, edited manual/presets, personal preset/IR/project sentinels, repair and document removal |

The native processor run checks the released 3,686-parameter compatibility contract, actual 4,967 host display names and 613 choice strings, binary/A-B state, automation/migration, native banks, and the five preset recalls. It does not establish DAW project or listening acceptance.

### Local failures and environment repairs retained

The first Python packaging-contract attempt could not locate CMake in PATH. Only the environment PATH was corrected, and the unchanged source's complete workflow blocks then passed. The final `source-contract-results.json` is authoritative; the first attempt's raw failure is retained in the archive for traceability.

The first native link encountered a demonstrated zero-byte generated `BinaryData209.cpp.o`. The invalid generated object was retained as evidence, removed from the build output, and rebuilt from the same source successfully. No source or threshold was changed. The desktop wrapper then failed before CTest because Xvfb could not bind local/unix sockets in this container. The three nongraphical processor/state tests were run directly and passed; no local native-window/UI pass is claimed.

## CI and acceptance

Fresh GitHub Actions runs are bound to the product SHA above. Final CI/artifact verification is recorded separately when completed. The local evidence archive is neither an installer nor a release approval.

All release/publication approvals remain false, and the waiver list remains empty. No release, tag, update-feed publication, or deployment is authorized by these results. Real installed-product/DAW/listening acceptance and any source-specific release decision must satisfy the unchanged release gate. Transpose live/DI follow-up remains deferred under the existing policy and is not represented as PASS.

## Evidence files

`local-evidence.zip` contains the original local reports/logs, both initial failures, final successful tests, compatibility audit, and scripts. `evidence-manifest.json` binds the archive and each included file by size and SHA-256. Readable summaries are provided beside it. Signed download URLs and transfer credentials are excluded.
