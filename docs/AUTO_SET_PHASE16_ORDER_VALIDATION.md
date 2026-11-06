# Auto Set Phase 16 — POST order regression review

Review requested 2026-11-05. Baseline: PR #267, `a2841df2ee7dc453ecb87b6a2e691ac53a5bb81f`.
The user approved this seven-file change for PR #267 after reviewing the local diff and regression results. The validation below was completed before publication; the CI snapshot refers to the baseline and dependency SHAs, not the new fix commit. Merge and release are outside this approval.

## Measured defect

`PluginProcessor.cpp` captures `Tap::post` before `postFX.process`. `AutoSetBus.h` feeds that capture into the Console VCA alone. A preamp or EQ preceding the compressor changes its actual input without changing the analysed input. This is a reproduced numerical defect, not just a suspected listening issue.

The regression compiles the repository's actual `PostFXChain`, `PostNativeDSP`, and `analyseBus` with JUCE 8.0.8. It observes the real compressor boundary using the existing `DynamicsAnalysis::Scope` in `PostFXChain::processRack`. It does not estimate compressor output from a simplified transfer equation. Samples are drained after every 256-sample stereo block; both channels carry the same synthetic signal and the capture asserts no dropped samples.

180 matrix cases: 44.1/48/96 kHz × Clean/Rock/Metal × initially off/already on × Strength 100/50% × five POST configurations. Input is four seconds of deterministic plucked notes (E2-root pentatonic notes, eight harmonics, soft/hard picking); the first 0.5 s is excluded. GR uses the product's 20 ms loud-playing block definition. The initial already-on setting is threshold −40 dB, make-up 5 dB, attack 0.1 ms, release 0.1 s, ratio 10:1. Existing attack/release/ratio are asserted unchanged.

EQ: native Console Four, 600 Hz bell, Q 0.5, ±12 dB. Preamp: native Iron Colour, LINE +15 dB or LINE −10 dB. Native bank trim/level are 0 dB; auto-fade and external sidechain are off. Other POST effects are bypassed. The reordered module moves ahead of dynamics without changing its parameter bank.

Representative result at 48 kHz, Metal, initially off, Strength 100% (dB):

| POST condition | Predicted GR | Actual GR | Error, actual − predicted | Bus level residual after make-up | Correct threshold using actual input |
|---|---:|---:|---:|---:|---:|
| Default, dynamics first | 3.129 | 3.129 | 0.000 | +0.194 | −21.5 |
| EQ +12 before dynamics | 3.129 | 7.306 | +4.176 | −4.051 | −16.0 |
| EQ −12 before dynamics | 3.129 | 1.306 | −1.823 | +1.760 | −24.5 |
| Preamp +15 before dynamics | 3.129 | 14.494 | +11.365 | −11.384 | −7.0 |
| Preamp −10 before dynamics | 3.129 | 0.030 | −3.100 | +2.482 | −31.0 |

The baseline applies the same −21.5 dB threshold and +2.5 dB make-up to all five off/Metal cases. The signed residual is the mean output-minus-input block level at the compressor boundary, not a LUFS result and not the final plug-in output. Negative means insufficient compensation for the actual attenuation. Changing only the make-up cannot restore the intended dynamics.

All 144 reordered cases exceed 0.5 dB prediction error. Across the matrix, the maximum prediction error is 12.013489 dB (96 kHz, Metal, already on, preamp +15). The actual GR is 15.077288 dB versus the prediction 3.063799 dB. Make-up residual ranges from −11.390672 dB to +2.482366 dB. Default-order prediction error is 0 within this fixture.

An additional already-on/on-target fixture reproduces a false keep: the analyser reports no change and predicts 3.12942 dB while the reordered rack produces 14.4941 dB (48 kHz, Metal, preamp +15). Thus the existing-compressor tolerance check does not protect reordered paths.

## Compared approaches

| Approach | Evidence and behavior | Remaining work |
|---|---|---|
| Analyse the actual input at the compressor boundary | Experimentally rendered the real rack, captured its compressor input, then re-analysed that signal. All 180 predictions match the rerendered DSP within 0.01 dB (observed 0 to six decimals); all style/Strength targets are within 0.5 dB (maximum target error 0.174727 dB); make-up residual magnitude is at most 0.249974 dB. | Product integration needs stereo capture, buffer/latency and route-transition handling, capture invalidation, and protection from changes applied upstream after capture. No production tap change is included here. |
| Replay every preceding POST module offline | Could reproduce a fixed native preamp/EQ prefix using the same DSP. | Must reproduce model/engine morphing, preamp alignment, effect state, creative/time effects, stereo and sidechains. A fresh mono prefix is not universally equivalent to the running rack. Not implemented or qualified here. |
| Keep unsupported orders unchanged — proposed immediate fix | Require a valid order with dynamics first. Any earlier module, even currently bypassed, causes a kept result. No order is rewritten and no bus control is changed. | Deliberately leaves reordered rigs without bus Auto Set until the actual-input path is qualified. Existing mono and same-pass de-esser approximations remain. |

The conservative guard is the proposed immediate change. It prevents the demonstrated wrong application without introducing a new real-time capture path.

## Proposed code and preservation

- `Source/AutoSetController.h`: validate POST order; track revisions for order slots and Console VCA engine/model/extension/bypass/trim/level/all controls; reject listening-time edits including A-B-A; apply the bus controls as a guarded group; on synchronous host interference restore only still-owned automatic writes.
- `Tests/AutoSetBusOrderDSPTests.cpp`: independent real rack measurement, CSV output, actual-input alternative and false-keep reproduction.
- `Tests/AutoSetBusOrderControllerTests.h`: unsupported-order preservation, edit-history and begin/value/end host callback rollback tests, including existing compressor state and undo.
- `Tests/AutoSetTests.cpp`: expose `--bus-order` and `--bus-edits` and include them in the full suite.
- `CMakeLists.txt`, `.github/workflows/auto-set-level.yml`: build and run the real-rack DSP regression in the existing three-platform Auto Set workflow.

Read-only dependencies are tracked separately from the write/undo list. Lock selection and Strength target calculation are unchanged. The analyser and DSP algorithms are unchanged. Unsupported orders retain both their order and every setting. User-owned edits are never reclaimed by rollback or UNDO AUTO.

## Validation status

Locally executed on Linux/GCC 13.3:

| Test | Original controller | Proposed controller |
|---|---|---|
| `--bus-order` | FAIL: unsupported POST order applied a correction | PASS, 24 order and dependency-history cases |
| `--bus-edits` | FAIL: bus host edit left partial correction | PASS, 36 synchronous begin/value/end host callback cases |
| Existing `--bus` | Not rerun for this comparison | PASS: rules at 44.1/48/96 kHz; real processor application, Strength, locks, model/key exclusions, manual edit and UNDO |

The original-controller binary uses the same new regression tests with the unmodified `a2841df` controller. The proposed-controller binary was rebuilt through the repository's `ChimeraAutoSetTests` CMake target. All checks compare actual parameter states; rollback tests preserve the lasting user edit and check every parameter before and after UNDO. DSP matrix: 180 rows PASS; the additional false-keep reproduction also PASS. The final source was also built as the new CMake DSP target and rerun: exit 0, all 180 rows numerically identical to the independent manual build, and the false-keep reproduction passes. The full Auto Set suite, Windows and macOS were not rerun locally on the proposed patch.

Reproduce in a normal configured project with `cmake --build build --target ChimeraAutoSetTests ChimeraAutoSetBusOrderDSPTests`, then run `ChimeraAutoSetBusOrderDSPTests`, `ChimeraAutoSetTests --bus`, `--bus-order`, and `--bus-edits`. The full Auto Set suite includes the new controller cases.

The local DSP measurement used GCC 13.3, C++20, `-O2 -ffp-contract=off`, JUCE 8.0.8 (`d6181bde38d858c283c3b7bf699ce6340c050b5d`). It directly linked the production DSP headers to JUCE core/audio-basics/audio-formats/DSP and the official JUCE Colour translation unit, without substituting DSP implementations. Full processor validation uses the repository CMake target.

## CI and stacked dependencies

- #267: open, HEAD `a2841df`; tested parent #266 was `d994a12`. The metadata wrapper initially returned `mergeable: false`; the later raw PR response returned `mergeable: true`, `mergeable_state: unstable`. This changing flag is not proof that the newer dependency heads are integrated. The dedicated Auto Set CI passes Windows/Linux/macOS; Open Beta, AAX and Update Candidate also succeeded. Full CI is not wholly green: OBS macOS attempt 1 failed after 48 kHz/5.1 recording during shutdown; its rerun is queued. Signal-path integration is still in progress in the latest snapshot.
- The failed job log directly shows `EXC_BAD_ACCESS`/`SIGSEGV`, `_platform_strlen`, obs-websocket, `signal_handler_signal`, `obs_sceneitem_remove_internal`, `scene_destroy`, and `tiny_tubular_task_thread`. No Chimera frame appears in the printed crashing stack. That establishes the observed failure location, not proof that Chimera cannot contribute to the failure.
- #266: open, latest HEAD `fac9a6c`; its listed base snapshot is the older #265 commit `ca38c17`. The new commit changes the LOW-cut ceilings to 55 Hz guitar / 25 Hz bass, adds Drop A regressions and updates documentation (four files). It is not present in tested #267. Its Auto Set workflow and most other jobs are queued. The older `d994a12` results must not be attributed to this new HEAD. Workflow responses are first-page-only.
- #265: open, current HEAD `69dd4d2`, base main `b4f85b3`. Auto Set is successful, but some other workflows are still pending. This newer head merges main and its EQ peak-path changes; #267 still contains the older `ca38c17` lineage. Both #265 and #266 therefore have newer changes absent from the tested #267 SHA.
- #264 is merged into main as squash `b4f85b3`. The #267 lineage contains the original Phase 13 commits, but not the newer main EQ peak-path change. Refresh the stack and rerun on the final integrated HEAD before merge. Do not transfer the current SHA's test evidence automatically.

The `percentile` review allegation is not a defect: `Source/AutoSet.h:130` takes `std::vector<float> values` by value, and sorts its own copy. No unnecessary copy was added.

## Limits

No real instrument, listening, DAW, or actual device test was performed. Synthetic real-DSP measurements demonstrate a numerical and control-application defect; audible pumping, tone damage, and the final loudness impact were not measured. Stereo-asymmetric inputs, sustained pre-existing DSP state, modulation/creative/delay/reverb before the compressor, external keys, nonzero bank trim/level, auto-fade, and changes from the earlier de-esser step are not qualified by this matrix. The proposed guard does not claim to solve every pre-existing Phase 16 approximation.

Source and CI links: [PR #267](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/267), [Auto Set CI](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41975298507), [OBS CI](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41975298417), [baseline controller](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/a2841df2ee7dc453ecb87b6a2e691ac53a5bb81f/Source/AutoSetController.h), [baseline DSP](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/a2841df2ee7dc453ecb87b6a2e691ac53a5bb81f/Source/FXChain.h).
