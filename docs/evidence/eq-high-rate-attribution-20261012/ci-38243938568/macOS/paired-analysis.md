# macOS-native-CI EQ paired-artifact analysis

Artifact: `/workspace/scratch/af7c830474b5/ci-artifacts/macos-72e361ba`

Native/DAW performance acceptance: **NOT_ASSESSED**.

## Dataset `.`

CSV checks: **PASS**. CTest receipt: **PASSED**.

Source: `72e361ba91ea2c6b8d830921a71275fa25cba493`.

Rows: {'summary': 48, 'paired': 384, 'parity': 108}. Lower candidate update medians: **38 / 48**.

Median update reduction: **14.755%**. Median CPU-cost reduction: **11.945%**, from 48 comparable cells.

### 192 kHz: 2-second cadence trials and one full-capacity catch-up

| FFT | Fixture / cadence | FFT windows | Update baseline → candidate, ms | Wall reduction | CPU baseline → candidate, ms | CPU reduction |
|---:|---|---:|---:|---:|---:|---:|
| 2048 | 20 ms cadence | 750 | 42.522 → 34.042 | 19.943% | 47.104 → 39.991 | 15.100% |
| 2048 | 40 ms cadence | 750 | 48.751 → 33.971 | 30.317% | 49.530 → 39.659 | 19.929% |
| 2048 | 80 ms cadence | 750 | 35.346 → 30.865 | 12.679% | 41.421 → 37.092 | 10.451% |
| 2048 | full-capacity catch-up | 128 | 5.356 → 5.690 | -6.231% | 6.452 → 6.953 | -7.773% |
| 4096 | 20 ms cadence | 375 | 35.926 → 40.819 | -13.621% | 42.905 → 45.525 | -6.106% |
| 4096 | 40 ms cadence | 375 | 47.038 → 36.688 | 22.004% | 51.011 → 41.694 | 18.265% |
| 4096 | 80 ms cadence | 375 | 42.579 → 32.379 | 23.956% | 48.093 → 39.224 | 18.442% |
| 4096 | full-capacity catch-up | 64 | 7.888 → 7.094 | 10.073% | 8.692 → 7.831 | 9.913% |
| 8192 | 20 ms cadence | 187 | 37.062 → 31.696 | 14.479% | 43.985 → 38.900 | 11.560% |
| 8192 | 40 ms cadence | 187 | 39.781 → 31.394 | 21.082% | 45.976 → 38.106 | 17.117% |
| 8192 | 80 ms cadence | 187 | 40.045 → 38.644 | 3.497% | 46.179 → 44.120 | 4.459% |
| 8192 | full-capacity catch-up | 32 | 7.001 → 7.461 | -6.563% | 7.803 → 7.796 | 0.081% |

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

Source: `72e361ba91ea2c6b8d830921a71275fa25cba493`. Runner: `macOS-15.7.9-arm64-arm-64bit-Mach-O`.

Build-selected FFT engine from JUCE 8.0.8 source/default configuration: AppleFFT / vDSP; artifact CSV does not directly name the engine.


| Actual CTest record | Status | Seconds |
|---|---|---:|
| ChimeraGateProcessorTests | PASSED | 28.9871 |
| ChimeraGraphicalEQTests | PASSED | 38.9800 |
| ChimeraGraphicalEQUITests | PASSED | 184.1230 |
| ChimeraEQAnalyzerTests | PASSED | 27.2176 |
| ChimeraEQAnalyzerPerformanceTests | PASSED | 11.5220 |
| ChimeraEQHighRateAttributionTests | PASSED | 176.3750 |


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


10 of 48 cells have a candidate update median at least as high as baseline. No cell is excluded. Negative reduction values below mean increased measured cost.


| Rate | FFT | Fixture | Update reduction % | Actual thread CPU reduction % | Adjacent-pair update reductions % |
|---:|---:|---|---:|---:|---|
| 44100 | 8192 | 80 ms | -15.178 | -3.856 | -25.612, 42.133, -51.455, -11.722 |
| 48000 | 2048 | full_capacity_catchup | -11.380 | -11.879 | -39.542, 10.855, 44.113, -209.615 |
| 48000 | 2048 | 40 ms | -9.917 | 0.199 | 14.124, 11.417, -54.401, -46.117 |
| 48000 | 4096 | 40 ms | -7.728 | -1.962 | 4.574, -15.975, 2.148, -18.419 |
| 48000 | 8192 | full_capacity_catchup | -20.240 | -23.830 | 23.157, -99.730, 54.544, -113.937 |
| 96000 | 2048 | full_capacity_catchup | -0.433 | -4.047 | 37.046, -56.087, 7.009, -10.472 |
| 96000 | 2048 | 40 ms | -14.482 | -10.012 | -25.844, 8.113, -4.575, 8.364 |
| 192000 | 2048 | full_capacity_catchup | -6.231 | -7.773 | 34.013, 22.132, -76.322, -38.156 |
| 192000 | 4096 | 20 ms | -13.621 | -6.106 | 0.451, -61.986, 4.267, -26.969 |
| 192000 | 8192 | full_capacity_catchup | -6.563 | 0.081 | -57.019, 26.298, -84.514, 28.180 |


All raw repetitions remain in the paired report JSON and source CSV. Mixed signs alone do not establish a scheduling cause or disprove a real regression. Wall and thread CPU measure different scopes: update wall excludes FIFO feed; the thread CPU observation includes it.


Physical DAW/audio-interface performance remains **NOT_ASSESSED**. Source and measurements remain attached to this run, separate from minimal local, full-linked local, or subsequent CI results.
