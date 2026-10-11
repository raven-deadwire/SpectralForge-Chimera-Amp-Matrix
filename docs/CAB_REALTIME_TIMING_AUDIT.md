# CAB real-time timing audit

Investigation date: 2026-10-09. PR: [#34](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/34).
This document records evidence and the scope of the timing fix. It does not claim
that the failure is resolved or authorize a release. Final implementation,
measurements are recorded below. Exact-commit CI acceptance is tracked in PR #34.

## Investigated sources and historical CI

The reported failure used `7c539160359d3a67483da83eabb7f5137df32c7f`.
The branch had advanced to `897225469f17f9da273ed90e5cc8f7a9f1b726e3` when the
investigation began. These results describe those commits, not the pending fix.

| Source | Workflow | Verified result |
|---|---|---|
| `7c539160` | [CAB contracts 37892857900](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37892857900) | Windows, Linux and macOS jobs completed successfully. |
| `7c539160` | [Full product 37892857889](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37892857889) | Linux 33/34 tests; macOS ultimately 32/34; Windows job success. The previously pending macOS result was also a failure. |
| `8972254` | [CAB contracts 37899290199](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37899290199) | Windows and Linux success; macOS 13/15 tests. Original and expanded integration timing tests failed on macOS. |
| `8972254` | [Full product 37899290122](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37899290122) | All three platform jobs were still building at the initial audit. This is not a final result or fix verification. |

All rows below are 96 kHz, 64 frames, two channels, three cabinets and six
microphones. The unchanged block period is 666.666... microseconds. Job logs
provide wall-clock and thread-CPU percentiles separately; these percentiles need
not identify the same individual callback.

| Source / job | Engine | Wall p99 (us) | Thread CPU p99 (us) | Wall deadline misses / 1600 |
|---|---|---:|---:|---:|
| `7c539160`, [Linux 113697529939](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37892857889/job/113697529939) | original-v1 | 680.449 | 680.712 | 18 |
| `7c539160`, [macOS 113697529976](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37892857889/job/113697529976) | original-v1 | 822.709 | 839.750 | 21 |
| Same macOS job | array-v3-8x10 | 986.166 | 609.375 | 28 |
| `8972254`, [CAB macOS 113717826708](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37899290199/job/113717826708) | original-v1 | 723.292 | 714.000 | 32 |
| Same CAB macOS job | expanded-v2 | 717.334 | 703.958 | 22 |

Every listed failed integration test reported `callback_new=0` and
`callback_delete=0`. This watches the test's thread-local C++ operators only;
it is not a universal allocator or lock trace.

The Linux wall and CPU p99 values are nearly equal. Original-v1 also exceeds
budget in thread CPU on macOS. Scheduler suspension alone therefore does not
explain the failures. The earlier macOS array result has a much larger wall/CPU
gap, so scheduling can contribute too. These observations do not isolate cache
misses, frequency changes, synchronization costs or a specific function.

## Separate acoustic generation from callback execution

`IRLibrary::build()` calls `cabLayout::generate()`. Non-layout requests dispatch
to `cabExpansion::generate()`, which dispatches original requests to
`originalCab::generate()`. The model cache and acoustic-response construction run
in preparation/the IR worker, not inside `Cab::process()`.

The integration executable instantiates `Cab` and `IRLibrary` directly. It does
not execute the plugin UI, artwork rendering or the processor's host-parameter
reading path. The original-v1 test creates original keys and therefore does not
render the new array model merely because the new catalogue is compiled in.
The new UI is not on this failing measured callback path. This does not rule out
separate costs in the complete processor, or indirect competition from the IR
worker during parameter automation.

All three acoustic generations retain an 85 ms response. At 96 kHz this is 8,160
samples. They use the same `Cab::Kernel` runtime, A/B handling and serialized
50 ms crossfades. The expanded catalogue does not automatically add active
convolution paths to an original-v1 configuration. Failures in multiple model
versions make the shared runtime a relevant optimization target; they do not
prove that the different generators have equal worker cost.

## Convolution, cache and thread findings

The inspected dependency is JUCE 8.0.8, selected by `CMakeLists.txt`.
Its [convolution implementation](https://github.com/juce-framework/JUCE/blob/8.0.8/modules/juce_dsp/frequency/juce_Convolution.cpp)
defines the default `Convolution` as zero-latency uniform convolution.
For a maximum block of 64, its FFT size is 256 and each impulse segment spans
192 samples. An 8,160-sample response consequently has 43 impulse partitions
per channel, compared with 20 at 44.1 kHz. Six stereo microphone paths, rising to
nine during the already-serialized per-rig crossfades, multiply that work.
The same complete response must be retained when changing partitioning.

The baseline `IRLibrary` cache is an eight-entry deque keyed by model identity.
It is searched and modified only during preparation/the worker. A hit copies
the generated samples but still constructs and prepares a new convolution
engine. A miss computes the response and evicts the oldest entry when full;
hits are not promoted. `prepare()` stops the worker and clears the cache before
changing the processing specification, so a cached response from another sample
rate is not reused. No cache search or deque mutation occurs in the CAB callback.
More catalogue entries do not increase the eight-entry bound. Rapid distinct
requests can nevertheless cause regeneration, copying, FFT preparation and
memory-bandwidth competition outside the callback. Cache hit/miss rates and
hardware cache misses were not measured in the historical jobs.

Each baseline `Kernel` owns a default `ConvolutionMessageQueue`, including a
private background loader thread. JUCE's loader checks its queue and sleeps
10 ms when idle. `IRLibrary` has an additional worker whose main loop waits
20 ms. Stopping the IR worker does not terminate the loaders owned by still-live
convolution kernels. Preparation, replacement and reclamation can also create
or join those loader threads outside the audio callback. Their existence is
confirmed by source; their contribution to the observed p99 has not been
quantified. A shared-queue change would need a separate ownership, lifetime and
single-producer review, rather than assuming it is safe or the cause of failure.

`Cab::process()` exchanges prepared raw pointers and defers destruction through
the retired slot. The IRLibrary mutex protects worker/control data and is not
acquired by this CAB processing function. Atomic accesses and common convolution
bookkeeping still have a cost. The C++ allocation watch cannot establish that
every third-party callback operation is lock-free.

## Runner and measurement separation

Both inspected workflows finish the `--parallel 2` build before CTest. Their
CTest commands have no parallel-test option, and the historical logs show tests
running sequentially. There is no evidence of another CTest executing alongside
the failing test. Separate GitHub matrix jobs are not evidence of work sharing
the same runner. Linux leaves its Xvfb/Openbox desktop running through CTest;
the direct CAB test itself does not exercise that desktop.

The existing integration test collects the following two measured phases:

| Phase | Measured callbacks | Existing workload |
|---|---:|---|
| `worker_and_automation` | 1200 | Worker active; requests change during the first 80 iterations, followed by convergence/settling. Sleeps outside the timed callback every eight iterations remain. |
| `forced_six_slot_publication` | 400 | After convergence and fade draining, stop the IR worker, construct and publish six prepared kernels before processing, then time the simultaneous publication/crossfade workload. Private JUCE loader threads can remain. |

The historical aggregate cannot identify which phase produced its p99.
Per-phase reporting is diagnostic partitioning of the same measured callbacks,
not an additional workload or permission to discard a slower phase. Convergence,
fade draining, reference-kernel construction and publication remain outside the
timed intervals, as in the existing test. Per-phase CPU/wall comparison can
improve attribution but is not hardware profiling or proof of causality.

## Acceptance constraints

The fix must preserve the existing aggregate acceptance calculation:
`times[1584] < 1e6 * block / sampleRate` after sorting all 1,600 wall-clock
samples. Thread-CPU timing remains diagnostic and cannot replace this wall gate.
Do not increase the budget, reduce sample rates/channels, shorten responses,
remove automation/publication stress or change the tested block sizes.

Keep 44.1/48/96 kHz, 64/256 frames, mono/stereo, three cabinets, six microphones,
the 50 ms transition, independent A/B behavior, no added convolution latency,
bounded transitions, zero watched callback allocation/deletion and existing
project/state compatibility. Numerical equivalence must compare the complete
response and streaming output with the baseline, including variable block
boundaries and synchronized convolution work. A faster partition schedule must
not pass by truncating an IR or changing the authored acoustic response.

## Final implementation

`ModeledCabConvolution` packs the two independent input channels as `L + iR`
for a mono modeled IR. By linearity, the real and imaginary inverse-transform
outputs reproduce the independent left and right convolutions. It uses one
complex FFT pair and shared IR spectra instead of two separate channel engines.
All frequency bins are retained. The original small-block uniform partition
schedule (hop rounded to the next power of two, FFT 4x hop, IR partitions 3x hop)
remains, so larger intermittent tail transforms are not introduced.

This route is limited to modeled mono IRs already at the processing rate,
prepared stereo, maximum block size <=128. Mono, larger prepared blocks,
resampled and captured/factory IR routes retain the existing JUCE engine.
Preparation allocates the contiguous spectra and FFT storage off the callback;
reset and processing use that storage. Prepared model kernels on the optimized
route no longer create a private JUCE convolution-loader thread. The existing
IRLibrary worker and pending/retired ownership handoff are unchanged.

The settled unity-gain/zero-delay microphone path also copies circular history
in contiguous wrapped spans and vectorizes settled A/B mixing. Smoothing remains
sample-by-sample, and history is maintained before mixing so later delay
changes read the same preceding audio. No model equation, kernel sample, key,
parameter, project schema, fade length, timing budget or timing sample count is
changed. FFT reduction and removal of redundant loaders are established by code;
cache-miss counts and universal scheduling guarantees were not measured.

### Rejected alternatives and retained evidence

`Validation/cab-realtime-local-20261009.json` retains every local candidate log,
including failures. Full original tests were run sequentially for each engine.
Exploratory one-condition runs varied convolution partition choices; they are
explicitly diagnostic, not replacements for the 12-route acceptance workload.

Non-uniform heads (128/256/512) and larger internal uniform blocks did not
provide reliable p99 improvement: intermittent tail work increased worst-block
cost despite some lower medians. They were rejected. A microphone-loop-only
candidate still failed the original-v1 96 kHz/64/stereo condition at 728.917 us.
The final implementation retains the uniform schedule and reduces duplicate
stereo transform work instead.

### Local response verification

The new `ChimeraOriginalCabRealtimeTests` compares an independent unmodified
JUCE uniform oracle against the production Kernel across 96 routes:
v1/v2/v3/synthetic stereo user IR, 44.1/48/96 kHz, maximum blocks
17/64/128/256, mono/stereo, fixed and irregular 1/17/63/max/3 chunks.
It verifies complete streamed tails, absolute model impulses, independent
channel excitation, reset and zero processing latency. Maximum observed
stream residual was 1.25729e-8; impulse residual 7.45058e-9; reset residual 0.
These remain below the existing 3e-6 response tolerance.

Eight independent sample-by-sample microphone post-path oracle routes cover
fractional/20 ms delay, gain/polarity, blend/bypass smoothing, B mute/wake and
multiple circular-buffer wraps in one block (8 kHz/1024). Maximum residual 0.

### Local CPU verification

The attached machine-readable evidence contains the full unchanged 12-route
(3 sample rates x 2 block sizes x 2 channel counts) integration workload for
all three engines, both worker/automation and forced-publication phases,
all deadline misses and watched C++ allocation/deletion counts. These are local
CAB-only measurements, not total-plugin CPU or target-DAW acceptance. Shared-host
variance remains possible. Numerical limits, 1600 blocks, all rates, buffer sizes,
channel counts and wall-clock p99 calculation were not relaxed.

All 36 final timing routes passed, with zero watched callback C++ new/delete.
The critical 96 kHz/64/stereo condition measured:

| Engine | Baseline p99 us | Final p99 us | Final misses /1600 | Deadline us |
|---|---:|---:|---:|---:|
| original-v1 | 891.058 (FAIL) | 413.407 | 6 | 666.667 |
| expanded-v2 | 449.046 | 400.453 | 9 | 666.667 |
| array-v3 | 589.355 | 326.271 | 1 | 666.667 |

A p99 pass does not mean zero deadline misses; all misses remain in the evidence.

Exact-source Windows/Linux/macOS CAB contracts and full product results must
be read from PR #34's current CI evidence. This local report does not assert
cross-platform acceptance, actual-instrument listening, or release approval.
