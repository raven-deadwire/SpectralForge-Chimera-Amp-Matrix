# macOS-native-CI EQ paired-artifact analysis

Artifact: `/workspace/scratch/af7c830474b5/ci-artifacts/macos-ddb0880`

Native/DAW performance acceptance: **NOT_ASSESSED**.

## Dataset `.`

CSV checks: **PASS**. CTest receipt: **PASSED**.

Source: `ddb0880bd61a4c794236783b869767b96208bf41`.

Rows: {'summary': 48, 'paired': 384, 'parity': 108}. Lower candidate update medians: **39 / 48**.

Median update reduction: **14.642%**. Median CPU-cost reduction: **11.031%**, from 48 comparable cells.

### 192 kHz: 2-second cadence trials and one full-capacity catch-up

| FFT | Fixture / cadence | FFT windows | Update baseline → candidate, ms | Wall reduction | CPU baseline → candidate, ms | CPU reduction |
|---:|---|---:|---:|---:|---:|---:|
| 2048 | 20 ms cadence | 750 | 46.333 → 40.154 | 13.335% | 55.546 → 50.249 | 9.536% |
| 2048 | 40 ms cadence | 750 | 44.671 → 36.836 | 17.540% | 50.851 → 44.724 | 12.049% |
| 2048 | 80 ms cadence | 750 | 38.024 → 31.514 | 17.121% | 45.477 → 39.122 | 13.974% |
| 2048 | full-capacity catch-up | 128 | 5.248 → 7.169 | -36.591% | 6.891 → 8.723 | -26.586% |
| 4096 | 20 ms cadence | 375 | 37.836 → 33.306 | 11.975% | 49.249 → 43.965 | 10.729% |
| 4096 | 40 ms cadence | 375 | 31.411 → 28.116 | 10.489% | 46.764 → 44.087 | 5.724% |
| 4096 | 80 ms cadence | 375 | 34.802 → 31.238 | 10.240% | 46.669 → 42.653 | 8.606% |
| 4096 | full-capacity catch-up | 64 | 6.484 → 4.226 | 34.831% | 7.766 → 5.409 | 30.354% |
| 8192 | 20 ms cadence | 187 | 34.304 → 28.343 | 17.377% | 46.136 → 40.442 | 12.342% |
| 8192 | 40 ms cadence | 187 | 30.320 → 27.722 | 8.569% | 45.543 → 44.400 | 2.510% |
| 8192 | 80 ms cadence | 187 | 34.049 → 29.821 | 12.417% | 48.168 → 41.350 | 14.155% |
| 8192 | full-capacity catch-up | 32 | 6.414 → 5.044 | 21.364% | 9.205 → 7.144 | 22.385% |

### Preservation and accounting

Parity: 108 cases; maximum raw/envelope/display error = 1.52587890625e-05 / 1.52587890625e-05 / 1.52587890625e-05 dB.

The 108 CSV rows are the four-rate, three-FFT-size, three-cadence, three-smoothing A/B spectral parity grid. The separate C++ checkFiniteRecovery() check covers nonfinite/overflow containment and reset-then-fresh-spectrum recovery at 192 kHz with three FFT sizes; it is not included in these 108 rows.

Cost FIFO drops: 0; parity FIFO drops/discontinuities: 0 / 0; entire cost trials with zero windows: 0.

### Interpretation

- CPU milliseconds include FIFO feeding and analysis. FFT-only wall measurements have different magnitude-pass scope across variants; use complete-update wall cost.
- Timing reductions are observations, with no performance threshold or statistical significance gate. Shared-runner variation remains visible in raw trials.
- FIFO pending/discarded values are asserted by the C++ test but absent from cost CSV columns; do not invent separate observed values.
- Zero-FFT consumer updates are legitimate when a batch is shorter than one hop. Consumer totals include them; no zero-update timing row is discarded.
- CSV consistency PASS is separate from the CTest process result and from untested DAW/audio-interface real-time acceptance.

## Independent CTest and functional receipt verification


Receipt consistency: **PASS**. These checks read completed artifacts; no benchmark was rerun.

Source: `ddb0880bd61a4c794236783b869767b96208bf41`. Runner: `macOS-15.7.9-arm64-arm-64bit-Mach-O`.

Build-selected FFT engine from JUCE 8.0.8 source/default configuration: AppleFFT / vDSP; artifact CSV does not directly name the engine.


| Actual CTest record | Status | Seconds |
|---|---|---:|
| ChimeraGateProcessorTests | PASSED | 29.4185 |
| ChimeraGraphicalEQTests | PASSED | 37.9643 |
| ChimeraGraphicalEQUITests | PASSED | 185.2280 |
| ChimeraEQAnalyzerTests | PASSED | 25.9148 |
| ChimeraEQAnalyzerPerformanceTests | PASSED | 11.4758 |
| ChimeraEQHighRateAttributionTests | PASSED | 175.8920 |


All six dedicated CSV grids below cover 44.1, 48, 96, and 192 kHz.


| Receipt | Rows | Independently checked result |
|---|---:|---|
| Temporal coverage | 24 | All bounded attacks > −60 dB; lowest -33.5502 dB; exact hop counts; zero drops |
| Partition invariance | 180 | Max raw/envelope/pixel errors {'max_raw_error_db': 0.0, 'max_envelope_error_db': 0.0, 'max_pixel_error_db': 0.0}; zero drops |
| FIFO stalls | 48 | Exact produced = consumed + discarded + dropped + pending; 45 lossless cases; 3 deliberate overflow/recovery cases |
| Whole-processor audio invariance | 12 | Bit-identical ON/OFF audio, error 0, host latency 42 samples; deliberate overflow exercised |
| Audio callback fixture | 48 | 28800 callbacks; allocation count 0; observed overruns 1 |
| Analyzer cadence fixture | 12 | 160 measured calls per cell; zero-call updates retained; zero FIFO drops |


The zero-FFT first 40 ms observations at 44.1/48 kHz with 8192 FFT are expected: the 2048-sample hop exceeds the 1764/1920-sample UI batch. Every attack is present within the explicit bounded observation tail.


The 192 kHz × 500 ms stall intentionally produces 96,000 samples for a 65,536-frame FIFO: 30,464 drops and 65,536 discarded stale frames are verified in each FFT size, followed by fresh-window recovery. The audio-invariance fixture separately forces overflow while preserving output. These are not lossless-input cases.


The generic `eq-cpu.csv` has only 48/96 kHz and legacy `eq-analyzer-full-callback.csv` only 48 kHz; they are not used to claim four-rate coverage. Native UI/producer attribution belongs to its separate report.


## Retained non-improving paired observations


9 of 48 cells have a candidate update median at least as high as baseline. No cell is excluded. Negative reduction values below mean increased measured cost.


| Rate | FFT | Fixture | Update reduction % | Actual thread CPU reduction % | Adjacent-pair update reductions % |
|---:|---:|---|---:|---:|---|
| 44100 | 2048 | 20 ms | -4.029 | 2.866 | 38.935, -21.105, 3.811, -27.564 |
| 44100 | 4096 | 40 ms | -24.560 | -11.984 | -34.573, -80.274, -13.463, 21.184 |
| 44100 | 8192 | 80 ms | -7.951 | -2.917 | -2.546, -20.605, 9.726, -1.463 |
| 48000 | 2048 | 20 ms | -6.538 | 2.504 | 12.061, -17.008, -23.091, -3.960 |
| 48000 | 2048 | 40 ms | -17.614 | -8.792 | -53.235, 5.096, -45.863, 22.787 |
| 48000 | 8192 | 20 ms | -31.759 | -14.888 | -59.104, 13.215, 8.145, -14.328 |
| 48000 | 8192 | 40 ms | -1.539 | -0.501 | -15.422, -50.243, 25.950, 32.621 |
| 96000 | 8192 | 40 ms | -0.829 | -2.240 | -13.834, -23.725, 7.512, 10.481 |
| 192000 | 2048 | full_capacity_catchup | -36.591 | -26.586 | -57.000, 24.055, -24.789, 0.911 |


The following cells improve update wall time but have non-lower measured thread CPU medians (ties or increases; the CPU measurement includes FIFO feed):

96000 Hz, FFT 4096, full_capacity_catchup: update reduction 1.891%, CPU reduction -4.954%.


All raw repetitions remain in the paired report JSON and source CSV. Mixed signs alone do not establish a scheduling cause or disprove a real regression. Wall and thread CPU measure different scopes: update wall excludes FIFO feed; the thread CPU observation includes it.


Physical DAW/audio-interface performance remains **NOT_ASSESSED**. Source and measurements remain attached to this run, separate from minimal local, full-linked local, or subsequent CI results.
