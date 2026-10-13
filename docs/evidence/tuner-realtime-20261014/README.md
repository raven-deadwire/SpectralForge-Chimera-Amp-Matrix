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

## Cross-platform execution

The new [Tuner realtime attribution run](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38435179453)
checks out exact source `2cf4570` on Windows, macOS and Linux. It builds clock/cost
controls, response/production callback probes, original accuracy/integration tests,
native tuner UI and state tests. Every failed result, raw CSV, WAV, screenshot,
JUnit file and LastTest log is uploaded using `if: always()` with source SHA and
attempt in the artifact name. Failures are not marked `continue-on-error`, and
the callback probe returns failure for any observed callback or finish deadline
miss. Existing general CI continues independently.

Run status and complete native callback results are recorded in the accompanying
status/summary files when collected. A queued or running job is **PENDING**, not
PASS. Release remains blocked regardless of reduced median compute cost.
