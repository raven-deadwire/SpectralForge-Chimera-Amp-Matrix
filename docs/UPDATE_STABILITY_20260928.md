# 2026-09-28 stability and IR browser changes

Baseline: `68ae65547e10b02df3118d5960c4859266cea272`. These are product code changes; the evidence below is Linux synthetic validation, not Windows DAW certification.

## Changes and evidence

- Transpose OFF and enabled/0 semitones retain input/dry history without FFT synthesis. A nonzero engagement fills the overlap-add window before fading to shifted audio. Existing steady-state pitch processing is unchanged.
- Comparison with the saved baseline headers: maximum steady-state sample difference **0** at 44.1/48/96 kHz for -12/-5/+7/+12 semitones. For 500 stereo blocks of 256 samples, bypass workload was 237.315 → 7.358 ms, 228.169 → 8.040 ms, and 263.548 → 6.974 ms respectively. These measure the transposer workload including synthetic input generation, not whole-plugin CPU.
- `PostRigGate` uses the clean input detector and a latency-aligned gain envelope. The integration point is after the rig and before POST delay/reverb. It is optional; the existing input gate remains the Legacy default. Synthetic tests check residual-noise attenuation, stereo linking, exact 0/37/2048-sample envelope delay, retained echo tails and unchanged legacy gate arithmetic.
- Opening the ordinary editor no longer creates an update-network worker. The service starts when Support is opened; configured automatic checking begins then. The Windows UI suite includes 30 editor/pending-popup/processor open/close cycles. This test has been authored but was not executed in the Linux headless environment.
- IR instrument and availability filters are independent. Scans expose invalid audio as non-loadable with a reason, and explicitly report truncation when more than 512 unique audio files are encountered. Factory/reference/installed counts retain their separate meanings; scanning never downloads or grants redistribution rights.

## Limits and remaining checks

- Transpose algorithm latency remains 2048 samples at 44.1/48 kHz and 4096 at 96 kHz: approximately 46.44/42.67/42.67 ms. No claim is made that the reported playing-latency or pitch artifacts are resolved.
- Closing an editor after opening Support still uses the existing cancellable network-worker join. Windows network cancellation and the reported Sonar/Cubase/Studio One project shutdown cases need actual host reproduction or a hang dump. Lazy creation is a reduction in exposure, not a demonstrated root-cause fix for issue #3.
- The legacy PRE GR display was already wired to its real compressor meter. No missing-meter defect has been confirmed. The global Output default is -6 dB; the reported approximately -10 dB change still requires stage-by-stage measurements with the reported preset and DI.
- IR scan validation is synchronous and bounded to 512 files, 4 MB per file and one second of audio. The loader validates selected files again. Platform UI layout, large-directory responsiveness and real cabinet/DI listening comparisons remain separate checks.

Primary regression entry points: `Tests/StabilityTests.cpp`, `Tests/IRCollectionTests.cpp`, and `checkEditorLifetime()` in `Tests/ChimeraUITests.cpp`. Local raw logs and source hashes are recorded in the execution evidence package.
