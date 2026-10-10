# CAB realtime performance: 1.3 optimization and evidence

## Scope and release integration

This change starts from release PR #35 at
`aaa1330dd5cf7d51faf1c45a7cef633b9b118458` and is proposed back to
`release/1.3.0-preparation`. It does not include the validation-policy changes in
PR #41 or the 1.3.1 signal-path/EQ integration. The authored acoustic models,
parameter IDs, model keys, state schema, preset definitions and cabinet artwork
are unchanged.

The acceptance criterion remains **CAB-only wall-clock p99 below one block
period**. This document does not grant a product release or DAW acceptance.

## Worker contention follow-up after 0f46f9f6

### Completed native results at the preceding source

All of the following are first attempts at
`0f46f9f6eba7bea38ff256e3a5eafb1ca83b7065`, tree
`a4587e804aa0cda0d2603593dda512bd26707759`. The dedicated success did not
establish that the full product met its deadline.

| Execution | CTests | CAB p99 | Individual wall misses / 105600 |
|---|---:|---:|---:|
| [Dedicated CAB / Windows](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072841041) | 5/5 PASS | 66/66 | 0 |
| Dedicated CAB / Linux | 5/5 PASS | 66/66 | 2 |
| Dedicated CAB / macOS | 5/5 PASS | 66/66 | 28 |
| [Full Candidate / Windows](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072840993/job/114273646190) | 40 PASS / 1 FAIL | 64/66 | 44 |
| [Full Product / Windows](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072840987/job/114273736639) | 38 PASS / 3 FAIL | 60/66 | 328 |
| [Full Product / macOS](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072840987/job/114273736539) | 37 PASS / 1 FAIL | 65/66 | 34 |
| Full Product / Linux | 38/38 PASS | 66/66 | 0 |

Candidate completed all 41 inventory tests, with no skips or missing/duplicate
JUnit entries, in a **63 min 30.4 s** job. Its final visual test passed in
**809.78 s** and complete CTest took **1749.58 s**, returning exit 8. The previous
whole-job 60-minute interruption is resolved in this attempt; the remaining
failure is performance. All four CAB timing CTests already have
`RUN_SERIAL=true`.

Candidate Layout 96 kHz / 64 measured p99 **780.8 us mono / 876.6 us stereo**
against **666.666667 us**, with 19 / 22 misses. Its other three misses were in
passing p99 conditions. All 44 occurred in the worker phase. The same p99
callbacks recorded **366260 / 414428 raw thread cycles**. Its environment
collector identified AMD EPYC 7763, a VM with 2 cores / 4 logical processors,
and High performance power plan. The collector's BelowNormal priority is not
evidence of the CTest or callback thread's priority.

Windows Product separately failed Expanded integration 96k/64 mono (769.7 us),
Expanded extended 48k/32 stereo (773.9 us), 96k/32 mono (600.0 us),
96k/32 stereo (589.8 us), 96k/64 mono (949.4 us), and Layout 96k/256 stereo
(28839.8 us). The last condition's maximum was **1076579 us wall**, with
**1524458 raw cycles** on that same callback; an on-time callback in the same
condition recorded 1523968 cycles. This does not identify a particular OS or VM
cause. No conversion of cycles to CPU time is made.

macOS Product failed Expanded extended 96k/32 stereo at **398.666 us**
against **333.333333 us**, with 28 worker-phase misses and no forced-phase
misses. The same wall-p99 callback recorded 282.917 us thread CPU. Thirteen
measured CPU observations also exceeded the budget, including one only 0.084 us
above it. CPU clocks surround the wall interval and include diagnostic boundary
cost, so these are not 13 proven pure-DSP overruns. CPU and wall tails must both
remain visible.

The [CAB Panel workflow](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072841049)
passed all 51 tests across three OSes. Product's current consolidated verdict is
**BLOCKED, ready=false, 1 PASS / 127 BLOCKED**. Windows/macOS packaging,
Setup/install/repair/uninstall and Defender checks were skipped. Linux packages
were produced. There is no Windows or macOS installer for this source; the older
`4bfddbc8` Setup must not be relabelled as a current-source result.

### Production changes in the next source

The first change applies the existing prepared modeled convolution to maximum
blocks through 512. The previous 256/512 modeled path created a private JUCE
convolution loader thread and message queue for every active, pending or fading
kernel. This change removes those per-kernel loaders for modeled responses,
without sharing a JUCE SPSC queue between its worker and audio producers.

The partition geometry matches JUCE 8.0.8's existing uniform engine:

| Rounded block B | FFT size | IR partition | Input-ring stride |
|---|---:|---:|---:|
| B <= 128 | 4B | 3B | 3 |
| B = 256 or 512 | 2B | B | 1 |

The delayed-partition sum remains stable across partial chunks. With 2B FFTs,
the extra-overlap range is empty and exactly B tail samples are carried.
Immutable response spectra may share the bounded worker cache; FFT state,
input rings, partial-block position and overlap remain independent per kernel.
User/factory IRs, rate mismatches and maximum blocks above 512 retain the JUCE
fallback. The 64-sample failure cannot be declared resolved by this large-block
change.

IR preparation now starts at `juce::Thread::Priority::low`. In JUCE 8.0.8 this
sets the new Windows thread to LOWEST and the new macOS thread to UTILITY;
Linux ignores this non-realtime request. The host/caller thread, process
priority, CPU affinity, power plan and benchmark sleeps are untouched.
Synchronous initial preparation still occurs before the worker starts.
Lower-priority asynchronous work must still meet the existing latest-request,
cancellation and lifecycle contracts; no convergence limit was extended.

The worker records its start result and priority at entry. Tests read that
record only after stop/join, label Windows/macOS native observations separately
from Linux's requested-only value, and record the actual CTest caller context.
The worker query runs only on its own entry; caller-context reads/output occur
before or after the full measured population. A scheduling contract failure
retains the raw results before the final failure verdict.

The third change caches the 48 fixed cone-quadrature coefficients used by both
v1 and v2/v3 generation. The original `sqrt`, `cos` and `sin` expressions are
evaluated once into separate doubles. The per-point operation order remains
`r = radius * sqrtCoefficient`, then `r * cosCoefficient` / `r * sinCoefficient`.
No pre-multiplied XY coefficient, approximate trigonometry, reduced quadrature,
model/key change, propagation-phase change, response truncation or FFT/taper
change is introduced. Actual GCC 13 optimized assembly retained the repeated
geometry sincos calls before this change; frequency-dependent propagation
trigonometry remains required.

### Validation and limits

The large-block implementation initially passed the existing **144-route**
independent convolution oracle, including **108 modeled routes** that now assert
the prepared path is selected and the JUCE loader path is absent. This includes
36 modeled 256/512 conditions. The 18 mic-post and four reprepare checks passed.
Two new 96 kHz / 256 and 512 stereo direct six-mic cases bring that coverage to
eight, keeping the existing 3e-6 tolerance, 50 ms fades and partial chunks.
Observed uniform/impulse/six-mic residuals were respectively
**1.25729e-08 / 7.45058e-09 / 4.84288e-08**, reset residual zero and latency zero.
These results establish local audio correctness, not native deadline success.

The fixed-geometry prototype was independently compiled against untouched
`0f46f9f6` headers with GCC 13.3 -O3. All **27 float waveforms** (v1/v2/v3,
three settings each, at 44.1/48/96 kHz), **117 complex response values** and
**144 geometry doubles** matched bit for bit. All 54 cancellation and 18 invalid
rate cases also matched. The C++ function-local static initializer uses the
original math expressions; this GCC build folded its results into read-only
data. Native compiler equivalence remains a separate requirement.

A bounded ABBA diagnostic measured six original and six cached 96 kHz 6x10
generations. Wall median was **149.912 -> 79.094 ms** and thread-CPU median
**149.909 -> 79.087 ms**, about 47.2% lower. The cached 129.188 ms outlier remains
in those six samples. These small generator-only measurements do not establish
callback p99 improvement or native Windows/macOS performance.

The final frozen source then passed one sequential **10/10 CTest** execution
on local Linux / GCC 13.3 in **60.35 s**. This included both independent
quadrature references (**864 v1 + 2592 v2/v3 complex comparisons**, and 144
geometry doubles, all exact bits), the 144-route convolution oracle, actual
worker cancellation/latest-six-mic convergence, model tests and all four timing
suites. All **66/66 strict p99 conditions** passed, while **34 individual wall
misses / 105600 callbacks** remain in the evidence. In particular, Layout
96k/64 stereo reached 23136.001 us wall; no complete removal of tails is claimed.
The 66 worker records confirm successful entry with the low request; Linux
reports that the request is ignored for non-realtime scheduling, and the actual
caller remained SCHED_OTHER / priority 0. Callback C++ new/delete counts stayed
zero within the existing instrumented scope.

A separate instrumented profile ran once per version, using a preserved
`0f46f9f6`-tree binary before rebuilding the follow-up. Both completed all 12
conditions and passed p99. The follow-up improved p99 in eight conditions and
worsened it in four; individual misses increased **1 -> 7 / 19200** (the new
seven comprise one worker-phase and six forced-publication misses). These are
diagnostic runs separated in time, not a repeatability study or replacement for
CTest acceptance. Selected 96 kHz results, in microseconds:

| Block / channels | Old p50 | Follow-up p50 | Old p99 | Follow-up p99 | Old / new misses |
|---|---:|---:|---:|---:|---:|
| 64 / mono | 51.047 | 51.227 | 137.037 | 148.715 | 1 / 0 |
| 64 / stereo | 66.590 | 66.790 | 185.000 | 172.771 | 0 / 0 |
| 256 / mono | 123.386 | 109.726 | 357.449 | 538.753 | 0 / 2 |
| 256 / stereo | 242.216 | 140.341 | 580.907 | 504.432 | 0 / 4 |

All local route summaries, source/binary hashes, original generator samples and
validation scope are retained in
[`Validation/cab-worker-contention-local-20261011.json`](../Validation/cab-worker-contention-local-20261011.json).

A separate four-partition MAC tile was considered and rejected. In a bounded
GCC/x64 SSE experiment it preserved arithmetic bits but regressed 512-bin median
cost by **11.21%** and p99 by **8.90%**. The production accumulation loop is
unchanged. That scratch experiment used CPU affinity and is not an acceptance
test or evidence for native Windows/macOS speed.

Native CI for the final follow-up source remains required. The unchanged
`times[1584] < 1e6 * block / sr` predicate, all 1600 callbacks, 80 automation
requests, 1200/400 phase split, per-rig overlap limit, and audio tolerances remain
authoritative. No previous failure is erased and no installer/release success
is inferred from local checks.


## Follow-up after the 4bfddbc8 Candidate failure

The passing dedicated run did not establish consistent performance in the full
Windows Candidate workflow. At the same source
`4bfddbc8d3407429353450851b917f4769abd0c2`,
[Candidate run 38065201856, job 114251441631](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38065201856/job/114251441631)
finished with **38 tests passed, one failed and one interrupted**. Expanded CAB
at 96 kHz / 64 samples / mono measured p50 **148.9 us**, p99 **2764.9 us** and
**33/1600** deadline misses against the unchanged **666.666667 us** deadline.
The worker/automation phase contributed 30 misses; forced publication contributed
three. Per-callback Windows thread CPU evidence was unavailable in that source.

There was also an independent workflow time-budget failure. The job began at
15:50:41 UTC on 2026-10-10 and was cancelled at 16:50:40 UTC under its 60-minute
whole-job limit. The final visual test had run for about 10 minutes 43 seconds;
packaging and installer verification were skipped. The same visual test passed
in about 13 minutes 32 seconds in the separate
[Product run 38065201763](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38065201763),
which passed all 40 Windows CTests. That separate success does not replace the
Candidate failure or show that the interrupted visual test was hung.

### Changes made for this follow-up

- The IR worker checks for an obsolete source/model request or shutdown before
  generation, every 64 frequency bins, before inverse FFT and after the completed
  waveform. v1/v2/v3 dispatch all carry the same optional cancellation token.
  Synchronous `prepare()` keeps cancellation disabled after joining the worker.
- Cancellation has a distinct exception type. Temporary responses, spectra and
  kernels are discarded on the worker. A new cache entry is committed only after
  a complete kernel and a final request check; cancellation neither publishes a
  partial kernel nor reports a bad user IR. The bounded eight-entry cache and
  per-microphone mutable convolution histories retain their existing ownership.
- `cancelledBuildCount()` records observed worker cancellations since the last
  prepare. It is diagnostic evidence, independent of audio publication. User-IR
  generation changes still use the existing final generation check. This change
  does not promise a fixed stop-time bound inside inverse FFT/kernel preparation
  or eliminate the pre-existing check-to-publication race for source changes.
- The existing 1600-callback timing suites save every callback in
  `cab-timing-diagnostics/<suite>/<engine>/<rate>-<block>-<channels>.csv` after
  measurement. Wall time and thread observations retain the same callback index,
  including all 1200 worker/automation and 400 forced-publication samples.
  Windows reads `QueryThreadCycleTime`; other platforms retain thread CPU time.
  Both reads are outside the unchanged wall interval. These additional test-only
  calls have measurement overhead; they are not part of shipped DSP.
- CPU cycles remain cycles, following the
  [Microsoft API contract](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-querythreadcycletime).
  No conversion to microseconds or scheduling-only diagnosis is made. The
  reported wall p99/maximum pairs identify their actual callback; independent
  wall and CPU quantiles are still not interchangeable.
- Dedicated evidence checks each CSV's 1600 indices, phase boundaries, 80
  automation requests, original deadline, finite clocks, cumulative-clock order,
  paired deltas and agreement with the executable's p50/p99/max/miss output.
  Full and partial CSVs are copied into hashed artifacts, including on failure.
  Candidate, Product and CAB Panel artifacts also preserve these raw files.
- Windows Candidate now bounds the whole job at 150 minutes, configure and probe
  builds at 10 minutes each, product build and CTest at 60 minutes each. CTest's
  default per-test timeout is 1200 seconds; existing explicit CMake timeouts
  continue to apply. Failed tests still block packaging. Streaming temporary
  CTest logs survive interruption. A read-only report records CPU/OS/runner and
  current power-plan context without changing priority, affinity or power policy.

The authored equations, response length/taper, convolution path, zero added
latency, 20 ms smoothing, 50 ms fades and strict
`times[1584] < 1e6 * block / sr` criterion are unchanged. All existing tests remain
selected; one small diagnostics-contract CTest is added.

### Validation status of the follow-up

Independent Linux/GCC 13.3 builds of the untouched `4bfddbc8` model headers and
the new default generator produced byte-identical float responses in nine
v1/v2/v3 x 44.1/48/96 kHz cases (maximum sample difference zero). The model
contracts also check identical output with a non-cancelling token, cancellation
before/during/after generation and invalid-rate errors. The worker contract
requires an actual cancellation, convergence of all six microphones, preserved
IR status and stop/clear/reprepare resource handling. Its stop section verifies
lifecycle behavior, not a deterministic interruption-time bound.

The 15 Python raw-evidence tests reject missing/duplicate callbacks, changed
deadlines/wall summaries, mismatched or exchanged CPU/cycle pairs, non-finite CPU
values and invented zero CPU time. They also preserve complete evidence from a
failed performance gate and partial files without treating either as a pass.

The local Release build passed **10/10 relevant CTests**, including all
**66/66 unchanged p99 timing executions**, the 144-route convolution oracle,
18 mic-post cases, four reprepares and six independent six-microphone cases.
All **105600** candidate callback rows in **66 CSVs** matched their executable
summaries. The tests observed 798 worker cancellations across these conditions.
There were still **27 individual deadline misses**, so this is not zero-dropout
evidence. Watched callback C++ new/delete counts remained zero.

In one sequential local comparison against an untouched `4bfddbc8` build,
Expanded 96 kHz / 64 mono p99 was **148.274 -> 151.199 us** (both 0/1600 misses),
and stereo was **178.439 -> 221.204 us** (0 -> 1/1600 misses). Both remained below
666.666667 us, but these are p99 increases. This measurement does not establish
a universal callback speedup from removing obsolete worker work. The separate
native comparison continues to use the fixed `aaa1330` pre-optimization baseline.

The resulting source was `0f46f9f6`, whose completed native results are retained
in the preceding source-bound table and PR #42. Cooperative cancellation removed
confirmed obsolete worker work and the Candidate timeout was resolved, but full
Windows/macOS timing failures remained. Earlier sources and their separate
Product/Candidate results are retained as historical observations.

## Confirmed failures, kept separate by source

| Source | Workflow / platform | Engine / channels at 96 kHz, 64 frames | p99, microseconds | Deadline misses |
|---|---|---|---:|---:|
| `aaa1330dd5cf7d51faf1c45a7cef633b9b118458` | [Candidate 38056482864, Windows](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38056482864/job/114225946073) | Original / mono | 720.2 | 22/1600 |
| same source and run | Windows | Original / stereo | 1105.8 | 39/1600 |
| same source and run | Windows | Expanded / mono | 791.0 | 26/1600 |
| same source and run | Windows | Expanded / stereo | 1026.4 | 43/1600 |
| `07dbfa5b0da24ae7a6000ad6dc215ba2aea67c46` | [Product 38057692032, macOS](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38057692032/job/114230559378) | Expanded / mono | 674.375 | 16/1600 |

The deadline is `1e6 * 64 / 96000`, approximately **666.667 microseconds**.
Windows Candidate passed 37/39 tests; both Original and Expanded integration
failed. The other source's macOS Product passed 35/36; only Expanded integration
failed. Windows/Linux Product on that second source passed 39/39 and 36/36.

Windows Expanded stereo's worker/automation phase p99 was 1079.2 microseconds
(36 misses), and forced six-slot publication p99 was 787.5 (7 misses). macOS
Expanded mono's worker phase p99 was 741.75 (16 misses); forced publication was
295.042 (zero misses). Its aggregate thread-CPU p99 was 683.417 microseconds,
so scheduling delay alone is not an established explanation. Windows's per-block
thread-CPU diagnostic is unavailable and reports -1.

The separate `ChimeraCabPanelStateTests` navigation failure in
[CAB run 38058672649](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38058672649)
and the earlier central-horn grille defect are distinct from these audio failures.
Success in another run does not erase any recorded failure.

## Measurement and bottlenecks

`ChimeraCabProfiling` compiles stage counters into a separate executable.
`CabRealtimeProfile.h` removes every hook from ordinary plugin and deadline-test
builds. Its callback counters cover filter-parameter refresh, kernel adoption,
filters, active and fading convolution, mic post-processing, forward/inverse FFT
and spectral multiplication/accumulation. Worker counters cover acoustic response
generation, kernel preparation, publication and reclamation. Worker counters
outlive the joined worker, including on a thrown test assertion.

The profiler retains the original 1600 callbacks and additionally measures the
production-style filter refresh on both microphones each callback. These clocks
and refresh calls add overhead. FFT/MAC scopes are nested inside convolution;
their times must not be summed with parent scopes. `PROFILE` reports callback
distributions; `WORKER_PROFILE` reports total worker time and call counts.

Initial local diagnostics used GCC 13.3.0, JUCE 8.0.8, Release `-O3`, and a shared
AMD EPYC 9V74 Linux environment. At 96 kHz/64/stereo, the baseline worker-phase
median spectral accumulation was 36.396 microseconds, compared with 12.600 for
forward FFT, 12.950 for inverse FFT, 5.831 for filters and 0.210 for adoption.
Accumulation/memory traversal is therefore a substantial callback cost; the
atomic adoption itself is small in this measurement.

The mono worker previously constructed/reclaimed JUCE convolution loader
instances. Across seven live worker builds, kernel preparation totaled 5354.54
microseconds and collection 37795.8. Initial optimized diagnostics recorded
89.636 and 29.866 respectively. Prepared-spectrum creation on a cache miss is
part of `worker_build`, outside the inner kernel-construction scope, so the
inner preparation totals must not be interpreted as complete IR-build cost.
Acoustic response generation still dominates total worker time (about 0.3 s for
three generated responses in these runs) and is unchanged.

The new mono active-convolution median was 46.010 microseconds versus 53.032 in
the baseline diagnostic. Separate kernel-only measurements supported the fused
stereo MAC. However, whole-route local wall timings were variable and not every
candidate route was faster than its earlier baseline sample. **No universal
whole-plugin or whole-route speedup is inferred from these local measurements.**
The paired CI comparison measures both actual commits on each runner.

### First native comparison and the MSVC follow-up

[Run 38063987771](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38063987771)
tested candidate `f10f24a102b9fec73c4642810a9b5e13d66f45f4` against the fixed
`aaa1330dd5cf7d51faf1c45a7cef633b9b118458` baseline. Windows, Linux and native
arm64 macOS each passed all five candidate CTests and 66 timing cases with the
unchanged p99 limits. The source/tree snapshots and complete artifact hashes
were independently checked. These are new same-runner measurements, separate
from the historical failures above.

| Expanded 96 kHz / 64 | Baseline p99, microseconds | First candidate p99 | Baseline / candidate misses per 1600 |
|---|---:|---:|---|
| Windows mono | 175.900 | 218.600 | 0 / 0 |
| Windows stereo | 187.600 | 311.500 | 0 / 0 |
| Linux mono | 261.191 | 252.244 | 0 / 0 |
| Linux stereo | 280.697 | 260.900 | 0 / 0 |
| macOS mono | 358.917 | 132.416 | 7 / 1 |
| macOS stereo | 316.333 | 194.917 | 4 / 1 |

This first candidate improved paired p99 in 11/36 Windows, 25/36 Linux and
31/36 macOS cases; all remaining pairs were slower and remain in the evidence.
Windows 64-frame median and p99 increased in 17/18 cases. Expanded 96 kHz/64
stereo's forced-publication median rose from 99.6 to 153.2 microseconds, while
the candidate's instrumented spectral-MAC median alone was 103.3. This is a
sustained small-block cost regression, not merely a few rare deadline spikes.
The first passing gate is therefore not treated as a universal speedup.

The follow-up makes four-bin fused multiply/accumulate explicit with SSE on
MSVC x86/x64. Unaligned loads and stores handle odd mono bin counts, and a
scalar remainder retains the final Nyquist bin. The same multiply/add/subtract
order is used without fast-math or a new CPU instruction-set requirement on
the x64 target. Other compilers retain the existing fused loop and macOS keeps
its native real FFT. This addresses the measured hot loop; it does not assert
an assembly-proven compiler defect. The follow-up is validated under its own
source SHA in a new run, while this first result remains visible in the PR.

## Implementation

### Convolution

- Retain the existing `4 * block` FFT, `3 * block` impulse partitions, uniform
  work schedule, full authored response and zero added processing latency.
- For mono modeled input at maximum blocks up to 128, retain JUCE's native real
  FFT (including vDSP on macOS), and store/accumulate only `N/2 + 1` complex bins.
  DC and Nyquist are retained. At 96 kHz/64, the input-spectrum ring uses
  133128 bytes rather than the packed stereo ring's 264192 bytes.
- Stereo keeps independent left/right signals in the real/imaginary parts of
  one complex convolution. Fuse the four spectral multiply/accumulate passes
  into one loop, retaining arithmetic order and ordinary floating-point flags.
  No fast-math or non-uniform tail bursts are introduced.
- Share only immutable `Prepared` response spectra. The eight-entry worker FIFO
  is cleared whenever `prepare` changes processing configuration; model, rate,
  block size and mono/stereo layout therefore remain bound together. Each mic
  and fading kernel owns its FFT, input ring, overlap and partial-block position.
- Construct, share and reclaim prepared data off the callback. Audio ownership
  transfer remains raw-pointer adoption/retirement. Larger blocks and factory/
  user IR decoding, normalization, resampling and convolution retain their
  existing JUCE path.

### Parameters and mic processing

- Recalculate HP/LP coefficients only when the clamped cutoff or rate changes.
- Recalculate the decibel/polarity target only when those controls change;
  initialize the cache and smoother together during prepare.
- Handle any settled gain in the zero-delay block path. Copy **pre-gain** audio
  into delay history first, preserving later delay automation.
- Replace per-sample integer remainder operations with bounded ring wrapping.
  The same fractional delay interpolation and channel/mic independence remain.
- Preserve the original 20 ms gain/blend/delay/bypass smoothing and serial 50 ms
  A/B kernel fades. Six active paths plus up to three fading paths remain possible.

## Regression contracts

The original three model-engine timing suites retain:

- 44.1/48/96 kHz, 64/256 frames, mono/stereo;
- three independent CAB inputs and all six microphones;
- 1200 callbacks with actual worker/automation activity and 400 with six
  complete kernels forcibly published before the same callback;
- latest-request convergence, bounded transitions, independent A/B response,
  constant-sum mix, same-response midpoint unity, stereo isolation, latency,
  worker teardown and callback C++ new/delete checks;
- exactly `times[1584] < 1e6 * block / sampleRate` over all 1600 sorted callbacks.

The added Expanded suite includes 32/64/128/256/512 frames, providing 30 additional
timing routes; the three original suites retain their 36 routes. No timing test,
callback, forced publication or failing platform is removed. Timing output uses
12 significant digits so an evidence parser does not round a valid boundary
measurement across its deadline.

The audio oracle additionally checks 144 modeled/user-IR rate/block/channel
routes, fixed/irregular chunks, the complete impulse tail, reset and zero latency;
18 independent mic post-processing routes; four same-instance rate/channel/buffer
reprepare cases; and six three-CAB/six-mic routes against a direct sparse-FIR
oracle. The last group checks every sample during simultaneous publication,
stale response rejection and reuse of an earlier model key. Existing `3e-6`
equivalence and `1e-8` reset-silence limits remain unchanged.

Initial local execution passed all five suites and all 66 timing routes. Audio
residuals were 1.25729e-8 against uniform convolution, 7.45058e-9 against authored
impulses, zero for mic-post/reprepare, and 4.84288e-8 for the six-mic oracle.
Reset residual and added latency were zero. Callback C++ new/delete counts were
0/0; this instrumentation is not a universal malloc/lock tracer. These are local
implementation results; final commit-bound platform results are recorded by CI.

## Three-platform evidence and reproduction

[The dedicated workflow](../.github/workflows/cab-realtime.yml) checks out the
fixed, unmodified baseline commit and the actual candidate head in separate
directories. It builds and measures both sequentially on Windows, Linux and
native macOS. Existing universal macOS Product builds remain separate.

`Tools/run_cab_realtime_ci.py` retains complete successful and failed logs,
CTest inventory/JUnit, `LastTest.log`, all route/phase/profile rows, source/tree
SHAs, compiler/runner/architecture metadata, executable hashes and a SHA-256
evidence index. This repairs the older successful-Product-output truncation to
1024 bytes. No recorded p99 is filled in from another source or platform.

A baseline failure remains an explicit failed reference. Candidate acceptance
requires its own complete uninstrumented CTest success and all original block
deadlines. Instrumented results remain diagnostic, with their exit code retained.
Missing routes, changed protocol, incomplete phases, dirty/changed source and
truncated evidence cannot produce a passing receipt. There are no retries.

The artifacts are named `CAB-realtime-<OS>-<candidate SHA>` and contain
`manifest.json`, `comparison.json`, `summary.md` and `SHA256SUMS.json` plus the
underlying evidence. The PR links the final source and actual run results. Full
Product/Candidate, UI, installation, release policy and target-DAW outcomes must
continue to be reported under their own source SHA and workflow.
