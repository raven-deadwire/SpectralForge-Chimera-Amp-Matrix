# Independent scalar / PR75 S8T acceptance experiment

Research only. Product Source files, PR73, original tests and deadlines are unchanged. No merge, packaging or release. PR74's completed timing population is not rerun or used as a baseline measurement.

## Fixed treatment and protocol

- A: scalar `c2263e4c90bffb9a56cc41285ebf8207e641b3aa`.
- B: `29865abb4da3ab78592c970a263ee62af63fc673` plus the exact S8T patch from PR75 `2df0b1e7c1a1e7022ed77a577bf0effb99a672fa`.
- Patch SHA256: `7aaef8ab7367c44083009121c6b2ac66d5b6378bc25b731b2b3720f5afbfcde0`.
- Resulting convolution header SHA256: `f95f02d204497aa8c1f18bcb7ad66954b2174780db2570d4a47bd7dba3dbeba4`.
- JUCE: `d6181bde38d858c283c3b7bf699ce6340c050b5d`.

The PR74 controller is copied into this independent tool to preserve the completed experiment. Its CMake, generated diagnostic probe, 1,600 callbacks per route, workload, clocks, six AB and six interleaved AA quartets per mode, ABBA/BAAB alternation, bootstrap and predeclared 2% / maximum absolute AA envelope rule are retained. Added wall means are descriptive; the acceptance endpoints are unchanged. A/A uses the identical A executable/hash, not a separately rebuilt binary.

A single native Apple Silicon macOS job builds both sources with the same AppleClang Release configuration and dependencies before any measurement. Source identity is checked before and after. Only the expected exact S8T header modification is allowed in B's disposable checkout. Every other tracked file, including all original tests, must match scalar A. Compiler flags, generated probe, binaries' hashes, environment, trial timestamps and per-quartet load averages are preserved.

For each of three CAB engines, 32/64 samples and mono/stereo at 96 kHz, report full CAB callback wall mean/p99/max, thread CPU mean/p50/p99, deadline misses, CPU-over-budget, and separate profiled spectral MAC wall timing. The all-callback and asynchronous worker phases are descriptive populations. Deterministic forced six-slot publication is the primary causal population; profile call exposure must match. Control and instrumented profile populations are never pooled.

Analysis resamples six quartet effects, not correlated callbacks (10,000 deterministic bootstrap draws). Both ABBA and BAAB must improve; lower 95% bound must exceed both 2% and the worst absolute AA quartet effect. Every primary control CPU-mean and profile MAC-mean route must pass, without established control CPU/wall-p99 regressions. Load averages are contextual and do not identify a thermal or scheduler mechanism. This is evidence within one hosted runner allocation, not universal hardware or DAW acceptance.

Both A and S8T run unchanged five CTests, 66 deadline routes, 144 realtime equivalence routes, 2,645,088 frozen floats plus 388,800 fade floats, immutable spectra, zero latency and callback allocation/scheduling assertions. Failures, misses and nonzero process exits are retained, never deleted or rerun to get a pass. Either-arm safety failure or any measured executable gate failure blocks performance acceptance. Collection success does not mean performance success. `integration_eligible=false` and `release_approved=false` remain explicit; the result is for subsequent review only.

## Reproduce

```sh
python Tools/CabS8TAB/test_analysis.py
python Tools/CabS8TAB/run.py --parent /clean/scalar --candidate /clean/pr73 --build /fresh/build --output /fresh/evidence
```

The workflow retains full CSVs, logs, CTest XML, compile metadata, source patches/hashes and the SHA256 manifest for 90 days. A compact lossless evidence archive should additionally be committed to this research branch after offline manifest and raw-clock verification. Full product/DAW/device acceptance is separate.
