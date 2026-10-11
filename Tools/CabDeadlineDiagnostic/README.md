# Frozen CAB scheduling experiment controller

This independent executable is diagnostic infrastructure. It does not edit or
replace Chimera's released source, product build, existing CTest acceptance,
p99 thresholds, callback populations, or recorded deadline misses.

Configure with a clean immutable source checkout and its full commit:

```sh
cmake -S Tools/CabDeadlineDiagnostic -B diagnostic-build -DCMAKE_BUILD_TYPE=Release \
  -DCHIMERA_FROZEN_SOURCE=/absolute/path/to/frozen-source \
  -DCHIMERA_FROZEN_SHA=7b20cd9d1f90e3e2969c250712b199e893cf50cd
cmake --build diagnostic-build --config Release \
  --target ChimeraOriginalCabIntegrationTests ChimeraOriginalCabRealtimeTests \
  ChimeraCabSchedulingProbe --parallel 2
```

On macOS, pass the same native architecture and deployment target as the control
build. `FETCHCONTENT_SOURCE_DIR_JUCE` may reuse the exact JUCE 8.0.8 checkout.
The controller adds the frozen product through `add_subdirectory`, so the
original targets and CTest entries keep their dependency settings. Their binary
working directory and ordinary gate CSV root are `diagnostic-build/frozen-product`.
Product CTest remains the acceptance authority. Only selected targets are built.

The wrapper takes `--caller-qos inherited` or `--caller-qos user-interactive` and
forwards the remaining arguments to the original test. Execute each process in
its own output directory: the original CSV paths must never overwrite a previous
condition or replicate. Remove `CHIMERA_LIFECYCLE_TRACE` from the launch environment
as in production CTest; the frozen test still rejects lifecycle tracing.

Use the four original invocations per condition:

1. No engine argument: 12 original-v1 routes.
2. `--expanded`: 12 expanded-v2 routes.
3. `--layouts`: 12 array-v3-6x10 routes.
4. `--expanded --extended-buffers`: 30 expanded-v2 extended routes.

This preserves 66 route entries and 105,600 measured callback intervals per
condition. The unchanged test still reports every miss and returns its own
original assertion/p99 verdict. The wrapper's `user-interactive` mode changes
only the diagnostic caller's requested QoS; the IR worker's explicit low priority
is unchanged. It does not request real-time policy. Failure to establish the
requested QoS is a setup error, never a fallback measurement.

## Separate synthetic scheduler sentinel

Add `--scheduler-sentinel` with the same four route selections to measure 1,600
short deterministic arithmetic intervals per route. There are no CAB instances,
IR workers, audio callbacks or UI instances in this mode. Each interval has 256
bounded scalar arithmetic iterations with paired wall and thread-CPU readings.
It sleeps nominally 1 ms after every eighth interval in the first 1,200 intervals
and does not sleep in the final 400, matching the original test's outside-timer
cadence. Input preparation, DSP work, worker work and their cache/power effects
are absent, so measured exposure is substantially shorter and miss rates are not
directly comparable to the CAB workload.

All synthetic intervals use distinct `scheduler-sentinel-*` suites and
`sentinel-*` engines. Their CSV retains the same paired-clock data fields but
uses `paced_control` and `burst_control` phases, `automation_requested=0`,
`workload=arithmetic_only`, and `worker_running=0`. Synthetic deadlines are
reference values. The sentinel has no acceptance gate and does not modify or
replace CAB results.

## Reproducibility and interpretation

The bounded runner executes the unchanged five-test CTest selection first and
then all four diagnostic conditions, preserving full raw data and return codes.
Use a clean, separate checkout at the frozen SHA. On each native macOS machine,
give every invocation fresh build and output directories:

```sh
python Tools/CabDeadlineDiagnostic/run_experiment.py \
  --source /absolute/path/to/frozen-source \
  --build diagnostic-build-ABBA --output diagnostic-evidence-ABBA --order ABBA
```

On the independently allocated second host, choose `--order BAAB` before
measurement. A means inherited caller QoS; B means user-interactive caller QoS.
The runner does not skip a failed p99 population to reach a passing condition.
It keeps failure logs, all 66 route CSVs per condition, source/dependency/binary
hashes, compiler settings, policy snapshots and exact command receipts. A setup
or incomplete-evidence error remains an error and does not authorize a result.

Build all conditions before measurements. On one runner, use a preregistered
order such as inherited, interactive, interactive, inherited (ABBA); reverse it
on the next independent runner/session. Keep every complete and failed run,
record return codes, source/tree/binary hashes, runner image/architecture,
compiler flags, per-run timestamps, and run order. Do not rerun until green or
discard the first/last run. Preserve CSVs before the next process starts.

The wrapper records caller QoS and Mach time-constraint policy before and after
the workload. These are requested-policy snapshots, not a trace of transient
boosts, CPU migration, preemption, or host/hypervisor behavior. Worker policy
observations remain the original test's own output.

The wall and CPU intervals have different envelopes: the two thread-CPU reads
surround both wall-clock reads. Therefore wall minus CPU is not an exact
off-CPU duration. Keep negative differences and raw values. Nominal clock
resolution is recorded but does not establish accounting accuracy. A guest
thread's CPU-time increase alone cannot identify which DSP function ran or
exclude virtualization/accounting effects. A synthetic interval with a matching
outlier is useful counterevidence to an exclusively CAB explanation; absence of
sentinel outliers is not evidence that external scheduling was absent.

Caller-QoS sensitivity establishes a diagnostic environment effect, not a
product performance fix. The diagnostic QoS also applies to setup and sleeps,
so it is not an isolated change only during each timed interval. The frozen
test executes CAB work on a normal process
thread, lacks a real CoreAudio/DAW deadline and has no editor/message loop. A
real-time-thread wrapper around its complete main would also elevate setup,
allocation, kernel preparation and CSV I/O, with no single valid period for all
routes; this controller deliberately does not do that. Actual DAW and editor
contention require a separate host/device experiment.
