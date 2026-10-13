# Local callback attribution validation — 2026-10-12

The Linux console checks completed successfully, and the independent verifier
accepted the expected raw artifact sets for each population. This establishes
local build, semantic, trace-contract, and artifact-integrity evidence. Product
performance improvement remains **unproven**. macOS console results and real audio
host validation are not established by this receipt.

[Machine-readable receipt](local-validation.json) preserves source and binary
SHA-256 values, exact available execution receipts, test results, per-route wall
and thread CPU summaries, and hashes for 234 raw route files. The original CSVs
and full verification reports remain in the local validation workspace; they
are not committed here. Their hashes identify files supplied separately and do
not replace the raw evidence.

## Environment and source binding

These runs used Linux x86_64, GNU 13.3.0, Release builds, and JUCE 8.0.8 at
`d6181bde38d858c283c3b7bf699ce6340c050b5d`. The ordinary frozen release gate used
`7b20cd9d1f90e3e2969c250712b199e893cf50cd`.

The new local binaries were built from a working tree whose embedded commit was
still `f89564f2444359d51bac7d8d0b1a53d2abeeb129`. Their source manifests and binary
hashes are retained separately. The later checked-out commit
`35617a2e0a5a599c5bd2b2a13113b6f234cd60a3` is **not** an embedded clean-commit
binding for these local binaries.

Two source batches are kept distinct:

| Local batch | Probe source SHA-256 | Coverage |
|---|---|---|
| Before explicit page touching | `e3628dbbb92312a2e46507745ba0b52fb6b08881c62300cdc8b952de0a24c1c1` | 36 route runs; 57,600 intervals |
| After explicit page touching | `9257527520f64c70e21dedc413dd9ebbed0ce1b831fce3d1ae7fb4fd829ae88b` | 3 route runs; 4,800 intervals |

The first batch's source and binary hashes are preserved verbatim from
`final-local-source-and-binaries.json`. The final local change explicitly touched
the prepared storage outside the measured interval in both probe binaries and
added corresponding metadata. All three subsequent cases report 20,122 touch
operations with a 4,096-byte stride; memory locking and guaranteed continued
residency remain false. The broad earlier batch does not validate that later
probe revision across all routes.

The final local diagnostic/control binary hashes are respectively
`6bcdb99e683cf46183c4fdf5a572aa34a4e27466b9d1d8599af178c403c695e4`
and `55048caeadeaeca2d6bf0cd69479c584345e27ac470a0accf60595805549b506`.
The final local build source manifest SHA-256 is
`d582a23b755f0093c8bee83135f04aeb79c3f91139a7208c357619a481433ae0`.

## Existing gates and diagnostic checks

| Check | Observed result |
|---|---|
| Frozen release: five original console targets | 5/5 passed; 66/66 original p99 gates passed; 105,600 callbacks; **49 individual wall deadline misses** |
| Working tree with attribution compiled out: same five targets | 5/5 passed; 66/66 original p99 gates passed; 105,600 callbacks; **26 individual wall deadline misses** |
| Diagnostic-enabled realtime semantic target | Exit 0; 144 equivalence routes, direct FIR six-mic checks, exact residue reference checks, 50 ms fade, and zero latency reported |
| Trace-contract executable | Exit 0; zero-capacity, bounded overflow, parent/depth, endpoint nesting, kernel context, packed worker state, and TLS stop checks passed |
| Independent verifier corruption tests | 21 tests passed; exit 0; reported duration 11.537 seconds |

The 49 and 26 misses are retained observations from separate sequential Linux
runs. They establish neither a causal reduction nor a macOS performance fix.
Passing the original p99 gate still permits individual deadline misses. The
working-tree gates, enabled semantic target, and trace-contract execution belong
to the first source batch; they were not repeated as clean final-commit builds
for this local receipt.

The first working-tree build attempt failed with
`No rule to make target 'ChimeraCabAttributionRealtimeTests'` because the existing
build tree had not configured the new target. Its captured output and log hash
are retained in the JSON, and the attempt is marked failed. Reconfiguration and
the subsequent build produced the targets used by the successful runs. The
post-touch reconfiguration and rebuild of both probe binaries also returned 0.

## Independently verified diagnostic populations

Every route below contains 1,600 intervals at 96 kHz / 64 samples, with the
prescribed 1,200/400 phase split. CAB populations cover all three engines and
both channel counts unless explicitly restricted. Arithmetic, memory, and clock
controls use `original-v1` and both channel counts.

`off` uses the separate binary with hooks compiled out; `stages` and `resources`
use the diagnostic binary. This A/B distinction measures instrumentation
overhead and differs from the historical QoS intervention. Resource capture is
an additional diagnostic population. These local executions were not the
independent ABBA/BAAB experiment.

### Before explicit page touching

| Population | Route runs | Intervals | Wall deadline misses retained | Integrity result |
|---|---:|---:|---:|---|
| CAB / off | 6 | 9,600 | 11 | Complete |
| CAB / stages | 6 | 9,600 | 15 | Complete |
| CAB / resources | 6 | 9,600 | 5 | Complete |
| Arithmetic / off | 2 | 3,200 | 0 | Complete |
| Arithmetic / stages | 2 | 3,200 | 1 | Complete |
| Arithmetic / resources | 2 | 3,200 | 0 | Complete |
| Memory / off | 2 | 3,200 | 0 | Complete |
| Memory / stages | 2 | 3,200 | 2 | Complete |
| Memory / resources | 2 | 3,200 | 0 | Complete |
| Clock / off | 2 | 3,200 | 0 | Complete |
| Clock / stages | 2 | 3,200 | 1 | Complete |
| Clock / resources | 2 | 3,200 | 0 | Complete |

These are 36 distinct route runs and 57,600 intervals. No timing distribution is
pooled across the rows.

### After explicit page touching

Only `original-v1`, stereo CAB was rerun locally. The `pretouch-cab-*` directory
names refer to runs **after** adding the explicit touch operation.

| Population | Route runs | Intervals | Wall deadline misses retained | Integrity result |
|---|---:|---:|---:|---|
| CAB / off | 1 | 1,600 | 0 | Complete |
| CAB / stages | 1 | 1,600 | 3 | Complete |
| CAB / resources | 1 | 1,600 | 4 | Complete |

“Complete” means the independent verifier found the required route set, callback
indices, raw clock consistency, event trees, slot snapshots, separate worker and
caller streams, and explicit error/availability records intact. It does not mean
zero misses or product acceptance. Compiled-out populations legitimately have
zero trace events and no stage attribution. Raw CPU-greater-than-wall samples
and all deadline misses were preserved.

## Remaining validation and interpretation limits

Native macOS CI run
[38225976064](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38225976064)
was requested for commit `35617a2e0a5a599c5bd2b2a13113b6f234cd60a3`, with independent
ABBA and BAAB allocations. Its results were **pending at preparation of this
local receipt** and are not evaluated here. Native results require their own
source-bound receipts and artifact review.

AU, VST3, CoreAudio device callbacks, uninterrupted DAW playback, and editor
interaction are **NOT_EXECUTED**. Linux `thread_rusage` validation does not
validate macOS resource APIs. Wall-minus-CPU and parent-minus-direct-child wall
residuals do not measure exclusive scheduler or virtualization delay. Worker
overlap remains descriptive; logical memory bytes are not measured cache misses
or hardware bandwidth. The C++ allocation guard covers observed `new`/`delete`
operators, not every allocation or lock implementation.

The [attribution plan](../callback-attribution-plan.md) and
[experiment instructions](../../../../Tools/CabCallbackAttribution/README.md)
remain the interpretation contract. A cause-specific performance change needs
independent repeat evidence and a separate development PR; existing response,
channel separation, fade, and realtime gates retain their meaning.
