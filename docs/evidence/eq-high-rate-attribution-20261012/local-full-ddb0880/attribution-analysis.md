# Independent audit: local Linux headless attribution

Source stamp: `ddb0880bd61a4c794236783b869767b96208bf41`.

Inputs: `eq-local-build/eq-attribution-headless/eq-attribution-{method.txt,summary.csv,callbacks.csv,updates.csv}` and `eq-local-build/local-validation-state.json`. The raw CSV files were read without modification. Reproducible audit: `python analyze_native_eq.py eq-local-build/eq-attribution-headless --output local-headless-audit.json`.

## Evidence scope and integrity

The manifest specifies the complete **22-case headless subset**, duration scale 1. It does not contain the native 90-case matrix. Case ordering/configuration, 51,423 callback rows, 300 update rows, callback counts, sequence numbers, FFT-window counts and FIFO accounting reconcile with zero errors or warnings. Six analyzer-active captures produced 2,302,208 frames in aggregate; their consumed-plus-pending counts account for every captured frame. All rejection and discard counts are zero. End-of-capture pending frames are 384–768 per analyzer-active case and are not losses. Every case reports 42 samples of host latency.

The separate local JUnit file contains four tests, zero failures and zero skips: GateProcessor, GraphicalEQ, EQAnalyzer and EQAnalyzerPerformance. This attribution run itself is a tone/source timing experiment, not another attack-detection repair.

**Native UI: NOT_RUN. DAW/performance acceptance: NOT_ASSESSED.** The source is an ordinary synthetic thread with 128-frame blocks, not an audio interface clock. At 192 kHz the nominal block period is 666.667 microseconds.

## 192 kHz stages, analyzer OFF

CPU percentages below are actual thread/process CPU-time deltas as a percentage of one core. Legacy and due cases are sequential controls, not randomized repeated trials.

| Stage | Legacy production | Due production | Producer CPU, legacy / due | Callback p99, legacy / due | Execution overruns, legacy / due |
|---|---:|---:|---:|---:|---:|
| No-op producer | 99.9893% | 99.9939% | 2.259 / 2.653% | 0.031 / 0.031 us | 0 / 0 |
| EQ pair | 99.5003% | 99.9931% | 4.106 / 3.587% | 34.762 / 12.609 us | 1 / 0 |
| Full processor | 97.3701% | 99.9929% | 80.441 / 76.511% | 664.984 / 718.445 us | 30 / 53 |

The full route dominates the local measured cost even with the analyzer OFF. Its legacy mean process interval is 527.484 us (77.042% of total capture wall time), versus 8.671 us / 1.294% for the EQ pair. Generator work is separately measured and is approximately 3 us per block. This supports investigating the full route and synthetic scheduling separately from FFT work; it does not locate an expensive substage within the full processor or establish a product defect.

For full-processor legacy case 5, total measured missing source time is **52.614145 ms**. Recorded deadline rebases discarded **52.483960 ms**, explaining **99.7526%** of the source-time shortfall. The residual is **0.130185 ms**, or 0.1953 block periods. Eleven of the twelve rebase rows contain a measured process interval longer than a block; the other follows a 6.698516 ms wake lateness from a positive sleep request. These are interval-attribution observations: a long process wall interval can include thread descheduling, so they do not prove eleven expensive DSP computations.

The due source preserves clock debt and delivers 99.9929% aggregate production for the full route, but still has **53 process execution overruns and 257 catch-up blocks**. It corrects synthetic source-time accounting; it does not prove real-time scheduling.

The rebase trigger is `entry lateness + generation wall + process wall > 3 block periods`, because the scheduled deadline advances by one period before comparison against deadline plus two periods. Raw event reconciliation in `local-headless-rebase-events.json` finds that all five EQ-pair rebases were already over this trigger at entry. For full OFF, one rebase was already over the trigger at entry and eleven crossed during the process wall span; ten of those eleven entered no more than one period late. No rebase crossed the trigger during generation. This distinguishes observed timing intervals while retaining scheduler/descheduling uncertainty.

## Generator cache control

Caching the identical tone reduces full-route generator mean duration from 3.219 to 0.541 us/block in legacy mode and from 3.437 to 0.391 us/block in due mode. Generator wall cost falls from 0.470 to 0.081 percentage points and from 0.516 to 0.059 percentage points, respectively. The no-op CPU controls fall by 0.377 and 0.720 percentage points.

Full cached legacy production is 99.6736%, versus 97.3701% uncached. However, process mean duration and scheduling outliers also change across these single sequential captures. The 2.3035-percentage-point production difference cannot all be assigned to sin/fmod generation. The directly measured generator cost is much smaller than full-route processing cost.

## 192 kHz direct FFT controls, 40 ms updates

Analyzer/update percentages are **inclusive wall-time equivalents**, not measured thread CPU and not additive to FFT percentages.

| FFT | Production, legacy / due | Message thread CPU, legacy / due | Analyzer wall equivalent, legacy / due | Analyzer max duration, legacy / due | Process execution overruns, legacy / due |
|---:|---:|---:|---:|---:|---:|
| 2048 | 99.7264 / 99.9881% | 5.341 / 5.892% | 5.238 / 5.840% | 3.149 / 3.869 ms | 56 / 46 |
| 4096 | 99.4226 / 99.9925% | 4.297 / 5.043% | 4.250 / 4.971% | 2.134 / 3.908 ms | 50 / 73 |
| 8192 | 99.5591 / 99.9943% | 5.878 / 6.543% | 5.823 / 6.484% | 3.544 / 8.038 ms | 52 / 36 |

Across these six direct cases, analyzer-inclusive wall cost is 4.250–6.484% and process CPU is 80.949–86.040%. Consumer maximum intervals **excluding the first** are 40.027–41.009 ms. Sampled FIFO high-water is 7,680–9,984 frames, well below capacity, with no observed rejection/discard. The headless consumer does not reproduce the historical macOS native timer gap.

The due 4096 case is a particularly important counterexample to acceptance by average production: it delivers **99.9925%** while retaining a **23.252268 ms** process maximum, **30.635666 ms** source-clock lateness, **73** execution overruns, **495** catch-up blocks and a longest consecutive catch-up run of **185** blocks. Near-nominal aggregate source delivery hides these timing failures unless the raw timing evidence is retained.

The full OFF due process CPU is 77.686%; the direct FFT due cases are 80.949–85.384%. The ordering of individual FFT sizes is based on short sequential captures and should not be described as a definitive crossover or platform-wide ranking. The separate matched old/new FFT benchmark is the appropriate evidence for the candidate optimization.

## Lower-rate source controls

| Rate | Full OFF production, legacy / due | Producer CPU, legacy / due | Execution overruns, legacy / due |
|---:|---:|---:|---:|
| 44.1 kHz | 99.9885 / 99.9843% | 16.318 / 17.209% | 0 / 0 |
| 48 kHz | 99.9876 / 99.9859% | 17.474 / 17.460% | 0 / 1 |
| 96 kHz | 99.9893 / 99.9881% | 35.914 / 38.379% | 7 / 2 |

These lower-rate headless cases are source controls only, with analyzer OFF. They are not the missing native rate/FFT/cadence matrix.

## Scheduler evidence and limits

Positive-request sleep filtering matters. For the full OFF legacy case, wake-lateness p99 is 109.119 us and maximum 6.698516 ms. For full OFF due it is 131.330 us / 0.294010 ms. Zero-request catch-up rows are excluded from those sleep statistics. In due FFT 4096, the positive-sleep maximum is only 0.253460 ms while process maximum is 23.252268 ms, so treating every recorded wake-late value as pure sleep jitter would misattribute this run.

The cgroup quota remained `800000 100000` (eight CPU-seconds per second). During the 40.777135-second attribution stage, cgroup usage increased by 103.516106 CPU-seconds, averaging 2.53858 cores; both `nr_throttled` and `throttled_usec` increased by zero. Functional/paired validation likewise had zero new throttling. These cgroup-wide counters do not isolate this test process and do not exclude host scheduling, competition or virtual-machine pauses. The evidence rules out newly recorded cgroup quota throttling during this local stage, not all execution-environment interference.

The process-minus-producer-minus-message CPU residual is 1.321/1.254 percentage points for no-op legacy/due, 1.427/1.170 for EQ pair, and 1.245/1.114 for full OFF. Across all 39.236086 seconds of capture windows, the test process used 18.516390 CPU-seconds: producer 17.354077, message 0.679567 and residual 0.482746. The cgroup's 103.516106 CPU-seconds use a larger process scope and include the 1.541050 seconds outside those capture windows. Large process-internal IR/background work during capture does not explain the cgroup excess: it would have appeared in process CPU, whose unassigned residual averages only 1.230 percentage points. Preparation/output work and other cgroup processes remain aggregated in the cgroup counter; their individual identities were not measured.

The fixture labels `UNDERPRODUCED` only below 95%. Thus the 97.3701% row is raw-labeled `NEAR_NOMINAL_DIAGNOSTIC`. The audit preserves the exact 2.6299-percentage-point shortfall and all below-100% rows separately. Neither label is a performance pass.

Windows/macOS producer differences, native message dispatch, suppressed-panel-vs-painted controls and the 20/40/80 ms cadence matrix remain unobserved by this local run. They must be evaluated from the remote 90-case artifacts before attributing platform differences. No physical audio-interface or DAW stability claim follows from these data.
