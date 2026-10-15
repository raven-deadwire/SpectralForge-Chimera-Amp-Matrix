# PR73 independent Apple Silicon experiment

A is the direct **pre-NEON PR branch parent**, c2263e4c90bffb9a56cc41285ebf8207e641b3aa.
B is pinned PR73 head, 29865abb4da3ab78592c970a263ee62af63fc673.
B's literal Git parent 6b66180d22f4f354aad63691d981e5295fccbd55 is the rejected first NEON implementation, not a scalar control. The controller verifies this ancestry and that A/B differ only in Source/ModeledCabConvolution.h.

One macos-15 ARM64 job builds both clean product checkouts and their diagnostic executables with the same AppleClang, Release options and pinned JUCE. All compilation finishes before measurement. Original product code, tests, thresholds, FMA order, IRs and latency contracts are unchanged. No PR73 edits, merge, packaging, publisher or release action occurs.

The diagnostic source is generated from the identical integration test in A/B by restricting its grid to 96 kHz and 32/64 samples, and writing existing per-callback spectral MAC counters after measurement. Three engines, mono/stereo, three rigs/six mics, original 1,600 callbacks, 1,200 worker/automation + 400 forced-publication callbacks, and all assertions remain. Existing response checks warm each route. Both diagnostic arms use the same generated file. The normal diagnostic target compiles stage clocks out; the profile target enables existing stage clocks and existing diagnostic filter refresh. These populations are never pooled or subtracted to infer exact observer cost.

For each mode: six A/B quartets, alternating ABBA/BAAB; six interleaved A/A quartets with the identical A binary in both logical positions. Outer AA/AB and control/profile order alternates by round. No reruns, outlier deletion or threshold adjustment. Every raw sample, clock validity, miss, command return code and failed population is preserved. Scheduling priority is observed by the original test and unchanged.

Primary causal population: deterministic forced six-slot publication, independently for every engine/buffer/channel. Worker/automation and all-callback populations are reported separately because asynchronous publication exposure can differ. Profile spectral call schedules must match in the primary population. Spectral MAC is wall elapsed stage time; thread CPU refers to the whole CAB callback. Neither is fabricated as the other.

Predeclared conservative evidence rule: paired quartet log-ratio effects, 10,000 deterministic bootstrap samples of six quartets (not individual callbacks); lower 95% bound must exceed both 2% and the largest absolute A/A quartet effect; both execution orders must improve. Require primary callback thread CPU mean and spectral MAC mean to pass for all 12 routes, and no clearly established control CPU/wall-p99 regression. With six quartets these intervals describe this runner allocation, not all Apple Silicon hardware. Deadline miss counts and CPU-over-budget counts are reported separately; a zero miss count is not universal glitch-free proof. Sparse misses are not treated as statistically proven reductions.

Both exact product sources also run the original five CAB CTests, 66 deadlines, 144 realtime equivalence routes and frozen residue/fade bit comparisons. CAB-only acceptance and performance demonstration are separate from **full product validation, real DAW/device acceptance and release approval**, which remain PENDING. `integration_eligible` means eligible for subsequent review only; the workflow cannot merge or publish.

Run with fresh paths:

```sh
python Tools/CabNeonAB/run.py --parent /path/to/A --candidate /path/to/B --build /fresh/build --output /fresh/evidence
```

Artifacts include source/runner/compiler/binary provenance, raw CSVs, original CTest XML/logs, per-route statistical analysis, result JSON, and SHA-256 manifest. The workflow succeeds only when collection is complete; consult `performance_demonstrated` and `integration_eligible` independently of the workflow's green collection status.
