# Chimera system performance measurements

This is a collection and audit framework for issue #97. It does **not** certify
CPU/RAM minima, alter release gates or claim that collector tests measured Chimera.
The public compatibility/package baseline remains 1.3.2-beta.1. A new integrated
Release candidate needs its own exact source, executable and package identities.

## What is executable now

- `performance.py`: Python 3.10+ standard-library CLI; observed environment capture,
  platform/format planning slots, scenario generation, binary/input/state hashing,
  process RSS/working-set sampling, callback CSV ingestion, raw-evidence verification
  and Markdown environment/results tables. No external Python packages are needed.
- `CallbackRecorder.h`: single-thread callback recorder for an instrumentable host
  or full-processor harness. Preallocate before playback; record wall and thread CPU
  clocks without callback file I/O or C++ heap allocations; export after stopping.
- The independent CMake project/CI tests the collector on Windows/macOS/Linux.
  **It does not build, load or benchmark the Chimera plugin.**
- Optional root target `ChimeraFullGraphHost` now links the existing production
  `ChimeraAmpMatrix` target and measures complete `processBlock` calls with
  1/2/4 independent parallel tracks, including fan-out and fan-in. See
  [full-graph host](FULL_GRAPH_HOST.md). This is synthetic paced-processor
  evidence, not the installed Standalone wrapper or a DAW/device qualification.

There is no installed proprietary-DAW profiler/adapter in this PR. The header is
not injected into the released plugin. Actual callback capture needs a host-side
adapter around the complete processing graph, or an equivalent host trace export.
When the host cannot expose callbacks, leave those counters unavailable and keep
the full measurement BLOCKED; never substitute the DAW CPU meter or a process
CPU percentage. Memory-only observations can still be retained as partial CSVs.
An actual product binary, host adapter, DAW/device and the source-bound workload
are prerequisites, not evidence produced by this framework.

## Default test matrix

All scenarios use **48 kHz, 128/256 samples and 4× amp oversampling**.

| Group | Conditions | Count per environment |
| --- | --- | ---: |
| core | Classic / Dual Blend / Matrix × 1/2/4 active parallel-track instances × both buffers × Transpose bypass/−2; editor closed, analyzer off | 36 |
| dual_crossover | Dual Crossover × 1/2/4 × both buffers × bypass/−2; closed/off | 12 |
| gui_focus | Each of the three modes × both buffers × bypass/−2; 4 instances, editors open and analyzers on | 12 |
| soak | Matrix, 4 instances, 128, −2, open/on; independent continuous playback and recording observations | 2 |

Short comparisons request 10 seconds of warmup followed by 60 seconds of retained
callbacks. The two representative soak observations request **1800 seconds each**
after warmup, not 30 minutes combined. They do not prove 30-minute stability of
every short-test condition. If this representative configuration fails, retain
it and add a separately identified lower-load soak; do not replace the failure.
The init command never launches a 62-condition unattended audio session.

Use a fixed, permission-cleared dry DI WAV including Low B attacks/sustains and
chords, with fixed levels and a documented loop boundary. The same input, state
bank and IR manifest must be used for the comparison. Save the actual state for
each mode (including amps, PRE/POST, CAB paths and mic count, filters, mix, gate,
GUI/analyzer settings). The bank hash binds the collection of per-scenario states;
the scenario hash distinguishes mode, CAB paths, count, shift, UI and transport.
Disable host anticipative/offline rendering for real-time tests and record host
multicore scheduling, monitoring and power settings. Verify all instances actually
process signal; a bypassed, suspended or idle instance is not an active instance.
Map Classic/Dual/Matrix to 1/2/3 CAB paths per instance, or document a separate
workload if exercising a different configuration. Do not silently use filter-only
CAB settings to represent the complete installed product.

For comparative conclusions, repeat each selected condition at least three times,
retain order/repeat identifiers, alternate conditions and include identical-binary
controls. Avoid builds/downloads in parallel. A single run is an observation, not
a hardware ranking. Comparisons within a study are bound to one executable hash;
different OS/architecture/format binaries require separate studies even when built
from the same source. Preserve all failed runs and raw traces.

## Collect a workstation study

From the repository root:

```sh
python Tools/SystemPerformance/performance.py init --out measurements/workstation.json
python Tools/SystemPerformance/performance.py targets --out measurements/targets.json
```

Fill actual OS/build, CPU, physical/logical/allocated cores, RAM, virtualization,
DAW/version, native plugin format, interface/firmware, driver/version, power
mode/source and scheduler fields. Auto-detection is deliberately conservative:
container host-visible RAM/cores are not effective limits; cgroup observations
are retained separately. Windows CIM/powercfg, macOS sysctl/pmset and Linux procfs
reads do not modify power settings. Unknown fields stay `null`/`UNKNOWN`.
The target roster covers Windows x64, macOS Intel/Apple Silicon and Linux x64,
with platform-appropriate VST3/AU/Standalone and separate AAX goals. Roster entries
are planning slots, not claimed installed systems. No Pro Tools version is assumed.

Bind the **actual loaded executable file** (the module inside a VST3/AU/AAX bundle,
or Standalone executable), not the directory name or a similarly named build:

```sh
python Tools/SystemPerformance/performance.py bind measurements/workstation.json \
  --binary /absolute/path/to/loaded-module --source-sha FULL_40_HEX_SOURCE_SHA \
  --version ACTUAL_VERSION --build-id ACTUAL_BUILD_ID \
  --input /absolute/path/to/DI.wav --preset-bank /absolute/path/to/states.zip \
  --ir-manifest /absolute/path/to/ir-hashes.json \
  --description "Exact state bank, input levels and IR manifest" \
  --out measurements/bound.json
```

Independently verify Release configuration, embedded build identity and package
hash; `bind` hashes files but cannot infer their build provenance. Store the package
SHA in `product.package_sha256` when available. Existing measured studies cannot
be rebound to a different binary. Use separate runs/studies for repeats; do not
overwrite an earlier measurement. SDK availability or unsigned AAX development
builds do not establish general Pro Tools compatibility.

Obtain a PID from the **DAW/host's own process namespace**; start the RSS sampler
at the same retained measurement boundary as the callback adapter, after warmup:

```sh
python Tools/SystemPerformance/performance.py sample-memory --pid 12345 \
  --seconds 1800 --interval 1 --out measurements/memory.csv
python Tools/SystemPerformance/performance.py receipt-template measurements/bound.json \
  --run soak-matrix-none-i4-b128-t-2-recording --out measurements/receipt.json
```

The generated receipt is deliberately incomplete. The host adapter/operator must
fill **observed** collector version, scope, process ID, dropped rows, warmup,
transport duration/continuity, recording verification, crash/xrun/glitch observations
and session reopen status. `settings_verified=true` means the loaded settings and
active parallel routing were read back/checked, not merely requested. Include
measurement clock resolution and monitoring overhead in notes. Do not claim
unavailable counters are zero. Retain DAW transport logs and recording file hashes
in the study's evidence directory and identify them in receipt notes.

In an instrumentable host, preallocate capacity for the whole measured duration
(675,000 callbacks at 48 kHz/128 for 1800 seconds, plus scheduling margin):

```cpp
// Before playback, on a non-audio thread:
chimera_perf::CallbackRecorder recorder(700000);
// In the single full-graph callback, after warmup:
auto stamp = recorder.begin();
processAllActiveChimeraInstances(); // adapter-specific, same release module(s)
recorder.end(stamp, frames, scheduledDeadlineNs); // use 0 if unavailable
// After stopping and joining the callback thread:
recorder.writeCsv("callbacks.csv");
// Put recorder.droppedRows() into receipt.telemetry_dropped_rows.
```

The example is an adapter contract, not an implemented DAW API. The independent
host implements this boundary with `FullGraphAdapter.h`. Timing each
instance separately and summing p99 values is invalid; enclose the whole graph.
Do not log/allocate or sort inside the callback. Quantify clock/adapter overhead
with a separately retained empty-graph control before interpreting small changes.
The test's zero-allocation result covers the recorder's C++ new calls only, not
the plugin, OS clock implementation, all C allocations or all real-time safety.

## Raw contract and calculations

`callbacks.csv` header is exactly:
`index,start_ns,end_ns,thread_cpu_ns,deadline_ns,frames`.
Indexes start at zero and are contiguous. Timestamps share one monotonic clock;
deadline is the absolute **scheduled finish** timestamp on that same clock.
Do not construct a fresh deadline as `actual_start + period`: that hides late
wakeups. Blank CPU/deadline fields mean unavailable, for the whole population.
Every measured callback is retained, including warm scheduling tails and misses.
Warmup is outside this file and its duration is separately attested.

`memory.csv`: `elapsed_seconds,pid,rss_bytes`, one sample per second by default.
Windows records process working set, Linux/macOS RSS. All plugin instances and
the DAW share this process value; it is not per-plugin heap usage. Samples are
not an OS high-water mark and can miss sub-second peaks. Growth is end minus
start and may be negative; it alone does not diagnose a leak. PID reuse/exit
aborts collection. Existing user processes are never terminated by the tool.

- Period = frames / sample rate. 128/256 at 48 kHz means 2.667/5.333 ms.
- Callback wall load = sum wall time / sum block periods × 100.
- Callback thread CPU load uses measured thread CPU time over the same periods;
  one callback thread/core = 100%. Worker threads/process CPU are outside it.
- p99 uses nearest rank over **all retained callbacks**; peak is max, not a
  percentile of percentiles. Per-callback wall and thread CPU are separate.
- `compute_over_budget_count`: wall processing duration > one block period.
- `scheduled_deadline_miss_count`: actual finish > scheduled finish, including
  wakeup delay when supplied. It is `null` without scheduled-deadline telemetry.
- Host/device xruns and observed glitches come from the observation receipt;
  they are not inferred from CPU or compute misses.

```sh
python Tools/SystemPerformance/performance.py ingest measurements/bound.json \
  --run soak-matrix-none-i4-b128-t-2-recording \
  --callbacks measurements/callbacks.csv --memory measurements/memory.csv \
  --receipt measurements/receipt.json --out measurements/result.json
python Tools/SystemPerformance/performance.py validate measurements/result.json
python Tools/SystemPerformance/performance.py table measurements/result.json \
  --out measurements/environment-and-results.md
```

Imports copy raw files into content-addressed paths. Validation re-hashes them,
checks identities and re-derives every metric; edited summaries cannot pass by
themselves. Duplicate JSON keys, non-finite numbers, wrong types, missing fields,
frame/count gaps, overlapping callbacks, partial counters, dropped telemetry,
stale receipts, incomplete durations and memory sampling gaps are rejected.
The closed Draft 2020-12 schemas also support independent schema validators.

## Status and publication rules

`NOT_RUN` = planned, not attempted. `BLOCKED` = a named prerequisite is unavailable.
`MEASURED` = a valid population was imported, **not** a performance acceptance.
`FAILED` = collection failed; retain partial logs separately and leave summary
metrics null. A measured row can contain failures/misses and a FAIL soak.

Soak `PASS` means a completed actual DAW/device observation with continuous
transport, no reported crash/xrun/glitch/scheduled-deadline miss, and verified
recording for a recording run. Unknown counters leave that observation BLOCKED.
Synthetic/offline/paced-processor runs never pass the real-device soak. Session
reopen is a separate recorded check and must be PASS before claiming session
acceptance. No numerical CPU headroom or leak threshold is invented here; choose
and approve those criteria before hardware qualification. No run certifies CPU,
RAM, a maximum instance count, OS support or release approval automatically.

Keep public build/package verification separate from these prospective results.
8 GB / 4 cores and 16 GB / 6–8 cores remain **unmeasured planning candidates**.
Do not reuse CAB-only, idle-GUI or an older binary's tests as full product evidence.
Update `docs/SYSTEM_REQUIREMENTS.md`, README, INSTALLATION, all manual languages
and download-facing release-note source together only with scoped new evidence.
