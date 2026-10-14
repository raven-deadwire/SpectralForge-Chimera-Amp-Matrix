# macOS: two separate paired-cost CI observations

**Recommendation: keep the narrow change in Draft development, with the repeated macOS cost observations unresolved. Native/DAW performance stays NOT_ASSESSED and October 23 final-installer acceptance stays on hold.**

Functional preservation and a recurring aggregate cost decrease support retaining the experimental candidate. The same full-capacity cell gets worse in both runs, with actual thread CPU also worse. No regression observation is discarded; its cause is not assigned to jitter. No fixture-specific production branch or immediate rollback is justified by this evidence alone.

## Comparable source and measurement conditions

- First run: `38240150849`, source `ddb0880bd61a4c794236783b869767b96208bf41`.
- Second run: `38243938568`, source `72e361ba91ea2c6b8d830921a71275fa25cba493`.
- All 111 recorded C++ source hashes and CMakeLists.txt hash match. Build metadata matches: Release, -O3 -DNDEBUG, /usr/bin/c++, Unix Makefiles.
- Same recorded macOS 15.7.9 arm64 image, ImageVersion 20260907.0337.1, 3 logical CPUs. Physical worker identity, CPU frequency, and per-trial core placement are not recorded.
- Both tests use clock_gettime(CLOCK_THREAD_CPUTIME_ID) for actual thread CPU. All 384 raw CPU observations are nonzero in each run.
- JUCE 8.0.8 source and default build configuration select AppleFFT/vDSP; no runtime backend field or remote symbol dump was supplied.

## Aggregate results, retained per run

| Run | Lower update medians | Update reduction median | Lower CPU medians | CPU reduction median |
|---|---:|---:|---:|---:|
| 38240150849 | 39/48 | 14.642% | 40/48 | 11.031% |
| 38243938568 | 38/48 | 14.755% | 40/48 | 11.945% |

## 192 kHz, FFT 2048, one full 65,536-frame catch-up

Every trial below consumes the same data and performs 128 FFT windows, with zero FIFO drops. Update wall excludes FIFO feed; the measured thread CPU includes feed. Times are milliseconds.

| Sequence | Variant | Run 1 update wall | Run 1 thread CPU | Run 2 update wall | Run 2 thread CPU |
|---:|---|---:|---:|---:|---:|
| 0 | baseline_beb72f | 4.830250 | 6.707250 | 6.105542 | 6.447750 |
| 1 | candidate | 7.583500 | 9.216833 | 4.028875 | 5.428333 |
| 2 | candidate | 7.553167 | 9.862333 | 4.417250 | 5.351042 |
| 3 | baseline_beb72f | 9.945625 | 10.220500 | 5.672750 | 7.241959 |
| 4 | candidate | 6.784416 | 8.229625 | 8.162542 | 8.603833 |
| 5 | baseline_beb72f | 5.436708 | 7.075083 | 4.629333 | 6.250417 |
| 6 | baseline_beb72f | 5.060042 | 6.520500 | 5.039917 | 6.455250 |
| 7 | candidate | 5.013959 | 7.306250 | 6.962958 | 8.477584 |

| Observation | Run 1 | Run 2 |
|---|---:|---:|
| Baseline update median, ms | 5.248375 | 5.356333 |
| Candidate update median, ms | 7.168792 | 5.690104 |
| Baseline thread CPU median, ms | 6.891167 | 6.451500 |
| Candidate thread CPU median, ms | 8.723229 | 6.952959 |
| Update cost increase | 36.591% | 6.231% |
| CPU cost increase | 26.586% | 7.773% |

Run 1 adjacent-pair update reductions: -57.000%, +24.055%, -24.789%, +0.911%.
Run 2 adjacent-pair update reductions: +34.013%, +22.132%, -76.322%, -38.156%.

The first two pairs in run 2 improve both update wall and CPU; the last two worsen both. Repeated higher medians and mixed pair signs are both retained. This does not identify whether the cause is hardware execution variability, code generation, backend interaction, or another condition.

## Common non-improving conditions

Positive percentages below mean increased measured cost.

| Rate | FFT | Fixture | Run 1 wall increase | Run 2 wall increase | Run 1 CPU increase | Run 2 CPU increase |
|---:|---:|---|---:|---:|---:|---:|
| 44100 | 8192 | 80 ms | 7.951% | 15.178% | 2.917% | 3.856% |
| 48000 | 2048 | 40 ms | 17.614% | 9.917% | 8.792% | -0.199% |
| 192000 | 2048 | full_capacity_catchup | 36.591% | 6.231% | 26.586% | 7.773% |

Two cells increase both wall and actual thread CPU in both runs: 44.1 kHz / FFT 8192 / 80 ms, and 192 kHz / FFT 2048 / full capacity.

## Every second-run non-improving update condition

| Rate | FFT | Fixture | Wall cost increase | Thread CPU cost increase |
|---:|---:|---|---:|---:|
| 44100 | 8192 | 80 ms | 15.178% | 3.856% |
| 48000 | 2048 | full_capacity_catchup | 11.380% | 11.879% |
| 48000 | 2048 | 40 ms | 9.917% | -0.199% |
| 48000 | 4096 | 40 ms | 7.728% | 1.962% |
| 48000 | 8192 | full_capacity_catchup | 20.240% | 23.830% |
| 96000 | 2048 | full_capacity_catchup | 0.433% | 4.047% |
| 96000 | 2048 | 40 ms | 14.482% | 10.012% |
| 192000 | 2048 | full_capacity_catchup | 6.231% | 7.773% |
| 192000 | 4096 | 20 ms | 13.621% | 6.106% |
| 192000 | 8192 | full_capacity_catchup | 6.563% | -0.081% |

## Functional preservation

Both artifacts have actual 6/6 CTest PASS, exact 48 summary / 384 paired / 108 parity rows, maximum raw/envelope/display difference 0.0000152588 dB, zero paired/parity FIFO drops, and zero parity discontinuities.
The dedicated four-rate functional grids pass in both: temporal attacks 24, partition invariance 180, FIFO stalls 48, bit-identical whole-processor audio 12 at unchanged 42-sample latency, callback allocation fixture 48, cadence fixture 12. Intentional FIFO overflow cases remain labelled.
The callback fixture reports 1 overrun in run 1 and 0 in run 2. The second observation does not erase the first; neither is a physical DAW/device test. No additional callback allocations were observed in either run.

No runtime/test source or original artifact was changed to produce this report.
