# Native macOS callback attribution: independent ABBA audit

- Source: `35617a2e0a5a599c5bd2b2a13113b6f234cd60a3`.
- Frozen source: `7b20cd9d1f90e3e2969c250712b199e893cf50cd`.
- Run: [38225976064](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38225976064), ABBA job `114733463181`, artifact `11737181865`.
- Archive SHA-256: `24bbf5281214372945badc89fe8f754a4474a37f2b19db0eecd85943567b2486`.
- Audit result: **PASS**. This means evidence integrity and completed console contracts, not a demonstrated product performance fix.

## Provenance and gate checks

Every one of 563 internal manifest hashes matched. The manifest SHA-256 is `931a20e9d5fd91cc7bf115d2a3f5aab6e5be0dc299d006fd31db0ed42f033ebd`; the complete producer report remains in the artifact as `report.json`, SHA-256 `1bb54d8621081591efd85e77c58cf1c811880660345d723b71c7551a42b34251`. All 567 ZIP entry sizes matched extracted files. The ZIP digest matches the GitHub artifact receipt. Source before/after records match clean commit `35617a2e0a5a599c5bd2b2a13113b6f234cd60a3` and its Git tree; frozen records match clean `7b20cd9d1f90e3e2969c250712b199e893cf50cd`. The nine compiled source hashes and seven controller hashes were checked against immutable Git blobs.

Both original five-test CTest gates were reparsed, including all 66 timing routes and 105,600 callbacks per gate. The frozen gate retains 64 individual deadline misses and the current hooks-off gate retains 59; all routes nevertheless pass the unchanged p99 checks. The trace contract and enabled semantics commands exited zero. All 28 controller commands succeeded, and all 20 registered mode directories were independently reverified from raw CSV with the full expected engine/channel grid. No mode was replaced by a new attempt.

The recorded compiler is AppleClang 17.0.0.17000013, targeting Darwin arm64 with macOS 12.0 deployment. The host identifies itself as Apple Virtual Machine 1 / Apple M1 (Virtual), three cores and 7 GB RAM. It is a virtual console environment. The recorded worker priority `-1` is JUCE `Thread::Priority::low`, with a valid scheduling receipt; it is not an unavailable-value sentinel. No QoS setter was called. The recorded executable fingerprints are consistent across arms, but the archive does not contain executable files for independent byte-level rehashing.

## CAB results at 96 kHz / 64 samples

Each row contains six routes (three engines, mono/stereo), 9,600 callbacks. Counts summarize the registered population; timings are interpreted per route and phase, as preserved in the JSON. Each maximum refers to one actual callback whose route is identified below. The deadline is 666.666… microseconds. Resource sampling is a separate population.

| Trial | Trace | Callbacks | Misses | CPU > wall | Misses also over CPU budget | Max wall (µs) | CPU of same max callback (µs) |
|---|---|---:|---:|---:|---:|---:|---:|
| 01-A | off | 9600 | 2 | 5123 | 0 | 13757.000 | 68.667 |
| 02-B | stages | 9600 | 10 | 4451 | 1 | 15477.000 | 145.917 |
| 03-B | stages | 9600 | 16 | 4460 | 16 | 2963.000 | 2962.500 |
| 04-A | off | 9600 | 2 | 5526 | 1 | 1017.000 | 412.584 |
| resources-after | resources | 9600 | 27 | 4467 | 20 | 5713.000 | 916.875 |

The two compiled-out A populations contain 4 CAB deadline misses; the two stage-enabled B populations contain 26. The resource population contains 27 and remains separate. This registered A/B intervention measures the observer effect of instrumentation. It is not a product optimization comparison. Different realized asynchronous worker exposure also remains visible in the raw traces.

## Synthetic controls

Each cell shows deadline misses / 3,200 callbacks. Arithmetic, memory and empty-scope clock populations stay separate from CAB and from one another.

| Trial | Arithmetic | Memory | Clock |
|---|---:|---:|---:|
| 01-A | 0 / 3200 | 0 / 3200 | 0 / 3200 |
| 02-B | 0 / 3200 | 0 / 3200 | 0 / 3200 |
| 03-B | 1 / 3200 | 1 / 3200 | 3 / 3200 |
| 04-A | 0 / 3200 | 0 / 3200 | 0 / 3200 |
| resources-after | 1 / 3200 | 0 / 3200 | 0 / 3200 |

In 03-B, deadline misses also appear in arithmetic, memory and clock controls. A high thread-CPU interval is therefore not sufficient to identify CAB convolution as its cause. Thousands of raw CPU deltas exceed wall deltas. The clocks have separate read envelopes and resolutions, and run in a virtualized host; these records do not establish the cause of each discrepancy. Every raw value is retained and none is clamped.

## Same-callback exemplars

The following stage spans refer to the largest wall callback within each CAB population. Parent/child rows are inclusive and must not be added. The JSON also preserves bounded examples from failing synthetic populations. Each selected callback includes its complete raw callback row, separate body and outer-envelope windows, the selected leaf and all ancestor rows, exact slot snapshots, raw overlapping worker events, and source-file hashes.

| Trial | Route / callback | Wall (µs) | Thread CPU (µs) | Largest leaf span | Worker state before → after |
|---|---|---:|---:|---|---|
| 01-A | array-v3-6x10 / 2 ch / 2 | 13757.000 | 68.667 | Unavailable; hooks compiled out | Unavailable; hooks compiled out |
| 02-B | expanded-v2 / 2 ch / 910 | 15477.000 | 145.917 | overlap_add: 15340.000 µs wall / 12.375 µs CPU | 7 → 7 |
| 03-B | expanded-v2 / 1 ch / 713 | 2963.000 | 2962.500 | filters: 1124.000 µs wall / 1125.500 µs CPU | 7 → 7 |
| 04-A | original-v1 / 2 ch / 121 | 1017.000 | 412.584 | Unavailable; hooks compiled out | Unavailable; hooks compiled out |
| resources-after | expanded-v2 / 2 ch / 514 | 5713.000 | 916.875 | kernel_swap: 5227.000 µs wall / 436.167 µs CPU | 7 → 7 |

Worker state numbers: 0 idle, 1 run, 2 build, 3 generate, 4 prepare, 5 publish, 6 collect, 7 wait, 8 cancelled, 9 error, 10 exit. Compiled-out rows have invalid worker snapshots, so their default numeric zero is not an observed idle state. A long wall interval with little measured thread CPU records a wall/CPU discrepancy. Clock accounting and recorder boundaries can contribute; these logs cannot apportion the discrepancy among those effects, scheduler preemption, virtualization, or other causes. Conversely, long leaf intervals may include the tracing clock, user/kernel execution, faults, or interruptions; they do not establish pure DSP computation.

### Linked longest elapsed callback

In `trials/02-B/cab/cab-expanded-v2-96000-64-2-stages.callbacks.csv`, callback `910` has **15,477,000 ns body wall time and 145,917 ns thread CPU time**. Its separately recorded outer envelope is 15,478,000 ns wall and 147,291 ns CPU. The body starts at wall 783,274,606,000 ns and ends at 783,290,083,000 ns.

The same-callback event lineage is `122791 → 122834 → 122837 → 122840 → 122841 → 122852`. Leaf event `122852`, `overlap_add`, belongs to lane 1 / mic 1 / active kernel. It records **15,340,000 ns wall, 12,375 ns CPU and 3,072 logical bytes**. These nested parent durations include the leaf and must not be added. This localizes the long elapsed interval to a measured region; it does not demonstrate that the overlap arithmetic, cache misses, OS scheduler, or hypervisor caused it.

Both valid worker snapshots record sequence 348 and state `wait`. The worker `worker_wait` interval spans wall 783,214,055,000–783,324,343,000 ns, encompassing this callback. All six slots are active before and after; none is fading or has a pending kernel at either snapshot, and each active generation is 1. Those snapshots and raw event endpoints are retained in `native-ABBA.json` so the linkage can be reproduced without adding inclusive parent/child costs.

## Memory and scheduling scope

Only the resource population requests resource snapshots. On macOS, TASK_EVENTS_INFO supplies process-wide faults and page-ins; the IR worker and recorder can contribute. Per-thread minor/major faults, voluntary/involuntary context switches and hardware cache counters are unavailable, and are retained as unavailable rather than zero. The CAB resource population records 39 process-wide faults across 11 callbacks and zero process page-ins; all 9,600 task and thread-state captures are valid. All sampled thread run-state pairs are `RUNNING → RUNNING`. The three synthetic resource controls record zero task faults and page-ins in their sampled windows. `thread_basic_info` is sampled at the boundaries only. A running-state snapshot does not prove continuous execution.

All modeled memory counters are logical source-level reads/writes. They are not cache misses, physical traffic or measured memory bandwidth. Arrays were page-touched before measurement, but memory was not locked and residency was not guaranteed.

## Boundaries

The audit reruns the reviewed evidence verifier and recomputes callback statistics independently of producer summaries. It does not replay captured audio samples because this artifact does not contain those samples. Existing semantic executable receipts are preserved separately. Actual AU/VST3/CoreAudio/DAW playback and a physical audio device remain untested. No causal product performance correction is authorized by this evidence alone.

Full raw files remain in the source-bound GitHub artifact. Every native miss is retained in the accompanying combined `native-deadline-misses.csv`; compact per-route counts, indices, fingerprints and same-callback spans are in `native-ABBA.json`.
