# Auto Set: final main integration, 2026-10-28

## Source and ancestry

- PR #156 was already merged into main at 2026-10-28 11:08:39 KST; this run did not merge it again. Merge commit: `1d3e3458c22b5b1f0c65742ee8827dd9358ecc7f`, parents `c47b4acbf8762e010ae3cf2a807c1b84b5d3c697` and verified #156 head `8b83631b82baa0a9a1aae646d157d49e3e4ca534`.
- #156 exact-head checks were 42 success / 2 conditionally skipped, with Graphical EQ all three OS, Open Beta, lifecycle attempt 2 and AAX completed/success. The original Xvfb failure is not erased or assigned an unproved root cause.
- PR #182 was already retargeted to main when inspected. Its starting head was `20e71e0823a4da7cfe0d154d8fa87c6cac4812ac`.
- Merge main `1d3e3458c22b5b1f0c65742ee8827dd9358ecc7f` into that head without conflicts. The source tree before adding this report/logs is `0777d6d573ac78feaae08a2ff7e32fdabf64484e`; independently constructed local and GitHub trees match exactly.
- Preserve main's incoming 29 files, including its reverb re-prepare fix and StateRecallAudioTests, Transpose research scoring/documents, beginner documentation and release-planning documentation. No new product fix or scoring change was authored for this integration.
- The only incoming product-source change is `Source/SpatialModules.h`: restore JUCE Reverb construction parameters before prepare so repeated prepare uses the same smoother starting targets. Its new audio recall regression is retained unchanged.
- Auto Set controller/edit tracking/strength, panel/editor option propagation and AutoSetTests match the previously reviewed #182 head byte for byte. The diff against main remains the same 13 Auto Set files, plus this new evidence. Gate 2/Expander locks, gates/strength/locks option order, Easy View propagation and ownership protections are preserved.

## Native validation

Linux x86_64, GCC 13.3, Release, JUCE 8.0.8; qualification-only build with tester codes disabled. Fresh isolated worktree/build. These are controller/DSP tests without a display, not GUI or actual DAW acceptance.

| Test | Result | Duration |
|---|---|---:|
| ChimeraAutoSetTests | PASS | 49.33 s |
| ChimeraCommonDynamicsTests | PASS | 3.01 s |
| ChimeraMultibandDynamicsCoreTests | PASS | 2.42 s |
| ChimeraStateRecallAudioTests | PASS | 24.24 s |

All four tests passed. Auto Set includes the 120 Gate-stage conditions (3 styles × 4 strengths × 10 ownership/option modes, all 21 Gate parameters), 72 protected attacks, 18 brightness positive controls, input/manual/host A-B-A preservation, frozen run options, late-lock undo protection, 28 lock/strength conditions and 36 attack/lock conditions.

State recall covers six factory/signature/Original/Niflheimr presets, repeated preset loads, A/B switching/copy and project restore/resave. Preset and A/B comparisons were bit-identical; project comparison differences were at most -146.866 dB relative energy, within the unchanged -120 dB requirement. This is synthetic audio equality, not listening approval.

Commands (run separately as executables became available):

```sh
cmake -S . -B build-integration -DCMAKE_BUILD_TYPE=Release \
  -DCHIMERA_BETA_QUALIFICATION_ONLY=ON \
  -DFETCHCONTENT_SOURCE_DIR_JUCE=/tmp/chimera-juce-8.0.8
cmake --build build-integration --target ChimeraAutoSetTests ChimeraCommonDynamicsTests ChimeraMultibandDynamicsCoreTests ChimeraStateRecallAudioTests -j2
ctest --test-dir build-integration -R '^ChimeraAutoSetTests$' -V
ctest --test-dir build-integration -R '^Chimera(CommonDynamicsTests|MultibandDynamicsCoreTests)$' -V
ctest --test-dir build-integration -R '^ChimeraStateRecallAudioTests$' -V
```

The first local link failed because generated artwork objects BinaryData184.cpp.o and BinaryData222.cpp.o were zero bytes. Only these generated objects were removed and rebuilt; product sources and assertions were unchanged. The subsequent link/build succeeded and no zero-byte objects remained. This does not establish a product defect or the cause of the earlier GitHub Xvfb timeout.

Raw CTest output: `main-integration-autoset.log`, `main-integration-dynamics.log`, `main-integration-recall.log`. The local precommit build metadata names starting head 20e71e0; the source tree and file hashes below identify the tested merged source. The new commit must receive its own CI; this is not a claim that old-head CI validates it.

## SHA-256 of tested source

| File | SHA-256 |
|---|---|
| Source/AutoSetController.h | 4cb77a8a5c5900a85db8a1b5bbbf7ddff3d2884af00958f9f64672e963fefce5 |
| Source/AutoSetStrength.h | 21d49bc20e46b05a97f44bfa0ceca86415283775faf4cb4920dad703e2b0df88 |
| Source/CommonDynamicsPanel.h | 64b30348740f713aa15747f95cfbb380a79611d68e28a9ee07ef46726e0f310d |
| Source/PluginEditor.cpp | 8ede15c3752467251a2bb21a2bd21df52e3cfebe8e20b643330b5ab4f83e23d6 |
| Tests/AutoSetTests.cpp | 7a87cab3f112a5f6616586f2c7c27eb4d08ed0d239283986a0c910a4cb931651 |
| Source/SpatialModules.h | fad060cc6596d4ffb65d7372962aa883a5cfda6b51febe3644facd0a7083cc2d |
| Tests/StateRecallAudioTests.cpp | f2d2a2b7ca8706df3c99c9497a60e136f7b41d4d7ee32f9e5df80e171947944e |

## Remaining gates

- Wait for the new exact head's Linux/Windows/macOS Graphical EQ Auto Set checks and all required CI; pending/queued/skipped tests are not passes.
- Recheck main ancestry, final diff, reviews and mergeability before a main merge with expected_head_sha. No force push or protection bypass.
- Actual guitar/bass DI listening, actual DAW Read/Touch/Latch/Write and deployment approvals remain incomplete. JUCE notification ownership does not provide atomic arbitration of arbitrary simultaneous host writes.
