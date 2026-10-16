# Tuner worker timing attribution

Development-only follow-up to PR #71, source
`f22b8cd48c0254518df79e070bdeb524ae97f2e7`.

`CHIMERA_TUNER_TEST_HOOKS` enables a bounded, worker-owned observation buffer.
The existing integration target already defines this macro. Product targets do
not: no timing storage, extra clocks, allocation, locks, notification, callback
work, polling interval or priority change is added to them. The estimator, FIFO,
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

Acquisition/settling in stdout remain the feeder's observed readings, including
its polling cadence. They must not be equated with estimator compute or publish
age. The existing >250 ms input-age invalidation still uses the original input
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
