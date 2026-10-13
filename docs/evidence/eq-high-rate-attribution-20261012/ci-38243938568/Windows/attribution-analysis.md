# Windows second observation: independent attribution audit

## Identity, scope and data integrity

Workflow **38243938568**, source **72e361ba91ea2c6b8d830921a71275fa25cba493**, artifact **11746006242**. Parent-verified ZIP SHA-256: `e8fcfc9ca1ff3b96d28564dd47cf0dd6c009997efdcf1684b7a5b1d849669701`. Inputs under `ci-artifacts/windows-72e361ba/eq-attribution` were read without modification. The first Windows execution built but failed provenance before the tests; it remains **NOT_RUN**, rather than receiving this later result retroactively.

The duration-scale-1 matrix has **90/90 structurally consistent cases**, **139,781 callback rows**, **2,026 update rows**, and zero structural/FIFO errors. Exact callback, FIFO and transform-window counts and independently calculated production ratios agree. Analyzer-active cases capture **9,942,912 frames**; rejection and discard counts are zero. Final pending data range **128–17,408 frames** in active cases and are included in conservation. Maximum sampled FIFO high-water is **17,280 frames**; a final pending count can exceed an earlier sampled maximum. Host latency is **42 samples in every case**.

Runner metadata is Windows Server 2025, version 10.0.26100, AMD64, 4 logical CPUs, `Intel64 Family 6 Model 207 Stepping 2, GenuineIntel`, image `win25-vs2026` version `20260925.250.1`. Cross-platform comparisons use different hardware and architectures and cannot isolate the OS alone. The native test window and synthetic source do not provide physical DAW/device evidence. **Performance acceptance remains NOT_ASSESSED.**

## CPU precision and unassigned process work

All reconstructed process and thread CPU deltas in this Windows artifact are multiples of **0.015625 CPU-seconds**. This is an observed limitation of these measurements, not a universal claim about API resolution. Short cases with reported CPU 0 still execute work. Case 19 has a process-minus-producer-minus-message residual of approximately **−1.0004e−11 percentage points**, or **−2.003e−13 CPU-seconds**. It is explicitly marked `CPU_COUNTER_QUANTIZATION_LIMIT`; no raw value is clipped. A nonnegative residual elsewhere does not certify either process or thread CPU as absolute truth.

Large native process-minus-thread residuals remain unassigned:

| 192 kHz, due pacing, 40 ms | Process CPU | Producer CPU | Message CPU | Unassigned residual |
|---|---:|---:|---:|---:|
| Full OFF, direct | 83.520% | 83.520% | 0.000% | 0.000%p |
| Full OFF, panel paint suppressed | 119.397% | 80.378% | 10.145% | 28.874%p |
| Full OFF, panel painted | 146.840% | 81.231% | 12.497% | 53.112%p |
| FFT 2048, panel painted | 177.153% | 81.162% | 24.973% | 71.017%p |
| FFT 4096, panel painted | 168.620% | 80.407% | 21.078% | 67.136%p |
| FFT 8192, panel painted | 186.607% | 88.229% | 24.204% | 74.175%p |

Percentages use one core as 100%; a process can use several cores. Additional process-thread activity accompanies the native editor, but the raw instrumentation does not identify those threads. Neither IR workers, renderer workers nor FFT work can be assigned the residual without an actual thread trace. The **67.1–74.2%p range refers specifically to painted 40 ms ON controls**, not the entire grid. Other native-painted cases reach a residual of approximately **124.903%p**. Paint callback wall itself is only 4.2–4.6% in the three ON controls above and cannot explain the whole residual.

## 192 kHz source and processing stages, analyzer OFF

| Stage | Legacy / due production | Reported producer CPU, legacy / due | Process p99, legacy / due |
|---|---:|---:|---:|
| No-op | 99.5652 / 99.9069% | 0.000 / 0.780% | 0.1 / 0.2 us |
| EQ pair | 99.4811 / 99.9014% | 0.780 / 0.781% | 27.5 / 32.3 us |
| Full processor | **53.5606 / 99.9451%** | **42.156 / 83.520%** | **815.1 / 701.7 us** |

The full-processor source underproduces before enabling either FFT or native UI. Its legacy mean process wall is **570.158 us**, versus **511.729 us** for due pacing; the period is **666.667 us**. There are **163/1,608** legacy and **71/3,001** due execution overruns. Wall spans can include descheduling and do not measure pure DSP instruction cost.

The no-op/EQ controls still have many late entries and catch-up blocks despite near-nominal aggregate production. The due no-op source records 1,886 late entries and 1,886 catch-up blocks with zero process execution overruns. A ratio close to 100% therefore does not establish accurate scheduling.

### Positive-request sleep observations

Only rows with `sleep_requested_us > 0` are included below. The raw summary's unfiltered wake quantile also contains zero-request catch-up rows and is a different statistic.

| Control | Legacy wake p50 / p99 / maximum | Due wake p50 / p99 / maximum |
|---|---:|---:|
| No-op | 1.762 / 1.985 / 2.053 ms | 1.405 / 1.902 / 1.918 ms |
| EQ pair | 1.492 / 1.970 / 2.472 ms | 1.439 / 1.883 / 2.084 ms |
| Full OFF | **1.801 / 1.869 / 2.026 ms** | **1.314 / 2.073 / 2.091 ms** |

Full OFF legacy has 578 positive sleep requests and due has 321. The unfiltered summary wake p99 values are 1.840 and 1.864 ms, respectively; they are not used as pure sleep-overshoot quantiles.

### The rebase trigger separates late entry from in-budget work

The legacy clock resets when `entry lateness + generation wall + process wall > 3 × period`, approximately 2 ms at 192 kHz. Full OFF legacy has **557 rebases**. Of these, **556 cross the threshold during the process wall interval**, but every one already enters more than one period late. Their median entry lateness is **1.803034 ms**, median generation **1.4 us**, and median process wall **0.5266 ms**. **551** follow a positive sleep request.

Only **19 of these 556 processor calls exceed their own 0.667 ms budget**. The other **537 are in budget**. The remaining rebase is already above three periods at entry: entry lateness 2.026686 ms, previous wake lateness 2.026486 ms, and process wall 0.6001 ms. Thus wake debt around 1.8 ms, often below the reset threshold on its own, plus a normally budgeted full process call can cross the reset threshold and discard source time. It would be incorrect to characterize every lost block as a DSP execution overrun.

The recorded **927.458764 ms** of discarded source time accounts for **99.7833%** of the measured **929.472600 ms** production-time deficit, leaving **2.013836 ms** of residual source debt. This is source-clock accounting, not proof that rebasing originally caused the wake delay. Capture boundaries and final wake/bookkeeping are not individually attributed by the residual.

The no-op control has only five rebases, all already above threshold after a positive sleep, with **6.751166 ms** discarded against an **8.704167 ms** deficit. The EQ-pair control has six: two already late at entry, one crossing in generation, and three crossing in process. None of its process calls exceeds one period. The much shorter EQ process work rarely adds enough time to the same broad wake-lateness distribution to cross the reset threshold.

### Generator control

Per-sample sin/fmod generation averages approximately **1.6 us per block** in the full controls. Precomputed generation reduces it to about **0.8 us**, but cached full legacy production changes from 53.5606% to **72.4834%** while mean process wall and wake phase also change. The production difference is not an isolated estimate of generator cost. Cached full due produces **99.9640%**. Cached no-op legacy remains **99.5703%**, close to its original 99.5652%; cached no-op does not exhibit the severe macOS pacing deficit.

Cached full legacy has 338 rebases: 337 cross during process, all already more than one period late; only eight of those calls exceed the block budget. The same wake-debt/processing interaction remains.

## Direct consumer, native dispatch and panel rendering

The following are the due-paced 192 kHz / 40 ms controls. Consumer maximum gaps exclude the first interval, which may contain setup.

| FFT | Direct message CPU / analysis wall | Native paint-suppressed message CPU / analysis wall | Painted message CPU / analysis wall / paint wall | Max gap, direct / suppressed / painted |
|---:|---:|---:|---:|---:|
| 2048 | 5.454 / 5.543% | 21.090 / 5.731% | 24.973 / 5.813 / 4.388% | 41.504 / 43.203 / 51.911 ms |
| 4096 | 6.236 / 5.082% | 14.819 / 5.268% | 21.078 / 4.503 / 4.180% | 41.087 / 43.147 / 58.191 ms |
| 8192 | 6.240 / 5.658% | 19.509 / 5.878% | 24.204 / 5.920 / 4.611% | 41.049 / 43.170 / 47.847 ms |

Panel painting adds measurable work and can extend consumer gaps, while other editor controls and path preparation remain active in the suppressed mode. Inclusive analysis/path/paint wall timers and event-dispatch timers overlap; they must not be summed and called CPU. Message CPU includes more than the analyzer and one paint callback. Sequential controls expose variation and do not constitute a fully isolated scheduler benchmark.

Native full OFF due production is **99.9208% suppressed / 99.8763% painted**, and legacy is **71.4675 / 67.5141%**. The two late repeated painted-OFF legacy endpoints produce **65.5910% and 65.5848%**, with corresponding due endpoints **99.8263% and 99.9097%**. Both native and direct full OFF failures exist; adding FFT is not necessary to produce them.

At 192 kHz, FFT 2048 paint wall equivalent for 20/40/80 ms is **5.809 / 4.079 / 4.387%**, and analyzer inclusive wall is **7.377 / 5.551 / 5.820%**. FFT 4096 paint is **5.930 / 4.430 / 4.278%** and FFT 8192 is **5.499 / 4.780 / 4.468%**. Longer cadence does not halve total measured Windows paint work at every step. Existing editor activity remains and its exact contribution is not traced. Linux's stronger inverse-cadence relation must not be assumed to apply quantitatively to Windows. No cadence, hop or FIFO policy was changed on this evidence.

## Four-rate native grid and retained scheduling failures

Each rate has twelve due/native-painted cases: FFT OFF/2048/4096/8192 by 20/40/80 ms.

| Rate | Production min–max | Callback total | Process execution overruns | Entries more than one period late |
|---:|---:|---:|---:|---:|
| 44.1 kHz | 99.8722–99.9416% | 4,973 | 8 | 188 |
| 48 kHz | 99.8704–99.9326% | 5,415 | 9 | 347 |
| 96 kHz | 99.8032–99.9693% | 10,870 | 85 | 3,081 |
| 192 kHz | 99.7340–99.9393% | 21,679 | **5,631** | **16,216** |

These short captures do not reject or discard FIFO frames, but they are not a no-overrun result. Case 34, native FFT 4096 / 40 ms, produces **99.9563%** with **591/3,001 execution overruns**, a **4.8048 ms** callback maximum, **6.712608 ms** maximum source-clock lateness and **2,843 catch-up blocks**. Case 81, FFT 4096 / 20 ms, produces **99.7875%** with **684/1,813 overruns**, **11.457324 ms** source-clock lateness and a longest **428-block catch-up sequence**. Due pacing preserves source debt by producing late blocks; it does not restore a hardware callback deadline.

Across the 90 cases, 15 retain the raw `UNDERPRODUCED` category. The others use the fixture's coarse near-nominal category; its 95% boundary is not a performance pass threshold. Numerical deficits, overruns, late entries and pending FIFO samples remain in the JSON.

## Cross-platform interpretation and saved outputs

macOS second-run no-op legacy produces **20.6886%**, with positive-request wake p50 **5.374 ms**; Windows produces **99.5652%**, with p50 **1.762 ms**. macOS no-op wakes commonly exceed the approximately 2 ms reset threshold before any processing. Windows full-route work commonly carries an already late entry over that threshold even when the process call itself is in budget. This supports separate fixture/scheduling and audio-route contributions. It does not identify a platform timer policy, physical-device stability or uniform native FFT performance.

`windows-second-audit.json` preserves all 90 cases, exact accounting, production-time debt, first-excluded gaps and separate CPU quality status. `windows-second-supplement.json` preserves every raw-derived rebase event, its preceding positive sleep and process-budget classification, process/thread residuals and inclusive dispatch measurements. `analyze_native_eq.py` and `supplement_native_eq.py` are the read-only derivation tools. Original CSVs and production/test C++ were not changed by this audit. The retained attack repair remains complete; physical-host and final test-installer acceptance remain **NOT_ASSESSED**.
