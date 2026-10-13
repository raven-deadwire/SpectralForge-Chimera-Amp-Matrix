# Native BAAB callback attribution — independent raw audit

The BAAB allocation of [run 38225976064](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38225976064)
completed, and the root-owned, source-bound verifier independently accepted all
**20 populations, 60 route runs, and 96,000 callback intervals** from its raw
files. Every mode returned `verified_complete` with exit 0, no integrity errors,
and no incomplete records. This establishes console diagnostic integrity;
**product performance improvement remains unproven**.

[native-BAAB.json](native-BAAB.json) contains the complete bounded receipt:
per-route and per-phase wall/CPU quantiles, 360 raw route file hashes, source and
compiler bindings, command order, native resource availability, and 28 selected
same-callback joins. [native-deadline-misses.csv](native-deadline-misses.csv)
preserves every raw field for all deadline misses from both allocations,
including the **291 BAAB misses** across its separately identified modes.

## Source, artifact, and environment

Candidate source before and after execution was clean at
`35617a2e0a5a599c5bd2b2a13113b6f234cd60a3`, tree
`27e6ea93d1eb810a4ba9646cf6260371aec56cbe`. The frozen original gate source was
clean at `7b20cd9d1f90e3e2969c250712b199e893cf50cd`, tree
`1b565308db6adc00937f7154905728ba27bcd6d9`. Source-manifest hashes matched the
committed source, and the reviewer executed the repository verifier rather than
code from the downloaded artifact.

Artifact **11737172076** is 108,323,230 bytes. Its independently recomputed ZIP
SHA-256 matches GitHub's published digest:
`73c6a82ca73368f639f843d8a67dd29bb10d96e7aeaa6c5bbe17655ba0ed943b`.
All 563 internal manifest entries matched SHA-256 and ZIP file sizes. The 567
archive members also include the hash manifest and three top-level attempt/test
receipts. The initial `not_yet_executed` marker is preserved alongside the final
completed report.

The runner reports **Apple Virtual Machine 1 / Apple M1 (Virtual)**, arm64, three
cores, 7 GB RAM, Darwin 24.6.0, and AppleClang 17.0.0.17000013. Both builds use the
pinned JUCE revision `d6181bde38d858c283c3b7bf699ce6340c050b5d`. All 28 recorded
commands exited 0. The exact sequence was resources-before, B, A, A, B.

The control has `CHIMERA_CAB_ATTRIBUTION` undefined, so its hooks compile out;
the diagnostic and enabled semantic targets define it as 1. CI-recorded binary
fingerprints match every trial's selected executable. Executable bytes are not
archived, so this audit verifies fingerprint consistency rather than claiming
an independent binary rehash. A and B have different binary hashes, and no
binary identity across independent allocations is assumed.

## CAB callbacks and controls

Every CAB row below contains all three engines, mono and stereo, at 96 kHz / 64
samples: six route runs and 9,600 intervals. The budget is approximately
666.667 µs. Counts retain every individual wall miss; timing quantiles remain
per route and phase in the JSON.

| Population in execution order | Wall misses | Worker/automation phase | Forced-publication phase | Wall misses also over CPU budget |
|---|---:|---:|---:|---:|
| Resources-before, B binary | 102 | 98 | 4 | 43 |
| 01-B, stages enabled | 57 | 47 | 10 | 55 |
| 02-A, hooks compiled out | 12 | 12 | 0 | 11 |
| 03-A, hooks compiled out | 5 | 5 | 0 | 3 |
| 04-B, stages enabled | 101 | 85 | 16 | 74 |

The registered arms therefore retain **A: 17; B: 158** CAB misses. The 102
resource-enabled misses remain separate. This A/B experiment concerns
instrumentation sensitivity, not the historical QoS intervention or a proposed
performance fix. Asynchronous worker publications are not assumed to provide
identical exposure in every trial.

Each synthetic control population has two channel routes and 3,200 intervals.
Their misses are also retained independently:

| Control | Resources-before | 01-B | 02-A | 03-A | 04-B |
|---|---:|---:|---:|---:|---:|
| Arithmetic | 0 | 1 | 0 | 0 | 0 |
| Memory | 0 | 0 | 0 | 2 | 0 |
| Empty-scope clock | 3 | 6 | 0 | 0 | 2 |

The verifier checked 4,674,276 stage events, 3,580 worker events, 31,779 separate
caller events, and 345,600 slot snapshots. It preserved all 46,249
CPU-greater-than-wall samples across the distinct populations. These totals
describe artifact inventory; they are not pooled timing distributions.

## Same-callback attribution examples

The largest wall and CPU callback were selected separately within each
population. The JSON joins their exact route and callback index to parent/child
stage IDs, model/generation identity, six-slot before/after snapshots, and
overlapping worker events. Inclusive parent and child durations are not added
together.

| Identified callback | Body wall / thread CPU, µs | Observed stage and worker context |
|---|---:|---|
| Resources-before; array-v3-6x10, mono, #810 | **39,689 / 156.959** | Lane 2 / mic 1 `convolution` in the active role spans 39,560 wall / 30.208 CPU µs. Its parent-minus-direct-child wall residual is 39,553 µs. Worker sequence 256 remains in wait. |
| 01-B; expanded-v2, mono, #504 | **4,221 / 4,206.333** | Lane 1 / mic 1 `fft_forward` spans 3,939 wall / 3,942.209 CPU µs. Worker sequence 186 remains in wait. |
| 04-B; expanded-v2, stereo, #67 | **26,433 / 347.792** | One lane 1 / mic 1 `spectral_mac` event spans 17,332 wall / 95.459 CPU µs. Worker snapshots move from sequence 98, wait, to 103, generate. |
| 04-B; original-v1, stereo, #81 | **8,480 / 8,412.5** | Lane 0 / mic 1 `spectral_mac` spans 8,304 wall / 8,236.417 CPU µs. Worker sequence 100 remains in wait. |

These callbacks contain kernel-swap scope entries but no actual
accept/reject/retire event or fade stage. The 39.689 ms resource case reports no
task-fault or task-page-in delta in its observation window. Neither that absence
nor a sleeping worker identifies the operating-system or virtualization cause.
The convolution residual includes unscoped work and clock boundaries; it is
not a measurement of exclusive scheduler delay.

The controls prevent these stage locations from becoming unsupported CAB-only
causal claims. In 01-B, arithmetic callback #1136 has 1,891 wall / 30.042 CPU µs,
and empty-scope clock callback #639 has 1,610 wall / 1,609.916 CPU µs. In 03-A,
memory callback #939 has 1,334 wall / 1,340.333 CPU µs. An FFT/MAC outlier locates
an observed interval; it does not alone justify a convolution performance patch.

## Native resource and acceptance boundaries

The resources-before population has valid `process_task_events` observations
and thread-state snapshots on all 19,200 intervals. Task counters include the
worker process activity; they are not callback-thread-exclusive. Minor/major
fault splits and voluntary/involuntary switch counts remain explicitly
unavailable (`-1`). Thread state is observed at boundaries rather than
continuously. Prepared storage was touched before measurement using the native
16,384-byte page size, but pages are not locked and continued residency is not
guaranteed. Logical memory bytes are not hardware cache or bandwidth counters.

The original frozen and current hooks-off gates each passed five tests and all
66 original p99 routes, with 105,600 callbacks per gate. Their individual wall
misses were 25 and 56 respectively; these sequential gate counts establish no
causal regression or improvement. The diagnostic-enabled semantic target and
trace-contract target exited 0. The new 21-test corruption suite and existing
20-test callback evidence suite also passed in CI.

AU/VST3 hosting, CoreAudio device callbacks, uninterrupted DAW playback, and
editor interaction remain **NOT_EXECUTED**. The completed virtual-console audit
does not establish those outcomes. Any cause-specific performance change still
requires independently repeated evidence and a separate development PR while
preserving the existing response, channel, fade, and realtime gates.
