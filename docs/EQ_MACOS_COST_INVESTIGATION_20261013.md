# EQ macOS cost investigation — 2026-10-13

## Decision

The repeated macOS regression is real as a descriptive observation, but its
cause is **not identified** by the existing measurements. Retain PR #52 as a
development candidate. No production optimization or sample/window dropping is
justified by this investigation. October 23 physical DAW/interface/installer
acceptance remains **NOT_ASSESSED** and withheld.

Reviewed source: PR #52 `a2ae89467665b9122b7d1ae744dc52fb8f693bd6`.
PR #59 head: `a47e879b7e1259b5d3ac71243be3bd187de0c1ec`, base exactly
`a2ae894`. All original evidence and frozen reference bytes are preserved.

## Recalculation of the actual macOS trials

Both 192 kHz / 2048 / full-FIFO datasets contain four trials per arm,
65,536 frames and 128 windows per trial, with no drops. Recalculation from the
original CSV retains 36.591% and 6.231% update-median increases and 26.586% and
7.773% thread-CPU-median increases. See
[evidence](evidence/eq-cost-investigation-20261013/historical-recalculation.json)
for every row and the original CSV hashes.

Second execution, update / inclusive FFT timer / feed-plus-analysis CPU, ms:

| Sequence | Arm | Update | FFT timer | CPU |
|---:|---|---:|---:|---:|
| 0 | baseline | 6.106 | 2.875 | 6.448 |
| 1 | candidate | 4.029 | 0.977 | 5.428 |
| 2 | candidate | 4.417 | 0.951 | 5.351 |
| 3 | baseline | 5.673 | 2.015 | 7.242 |
| 4 | candidate | 8.163 | 2.951 | 8.604 |
| 5 | baseline | 4.629 | 1.771 | 6.250 |
| 6 | baseline | 5.040 | 2.355 | 6.455 |
| 7 | candidate | 6.963 | 2.169 | 8.478 |

The candidate first improves and later worsens within the same executable.
Its real-transform timer changes by more than 3x. CPU also changes, so an
explanation involving only time spent descheduled is insufficient. Actual
Apple core type, migration, frequency, thermal state and counter snapshots are
absent; **P/E-core migration and frequency changes are hypotheses, not findings**.

The same 2048/full-FIFO workload has the same frame/window count at every rate,
although the historical fixture generates different sample values by rate and
executes rates in fixed order. At 48 kHz the second execution also regresses
(7.719 -> 8.598 ms); at 96 kHz it is nearly equal (5.236 -> 5.259 ms).
The first execution improves at both those rates. Thus the existing evidence
does not establish a uniquely 192 kHz algorithmic defect. Four observations
per arm do not provide a robust uncertainty estimate.

## Attribution boundaries and hypotheses

| Component | Evidence and conclusion |
|---|---|
| Native UI/path/paint | Not present in paired cost executable. Cannot cause this paired regression. Native attribution captures independently show rendering costs. |
| Synthetic pacing | No producer sleep/rebase thread in paired executable. Cannot cause this paired regression. Native no-op OFF underproduction is a separate scheduling defect in the fixture. |
| FFT backend | Both arms call the same JUCE 8.0.8 real FFT; baseline's frequency-only wrapper additionally takes magnitudes and clears the tail. Narrow historical timers cover unequal work. Backend timing variability is observed; its physical cause is not recorded. |
| FIFO/history/window | Production FIFO push/pop and chronological window traversal are unchanged. Feed residual medians rise in run 1 but fall in run 2. No direct memory/cache/FIFO-stage counters exist; no causal FIFO regression has been proved. |
| Complex power | Candidate removes sqrt/square, uses double real/imag power with two finite checks and overflow bound, caches gain, and removes tail clearing. More complex input loads/guards could affect generated code. Existing residuals do not isolate this loop. |
| Tail memory | Apple backend normalizes 2N floats; ignored negative-frequency tail is not consumed by the analyzer. A resolution change can leave old tail values, unlike a fresh benchmark instance. Consider a dedicated size-switch/tail control if investigating production behavior; it does not explain fresh-instance rows by itself. |
| CPU counters | Paired macOS CLOCK_THREAD_CPUTIME_ID differs from inconsistent native aggregate counters. CPU increases cannot simply be dismissed, but neither counter proves core/frequency stability. Windows short counters are quantized. |

Median update-minus-FFT residuals rise from 3.326 to 4.107 ms and 3.044 to
4.130 ms. These are **unequal timer residuals**, not measurements of FIFO,
memory bandwidth or power-loop cost. Removing wrapper magnitude/tail work
from the timed FFT interval automatically moves some work outside that
interval. Do not compare residuals as an identical stage A/B.

## New manual experiment

`ChimeraEQAnalyzerCostInvestigation` adds test-only measurements:

- Twelve rounds, four rates, four independent instances (baseline, candidate,
  baseline A/A and candidate A/A), sixteen full-FIFO replays per cell. Seeded
  shuffled order replaces fixed rate/implementation ordering.
- Identical 192 kHz-generated input bytes across all four rate settings,
  isolating sample content from sample-rate-dependent smoothing math.
- Per-rate instances prevent intentional rate-switch discontinuity counters
  from invalidating a steady-state test. Every replay checks exactly 128
  windows, 65,536 samples, empty final FIFO and zero drops/discontinuities.
- Absolute thread-CPU and steady-clock endpoints, start/end Linux CPU IDs.
  Apple core/frequency placement is explicitly NOT_OBSERVED. Start/end IDs
  cannot detect all intervening migrations, nor prove fixed frequency.
- An isolated 24-round/1024-transform stage control times preparation, the
  **same real FFT API** and postprocessing separately. Baseline reproduces
  JUCE magnitude/tail behavior; candidate uses the changed power equations.
  It is a separate workload, not production full-update stage attribution.
  Checksums are consumed outside wall-stage timers to prevent dead-code
  elimination; reported thread CPU includes that checksum and timer overhead.
- Existing 108 spectral parity cases and finite/corrupt-input recovery run
  first. No waits/allocations/observers are added to production callbacks.

Build and run on any supported development OS:

```sh
cmake -S . -B build-cost -DCMAKE_BUILD_TYPE=Release -DCHIMERA_EQ_DEVELOPMENT_PREVIEW=ON
cmake --build build-cost --config Release --target ChimeraEQAnalyzerCostInvestigation --parallel 2
python3 Tools/run_eq_cost_investigation.py build-cost
```

The dedicated three-OS workflow retains all raw rows, source/binary hashes,
CMake flags and runner metadata. Its successful exit means functional/sample
accounting checks passed, **not** a performance regression clearance. The
experiment is excluded from CTest and release/product targets. These are
within-process controls on one runner, not an OS-only hardware comparison.

## Minimal improvement candidates and PR #59 dependency

First obtain longer macOS randomized A/B + A/A data and same-kernel stage
controls. If the power loop remains slower with stable execution observations,
consider peeling DC/Nyquist endpoints out of the interior loop, preserving
all finite/overflow bounds, one-sided gain and stereo power meaning. This is
an unimplemented hypothesis requiring before/after measurement, corruption
recovery and spectral parity. No measured basis currently supports an Apple-only
fallback, restoring sqrt calculations or changing overlap/cadence/FIFO capacity.

Rendering caching is a separate candidate for native editor CPU. It cannot
resolve a regression in this headless executable. Worker-thread analysis is
not a minimal change and requires lifecycle/freeze continuity qualification.

PR #59 changes this same analyzer for completed-frame storage, candidatePower,
Freeze/thaw and callback acknowledgement. It adds a distinct incremental
memory/copy cost. A PR #59-versus-#52 comparison is needed separately; the old
beb72f comparison does not isolate it. This investigation modifies no analyzer,
processor, panel, state schema or host parameter and can be layered without a
Source conflict. CMake target registration/workflow additions still need normal
integration review. Do not replace #59 freeze validation with this probe.

## October 23 acceptance remains separate

Require the actual selected installation package and source SHA, supported
DAW/plugin format/driver/interface/rate/buffer, sustained playback/recording,
analyzer OFF and all FFT sizes, visible/hidden editor, representative amp/cab
routes, automation/state/freeze/thaw/stop/restart/offline render, multiple
instances and teardown. Record device xruns/dropouts, callback tails, FIFO
continuity, transient audition, original 42-sample latency and old project/
preset round trips. Unsupported device rates are NOT_RUN. Synthetic catch-up
production ratio or a functional CI pass does not satisfy this gate.

## Completed local Linux observations

Direct GCC 13.3 / JUCE 8.0.8 / Release-style -O3 build (not a full product
CMake build), twelve rounds and sixteen replays per cell: 192 cells / 3,072
full-FIFO replays, exact sample/window accounting and no drops. Spectral parity
108/108 passed, maximum 0.000015259 dB; nonfinite/overflow recovery passed.
All raw CSVs, run log, source/binary hashes and module build recipe are retained
[in the evidence folder](evidence/eq-cost-investigation-20261013/).

| Rate | Update median baseline -> candidate, ms per replay | Reduction | Thread CPU reduction |
|---:|---:|---:|---:|
| 44.1 kHz | 36.660 -> 32.720 | 10.748% | 10.489% |
| 48 kHz | 34.721 -> 32.472 | 6.477% | 2.666% |
| 96 kHz | 34.645 -> 37.888 | **-9.362%** | **-9.628%** |
| 192 kHz | 35.498 -> 31.197 | 12.118% | 11.690% |

These are ratios of implementation medians, not medians of round-wise ratios.
All counterexamples are retained. At 192 kHz baseline A/A apparent reductions
range from -89.38% to +80.87%, candidate A/A from -99.49% to +60.66%.
At 96 kHz the baseline/candidate ratio-of-medians regresses while the median of
round-wise ratios improves. The result is highly variable; it does **not**
establish a new Linux algorithmic regression or clear the original macOS one.

The separate stage control (1024 transforms, 24 observations per arm) records
real-FFT medians 44.715/43.805 ms and post-stage medians 32.954/2.758 ms.
It supports the expected advantage of the power rewrite on this Linux backend
and isolated input. It neither tests Apple's vDSP nor explains full-update
variance. CPU includes checksum/timing overhead; wall-stage intervals exclude
checksum. Do not extrapolate these stage reductions to a production callback.

A first development probe invocation failed its discontinuity assertion because
instances were reused across rate changes while the existing helper requires
zero cumulative discontinuities. The fixture was corrected to use per-rate
instances; no production reset semantics or assertion was relaxed. That failed
invocation is not counted among completed measurements.

### Linux affinity control (separate execution)

A second invocation restricts **only the test process** to one allowed Linux
CPU. Six rounds / sixteen replays, 96 cells / 1,536 full-FIFO replays, all
108 parity cases and corruption recovery pass with exact accounting and zero
drops. Observed start/end CPU IDs match the requested affinity for every row.
This limits CPU migration; it does not fix frequency, VM scheduling or load.

| Rate | Update reduction | Thread CPU reduction |
|---:|---:|---:|
| 44.1 kHz | 9.381% | 9.449% |
| 48 kHz | 10.372% | 11.015% |
| 96 kHz | 17.340% | 17.357% |
| 192 kHz | 3.299% | 3.334% |

These independent execution medians all improve. This is consistent with an
execution-environment contribution to the first execution's variability, but
is **not** an isolated before/after effect of pinning: load/frequency/time also
changed. A/A variation remains visible in the raw data. It does not identify
Apple core migration or clear macOS performance acceptance. No CPU affinity or
scheduling change is added to the product.
