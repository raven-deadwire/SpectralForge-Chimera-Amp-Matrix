# Graphical EQ validation — 2026-10-10

Development scope: Draft PR #38, `codex/1.3.1-graphical-eq`, based on
`b7e4f18efa295f0babf2278ca63117fbccb54143`. No release approval or publication.
Source hashes are recorded in `evidence/graphical-eq-20261010/source-sha256.txt`.

## Local Linux evidence

The real Release Standalone and VST3 built successfully. The three focused
CTest contracts passed: legacy gate/parameter contract, static EQ DSP/state/
routing, and native EQ UI. Native UI screenshots were inspected at 75% and
100%; tests also render and check transformed dragging and bounds at 125/150%.
Final portability adjustments are rerun before commit publication.

The build metadata was configured at `5cbaed8936a3766d2c28bbe861c0cfdc3037c53f`
with subsequent source changes. This is a local working-tree test, not an
exact-final-commit binary certification. Exact-head builds run in GitHub CI.

### Audio and persistence coverage

- Real Bell, shelf, HP/LP/notch/band-pass responses, 44.1/48/96/192 kHz,
  mono/stereo; settled bypass is bit exact; no added latency.
- Both independent 12-node banks, rapid automation and filter changes,
  finite extreme outputs; no ordinary `new`/`new[]` allocations in instrumented
  EQ callbacks. Aligned allocations and native allocation APIs are not covered
  by that instrumentation.
- 122 appended host parameters, original ordinal and AU hint contracts,
  saved project/A/B audio, missing-parameter legacy defaults, all 48 factory
  presets retain bypass/disabled EQ defaults.
- Classic, Dual blend, Dual crossover and Matrix: captured node transfers,
  post-rig gate enabled, alternating loud/quiet input, actual POST compressor
  and stereo width compared against an independent existing-chain oracle.
  Final capture is compared against the actual host output-trim gain, allowing
  two float epsilons for cross-platform arithmetic.
- Both panels: native numeric input on all 24 nodes, seven filter choices,
  balanced host gestures, graph drag/wheel Q, independent bypass/enable,
  opposite-polarity stereo FFT calibration, hidden/closed analyzer suspension.

### Measured performance (shared Linux machine)

`evidence/graphical-eq-20261010/eq-cpu-linux.csv` records 24 cases, two EQs with
all 24 bands active. Each case has 40 warm-up and 600 measured blocks; analyzer
capture on/off, 48/96 kHz, blocks 64/128/512, mono/stereo. The 96 kHz/64/stereo
capture-on case measured p50 10.906 us and p99 25.048 us, versus a 666.667 us
buffer deadline (p99 3.76%). This is the EQ pair only, not the whole plugin.
Timing results are observations, not a real-time deadline certification.

UI work is measured separately on the message thread. Latest native UI run:
2048-point input/output stereo FFT work 195.345/183.928 us per respective panel.
Idle process CPU over one second, no audio: 75%=4.68125%, 100%=3.79352%,
125%=4.83069%, 150%=6.46222%, expressed as percentage of one core. Timer and
native window processing are included. No universal CPU threshold is claimed.

## Cross-platform CI

[Graphical EQ workflow](../.github/workflows/graphical-eq.yml) builds exact PR
head and runs the three focused contracts on Ubuntu, Windows and macOS.
JUnit, CTest logs, CPU CSV and four screenshots are uploaded per OS/head SHA.
Consult [PR #38 checks](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/38/checks)
for current results. A pending/failed run is not a passing result.

The first macOS run exposed exact-float assumptions in numeric gain and
unity output-trim test expectations. The updated macOS DSP contract passes. Tests now check gain with a tolerance smaller than parameter resolution and
compare output against the actual host trim. The numeric UI test logs values
for diagnosing any remaining platform issue. Linux passed the earlier CI run;
updated head must pass independently on all three OSes.

The second macOS run passes DSP/state/routing and exposes the native button
message not arriving within the test's fixed 5 ms delay. JUCE `triggerClick()`
posts an asynchronous command. Button tests now pump the message loop until
the actual host value arrives (bounded at two seconds), then retain the same
value assertion. Windows and Linux already pass; final head is rerun.

Existing product/CAB workflows remain enabled. Development release-policy
version/channel metadata matches 1.3.1 Preview; existing release requirements,
publication prohibition and hard gates are retained. Local catalog, CI map,
release scope, packaging-version and release producer/consolidator tests pass.

## Outstanding release acceptance and future work

Native DAW loading, AU/VST3 host automation sessions and real guitar listening
instruments have not been manually performed on all three OSes. They remain
release acceptance work; automated native-peer tests do not replace them.
Dynamic EQ, compressor/limiter expansion and independent four-band dynamics
are planned follow-ups, not implemented by this PR. No merge/release is made.
