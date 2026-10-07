# Niflheimr repeatable technical measurements

This harness reuses **ChimeraNiflheimrRender → Amplifier.h / AmpNativeDSP →
NiflheimrDSP**, including the shipped oversamplers and latency compensation.
It measures the current five native voices by their stable saved keys.
It does not alter voicings, parameter IDs, factory rigs, or release policy.
Every evidence file retains `release_approved: false`.

## Build and run

Requires the regular CMake/JUCE development dependencies and Python with NumPy
(the measurement workflow pins Python 3.12 / NumPy 2.2.6). Normal product builds
still do not require NumPy. Use an optimized Release build:

```sh
python -m pip install numpy==2.2.6
cmake -S . -B build-measurements -DCMAKE_BUILD_TYPE=Release -DCHIMERA_ENABLE_NIFLHEIMR_MEASUREMENTS=ON
cmake --build build-measurements --config Release --target ChimeraNiflheimrRender --parallel 2
ctest --test-dir build-measurements -C Release -R 'ChimeraNiflheimr(Renderer|Measurement)Tests' --output-on-failure
python Tools/measure_niflheimr.py --renderer build-measurements/ChimeraNiflheimrRender_artefacts/Release/ChimeraNiflheimrRender --output measurement-a --profile standard --machine-label studio-pc
```

Append `.exe` to the renderer on Windows. Use a new output directory each time.
Rerun CMake configure after changing commit or source; a stale renderer is
rejected against configured HEAD, the renderer translation unit and the complete
source-header fingerprint. A modified working tree is recorded rather than
misrepresented as a clean exact-head result. Binary and harness hashes are also
recorded. Rebuilds can differ in binary hash; retain both for audit.

`--rates`, `--gains` (normalized 0–1), `--factors` (1/2/4/8) and `--blocks`
can narrow or expand coverage. `--keep-audio` preserves head-only float WAVs;
otherwise their hashes remain in each render manifest. Inputs always remain.
No external instrument recordings or proprietary IRs are required.

| Protocol | Smoke (contract/CI) | Standard (investigation) |
| --- | --- | --- |
| Channels | All five | All five |
| Rates | 48 kHz | 44.1 / 48 / 96 kHz |
| GAIN | 7.5 | 5 / 7.5 / 10 |
| Oversampling | 1 / 4 | 1 / 2 / 4 / 8 |
| Nominal tones | 6011 Hz | 997 / 3001 / 6011 Hz |
| FFT length | 8192 | 32768 |
| Pre-analysis settling | At least 1 s | At least 2 s |
| Alias routes / CPU routes | 10 / 10 | 540 / 360 |
| CPU block sizes | 128 | 64 / 256 |
| CPU repeats / timed blocks per repeat | 2 / 64 | 5 / 512 |
| CPU warmup blocks per repeat | 384 | 1536 |

BLEND is fixed at **1** to expose the dirty path; other controls are native
channel defaults and all fourteen resolved values are written. Input peak is
−12 dBFS, stereo. These are synthetic stress probes, not bass-playing presets.
A seeded shuffled route order and actual execution order are retained. The
renderer visits its five channels in stable ordinal order within each route.
Smoke only validates coverage/plumbing and is too short for tail-latency claims.
Standard is broader but still does not cover every control or real-time state
transition. Quality/channel switching and whole-plugin CPU remain separate.

## Spectrum definition and boundaries

Each single tone is moved to an odd FFT bin (coprime to the power-of-two FFT
length). The exact bin/frequency, input WAV/hash, amplitude and analysis start
are retained. Both stereo lanes receive the same tone. The start follows the
settling interval plus the wrapper's reported latency; two adjacent windows
show whether residual transients still affect the result.

A periodic Hann window is applied. `power_fs2` is one-sided bin power, with
interior bins doubled and normalization by `N * sum(window**2)`. Summing bins
therefore gives window-weighted mean-square power. dBFS uses 10 log10(power):
a sine of peak 1 has total approximately −3.0103 dBFS. dBc is relative to the
measured fundamental (±2 bins). Values below 1e−30 power are floored at −300 dBFS;
this floor is numerical bookkeeping, not measurement resolution.

- **In-band harmonic mask:** ordinary harmonics strictly below Nyquist, ±2 bins.
- **Foldback candidate mask:** harmonics of orders 2–127 at/above Nyquist folded
  into baseband, ±2 bins, excluding DC and ordinary harmonic masks.
- **Off-harmonic residual:** all bins outside ordinary harmonics and DC.

Candidates can contain envelope/time-varying sidebands, finite settling,
numerical noise and input quantization as well as aliasing. True aliases that
coincide with an ordinary harmonic are excluded, and orders above 127 are not
exhaustive. The metric is thus **not isolated total alias distortion**, an
alias-free-reference null, or an audibility/quality PASS. Oversampling need not
monotonically improve every candidate metric, because the complete nonlinear
path and filtering also change. No higher oversampling factor is assumed to be
ground truth. Inspect full spectra, input floor, both windows, fundamentals and
residuals together. GAIN/oversampling comparisons use the same input and controls.

Every `.npz` contains `frequency_hz`, `power_fs2[window, stereo_lane, bin]`, input
spectrum, and all masks. JSON includes both windows/lanes, absolute dBFS and dBc.
`alias_oversampling_comparison` records left-lane two-window mean dBc differences
against the matching 1x route. Negative values mean less candidate energy
relative to the measured fundamental, not an acceptance verdict.

Numerical controls exercise a linear sine, genuine in-band third harmonic and a
known out-of-band third harmonic that folds into baseband. They verify both bin
classification and calibrated energy, rather than requiring the product to
meet a newly invented alias threshold.

## CPU method and provenance

The renderer's opt-in `--benchmark-repeats`, `--benchmark-blocks` and
`--benchmark-warmup-blocks` create a fresh production `Amp` per repetition.
A periodic stereo multitone is preloaded. The wrapper is prepared, selected,
reset and warmed before timing. Every measured interval encloses only
`Amp::process()` using `std::chrono::steady_clock`. Input copies, validation,
checksum, memory allocation by the harness, file I/O and statistics are outside
the timed interval. `juce::ScopedNoDenormals` is active. Checksums consume the
output and reject nonfinite results. No second DSP implementation is benchmarked.

Raw block microseconds and per-repeat median/p95/p99/max/mean are retained,
including outliers. Mean processing time / block audio budget and observed
over-budget block counts are descriptive; these are **not actual audio-device
underruns**. Timer overhead and scheduler preemption remain in the samples.
This is an offline head-only benchmark, not full-plugin, UI or commercial-DAW CPU.

Provenance includes source/configuration and hashes, compiler/version/build
flags, C++/JUCE versions, architecture/runtime, OS/CPU, logical CPU count,
affinity, Linux power governor/cgroup quota where available, start/end load,
Python/NumPy versions and allowlisted CI run/runner-image identifiers. Unsupported
fields are null/unknown. No serial numbers, credentials or arbitrary environment
dump are collected. Shared-runner thermal/turbo/contention effects remain unknown.

For a second compatible run:

```sh
python Tools/measure_niflheimr.py --renderer build-measurements/ChimeraNiflheimrRender_artefacts/Release/ChimeraNiflheimrRender --output measurement-b --profile standard --machine-label studio-pc --baseline measurement-a/evidence.json
```

The comparison requires identical protocol/configuration, CPU/environment class,
compiler flags, complete CPU route coverage, input hashes and resolved controls.
Mismatches produce `NOT_COMPARABLE` with reasons. Matching runs report each
current/baseline median ratio and overlap of repeat-median ranges. Changes in
source and binary hashes are expected for a code regression investigation;
there is **no hard CPU PASS/FAIL or automatic regression verdict**, even when
provenance matches. Runs are unpaired and two matching shared-runner instances
are not necessarily the same physical host. Reproduce a suspected regression
on stable target hardware, preferably interleaving baseline/candidate runs.

## Evidence and automation

`evidence.json` has schema `spectralforge.niflheimr.measurements.v1`, a versioned
protocol, full configuration/provenance, per-route `alias`/`cpu` arrays, comparison,
manual acceptance and an SHA-256 inventory of all supporting files. Each render
manifest holds raw CPU arrays and fourteen controls per channel. Results are
written in staging and published only after all requested routes complete;
invalid/stale/nonfinite/incomplete runs fail without a success evidence file.

The dedicated `Niflheimr technical measurements` workflow runs smoke on relevant
PR changes and accepts smoke/standard manual dispatch on Linux/Windows/macOS.
Artifacts retain JSON, inputs, spectra, raw timing manifests and test logs.
Successful CI means the harness completed and its contracts held. The workflow
has read-only repository permissions and no release/publisher integration.

Actual bass DI, common IR/level-matched listening, musical acceptance, commercial
DAW lifecycle, Windows/macOS product acceptance and target-hardware CPU approval
remain explicit `PENDING_MANUAL_EVIDENCE`. Neither this harness nor its CI job can
promote `release_approved`, even if every technical measurement completes.
