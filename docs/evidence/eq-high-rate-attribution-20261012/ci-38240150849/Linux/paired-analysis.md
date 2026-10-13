# Linux-native-CI EQ paired-artifact analysis

Artifact: `/workspace/scratch/af7c830474b5/ci-artifacts/linux-ddb0880`

Native/DAW performance acceptance: **NOT_ASSESSED**.

## Dataset `.`

CSV checks: **PASS**. CTest receipt: **PASSED**.

Source: `ddb0880bd61a4c794236783b869767b96208bf41`.

Rows: {'summary': 48, 'paired': 384, 'parity': 108}. Lower candidate update medians: **48 / 48**.

Median update reduction: **3.717%**. Median CPU-cost reduction: **3.733%**, from 48 comparable cells.

### 192 kHz: 2-second cadence trials and one full-capacity catch-up

| FFT | Fixture / cadence | FFT windows | Update baseline → candidate, ms | Wall reduction | CPU baseline → candidate, ms | CPU reduction |
|---:|---|---:|---:|---:|---:|---:|
| 2048 | 20 ms cadence | 750 | 381.827 → 367.204 | 3.830% | 382.396 → 367.743 | 3.832% |
| 2048 | 40 ms cadence | 750 | 381.950 → 366.944 | 3.929% | 382.519 → 367.453 | 3.939% |
| 2048 | 80 ms cadence | 750 | 381.617 → 367.193 | 3.780% | 382.197 → 367.703 | 3.792% |
| 2048 | full-capacity catch-up | 128 | 65.112 → 62.641 | 3.794% | 65.221 → 62.730 | 3.820% |
| 4096 | 20 ms cadence | 375 | 395.884 → 380.556 | 3.872% | 396.459 → 381.132 | 3.866% |
| 4096 | 40 ms cadence | 375 | 395.394 → 380.144 | 3.857% | 395.951 → 380.669 | 3.859% |
| 4096 | 80 ms cadence | 375 | 395.451 → 380.528 | 3.774% | 395.985 → 381.053 | 3.771% |
| 4096 | full-capacity catch-up | 64 | 67.548 → 64.860 | 3.979% | 67.635 → 64.960 | 3.955% |
| 8192 | 20 ms cadence | 187 | 440.776 → 426.214 | 3.304% | 441.348 → 426.811 | 3.294% |
| 8192 | 40 ms cadence | 187 | 440.559 → 426.026 | 3.299% | 441.152 → 426.585 | 3.302% |
| 8192 | 80 ms cadence | 187 | 440.727 → 426.179 | 3.301% | 441.249 → 426.694 | 3.299% |
| 8192 | full-capacity catch-up | 32 | 75.336 → 72.873 | 3.270% | 75.430 → 72.952 | 3.285% |

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

Source: `ddb0880bd61a4c794236783b869767b96208bf41`. Runner: `Linux-6.8.0-1064-azure-x86_64-with-glibc2.35`.

Build-selected FFT engine from JUCE 8.0.8 source/default configuration: FFTFallback; artifact CSV does not directly name the engine.


| Actual CTest record | Status | Seconds |
|---|---|---:|
| ChimeraGateProcessorTests | PASSED | 13.0183 |
| ChimeraGraphicalEQTests | PASSED | 39.9972 |
| ChimeraGraphicalEQUITests | PASSED | 117.5320 |
| ChimeraEQAnalyzerTests | PASSED | 40.3340 |
| ChimeraEQAnalyzerPerformanceTests | PASSED | 76.6435 |
| ChimeraEQHighRateAttributionTests | PASSED | 151.0330 |


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


All raw repetitions remain in the paired report JSON and source CSV. Mixed signs alone do not establish a scheduling cause or disprove a real regression. Wall and thread CPU measure different scopes: update wall excludes FIFO feed; the thread CPU observation includes it.


Physical DAW/audio-interface performance remains **NOT_ASSESSED**. Source and measurements remain attached to this run, separate from minimal local, full-linked local, or subsequent CI results.
