# Same-callback CAB attribution — executed evidence, 2026-10-12

The diagnostic implementation at
[`35617a2e0a5a599c5bd2b2a13113b6f234cd60a3`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/35617a2e0a5a599c5bd2b2a13113b6f234cd60a3)
completed the registered **ABBA and BAAB macOS console allocations** in
[Actions run 38225976064, attempt 1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38225976064).
This follow-up adds evidence about individual callback intervals. **It does not
implement or demonstrate a product performance fix.** The original p99 gates
remain authoritative and continue to coexist with individual deadline misses.
Actual AU/VST3/CoreAudio/DAW playback validation remains **NOT_EXECUTED**.

The later commit containing this report changes evidence documentation only;
the measured source is the immutable commit above. Normal product and original
acceptance targets compile out the new attribution hooks. No caller/worker QoS
setter, DSP optimization, reduced workload or relaxed acceptance criterion is
part of the implementation.

## Prior evidence and remaining hypotheses

The [reviewed protocol](../callback-attribution-plan.md) distinguishes evidence
against a sole explanation from evidence that a mechanism can never contribute.
The original investigation is frozen at
[`f89564f`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/f89564f2444359d51bac7d8d0b1a53d2abeeb129/docs/evidence/macos-cab-deadlines-20261012/README.md).

| Disposition | Evidence and consequence |
|---|---|
| Existing p99 success is not zero-miss playback | The released macOS population passed 66/66 p99 criteria while retaining 54/105,600 misses, 33 at 96 kHz / 64. |
| Convolution work alone does not explain the largest old observation | The 29,913.208 microsecond wall interval had 102.25 microseconds paired caller CPU. Its exact scheduler, clock, VM or other cause was not traced. |
| Worker activity and fades are insufficient as universal explanations | Eight old misses occurred after worker stop. A CPU-over observation at forced-phase index 167 followed both serialized fades. These mechanisms remain possible contributors elsewhere. |
| Caller-QoS improvement is unproven | The old QoS ABBA had A18/B13 misses, while BAAB had A14/B67. Neither repeatable benefit nor intrinsic harm from higher QoS is established. |
| Some paths are absent from this console executable | Editor, graphical EQ, tuner, full processor and AU/VST3 wrapper work cannot explain intervals that do not execute them. Their actual host interaction remains unmeasured. |
| Backend-specific conjectures need the selected source path | The measured modeled convolver uses vDSP and a uniform partition schedule. JUCE fallback FFT locking, asynchronous JUCE convolution loading or an occasional larger tail partition are not the selected mechanisms here. This is not a universal claim about every plugin path or native-library internals. |
| Memory and operating-system mechanisms remain open | Explicit copies, clears and FFT/MAC stages can now be associated with a callback. Cache events, thread-specific Darwin page faults, continuous scheduling and hypervisor events are not supplied by these observations. |

The historical arithmetic control also exceeded the 666.667-microsecond
reference in both wall and paired CPU without CAB. Raw CPU-greater-than-wall
observations remain valid recorded observations, not values to clamp or
automatically classify as convolution work.

## What the code now records

The [build and schema instructions](../../../../Tools/CabCallbackAttribution/README.md)
describe the implementation and exact reproduction commands.

| Area | Same-interval evidence |
|---|---|
| Callback and stage clocks | Raw wall/thread-CPU endpoints, validity/errors, callback/event/parent IDs, nesting depth and a wider observer envelope. |
| Convolution and explicit memory operations | Active/fading convolution, FFT forward/inverse, spectral MAC, pack/unpack, input/copy, history, overlap and reset scopes, with source-level logical byte counts. |
| Kernel and fade state | Accept/reject/retire and wake scopes; before/after snapshots of six audio-owned slots, active/fading model/generation identities, requested keys, fade remaining and safe pending presence. |
| IR worker | Build, generate, prepare, publish, collect, cancel, wait and exit events in worker-owned storage; lock-free packed state/sequence snapshots at callback boundaries and sequence observations at stage boundaries. |
| Outside-callback work | Reference-impulse processing and forced publication in a separate caller stream. Worker CPU is never added to caller CPU. |
| Native resources | Separate resource condition, its own query intervals, capability/errors, Darwin task faults/page-ins and instantaneous thread state. Unsupported counters remain unavailable. |

The recorder uses bounded preallocated arrays, and both A and B explicitly
read/write their pages before measurements. Native macOS receipts report a
16,384-byte page stride and 5,036 touch operations per route. Pages are not
locked; future residency is not guaranteed. Recorder-owned callback code does
not allocate, log, perform file I/O or acquire the worker mutex. The C++
allocation guard is narrower than a native allocator, aligned-allocation or
lock trace. Worker buffers are
read after joining the worker. A pending pointer is never dereferenced by a
snapshot because another thread can replace and destroy it; exact identities
are recorded where ownership is safe.

The two clocks have different envelopes: wall is read first, CPU second at each
endpoint. Neither CPU alone nor wall minus CPU is exclusive DSP or exact
scheduler time. Parent convolution spans contain FFT/MAC/memory children; adding
all inclusive spans would double-count. The verifier subtracts direct children
only for a labeled boundary remainder, without naming that remainder as an OS
delay. Logical byte counts do not measure physical memory traffic, cache misses
or Accelerate's internal memory operations.

## Registered native allocations

**A/B now mean instrumentation off/on, not the historical QoS comparison.**
A is the same diagnostic harness with source hooks compiled out. B is a separate
binary with hooks compiled in and stage recording enabled. Each allocation has
its own recorded binary hashes. Both retain inherited scheduling. CAB covers
three engines, mono and stereo, three lane buffers with two microphone slots,
full responses and the original serialized 50 ms fades at **96 kHz / 64**.

Each arm runs six CAB routes and two routes for each independent arithmetic,
memory and clock workload. Every route retains all 1,600 intervals. CAB retains
1,200 worker/automation intervals followed by 400 after worker stop and forced
publication. Actual asynchronous publication and fade states can differ even
with equal requests, so the arms are not guaranteed to contain identical
realized work. Sentinels have their corresponding paced/burst phases and are not
CAB-equivalent subtraction terms.

| Allocation | Recorded order | Environment | Completed commands |
|---|---|---|---:|
| ABBA | A, B, B, A, then separate resources | macOS 15.7.9, arm64, Apple M1 (Virtual), 3 visible cores, 7 GB | 28/28 returned 0 |
| BAAB | Separate resources, then B, A, A, B | macOS 15.7.9, arm64, Apple M1 (Virtual), 3 visible cores, 7 GB | 28/28 returned 0 |

The runner receipts identify different guest instances and an Apple virtual
machine. Independent allocation does not establish distinct physical hosts or
controlled hypervisor scheduling. Compiler metadata identifies AppleClang
17.0.0.17000013 and pinned JUCE 8.0.8 at
`d6181bde38d858c283c3b7bf699ce6340c050b5d`. Current source remained clean at
`35617a2e0a5a599c5bd2b2a13113b6f234cd60a3` / tree
`27e6ea93d1eb810a4ba9646cf6260371aec56cbe` before and after both executions.

## Observed diagnostic misses

Each CAB entry below has **9,600 intervals**; each arithmetic, memory or clock
entry has **3,200 intervals**. These are counts against the nominal
666.666667-microsecond body reference. They are not an audio-device deadline
measurement, a new product gate, or pooled percentile estimates.

| Allocation | Trial | Level | CAB | Arithmetic | Memory | Clock |
|---|---|---|---:|---:|---:|---:|
| ABBA | 01-A | off | 2 | 0 | 0 | 0 |
| ABBA | 02-B | stages | 10 | 0 | 0 | 0 |
| ABBA | 03-B | stages | 16 | 1 | 1 | 3 |
| ABBA | 04-A | off | 2 | 0 | 0 | 0 |
| ABBA | resources-after | resources | 27 | 1 | 0 | 0 |
| BAAB | resources-before | resources | 102 | 0 | 0 | 3 |
| BAAB | 01-B | stages | 57 | 1 | 0 | 6 |
| BAAB | 02-A | off | 12 | 0 | 0 | 0 |
| BAAB | 03-A | off | 5 | 0 | 2 | 0 |
| BAAB | 04-B | stages | 101 | 0 | 0 | 2 |

Within each allocation, equal-sized CAB condition totals are **A4/B26** in
ABBA and **A17/B158** in BAAB, with 19,200 CAB intervals per condition. The
resource populations remain separate. These results show sensitivity to the
observer condition and substantial allocation/trial variation; they establish
no product improvement or exact causal decomposition of the recorder overhead.
Non-CAB controls also miss. Neither their anomalies nor a clean control proves
the cause of a separately timed CAB interval.

Across the two allocations, the retained inventory is **120 route populations,
192,000 diagnostic intervals**: 96,000 CAB and 96,000 sentinel intervals. The
[durable miss table](native-deadline-misses.csv) retains **all 354 diagnostic
misses**, including resource and sentinel populations, with all 115 original
callback CSV fields plus seven source/allocation/file labels. The table is not
a substitute for zero-miss rows or the full stage, slot, worker and caller files
in the original artifacts.

The miss table is 225,442 bytes with SHA-256
`f8e8cba73cde939aefbfe233250132b77c6aff83560004edd09cdb2bb22513ca`.

### A callback can now be joined to the scope containing its outlier

In ABBA `02-B`, expanded-v2 / stereo callback **910** records a body of
**15,477 microseconds wall / 145.917 microseconds thread CPU**. Its wider
entry/exit observer envelope is 15,478 / 147.291 microseconds. Within that same
callback, the leaf `overlap_add` event for lane 1 / microphone 1 / active kernel
records **15,340 microseconds wall / 12.375 microseconds CPU**, with 3,072
source-level logical bytes. Its `convolution` and `active_convolution` parents
contain that event; their inclusive durations are not additional costs to add.

This observation identifies the measured source scope across which elapsed
time accumulated. It does not establish that overlap arithmetic took 15 ms of
CPU, that those logical bytes incurred cache or page faults, or that a specific
scheduler/hypervisor event occurred. The ABBA receipt retains the selected raw
event, callback and state relationships so the join can be inspected.

BAAB supplies different observations that must retain their own interpretation:

| Selected callback | Callback body wall / CPU (microseconds) | Linked source observation |
|---|---|---|
| resources-before, array-v3-6x10, mono, 810 | 39,689 / 156.959 | Lane 2 / mic 1 `convolution` spans 39,560 wall / 30.208 CPU. Its parent-minus-direct-child wall remainder is 39,553 microseconds. The remainder includes clock boundaries and recorder work; it is not an exact scheduler interval. |
| 04-B, original-v1, stereo, 81 | 8,480 / 8,412.5 | Lane 0 / mic 1 `spectral_mac` spans 8,304 wall / 8,236.417 CPU. This locates an accounted CPU excursion inside the measured span, without identifying instruction cost, native clock behavior or VM scheduling as its cause. |

In both selected BAAB callbacks, worker sequence remains unchanged in the
observed wait state, and no fade/adoption event occurs. The resource callback
has zero task-fault/page-in deltas, not a thread-specific no-fault proof. The
stage-only CPU example has resource counters disabled. The BAAB receipt retains
the raw joins and distinguishes these observation capabilities.

There are counterexamples to a CAB-only CPU explanation in the new data too:
BAAB `01-B` clock-control callback 639 records 1,610 wall / 1,609.916 CPU
microseconds, and `03-A` memory-control callback 939 records 1,334 /
1,340.333 microseconds. The clock control executes empty measurement scopes;
it is not an audio workload. The memory control is a prepared synthetic loop;
it is not a matched CAB memory operation. Both preserve their raw clock pairs
and exceed the nominal reference without convolution.

## Original acceptance and other validation

Every gate below runs with attribution hooks off and retains the original five
tests, full 66-route matrix and p99 comparisons. None discards individual misses.
Frozen source is the released `7b20cd9d1f90e3e2969c250712b199e893cf50cd`.

| New native allocation | Original gate | Tests | p99 routes | Callbacks | Individual wall misses |
|---|---|---:|---:|---:|---:|
| ABBA | Frozen release | 5/5 | 66/66 | 105,600 | 64 |
| ABBA | Current hooks off | 5/5 | 66/66 | 105,600 | 59 |
| BAAB | Frozen release | 5/5 | 66/66 | 105,600 | 25 |
| BAAB | Current hooks off | 5/5 | 66/66 | 105,600 | 56 |

The sequential gate counts are not matched performance interventions. In each
native job, the original 20 Python evidence contracts and new 21 attribution
corruption/fixture checks pass. The recorder contract executable and the
original semantic regression source compiled with attribution enabled also
return 0. The probe's own numerical/channel/blend/fade/latency assertions run
with its selected recording condition. An internally consistent CSV is not an
independent reanalysis of output audio or a replacement for those executed
assertions.

Other evidence is preserved with its actual source and execution scope:

| Record | Result and boundary |
|---|---|
| [Clean-source portable controller](portable-controller.md) | One Linux KVM ABBA allocation: 28/28 commands, 20 modes, 60 routes / 96,000 intervals; all 561 manifest files checked. Frozen/current original misses were 25/22. Portable validation supplies no additional native macOS repetition. |
| [Existing CAB realtime CI](existing-cab-ci.md) | Run 38225982755 passed on macOS, Linux and Windows, each with 5/5 tests, 66/66 original p99 routes and 105,600 callbacks. macOS retained 41 misses, including 18 at 96 kHz / 64; Linux/Windows had zero. Its baseline is `aaa1330`, not the released frozen source used above. |
| [Precommit local validation](local-validation.md) | Earlier working-tree builds, file hashes, numerical/trace checks and a retained setup failure. Embedded HEAD alone does not bind these builds to final source; they are not final-commit native evidence. |
| Separate CAB Panel CI | Run 38225982760 has a macOS UI navigation failure in `ChimeraCabPanelStateTests`; 16/17 tests passed. The failed result is retained, and no timeout is extended or post-failure delivery converted to a pass. This prevents a claim that every PR check is green. |

The separate UI failure records `cabViewIRLoader` not reached at the deadline,
2,040.09 ms elapsed, and a 1,857.58 ms longest dispatch gap. A diagnostic
post-failure pump delivered the listener at 2,040.39 ms, leaving
`original_result=FAILED`. The relevant UI test, editor, CAB panel/room, root
CMake and workflow sources are byte-identical to `f89564f`; the regular target
has no attribution definition. These facts do not establish the UI failure's
cause. The [failure receipt with its complete CTest-step excerpt](cab-panel-failure.json)
preserves the observed result. The ordered JSON strings retain the decoded log's
characters, including trailing spaces, without changing its result. See the
[unaltered failed job](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38225982760/job/114733484902).

## Evidence inventory and independent recheck

The allocation receipts document raw-row validation, source/build binding and
their exact audit scope:

- [ABBA report](native-ABBA.md) and [machine-readable receipt](native-ABBA.json).
- [BAAB report](native-BAAB.md) and [machine-readable receipt](native-BAAB.json).
- [All diagnostic miss fields](native-deadline-misses.csv).

Each allocation's **20 mode directories / 60 routes / 96,000 intervals** was
reverified with the standalone raw verifier and explicit engine/channel grids.
Both audits report zero errors or incomplete reasons. All **563 listed manifest
files per allocation** match their original hashes and sizes. Every miss-table
row was additionally compared field-for-field with the original callback CSVs;
none was missing, duplicated, added or changed.

Native executables are not included in these uploaded archives. The audit
checks the controller's recorded binary digests and their consistency across
trial receipts; it cannot independently rehash absent executable bytes. Source
and compiler receipts, raw records and retained build logs supply the stated
binding, without claiming cross-allocation binary identity or full dependency
tree cleanliness.

| Allocation | Artifact ID | ZIP bytes | Published and downloaded ZIP SHA-256 |
|---|---:|---:|---|
| ABBA | 11737181865 | 106,583,181 | `24bbf5281214372945badc89fe8f754a4474a37f2b19db0eecd85943567b2486` |
| BAAB | 11737172076 | 108,323,230 | `73c6a82ca73368f639f843d8a67dd29bb10d96e7aeaa6c5bbe17655ba0ed943b` |

Both original ZIPs contain 567 members. Actions reports expiry at
**2027-01-10 08:49:39 UTC**. Raw artifact retention is finite. The committed
receipts, route tables and full miss fields remain available, but repeating
the full audit requires the original ZIPs, including every zero-miss callback
and complete event streams. Download from the linked run and first check the
ZIP digest. Do not execute downloaded binaries to perform the offline audit.

Use `verify_attribution.py` from the measured source, independently of the
controller. For each allocation's `callback-attribution-evidence/trials`, verify
all five population directories and all four modes, using the directory's
registered trace level. A CAB population requires engines
`original-v1,expanded-v2,array-v3-6x10` and channels `1,2`; sentinels require
engine `original-v1` and channels `1,2`. For example:

```sh
python Tools/CabCallbackAttribution/verify_attribution.py \
  --input /path/to/ABBA/callback-attribution-evidence/trials/02-B/cab \
  --output /path/to/new-audit/ABBA-02-B-cab.json \
  --expected-mode cab --expected-trace-level stages \
  --expected-engines original-v1,expanded-v2,array-v3-6x10 \
  --expected-channels 1,2
```

Exit 0 means complete internally consistent diagnostic evidence, 2 means
inconsistent evidence and 3 means incomplete evidence. Check each result;
missing populations, overflow, failed clocks or corrupt rows must not become
zero cost. Verify `SHA256SUMS.json` entries against their original relative
paths and the source/compiler/binary receipts as described by each allocation
report. A manifest or source-commit string alone is not authentication of the
measurements or an audio-output oracle.

## What remains unverified

The new data can identify the scope containing an outlier, associate actual
kernel/fade state and worker events, and distinguish explicit source memory
operations. It does not by itself identify preemption, a cache miss, a native
allocator/lock, or a hypervisor interruption. In particular, Darwin
`TASK_EVENTS_INFO` is task/process-scoped and can include the worker;
its query window also extends beyond the callback body. `THREAD_BASIC_INFO`
snapshots are instantaneous. Continuous scheduler traces,
thread-specific page-fault attribution and hardware cache counters are not
available in these records.

Actual AU/VST3 processing in a DAW, CoreAudio device callbacks, editor open/closed,
EQ/tuner interaction, real host automation/variable blocks and sustained
uninterrupted playback remain **NOT_EXECUTED**. No audio host was available in
these console allocations. A later host record must bind device, driver,
DAW/version, plugin format, hardware/VM, rate/block size, duration and observed
dropouts to the whole host callback.

A future product change requires a supported cause and a reproducible benefit
in independent controlled repetitions, then a **separate development PR**.
Full IR responses, lane/microphone count, channel separation, 50 ms fades,
buffer sizes, numerical tolerances, original p99 gates and all individual misses
remain mandatory. This diagnostic PR remains a draft and establishes no reason
to change those contracts.
