# Independent native attribution audit: first Linux and macOS observations

## Identity and boundaries

Both datasets identify source `ddb0880bd61a4c794236783b869767b96208bf41`, workflow run **38240150849**, attempt 1. Linux job: **114780458034**. macOS job: **114780458469**. The parent agent downloaded and SHA-verified the artifacts; this independent audit read the extracted method manifests and raw summary/callback/update CSVs without modification.

Linux runner: Ubuntu 22.04 image `20261004.315.1`, x86_64, 4 logical CPUs. macOS runner: macOS 15.7.9 image `20260907.0337.1`, ARM64, 3 logical CPUs. Hardware, architecture and runtime differ, so absolute cross-platform CPU percentages are not an OS-only benchmark.

These are the first observed datasets. They must remain distinct from the portability-only `72e361ba` rerun, **38243938568**. Windows compiled in the first run but did not run tests because provenance validation failed; checkout line-ending conversion is an inference consistent with independent reproduction, rather than a directly recorded first-run hash/configuration. No Windows throughput result is inferred from that failure.

The completed transient repair and its earlier evidence remain preserved. This report diagnoses source, processor, analyzer, timer and rendering behavior; it does not repeat the original attack-loss repair.

**Every row retains `performance_acceptance=NOT_ASSESSED`. Actual DAW/audio-interface stability and October 23 installer acceptance are not established by these captures.**

## Evidence integrity

| Check | Linux | macOS |
|---|---:|---:|
| Expected / actual matrix cases | 90 / 90 | 90 / 90 |
| Raw callback records | 154,583 | 126,127 |
| Raw analyzer update records | 2,141 | 1,254 |
| Structural or FIFO-accounting errors | 0 | 0 |
| FIFO rejection / discarded samples | 0 / 0 | 0 / 0 |
| Captured frames across analyzer-active rows | 11,050,880 | 9,451,136 |
| Pending at stop, per active row | 0–7,680 | 0–18,560 |
| Largest sampled FIFO high-water | 15,616 | 36,096 |
| Host latency in every row | 42 samples | 42 samples |

The script validates exact case order/configuration, callback counts and sequence numbers, window counts, summary production ratios, and `initial pending + produced = consumed + discarded + rejected + pending`. Final pending frames are retained and are not counted as loss. First consumer gaps are excluded from the gap figures below because they may include setup.

The fixture defines `UNDERPRODUCED` as below 95%, not below 100%. Linux has 90 `NEAR_NOMINAL_DIAGNOSTIC` rows. macOS has 21 `UNDERPRODUCED` rows, corresponding to every legacy-pacing row, and 69 `NEAR_NOMINAL_DIAGNOSTIC` rows. Exact deficits and all below-nominal rows remain in the audit JSON. Neither classification is performance acceptance.

## 192 kHz source and audio-route controls

All these controls use FFT OFF and no native window. Block size is 128 frames; nominal block period is **666.667 us**.

| Stage | Linux legacy production | Linux due production | macOS legacy production | macOS due production |
|---|---:|---:|---:|---:|
| No-op producer | 99.9917% | 99.9947% | 20.0712% | 99.7218% |
| EQ pair | 99.9948% | 99.9947% | 18.1565% | 99.5940% |
| Full processor | 99.9960% | 99.9946% | 42.9728% | 99.8390% |

Linux direct CPU remains dominated by the full processor: approximately 85% of one core for the producer, versus approximately 1.8% for no-op and 3.4% for EQ pair. On macOS the no-op legacy producer uses just **0.847%** of a core, and its entire process uses **1.232%**, yet produces only **20.0712%** of nominal samples. FFT, panel painting and expensive audio DSP are absent from that control. Low legacy production therefore demonstrably occurs in the macOS test/runner source scheduling path itself.

### Requested-sleep evidence

These statistics include only raw rows with `sleep_requested_us > 0`; catch-up rows with no positive request are excluded.

| Source control | Linux positive-sleep wake p99 / max | macOS positive-sleep wake p99 / max |
|---|---:|---:|
| No-op legacy | 73.685 / 112.384 us | 8,427.251 / 11,920.376 us |
| No-op due | 76.439 / 103.503 us | 8,183.919 / 8,342.713 us |
| Full OFF legacy | 113.719 / 115.712 us | 4,265.879 / 4,729.303 us |
| Full OFF due | 114.051 / 117.366 us | 4,480.577 / 4,904.957 us |

The macOS no-op legacy median wake lateness is **5.591375 ms**, already over eight block periods. The due control retains these late wakes but issues catch-up blocks to recover aggregate production. This establishes an observed wake-pacing limitation in this normal-thread fixture; it does not identify a particular OS timer policy, CPU scheduling class or physical-device problem.

### Legacy source-time loss and trigger intervals

The source increments its deadline by one period before checking whether processing finished more than two periods late. The equivalent trigger is `entry lateness + generation wall + process wall > 3 periods`.

On macOS, all **293 no-op**, **277 EQ-pair**, and **279 cached-no-op** rebases were already over this threshold at entry. Every one followed a positive sleep request. Thus they were not first triggered by the current FFT or audio computation.

For macOS full OFF, **326 of 390 rebases** were already above threshold at entry, accounting for **1.065701 seconds** of discarded source time. The remaining **64** crossed during the measured process wall interval, accounting for **0.106435 seconds**; only four of those 64 entered within one block period of schedule. Full OFF's **1.172230-second** total production-time deficit is reconciled by **1.172136 seconds** of deadline rebasing (**99.9919%**), leaving **94.742 us** of residual debt. This is time accounting, not proof that the original delay was exclusively sleep or computation. Process and generation wall intervals can include descheduling.

Linux had only six rebases in the entire 90-case matrix: one in cached full OFF and five in native-painted 4096 legacy. All six crossed during the process wall span after entry was already more than one block late; their preceding raw records requested no sleep. Total rebased source time was **8.221386 ms**. The ordinary no-op, EQ-pair and full OFF stage controls had no rebases.

### Generator cache interpretation

Precomputing the identical tone does not cure macOS legacy underproduction: cached no-op produces **18.7663%**, versus **20.0712%** with sin/fmod. The no-op due controls both approach aggregate nominal rate. Direct measured generation cost is small relative to full audio processing. Full-route CPU and callback timing change substantially between sequential cached/uncached captures; those changes cannot all be assigned to eliminating a few microseconds of generation. No product optimization is justified solely by that uncontrolled full-route difference.

## Native rendering and update cadence

On Linux, panel rendering has a clear isolated cost. The 192 kHz due controls below share 40 ms requested cadence and full processing. CPU columns are measured message-thread CPU as a percentage of one core; paint is inclusive wall-time equivalent.

| FFT | Message CPU, panel paint suppressed | Message CPU, panel painted | Panel paint wall equivalent |
|---:|---:|---:|---:|
| 2048 | 22.965% | 36.925% | 12.349% |
| 4096 | 22.683% | 37.365% | 12.245% |
| 8192 | 26.071% | 40.025% | 12.566% |

Producer CPU stays approximately 86% in these paired controls. Suppression removes EQ-panel drawing only; other editor activity remains. This provides evidence of panel-rendering cost independent of the producer and audio route.

The Linux 192 kHz cadence grid confirms the distinction between rendering frequency and FFT throughput:

| FFT | Requested cadence | Analyzer inclusive wall equivalent | Panel paint wall equivalent | Message CPU |
|---:|---:|---:|---:|---:|
| 2048 | 20 / 40 / 80 ms | 18.768 / 18.598 / 18.433% | 25.273 / 12.111 / 5.779% | 52.757 / 36.842 / 28.847% |
| 4096 | 20 / 40 / 80 ms | 19.488 / 19.349 / 19.019% | 24.569 / 12.780 / 5.741% | 52.670 / 38.509 / 29.312% |
| 8192 | 20 / 40 / 80 ms | 20.935 / 20.563 / 20.743% | 24.887 / 12.625 / 6.072% | 54.205 / 39.285 / 31.401% |

Longer cadence reduces rendering and path-building frequency while preserving nearly the same scheduled overlap-FFT work per produced sample. It also increases display latency and queued work per update. Reducing unnecessary repaint or caching static panel content is supported as a candidate rendering optimization; these data do not authorize declaring the existing 40 ms cadence unstable in a real DAW, nor do they test such a cache implementation.

The macOS 192 kHz due 40 ms painted controls consume approximately **1.378–1.654% analyzer wall equivalent** and **2.074–2.408% paint wall equivalent**. Suppressed-to-painted message CPU changes from **6.129 to 9.044%** for 2048, **5.250 to 9.217%** for 4096, and **4.633 to 7.684%** for 8192. Full-route CPU also varies across these sequential trials, so process-wide differences are not a clean paint-only estimate. Linux absolute percentages should not be transplanted to macOS.

## FIFO consumers, timers and the macOS test-only pump

Linux headless direct 40 ms consumers have first-excluded maximum gaps of **40.019–40.055 ms**. macOS headless direct consumers have maxima of **168.019–189.060 ms**, with medians of **0.760–16.353 ms**: delayed updates are followed by rapid catch-up. A native UI is absent in these direct cases, so native panel painting is not required to produce long consumer gaps.

For macOS native due 40 ms, first-excluded consumer maxima are **84.213 / 101.622 / 95.275 ms** with panel paint suppressed, and **109.178 / 99.937 / 102.234 ms** with painting enabled, for FFT 2048/4096/8192. FFT OFF has no analyzer consumer records, but the independent nominal 10 ms heartbeat still reaches **90.142 ms** with painting suppressed and **110.933 ms** with painting enabled. Long timer delivery intervals therefore persist without analyzer work.

The macOS fixture dispatches approximately **156–171 requested 1 ms slices per second** in these native controls. Total CF wall time divided by dispatch calls averages approximately **5.56–6.07 ms per dispatch**, and AppKit polling approximately **0.21–0.74 ms**. In first native OFF case 23, a CF call reaches **91.894 ms**, and a poll reaches **44.455 ms**. Most other controls have CF maxima near 9–12 ms while heartbeat gaps remain near 80–90 ms. Aggregate maxima do not provide a timestamped dispatch trace that can assign every heartbeat gap to a single call or timer thread.

CF/AppKit/dispatch wall times contain waits and dispatched callbacks; they must not be summed with analyzer or paint times and called CPU. No `sendEvent` duration was recorded in these captures. The pump is test-only, and a real DAW supplies a different host event loop. The supported conclusion is a synthetic source/timer/runner scheduling limitation plus measurable rendering work, not a DAW failure diagnosis.

## The complete native rate/FFT/cadence grid

Each rate has 12 cases: FFT OFF/2048/4096/8192 by 20/40/80 ms, using due pacing and native painting. These bounds are observed source throughput, not performance acceptance.

| Rate | Linux production min–max | macOS production min–max | Linux / macOS process execution overruns across 12 cases |
|---:|---:|---:|---:|
| 44.1 kHz | 99.9902–99.9912% | 98.3717–99.7901% | 0 / 1 |
| 48 kHz | 99.9901–99.9916% | 98.5486–99.9575% | 0 / 1 |
| 96 kHz | 99.9905–99.9924% | 99.4375–99.9622% | 0 / 93 |
| 192 kHz | 99.9438–99.9942% | 99.7088–99.9909% | 785 / 2,405 |

No grid row rejects or discards FIFO samples. This states what was observed in short captures; it is not a guarantee over longer sessions or hardware-driven callbacks.

### Near-100% production can conceal timing failures

Linux native full OFF due case 26 produces **99.9938%** while recording **143/3,002 process execution overruns**, **3.612 ms** source-clock lateness, and **744 catch-up blocks**, with a longest run of **166**. Linux native 2048/20 ms case 78 produces **99.9910%** while recording **186/1,801 overruns** and **538 catch-up blocks**.

macOS native full OFF due case 26 produces **99.9048%** while recording **504/3,006 execution overruns**, a **26.352 ms** process maximum, **82.382 ms** source-clock lateness, and **2,723 catch-up blocks**, including **910 consecutive catch-up blocks**. These are decisive reasons not to label the corrected synthetic production rate as real-time or DAW stability.

## Process CPU residuals and measurement qualifications

Linux `process − producer − message` CPU residual is **0.643–0.931 percentage points** in direct cases and **1.146–1.848 points** in native cases. macOS direct residuals are generally approximately **0.1–0.36 points**, and native cases span **1.189–6.300 points**. These are unassigned process CPU, not identified IR or background-worker costs. Native controls include other editor/background activity.

macOS cached no-op due case 14 has a preserved **CPU_COUNTER_INCONSISTENT** warning: process **0.1772%**, producer **0.4703%**, message **0.0106%**, giving **−0.3038 points**. Its process aggregate and residual are explicitly excluded from performance inference. Independent process/thread counter sampling does not support an exact CPU-cost claim for that small-cost row. It is not negative worker CPU; the raw values were not clipped or rewritten. Absolute start/end counters were not exported, so the origin of the inconsistency cannot be reconstructed, and thread CPU is not certified as absolute truth. Structural/FIFO audit consistency is a separate check from CPU-counter fidelity; the audit JSON now has a distinct `cpu_measurement_quality` warning section.

The local workspace cgroup comparison is separate from these CI runs. Its 103.516 CPU-seconds include all cgroup processes and setup/output windows; the local test process used only 18.516 CPU-seconds within 39.236 seconds of measured captures, with 0.483 CPU-seconds left after producer/message attribution. Large local cgroup usage should not be described as hidden processor-worker cost.

## Reproducible scratch outputs

- `linux-native-audit.json` and `macos-native-audit.json`: exact matrix/raw accounting, source-time reconciliation, positive-request sleep diagnostics and every numeric row.
- `linux-native-supplement.json` and `macos-native-supplement.json`: process/thread residuals, native dispatch metrics and raw rebase-trigger classifications.
- `analyze_native_eq.py`: read-only standard-library audit accepting one extracted `eq-attribution` directory.

This first-run audit contains no Windows controls. The completed second-run Windows controls are recorded separately in `ci-38243938568/Windows`; the first failure remains tests NOT_RUN. These observations support retaining the completed attack fix, the independently measured FFT optimization, honest source-clock instrumentation and targeted rendering-cost follow-up. They do not establish physical-device/DAW acceptance or final test-installer readiness.
