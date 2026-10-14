# POST arithmetic cache evidence — October 14, 2026

Reference: `b0bf7192a4d3084f9ffda1f30d10e2bc8f947c83`.
Candidate: the `Source/PostNativeDSP.h` blob identified in `provenance.json`.

The processor-stage diagnostic identified the amp/cab and POST stages as the
largest costs. This correction removes repeated pure arithmetic in POST:
rate/model constants are prepared once; identical gain/exponential arguments and
unchanged filter-control snapshots reuse their previous result. State, meter,
bypass histories and the 32-sample filter update clock still advance unchanged.
No DSP is bypassed and no deadline/accuracy limit is changed.

`ChimeraPostNativeTests` includes a frozen pre-cache arithmetic oracle and compares
**1,028,160 audio samples exactly**, plus meter/reduction histories across all nine
models, mono/stereo, variable blocks, automation, bypass/model transitions, reset,
and repeated preparation at 44.1/48/96/48 kHz. The existing POST suite also passes:
64 audio controls, 3 meter controls, 288 stress blocks, stereo isolation, exact
bypass, latency and spatial controls. Raw output: `post-tests.log`. The 1,080-callback allocation/free interception
harness also passed with zero calls (`post-allocation.log`).

## Narrow local cost measurement

Linux x86-64, GCC 13.3.0, `-O3`, JUCE 8.0.8; no editor, driver or DAW. Four paired
runs per setting alternate execution order, 100 warmup + 1,000 timed blocks per
run. Values are medians of batch-average wall microseconds for **the three native
POST sections only**, not callback p99/peak. Model selects the same index in each
section; bypass 1 preserves and advances the bypassed models' processing history.
All 48 pairs are retained in `benchmark.csv`; source is `benchmark.cpp`.

| Rate | Block | Model | Bypass | Before µs | After µs | Change |
| --- | --- | --- | --- | ---: | ---: | ---: |
| 48000 | 256 | 0 | 1 | 136.75 | 138.14 | +1.0% |
| 48000 | 256 | 0 | 0 | 146.45 | 126.41 | -13.7% |
| 48000 | 256 | 1 | 1 | 156.76 | 127.66 | -18.6% |
| 48000 | 256 | 1 | 0 | 162.55 | 111.79 | -31.2% |
| 48000 | 256 | 2 | 1 | 151.54 | 118.99 | -21.5% |
| 48000 | 256 | 2 | 0 | 137.11 | 128.00 | -6.6% |
| 96000 | 64 | 0 | 1 | 36.43 | 32.15 | -11.7% |
| 96000 | 64 | 0 | 0 | 36.73 | 30.75 | -16.3% |
| 96000 | 64 | 1 | 1 | 37.74 | 31.58 | -16.3% |
| 96000 | 64 | 1 | 0 | 39.77 | 35.55 | -10.6% |
| 96000 | 64 | 2 | 1 | 41.33 | 27.96 | -32.4% |
| 96000 | 64 | 2 | 0 | 34.02 | 27.83 | -18.2% |

The 96 kHz/64 cases show lower median section cost in this run; one 48 kHz case
is 1% slower. Shared-host variability and the narrow scope preclude claiming that
callback deadline overruns are fixed. Prior macOS measurements had genuine
same-thread CPU overruns with tuner both on and off. Fresh full-processor,
platform and real-driver/DAW qualification remain required; this evidence is
**NOT_QUALIFIED** for release performance. No signing/host/acceptance gap is waived.

## Reproduction

Build and run the normal `ChimeraPostNativeTests` target for exact parity and the
existing contracts. The Linux `Tests/PostNativeAllocationTests.cpp` harness uses
`--wrap=malloc/calloc/realloc/free` and C++ new/delete interception.

For the optional local benchmark, compile `benchmark.cpp` with the repository's
`Source` and `Tests` include paths and JUCE 8.0.8 core/audio_basics/audio_formats/dsp
objects, C++20, `-O3`, `NDEBUG`, `JUCE_USE_CURL=0`, `JUCE_WEB_BROWSER=0`, and the
normal JUCE global/module definitions. Link pthread, dl and rt. Redirect stdout
to a new CSV; preserve every pair, including slower results. It is not a CI gate.
