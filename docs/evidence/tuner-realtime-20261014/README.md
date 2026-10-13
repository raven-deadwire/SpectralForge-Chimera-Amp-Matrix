# Tuner real-time investigation — 2026-10-14 KST

**Release approval: false. Response/deadline qualification remains FAIL / BLOCKED.**

This investigation extends PR #67 (`36e41a50a9179b52eba21c3e8d5f99d0aad5bbef`),
not the independently overlapping #66. Candidate implementation:
`2cf45707e7d3ad9e50569fe1c5fa13815387b975`.
No release, tag, installer, update manifest or release approval was created.
The old 290 ms response and 0.1/0.3-cent accuracy gates are unchanged.

## Preserved evidence and its limits

The original `../tuner-20261013/` directory remains intact, including the
305.648 ms E2 failure, earlier UI acquisition failure and 966.5% screenshot.
The previous local #66-oriented callback CSV is retained losslessly in
`previous-pr66-unqualified-callbacks.csv.gz.b64` (lossless base64/gzip). Its original binary/source identity
was not recorded: **do not attribute its results to #66 HEAD or a candidate**.

Full existing CI job logs are archived losslessly as `.log.gz` (stored as
`.gz.b64`; run `python3 decode_archives.py` to validate and restore), with summaries
in `previous-ci-summary.json`. Their recorded source is `36e41a5`, not the current
candidate. The old Windows/macOS UI bounds failure, all-platform CAB state/gate
parameter-count failures, and macOS response failures remain failures. A later
passing run cannot retroactively clear any of these observations.

The old screenshot and 305.648 ms run have no paired thread CPU clock. Their
individual cause cannot be reconstructed. New evidence below establishes what
happened in the **new** trace, not a retrospective explanation of those peaks.

## Minimal product change

1. Reserve/write/publish the SPSC FIFO once per audio block, instead of invoking
   FIFO atomics for each decimated sample. Commit only the samples actually
   written; preserve finite-input, generation, overflow and channel rules.
2. Hoist the delay-independent reference mean/power out of fractional-delay
   correlation evaluation. Use 22 golden-section refinements instead of 30;
   the remaining period bracket is below 0.02 cent over the supported search
   domain. The input rate, analysis window, 10 ms wait, harmonic checks,
   confidence threshold, strobe response and acceptance thresholds are unchanged.

The 576 original synthetic cases plus 18 weak-fundamental/decay/DC cases pass.
Worst reported synthetic error: **0.0606196 cent** (`candidate-core.log`). This
is not a real-instrument/sample-clock or Peterson-equivalence certification.

The same executable alternates frozen PR #67 and candidate kernels on identical
4096-sample decaying harmonic signals, reversing order each repeat. It records
paired wall/CPU times for all 120 estimates; none are filtered. Median CPU cost:

| Input Hz | PR #67 CPU µs | Candidate CPU µs | Reduction |
|---:|---:|---:|---:|
| 16.352 | 12280.77 | 10335.57 | 15.84% |
| 27.5 | 11037.59 | 8235.22 | 25.39% |
| 30.867706 | 9534.78 | 7314.99 | 23.28% |
| 82.406889 | 4114.44 | 3203.09 | 22.15% |
| 440 | 3392.33 | 2696.32 | 20.52% |
| 1318.5102 | 2663.98 | 1547.26 | 41.92% |

These are local shared-container observations, not a guaranteed speedup on each
OS or CPU. Other compilation workloads were observed in this environment.

## Response failure attribution

`baseline-response/` and `candidate-response/` retain every push and worker cycle.
Three E2 repeats each use the original phase-discontinuous 10-cent change and
an additional phase-continuous change, both at 48 kHz/256 samples. The existing
three-audio-block confirmation/290 ms rule remains. The derived report also
shows three **distinct worker publications**, so repeated reads of one estimate
cannot be mistaken for independent analysis confirmations.

Baseline: 6/6 routes pass in this trace. Candidate: **5/6**, with one initial
acquisition failure (sentinel -1, not a negative measured latency). This failure
is retained despite reduced median work:

- Candidate repeat 2, continuous fixture: push index 14 ends at 74.832 ms;
  index 15 starts at 268.156 ms. Its scheduled start is 80 ms: **188.156 ms late**.
- The worker therefore has only 1920/4096 analysis samples at 180–273 ms, with
  **zero queued samples** and no generation reset. This directly identifies an
  input-delivery shortfall during that interval, not expensive pitch estimation.
- At 292.605 ms the worker receives enough data (newest input frame 10240), then
  takes **117.245 ms wall / 60.133 ms thread CPU** to publish at 409.849 ms.
  About 57.112 ms is outside measured thread CPU; the remaining CPU cost is also
  much larger than usual. Preemption/contention is consistent with the gap;
  thread clocks alone do not identify the OS scheduling mechanism or prove that
  all excess time is scheduler delay.
- The unchanged audio-block step confirmation is 269.156 ms, but three distinct
  worker confirmations take **396.532 ms**. Reporting only the former would hide
  the slow update cadence. Both are preserved.

The unchanged 32-route integration test on the candidate also fails two routes
in this shared environment (`candidate-integration.csv/.log`): 96 kHz/64/A0 and
96 kHz/256/A4. Final steady error failures remain failures, not missing-data
passes. No further rerun is used to replace or delete them.

## Paired callback / UI / output method

`CHIMERA_TUNER_DIAGNOSTICS=ON` is an opt-in diagnostic build. Ordinary builds have
no diagnostic clock reads, trace allocation or trace writes. Storage is bounded
and allocated before audio starts; CSV/WAV output occurs after the feeder ends.
The trace records dropped records and fails if any are lost.

The actual production processor is fed a deterministic A0 harmonic signal, with
mute disabled and otherwise default parameters. Every callback, including the
one-second warm-up, is retained. Two reversed-order repeats cover:

- 48 kHz/256 and 96 kHz/64;
- tuner OFF and ON;
- no editor; editor only; editor + needle; editor + strobe.

For every callback: wall time, same-callback thread CPU/cycles, start lateness,
finish lateness, tuner push wall/CPU, and measured Hz. For every worker iteration:
queue size/discards, generation, newest source frame, analysis/publication flags,
wall start/end and paired CPU. The test keeps source-clock debt instead of
rebasing time after an interruption. Worst callback wall time is accompanied by
**that callback's** CPU value, not the independently largest CPU sample.

CPU clocks: POSIX `CLOCK_THREAD_CPUTIME_ID`; Windows `GetThreadTimes` kernel+user
time plus raw `QueryThreadCycleTime`. Windows short intervals may be quantized;
zero CPU deltas are not evidence of zero work. CPU cycles are not converted into
time ([Microsoft GetThreadTimes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadtimes), [QueryThreadCycleTime](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-querythreadcycletime)). `TunerClockControls` emits empty, sleeping and CPU-busy controls. Clock
reads and diagnostic overhead are included conservatively; no subtraction is
used to make a deadline pass.

Output is tested separately against the identical tuner-OFF/no-editor reference:
nonfinite samples, unexpected all-zero blocks after warm-up, peak/sample jumps,
and maximum sample difference. The first 48 kHz repeat writes all eight actual
production output WAVs. Exact digital equality does **not** mean absence of
physical audio-device underruns when callback deadlines are missed.

This is a normal-priority synthetic callback feeder with native UI, not an ASIO,
CoreAudio, ALSA or installed DAW test. Physical output, driver xruns and real DI
remain **NOT TESTED**. A failed wall deadline remains a failure even if its CPU
share is low, and stable rendered samples cannot override it.

## Completed local full-processor comparison

`baseline-full-processor/` is the original PR #67 production processor plus
measurement-only hooks, **not the optimized candidate**. All 32 scenarios and
78,192 callback rows are retained. Every scenario fails timing in this shared
normal-priority container: 36,379 callback wall-budget misses and 73,382 late
finishes. The worst wall time is 2,420,996.908 µs; the same callback has
2,135,316.459 µs CPU but only 72.231 µs in the tuner push. This identifies the
large measured cost as outside the tuner-input portion in that callback; it does
not isolate a specific remaining DSP stage or exclude worker/UI contention.
Tuner-OFF callbacks also miss deadlines. No 570%/966.5% historical peak is
retroactively attributed from this run.

Digital output passes independently: zero nonfinite samples, zero unexpected
all-zero post-warm-up blocks, and zero float-sample difference from the reference
in every scenario. All eight output WAVs have identical bytes. They are stored
once losslessly and the manifest restores every original filename.

The bounded worker trace overflowed by 2,246 records in three tuner-OFF scenarios
under prolonged execution. This is explicitly a **trace completeness failure**;
no complete worker analysis is claimed for those scenarios. All callback rows
remain present. The harness therefore reports three instrumentation/output-group
failures despite the separate digital-output equality pass. No records or failed
verdicts are discarded to produce a clean result.

The full raw callback/worker streams and WAV use `.xz.b64` for lossless archival.
Run `python3 baseline-full-processor/decode_archives.py` to verify and restore
them. These local results establish neither physical-driver stability nor the
optimized candidate's full-processor performance; the exact-source CI below is
separate.

## Cross-platform execution

The new [Tuner realtime attribution run](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38435179453)
checks out exact source `2cf4570` on Windows, macOS and Linux. It builds clock/cost
controls, response/production callback probes, original accuracy/integration tests,
native tuner UI and state tests. Every failed result, raw CSV, WAV, screenshot,
JUnit file and LastTest log is uploaded using `if: always()` with source SHA and
attempt in the artifact name. Failures are not marked `continue-on-error`, and
the callback probe returns failure for any observed callback or finish deadline
miss. Existing general CI continues independently.

### Exact-source candidate results collected

| Check | Linux | Windows | macOS |
|---|---|---|---|
| Original accuracy + 32-route integration | PASS | PASS | QUEUED |
| E2 48 kHz/256, six response routes, unchanged 290 ms gate | PASS | PASS | QUEUED |
| Step confirmation range, original callback-based rule | 174.736–190.758 ms | 170.247–197.366 ms | PENDING |
| Three distinct worker publications, step confirmation | 184.728–200.256 ms | 180.574–205.118 ms | PENDING |
| Native tuner UI / state | PASS / PASS | FAIL bounds / PASS | QUEUED |
| Callback/finish timing | **FAIL 6/32 scenarios** | **FAIL 30/32 scenarios** | QUEUED |
| Output equality / nonfinite / zero blocks / trace loss | PASS | PASS | PENDING |
| Physical device/driver output stability | NOT TESTED | NOT TESTED | NOT TESTED |

Each completed platform retains **78,192 callbacks** including startup. Linux:
2 callback wall-budget misses and 12 late finishes. Windows: 159 wall-budget
misses and 41,323 late finishes. The latter includes normal-priority synthetic
feeder wake-up delay; it is not a measurement of an ASIO callback or a device
xrun counter. No scheduling debt was reset to hide misses.

| Platform / configuration | Mean callback wall | Worst callback wall | Same callback CPU | Deadline |
|---|---:|---:|---:|---:|
| Linux 48 kHz / 256 | 785.170 µs | 1,872.807 µs | 1,102.709 µs | 5,333.333 µs |
| Linux 96 kHz / 64 | 240.869 µs | 760.865 µs | 762.301 µs | 666.667 µs |
| Windows 48 kHz / 256 | 646.606 µs | 15,338.200 µs | 15,625 µs, quantized | 5,333.333 µs |
| Windows 96 kHz / 64 | 197.097 µs | 10,547.200 µs | 0 µs, quantized | 666.667 µs |

The Linux 96 kHz worst callback has the tuner **OFF**, editor open, and only
1.091 µs in tuner push. Its CPU and wall are almost equal (CPU encloses the wall
clock reads), so this measured budget overrun contains actual execution cost
outside the tuner-input section. The other wall overrun also has the tuner OFF.
This does not identify the remaining DSP stage; it rules out assigning the whole
callback peak to the pitch estimator. Linux also has late finishes with callback
execution below budget, demonstrating the distinct input scheduling issue.

Windows `GetThreadTimes` deltas are observed in **15,625 µs** units. Its raw
`cpu_misses` column must not be used to classify sub-millisecond DSP overruns.
At the 96 kHz worst callback, 760,108 thread cycles accompany 10,547.2 µs wall;
the same scenario's median is 481,038 cycles. The cycle increase is much smaller
than the wall-time increase, consistent with substantial interruption, but no
cycle-to-time conversion or exact off-CPU duration is claimed. At 48 kHz the
worst callback has 3,561,712 cycles versus a 1,411,446 median: both extra work and
interruption remain possible. Both worst callbacks have tuner OFF.

The same-run paired kernel comparison on Linux reduces median CPU cost by
**4.27–24.21%**, including **12.58% at E2** (1,201.279 → 1,050.124 µs). This
smaller reduction than the shared-container result illustrates why environments
must not be pooled. Windows kernel comparisons use raw cycles rather than its
quantized per-call CPU time; exact figures are in `windows-candidate/derived-summary.json`.
Neither result proves deadline safety.

Artifacts are archived losslessly in `linux-candidate/` and `windows-candidate/`
with a per-file byte count and SHA256 manifest. Run `python3 decode_ci_archives.py`
to restore all original CSV, WAV, screenshot, JUnit, build identity and LastTest
files. Identical WAVs are deduplicated in storage and restored under all original
names. Full CI job logs are also retained as `.log.xz.b64`. The original GitHub
artifacts are [Linux](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38435179453/artifacts/11834335846)
and [Windows](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38435179453/artifacts/11833843019).

macOS is still **QUEUED** in GitHub Actions at collection, not passed or omitted.
Its old failed logs remain archived. The current PR source is not changed by this
evidence-only branch. Release remains blocked by observed misses, pending macOS
execution and absent physical-device evidence regardless of reduced median cost.

### Remaining qualification work

1. Collect the already queued exact-source macOS job without replacing prior attempts.
2. Capture per-stage processor CPU and an OS scheduling trace on affected hardware
   to isolate the remaining non-tuner cost and interruption sources. Windows needs
   ETW/WPA or an equivalently precise scheduler trace; the coarse thread clock alone
   cannot settle attribution.
3. Run the same on/off matrix through ASIO, CoreAudio and ALSA/JACK or a real DAW,
   recording driver xruns and loopback/output capture together with callback timing.
   Include real DI pitch changes. These device-dependent checks require hardware
   access not present in the hosted CI/container.
4. Retain the 290 ms and accuracy gates and every previous failure; do not mark
   either overlapping Draft PR as release-approved until the independent timing,
   response and physical-output gates are supported by evidence.
