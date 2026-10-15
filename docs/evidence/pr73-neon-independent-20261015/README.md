# PR73 independent NEON comparison — HOLD

2026-10-15. Native collection and independent raw-data audit completed. **The NEON change did not demonstrate a CAB performance improvement. Some fixed-workload CPU/stage costs regressed beyond the prespecified A/A noise envelope. Do not integrate PR73 on performance grounds.**

## Identity and execution

- Scalar pre-NEON branch parent A: `c2263e4c90bffb9a56cc41285ebf8207e641b3aa`.
- PR73 current candidate B: `29865abb4da3ab78592c970a263ee62af63fc673`.
- Literal B parent `6b66180d22f4f354aad63691d981e5295fccbd55` is the rejected first NEON implementation, not a scalar control. Its parent is A.
- Production A/B diff is only `Source/ModeledCabConvolution.h`; source snapshots were clean before and after execution.
- Controller: `8c7183df95a58d7e46028c6439d137e67eaaf72b`. This evidence-only commit does not change the tested controller or product sources.
- One GitHub-hosted macos-15 ARM64 job, Apple M1 (Virtual), 3 cores / 7 GB; AppleClang 17.0.0.17000013; Release; native arm64; JUCE `d6181bde38d858c283c3b7bf699ce6340c050b5d`.
- [Native run](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38700815250); [raw artifact, 50,747,037 bytes](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38700815250/artifacts/11957749583). Raw artifact retention expires 2027-01-13; compact results, manifest and audit are preserved here.
- Artifact SHA-256: `f7990296f7775dfae150e105e7d02517f9978784ce44ee948860995fadcbcc24`.

## Protocol and independent audit

Six ABBA/BAAB quartets and six interleaved A/A quartets per measurement mode; 96 trials, 12 engine/buffer/channel routes per trial, 1,600 callbacks per route. All compilation finished before measurement. A/A uses the identical parent binary/hash in both logical arms. Stage clocks off is the primary callback timing population; stage clocks on is a separate spectral MAC diagnostic. The two modes are not pooled.

Primary causal phase is the 400-callback forced six-slot publication. All 12 profile routes had identical per-callback MAC call schedules across A/B and A/A. The 1,200-callback asynchronous worker/automation phase is reported separately. It has different observed MAC exposure and cannot replace the fixed-workload comparison.

Positive `improvement_pct` in the machine-readable analysis means faster. Negative values mean slower. Effects use paired quartet log-ratios, with 10,000 deterministic quartet bootstrap resamples; callbacks are not treated as independent replicates. The lower 95% bound must exceed both 2% and the largest absolute A/A quartet effect, and both execution orders must improve. A regression flag requires the upper improvement bound to be below the negative A/A envelope/2% floor. These are within-allocation estimates, not universal hardware or multiplicity-adjusted family-wide guarantees.

Independent post-download audit verified all **2,174 manifest files**, recomputed key metrics for **1,843,200 raw callbacks / 3,456 route-phase summaries**, verified identical normalized probe compiler commands across A/B, and verified identical A/A binary identity. No failed trial or outlier was removed. Nine diagnostic process exits due to their unchanged p99 assertion were preserved; all planned trials still completed. Workflow success means collection complete, not performance accepted.

## Prespecified regression findings in fixed workload

Percentages below are cost increases (the sign of `improvement_pct` is reversed). They are paired-quartet estimates, not ratios of the medians in the following table.

| Engine | Samples | Channels | Metric | Cost increase | 95% interval | A/A envelope |
|---|---:|---:|---|---:|---|---:|
| expanded-v2 | 64 | 2 | callback thread CPU mean | +22.89% | +19.03% to +26.68% | 15.28% |
| original-v1 | 32 | 2 | callback thread CPU mean | +40.00% | +25.60% to +57.54% | 22.23% |
| array-v3-6x10 | 32 | 2 | spectral MAC wall mean | +31.15% | +26.82% to +35.19% | 16.68% |
| expanded-v2 | 64 | 2 | spectral MAC wall mean | +25.36% | +16.94% to +34.27% | 9.00% |
| original-v1 | 32 | 1 | spectral MAC wall mean | +27.54% | +12.28% to +48.37% | 9.80% |

No primary route passed the improvement rule. CPU mean medians and MAC mean medians were higher in B on all 12 fixed-workload routes. Wall-p99 distributions were noisy; a deterministic causal wall-p99 regression or miss-rate change is not established from the descriptive counts alone.

## Fixed-workload timings: A → B

All times are µs per CAB callback (three rigs / six microphones). Values are medians across the 12 A or B trial summaries. Spectral MAC is separately instrumented elapsed wall time; CPU and wall p99 use the stage-clock-off target. Misses are counts over 4,800 callbacks per arm for each route.

| Engine | Samples | Channels | MAC mean µs | Thread CPU mean µs | Thread CPU p99 µs | Wall p99 µs | Misses A/B |
|---|---:|---:|---|---|---|---|---|
| array-v3-6x10 | 32 | 1 | 16.728 → 21.226 | 21.088 → 23.797 | 36.125 → 40.625 | 34.729 → 39.270 | 0/0 |
| array-v3-6x10 | 32 | 2 | 32.991 → 41.489 | 37.996 → 45.204 | 93.104 → 109.438 | 91.896 → 116.166 | 1/3 |
| array-v3-6x10 | 64 | 1 | 14.645 → 16.724 | 23.061 → 24.711 | 67.312 → 40.729 | 66.751 → 40.104 | 0/0 |
| array-v3-6x10 | 64 | 2 | 29.682 → 38.156 | 41.212 → 51.905 | 79.145 → 142.479 | 107.084 → 137.438 | 0/1 |
| expanded-v2 | 32 | 1 | 16.228 → 20.770 | 21.362 → 27.142 | 46.292 → 65.062 | 45.583 → 64.104 | 0/1 |
| expanded-v2 | 32 | 2 | 32.710 → 37.715 | 35.730 → 44.474 | 47.125 → 67.395 | 49.124 → 66.834 | 0/2 |
| expanded-v2 | 64 | 1 | 15.067 → 17.068 | 23.591 → 25.897 | 56.354 → 38.042 | 54.376 → 37.729 | 0/0 |
| expanded-v2 | 64 | 2 | 30.343 → 38.255 | 41.528 → 50.507 | 77.230 → 137.687 | 75.334 → 135.355 | 0/1 |
| original-v1 | 32 | 1 | 16.849 → 21.623 | 21.348 → 24.987 | 46.791 → 40.020 | 47.229 → 38.292 | 1/0 |
| original-v1 | 32 | 2 | 35.716 → 39.324 | 37.194 → 44.881 | 66.437 → 70.875 | 59.271 → 68.999 | 0/2 |
| original-v1 | 64 | 1 | 14.702 → 16.916 | 22.202 → 25.193 | 40.376 → 42.583 | 39.104 → 45.416 | 0/0 |
| original-v1 | 64 | 2 | 30.563 → 37.383 | 44.846 → 52.038 | 90.312 → 108.938 | 104.062 → 107.458 | 1/0 |

## All-callback timing and misses

These include asynchronous worker/automation exposure and are descriptive supplementary observations, not the primary matched-workload causal test. Each route contains 19,200 callbacks per arm.

| Engine | Samples | Channels | Thread CPU mean µs A→B | Thread CPU p99 µs A→B | Wall p99 µs A→B | Misses A/B |
|---|---:|---:|---|---|---|---|
| array-v3-6x10 | 32 | 1 | 37.023 → 44.308 | 115.729 → 118.375 | 113.980 → 115.396 | 6/9 |
| array-v3-6x10 | 32 | 2 | 62.637 → 78.376 | 154.313 → 193.625 | 152.980 → 193.104 | 13/33 |
| array-v3-6x10 | 64 | 1 | 37.871 → 47.295 | 112.271 → 120.916 | 110.416 → 119.500 | 1/7 |
| array-v3-6x10 | 64 | 2 | 71.567 → 91.949 | 177.583 → 229.896 | 177.105 → 229.500 | 3/9 |
| expanded-v2 | 32 | 1 | 38.903 → 46.930 | 112.812 → 133.021 | 111.959 → 132.645 | 8/20 |
| expanded-v2 | 32 | 2 | 58.889 → 81.273 | 156.750 → 201.791 | 154.771 → 199.959 | 6/37 |
| expanded-v2 | 64 | 1 | 43.295 → 49.543 | 108.083 → 127.729 | 106.812 → 126.458 | 1/2 |
| expanded-v2 | 64 | 2 | 78.864 → 85.361 | 190.500 → 224.166 | 189.271 → 223.687 | 2/6 |
| original-v1 | 32 | 1 | 37.747 → 46.828 | 105.229 → 119.103 | 103.521 → 117.291 | 6/9 |
| original-v1 | 32 | 2 | 66.119 → 81.596 | 159.667 → 195.250 | 158.354 → 192.395 | 6/8 |
| original-v1 | 64 | 1 | 42.857 → 42.364 | 110.855 → 122.166 | 109.125 → 123.000 | 1/5 |
| original-v1 | 64 | 2 | 73.501 → 92.859 | 182.834 → 229.979 | 181.000 → 229.208 | 10/8 |

| Samples | Budget µs | Callbacks per arm | Wall misses A→B | CPU over budget A→B |
|---|---:|---:|---|---|
| 32 | 333.333 | 115,200 | 45 → 116 | 35 → 103 |
| 64 | 666.667 | 115,200 | 18 → 37 | 13 → 35 |

## Output and latency contracts

Both A and B independently passed the unchanged original **5/5 CAB CTests and 66/66 deadline rows**, plus **144 realtime equivalence routes**. The frozen residue-bank comparison passed **2,645,088 exact float samples + 388,800 fade-mix samples**, immutable spectra, resets/wraps/private kernels, and **zero added processing latency** on both sources.

Those original one-shot deadline gates do not guarantee zero misses in the larger repeated population. Both facts are retained without replacing a failed diagnostic observation with a passing original test.

## Decision

- Collection and independent evidence integrity: COMPLETE.
- Output-bit and zero-latency compatibility: PASS on both A and B.
- Independent performance improvement: NOT DEMONSTRATED; fixed-workload regression flags present.
- PR73 integration: HOLD; keep Draft. No source merge or release action was performed.
- Full product/update-candidate validation for the NEON candidate: separate PENDING condition.
- Actual DAW/physical interface/listening acceptance: separate PENDING condition.
- Release approval: false.

This experiment establishes measured performance behavior, not the instruction-level cause of the slowdown. Assembly/unrolling or memory-behavior explanations would require a new investigation.

Files: `analysis.json.gz` contains all 72 mode/route/phase results, A/A envelopes, order effects and bootstrap intervals; `report.json.gz` preserves per-trial statistics and command receipts; `native-manifest.json.gz` identifies every raw artifact file; `audit.json` and `audit_neon_artifact.py` document the independent recomputation.
