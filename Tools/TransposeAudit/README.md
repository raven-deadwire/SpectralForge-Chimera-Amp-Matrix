# Pinned PR125 / PR128 Transpose audit

Research/verification only. No Source DSP, product state/PDC, version, workflow,
release policy, or approval changes. The 10 ms requirement remains unchanged.

Run on PR128 commit `9d28b06c3cb6498724d6618d08bebac1771f63eb` with these audit
files. The build reads the actual public `3bec6c70...` and PR125 `1ef032c...`
headers via `git show`; only namespace names change to coexist in one executable.
PR128 `corr40-split-rt` is compiled with all four options enabled. Python never
passes note frequencies or event labels into DSP. The generated `wrapper125.h`
is the exact Transposer class excerpt from PR125; full header hashes are recorded.

Use pinned JUCE 8.0.8, GCC 13, NumPy/SciPy, matplotlib and zlib headers.

```sh
python Tools/TransposeNext/build.py --juce ../JUCE --out build/transpose-audit
python Tools/TransposeAudit/build.py --juce ../JUCE --out build/transpose-audit
build/transpose-audit/CorrelationTests
build/transpose-audit/PitchRegressionTests
build/transpose-audit/PitchRegressionTests125
build/transpose-audit/StabilityTests
build/transpose-audit/GateTests
# This newly extended test FAILS on the frozen RT candidate. Do not hide it.
build/transpose-audit/CorrelationTestsRT
python Tools/TransposeAudit/compare.py --renderer build/transpose-audit/AuditProbe --out build/transpose-audit/quality
python Tools/TransposeAudit/partition_audit.py --renderer build/transpose-audit/AuditProbe --out build/transpose-audit/partition
# Only after every build/quality/functional process has ended, run timing alone:
python Tools/TransposeAudit/compare.py --renderer build/transpose-audit/AuditProbe --out build/transpose-audit/timing --timing
python Tools/TransposeAudit/compare.py --renderer build/transpose-audit/AuditProbeFTZ --out build/transpose-audit/timing-ftz --timing --pin-cpu
python Tools/TransposeAudit/report.py
```

The first timing pass preserves the existing research harness behavior. The
second matches `PluginProcessor::processBlock()`'s `ScopedNoDenormals`, and pins
the process to one available vCPU. Both datasets remain available. They are
sequential, not simultaneous; do not attribute their difference solely to FTZ.
No functional/DSP code was tuned on these results. Quality uses the original
probe convention for comparability; each result records its renderer SHA.

Core suite: 48 kHz, 128 frames, stereo R=-0.65L, B0/A0/A1, -2/-1/+1/+2.
219 renders include weak fundamentals, repeated and alternating picks, fresh
seed 2026102307, impulse phases, tremolo, block/default/wrapper checks. The
separate block diagnostic adds 120 renders at 44.1/48/96 kHz. Each timing pass
adds 117 renders (three order-rotated repeats, 18 seconds per render).

`quality.json` retains all event lags, tone metrics and controls. `comparison.csv`
aggregates only stationary harmonic pitch/warble and regular/alternating picks;
`per-condition.csv` exposes individual shifts and weak-fundamental cases.
Low-band energy centroid is the existing 120 Hz filtered proxy, not a physical
transport delay or perceptual threshold. Known-delay controls expose event-window
bias. Warble is a demodulation proxy and is not an isolated listening rating,
especially if the target component is weak/detuned or deliberately modulated.

No actual DI, original Neural DSP capture, DAW PDC/automation/recall, physical
round trip, chord quality, full plugin CPU or target hardware soak acceptance.
The report does not turn narrow estimator experiments into a universal
mathematical impossibility theorem. Do not merge this research branch into a
release solely because nominal latency is 480 samples.
