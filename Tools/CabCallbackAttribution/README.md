# CAB callback attribution

This diagnostic follows [PR #49's original investigation](../../docs/evidence/macos-cab-deadlines-20261012/README.md).
It records the same console interval's CAB stages, kernel transitions and
worker observations. It does not implement a performance fix, alter caller or
worker scheduling, or certify audio-host playback. The reviewed hypotheses and
limits are in the [attribution protocol](../../docs/evidence/macos-cab-deadlines-20261012/callback-attribution-plan.md).

## Build and acceptance boundaries

| Target | Purpose |
|---|---|
| `ChimeraCabCallbackProbeControl` | The same focused harness with `CHIMERA_CAB_ATTRIBUTION` source hooks compiled out. |
| `ChimeraCabCallbackProbe` | Separate diagnostic binary with source hooks compiled in; `stages` or additional `resources` capture. |
| `ChimeraCabCallbackTraceTests` | Bounded-buffer overflow, hierarchy, clock, context, worker snapshot and capture-lifetime contracts; checks remain active in Release. |
| `ChimeraCabAttributionRealtimeTests` | The existing direct-FIR, residue-bank, channel and fade regression source compiled with attribution enabled. Timing is not a new acceptance gate. |
| Original five CTest entries | Unmodified assertions and 66 original route populations, run on both frozen release source and current source with hooks off. |

The definition is private to diagnostic targets. The normal product, original
acceptance targets and old `CHIMERA_CAB_PROFILE` system retain their own
definitions. No response is shortened, no mic/lane is removed, and the serialized
50 ms fades and original numerical tolerances are preserved.

Configure from this directory, which reuses the product's pinned JUCE 8.0.8 and
IR data. Objective-C++ is enabled in the common parent on Apple platforms.

```sh
cmake -S Tools/CabCallbackAttribution -B /tmp/chimera-attribution-build \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/chimera-attribution-build --config Release --parallel 2 \
  --target ChimeraCabCallbackProbeControl ChimeraCabCallbackProbe \
  ChimeraCabCallbackTraceTests ChimeraCabAttributionRealtimeTests
```

An existing exact JUCE checkout can be supplied with
`-DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE`. The controlled driver verifies its
commit rather than accepting an arbitrary dependency under the same tag.

## Workload and independent controls

The focused CAB workload covers all three engines (`original-v1`, `expanded-v2`,
`array-v3-6x10`), mono and stereo, at **96 kHz / 64 samples**. Each route retains
1,600 intervals: the first 1,200 with asynchronous worker/automation and the
last 400 following worker stop and six ready kernel publications. The first
80 intervals request automation. Three independent lane buffers carry two
microphone slots each. Full response, channel isolation, constant-sum blend,
latest-request convergence, zero added modeled latency and bounded-fade checks
use the original limits.

The final six targets share the same generated Mic-A response at position 0.85,
as in the historical harness. Their convolution histories remain independent.
The old outgoing responses can still contribute while fades finish.

`arithmetic`, `memory`, and `clock` modes run separate non-CAB populations with
the same nominal sample count and outside-timer pacing. They are not equivalent
DSP workloads. The memory control touches bounded prepared arrays; the clock
control records empty scopes. No control duration or miss count is subtracted
from a CAB callback, and clean controls do not rule out external interference
in a separately timed CAB population.

The observer comparison is fixed before results:

- **A:** control binary, `--trace-level off`.
- **B:** diagnostic binary, `--trace-level stages`.
- One independent macOS allocation uses **ABBA**; another uses **BAAB**.
- A separately labeled `resources` population runs after ABBA and before BAAB.
- Each arm runs CAB and all three independent control modes, with fresh output.

These A/B labels differ from PR #49's historical caller-QoS experiment. No QoS
setter is called. A and B are two binaries with recorded hashes, not one
identical binary. Even equal automation requests do not make the asynchronous
worker-present phase exposure-matched: actual publication/fade states must be
considered when interpreting the observer comparison. A smaller B miss count
would not establish a product optimization.

## Recorded evidence

Each route writes the following files after measured work and worker shutdown.
The prefix identifies mode, engine, sample rate, block size, channels and trace
level. Raw data is never overwritten.

| File | Contents |
|---|---|
| `.callbacks.csv` | All 1,600 raw callback indices, phase and requests, wall/thread-CPU endpoints, exact deadline/miss, outer observer envelope, worker snapshots, resource queries and allocation guard counts. |
| `.stages.csv` | Callback-linked event tree: IDs, parent/depth, lane/mic, active/fading role, model/generation, raw clocks and source-level logical bytes. |
| `.worker.csv` | Single worker's build, generation, preparation, publication, collection, wait, cancellation and exit scopes. |
| `.caller.csv` | Reference-impulse and forced-publication work outside measured callbacks, with its own event IDs and caller CPU timeline. |
| `.slots.csv` | Six audio-owned slot snapshots before/after each traced CAB callback: actual active/fading identities, fade remaining, requested key and safe pending presence. |
| `.metadata.json` | Completion/error counters, capabilities, workload/semantic checks, build identity, counts and explicit nonacceptance/host status. |

The event, callback-row, slot-snapshot and synthetic-memory arrays have fixed
capacities. Both binaries explicitly read and write a byte at every native
page-size stride, plus each array's final byte, before measurement. A failed
page-size query uses a one-byte stride so no page is skipped. Pages are touched;
memory is not locked and later residency is not guaranteed. The stride,
native page size and number of touch operations are recorded in metadata.
Audio and worker threads write separate buffers. The caller reads
worker event storage only after stop joins it. Callback snapshots use one
lock-free packed atomic for worker state/sequence; no retry loop, worker mutex
or concurrently mutable worker object is read. Full event identity, clock
errors, overflow and nesting failures remain visible. Overflow invalidates
complete attribution rather than dropping inconvenient callbacks.

Pending kernel pointers are not dereferenced by snapshots: the worker can
replace and destroy them after an atomic load. Exact pending identities are
recorded where ownership is safe, in publication/accept/reject scopes. The
compiled-out control retains the same prepared buffer footprint but has no
stage/worker/slot attribution; missing attribution is explicitly identified.

### Clocks, nesting and memory

The new stamps read wall first and thread CPU second at each endpoint. They
are **different clock envelopes**, also different from the historical test's
CPU-outside-wall envelope. Raw CPU-greater-than-wall values are retained.
Neither CPU alone nor wall minus CPU proves exclusive DSP, scheduler or
hypervisor time. Native clock-read cost and recorder bookkeeping are included
in diagnostic measurements.

The `.callbacks.csv` body envelope contains the workload and enabled recording;
the wider entry/exit envelope also contains slot snapshots and optional native
resource queries. Resource-query intervals are retained separately. They are
observer operations, not silently attributed to the normal CAB audio path.

`active_convolution`/`fading_convolution` contain `convolution` and the
FFT/MAC/pack/history children. Inclusive values cannot be summed. The verifier
computes parent remainders by subtracting **direct children only** and labels
them as clock-boundary remainders, not scheduler time. Logical byte counts
describe explicit source operations and exclude child operations' own counts.
They are not unique working-set size, cache misses, physical bandwidth or FFT
library-internal traffic. The JUCE fallback convolver remains an opaque
convolution span; fine FFT/MAC attribution covers the modeled backend.

### Native resource capabilities

- Linux: `getrusage(RUSAGE_THREAD)` raw fault and context-switch observations,
  labeled thread scope.
- macOS: Mach `TASK_EVENTS_INFO` faults/page-ins, labeled **process/task scope**,
  which can include the IR worker. Unsupported minor/major/thread switch fields
  stay unavailable. `THREAD_BASIC_INFO` run/suspend state is an instantaneous
  observation, not a continuous scheduler trace or hypervisor event record.
- Hardware cache counters are not collected. Unsupported API results remain
  unavailable; zero is not substituted for an absent observation.

Apple's [resource header](https://github.com/apple/darwin-xnu/blob/main/bsd/sys/resource.h)
marks the legacy `rusage` fault fields unused; this is why the diagnostic does
not claim thread page faults from zero-valued macOS `getrusage` fields.

## Run and independently verify

Example single diagnostic population (Linux is portable validation only):

```sh
/tmp/chimera-attribution-build/ChimeraCabCallbackProbe_artefacts/Release/ChimeraCabCallbackProbe \
  --mode cab --engine all --channels all --trace-level stages \
  --output-dir /tmp/chimera-attribution-evidence
python Tools/CabCallbackAttribution/verify_attribution.py \
  --input /tmp/chimera-attribution-evidence \
  --output /tmp/chimera-attribution-verification.json \
  --expected-mode cab --expected-trace-level stages \
  --expected-engines original-v1,expanded-v2,array-v3-6x10 --expected-channels 1,2
```

The verifier imports no producer/controller summary logic. It recalculates
clocks, counts, misses, event hierarchy and callback/slot association from raw
files. Its corruption tests cover missing/altered evidence. Exit **0** means
complete internally consistent diagnostic evidence, **2** means inconsistent
evidence, and **3** means incomplete/missing evidence. None is product acceptance.

The controlled Actions execution uses fresh clean checkouts and directories:

```sh
python Tools/CabCallbackAttribution/run_attribution.py \
  --source /path/to/current-clean-source --source-sha FULL_CURRENT_COMMIT \
  --frozen-source /path/to/clean-release-7b20cd9d \
  --build-root /tmp/unique-attribution-build \
  --output /tmp/unique-attribution-run \
  --order ABBA --environment native-macos
```

All targets are built before measurements. The original five CTests run first
on frozen and current hooks-off source, retaining every original population
and failure. Enabled semantic/recorder checks do not supersede them. A failed
gate is still a failure even if every subsequent diagnostic file is valid.
Subsequent registered arms are preserved without replacement reruns. Logs,
source/dependency/compiler/binary receipts and per-file SHA-256 manifests are
uploaded even when the attempt is incomplete. The old QoS controller and
original CAB performance workflow remain separate.

Local precommit builds must be identified by their actual file digests and
working-tree state. The HEAD embedded during configuration alone does not bind
uncommitted modifications. A clean source-bound native run supplies the
authoritative commit receipt.

## Host testing and later product changes

`native-macos` means a Darwin **console** allocation. This program creates no
CoreAudio device or DAW, no AU/VST3 audio wrapper, and no editor/EQ/tuner path.
All actual host validation remains `not_executed` until a separately recorded
real host/device test exists. Stage recording, native resource queries and
their timing overhead must also be reconsidered before any host diagnostic.

A product performance change belongs in a separate development PR after a
cause and repeatable benefit survive independent controls. Original response,
channel separation, full IRs, fades and real-time acceptance remain mandatory.
Instrumented console p99 success, fewer misses on one runner, or valid CSVs do
not establish uninterrupted real-host playback.
