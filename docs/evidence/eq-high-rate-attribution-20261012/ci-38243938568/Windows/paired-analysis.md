# Windows-native-CI EQ paired-artifact analysis

Artifact: `/workspace/scratch/af7c830474b5/ci-artifacts/windows-72e361ba`

Native/DAW performance acceptance: **NOT_ASSESSED**.

## Dataset `.`

CSV checks: **PASS**. CTest receipt: **PASSED**.

Source: `72e361ba91ea2c6b8d830921a71275fa25cba493`.

Rows: {'summary': 48, 'paired': 384, 'parity': 108}. Lower candidate update medians: **48 / 48**.

Median update reduction: **5.632%**. CPU reduction: **QUANTIZATION_LIMITED**; raw observations and arithmetic ratios are preserved, but no precise CPU-saving percentage is established.

### Actual thread CPU counter limitation

Observed raw CPU step: **15.625 ms**; zero-valued raw observations: **1**. CPU medians decrease / tie / increase in 15 / 24 / 9 cells. Non-lower measured CPU medians include both ties and increases; these are quantization-limited counter observations, not precision CPU conclusions.

Four-value medians may have half-step values because the middle two counter observations are averaged. That does not improve the timer's observed resolution.

| Raw CPU observation, ms | Count |
|---:|---:|
| 0.0 | 1 |
| 15.625 | 143 |
| 31.25 | 95 |
| 46.875 | 45 |
| 62.5 | 26 |
| 78.125 | 5 |
| 93.75 | 30 |
| 109.375 | 33 |
| 125.0 | 6 |

Zero CPU example: 44100 Hz, FFT 4096, full_capacity_catchup, sequence 4, candidate: update wall **14.5834 ms**, feed + analysis wall **14.715 ms**, observed CPU **0 ms**. This is not zero-cost execution.

### 192 kHz: 2-second cadence trials and one full-capacity catch-up

| FFT | Fixture / cadence | FFT windows | Update baseline → candidate, ms | Wall reduction | CPU baseline → candidate, ms | CPU reduction |
|---:|---|---:|---:|---:|---:|---:|
| 2048 | 20 ms cadence | 750 | 107.666 → 93.977 | 12.715% | 109.375 → 93.750 | Resolution limited |
| 2048 | 40 ms cadence | 750 | 102.167 → 96.315 | 5.728% | 101.562 → 93.750 | Resolution limited |
| 2048 | 80 ms cadence | 750 | 104.446 → 98.564 | 5.632% | 109.375 → 101.562 | Resolution limited |
| 2048 | full-capacity catch-up | 128 | 17.510 → 16.643 | 4.951% | 15.625 → 15.625 | Resolution limited |
| 4096 | 20 ms cadence | 375 | 93.529 → 87.052 | 6.926% | 93.750 → 93.750 | Resolution limited |
| 4096 | 40 ms cadence | 375 | 97.267 → 87.834 | 9.698% | 93.750 → 93.750 | Resolution limited |
| 4096 | 80 ms cadence | 375 | 95.829 → 88.170 | 7.992% | 93.750 → 93.750 | Resolution limited |
| 4096 | full-capacity catch-up | 64 | 15.584 → 15.342 | 1.554% | 15.625 → 15.625 | Resolution limited |
| 8192 | 20 ms cadence | 187 | 116.469 → 109.920 | 5.623% | 109.375 → 109.375 | Resolution limited |
| 8192 | 40 ms cadence | 187 | 113.889 → 107.469 | 5.637% | 117.188 → 109.375 | Resolution limited |
| 8192 | 80 ms cadence | 187 | 113.739 → 105.283 | 7.435% | 117.188 → 109.375 | Resolution limited |
| 8192 | full-capacity catch-up | 32 | 20.243 → 18.226 | 9.964% | 15.625 → 15.625 | Resolution limited |

### Preservation and accounting

Parity: 108 cases; maximum raw/envelope/display error = 1.52587890625e-05 / 1.52587890625e-05 / 1.52587890625e-05 dB.

The 108 CSV rows are the four-rate, three-FFT-size, three-cadence, three-smoothing A/B spectral parity grid. The separate C++ checkFiniteRecovery() check covers nonfinite/overflow containment and reset-then-fresh-spectrum recovery at 192 kHz with three FFT sizes; it is not included in these 108 rows.

Cost FIFO drops: 0; parity FIFO drops/discontinuities: 0 / 0; entire cost trials with zero windows: 0.

### Interpretation

- Short Windows GetThreadTimes observations may be coarse/quantized. A zero baseline or candidate CPU median yields no CPU percentage; inspect raw CPU trials and wall timings.
- GetThreadTimes CPU observations are QUANTIZATION_LIMITED. A numerical 0% median reduction or a 0 ms individual observation is not evidence of equal cost or free execution.
- CPU milliseconds include FIFO feeding and analysis. FFT-only wall measurements have different magnitude-pass scope across variants; use complete-update wall cost.
- Timing reductions are observations, with no performance threshold or statistical significance gate. Shared-runner variation remains visible in raw trials.
- FIFO pending/discarded values are asserted by the C++ test but absent from cost CSV columns; do not invent separate observed values.
- Zero-FFT consumer updates are legitimate when a batch is shorter than one hop. Consumer totals include them; no zero-update timing row is discarded.
- CSV consistency PASS is separate from the CTest process result and from untested DAW/audio-interface real-time acceptance.

## Independent CTest and functional receipt verification


Receipt consistency: **PASS**. These checks read completed artifacts; no benchmark was rerun.

Source: `72e361ba91ea2c6b8d830921a71275fa25cba493`. Runner: `Windows-2025Server-10.0.26100-SP0`.

Build-selected FFT engine from JUCE 8.0.8 source/default configuration: FFTFallback; artifact CSV does not directly name the engine.


| Actual CTest record | Status | Seconds |
|---|---|---:|
| ChimeraGateProcessorTests | PASSED | 12.4138 |
| ChimeraGraphicalEQTests | PASSED | 35.8179 |
| ChimeraGraphicalEQUITests | PASSED | 119.2560 |
| ChimeraEQAnalyzerTests | PASSED | 20.8219 |
| ChimeraEQAnalyzerPerformanceTests | PASSED | 20.5527 |
| ChimeraEQHighRateAttributionTests | PASSED | 151.6120 |


All six dedicated CSV grids below cover 44.1, 48, 96, and 192 kHz.


| Receipt | Rows | Independently checked result |
|---|---:|---|
| Temporal coverage | 24 | All bounded attacks > −60 dB; lowest -33.5502 dB; exact hop counts; zero drops |
| Partition invariance | 180 | Max raw/envelope/pixel errors {'max_raw_error_db': 0.0, 'max_envelope_error_db': 0.0, 'max_pixel_error_db': 0.0}; zero drops |
| FIFO stalls | 48 | Exact produced = consumed + discarded + dropped + pending; 45 lossless cases; 3 deliberate overflow/recovery cases |
| Whole-processor audio invariance | 12 | Bit-identical ON/OFF audio, error 0, host latency 42 samples; deliberate overflow exercised |
| Audio callback fixture | 48 | 28800 callbacks; allocation count 0; observed overruns 0 |
| Analyzer cadence fixture | 12 | 160 measured calls per cell; zero-call updates retained; zero FIFO drops |


The zero-FFT first 40 ms observations at 44.1/48 kHz with 8192 FFT are expected: the 2048-sample hop exceeds the 1764/1920-sample UI batch. Every attack is present within the explicit bounded observation tail.


The 192 kHz × 500 ms stall intentionally produces 96,000 samples for a 65,536-frame FIFO: 30,464 drops and 65,536 discarded stale frames are verified in each FFT size, followed by fresh-window recovery. The audio-invariance fixture separately forces overflow while preserving output. These are not lossless-input cases.


The generic `eq-cpu.csv` has only 48/96 kHz and legacy `eq-analyzer-full-callback.csv` only 48 kHz; they are not used to claim four-rate coverage. Native UI/producer attribution belongs to its separate report.


## Retained non-improving paired observations


0 of 48 cells have a candidate update median at least as high as baseline. No cell is excluded. Negative reduction values below mean increased measured cost.


| Rate | FFT | Fixture | Update reduction % | Actual thread CPU reduction % | Adjacent-pair update reductions % |
|---:|---:|---|---:|---:|---|
| — | — | None | — | — | — |


The following cells improve update wall time but have non-lower measured thread CPU medians (ties or increases; the CPU measurement includes FIFO feed):

These Windows counter observations are quantization-limited. A 0% ratio is a tie in the measured CPU medians, not a CPU increase or evidence of equal execution cost.

44100 Hz, FFT 2048, full_capacity_catchup: update reduction 10.979%, CPU reduction 0.000%.

44100 Hz, FFT 2048, 20: update reduction 10.303%, CPU reduction 0.000%.

44100 Hz, FFT 2048, 40: update reduction 5.330%, CPU reduction 0.000%.

44100 Hz, FFT 2048, 80: update reduction 3.579%, CPU reduction 0.000%.

44100 Hz, FFT 4096, full_capacity_catchup: update reduction 7.405%, CPU reduction 0.000%.

44100 Hz, FFT 8192, full_capacity_catchup: update reduction 2.713%, CPU reduction 0.000%.

44100 Hz, FFT 8192, 40: update reduction 3.404%, CPU reduction -100.000%.

44100 Hz, FFT 8192, 80: update reduction 6.100%, CPU reduction -33.333%.

48000 Hz, FFT 2048, full_capacity_catchup: update reduction 1.769%, CPU reduction 0.000%.

48000 Hz, FFT 2048, 20: update reduction 4.887%, CPU reduction -100.000%.

48000 Hz, FFT 2048, 40: update reduction 4.603%, CPU reduction -100.000%.

48000 Hz, FFT 4096, full_capacity_catchup: update reduction 6.432%, CPU reduction 0.000%.

48000 Hz, FFT 4096, 20: update reduction 4.212%, CPU reduction -33.333%.

48000 Hz, FFT 4096, 40: update reduction 4.684%, CPU reduction 0.000%.

48000 Hz, FFT 8192, full_capacity_catchup: update reduction 5.499%, CPU reduction 0.000%.

48000 Hz, FFT 8192, 20: update reduction 5.650%, CPU reduction -100.000%.

48000 Hz, FFT 8192, 40: update reduction 2.970%, CPU reduction 0.000%.

48000 Hz, FFT 8192, 80: update reduction 3.979%, CPU reduction 0.000%.

96000 Hz, FFT 2048, full_capacity_catchup: update reduction 5.096%, CPU reduction 0.000%.

96000 Hz, FFT 2048, 20: update reduction 5.365%, CPU reduction -16.667%.

96000 Hz, FFT 2048, 80: update reduction 4.122%, CPU reduction -16.667%.

96000 Hz, FFT 4096, full_capacity_catchup: update reduction 4.401%, CPU reduction 0.000%.

96000 Hz, FFT 4096, 20: update reduction 5.632%, CPU reduction 0.000%.

96000 Hz, FFT 4096, 40: update reduction 5.350%, CPU reduction 0.000%.

96000 Hz, FFT 8192, full_capacity_catchup: update reduction 7.312%, CPU reduction 0.000%.

96000 Hz, FFT 8192, 40: update reduction 2.292%, CPU reduction -14.286%.

192000 Hz, FFT 2048, full_capacity_catchup: update reduction 4.951%, CPU reduction 0.000%.

192000 Hz, FFT 4096, full_capacity_catchup: update reduction 1.554%, CPU reduction 0.000%.

192000 Hz, FFT 4096, 20: update reduction 6.926%, CPU reduction 0.000%.

192000 Hz, FFT 4096, 40: update reduction 9.698%, CPU reduction 0.000%.

192000 Hz, FFT 4096, 80: update reduction 7.992%, CPU reduction 0.000%.

192000 Hz, FFT 8192, full_capacity_catchup: update reduction 9.964%, CPU reduction 0.000%.

192000 Hz, FFT 8192, 20: update reduction 5.623%, CPU reduction 0.000%.


All raw repetitions remain in the paired report JSON and source CSV. Mixed signs alone do not establish a scheduling cause or disprove a real regression. Wall and thread CPU measure different scopes: update wall excludes FIFO feed; the thread CPU observation includes it.


Physical DAW/audio-interface performance remains **NOT_ASSESSED**. Source and measurements remain attached to this run, separate from minimal local, full-linked local, or subsequent CI results.

## Prior Windows execution

The earlier Windows job for run `38240150849`, source `ddb0880`, remains **NOT_RUN** for tests: its preservation/EOL provenance gate failed after build and before tests. No first-run Windows cost or functional PASS is inferred. The parent task verified that execution state; this report analyzes only the completed second-run artifact.
