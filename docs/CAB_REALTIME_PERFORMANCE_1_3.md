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
