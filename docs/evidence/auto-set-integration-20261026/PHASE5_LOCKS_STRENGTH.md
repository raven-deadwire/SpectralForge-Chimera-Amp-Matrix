# Phase 5 gate-stage lock and strength coverage (2026-10-28)

## Source and scope
Reviewed PR #182 merge head `156f8a3d67a8288eb7b834b289c52b68afbdd584`, with parents `f4c8a3c817773ea9eb15818ccd2e619fed2006e8` and PR #156 `8b83631b82baa0a9a1aae646d157d49e3e4ca534`.
The merge includes Gate 2 / Expander, the input A-B-A guard, Gate lock classification for all 21 affected parameters, and the `gates, strength, locks` option order through Controller, panel, and editor callbacks.
This follow-up changes tests and evidence only. It does not merge either PR or retarget #182.

The earlier five-pass strength/lock fixture deliberately disables the new gate stages. Its older PASS does not certify these stages. The expanded controller test feeds separate input and rig signals through the real capture seam, with audible rig hiss and positive controls for the soft and hard plans.

## Matrix
Three styles (clean/rock/metal), four strengths (25/50/75/100%), and ten modes = 120 cases:
1. Normal apply and Undo.
2. Manual input A-B-A during capture.
3. Host input A-B-A without gestures during capture.
4. Gate lock with newer host gate values during capture.
5. Host A-B-A on every affected gate parameter during capture.
6. Host A-B-A on every affected gate parameter after apply, before Undo.
7. Changing gates/strength/lock options during capture: active plan remains frozen; newly selected Gate lock protects Undo.
8. Selecting the Gate lock after apply protects Undo.
9. Input locked, Gate free: thresholds follow the actual unchanged input gain.
10. Non-default Gate lock without host edits, as an independent lock-only control.

All 21 gate-related parameters are compared at apply and Undo boundaries. A-B-A checks preserve the raw values actually delivered by host notification, including parameter normalisation. Unlocked controls also assert measured evidence, plan activation, the independent noise-plus-margin threshold at the actual input gain (allowing integer-dB quantisation), the Gate 2 threshold offset, and unscaled protection range.

The existing attack, brightness, rounding, seven-lock/four-strength, and attack/lock regressions remain enabled.

## Validation
Linux x86_64, GCC 13.3, Release, JUCE 8.0.8. Native tests passed:
- Auto Set: **PASS, 47.33 s**, including all 120 gate-stage cases and the existing regressions. Raw log: [phase5-lock-strength-autoset.log](phase5-lock-strength-autoset.log).
- Common Dynamics: **PASS, 2.06 s**.
- Multiband Dynamics Core: **PASS, 1.85 s**.
- The latter two ran separately without rerunning Auto Set; raw log: [phase5-lock-strength-dynamics.log](phase5-lock-strength-dynamics.log). All three requested tests passed.

Commands (after configuring Release with `CHIMERA_BETA_QUALIFICATION_ONLY=ON`):
```sh
cmake --build build-followup --target ChimeraAutoSetTests ChimeraCommonDynamicsTests ChimeraMultibandDynamicsCoreTests --parallel 2
ctest --test-dir build-followup -R '^ChimeraAutoSetTests

## Approval boundaries
Only synthetic native controller/DSP regression coverage is claimed. Linux/Windows/macOS Graphical EQ and required CI must succeed on the final pushed SHA before merging. Previous-head CI is not inherited. After #156 merges into main, #182 still requires main retargeting and review/testing of any further integration delta.

Actual guitar/bass DI listening, DAW Read/Touch/Latch/Write, and deployment approval remain incomplete. Native notification history is not atomic arbitration of arbitrary concurrent host writes. This does not approve a release.
 -V
ctest --test-dir build-followup -R '^Chimera(CommonDynamics|MultibandDynamicsCore)Tests

## Approval boundaries
Only synthetic native controller/DSP regression coverage is claimed. Linux/Windows/macOS Graphical EQ and required CI must succeed on the final pushed SHA before merging. Previous-head CI is not inherited. After #156 merges into main, #182 still requires main retargeting and review/testing of any further integration delta.

Actual guitar/bass DI listening, DAW Read/Touch/Latch/Write, and deployment approval remain incomplete. Native notification history is not atomic arbitration of arbitrary concurrent host writes. This does not approve a release.
 -V
```

These controller/DSP tests ran headlessly, without a native desktop. A separate local desktop probe could not open an Xvfb listener socket; it is not a passing GUI test and does not establish the cause of the earlier GitHub runner's readiness timeout. No product code, CI assertion, desktop readiness limit, or workflow was changed.

SHA-256 of the tested files:
| File | SHA-256 |
|---|---|
| Tests/AutoSetTests.cpp | `7a87cab3f112a5f6616586f2c7c27eb4d08ed0d239283986a0c910a4cb931651` |
| Source/AutoSetController.h | `4cb77a8a5c5900a85db8a1b5bbbf7ddff3d2884af00958f9f64672e963fefce5` |
| Source/AutoSet.h | `864b40eb714122b5835d4f823b2f7e38317b7f0687b97a4fce5e40e2218e1245` |
| Source/PluginEditor.cpp | `8ede15c3752467251a2bb21a2bd21df52e3cfebe8e20b643330b5ab4f83e23d6` |
| Source/CommonDynamicsPanel.h | `64b30348740f713aa15747f95cfbb380a79611d68e28a9ee07ef46726e0f310d` |

All product files are unchanged from the reviewed merge head. Final-SHA GitHub CI is still required; these local results do not certify the old or newly queued CI runs.

## Approval boundaries
Only synthetic native controller/DSP regression coverage is claimed. Linux/Windows/macOS Graphical EQ and required CI must succeed on the final pushed SHA before merging. Previous-head CI is not inherited. After #156 merges into main, #182 still requires main retargeting and review/testing of any further integration delta.

Actual guitar/bass DI listening, DAW Read/Touch/Latch/Write, and deployment approval remain incomplete. Native notification history is not atomic arbitration of arbitrary concurrent host writes. This does not approve a release.
