# PR #250 independent follow-up — 2026-11-02

## Scope and decision

- Reviewed production source: `d64831fd05a378f17130c0cf07b6453f2c020ae6`.
- Latest main at review: `58e193434ffe48dac8c96005e3479e84a1cd1ed4`.
- Previous successful Phase 9 head: `635502ca40b5faa3ee3e04ec441024dfecc146a6`.
- No additional production CAB defect reproduced in this review. The CAB placement ownership fix passes independent execution and negative-control checks.
- A separate finite-burst Dynamics UI test timing defect was reproduced. This follow-up changes only the test and records the evidence. New-head remote CI remains a merge gate; an old-head pass is not a new-head pass.

## CAB ownership verification

`intact()` checks the edit revision, active gesture and current owned value of all three placement controls. Every write rechecks the group. A synchronous host edit invalidates the group; rollback touches only still-owned entries and restores their previous snapshot bookkeeping. A host edit, including one returned to its initial value, remains outside Auto Set's UNDO ownership. These checks concern final ownership and restoration after edits; they do not claim sample-atomic publication of three APVTS parameters.

Independent Linux GCC 13.3 Release / JUCE 8.0.8 execution:

| Check | Result |
|---|---|
| Exact reviewed head, `--cab-edits` | 92 cases passed, 8 useful unedited moves, 32.317 s |
| Same tests with main's pre-fix controller, renamed in an isolated runner | Fails with `Edited mic left a partial automatic placement` |
| Supplemental value-only A-B-A automation | 92 cases passed |
| Supplemental same-value host notifications | 92 cases passed |
| Supplemental persistent host values | 92 cases passed, 29.839 s |
| Full `ChimeraAutoSetTests` | PASS, 254.969 s, including the 92 edit cases |
| `ChimeraCabLayoutModelTests` | PASS: 9 layouts, 270 compatibility requests, 15,840 compatible driver/mic/unit boundary combinations |

Returned edits span Strength 25/50/75/100%; held gestures and begin/value/end callback combinations use 50%. The 92 cases are not a full Cartesian product of every setting.

The supplemental runners use the production processor/controller and the same compile flags and linked dependencies as the normal Auto Set target. They vary the edit action in `AutoSetCabEditTests.h`: value-only A-B-A, a repeated current value, or a retained new value. Persistent position/distance changes during capture may be rejected by the earlier `cabinet or mic changed while listening` guard; this is an expected safe outcome, not a partial placement. The first diagnostic assertion was adjusted to recognize that existing guard; no production change was needed.

The full suite independently reproduced:

- 90 generated-IR pairs / 270 level cases; worst level error 0.010091 dB.
- 18 accepted moves; largest generated-IR local-extrema addition 2.9698 dB, below 3 dB.
- 24 bass controller moves / six lock cases, A/B, four Strength settings, full-state UNDO.
- Parameter notifications, host-thread A-B-A, same-value gestures, begin/value/end reentrancy and zero observer allocations.

The CAB controller SHA-256 remains `c3bcaf6659f28a7a79be69a95426d69541d42058f8531a9581e0e0132be2d136`.

## macOS failure attribution

| Source / run | Common Dynamics | Native Dynamics | Native queue |
|---|---:|---:|---:|
| Previous head, run 41380883382 | PASS 55.62 s | PASS 64.81 s | PASS 64.13 s |
| Reviewed head, run 41425791522, attempt 1 | FAIL 44.87 s | PASS 67.55 s | PASS 68.03 s |
| Same head, same run, targeted rerun | PASS 70.89 s | PASS 71.21 s | PASS 70.55 s |

The failed job was `125211957140`; the rerun was `125253359624`. The rerun log confirms checkout of `d64831f`; no source edit preceded the rerun. The original failure is retained rather than reclassified as a clean first pass.

The failed job reports native-dispatch overruns of 762.265 ms, 716.806 ms and 321.651 ms before `Gate did not acquire its own new input`.

`CommonDynamicsUITests.h`, `NativeUITests.h`, `NativeMessageLoop.mm`, `CommonDynamicsPanel.h`, `DynamicsSpectrumPanel.h`, `EQSpectrumAnalyzer.h`, `PluginProcessor.cpp` and the Dynamics workflow are identical between the previous successful head, main and the reviewed head. The main differences from the previous successful head are Phase 12 code/tests and documentation; the Dynamics target definition is unchanged. No separate current-main Dynamics run was available: the latest-main comparison is source equality, not a claimed current-main test execution. The failing Gate assertion also precedes the test's Auto Set entry exercise. The newly changed `finishCab()` path is not being executed there.

The analyzer intentionally retires live raw bins after 120 ms without input. The test feeds finite offline bursts and then dispatches native UI events. An overlong dispatch can therefore make a valid completed Gate observation look empty at the later raw-bin assertion.

Independent analyzer reproduction at 48 kHz / 3 kHz:

- Fresh observation: 13 FFT frames, valid, raw bin 256 = -12.0412 dB.
- After 180 ms without input: still 13 frames, valid envelope, raw bin = -120 dB.
- Freeze of the last completed frame: valid, raw bin = -12.0412 dB.

An isolated copy of the unchanged native UI test with only `settle(180)` inserted after the Gate burst fails with the identical Gate error under Linux Xvfb/Openbox. Thus the test timing failure is reproducible independently of CAB placement changes. The original macOS failure did not log raw-bin/capture state at its assertion, so its exact internal state cannot be reconstructed; the same-source rerun and controlled reproduction support classification as an existing UI-test timing issue, not evidence of a CAB production regression.

## Minimal regression correction

`Tests/CommonDynamicsUITests.h` now deliberately exercises the 180 ms delay, freezes the last complete Gate frame, and requires:

1. A new FFT frame count beyond the pre-Gate-burst count.
2. A valid frozen observation.
3. The original 3 kHz / -35 dB input assertion.

The preceding old-callback rejection and empty-after-acknowledgement assertions are retained. The test unfreezes and acknowledges capture again before the subsequent hidden-view checks, so freezing cannot mask continued hidden FFT consumption. No retry loop, threshold relaxation, DSP change or new product test hook is introduced.

The final focused `ChimeraUITests.cpp` runner, compiled with the target's unchanged Release flags and linked modules, passes `--common-dynamics-only` under Linux Xvfb/Openbox in 58.877 s, including the deliberate 180 ms delay and resumed-capture hidden-view checks. The CMake `ChimeraUITests` target also builds successfully. `git diff --check` passes.

## State compatibility and remaining gates

Reviewed-head lifecycle run `41425791695` passed runtime and historical restoration on Windows, Linux and macOS. The raw historical-restore logs confirm released beta.2 fixture restoration, all host values/ordinals, A/B and personal IRs before/at/after expiry, plus resave/reopen. The reviewed production patch does not change saved IDs, ranges or serialization; this follow-up changes no production source.

The reviewed-head Open Beta run `41425791497` was still running when this follow-up was prepared. This follow-up commit requires its own current-head CI. Keep PR #250 in Draft until that CI is complete; do not carry the old head's green Dynamics rerun over to the new head.

Real A0/B0 bass DI, guitar DI, listening, and native DAW acceptance were not performed. Synthetic IR accuracy, ownership tests and state restoration are separate evidence and do not approve sonic quality or a public release.

## Reproduce

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCHIMERA_UI_PROBES=ON -DCHIMERA_BETA_QUALIFICATION_ONLY=ON
cmake --build build --target ChimeraAutoSetTests ChimeraCabLayoutModelTests ChimeraUITests --parallel 2
build/ChimeraAutoSetTests_artefacts/Release/ChimeraAutoSetTests --cab-edits
ctest --test-dir build -R '^(ChimeraAutoSetTests|ChimeraCabLayoutModelTests)$' --output-on-failure
python3 Tools/run_linux_ui_tests.py -- ctest --test-dir build -R '^Chimera(Common|Native)Dynamics(Queue)?UITests$' --output-on-failure
```

## Remote evidence

- https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/250
- https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41380883382
- https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41425791522
- Original macOS failure: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41425791522/job/125211957140
- Same-head macOS rerun: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41425791522/job/125253359624
- https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41425791695
- https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41425791497
