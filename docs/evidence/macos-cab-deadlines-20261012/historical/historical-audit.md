# Historical macOS CAB deadline evidence: 1.3.1-beta.1

This audit concerns immutable published-candidate measurements. It does not certify a subsequent performance fix or alter the released beta. Original archive hashes were checked against freshly retrieved GitHub metadata, then every candidate CSV was independently recomputed.

## Sources

| Candidate | CAB run | macOS artifact | SHA-256 |
|---|---|---|---|
| `7b20cd9d1f90e3e2969c250712b199e893cf50cd` | [38175545597/1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38175545597) | [11715842700](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38175545597/artifacts/11715842700) | `2630e2e9f54dd9f3beed1f93499fbe37a7fb53ec76ffd70a4cbd6045972ca239` |
| `781d03dd49bba51d6cfa7b1c6a0ec8d9e75a0333` | [38173348216/1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38173348216) | [11714524399](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38173348216/artifacts/11714524399) | `9b86ba93f0ccfa9a9983be37df5eb1ae1082ff2dec8028128e27b9e51c7ed631` |

## Recomputed acceptance population

For both candidates, each OS contains 66 populations of 1,600 callbacks (105,600 total) with 66/66 original p99 gates passing. Original p99 uses sorted index 1584; the gate remains `p99_us < 1e6 * block_size / sample_rate`. An individual miss remains `wall_us > budget_us`. No callback is discarded or replaced with thread CPU time.

This is a CAB-only wall-time budget check, not the full plugin CPU envelope or a zero-glitch guarantee. Other DSP stages and callback-release scheduling are not measured. The complete [route table](route-summary.csv) retains all 396 populations, including zeros; the [miss table](deadline-misses.csv) preserves all 128 original miss records across both candidate sources. See the [portable artifact manifest](archives-manifest.json) and [reproduction instructions](README.md).

| macOS metric | Previous 781d03dd | Published 7b20cd9d |
|---|---:|---:|
| Deadline misses | 74 | 54 |
| Affected populations | 26 | 21 |
| Largest wall duration | 27.888 ms | 29.913208 ms |
| Misses with paired CPU below/equal deadline | 48 | 29 |
| Misses with paired CPU above deadline | 26 | 25 |
| Worker/automation phase misses | 72 | 46 |
| Forced publication phase misses | 2 | 8 |

Windows and Linux candidate measurements have zero misses in both runs. Latest macOS breakdown: 44.1 kHz **4**, 48 kHz **5**, 96 kHz **45**; 32-sample **4**, 64-sample **40**, 128-sample **0**, 256-sample **9**, 512-sample **1**. Mono **22**, stereo **32**. Only one of 54 misses occurs on a callback requesting new automation.

## All 21 affected latest populations

Each row contains 1,600 callbacks, three independent CAB lane buffers processed sequentially on the measured caller thread, and two microphones per lane. “Channels” is mono/stereo per lane. `integration` and `extended` are independently executed populations, including overlapping buffer settings. Maximum CPU below is the CPU measurement paired with that exact maximum wall callback; it is not an independently sorted CPU maximum.

| Suite | Engine | Hz | Buffer | Channels | Misses | Max wall us | Paired CPU at max us |
|---|---|---:|---:|---:|---:|---:|---:|
| extended | expanded-v2 | 48000 | 512 | 1 | 1 | 12051.500 | 119.708 |
| extended | expanded-v2 | 96000 | 32 | 1 | 1 | 998.042 | 67.958 |
| extended | expanded-v2 | 96000 | 32 | 2 | 3 | 2047.000 | 2051.166 |
| extended | expanded-v2 | 96000 | 64 | 1 | 2 | 2903.167 | 2907.500 |
| extended | expanded-v2 | 96000 | 64 | 2 | 3 | 22094.833 | 88.166 |
| extended | expanded-v2 | 96000 | 256 | 1 | 2 | 8902.500 | 108.500 |
| extended | expanded-v2 | 96000 | 256 | 2 | 1 | 2910.000 | 2913.500 |
| integration | array-v3-6x10 | 96000 | 64 | 1 | 4 | 26398.042 | 69.250 |
| integration | array-v3-6x10 | 96000 | 64 | 2 | 2 | 865.584 | 53.250 |
| integration | array-v3-6x10 | 96000 | 256 | 1 | 2 | 5086.375 | 87.833 |
| integration | array-v3-6x10 | 96000 | 256 | 2 | 2 | 11928.500 | 146.000 |
| integration | expanded-v2 | 44100 | 64 | 2 | 4 | 15950.833 | 90.084 |
| integration | expanded-v2 | 48000 | 64 | 1 | 1 | 2855.875 | 45.792 |
| integration | expanded-v2 | 48000 | 256 | 1 | 1 | 29913.208 | 102.250 |
| integration | expanded-v2 | 96000 | 64 | 1 | 2 | 3893.250 | 3894.792 |
| integration | expanded-v2 | 96000 | 64 | 2 | 6 | 2356.583 | 2363.292 |
| integration | original-v1 | 48000 | 64 | 1 | 1 | 1488.708 | 78.042 |
| integration | original-v1 | 48000 | 64 | 2 | 1 | 1698.333 | 50.959 |
| integration | original-v1 | 96000 | 64 | 1 | 4 | 4233.875 | 4237.625 |
| integration | original-v1 | 96000 | 64 | 2 | 10 | 4491.916 | 129.375 |
| integration | original-v1 | 96000 | 256 | 1 | 1 | 4726.750 | 4732.375 |

## Configuration and execution scope

The [source-bound harness](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/7b20cd9d1f90e3e2969c250712b199e893cf50cd/Tests/OriginalCabIntegrationTests.cpp) defines these modeled requests:

| Engine | Cabinet/driver | Mic A / Mic B | Unit targets |
|---|---|---|---|
| `original-v1` | Authored bass four-driver/10-inch family | Attack dynamic / body ribbon | 0 / 1 |
| `expanded-v2` | Bass four-driver family; driver ordinal 12 = Alloy 10 | Ordinal 20 = Chimera Strike / ordinal 10 = Silk 121 | 0 / 1 |
| `array-v3-6x10` | Layout ordinal 7 = Bass 6x10; driver ordinal 9 = Foundry 10 | Chimera Strike / Silk 121 | 0 / requested 7 mapped to effective 5 |

All are closed-rear models with tweeter level 0.3 and mic A/B blend 0.5; expanded/array tweeter type is 2. The Mic A settings object begins at position 0.25, distance 10 cm; B at position 0.25, distance 18 cm. For callbacks 0–79, all lanes request A position `(n % 60)/60` and B distance `5 + 0.2*n` cm. Key quantization applies. The first phase contains 1,200 callbacks while the IR worker runs; `sleep(1 ms)` occurs every eight callbacks outside the measured region. Three lane inputs remain separate and their CAB calls execute sequentially on the caller thread.

After convergence and fade drain, the worker is stopped. The harness sets `a.position = .85`, derives one `nextKey = modelKey(a)` from Mic A's settings, and generates one response for that key. It copies that response into six separately prepared kernel objects and publishes the **same unchanged `nextKey` to all six slots**, including every Mic B slot. The final 400 callbacks measure those publications, the serialized transitions, and the subsequent tail; the final assertion requires all six active models to converge to that same key.

The table's A/B configurations therefore describe initial/requested models and transition origins, not six distinct steady configurations throughout the forced-publication phase. Its common final target uses Mic A's unit, microphone, distance, and position 0.85. The initial functional checks also change the first lane's second-mic request for a unity check before the measured automation requests begin; requested settings alone do not establish every slot's active state at the first measured callback.

The 66 populations comprise three integration engines × three rates × two buffers (64/256) × two channel counts = 36, plus expanded-v2 × three rates × five buffers (32/64/128/256/512) × two channel counts = 30. No Classic/Dual/Matrix processor mode is selected: this is a CAB-only console executable. It does not run the full `ChimeraProcessor::processBlock`, GUI painting, real DAW or CoreAudio callback scheduling. Normal production mic cut-filter refresh is included only in the separate profiler.

CSV fields identify phase and requested automation, but do not record actual active/pending kernel keys, generations or transition counters per callback. Exact active modeled state at an individual miss cannot be reconstructed beyond the requested configuration and phase.

## What the paired clocks support

The largest latest miss is expanded-v2/integration, 48 kHz, 256 samples, mono, callback **1030**: **29,913.208 us wall**, **102.25 us paired thread CPU**, **5,333.333 us budget**, with no new automation requested. Its wall/CPU gap supports a substantial waiting or non-running interval; without scheduler traces it does not identify the responsible OS or application event. All 29 latest below-budget CPU pairs are at most 10% of their wall duration.

The other 25 misses also exceed deadline in paired thread CPU. Five of these occur in the forced publication phase after the IR worker has stopped. Worker contention and non-running intervals cannot be treated as a complete explanation. CPU clocks surround the wall clock reads and add overhead; kernel activity, virtualization and other causes remain to be separated. No per-callback stage trace proves a convolution, allocation or retire-queue root cause.

The C++ callback watcher records **new=0/delete=0**. Its scope is thread-local C++ operators; it does not trace C malloc/free, Objective-C runtime activity, page faults, mutex waits or system framework allocations. Existing misses occur without Chimera GUI painting; actual DAW/UI interference is an additional unmeasured condition.

Separate diagnostic profiler populations retain extra stage clocks and mic-filter refresh. Their results are not mixed into the 66 acceptance populations: latest **9** misses/max **8.182125 ms**, previous **23**/max **58.278167 ms**. The original fixed baseline has only 36 integration populations (57 misses/max 18.5696 ms in the prior run, 26/max 53.431 ms in the latest); it is not the previous release candidate.

## Environment comparison and causal limit

Both macOS jobs use GitHub-hosted Apple M1 virtual machines (VirtualMac2,1), **3 cores/7 GB**, ARM64; macOS **15.7.9/24G830**, Darwin 24.6.0; runner 2.337.0; image **macos-15-arm64 20260907.0337.1**; CMake/CTest 4.4.3; AppleClang 17.0.0.17000013; Release, arm64, deployment target 12.0. Build parallelism is 2, CTest 1/RUN_SERIAL. Lifecycle tracing is disabled. Both use fixed baseline `aaa1330dd5cf7d51faf1c45a7cef633b9b118458`, the same configure/build/test commands and baseline→candidate→diagnostic order. Caller base QoS value **17** and worker requested/observed JUCE low priority **−1** are reported in both.

Prior was `workflow_dispatch` at 20:42:41 UTC; latest was `pull_request` at 21:14:34 UTC on 2026-10-11. VM hostname, runner ID and worker ID differ. Host load, scheduling pressure, physical host equivalence, thermal/power state and CPU residency are unrecorded. There was no randomized/crossover comparison, and fresh binary hashes differ in both candidate and unchanged baseline.

The [four-file source diff](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/compare/781d03dd49bba51d6cfa7b1c6a0ec8d9e75a0333...7b20cd9d1f90e3e2969c250712b199e893cf50cd) contains opaque UI flags, Windows measured-child priority handling, and its Python tests. No CAB DSP or C++ timing-harness change exists, and POSIX child launch is unchanged. The UI components are not instantiated by this measurement. **74→54 does not establish a macOS performance improvement caused by those commits.**

## Reproducible re-analysis

Use `analyze_archives.py --archives <source-bound manifest> --output <new directory>`. The analyzer verifies ZIP SHA-256, source/run binding when supplied, manifest/receipt equality, CSV digests and lengths, the complete distinct 66-key grid, exactly 1,600 indexed samples, original phase/automation assignment, finite budgets/timings, original miss flags, and platform-specific clock validity/deltas and cumulative clock progression. It retains all zero-miss populations and every field of every miss, and exits nonzero on invalid evidence. Input ZIPs and manifests are never rewritten.

All measurements above are historical facts only. New performance claims require independent same-binary controls, preserved failure records, unchanged p99/deadline criteria, and an intervention that repeatedly improves the relevant condition.
