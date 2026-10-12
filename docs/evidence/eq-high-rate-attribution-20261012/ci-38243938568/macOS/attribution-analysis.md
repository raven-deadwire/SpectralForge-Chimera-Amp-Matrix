# macOS second observation: independent attribution audit

## Identity, scope and data integrity

Workflow **38243938568**, source **72e361ba91ea2c6b8d830921a71275fa25cba493**, artifact **11746655744**. Parent-verified ZIP SHA-256: `41658829fe1e6069fc63d33690f6587653cb72e2c49672e146388d15840aac75`. Inputs under `ci-artifacts/macos-72e361ba/eq-attribution` were read without modification. This remains separate from the first `ddb0880` observation.

The full duration-scale-1 matrix is **90/90 structurally consistent**, with **131,070 callback rows**, **1,297 update rows**, zero structural/FIFO errors, and a separate CPU-counter warning described below. Exact callback/FIFO/window counts and calculated production ratios agree. Analyzer-active cases capture **9,856,384 frames**; rejection and discard counts are zero. Final pending ranges **0–22,656 frames**, is included in conservation, and is not a loss. Maximum sampled FIFO high-water is **35,456 frames**. Host latency is **42 samples in every case**.

Runner metadata is macOS 15.7.9 ARM64, 3 logical CPUs, image `20260907.0337.1`. Recorded C++ source/header hashes match the first macOS observation. Neither a physical audio device nor a DAW was tested. **Performance acceptance remains NOT_ASSESSED.**

## Critical CPU-counter qualification

**Case 85, native-painted 192 kHz / 8192 / 40 ms, is CPU_COUNTER_INCONSISTENT. Its whole-process CPU and process-minus-thread residual are excluded from performance inference.**

| Recorded value | Percentage of one core | CPU seconds over the 1.201451458 s capture |
|---|---:|---:|
| Whole process | 15.386972% | 0.184867 s |
| Producer thread | 75.438753% | 0.906360 s |
| Message thread | 10.726775% | 0.128877 s |
| Process minus producer minus message | **−70.778557%p** | **−0.850370 s** |

The reported producer and message CPU alone sum to 1.035237 CPU-seconds, which cannot be contained in the recorded 0.184867 process CPU-seconds over the claimed window. The separately timed process wall spans total approximately 0.903841 seconds, but this does not certify the thread counter as absolute truth.

The process helper uses `CLOCK_PROCESS_CPUTIME_ID`; attribution thread CPU uses macOS `thread_info`. Their source hashes are unchanged between runs. Absolute start/end CPU counter snapshots were not exported, so this evidence cannot establish why the counters disagree. No counter implementation was changed, no raw value was clipped, and no additional CI was requested to invent a remedy. Structural/FIFO correctness, steady-clock production/callback wall timing and the separately measured paired-FFT CPU benchmark remain distinct measures. The first macOS observation's smaller case-14 warning is also preserved separately.

Other second-observation residuals are **0.258–0.522 percentage points** in direct cases, **2.189–3.560** in native panel-suppressed cases, and **1.992–8.073** in native painted cases excluding case 85. A nonnegative residual is a consistency observation, not proof of counter accuracy or attribution to a specific worker.

## 192 kHz source and processing stages, analyzer OFF

| Stage | Legacy / due production | Reported producer CPU, legacy / due | Process p99, legacy / due |
|---|---:|---:|---:|
| No-op | 20.6886 / 99.9083% | 0.809 / 1.494% | 0.125 / 0.084 us |
| EQ pair | 21.1358 / 99.7734% | 1.378 / 3.815% | 47.166 / 60.042 us |
| Full processor | 65.0940 / 99.7937% | 74.008 / 85.607% | 1,427.500 / 1,498.708 us |

The no-op control still underproduces severely with neither analyzer, native UI nor heavy audio computation. Cached no-op produces **18.5161% legacy / 99.9136% due**, so eliminating per-sample sin/fmod does not cure the source-pacing problem.

At 192 kHz the nominal period is **666.667 us**. Positive-request sleep statistics, excluding no-request catch-up rows, are:

| Control | Legacy wake p50 / p99 / maximum | Due wake p50 / p99 / maximum |
|---|---:|---:|
| No-op | 5.374 / 8.454 / 15.185 ms | 3.705 / 8.173 / 8.354 ms |
| EQ pair | 5.434 / 8.582 / 9.355 ms | 1.653 / 7.495 / 8.160 ms |
| Full OFF | 1.978 / 4.201 / 4.418 ms | 1.956 / 4.269 / 4.513 ms |

These sleep/wake delays are properties of the observed normal-thread fixture and runner. They do not identify a particular macOS timer policy or establish hardware callback behavior.

### Source clock loss and where the threshold was crossed

All **312 no-op**, **299 EQ-pair**, and **265 cached-no-op** legacy rebases were already above the approximately 2 ms/three-period threshold at entry, and every one followed a positive sleep request.

Full OFF legacy has 310 rebases: **121 already above threshold at entry**, **188 crossing during process wall**, and **one crossing during generation wall**. Only five of the 188 process-span events entered within one nominal period; most entered with existing lateness. Their rebased source-time contributions are **390.005486 ms**, **304.151598 ms**, and **2.423919 ms**, respectively.

The full OFF production-time deficit is **698.898501 ms**. Rebasing discards **696.581003 ms**, explaining **99.6684%** arithmetically; the **2.317498 ms** residual remains reported. Source-time accounting is not an exclusive causal diagnosis of the original delay: generation/process wall spans can contain descheduling.

Full OFF due restores **99.7937%** aggregate production but retains **998 execution overruns**, **2,698 source entries more than one period late**, **2,929 catch-up blocks**, **34.111 ms maximum source-clock lateness**, and **281 consecutive catch-up blocks**. The clock correction therefore cannot be labeled real-time stability.

## Consumer gaps, native dispatch and panel rendering

Headless direct 40 ms consumer maximum intervals, **excluding the first gap**, range **167.205–201.823 ms**. These occur without a native UI. For due 40 ms native controls, first-excluded maximum consumer gaps are:

| FFT | Panel paint suppressed | Panel painted | Analyzer inclusive wall, painted | Panel paint wall |
|---:|---:|---:|---:|---:|
| 2048 | 102.821 ms | 110.446 ms | 1.914% | 2.835% |
| 4096 | 116.675 ms | 110.377 ms | 1.773% | 2.936% |
| 8192 | 99.205 ms | 114.854 ms | 1.890% | 3.239% |

Reported message CPU rises from **7.837 to 11.019%**, **7.786 to 11.407%**, and **7.220 to 11.811%** with painting for those controls. Whole-process differences also include other activity and should not be treated as paint-only cost.

FFT OFF due heartbeat maxima are **90.127 ms** with panel painting suppressed and **91.000 ms** with painting. Thus delayed nominal 10 ms timer delivery persists without FFT work. First native OFF legacy case 23 has a larger **298.895 ms heartbeat maximum** and **284.252 ms CF-call maximum**. Heartbeat summary data include their first interval; the artifact lacks per-heartbeat rows to remove that first value, so this initial native outlier is not presented as a first-excluded sustained gap.

Across native controls 23–38, requested 1 ms dispatch calls occur approximately **131–169 times per second**. CF total wall time divided by dispatch count averages **5.50–6.49 ms per dispatch**, and AppKit polling **0.23–1.12 ms**. Polling reaches **63.920 ms** in case 27. These inclusive native-pump timings contain waiting and dispatched work. They must not be added to FFT/paint durations or presented as CPU. The real DAW's event loop differs from this test-only pump.

At 192 kHz, changing 20→40→80 ms cadence reduces panel paint wall from **3.343→2.694→1.841%** for 2048, **3.670→2.254→1.904%** for 4096, and **3.620→2.721→1.924%** for 8192. Analyzer wall remains around **1.60–1.94%**. No cadence or overlap-window change was applied; these are observed cost/latency tradeoffs.

## Four-rate native grid and retained deadline failures

Each rate has twelve due/native-painted cases for FFT OFF/2048/4096/8192 by 20/40/80 ms.

| Rate | Production min–max | Process execution overruns | Source starts more than one period late |
|---:|---:|---:|---:|
| 44.1 kHz | 98.7670–99.8804% | 13 | 3,376 |
| 48 kHz | 98.9713–99.9624% | 20 | 3,734 |
| 96 kHz | 99.4120–99.9112% | 134 | 7,173 |
| 192 kHz | 99.5007–99.9631% | 6,207 | 17,260 |

No grid row rejects or discards FIFO samples. However, case 73 (96 kHz / 8192 / 40 ms) retains a **72.361 ms** process maximum and **75.753 ms** source-clock lateness while producing **99.8345%**. Case 29 (192 kHz / 2048 / 40 ms, panel painting suppressed) produces **99.9781%** with **94.947 ms** source-clock lateness, **831/3,012 execution overruns**, and a longest **614-block** catch-up run.

Headless 8192 due case 22 produces only **95.2801%**, retaining **93.350 ms** source-clock lateness and **967/2,874 overruns**. It remains raw-labeled `NEAR_NOMINAL_DIAGNOSTIC` solely because the fixture threshold is 95%; its actual **4.7199-percentage-point deficit** is preserved and is not described as stable nominal production.

## Relationship to the first run

No-op legacy production is similar in the two runs (20.0712% then 20.6886%), and severe requested-sleep lateness persists. Full OFF legacy production and reported CPU vary substantially despite matching C++ hashes: 42.9728% production / 37.882% producer CPU in the first observation versus 65.0940% / 74.008% here. Those cross-run changes are not a code optimization result. The counter anomaly has also moved from a small-cost no-op row to case 85; both warnings remain distinct.

Saved outputs: `macos-second-audit.json` contains structural/FIFO results, all rates and a separate `cpu_measurement_quality` section. `macos-second-supplement.json` contains process/thread residuals, explicit inference exclusions, CF/AppKit metrics and every rebase-trigger event. Original CSVs and C++ remain unchanged. Windows and Linux second-run observations provide separate controls; actual DAW/device and final installer acceptance remain unassessed.
