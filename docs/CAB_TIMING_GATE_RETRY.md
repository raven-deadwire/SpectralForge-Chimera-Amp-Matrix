# CAB timing gate — one re-measurement for descheduled callbacks

`ChimeraOriginalCabIntegrationTests` (also run as `ChimeraCabExpansionIntegrationTests`,
`ChimeraCabExtendedTimingTests` and `ChimeraCabLayoutIntegrationTests`) times
three cabinets with six mics for 1600 callbacks per route (sample rate × block
size × channels) and fails when the wall-clock p99 reaches one block period
("CAB-only p99 exceeds one block period (runner CPU gate)"). That verdict is
unchanged. What is new: a route whose failure was the test thread **not
running** measures once more, and the second measurement's verdict stands.

## Why

On 2026-11-13 the gate failed four times on shared `windows-latest` runners
(PRs #308 and #311, in three different CAB tests) and passed each time the job
was re-run, at about three hours per re-run. The test binary is built from the
CAB sources only, and neither PR touched them. The logs' paired thread clock
showed the failing callbacks had done ordinary work:

| Route | Callback | Wall time | Thread cycles |
|---|---|---|---|
| 96 kHz / 256 / mono | slowest | 228,906 µs | 1,085,387 |
| 48 kHz / 64 / stereo | slowest | 88,782 µs | 547,497 |
| 44.1 kHz / 64 / mono | p99 | 3,518 µs | 212,872 |

The median callback of those routes took 60–164 µs. A million cycles is well
under a millisecond of CPU on any current core, so for more than 99% of the
slowest callback's wall time the thread was not running.

## Rule (`cabTiming::classifyMisses`, `Tests/CabTimingDiagnostics.h`)

- A callback's own work is estimated from its thread clock relative to the
  route's median callback: work ÷ median work × median wall time. The clock is
  CPU cycles on Windows (`QueryThreadCycleTime`) and thread CPU time elsewhere.
  Cycles are only divided by cycles of the same run; no cycle rate is assumed.
- A deadline miss is **descheduled** when that estimate fits in half the block
  period.
- A route measures once more only when its p99 failed, **every** miss was
  descheduled, every callback has a valid thread clock, and the median callback
  is itself within half the block period. At most one extra measurement per
  route.
- A miss with heavy work (real CAB cost) fails at once, as before. The
  2026-10-27 failures recorded in `RECALL_CAB_READINESS_20261027.md`, where
  thread CPU p99 equalled wall p99, would not have been re-measured.

## Evidence kept

- Every measurement that fails the p99 gate prints a `GATE_CLASSIFICATION` row:
  misses, how many were descheduled, the largest estimated work, the limit and
  whether it measures again.
- The discarded attempt's rows are printed with a `DISCARDED_ATTEMPT ` prefix
  and a `TIMING_RETRY` row gives the reason (first p99, misses, estimated work,
  limit). Its CSV is saved under `cab-timing-diagnostics/<suite>-discarded/`.
- Each route therefore still has exactly one `TIMING` / `PHASE` /
  `PAIRED_TIMING` row set and one CSV at its usual path, as
  `Tools/run_cab_realtime_ci.py` requires.

## Self-test (every run, every platform)

Before the measurement that counts, the test checks the rule:

- **Classifier on synthetic callbacks:** descheduled misses qualify; heavy,
  mixed, slow-route and clockless misses do not.
- **End to end on one route (48 kHz / 64 / mono), with the platform's thread
  clock:** twenty steady callbacks of the first attempt (every 50th from 200,
  after the warm-up and the automation storm) overrun by three block periods.
  Asleep (no CPU work), the route must measure again; spinning (CPU work), it
  must fail without a retry. A wake-up on a busy runner can itself cost real
  work, so the sleeping pass measures again when at least 18 of its 20 misses
  are descheduled; the strict all-misses rule is covered by the synthetic
  check. Rows are prefixed `GATE_SELF_TEST ` and CSVs go to
  `<suite>-selftest-*`.

Measured on Linux (six runs): sleeping misses were estimated at 105–186 µs of
own work against a 667 µs limit; spinning misses at about 4,000 µs.

The first CI run (PR #313) failed this self-test once on `macos-15`: the
sleeping pass was not measured again, although its misses showed little thread
CPU (91-219 us at the p99 and slowest callbacks, limit 667 us). The log did
not say which miss was counted as work. The likely one: the pass then also
overran callbacks 5 and 65, inside the warm-up and the automation storm, where
a callback's own work is far above the median. The overruns moved to steady
callbacks, the 18-of-20 allowance followed, and the `GATE_CLASSIFICATION` row
now records the classification of every failed measurement.

## Not yet validated

The Windows and macOS thread clocks are exercised only by the self-test in CI,
not locally. Whether the rule removes the flake can only be judged over
further runner executions.
