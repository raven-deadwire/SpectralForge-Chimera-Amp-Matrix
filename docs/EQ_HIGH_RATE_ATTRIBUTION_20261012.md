# EQ high-rate attribution — development investigation

## Scope and retained result

The chronological N/4-hop transient repair in
[`beb72f520721685f3698883297371be7def92198`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/beb72f520721685f3698883297371be7def92198)
is an established functional result. Its completed three-platform run is
[38224717793](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38224717793).
The 84 files in `docs/evidence/eq-transient-fix-20261012` are retained byte for
byte. `Tools/eq_attribution_provenance.py` checks their canonical SHA-256 tree
digest `bb47058bd2798bea744da7d61754aefb5b7dbd5d8488086c313a5ae92897bc2e`.
The original missing-attack diagnosis and negative controls are not repeated.

This follow-up separates source-clock scheduling, waveform generation, audio
processing, FIFO consumption, FFT work and native UI dispatch. New code stays
in draft PR #52. No public-beta version, publication workflow, release asset,
feed, website, audio parameter, EQ coefficient or host latency policy changes.

## Why the previous native production ratio cannot identify the bottleneck

The historical `SyntheticAudio` fixture in `Tests/GraphicalEQUITests.cpp`
generates 128 samples on an ordinary `std::thread`, calls the processor, then
sleeps until its next nominal deadline. If it is more than two periods late,
it sets that deadline to the current time. That operation discards source-clock
debt without recording how much simulated audio time was abandoned. The
128-sample period is 0.667 ms at 192 kHz.

The callback timer encloses only the processor call. It excludes waveform
generation, buffer fills, thread wake latency and source-clock rebasing. The
historical 96/192 kHz observations also have no analyzer-OFF control. Their
underproduction therefore cannot be attributed to the analyzer from its
measured update time alone. This is a fixture limitation, not yet proof that
any one operating system's scheduler is the sole cause.

On macOS the native test has its own CFRunLoop/AppKit pump. Its inclusive
dispatch durations are distinct from those of a DAW host's normal event loop.
A long gap between analyzer updates does not by itself show an equally long
FFT or audio callback.

## Controlled measurements

`ChimeraGraphicalEQUITests --attribution <directory>` creates 90 source-bound
cases; the normal historical UI suite remains unchanged. The matrix includes:

| Control | Conditions | What it isolates |
|---|---|---|
| Source and callback stages | 192 kHz, producer only / EQ pair / whole processor, analyzer OFF | Scheduling/generation versus audio work |
| Source-clock policy | Historical sleep/rebase versus due-sample catch-up | Source time discarded by the fixture |
| Generator | Per-sample `sin`/`fmod` versus the identical precomputed tone | Synthesis overhead outside `processBlock` |
| Consumer/UI | Direct consumer, native timer with EQ panel painting suppressed, normal native panel | FIFO/FFT versus dispatch/path/paint |
| Rate/FFT/cadence | 44.1/48/96/192 kHz × OFF/2048/4096/8192 × 20/40/80 ms | Rate and requested consumer cadence sensitivity |
| Repeated endpoints | Repeated native analyzer-OFF source policies | Within-run variability |

Per-callback rows retain generation time, processor time, source-clock lateness,
sleep request and actual wait, wake lateness, rebases and abandoned source time.
Preallocated records are written to CSV only after joining the producer.
No test waits or allocations are inserted into the production audio callback.

Summary rows retain actual process/producer/message-thread CPU time, callback
overruns, consumer gap tails, exact FIFO accounting, FFT window counts and
separate update/path/panel-paint wall costs. A native heartbeat observes message
loop service gaps. macOS additionally records CF dispatch, AppKit event polling
and event sending. Those timings are **inclusive** and must not be added to
the FFT/paint times nested within them.

Due-sample catch-up intentionally preserves the original source time and may
produce late blocks in bursts. A nominal aggregate rate from this diagnostic
does not qualify real-time delivery; its late callbacks and maximum source
clock debt remain visible. Sampled FIFO high-water is a lower bound on the
true instantaneous maximum. Pending data at stop are not counted as lost data.

## Narrow analyzer cost experiment

`ChimeraEQAnalyzerPerformanceTests` compiles the already-fixed analyzer's exact
header bytes alongside the candidate in the **same executable**, using the
same JUCE backend and build flags. The frozen copy is test-only and is never
included in the plug-in. Its SHA-256 is
`ad0e9f7dc7fe46579b24707291bf63a5ee09363de701b1606c5cedf98f970f26`.

ABBA BAAB measurement order compares complete update wall time and measured
feed-plus-analysis thread CPU. The comparison covers all four rates and three
FFT sizes at 20/40/80 ms, plus separately labelled full-capacity replay.
It verifies every chronological window, raw/envelope/display spectral parity,
DC/Nyquist calibration, antiphase stereo and corruption recovery. It never
skips an overlap window or treats a changed timer interval as a new FFT hop.

The candidate under measurement computes power from the real transform's
complex bins instead of first taking a magnitude and then squaring it. It
also precomputes coherent power gain and removes redundant clearing performed
by the frequency-only helper. The product's capture producer, FIFO size,
chronological N/4 overlap, smoothing sample time and audio path remain intact.
The implementation is accepted only if paired measurements demonstrate a
useful cost change without numerical or functional regressions.

### Initial local paired result

The [same-executable local receipts](evidence/eq-high-rate-attribution-20261012/local-paired/README.md)
support retaining this narrow optimization: all 48 cost cells improved, with
median complete-update wall-cost reduction 9.33% (range 2.58–18.34%). All 108
spectral parity cells passed; maximum raw/envelope/display error was
0.0000152587890625 dB. Every expected sample/window was retained. The initial
loaded A/A observation and the subsequent idle A/A control are preserved
separately; only matched A/B cost supports this implementation decision.
These are local Linux cost measurements, not Windows/macOS or DAW acceptance.

## Functional regression and acceptance boundaries

The existing analyzer contracts now exercise 44.1 kHz as well as 48/96/192 kHz:
24 early/late transient cases, 180 partition/ballistics cases, 48 deterministic
FIFO/stall cases, 48 callback settings and 12 whole-processor audio-invariance
settings. This is a regression of the retained repair, not a reopening of its
original cause. The separate redundant CI replay of the same transient suite
is removed; the required `ChimeraEQAnalyzerTests` still executes those gates.

The CI keeps these decisions separate:

1. **Functional contract:** detection, numerical parity, sample accounting,
   output identity, existing host latency and counted callback allocations.
2. **Hosted diagnostic observation:** production ratio, processing and CPU
   costs, callback/consumer tails, losses and catch-up debt, with raw evidence.
3. **DAW/device acceptance:** actual hardware clock, host buffer schedule,
   audible behavior, sustained sessions, installation and project loading.

Every new native performance row retains `performance_acceptance=NOT_ASSESSED`.
A test-process exit code of zero means its instrumentation/accounting assertions
passed; it does not upgrade the third decision. Real DAW/audio-interface
acceptance and October 23 installer acceptance remain pending until directly
validated. Results and source-bound evidence are recorded after the new run.
