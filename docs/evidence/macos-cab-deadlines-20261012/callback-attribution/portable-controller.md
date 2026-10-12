# Portable controller allocation — clean-source ABBA, 2026-10-12

One prespecified Linux console allocation completed the full controller path from clean source: both builds, the frozen and current original gates, the trace-contract and enabled semantic checks, ABBA measurement arms, the separately scheduled resource population, and independent raw-artifact verification. **All 28 recorded commands returned 0; all 20 mode populations were `verified_complete`.** This is portable pipeline validation from one KVM environment. It establishes no macOS, physical audio-device, AU/VST3, CoreAudio, DAW, or product performance-fix result.

[Machine-readable receipt](portable-controller.json) retains each of the 60 route populations and both 1,200/400-interval phases separately. The report and full raw-file manifest are identified by SHA-256 below. Raw records remain in the local execution workspace and are not committed with this bounded receipt.

## Execution and source binding

The controller ran from 2026-10-12T08:54:30.620729+00:00 to 2026-10-12T09:08:31.397225+00:00. It used Linux x86_64, kernel 6.18.44, a reported AMD EPYC 9V74 processor with 9 visible CPUs, and KVM full virtualization. These are the controller environment receipts; they do not characterize scheduler isolation or imply a physical host measurement. CMake and CTest were 3.31.6; the Release C++ compiler was GNU 13.3.0.

| Checkout | Commit | Tree | Before | After |
|---|---|---|---|---|
| Instrumented/current | `35617a2e0a5a599c5bd2b2a13113b6f234cd60a3` | `27e6ea93d1eb810a4ba9646cf6260371aec56cbe` | Clean | Clean, same commit/tree |
| Frozen release | `7b20cd9d1f90e3e2969c250712b199e893cf50cd` | `1b565308db6adc00937f7154905728ba27bcd6d9` | Clean | Clean, same commit/tree |

The diagnostic worktree was detached at the recorded current commit before configuration. The frozen and current checkouts remained clean and unchanged throughout this allocation. JUCE was 8.0.8 at `d6181bde38d858c283c3b7bf699ce6340c050b5d` in both builds; its recorded FFT source SHA-256 was `aa1e5209d0bea072e8e57831110d86adfc6f91670c3ce72d3102f1e9a64843a3`. The JUCE receipt records the commit and FFT source hash, not a separate full-tree cleanliness receipt.

All builds and gates finished before the diagnostic measurement arms. The command was executed once, with no retry or threshold change:

```sh
source /workspace/scratch/f3df933ddf5b/toolchain/env.sh
python3 /workspace/scratch/f74ffd99d2f8/portable-source/Tools/CabCallbackAttribution/run_attribution.py \
  --source /workspace/scratch/f74ffd99d2f8/portable-source \
  --source-sha 35617a2e0a5a599c5bd2b2a13113b6f234cd60a3 \
  --frozen-source /workspace/scratch/f74ffd99d2f8/frozen-source \
  --build-root /workspace/scratch/f74ffd99d2f8/portable-controller-build \
  --output /workspace/scratch/f74ffd99d2f8/portable-controller-evidence \
  --order ABBA --environment portable-validation \
  --juce-source /workspace/scratch/f3df933ddf5b/toolchain/JUCE
```

## Prespecified populations and preserved gates

| Sequence | Condition | CAB routes | Arithmetic / memory / clock routes | Integrity |
|---|---|---:|---:|---|
| 01-A | Same harness, hooks compiled out, trace off | 6 | 2 / 2 / 2 | All four modes verified |
| 02-B | Hooks compiled in, stage recording on | 6 | 2 / 2 / 2 | All four modes verified |
| 03-B | Hooks compiled in, stage recording on | 6 | 2 / 2 / 2 | All four modes verified |
| 04-A | Same harness, hooks compiled out, trace off | 6 | 2 / 2 / 2 | All four modes verified |
| resources-after | Separate stage + resource recording | 6 | 2 / 2 / 2 | All four modes verified |

Every diagnostic route contains 1,600 retained intervals at 96 kHz / 64 samples: 1,200 worker/automation intervals followed by 400 forced-publication intervals for CAB, or the corresponding paced/burst intervals for each sentinel. CAB covers `original-v1`, `expanded-v2`, and `array-v3-6x10`, each in mono and stereo. Each sentinel uses `original-v1` as its harness label in mono and stereo. This gives **60 diagnostic routes and 96,000 intervals, including 30 CAB routes and 48,000 CAB intervals**. These totals describe the recorded inventory; they are not pooled performance populations.

The original five-target gate remained separate and retained its own timing envelope and p99 criterion:

| Original gate | Tests passed | Original p99 populations passed | Raw callback intervals | Individual wall deadline misses |
|---|---:|---:|---:|---:|
| Frozen release `7b20cd9` | 5 / 5 | 66 / 66 | 105,600 | **25** |
| Current hooks-off `35617a2` | 5 / 5 | 66 / 66 | 105,600 | **22** |

Both the original raw callback inventory and these miss counts were recomputed while preparing this receipt. The 25 and 22 misses are separate observations from sequential Linux gates. Passing p99 does not erase those misses, and their numerical difference does not demonstrate an improvement. No original response, channel separation, fade, allocation, or real-time acceptance threshold was relaxed.

The enabled semantic target returned 0 and reported 144 equivalence routes, eight six-mic direct-FIR routes, 2,645,088 exact residue-reference float samples, 388,800 fade-mix samples, a 50 ms fade, and zero latency. The trace-contract target returned 0 for zero-capacity and bounded-overflow behavior, hierarchy and endpoint nesting, kernel context, packed worker state, and TLS detachment at stop. The per-route probes also completed their semantic assertions with zero observed callback allocations or deletions. The independent verifier validates trace and raw-record consistency; semantic validity remains the executed C++ assertion receipt, not an offline reanalysis of recorded output audio.

## Per-route callback observations

Each table row is one distinct 1,600-interval population. Miss columns retain the two phases separately: **W/F** means 1,200 worker/automation and 400 forced-publication intervals for CAB; **P/B** means 1,200 paced and 400 burst intervals for a sentinel. Timing columns show **p50 / p99 / maximum**, in microseconds, from the callback body endpoints. The JSON retains corresponding timing distributions for each phase and the thread CPU duration paired with the maximum-wall callback. Percentiles use sorted index `floor(N*p)` without interpolation, matching the verifier.

Wall and CPU distribution percentiles are separate statistics; their maxima need not identify the same callback. The body includes its measurement and recording work. Slot snapshots and optional resource queries are inside the separately recorded outer entry/exit envelope, not these body columns. Wall minus CPU is not treated as exact scheduling time, and logical memory bytes are not hardware traffic or cache-latency measurements.

### 01-A — off

| Mode | Engine | Ch | Misses total (W/F or P/B) | Wall p50 / p99 / max µs | Thread CPU p50 / p99 / max µs |
|---|---|---:|---:|---|---|
| cab | array-v3-6x10 | 1 | 0 (0/0) | 46.541 / 127.874 / 308.756 | 46.489 / 128.190 / 309.059 |
| cab | array-v3-6x10 | 2 | 1 (1/0) | 63.797 / 193.071 / 1159.151 | 63.755 / 193.487 / 1160.000 |
| cab | expanded-v2 | 1 | 2 (1/1) | 46.540 / 146.812 / 1065.449 | 46.479 / 147.398 / 1066.321 |
| cab | expanded-v2 | 2 | 1 (1/0) | 72.139 / 192.711 / 1087.032 | 72.417 / 193.497 / 1088.114 |
| cab | original-v1 | 1 | 0 (0/0) | 46.270 / 135.394 / 328.757 | 46.228 / 136.172 / 329.719 |
| cab | original-v1 | 2 | 0 (0/0) | 64.557 / 186.392 / 322.427 | 64.516 / 186.747 / 322.869 |
| arithmetic | original-v1 | 1 | 0 (0/0) | 0.561 / 0.851 / 2.253 | 0.561 / 0.792 / 11.928 |
| arithmetic | original-v1 | 2 | 0 (0/0) | 0.561 / 0.851 / 175.284 | 0.561 / 0.802 / 29.434 |
| memory | original-v1 | 1 | 0 (0/0) | 1.713 / 4.246 / 167.694 | 1.733 / 4.036 / 167.999 |
| memory | original-v1 | 2 | 0 (0/0) | 1.652 / 3.185 / 120.231 | 1.672 / 3.225 / 120.399 |
| clock | original-v1 | 1 | 0 (0/0) | 0.140 / 0.331 / 0.500 | 0.140 / 0.220 / 0.391 |
| clock | original-v1 | 2 | 0 (0/0) | 0.140 / 0.360 / 0.551 | 0.140 / 0.250 / 0.420 |

### 02-B — stages

| Mode | Engine | Ch | Misses total (W/F or P/B) | Wall p50 / p99 / max µs | Thread CPU p50 / p99 / max µs |
|---|---|---:|---:|---|---|
| cab | array-v3-6x10 | 1 | 2 (1/1) | 140.242 / 244.249 / 1578.444 | 140.118 / 244.252 / 1578.390 |
| cab | array-v3-6x10 | 2 | 6 (4/2) | 113.120 / 434.766 / 1633.808 | 112.667 / 434.655 / 1633.762 |
| cab | expanded-v2 | 1 | 3 (3/0) | 93.682 / 327.365 / 1406.765 | 93.599 / 327.356 / 1406.726 |
| cab | expanded-v2 | 2 | 2 (2/0) | 105.850 / 335.406 / 2002.915 | 105.817 / 335.388 / 2002.860 |
| cab | original-v1 | 1 | 1 (1/0) | 90.406 / 290.288 / 680.617 | 90.394 / 290.030 / 680.489 |
| cab | original-v1 | 2 | 0 (0/0) | 106.210 / 329.227 / 641.469 | 106.258 / 329.108 / 641.451 |
| arithmetic | original-v1 | 1 | 0 (0/0) | 1.212 / 2.353 / 51.778 | 1.202 / 2.314 / 51.857 |
| arithmetic | original-v1 | 2 | 1 (1/0) | 1.202 / 2.053 / 2792.418 | 1.192 / 2.043 / 2792.330 |
| memory | original-v1 | 1 | 0 (0/0) | 3.355 / 23.205 / 317.870 | 3.325 / 23.244 / 317.721 |
| memory | original-v1 | 2 | 0 (0/0) | 3.335 / 8.964 / 204.539 | 3.305 / 8.953 / 204.393 |
| clock | original-v1 | 1 | 2 (1/1) | 21.132 / 108.164 / 1906.138 | 21.092 / 108.150 / 1905.906 |
| clock | original-v1 | 2 | 1 (0/1) | 21.112 / 110.016 / 1562.019 | 21.071 / 110.003 / 1561.956 |

### 03-B — stages

| Mode | Engine | Ch | Misses total (W/F or P/B) | Wall p50 / p99 / max µs | Thread CPU p50 / p99 / max µs |
|---|---|---:|---:|---|---|
| cab | array-v3-6x10 | 1 | 6 (2/4) | 94.633 / 346.523 / 2673.879 | 94.571 / 346.414 / 2673.704 |
| cab | array-v3-6x10 | 2 | 1 (1/0) | 104.929 / 312.492 / 1136.346 | 104.875 / 312.393 / 1136.145 |
| cab | expanded-v2 | 1 | 9 (8/1) | 122.675 / 406.895 / 2157.499 | 122.632 / 406.923 / 2157.359 |
| cab | expanded-v2 | 2 | 3 (3/0) | 120.222 / 361.486 / 1073.751 | 120.048 / 361.385 / 1073.742 |
| cab | original-v1 | 1 | 2 (2/0) | 91.217 / 301.646 / 1372.663 | 91.155 / 301.727 / 1372.515 |
| cab | original-v1 | 2 | 2 (1/1) | 105.639 / 338.061 / 689.732 | 105.566 / 338.061 / 689.703 |
| arithmetic | original-v1 | 1 | 0 (0/0) | 1.252 / 1.953 / 224.589 | 1.241 / 1.882 / 224.633 |
| arithmetic | original-v1 | 2 | 0 (0/0) | 1.262 / 2.143 / 48.863 | 1.242 / 2.053 / 48.752 |
| memory | original-v1 | 1 | 0 (0/0) | 2.975 / 5.959 / 57.246 | 2.965 / 5.878 / 57.315 |
| memory | original-v1 | 2 | 4 (4/0) | 3.215 / 7.962 / 3847.346 | 3.185 / 7.862 / 3847.224 |
| clock | original-v1 | 1 | 0 (0/0) | 20.111 / 82.044 / 291.911 | 20.099 / 82.022 / 291.903 |
| clock | original-v1 | 2 | 3 (3/0) | 20.121 / 95.844 / 2368.607 | 20.100 / 92.117 / 2368.622 |

### 04-A — off

| Mode | Engine | Ch | Misses total (W/F or P/B) | Wall p50 / p99 / max µs | Thread CPU p50 / p99 / max µs |
|---|---|---:|---:|---|---|
| cab | array-v3-6x10 | 1 | 0 (0/0) | 46.320 / 121.243 / 344.951 | 46.298 / 121.299 / 345.442 |
| cab | array-v3-6x10 | 2 | 0 (0/0) | 61.733 / 152.079 / 361.185 | 61.711 / 152.586 / 361.967 |
| cab | expanded-v2 | 1 | 0 (0/0) | 67.722 / 146.261 / 341.747 | 67.670 / 146.367 / 342.659 |
| cab | expanded-v2 | 2 | 1 (1/0) | 61.613 / 138.689 / 2117.388 | 61.571 / 138.495 / 2119.112 |
| cab | original-v1 | 1 | 1 (1/0) | 45.990 / 122.975 / 1085.830 | 45.939 / 122.983 / 1087.352 |
| cab | original-v1 | 2 | 0 (0/0) | 61.073 / 182.836 / 355.397 | 61.020 / 183.242 / 356.198 |
| arithmetic | original-v1 | 1 | 0 (0/0) | 0.561 / 0.862 / 30.626 | 0.561 / 0.742 / 45.016 |
| arithmetic | original-v1 | 2 | 0 (0/0) | 0.561 / 0.871 / 29.114 | 0.561 / 0.811 / 44.787 |
| memory | original-v1 | 1 | 0 (0/0) | 1.663 / 4.276 / 217.799 | 1.672 / 4.316 / 218.223 |
| memory | original-v1 | 2 | 1 (1/0) | 1.903 / 6.851 / 2215.545 | 1.913 / 6.780 / 2216.146 |
| clock | original-v1 | 1 | 0 (0/0) | 0.141 / 0.360 / 40.912 | 0.141 / 0.280 / 12.488 |
| clock | original-v1 | 2 | 0 (0/0) | 0.150 / 0.431 / 12.539 | 0.150 / 0.330 / 11.226 |

### resources-after — resources

| Mode | Engine | Ch | Misses total (W/F or P/B) | Wall p50 / p99 / max µs | Thread CPU p50 / p99 / max µs |
|---|---|---:|---:|---|---|
| cab | array-v3-6x10 | 1 | 7 (3/4) | 98.589 / 333.093 / 2220.623 | 98.076 / 332.974 / 2220.552 |
| cab | array-v3-6x10 | 2 | 5 (3/2) | 106.361 / 380.555 / 3407.055 | 106.217 / 380.263 / 3406.711 |
| cab | expanded-v2 | 1 | 11 (11/0) | 92.840 / 460.215 / 3045.368 | 92.747 / 459.952 / 3045.256 |
| cab | expanded-v2 | 2 | 11 (11/0) | 110.536 / 461.136 / 11300.333 | 110.424 / 461.083 / 11300.065 |
| cab | original-v1 | 1 | 9 (8/1) | 93.351 / 322.917 / 2125.229 | 93.018 / 322.808 / 2125.151 |
| cab | original-v1 | 2 | 10 (10/0) | 107.111 / 485.513 / 2234.194 | 107.028 / 485.280 / 2234.123 |
| arithmetic | original-v1 | 1 | 0 (0/0) | 1.202 / 1.883 / 21.813 | 1.202 / 1.863 / 21.832 |
| arithmetic | original-v1 | 2 | 0 (0/0) | 1.202 / 1.843 / 24.396 | 1.192 / 1.803 / 24.316 |
| memory | original-v1 | 1 | 0 (0/0) | 3.054 / 6.921 / 75.384 | 3.034 / 6.770 / 75.291 |
| memory | original-v1 | 2 | 0 (0/0) | 3.045 / 14.091 / 117.267 | 3.004 / 13.871 / 117.234 |
| clock | original-v1 | 1 | 0 (0/0) | 20.841 / 103.216 / 426.744 | 20.821 / 103.193 / 426.743 |
| clock | original-v1 | 2 | 0 (0/0) | 20.841 / 90.977 / 293.473 | 20.821 / 90.975 / 293.435 |

All 96,000 diagnostic body intervals had valid wall and thread CPU endpoints. Every retained route reported zero dropped events, clock errors, nesting errors, callback allocations, and callback deletions. The optional resource population successfully recorded the Linux per-thread API; it remains distinct from both ABBA conditions and provides no validation of Darwin resource APIs.

Both binaries explicitly touched the prepared event, worker, caller, callback-row, slot-row, and memory-array pages before measurement. Every route reported a 4,096-byte stride and 20,122 touch operations. Memory was not locked, and continued residency was not guaranteed. Worker publication remains asynchronous, so matching automation requests do not establish identical actual worker exposure across conditions.

## Evidence verification and fingerprints

All **561 files** listed in `SHA256SUMS.json` were read and hashed during receipt preparation: **561 matched, 0 missing, 0 mismatched; 820,758,367 bytes checked**. All 60 diagnostic route and phase summaries were also recomputed from the raw callback CSVs and matched the independent-verifier report. The four executable hashes were checked against the controller report.

| Evidence | SHA-256 |
|---|---|
| Complete `report.json` | `effe8b4558c03046d3bc0bced323e438184935bd9128a50beac62b39a1286f0b` |
| Full 561-file `SHA256SUMS.json` | `2a186d4aaad7451587310f2a5a53fbce7dbfa4ececb17670fea8e976e66a4eb4` |
| Diagnostic source manifest | `e5ac5e4f500959cdaaa5a20e70af30cd195a620b838bbc871a318cab61a07924` |
| Recorded C++ compiler metadata, same in both builds | `c176b7bdaaa455886bf3f9e9d8a06e77f261a986ba2ae6aaa05dc9301b9503ca` |
| Current compile commands | `d2c001aecad9538a7224dbbaf8f58e48b1e38331d9b13fe34a2fa5e302b4bcef` |
| Frozen compile commands | `05e477408faadae9cd77376c4afa66064edeb69e0963d3d5295fbe54809acc3f` |
| Current CMake cache | `34745c1d52a3b6fc25116c69af09a861e11357a4bafd7ac9dc350d16dad21d67` |
| Frozen CMake cache | `96de434264e0408cd177822c6a9fc02c793998656bd38d2101f8a29b4e1cb67c` |

| Executable | SHA-256 |
|---|---|
| `ChimeraCabCallbackProbeControl` | `db1ff188e43ab8a9c96da3acefcf1c707b5b3c03f5d349a94a4d66f3b1bfb12c` |
| `ChimeraCabCallbackProbe` | `8ac3d6d3879170f10ef992e53429b9959dc198aa9a1eff973c174977e055cb72` |
| `ChimeraCabAttributionRealtimeTests` | `86751460d00454b2f8a3d7d9f20b507b37654d4fb2464f24d47ae9a7ed3e516d` |
| `ChimeraCabCallbackTraceTests` | `4c1325d4a9fe5920b23ff2acc4f1616ddacdbd1d3cbfe6e31b7bb610ebf4c4df` |

The recorded compiler driver was `/usr/bin/c++`, resolving at receipt time to `/usr/bin/x86_64-linux-gnu-g++-13`. Its receipt-time SHA-256 was `52f1ddb33fe78b9441e0f42e9cd22c571f1101938e046c8a26582494e041cc73`. This additional current-file hash is distinguished from the compiler metadata captured by the build. Source-file, controller-file, build-metadata, command-return-code, and mode-integrity receipts are in the companion JSON.

The complete raw evidence, build and command logs, full report, and manifest remain at `/workspace/scratch/f74ffd99d2f8/portable-controller-evidence`. Build artifacts remain at `/workspace/scratch/f74ffd99d2f8/portable-controller-build`; the top-level console log is `/workspace/scratch/f74ffd99d2f8/portable-controller-console.log`. These raw files are **not committed**. Their digests identify separately supplied evidence and do not replace access to that evidence.

This allocation verifies that the clean-source portable controller can produce and independently validate the required console populations. It contains no independent macOS repetition, no real audio-host run, and no product performance intervention. A/B differences measure sensitivity to instrumentation under this allocation; any future product performance change still requires an identified cause and independent repeated experiments under the appropriate macOS environment.
