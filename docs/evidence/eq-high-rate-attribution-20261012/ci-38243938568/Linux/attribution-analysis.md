# Linux second observation: independent attribution audit

## Identity and scope

Workflow run **38243938568**, attempt 1; source **72e361ba91ea2c6b8d830921a71275fa25cba493**; artifact **11745561522**. ZIP SHA-256 supplied and verified by the parent: `843ba2497e463daf72159ad0bb26224b463f96076237f83ed451f7711988291a`.

Inputs are the unchanged files under `ci-artifacts/linux-72e361ba/eq-attribution`. The method manifest specifies the full **90-case** matrix, duration scale 1. This report is a separate observation from run 38240150849/source ddb0880. No input CSV, C++ source or fixture execution was changed by this audit.

Recorded runner metadata: Linux 6.8.0-1064-azure, x86_64, 4 logical CPUs, Ubuntu 22 image `20261004.315.1`. All recorded `Source/` and `Tests/` `.h`, `.cpp` and `.mm` SHA-256 values match the first Linux observation exactly. The record does not identify the underlying physical CPU model or all competing workloads.

**Performance/DAW acceptance remains NOT_ASSESSED.**

## Exact accounting

The audit is structurally **CONSISTENT**: 90/90 cases in the expected order and configuration; **154,402 callback rows**; **2,137 update rows**; zero errors or warnings. Callback count/sequence, FFT-window totals, production ratios and full FIFO conservation match the summaries.

Analyzer-active cases capture **11,027,968 frames** in aggregate. Rejection and discarded counts are zero. Pending at capture end ranges from **0 to 15,488 frames** and is accounted for; it is not treated as loss. Largest sampled FIFO high-water is **15,616 frames**. Every row reports **42 samples** of host latency.

The fixture's `UNDERPRODUCED` label means below 95%; no row reaches that threshold. The exact lowest observed production is **98.3728%** in case 28, and its 1.6272-percentage-point deficit remains visible. No `NEAR_NOMINAL_DIAGNOSTIC` label is interpreted as a performance pass.

## 192 kHz stages with analyzer OFF

No native window is present in these stage controls. Nominal 128-frame period is **666.667 us**. CPU percentages use a single core as 100%.

| Stage | Production, legacy / due | Producer CPU, legacy / due | Process CPU, legacy / due | Process p99, legacy / due |
|---|---:|---:|---:|---:|
| No-op | 99.9941 / 99.9957% | 0.647 / 0.793% | 0.893 / 1.069% | 0.030 / 0.030 us |
| EQ pair | 99.9958 / 99.9958% | 1.434 / 1.621% | 1.669 / 1.886% | 7.341 / 9.865 us |
| Full processor | 99.9959 / 99.9957% | 51.075 / 50.690% | 51.295 / 50.923% | 357.737 / 384.498 us |

Full-route mean process wall time is **335.620 / 332.925 us** for legacy/due; generation mean is **1.325 / 1.340 us**. The corresponding EQ-pair process means are **5.644 / 5.693 us**. The full audio route remains the largest measured cost even with the analyzer off.

No stage control rebases its clock. Nevertheless, full legacy records one process execution overrun and three catch-up blocks; full due records **10 overruns, 22 catch-up blocks and 1.246 ms maximum source-clock lateness**. Average production near 100% does not remove these misses.

Positive-request sleep statistics exclude zero-request catch-up rows:

| Stage | Wake p99, legacy / due | Wake maximum, legacy / due |
|---|---:|---:|
| No-op | 144.262 / 228.487 us | 449.399 / 608.894 us |
| EQ pair | 56.521 / 240.561 us | 253.321 / 646.168 us |
| Full OFF | 58.641 / 58.429 us | 332.905 / 243.757 us |

Cached no-op retains 99.9957/99.9959% production with 0.387/0.463% producer CPU. Cached full OFF retains 99.9955/99.9957% with 49.919/50.281% producer CPU. The directly measured generator mean falls to approximately 0.11–0.18 us; sequential full-route differences are not a clean causal speedup estimate.

## Legacy clock loss and observed trigger intervals

Only five cases contain rebases. The trigger is `entry lateness + generation wall + process wall > 3 nominal block periods`. Interval crossings may include descheduling and do not uniquely identify computation as the cause.

| Case | Control | Production | Rebase events | Rebased source time | Residual source-time debt |
|---:|---|---:|---:|---:|---:|
| 21 | Direct 8192 legacy | 99.8420% | 2 | 3.081333 ms | 89.334 us |
| 23 | Native OFF, panel suppressed, legacy | 99.8482% | 2 | 2.951815 ms | 85.295 us |
| 27 | Native 2048, panel suppressed, legacy | 99.8592% | 1 | 2.725196 ms | 95.291 us |
| 28 | Native 2048, painted, legacy | 98.3728% | 17 | 32.458749 ms | 104.916 us |
| 36 | Native 8192, painted, legacy | 99.6588% | 4 | 6.721625 ms | 104.868 us |

Case 27's sole event is already beyond three periods at entry and follows a positive sleep request. The other **25 events** cross during process wall spans. Eleven of those 25 entered within one nominal period. Total rebased source time is **47.938718 ms**. For case 28, rebases explain **99.6778%** of the measured 32.563665 ms source-time shortfall; the remaining 104.916 us is retained rather than forced to zero. This is source-time accounting, not exclusive attribution of the first delay to the DSP.

## UI rendering and consumer cadence

The 192 kHz due controls at 40 ms continue to expose panel rendering cost:

| FFT | Message CPU, suppressed / painted | Analyzer inclusive wall, suppressed / painted | Painted panel wall equivalent |
|---:|---:|---:|---:|
| 2048 | 16.064 / 24.337% | 13.987 / 12.833% | 8.127% |
| 4096 | 14.885 / 23.777% | 13.163 / 12.922% | 7.855% |
| 8192 | 18.130 / 25.288% | 15.608 / 14.012% | 8.150% |

Suppression applies only to panel drawing. Analyzer wall times are inclusive, not independently measured CPU. The six first-excluded maximum consumer gaps are **40.803–41.901 ms**; the corresponding heartbeat maxima are **16.186–19.875 ms**. Direct 40 ms consumers have first-excluded maxima **40.006–40.328 ms**. First gaps are retained separately in JSON and are not used in those bounds.

The 192 kHz native grid still shows the same cadence tradeoff:

| FFT | Cadence | Analyzer inclusive wall | Panel paint wall | Message CPU |
|---:|---:|---:|---:|---:|
| 2048 | 20 / 40 / 80 ms | 12.235 / 13.587 / 12.674% | 14.602 / 8.666 / 3.757% | 31.046 / 25.715 / 18.562% |
| 4096 | 20 / 40 / 80 ms | 13.132 / 12.913 / 14.004% | 15.394 / 8.181 / 4.080% | 33.181 / 24.226 / 20.465% |
| 8192 | 20 / 40 / 80 ms | 14.351 / 15.830 / 13.899% | 15.843 / 9.577 / 4.244% | 34.439 / 28.980 / 20.425% |

Rendering cost falls with fewer updates, whereas overlap-FFT cost remains broadly stable with sequential-run variation. No cadence, overlap/window coverage or UI implementation change was applied. The evidence supports recording rendering-cost candidates with their display-latency and per-update backlog tradeoffs.

## Complete native grid, four rates

Each rate has 12 due/native-painted cases covering FFT OFF/2048/4096/8192 and 20/40/80 ms.

| Rate | Production min–max | Process execution overruns | Source starts more than one period late |
|---:|---:|---:|---:|
| 44.1 kHz | 99.8285–99.9940% | 0 | 1 |
| 48 kHz | 99.9930–99.9941% | 0 | 0 |
| 96 kHz | 99.9864–99.9940% | 0 | 0 |
| 192 kHz | 99.9930–99.9961% | 140 | 132 |

All FIFO counters reconcile without rejection/discard. Short-capture rate bounds and FIFO continuity are not evidence of long-session hardware callback stability.

One explicit counterexample to acceptance by average rate is **case 25**, native full OFF due with panel painting suppressed: **99.9957% production**, but **77 process execution overruns**, **128 late source starts**, **10.604596 ms source-clock lateness** and **143 catch-up blocks**. Case 26 with painting enabled likewise delivers **99.9948%** while retaining 61 overruns and 7.531545 ms maximum source lateness. These rows remain unfiltered.

## CPU residuals and comparison with the first observation

`process − producer − message` CPU residual is **0.179–0.265 percentage points** in direct cases, **0.354–0.548** in native suppressed cases, and **0.301–0.650** in native painted cases. No negative residual is present. These small unassigned costs do not identify particular worker threads.

All captured C++ source hashes match the first observation, and the listed runner image/architecture/CPU count also match. Yet full OFF producer CPU falls from **85.285/84.900%** in the first run to **51.075/50.690%** here; native 40 ms paint wall cost falls from approximately 12% to approximately 8%. Those cross-run differences must not be claimed as a code optimization. The available provenance does not isolate physical CPU model, scheduling competition, frequency policy or other execution-environment differences. Matched old/new FFT measurements within one benchmark remain the appropriate basis for the candidate code optimization.

## Saved audit data

- `linux-second-audit.json`: exact matrix and raw FIFO/source accounting, every case, positive-request sleep distributions and legacy source debt.
- `linux-second-supplement.json`: process/thread residuals, dispatch fields and each raw rebase-trigger event.
- Input dataset and first-observation reports remain unchanged.

This second Linux observation preserves the same conclusions about separating aggregate source recovery, analyzer/rendering cost and real-time deadlines. Windows and macOS second observations are separate records; no DAW/audio-interface acceptance is inferred here.
