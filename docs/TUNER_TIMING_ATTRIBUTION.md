# Tuner worker timing attribution

Development-only follow-up to PR #71, source
`f22b8cd48c0254518df79e070bdeb524ae97f2e7`.

`CHIMERA_TUNER_TEST_HOOKS` enables a bounded, worker-owned observation buffer.
The existing integration target already defines this macro. Product targets do
not: no timing storage, extra clocks, allocation, locks, notification, callback
work or polling interval change is added by attribution. The macOS active-worker
QoS correction below is a separate production change. The estimator, FIFO,
generation transitions and freshness policy are unchanged.

The integration test writes tagged CSV records to stderr while preserving its
original results CSV on stdout. `TIMING_HEADER` names the columns; each `TIMING`
row identifies rate, block size, nominal tone, attempt index and generation.
`TIMING_END` reports the route identity, retained row count and dropped count.
All 32 candidate routes retain every estimate attempt, including rejected pitches
and results discarded by the existing generation/enabled publication guard.
Missing observations or buffer overflow fail the test. Failed acquisition,
settling and accuracy routes still emit their observations before the final gate.

All clock values are milliseconds on JUCE's monotonic clock. Absolute event
columns are relative to the route's input-start epoch:

* `window_complete_ms`: existing input timestamp of the final callback block in
  the **selected** 4096-sample decimated window, read before releasing FIFO slots.
  It is block-arrival completion, not an ADC timestamp or the earliest possible
  complete window in a backlog. The worker selects the newest complete window.
* `wake_ms`: worker iteration service entry, after any deliberate test pause.
  `wake_delay_ms = max(0, wake - window_complete)` includes polling and OS delay;
  it cannot distinguish those causes without an OS scheduler trace.
* `service_ms`: after the FIFO snapshot is consumed. `service_delay_ms` is the
  interval from `max(wake, window_complete)` to this point. If input arrives
  during an iteration, this avoids a negative wake interval or double counting.
* `preparation_ms`: copying the ordered analysis window before estimation.
* `compute_ms`: wall time around the estimator call, including any preemption.
  This is not thread CPU time.
* `publication_ms`: estimator return to completion of the existing publication
  guard and stores. `published` means those stores occurred, not that the pitch
  was accepted or remains fresh when subsequently read. For discarded attempts,
  `publish_ms` marks the completed publication decision.
* `publish_age_ms`: publication/decision time minus window completion. It equals
  wake delay + service delay + preparation + compute + publication.

Acquisition/settling in stdout are the feeder's observed readings, including
its polling cadence, expressed in audio time (see "Route gate timing basis"
below). They must not be equated with estimator compute or publish age. The existing >250 ms input-age invalidation still uses the original input
timestamp; timing collection never refreshes it. Logs contain synthetic input
only and make no driver, DAW or realtime qualification claim.

## Focused checks

`ChimeraTunerTimingTests` checks decomposition including wake/input overlap,
bounded retention, overflow accounting and reset. `ChimeraTunerWorkerTimingTests`
runs the real worker with `--timing-only`: incomplete window rejection, complete
first-activation window retention through a deliberate pause, input-origin
timestamps, separate delay/compute attribution and re-prepare reset. Both are
registered with CTest. The full existing integration test also runs the worker
timing checks after its unchanged channel, lifecycle and stalled-input checks.

```sh
ctest --test-dir build -C Release --output-on-failure -R 'Chimera(Tuner.*TimingTests|PrecisionTunerTests|PrecisionTunerIntegrationTests)$'
# Or retain both full raw streams directly:
ChimeraPrecisionTunerIntegrationTests > routes.csv 2> attribution.log
```

The <290 ms acquisition/settling, <0.1 cent route accuracy, stale invalidation,
first-activation/channel-change generation semantics and shutdown requirements
are unchanged. Prior macOS failures remain unresolved until exact-source native
verification. No merge, push, release, tag, installer or publication is part of
this change.

## macOS active-worker service correction

At the instrumented source `452489320930bfe4aa19399f84b163c21f8a9d80`, the
macOS focused run 38868065214 passed 32 routes but recorded 425 estimates.
The interval between one publication and the next serviced window had median
27.450875 ms, p95 52.074542 ms and maximum 84.774792 ms despite a requested
10 ms wait. The local Windows population had median 10.631900 ms. These are
observations, not an OS scheduler trace proving a unique cause.

The correction requests JUCE `Priority::highest` only while tuning is enabled on
macOS. JUCE 8.0.8 maps this to `QOS_CLASS_USER_INTERACTIVE`, below realtime audio;
disabling tuning restores `Priority::normal` / `QOS_CLASS_DEFAULT`. Both changes
occur on the worker itself. The callback, 10 ms wait interval, FIFO and estimator
remain unchanged. Windows/Linux retain normal priority. If the platform refuses
the request, processing continues; the macOS focused test checks the actual
worker priority and will fail rather than certify an unapplied request.

The same binary supports `--normal-qos-reference` for the original normal-QoS
32-route control. CI retains its complete output and exit code even on timing
failure, then executes the mandatory candidate tests with unchanged gates. The
control is diagnostic and cannot convert a failing candidate into PASS. Active
QoS application and restoration on disable are covered by `--timing-only`.
The first USER_INITIATED candidate (`4eef53d`) still failed one macOS route:
48 kHz/256/27.5 Hz acquired at 293.274 ms. Its first pitch was published at
282.002 ms and observed at 293.274 ms; the feeder also delivered the first full
window late (195.241 ms versus its nominal 165.333 ms). This failed run is retained.
The revised active policy uses USER_INTERACTIVE, which JUCE maps from `highest`,
for the continuous user-facing tuner update. It is still a non-realtime worker;
the test feeder and every acceptance threshold remain unchanged.

New focused and Product results are required before calling this correction
verified. No realtime hardware or release acceptance is inferred from it.

## Route gate timing basis (audio time, feeder lateness judged separately)

PR #143 (`c15811a`, which changed only `OBS/`, `Tools/obs_testkit.py` and docs,
none of them built by the root CMake project) failed the macOS route gate on
two attempts, while PR #141 passed on the same base. The feeder rows in those
logs show the runner waking the synthetic feeder late: `sleep_until` overshoot
of up to about 14 ms. The worker trace shows the same wake slack. The older
macOS failures retained under `evidence/preset-dynamics-eq-20261019/` show
whole-process stalls: feeder sleeps of 146, 189 and 374 ms. Across the 264
retained macOS routes, per-route maximum feeder lateness has median 15.8 ms,
p95 47.2 ms and p99 89.3 ms. The <290 ms gates measured wall time from feeder
start. Any late feeder wake on the deciding read went straight into the tuner's
acquisition time. For example, 48 kHz/256/30.867706 Hz failed at 322.3 ms wall
time, but its deciding read was taken 146 ms late, after only 176.0 ms of audio
had been fed.

The gate now uses this basis:

* **Audio time.** `acquire` and `settle` are the input duration fed,
  `1000 * (offset + block) / rate`, at the first accurate, confident read
  (acquire), or at the third consecutive one after the +10 ct step, minus the
  step position (settle). That duration is the read's scheduled deadline. The
  thresholds are unchanged: `0 < acquire < 290`, `0 < settle < 290` and final
  `|error| < 0.1 ct`. The tuner's own delays (worker wake, the 10 ms poll,
  window copy, compute and publication) elapse while the feeder keeps feeding
  in real time, so they still add to the audio-time result.
* **Feeder lateness, judged separately.** For every read, lateness is the
  wall-clock read time minus that read's deadline. The `ROUTE_TIMING` rows
  record wall and audio results, lateness at the deciding reads, and the
  route's maximum and p99 lateness. If any read in an attempt is more than
  50 ms late, the attempt is not a valid real-time measurement. It is logged
  as `FEEDER_LATE` and the A/B route pair is repeated, up to three attempts.
  A feeder-valid attempt is final: a tuner gate failure is never repeated.
  If every attempt is feeder-invalid, the test fails with "Feeder lateness
  invalidated every attempt of a route". That is reported as a runner timing
  failure, distinct from the tuner gate message.
* **Residual credit.** At the deciding read the tuner may have had up to that
  read's lateness, at most 50 ms and typically under 15 ms, of extra wall time
  beyond the audio fed. A regression can therefore pass only if it lands within
  that margin below 290 ms on a run whose deciding read was also late. Repeated
  CTest runs and the 32 routes make this visible in `ROUTE_TIMING`.
* **Frozen baseline.** The frozen 2026-10-13 tuner is still measured and
  recorded in the same run on the same audio-time basis. It is not used as a
  relative gate. It runs at a different time from the candidate, so it sees
  different runner stalls, and it is a different estimator (2048 samples at
  12 kHz) with its own settling behavior. A candidate-minus-baseline gate would
  add the noise of two measurements without removing either.

The compact `ROUTE_TIMING*`, `FEEDER_LATE`, `ROUTE FAIL` and
`ROUTE FEEDER INVALID` lines are written after the deferred FEED/TIMING streams,
so they remain within the job-log tail that the API returns. Exactly one
`worker,new,` row per route is still written, from the judged attempt.

Local Linux checks (temporary, uncommitted patches; not native macOS evidence):

| Patch | Old wall gate | Audio-time gate |
|---|---|---|
| none | pass (max acquire 197.5 ms) | pass (197.4 ms) |
| feeder wake 0–13 ms late on every sleep | pass (211.1 ms) | pass (203.2 ms) |
| 110 ms feeder stall at the acquisition read, 50% of attempts | 8 attempts fail (up to 308.4 ms) | all valid attempts pass (≤197.4 ms); 20 attempts repeated; 4 routes stalled on all 3 attempts → feeder-invalid failure |
| tuner sleeps 100 ms before each publication | — | 32/32 routes fail (acquire ≈ 295 ms, settle −1) |
| tuner publishes only after 2× its window of input | — | 32/32 routes fail (acquire −1) |

Native macOS CI results on the changed source are still required.
