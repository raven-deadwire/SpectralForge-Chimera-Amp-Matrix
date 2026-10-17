# Local paired FFT-cost evidence — 2026-10-12

The candidate removes a measured redundant analysis pass. Across 48 paired cells,
the median reduction in total analyzer-update wall cost was **9.33%**; every cell
had a lower candidate median in this run. Median feed-plus-analysis thread CPU
reductions by FFT size were **8.46% / 11.82% / 7.50%** for 2048 / 4096 / 8192.
These are same-process comparative observations on a shared Linux worker.
**DAW and audio-device real-time performance remain NOT_ASSESSED.**

## Source and backend

- The frozen header is `Tests/reference/EQSpectrumAnalyzerBeb72f.h`, copied
  byte-for-byte from PR #52's already-fixed analyzer. Both `beb72f520721685f3698883297371be7def92198`
  and the evidence-only follow-up have Git blob `34f31791c005c291c294afc3fb2ab4783830b4bd`.
  This reference already uses every chronological N/4 window and the 65,536-frame FIFO.
- Baseline SHA-256: `ad0e9f7dc7fe46579b24707291bf63a5ee09363de701b1606c5cedf98f970f26`.
- Candidate header SHA-256: `a7dd27cef57df2edea050cdbaa07c299a63e526269f8518d00409a4f1f6ff941`.
- `Tests/EQAnalyzerPerformanceTests.cpp` includes the production class normally,
  then renames only the frozen reference class with an include-time macro. There
  is no production benchmark mode. Both use the same FFT implementation and
  executable. The source, binary, backend and receipt hashes are recorded in
  `build-manifest.json`; binaries are not included in this evidence directory.
- Backend: JUCE 8.0.8 `FFTFallback`, Linux x86_64, GCC 13.3, Release/O3. `nm -C`
  confirmed the linked FFTFallback engine vtable. Full real JUCE core,
  audio-basics, audio-formats and DSP translation units were linked. The exact
  JUCE Colour implementation satisfies static header colour initialization;
  no GUI message loop or native audio device is involved in these measurements.

The production candidate calls JUCE's real-only FFT and derives double-precision
squared magnitude directly. JUCE's frequency-only wrapper previously called the
same real-only FFT, took an absolute value/square root for every bin, and cleared
the unused transform tail; the analyzer then immediately squared each magnitude.
The candidate also caches the coherent stereo power gain and removes the redundant
input-tail clear. DC/Nyquist weighting, separate L/R power averaging, finite-bin
containment, sample-time ballistics and every N/4 endpoint are preserved. FIFO
capacity, producer/consumer scheduling and the audio callback are unchanged.

## Controls and complete receipts

| Directory | Compiled classes | Conditions and interpretation |
|---|---|---|
| `aa-loaded` | Fixed baseline / identical fixed baseline | An unplanned juceaide rebuild overlapped this first control. Preserved as a loaded control and superseded by `aa-idle` for interpretation. |
| `aa-idle` | Fixed baseline / identical fixed baseline | No local compiler was running. Median apparent reduction 0.256%; median absolute difference 1.794%. Individual cells still ranged from −14.19% to +16.87%, showing shared-worker variability. |
| `ab-candidate` | Fixed baseline / candidate | Median update reduction 9.327%, range 2.584–18.336%, lower candidate medians in all 48 cells. True thread CPU observations support the same direction. |

The A/A executable was compiled while both headers were byte-identical. The clean
A/A run happened after the working production header had been edited; that edit
cannot change the already-linked A/A executable. Each directory contains the
complete parity CSV, paired raw timing CSV, summary CSV and original run log.
Receipt hashes preserve their original bytes.

The timing sequence is ABBA BAAB: four trials per implementation for each of four
rates (44.1, 48, 96 and 192 kHz), three FFT sizes, and four batching fixtures.
The normal fixtures use 20, 40 and 80 ms batches for two seconds of precomputed
audio. The separately labelled `full_capacity_catchup` fixture queues 65,536
frames and measures one bounded snapshot update, with no nominal UI interval.
Input creation, object construction and FFT priming happen outside timing.

`analysis_update_wall_ms` measures the entire update, including the power/envelope
work after the transform. `fft_operation_wall_ms` includes the legacy magnitude
wrapper in the baseline but only the real transform in the candidate; it must
not be used alone to estimate total savings. `feed_and_analysis_thread_cpu_ms`
includes FIFO production plus analysis in this single-thread fixture and excludes
time descheduled by the OS. `*_update_wall_equivalent_percent` is an elapsed-time
equivalent for the simulated audio span, not a measured audio-device CPU meter.

## 192 kHz observations

The 40 ms cadence figures below are median total update cost across **two seconds
of simulated audio**. Each row preserves exactly the same scheduled windows.

| FFT | Windows per trial | Baseline update | Candidate update | Reduction |
|---:|---:|---:|---:|---:|
| 2048 | 750 | 102.017 ms | 90.991 ms | 10.81% |
| 4096 | 375 | 95.400 ms | 86.098 ms | 9.75% |
| 8192 | 187 | 120.695 ms | 107.487 ms | 10.94% |

The following rows measure one full-capacity catch-up operation on this worker;
they do not replace the earlier 86.40 ms observation from a different CI run.

| FFT | Complete windows | Baseline update | Candidate update | Reduction |
|---:|---:|---:|---:|---:|
| 2048 | 128 | 17.956 ms | 16.564 ms | 7.75% |
| 4096 | 64 | 15.584 ms | 13.726 ms | 11.93% |
| 8192 | 32 | 19.338 ms | 17.637 ms | 8.80% |

## Spectral preservation

The independent A/B parity test covers 108 cases: all four rates, three FFT
sizes, three batching intervals and all three smoothing modes. It compares every
raw and smoothed bin, including DC/Nyquist, plus representative display pixels
after every update. The fixture contains bass fundamentals, off-bin tones,
broadband energy, short bursts, DC, Nyquist and antiphase stereo with unequal
input/output channel levels.

- Maximum raw, smoothed and display discrepancy: **0.0000152587890625 dB** in
  each metric, below the existing 0.002 dB partition-preservation tolerance.
- Maximum relative power discrepancy reconstructed from the public dB values:
  **3.5134721954 × 10⁻⁶**.
- Every expected N/4 window and every produced sample was accounted for; FIFO
  drops and discontinuities were zero. Full-capacity timing trials likewise
  assert exact capacity/hop window counts and zero drops.
- Separate NaN/infinity/finite-overflow input probes remained finite and a fresh
  observation recovered. These are containment checks; corrupt-input output is
  not claimed to have sample-exact parity.

No callback/output/latency acceptance is inferred from this isolated benchmark.
Those require the separate processor regression suite. No statistical timing gate
or native performance PASS is emitted by this benchmark. Windows, macOS, a real
DAW and an actual audio interface need their own evidence.
