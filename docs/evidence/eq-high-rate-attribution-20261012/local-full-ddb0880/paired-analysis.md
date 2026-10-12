# Linux-full-linked-local EQ paired-artifact analysis

Artifact: `/workspace/scratch/af7c830474b5/eq-local-build`

Native/DAW performance acceptance: **NOT_ASSESSED**.

## Dataset `.`

CSV checks: **PASS**. CTest receipt: **PASSED**, independently read from `eq-results-local.xml`.

Source: `ddb0880bd61a4c794236783b869767b96208bf41`.

Rows: {'summary': 48, 'paired': 384, 'parity': 108}. Lower candidate update medians: **45 / 48**.

Median update reduction: **8.666%**. Median CPU-cost reduction: **8.628%**, from 48 comparable cells.

### 192 kHz: 2-second cadence trials and one full-capacity catch-up

| FFT | Fixture / cadence | FFT windows | Update baseline → candidate, ms | Wall reduction | CPU baseline → candidate, ms | CPU reduction |
|---:|---|---:|---:|---:|---:|---:|
| 2048 | 20 ms cadence | 750 | 106.546 → 95.324 | 10.533% | 107.169 → 95.912 | 10.504% |
| 2048 | 40 ms cadence | 750 | 104.414 → 94.247 | 9.737% | 105.094 → 94.832 | 9.764% |
| 2048 | 80 ms cadence | 750 | 102.388 → 94.605 | 7.602% | 102.902 → 95.134 | 7.549% |
| 2048 | full-capacity catch-up | 128 | 17.696 → 15.734 | 11.090% | 17.765 → 15.798 | 11.071% |
| 4096 | 20 ms cadence | 375 | 92.586 → 81.368 | 12.116% | 93.207 → 81.902 | 12.129% |
| 4096 | 40 ms cadence | 375 | 94.216 → 84.366 | 10.454% | 94.732 → 84.982 | 10.293% |
| 4096 | 80 ms cadence | 375 | 92.882 → 83.725 | 9.858% | 93.388 → 84.224 | 9.813% |
| 4096 | full-capacity catch-up | 64 | 15.749 → 14.115 | 10.370% | 15.818 → 14.185 | 10.327% |
| 8192 | 20 ms cadence | 187 | 113.507 → 104.921 | 7.564% | 114.076 → 105.449 | 7.562% |
| 8192 | 40 ms cadence | 187 | 115.653 → 105.482 | 8.794% | 116.197 → 106.006 | 8.770% |
| 8192 | 80 ms cadence | 187 | 117.342 → 109.166 | 6.968% | 117.826 → 109.688 | 6.906% |
| 8192 | full-capacity catch-up | 32 | 20.182 → 18.415 | 8.755% | 20.316 → 18.530 | 8.791% |

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

## Full CMake local execution receipts

This report contains only the new full CMake-linked source `ddb0880bd61a4c794236783b869767b96208bf41` run. Earlier minimal direct-build timing results are not pooled.

| CTest | Status | Duration |
|---|---|---:|
| ChimeraGateProcessorTests | PASSED | 13.965 s |
| ChimeraGraphicalEQTests | PASSED | 29.945 s |
| ChimeraEQAnalyzerTests | PASSED | 18.248 s |
| ChimeraEQAnalyzerPerformanceTests | PASSED | 20.851 s |

The local validation state records both selected CTests and the 22-case headless diagnostic exiting 0. The headless CSV independently contains 22 rows, zero sample-accounting errors, and NOT_ASSESSED in every acceptance field. Native UI was NOT_RUN; no DAW or physical audio-interface acceptance was performed.

## Three cells without a lower candidate median

These measured rows are retained. The raw repeats vary; no outlier has been removed and no per-cell statistical significance is claimed.

| Rate | FFT | Fixture | Baseline update median | Candidate update median | Wall change (positive = reduction) | CPU change |
|---:|---:|---|---:|---:|---:|---:|
| 48000 | 8192 | 40 ms cadence | 29.059 ms | 30.496 ms | -4.944% | -4.738% |
| 96000 | 4096 | 80 ms cadence | 49.838 ms | 50.855 ms | -2.041% | -2.172% |
| 96000 | 8192 | full-capacity catch-up | 23.593 ms | 24.788 ms | -5.065% | -5.277% |

All 12 cells at 192 kHz have lower candidate update and CPU medians. The aggregate 8.666% wall and 8.628% CPU reductions include all 48 cells, including the three above.

## Separately observed zero-FFT consumer calls

The new `eq-analyzer-cpu.csv` records 160 measured calls at 25 Hz per case. FFT 8192 reports 22 calls with zero completed FFT windows at 44.1 kHz and 10 at 48 kHz; every other rate/FFT combination reports zero such calls. All 12 rows report zero FIFO drops. These are legitimate sample-clocked incomplete hops and remain in the measured CPU denominator.
