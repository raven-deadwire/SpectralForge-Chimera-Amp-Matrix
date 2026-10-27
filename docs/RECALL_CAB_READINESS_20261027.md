# Recall CAB readiness — 2026-10-27 KST

Status: development candidate on PR #194 (`9d60312e13dbc0ef21a2db60f7041cce3afad791`), stacked on #188.
Final source: `d7abb79c97ffd02fe4690eb2793a5473f9bddae9`. This is not release, listening, OBS, or DAW approval.

## Transition structure and confirmed defects

The stack is #184 → #187 → #188. #184 measures the real processor against held-old
and held-destination renders. #187 replaces repeated linear parameter-tree lookup
with an index and introduces a 5 ms output fade-out, state-application hold, reset,
latency fill, and 5 ms fade-in. Its message thread waits up to 30 ms for the fade;
the audio callback never waits on the message thread. #188 adds 20 ms settling and
waits for audible cabinet IR installation plus the existing 50 ms cabinet fade.

PR #194 was also found during the audit. It retains the same deadline defect while adding instant swaps behind the mute, a second 20 ms settle for late kernels, parallel response generation and corrected pacing. This candidate retains its instant-swap/late-settle behavior and replaces its helper fan-out with the bounded policies below.

The #188/#194 deadline is **fail-open**: when `switchCabinetBudget` reaches zero,
`applySwitchFade()` starts fading in even if `audibleCabinetsSettled()` is false.
The previous CAB can therefore be heard. Also, readiness and pending-kernel
acceptance do not check user-IR generation, and pending acceptance checks the
model key but not captured source. These are correctness defects independent of
any performance result.

The #184/#188 probe has two measurement defects (the first was already fixed in #194):

* Pacing stops when the UI action completes, even though the IR worker continues
  in wall time. The remaining audio horizon then runs faster/slower than real
  time depending on CPU load. Previously reported gaps are not comparable with
  measurements that pace the entire recall.
* The destination preset can turn time effects back on during the switched
  render; the held reference disables them after setup. The new probe disables
  those effects immediately after both paths apply their destination.

## Candidate behavior

* The 500 ms CAB budget is a diagnostic deadline. An incomplete or failed target
  stays muted, with the graph still processing. The existing #194 instant-swap path remains behind the mute, and late kernels get 20 ms to settle. A late exact target automatically
  resumes through the existing fade. A host tail reset cannot bypass this wait. State application has its own fail-closed 500 ms diagnostic; a delayed commit cannot resume old/partial audio. A recall after prepare but before the first callback also enters the readiness guard; states loaded before prepare are installed synchronously.
* Readiness includes source, modeled identity, and user-file generation. Stale
  pending kernels are retired to the worker, never destroyed in the callback.
  An unavailable nonzero source is not accepted as a prepared IR.
* Readiness must hold before and after processing a chunk. Completion at the end
  of a block cannot unmute earlier samples rendered through the previous CAB.
* The worker prioritizes audible microphones. Muted/solo-excluded lanes, lanes
  outside the routing mode, zero-weight Dual paths, a Matrix LOW path at zero amp
  mix, and a fully unused microphone do not hold up audible recovery. Blend ramps
  are included conservatively.
* The optional kernel-reuse experiment retains up to two retired **modeled** kernels per
  microphone (12 maximum). A/B revisits match content identity, clear private
  history on the worker, and publish the already allocated kernel. There is no
  sharing of convolution histories. User IR files are not recycled this way.
* The default audible-first worker expands the existing immutable response/spectrum cache from eight to 12 entries, enough for two
  six-microphone states. Prepare clears it across sample-rate/block/channel
  changes; stop/release clears every recycled kernel. No new cache operation,
  mutex, allocation, FFT construction, thread creation, or deletion runs on audio.
* At most two simultaneous worker preparations are available only as a benchmark policy.
  The final default uses the audible-first single worker and 12-entry immutable response cache; complete-kernel reuse is opt-in for experiments. The experimental path joins background
  work and protects only the shared response cache; it never joins on audio.

## Measurement protocol

Local benchmark targets use `CHIMERA_HEADLESS_RECALL_BENCHMARKS=ON`: editor construction and graphical artwork are omitted; processor/DSP, state serialization, IR preparation and JUCE audio processing are real. This is not a full product/UI build.

48 kHz, stereo, host blocks 64/128/256, fixed synthetic Drop A fixture, 0.6 s
preroll and 1 s post-switch horizon. Both preset recalls and A/B recalls are run;
the lane-3-solo A/B case exposes queue ordering. Cold and warm A/B are separate.
Time effects are disabled consistently. The complete post-switch interval is
paced in real time; comparisons run sequentially, never simultaneously.

| Metric | Meaning |
|---|---|
| `response_ms` | First detectable departure from old audio; muting counts. The existing 60 ms recall gate remains this metric only. |
| `action_ms` | Time taken by the UI state-recall function, including its fade handshake. |
| `audible_ms` | First signal above -60 dB relative to destination average, after the recall actually entered silence. |
| `recovery_ms` | First of five consecutive 1 ms windows within ±3 dB of destination energy after silence. This is level recovery, not timbre convergence. |
| `response_ready_wall_ms`, `audible_ready_wall_ms`, `recovery_ready_wall_ms` | Elapsed wall time until the callback containing the corresponding event returns. This exposes callback backlog; it is not a physical speaker/device measurement. |
| `gap_ms` | Longest continuous below-threshold interval while the destination reference is audible. |
| `settle_ms` | Waveform residual convergence at -40 dB, inherited from #184; modulation may prevent convergence. |
| `click_db` | Worst 1 ms second-difference energy relative to the hardest reference picking attack. A proxy, not a perceptual click verdict. |
| `overshoot_db` | Worst 5 ms energy excess over both held sounds. |
| `post_p99_ms`, `cpu_switch_ms` | Callback wall-time p99 and transition maximum. |
| `overruns` | Callback durations exceeding 1.333/2.667/5.333 ms. These are measured budget violations, not measured hardware xruns. |
| `process_cpu_percent` | Total process CPU time divided by elapsed time over the post-switch interval; includes the IR/UI workers. One logical CPU is 100%, so values may exceed 100%. |

`response_60ms` and `acoustic_60ms` are reported separately on the sample timeline. The corresponding `*_ready_wall_60ms` flags separately evaluate elapsed buffer-availability time. A quick fade into
hundreds of milliseconds of silence cannot earn an acoustic 60 ms pass.

Reproduce with the normal Linux development dependencies installed. Product/editor builds remain separate:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCHIMERA_BETA_QUALIFICATION_ONLY=ON -DCHIMERA_HEADLESS_RECALL_BENCHMARKS=ON
cmake --build build --target ChimeraSwitchTimingProbe ChimeraRecallSafetyTests ChimeraIRLibraryTests ChimeraIntegratedProcessorTests ChimeraOriginalCabRealtimeTests ChimeraOriginalCabIntegrationTests
```

Then run the comparison (one pass runs all seven current variants; the archived six-policy pass and separate warm control total the same 84 recalls; use `--repeats 2` for an additional repeat):

```sh
python Tools/run_recall_comparison.py --probe /absolute/path/to/ChimeraSwitchTimingProbe --out /absolute/evidence/dir --repeats 1
ctest --test-dir build -C Release --output-on-failure -R 'Chimera(RecallSafety|IRLibrary|IntegratedProcessor|NiflheimrPreset|OriginalCabRealtime)Tests'
```

## Regression coverage

`ChimeraRecallSafetyTests` injects a stopped worker, crosses the 500 ms budget at
all three block sizes, both during playback and before the first callback, asserts exact zero output, applies a host reset while
waiting, and injects late valid kernels to verify recovery and release cleanup.
A separate stalled-state transaction crosses the hold deadline without exposing old audio. Separate cabinet assertions reject a missing IR, an old user-file generation and a superseded
captured source. Routing checks cover Classic, Dual zero weight, Matrix solo and LOW DI. Injected kernels test readiness/ownership, not modeling fidelity. The IR library test checks repeated A/B cache hits, identical reset audio, superseded requests, sample-rate/block/channel reprepare and resource release.

Windows/macOS, physical audio device callbacks, live playing, OBS and actual DAW
listening remain unverified until their own evidence is recorded. Synthetic
signal measurements and Linux test success do not approve those qualities.


The experiment includes a sequential/50 ms CAB-fade control (`baseline`),
sequential/instant-swap control (`serial`), audible-first, bounded two-way parallel,
and audible-first kernel caching (cold and warm A/B). Both controls contain the
new safety checks, so they are not byte-for-byte executions of old PR binaries.

## Source and execution identity

Experiment implementation checkpoint: `61690ebff18a9ea957f11ffdd2435b88b00c7061`.
The measurement binary was built from this worktree on base #194; `summary.json`
records the SHA-256 of Source, Tests and CMake files, in addition to that base SHA.
The generated build-revision string is the base revision, not a release artifact identity.

| PR | Inspected head | Base |
|---|---|---|
| #184 | `9d41f74965d7ee07bac6bf0eaa70af7832775974` | main |
| #187 | `5eea8e4bcfb89debe456f8274f025659cd4342e7` | #184 |
| #188 | `29d919987b59bf2aac12f61894a9e0b91ffdc832` | #187 |
| #194 | `9d60312e13dbc0ef21a2db60f7041cce3afad791` | #188 |

Linux x86_64, GCC 13.3, JUCE 8.0.8, Release `-O3`, shared container.
The first comparison is one complete sequential pass over six policies and
three buffer sizes (four recall scenarios per run). It is exploratory, not a
statistical ranking or hardware deadline qualification. All nonzero exits and
censored recovery results are retained. No thresholds are relaxed for this host.

Priority is nonpreemptive and sampled per worker batch; it cannot interrupt a
kernel already being generated. The content-keyed cache retains visited modeled
A/B states, rather than proactively preparing an unheard slot or imported user IR.
Changes to a slot invalidate a content match naturally. These limits matter when
interpreting cold versus warm results.

## Measured results (exploratory; performance qualification blocked)
21 runs / 84 recalls: **12 runs returned failure**, 4 recalls did not recover within the 1 s audio horizon. 74/84 passed the existing sample-timeline response gate, but **0/84** recovered destination level within 60 ms on either the sample timeline or the wall-clock buffer-availability metric. All 84 recalls had callback budget violations. No failed or censored run is discarded.
Maximum observed 5 ms overshoot was **+0.124 dB** (predeclared regression limit +1 dB). This is small, but not an exact 0 dB claim. Maximum click proxy was **+1.005 dB** above the sharpest reference attack. Neither metric approves listening quality; censored cases have no observed recovery to approve.

### A/B destination-level recovery: wall-clock buffer availability (ms)
| Policy | 64 | 128 | 256 |
|---|---:|---:|---:|
| Sequential, 50 ms CAB fade | 718.6 | 394.3 | 825.3 |
| Sequential, instant swap | censored | 679.7 | 467.8 |
| Audible-first, cold | 537.1 | 777.6 | 537.2 |
| Two preparations, cold | 468.8 | 890.8 | censored |
| Kernel cache, cold | 547.2 | censored | 824.5 |
| Kernel cache, warm A/B | 146.5 | 270.7 | 167.1 |
| Response cache only, warm A/B | 263.7 | 255.5 | 308.1 |

### A/B with only lane 3 audible: wall-clock recovery availability (ms)
| Policy | 64 | 128 | 256 |
|---|---:|---:|---:|
| Sequential, 50 ms CAB fade | 681.8 | 472.0 | 538.3 |
| Sequential, instant swap | 882.8 | 1036.2 | 712.3 |
| Audible-first, cold | 524.4 | 871.4 | 516.0 |
| Two preparations, cold | 455.5 | 3052.3 | 675.5 |
| Kernel cache, cold | 670.1 | 2207.9 | 393.5 |
| Kernel cache, warm A/B | 220.3 | 352.8 | 930.3 |
| Response cache only, warm A/B | 164.7 | 80.6 | 178.4 |

### Preset recalls: wall-clock recovery availability (ms)
All entries below are cold; the two preset scenarios are separate.
| Policy / preset | 64 | 128 | 256 |
|---|---:|---:|---:|
| Sequential, 50 ms CAB fade / preset 0 > 17 | 214.3 | 398.4 | 412.7 |
| Sequential, instant swap / preset 0 > 17 | 343.8 | 250.1 | 220.9 |
| Audible-first, cold / preset 0 > 17 | 188.7 | 360.9 | 671.3 |
| Two preparations, cold / preset 0 > 17 | 275.9 | 255.8 | 196.9 |
| Kernel cache, cold / preset 0 > 17 | 386.9 | 190.8 | 350.3 |
| Sequential, 50 ms CAB fade / preset 17 > 33 | 919.6 | 599.3 | 676.1 |
| Sequential, instant swap / preset 17 > 33 | 307.5 | 946.8 | 472.0 |
| Audible-first, cold / preset 17 > 33 | 363.5 | 597.9 | 944.2 |
| Two preparations, cold / preset 17 > 33 | 701.8 | 410.9 | 1185.8 |
| Kernel cache, cold / preset 17 > 33 | 701.2 | 312.3 | 614.3 |

### Response versus silence versus recovery in the kernel-reuse experiment
Times except the wall column are measured in the rendered audio. Buffer availability has 1 ms window / host-block resolution and is not physical sound propagation.
| Block | Scenario | UI call ms | Response ms | First audible ms | Level recovery ms | Recovery ready wall ms | Silence ms |
|---:|---|---:|---:|---:|---:|---:|---:|
| 64 | preset 0 > 17 (cold) | 265.4 | 8.0 | 379.0 | 383.0 | 386.9 | 373.0 |
| 64 | preset 17 > 33 (cold) | 396.2 | 3.0 | 526.0 | 530.0 | 701.2 | 518.0 |
| 64 | A > B (warm) | 70.6 | 23.0 | 125.0 | 128.0 | 146.5 | 99.0 |
| 64 | A > B lane 3 solo (warm) | 59.9 | 23.0 | 68.0 | 100.0 | 220.3 | 41.0 |
| 128 | preset 0 > 17 (cold) | 65.4 | 8.0 | 186.0 | 189.0 | 190.8 | 179.0 |
| 128 | preset 17 > 33 (cold) | 84.9 | 1.0 | 146.0 | 150.0 | 312.3 | 141.0 |
| 128 | A > B (warm) | 149.0 | 37.0 | 186.0 | 189.0 | 270.7 | 149.0 |
| 128 | A > B lane 3 solo (warm) | 309.4 | 81.0 | 309.0 | 351.0 | 352.8 | 224.0 |
| 256 | preset 0 > 17 (cold) | 228.9 | 10.0 | 319.0 | 323.0 | 350.3 | 309.0 |
| 256 | preset 17 > 33 (cold) | 195.3 | 6.0 | 527.0 | 531.0 | 614.3 | 516.0 |
| 256 | A > B (warm) | 70.7 | 21.0 | 138.0 | 141.0 | 167.1 | 117.0 |
| 256 | A > B lane 3 solo (warm) | 586.4 | 145.0 | 309.0 | 313.0 | 930.3 | 160.0 |

### CPU and callback stability: normal A/B recall
One logical CPU is 100%. Overruns are callback wall durations beyond 1.333 / 2.667 / 5.333 ms, not device xrun measurements. Neither wall-time spikes nor failed deadlines are dismissed as harmless scheduler noise; this run does not separate all OS and processor causes.
| Block | Policy | Process CPU % | Callback p99 ms | Overruns / callbacks | Click proxy dB | Overshoot dB |
|---:|---|---:|---:|---:|---:|---:|
| 64 | Sequential, 50 ms CAB fade | 134.4 | 15.168 | 526/750 | -0.203 | 0.038 |
| 64 | Sequential, instant swap | 225.3 | 7.815 | 269/750 | -18.993 | -0.582 |
| 64 | Audible-first, cold | 140.8 | 3.882 | 170/750 | -17.270 | 0.003 |
| 64 | Two preparations, cold | 160.7 | 8.317 | 157/750 | -0.209 | 0.025 |
| 64 | Kernel cache, cold | 138.2 | 13.219 | 327/750 | -0.383 | 0.003 |
| 64 | Kernel cache, warm A/B | 96.1 | 5.013 | 177/750 | -0.448 | 0.120 |
| 64 | Response cache only, warm A/B | 112.3 | 6.723 | 448/750 | -0.225 | 0.002 |
| 128 | Sequential, 50 ms CAB fade | 115.5 | 9.602 | 52/375 | -0.392 | 0.003 |
| 128 | Sequential, instant swap | 152.5 | 8.430 | 83/375 | -0.180 | 0.020 |
| 128 | Audible-first, cold | 144.7 | 12.902 | 270/375 | -0.343 | 0.003 |
| 128 | Two preparations, cold | 191.7 | 8.689 | 163/375 | -0.435 | 0.012 |
| 128 | Kernel cache, cold | 201.1 | 15.145 | 146/375 | -48.029 | -10.386 |
| 128 | Kernel cache, warm A/B | 111.1 | 11.251 | 199/375 | -0.858 | 0.124 |
| 128 | Response cache only, warm A/B | 108.4 | 20.249 | 258/375 | -0.579 | 0.006 |
| 256 | Sequential, 50 ms CAB fade | 145.0 | 10.018 | 21/188 | 0.994 | 0.013 |
| 256 | Sequential, instant swap | 122.9 | 10.886 | 35/188 | 0.879 | 0.000 |
| 256 | Audible-first, cold | 127.5 | 11.247 | 22/188 | -17.337 | 0.001 |
| 256 | Two preparations, cold | 199.2 | 14.858 | 57/188 | -19.890 | -0.480 |
| 256 | Kernel cache, cold | 172.1 | 16.989 | 71/188 | -22.756 | 0.001 |
| 256 | Kernel cache, warm A/B | 108.9 | 18.859 | 64/188 | 0.967 | 0.033 |
| 256 | Response cache only, warm A/B | 108.5 | 9.340 | 25/188 | 1.005 | 0.000 |

### Interpretation and adoption limit
* Audible-first scheduling alone did not produce a consistent improvement in this pass. It is nonpreemptive and uses a batch priority snapshot, so a newly audible lane can still wait behind work already selected.
* Two-way preparation was not consistently faster and did not establish callback deadline safety. It remains an explicit benchmark policy, not the candidate default.
* Visiting both A/B states helps. Complete-kernel reuse passes independent ownership/reset/invalidation regressions, but its incremental benefit over the pre-existing warmed response cache is mixed: the warm control is faster in some cases. Cold kernel caching is not a solution to first-use generation cost.
* Based on the mixed warm-control result, the final default is **audible-first, one worker, 12-entry immutable response cache**. Complete-kernel reuse and parallel preparation remain explicit experiments. The fully demonstrated correction is fail-closed readiness and stale-identity rejection; performance approval is blocked by the recorded failures. Do not merge or release on the response gate alone.
* State-recall API work is also substantial (see `action_ms`); moving the start of the mute earlier would change the response metric without solving the silent interval. No such change is used to hide failed response gates.

The main six-policy pass ran first. The warmed response-cache-only control was then run separately with the same executable and identical Source/Tests/CMake fingerprint. Timing order and shared-host variability limit ranking. Raw CSVs, logs and both manifests are in [the evidence directory](evidence/recall-cab-20261027/).

Source/Tests/CMake fingerprint: `52f911884c697108485e28109bf81c45ef678f481d2154201d8c9317792e30f6`.

The default-policy decision was made after the comparison, not selected per buffer.
Every comparison run explicitly selected its policy; switching the product/probe
default from `audibleCached` to `audibleFirst` leaves the compared algorithms intact. The final audible-first response cache is enlarged from eight to 12 entries; the final-default rerun identifies that build separately. The IR cache regression explicitly selects the optional
reuse policy. The archived timing binaries remain identified with the experiment
checkpoint, rather than being mislabeled as the final-default build.

## Final product-default measurements (separate build)

The final source `d7abb79c…` was rebuilt and tested **without a benchmark policy override**. It uses one audible-first worker and 12 immutable cached responses; all 12 rows report zero recycled-kernel hits. Preset recalls are cold; only A/B cases revisit prewarmed states. This is another one-pass, shared-host observation, not a controlled proof of improvement over the earlier build.

3 runs / 12 recalls: one run failed the response gate; **11/12** passed the 60 ms response gate on both sample and callback-return timelines, but **0/12** passed either acoustic recovery gate. All 12 recovered within the rendered horizon, and all 12 had callback budget violations. Maximum 5 ms overshoot was **+0.205 dB**, maximum click proxy **+0.980 dB**. An exit-zero probe run does not approve real-time performance; the probe response/overshoot assertions and the CAB CPU gate are separate.

### Response, silence and level recovery

Response/first audible/level recovery below are callback-return wall times from the recall request. The gap is rendered-audio duration, which excludes extra wall time spent stalled in callbacks. These are buffer availability measurements, not measured speaker output. UI call duration is retained separately in the raw CSV.

| Block | Scenario | Response ms | First audible ms | Level recovery ms | Audio gap ms |
|---|---|---:|---:|---:|---:|
| 64 | preset 0 > 17 | 8.6 | 157.9 | 162.4 | 152 |
| 64 | preset 17 > 33 | 2.0 | 385.3 | 406.2 | 378 |
| 64 | A > B | 330.0 | 705.6 | 710.0 | 235 |
| 64 | A > B lane 3 solo | 35.3 | 187.4 | 195.9 | 95 |
| 128 | preset 0 > 17 | 9.0 | 165.4 | 170.1 | 158 |
| 128 | preset 17 > 33 | 4.2 | 352.8 | 359.1 | 255 |
| 128 | A > B | 19.7 | 223.7 | 235.5 | 91 |
| 128 | A > B lane 3 solo | 19.4 | 116.8 | 122.1 | 93 |
| 256 | preset 0 > 17 | 6.4 | 493.7 | 494.8 | 464 |
| 256 | preset 17 > 33 | 7.9 | 264.1 | 269.5 | 255 |
| 256 | A > B | 17.1 | 114.4 | 120.3 | 96 |
| 256 | A > B lane 3 solo | 23.0 | 127.0 | 131.9 | 101 |

### CPU and signal proxies

Process CPU includes background work (100% is one logical CPU). Callback p99 is post-switch wall time; overrun denominators are 750/375/188 callbacks for 64/128/256 respectively. The high 64-sample A/B response and UI-call time are retained, not removed as outliers.

| Block | Scenario | Process CPU % | Callback p99 ms | Overruns | Overshoot dB | Click proxy dB |
|---|---|---:|---:|---:|---:|---:|
| 64 | preset 0 > 17 | 56.6 | 1.606 | 18 | +0.035 | -24.190 |
| 64 | preset 17 > 33 | 127.8 | 5.241 | 199 | -0.001 | -0.482 |
| 64 | A > B | 159.0 | 5.880 | 120 | +0.023 | -0.199 |
| 64 | A > B lane 3 solo | 106.3 | 4.183 | 483 | +0.001 | -0.312 |
| 128 | preset 0 > 17 | 69.0 | 4.688 | 20 | +0.020 | -14.902 |
| 128 | preset 17 > 33 | 123.9 | 10.197 | 107 | +0.011 | -0.205 |
| 128 | A > B | 105.3 | 12.418 | 111 | +0.151 | -0.447 |
| 128 | A > B lane 3 solo | 92.8 | 6.366 | 110 | +0.002 | -0.127 |
| 256 | preset 0 > 17 | 120.0 | 148.995 | 14 | +0.205 | -12.287 |
| 256 | preset 17 > 33 | 85.9 | 6.206 | 6 | +0.000 | +0.713 |
| 256 | A > B | 66.0 | 7.087 | 8 | +0.010 | +0.980 |
| 256 | A > B lane 3 solo | 77.1 | 6.924 | 7 | +0.007 | +0.411 |

Reproduce the product-default path separately (the runner supplies no `--policy` argument):

```sh
python Tools/run_recall_comparison.py --probe /absolute/path/to/ChimeraSwitchTimingProbe --out /absolute/evidence/final-default --repeats 1 --final-default
```

Final Source/Tests/CMake fingerprint: `9da5948255d2b8cd6ec12dcdf48000443dd471888013c628e403c4b6c85cdc64`.
See [the manifest](evidence/recall-cab-20261027/final-default/summary.json) and [binary identities](evidence/recall-cab-20261027/final-default/build-identity.json).

## Validation phases and remaining failures

The experiment checkpoint (`61690eb…`) passed the full
`ChimeraIntegratedProcessorTests` in 580.40 s and the independent CAB numerical
regression in 13.44 s. Focused timeout, user-IR preservation, cache lifecycle and
cold/reprepared microphone tests also passed. This is **not** a rerun of all 65
previous Linux tests.

`ChimeraOriginalCabIntegrationTests` failed its existing CPU p99 gate. Its
48 kHz / 256-sample stereo CAB-only p99 was 13.810 ms against a 5.333 ms budget;
paired thread CPU p99 was 13.809 ms. These observations cannot be dismissed as
wall-clock preemption alone. The run reported `callback_new=0`,
`callback_delete=0` and 48 kHz CAB-integration kernel residuals below 3.73e-9, but the allocation
watch covers thread-local C++ operators only, not every malloc, lock or host path.
The failing log remains in the evidence directory.

The final default-selection implementation is
`d7abb79c97ffd02fe4690eb2793a5473f9bddae9`: one audible-first worker, 12 immutable
responses, no complete-kernel reuse by default. The final-default directory
contains separate binary identities, focused validation and a no-policy-override
comparison. The full preset-bank run above belongs to the experiment checkpoint;
it is not relabeled as a full final-default test.

Final-default safety, IR library and prepared-state checks passed in 17.22 s,
1.25 s and 3.38 s respectively. The independent CAB numerical oracle binary is
byte-identical to the one already passed, so it was not rerun. Full integrated
preset-bank validation was not repeated after the final default selection.

The final-default CAB integration test again failed its existing CPU gate
(24.70 s). At 48 kHz / 64 samples / stereo, callback p99 was **4.401 ms**
against a **1.333 ms** budget, paired thread CPU p99 **4.402 ms**, with
**200/1600** budget violations. At 256 stereo, p99 was 1.999 ms against
5.333 ms, but one callback still exceeded budget. The integration suite has no
128-sample case; the recall probe covers that buffer size above. Callback C++
new/delete counts remained zero and 48 kHz kernel residuals below 3.73e-9.
These narrow correctness/allocation observations do not cancel the CPU failure.
All final logs and exits are retained in `final-default/validation.json` and its
matching logs.

Windows/macOS builds, full product/editor packaging, physical device xruns, live
playing, OBS and DAW listening remain unverified. Both listening and real-time
performance approval remain withheld.
