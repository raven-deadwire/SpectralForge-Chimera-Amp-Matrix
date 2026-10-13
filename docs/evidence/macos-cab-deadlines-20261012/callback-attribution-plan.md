# Same-callback CAB attribution: evidence and prospective protocol

## Scope and evidence boundary

This follow-up adds diagnostic observability for the unresolved macOS CAB
deadline excursions. It does not establish or ship a performance improvement.
The released reference remains
`7b20cd9d1f90e3e2969c250712b199e893cf50cd`; the reviewed investigation is frozen at
[`f89564f2444359d51bac7d8d0b1a53d2abeeb129`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/f89564f2444359d51bac7d8d0b1a53d2abeeb129).
Historical measurements, original acceptance assertions and the completed QoS
experiments remain separate records.

The new source hooks are guarded by `CHIMERA_CAB_ATTRIBUTION`. A separate
`Tools/CabCallbackAttribution` probe/controller and workflow exercise them;
normal product builds and uninstrumented acceptance targets are not replaced
by a diagnostic verdict. This document states the attribution protocol and its
interpretation limits. A planned measurement is not a completed measurement.
Actual execution results require their own source-bound receipts and validation
record; no prospective entry below certifies a run.

The subsequent [execution report](callback-attribution/README.md) records the
completed source-bound macOS ABBA/BAAB and portable validation at `35617a2e0a5a`,
with independent raw-data audits and preserved individual misses. It is a
separate result record; the protocol and its acceptance boundaries below are
unchanged.

## Review of the original observations

The [historical audit](historical/historical-audit.md),
[source audit](source-audit.md), [comparison review](comparison-review.md), and
the [ABBA](experiments/native-ABBA/README.md) and
[BAAB](experiments/native-BAAB/README.md) reports define the prior evidence.
The following counts were also checked directly against the committed
[historical miss fields](historical/deadline-misses.csv) and the two native
`condition-summary.json` files. This follow-up review of those durable tables
does not claim a fresh independent verification of every original ZIP member.

| Observation | Result and consequence |
|---|---|
| Published macOS population | 54 individual wall misses in 105,600 callbacks; all 66 original p99 gates pass. Passing p99 does not mean zero misses. |
| Main attribution target | 33 misses occur at 96 kHz / 64 samples, whose reference budget is 666.666667 microseconds. Preserve mono and stereo and distinguish overlapping integration/extended populations. |
| Largest original interval | Expanded-v2 integration / 48 kHz / 256 / mono, callback 1030: 29,913.208 microseconds wall and 102.25 microseconds paired thread CPU against a 5,333.333333-microsecond budget. Its cause was not traced. |
| Worker-absent observations | Eight misses follow worker shutdown. Five also exceed the reference budget in paired CPU, all at 96 kHz / 64 / stereo: expanded-v2 forced-phase indices 97, 125, 132, 167 and original-v1 index 131. |
| Original caller-QoS experiment | ABBA: A17, B7, B6, A1 misses; BAAB: B49, A7, A7, B18. Equal-sized condition aggregates are A18/B13 and A14/B67. The direction does not reproduce. |
| Independent arithmetic control | One ABBA interval records 816.459 microseconds wall / 820.417 microseconds CPU against a 666.666667-microsecond reference with no CAB or worker. CPU-over alone cannot identify expensive convolution. |
| Earlier versus published candidate | Relevant measured CAB DSP and C++ timing sources are identical between `781d03dd` and `7b20cd9d`. Their 74-to-54 miss difference is not evidence of a source-level CAB optimization. |

Each original interval processes three independent CAB lane buffers
sequentially on one caller, with two microphone slots per lane. These are not
complete Classic/Dual/Matrix routes or actual device callbacks. The first
1,200 intervals include an asynchronous IR worker and outside-timer sleep
requests; model requests change during the first 80. After worker shutdown,
the final 400 intervals include prescribed publication and serialized fades.

That forced phase constructs six separate kernels using copies of one response
derived from Mic A at position 0.85. Previous A/B differences contribute during
outgoing transitions; the final steady targets are not six distinct A/B
responses. Histories remain private to each slot. The historical CSV lacks
actual per-callback active/fading keys and transition counts, so requested
settings do not reconstruct the exact state at each old miss.

## Hypothesis disposition

The scope of an exclusion matters. Absence from a console executable does not
exclude the same mechanism in a real plugin host. A counterexample to a sole
cause also does not show that the mechanism never contributes.

| Status | Hypothesis or claim | Evidence and remaining limit |
|---|---|---|
| Directly outside the measured console path | Editor painting, graphical EQ, tuner, AU/VST3 wrapper callback locks, full processor routing cause the recorded CAB-only intervals. | The console harness does not instantiate those paths. Their host interaction remains unmeasured. |
| Different selected backend | JUCE fallback FFT spinlock or asynchronous JUCE convolution loading causes these modeled-route intervals. | These configurations select the custom modeled convolver and Apple vDSP. This does not prove that Accelerate has no internal synchronization or that all plugin IR paths use the same backend. |
| Not in the fixed-block source schedule | An occasional large tail FFT explains these outliers. | The custom convolver uses a uniform partition schedule. Variable/smaller host blocks require separate coverage. |
| Source attribution excluded | The 74-to-54 reduction demonstrates a macOS CAB DSP improvement in the published commit. | The measured DSP/timing code is unchanged; GUI flags are not exercised and the launcher change affects Windows. Execution conditions can still differ. |
| Insufficient as the sole cause | Active IR worker interference explains every miss. | Eight latest misses occur after `library.stop()`, including five CPU-over pairs. Worker interference in other callbacks remains possible. |
| Insufficient as the sole cause | Initial publication or fade work explains every CPU-over excursion. | Forced-phase index 167 at 96 kHz / 64 follows both serialized 50 ms fades in this harness. Other swap/fade contributions require same-callback observation. |
| Not demonstrated | Higher caller QoS reliably fixes the deadline excursions. | The ABBA benefit reverses in BAAB and repeated baselines drift substantially. QoS applies to setup, warmup, sleeps and processing, not just the measured interval. |
| Not demonstrated | Higher QoS itself causes the BAAB excess. | Order, publication state and runner conditions are not isolated sufficiently for that conclusion. |
| Not demonstrated | Failed ARM vectorization, denormals or intrinsically heavier v1/v2 callback geometry is the cause. | No affected-callback assembly/counter evidence or FPCR observation establishes these claims; the model generations share response length and partition geometry at a fixed configuration. |
| Narrow source/guard finding | Callback allocation, locking and destruction have been universally ruled out. | No explicit allocation, library mutex acquisition or kernel destruction was found in the measured custom route, and C++ new/delete counts are zero. This is not a native allocation, lock, page-fault or system-framework trace. |
| Unresolved | Convolution-stage spikes, reset/history clearing, memory residency/cache disturbance, wake/pacing, OS dispatch or guest/hypervisor accounting. | The original rows do not associate these events with the same callback. Paired clocks and sentinel examples do not uniquely distinguish them. |

## Same-callback evidence contract

Attribution should join the whole interval, its stage observations and kernel
events using a stable run/trial/configuration/callback identity. Preserve every
callback, including measured zeros and all misses; an overflow, dropped row,
clock failure or incomplete population must remain explicit. A missing
observation must not be converted to a zero cost.

The diagnostic focuses on 96 kHz / 64 samples without reducing the original
acceptance matrix. Additional configurations, including 96 kHz / 32 or other
previously affected settings, are separately identified diagnostic populations.

| Evidence | Required interpretation |
|---|---|
| Callback wall and thread-CPU endpoints/deltas | Store clock validity/errors and identify the exact clock envelope. The whole measured interval is console CAB work, not a full host callback. |
| Convolution stages | Separate active/fading processing, FFT forward/inverse, spectral multiply/accumulate and supporting pack/copy/overlap/history work. Preserve inclusive-versus-leaf relationships. |
| Kernel transition and fade state | Record actual swap/reject/retire/reset/wake events, active/fading model and generation identities, fade remaining and rendered path counts, rather than assuming requested state equals active state. |
| Memory-related work | Attribute explicit buffer copies, packing, clears and history/overlap movement as operations. Their measured cost does not by itself identify cache misses, page faults or memory bandwidth. |
| Worker activity | Retain timestamped build/cancel/publish/collect/state observations or equivalent safe counters, with model/generation identity where applicable. Overlap is a correlation, not proof of causal interference. |
| Resource observations | Keep native fault/switch/accounting observations, scope and API status distinct from DSP clocks. A process-scoped count cannot be relabeled as an audio-thread event. |
| Source and execution receipts | Preserve source/dependency/tree hashes, actual binary hashes, compiler flags, runner/architecture, order, command/return status and clock/resource capabilities. Source parity does not establish cross-run binary or physical-host identity. |

The old profiler's `ActiveConvolution` and `FadingConvolution` include nested FFT
and spectral stages. Summing all such times double-counts work. New stage totals
must state whether they are inclusive, exclusive or operations that overlap in
scope. Any unassigned remainder includes measurement boundaries and overhead;
it must not be presented as exact scheduler time.

## Clock and observer-effect limits

The original thread-CPU reads surround both wall-clock reads. The envelopes
differ: CPU is not exclusive DSP execution, and wall minus CPU is not an exact
off-CPU or scheduling duration. Preserve raw CPU-greater-than-wall values and
negative derived differences; do not clamp them to create a preferred
explanation. New stage clocks need the same explicit envelope description.

Native clock resolution is metadata, not a guarantee of accounting accuracy.
CPU-over measurements can occur in the arithmetic-only control. Conversely, a
large wall/CPU gap does not identify a particular scheduling, waiting or VM
event. Establishing such an event needs an appropriate native trace with a
correlatable time base.

Recording itself can change instruction layout, cache state and dispatch
exposure. Callback recording must use bounded, prepared storage with no logging
I/O, allocation or blocking synchronization. Worker state must be observed
without reading worker-owned non-atomic mutable data concurrently. Any native
resource-query overhead needs a separate measurement condition and cannot be
quietly folded into the basic stage-timing arm.

## Prespecified controls for the new attribution experiment

**A and B below identify instrumentation conditions. They are not the A/B QoS
conditions in the completed experiment.** Both retain the same inherited
caller scheduling policy; this follow-up is not a product QoS intervention.
The same diagnostic harness is compiled into two targets: A has the attribution
hooks compiled out; B has the hooks compiled in and stage recording enabled.
They are distinct binaries. Record both hashes and their compiler/source
receipts; shared harness/source does not imply binary identity. An optional
runtime-recording-off mode in B is an additional condition, not a substitute
for the compiled-out A control.

| Population | Purpose and boundary |
|---|---|
| Original uninstrumented acceptance | Retain the existing complete 66-route population, numerical response, channel-isolation, fade and real-time assertions. This remains the acceptance authority. |
| A: attribution hooks compiled out | Control measurement of the same diagnostic workload without callback-stage hooks. Keep its full population and actual execution receipts. |
| B: callback attribution compiled in and enabled | Capture stage clocks and kernel/worker evidence on the same configuration/input/protocol. Its timing is instrumented diagnostic evidence and must not replace A or the original gate. |
| Resource-sampling condition | A separately labeled instrumented population for supported native resource evidence. Record unavailable APIs and errors explicitly; do not attribute its added query cost to normal CAB processing. |
| Arithmetic-only control | Independent non-CAB, non-worker workload, with matching reference labels/cadence where implemented. Shorter work and exposure prohibit direct time subtraction or equal miss-probability assumptions. |

Before observing results, register **ABBA** for one allocation and **BAAB** for
an independently allocated runner. Retain both results, all individual trials,
zero-miss configurations and failed/incomplete attempts. Repeated callbacks
within a process are not independent runner-level experiments. Never replace a
failed or unfavorable attempt with a rerun-until-pass result.

Run the original gates and build the experiment's needed targets before the
timed conditions. Keep input, complete IRs, lane/microphone count, sample count,
publication protocol, QoS and pacing fixed across A/B. Analyze worker-present,
forced-transition and steady worker-absent observations separately when the
trace establishes those states. In the asynchronous worker phase, equal
requests or callback indices do not ensure equal publication/fade workloads;
observed state differences must be retained as a comparison limit.

Preserve response, channel-separation, fade and real-time contracts with the
default/compiled-out targets. Run the applicable numerical and transition
oracles with attribution enabled as diagnostic semantic checks as well. The
enabled checks can establish that recording preserves output/transition
semantics in their tested cases; they cannot give instrumented timing authority
over the original real-time gate. Claims about compiled-out callback overhead
require inspection or build evidence, not just the intended macro design.

This A/B protocol measures recording effects and improves attribution. A
smaller miss count in B is not a DSP improvement. The arithmetic control can
show an anomaly without CAB, but a clean control does not exclude external
interference in a separately timed CAB arm. Resource counters can supply
correlated evidence; they do not automatically prove a cause or a remedy.

## Console CI and real audio-host validation

The workflow can exercise compilation, complete-population/schema validation,
recording behavior, original regression contracts and bounded macOS console
experiments. Completion must be supported by the actual run/artifacts, not by
the presence of a workflow or an expected target name. Non-macOS checks are
useful portability/contract evidence and are not native macOS measurements.

The following host work is **NOT_EXECUTED** unless a separate result explicitly
records a real execution environment and its evidence:

| Host validation | Current evidence boundary |
|---|---|
| Actual AU and VST3 processing inside a DAW | **NOT_EXECUTED** by the console investigation or this prospective protocol. |
| CoreAudio device callback and whole-callback release/deadline behavior | **NOT_EXECUTED**; a nominal console budget is not a device callback deadline. |
| Editor closed/open, EQ analyzer, tuner and message-thread contention | **NOT_EXECUTED**; absent from the CAB-only harness. |
| MIDI/parameter automation, complete routing and host-variable blocks | **NOT_EXECUTED** in a real host by these measurements. |
| Sustained uninterrupted playback and audible dropout assessment | **NOT_EXECUTED**; p99 or console schema success cannot certify it. |

A future host record should bind hardware/VM status, macOS, DAW/version, plugin
format, audio device/driver, actual rate/block sizes, caller policy and tested
duration to whole-callback timing and observed dropouts. Editor, tuner, analyzer
and automation changes should be tested one factor at a time. An unavailable
host or tracing capability remains unavailable, never PASS.

## Gate for a later product-performance PR

Instrumentation completion is not a performance fix. A subsequent product
change must have an evidenced, reproducible cause and an independently repeated
controlled improvement. Implement one justified intervention in a separate
development PR, preserving identical actual publication/DSP workload where
required to isolate that intervention.

Retain every original IR sample, lane and microphone path, channel separation,
50 ms fade, buffer configuration, p99 threshold and individual miss definition.
Do not shorten IRs, increase buffers, relax assertions, discard callbacks or
suppress failures to obtain an improvement. A measured reduction of ordinary
convolution cost is a different claim from resolution of the long-tail deadline
excursions; each needs evidence that supports its own scope.
