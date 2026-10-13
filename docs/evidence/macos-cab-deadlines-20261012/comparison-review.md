# Scheduling controller: comparison-validity review

## Review conclusion

The controller supports a bounded comparison of **whole console-test caller QoS contexts**. No additional blocker was found that requires stopping the prespecified diagnostic runs. It does not isolate a product-code fix or pure audio-callback preemption. The original acceptance target and every recorded deadline miss remain separate evidence.

This review covers controller source and frozen source `7b20cd9d1f90e3e2969c250712b199e893cf50cd`. The protocol assessment is independent of outcome; the appended native verification identifies the specific completed run inspected and does not assume other runs' results.

## Baseline and probe parity

The controller imports the [frozen CMake project](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/7b20cd9d1f90e3e2969c250712b199e893cf50cd/CMakeLists.txt) and runs its original five selected CTests first. The [integration-test body](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/7b20cd9d1f90e3e2969c250712b199e893cf50cd/Tests/OriginalCabIntegrationTests.cpp) is also included verbatim in a separately built probe, with `main` renamed. A and B invoke the same probe executable in fresh child processes. A never sets QoS; B verifies both the setter result and the observed requested class before proceeding.

The probe uses the same IR library source, JUCE libraries, C++ standard and recommended configuration flags as the original integration target. Release configuration, native architecture and macOS deployment target follow the historical launcher. Stage profiling is prohibited. Probe-only differences include its wrapper entry point, extra diagnostic headers/definitions, executable identity and diagnostic code. Those can change binary layout or compiler decisions, so the probe must not be described as the exact deployed binary. They are common to A and B.

Native `compile_commands.json`, compiler metadata and binary hashes should be used to confirm the actual target commands before claiming full compilation parity. Historical evidence retains CMake cache/compiler metadata; it does not necessarily contain every original translation unit's command.

### Suite order is not identical to the ordinary gate

| Invocation | Timing-suite order |
|---|---|
| Frozen CTest registration | Original → Expanded → Extended buffers → Array layouts |
| Diagnostic probe controller | Original → Expanded → Array layouts → Extended buffers |

The ordinary gate also runs the independent realtime-response oracle between original and expanded integration. The diagnostic probe does not run that oracle between its timing suites. All diagnostic A/B arms use the same controller order, so this does not create an A-versus-B source difference. It does mean that ordinary-gate versus inherited-probe results are not an identical-order A/A comparison. Preparation, elapsed exposure and preceding workload differ. Keep those populations labeled separately.

## What the QoS intervention actually changes

`requestQos()` executes before `frozenCabMain()`. Consequently, B changes the caller's context during response preparation, warmup, reference checks, model requests, measurement, sleeps and teardown. It does not switch QoS only around the timed CAB calls, and it does not establish a CoreAudio realtime scheduling policy.

The [IR worker](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/7b20cd9d1f90e3e2969c250712b199e893cf50cd/Source/IRLibrary.cpp) retains its requested low priority. However, the first 1,200 callbacks contain asynchronous preparation and publication. A different caller context can change when requests are cancelled, when kernels become pending, and which callbacks execute fading convolutions. The protocol is unchanged, but the exact amount of DSP work at a given callback index need not match across A and B. The existing CSV does not record per-callback active/fading kernel counts.

Therefore a B-minus-A difference is a **total test-context effect**, potentially including dispatch, processor selection/frequency, setup history, worker progress and convolution-overlap timing. It cannot be attributed exclusively to less callback preemption.

The forced 400-callback phase is the stronger within-protocol comparison: the worker has stopped, six separate kernels are prebuilt, publication occurs at the prescribed boundary, and microphone fades are serialized by the same code. Those six kernels contain copies of one common response. The [frozen test](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/7b20cd9d1f90e3e2969c250712b199e893cf50cd/Tests/OriginalCabIntegrationTests.cpp#L168-L201) sets `a.position=.85`, derives `nextKey` from microphone A, and publishes that same key/wave to every A and B slot. Previous A/B unit, microphone and distance distinctions participate during the outgoing/queued transitions; the completed forced swaps leave all six paths using the same replacement model response. This does not imply identical per-slot history or fade ordering across repetitions. Analyze that phase separately. It still runs after context-dependent setup and does not remove runner, cache, fault or measurement effects.

## Sentinel cadence and exposure

The sentinel and CAB test request the same sleep operation at the same index residue. In pinned JUCE, [Thread::sleep delegates directly to `std::this_thread::sleep_for(milliseconds)`](https://github.com/juce-framework/JUCE/blob/d6181bde38d858c283c3b7bf699ce6340c050b5d/modules/juce_core/native/juce_SharedCode_posix.h#L56-L59), which the sentinel calls itself. A different sleep API is therefore not an established confound.

The sentinel is nevertheless not a matched CAB workload:

- Each measured interval performs the same 256 scalar arithmetic iterations. Rate, buffer and channel labels only select deadline references.
- It contains no CAB instances, worker, model automation, convolution, GUI or audio-device callback.
- Its measured intervals are much shorter than CAB's, and it omits CAB preparation and warmup. Retaining 1,600 intervals per label does not match execution-time exposure.
- It runs after its arm's CAB suites, not simultaneously with the CAB intervals whose anomalies it contextualizes.

A sentinel miss shows that an anomaly can occur without CAB. A clean sentinel does not establish that a CAB anomaly is caused by CAB DSP. Do not subtract sentinel time, equate miss probabilities across these different exposures, or interpret its channel/layout labels as actual rendered channels/cabinets. Synthetic `paced_control` and `burst_control` records must remain separate from audio results.

## Interpretation constraints for the completed experiments

ABBA and BAAB counterbalance ordering, but they do not eliminate nonlinear load changes or differences between runner allocations. Two repetitions per condition in a run are the experimental repetitions; the many callbacks within a population are not independent runner-level trials. Report individual repetitions, matched configuration/phase results and actual observed QoS before considering aggregates.

Paired CPU clocks retain their original wider envelope around the wall interval. They do not provide exact DSP CPU time, an exclusive wall-minus-CPU scheduler duration, or a trace of host/guest scheduling. Context snapshots are also not continuous records of temporary priority changes.

If results are mixed, preserve the mixed result. Additional attribution work should separate worker-present and forced-publication phases, record actual fade/reset state, and use an independent scheduling/fault or stage-timing observation. No product scheduling change, relaxed p99 threshold, changed block size, shortened IR or omitted miss is justified by this controller alone.

## Native verification: ABBA run 38185786916

The artifact from [run 38185786916](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38185786916) was read directly from `diagnostic-38185786916-ABBA.zip`. The checks below use `build-metadata/compile_commands.json`, `CMakeCXXCompiler.cmake`, `CMakeCache.txt`, `report.json` and the identified raw CSVs. They compare the unchanged original integration target with the diagnostic probe built in this run; they do not establish byte identity with the published release binaries.

### Actual compilation parity

Both `ChimeraOriginalCabIntegrationTests` and `ChimeraCabSchedulingProbe` have nine translation units: their respective entry point, the same frozen `IRLibrary.cpp`, and the same seven JUCE module compilation units. Comparing all nine corresponding commands found identical compiler and non-path flags after excluding the two probe provenance definitions and expected source, output and include paths.

| Property | Both native targets |
|---|---|
| Compiler | AppleClang `17.0.0.17000013`, invoked as `/usr/bin/c++` |
| Build and optimization | Release, `-O3`, `-DNDEBUG`; recommended configuration also supplies `-O3` |
| Architecture | `-arch arm64` |
| Minimum macOS version | `-mmacosx-version-min=12.0` |
| C++ source standard | `-std=gnu++20` |
| JUCE Objective-C++ standard | `-x objective-c++ -std=gnu++2a` |
| Profile/sanitizer command flags | None present |

`IRLibrary.cpp` has the same product/JUCE definitions in both targets, including `JUCE_USE_CURL=0`, `JUCE_WEB_BROWSER=0`, standalone/module settings and release assertions settings. The probe adds only `CHIMERA_FROZEN_SHA` and `FROZEN_CAB_TEST_FILE` definitions. No stage-profile definition is present in either target. The shared frozen IR source, binary-data include and pinned JUCE module paths agree. Expected include differences are the target-specific generated JUCE directory, the original target's generated-version directory, and the probe's explicit `Tests` directory. The earlier wrapper/layout caveat remains despite flag parity.

The report records `complete=true`, all five unchanged ordinary CTests passing, and the pinned JUCE SHA `d6181bde38d858c283c3b7bf699ce6340c050b5d`. All 32 probe child processes have the requested observed class both before and after their workloads: A is 17; B is 33. These snapshots verify the intended intervention at the observation points, not continuous dispatch behavior.

### Four raw sentinel anomalies

All four entries below were matched field-for-field against the corresponding raw CSV row. Every row reports `arithmetic_only`, `worker_running=0`, a valid CPU clock and zero clock-start/end errors. Values are in microseconds; rate/block/channel combinations are sentinel labels, not audio rendering.

| Trial | Label: rate / block / channels | Interval and phase | Wall | Paired CPU | Deadline reference |
|---|---|---|---:|---:|---:|
| 01-A | 44,100 / 32 / 1, expanded | 1180, paced | 767.375 | 69.917 | 725.624 |
| 01-A | 44,100 / 32 / 1, expanded | 1212, burst | 4,992.000 | 13.375 | 725.624 |
| 02-B | 96,000 / 64 / 1, expanded | 480, paced | 2,662.458 | 8.792 | 666.667 |
| 04-A | 96,000 / 64 / 1, array label | 181, paced | 816.459 | 820.417 | 666.667 |

Raw locations under each `trials/<trial>/sentinel/cab-timing-diagnostics/` directory are `scheduler-sentinel-extended/sentinel-expanded-v2/44100-32-1.csv` for both 01-A entries, `scheduler-sentinel-integration/sentinel-expanded-v2/96000-64-1.csv` for 02-B, and `scheduler-sentinel-integration/sentinel-array-v3-6x10/96000-64-1.csv` for 04-A.

The first three paired CPU/wall ratios are approximately 9.11%, 0.268% and 0.330%. The last paired CPU observation also exceeds the reference deadline; its immediately preceding and following intervals each have 2.750 microseconds wall time. Thus an observation with both wall and paired CPU above the reference can occur in this arithmetic-only control. It does not identify a CAB computational defect, establish that all CAB anomalies have the same cause, or remove the CPU-clock envelope limitation described above.

The run reports CAB miss counts of 17, 7, 6 and 1 in A/B/B/A order, while sentinel counts are 2, 1, 0 and 1. The final inherited A has fewer CAB misses than either B repetition. A pooled reduction alone therefore does not demonstrate a reproducible QoS benefit. Preserve individual repetitions and the independent repeat before making a product-performance claim.

## Native verification: BAAB run 38186063935

The independently allocated [BAAB run 38186063935](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38186063935) was checked directly in `diagnostic-38186063935-BAAB.zip`, without extraction. Its SHA-256 matches `e35401f002edb8bb7bbe6fbd91b68534e22137ea456c66cf710b337b905b64cd`.

All 18 relevant compilation command strings—nine original integration units and nine probe units—are **exactly identical between ABBA and BAAB**, including their paths. The previously established within-run flag parity therefore also holds here: AppleClang `17.0.0.17000013`, Release `-O3`, arm64, minimum macOS 12.0, and no profile/sanitizer flags. BAAB additionally archives `CMakeOBJCXXCompiler.cmake`, which independently records the same AppleClang version.

Both runs record the same clean frozen product SHA `7b20cd9d1f90e3e2969c250712b199e893cf50cd` and tree before and after execution, and the same pinned JUCE SHA `d6181bde38d858c283c3b7bf699ce6340c050b5d`. Controller `CabSchedulingProbe.cpp` and `CMakeLists.txt` fingerprints agree across runs; `run_experiment.py` and README fingerprints differ for the repeat's order-capable controller. The report has `complete=true`, all five unchanged ordinary tests passing, and all 32 child processes recording the intended before/after caller QoS: A=17, B=33.

The recorded runner node changes from the ABBA `iad20-gt1018-...` node to BAAB `sjc22-be112-...`. Both report GitHub-hosted macos15 image `20260907.0337.1`, Darwin `24.6.0`, arm64 Apple M1 (Virtual), three cores and 7 GB memory. Recorded Git, CMake/CTest and Python versions also agree. Apart from node identity, observation times and run ID, these captured environment fields are unchanged. They do not describe unrecorded host contention, CPU dispatch or faults.

These are separate native builds. The probe fingerprint changes from ABBA `7f89446dbd0dd48c84a9134b7c58ddff27ea80d21fe6c307376d1ecbcf8cf291` to BAAB `29b607d216baec5dc1d31b4ef6472eff3d838c7a2a8bc25dba7d5da8e82a43ca`. Both ordinary test executable fingerprints also differ and remain recorded in each `report.json`. Consequently, the cross-run evidence establishes source and compiler-command parity, not binary identity. Each run's A/B arms use that run's common probe executable.

### BAAB sentinel raw check

BAAB has one sentinel wall miss, in trial `02-A`, expanded label 96,000 Hz / 128 samples / one channel, paced interval 787. The raw row records **1,519.167 microseconds wall, 28.084 microseconds paired CPU, and a 1,333.333 microseconds reference deadline**, with valid CPU and zero clock errors. It was matched against `trials/02-A/sentinel/cab-timing-diagnostics/scheduler-sentinel-extended/sentinel-expanded-v2/96000-128-1.csv`; its immediately adjacent wall measurements are 1.333 and 0.500 microseconds.

A direct scan of all 422,400 BAAB sentinel rows finds no valid paired CPU value above its reference deadline. Thus there is no additional CPU-over sentinel example in this repeat; the earlier ABBA example remains recorded. This preserves the result without assigning every CAB anomaly to one cause. Full configuration/phase comparisons belong to the accompanying measurement analysis; these native checks do not establish a product fix.
