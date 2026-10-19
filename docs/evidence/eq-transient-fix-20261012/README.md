# EQ transient repair — verification evidence, 12 October 2026

This bundle separates **continuous spectrum coverage**, **FIFO sample retention**,
**audio/state invariance**, and **real-time performance acceptance**.

- Development PR: [#52](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/52).
- Original verified source: `360a2d843981691cc8670f292610dbd7c2816bd7`.
- Tested repair code: [`beb72f520721685f3698883297371be7def92198`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/beb72f520721685f3698883297371be7def92198).
- Dedicated CI: [run 38224717793](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38224717793).
- [Design, alternatives, resolution and processing contract](../../EQ_ANALYZER_TRANSIENT_FIX_20261012.md).

## Disposition

The development repair restores the reproduced early-attack spectrum and preserves
existing audio/state contracts. It is eligible for a development integration-test
candidate. The original and capacity-only negative controls
still fail the same actual display-detection assertion, without FIFO overflow.

**October 23 installer/DAW acceptance is not complete.** The macOS native synthetic
producer remains substantially below the requested sample rate and its consumer
intervals are much longer than the nominal 40 ms timer period. Windows's native
192 kHz source also underproduces. Zero drops in those streams cannot qualify
their requested rates. Linux CI's 192 kHz analysis cost is 21.76–25.18% of one core
equivalent, with full-queue catch-up reaching 86.40 ms on the message thread.
Hosted callback timing outliers and these qualification limits remain visible
in the raw results.
No merge, installer qualification or public-beta publication is implied.

## Completed product and platform results

All three CI platforms passed the four required CTest suites and the separate
transient/partition regression target for the exact repair source. All 147 remote
PNGs decoded; the 49 local PNGs also decoded. This checks native component evidence
integrity and does not qualify physical-display presentation.

| Platform | CTest | Total time | Raw evidence |
|---|---:|---:|---|
| Linux CI | 4/4 PASS | 200.66 s | [ci-linux](ci-linux/) · [CI artifact](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38224717793/artifacts/11736082685) |
| Windows CI | 4/4 PASS | 183.82 s | [ci-windows](ci-windows/) · [CI artifact](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38224717793/artifacts/11736478631) |
| macOS CI | 4/4 PASS | 257.63 s | [ci-macos](ci-macos/) · [CI artifact](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38224717793/artifacts/11736512112) |
| Local Linux | 4/4 PASS | 170.78 s | [local-linux](local-linux/) |

Each platform passes 18 temporal cases, 90 partition cases, 24 deterministic FIFO
cases and nine whole-processor audio-invariance configurations. All 90 partition
cases have exactly 0 dB raw/envelope/pixel differences on every platform. Each
platform measures 21,600 EQ-pair callbacks; counted callback allocations are zero.

### Controlled analyzer update cost

These columns retain FFT size and host rate. CPU is the mean update wall-time
equivalent at 25 Hz; p99 is the complete analyzer update, including snapshot
dequeue and all chronological FFT windows. Each window has four transforms.

| Platform | FFT | 96 kHz CPU | 96 kHz update p99 | 192 kHz CPU | 192 kHz update p99 |
|---|---:|---:|---:|---:|---:|
| Linux CI | 2048 | 10.880% | 4.675 ms | 21.758% | 8.756 ms |
| Linux CI | 4096 | 11.336% | 4.962 ms | 22.649% | 9.698 ms |
| Linux CI | 8192 | 12.587% | 5.400 ms | 25.175% | 10.783 ms |
| Windows CI | 2048 | 3.000% | 1.560 ms | 5.942% | 2.593 ms |
| Windows CI | 4096 | 2.870% | 2.082 ms | 6.201% | 4.867 ms |
| Windows CI | 8192 | 3.241% | 1.454 ms | 7.694% | 5.391 ms |
| macOS CI | 2048 | 0.782% | 0.598 ms | 1.501% | 0.673 ms |
| macOS CI | 4096 | 0.810% | 0.659 ms | 1.648% | 1.277 ms |
| macOS CI | 8192 | 0.720% | 0.357 ms | 1.521% | 0.704 ms |
| Local Linux | 2048 | 2.701% | 1.451 ms | 5.330% | 2.563 ms |
| Local Linux | 4096 | 2.340% | 1.269 ms | 4.791% | 2.332 ms |
| Local Linux | 8192 | 2.901% | 1.602 ms | 5.994% | 3.112 ms |

Linux CI is materially more expensive than the local Linux run. These different
workers/build environments do not establish a causal regression or an optimization.
The Linux CI 192 kHz cost of 21.76–25.18% of one core is a real qualification limit.

### Native source rate and consumer stability

Each range covers FFT 2048/4096/8192, each observed for 10 seconds. All rows retain
zero rejected/discarded samples, exact sample accounting and zero unobserved
diagnostic updates. The near-nominal column checks the actual producer against
the requested rate within 5%; it does not grant DAW acceptance.

| Platform | Requested rate | Actual / nominal range | Near-nominal cases | Consumer-gap p99 range | Maximum gap |
|---|---:|---:|---:|---:|---:|
| Linux CI | 96 kHz | 99.9977–99.9981% | 3/3 | 41.14–41.85 ms | 42.78 ms |
| Linux CI | 192 kHz | 98.97–99.87% | 3/3 | 41.23–41.55 ms | 41.81 ms |
| Windows CI | 96 kHz | 99.47–99.68% | 3/3 | 43.11–43.23 ms | 44.19 ms |
| Windows CI | 192 kHz | 71.44–75.59% | 0/3 | 43.24–43.78 ms | 44.77 ms |
| macOS CI | 96 kHz | 42.31–43.84% | 0/3 | 187.05–188.36 ms | 189.42 ms |
| macOS CI | 192 kHz | 54.84–55.92% | 0/3 | 187.29–188.88 ms | 189.85 ms |
| Local Linux | 96 kHz | 99.61–99.94% | 3/3 | 41.81–42.12 ms | 47.68 ms |
| Local Linux | 192 kHz | 97.68–98.98% | 3/3 | 41.83–42.17 ms | 72.32 ms |

Windows 192 kHz and both macOS rates do not reach the requested source rate. Their
zero-drop results only cover the samples actually supplied. On macOS, measured
FFT/update work is much shorter than the approximately 92–105 ms median consumer
interval; the data localizes substantial delay outside analyzer update work, but
does not identify a specific host, OS scheduling or event-dispatch cause.

### Backlog and callback tails

| Platform | Full 65,536-frame catch-up range | EQ-pair deadline overruns / 21,600 | Full-processBlock overruns: OFF / 2048 / 4096 / 8192 |
|---|---:|---:|---|
| Linux CI | 74.55–86.40 ms | 0 | 0 / 0 / 0 / 0 |
| Windows CI | 18.67–22.14 ms | 0 | 0 / 0 / 0 / 0 |
| macOS CI | 5.26–7.67 ms | 0 | 2 / 1 / 0 / 3 |
| Local Linux | 15.33–19.32 ms | 2 | 0 / 2 / 0 / 0 |

The full-queue replay range covers both rates and all FFT sizes. Linux CI reaches
74.55–86.40 ms on a fully populated queue, so bounded memory/work does **not** mean
a 40 ms UI budget is always met. The normal Linux 192 kHz native update p99 values
are 11.06/12.15/13.54 ms, below that replay extreme but still material. A worker
would move this work away from the message thread; it would not remove FFT CPU cost.

The full-processBlock rows above are separate 48 kHz/128-sample observations.
macOS underproduces even with analysis OFF (34.27% nominal); all four macOS modes
are 34.27–38.86% nominal. Those runs cannot isolate the analyzer as the cause of
the scheduling shortfall. Local Linux has two additional measured EQ-pair overruns
at 192 kHz and two full-processBlock overruns with FFT2048. Every native performance row remains
`performance_acceptance=NOT_ASSESSED`.

### Development binary and source identity

[Windows development Standalone](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38224717793/artifacts/11736868386)
has identity `1.3.1-preview.beb72f5207` and source `beb72f520721685f3698883297371be7def92198`.
Its downloaded ZIP SHA-256 is `a712c5066b3f68de0fdac44f184787c17786287e07455085fa5713b2e38f1201`.
The artifact digest and source manifest were verified. It is a development build,
not an October 23 installer or installation/host-launch acceptance result.

Machine-readable [assessment](assessment.json), platform CSV/XML/CTest logs,
artifact metadata, file hashes and the [independent local analysis](local-linux/local-final-analysis.md)
retain the numerical inputs and limitations.

## Evidence interpretation

`eq-analyzer-temporal-coverage.csv` records both the display at the first 40 ms
boundary and a bounded extra observation tail. Every fixture starts after N real
silent samples. It does not certify sample-zero startup impulse response. The
three original missing early attacks now appear at the first 40 ms boundary:

| Rate / FFT | Original display | Original + larger FIFO | Repaired display |
|---|---:|---:|---:|
| 96 kHz / 2048 | -120 dBFS | -120 dBFS | -24.0521 dBFS |
| 192 kHz / 2048 | -120 dBFS | -120 dBFS | -26.8510 dBFS |
| 192 kHz / 4096 | -120 dBFS | -120 dBFS | -29.9840 dBFS |

All three variants have zero FIFO drops in those controls. The original and
capacity-only diagnostic CSVs are byte identical. Their negative detection-gate
executables exit 1. [Probe manifest](probe-manifest.json), [original CSV](original-temporal-coverage.csv),
[capacity-only CSV](capacity_only-temporal-coverage.csv), and
[negative gate source](baseline-negative-probe.cpp) preserve this distinction.

`rawDB()` continues to describe the latest real FFT window, so it can legitimately
read -120 dBFS after an early attack has left that window. The visible envelope
retains the attack because every scheduled window advanced the original power
ballistics in audio sample time. No synthetic peak is injected.

A large Hann window still needs observation time. The 96 kHz/8192 late-burst
fixture is about -79.49 dBFS at 40 ms and -31.44 dBFS within its bounded extra tail.
The 48 kHz/8192 fixture has no complete hop at the first 40 ms boundary. Neither
case is relabelled as immediate detection. Full-rate FFT bin spacing and window
lengths are unchanged; overlap improves time coverage, not low-frequency resolution.

## Separate finite-queue evidence

The old 8192-frame queue had 42.67 ms capacity at 192 kHz, only 2.67 ms beyond a
nominal 40 ms consumer tick. The new 65,536-frame queue has 341.33 ms capacity.
The consumer copies a finite snapshot into preallocated storage before FFT work,
then processes every complete N/4 hop. It never skips a scheduled window to meet
a transform-count cap.

The controlled queue corpus has 24 cases: 96/192 kHz, all three FFT sizes, 100/250/500 ms
backlogs injected without consumption, plus exactly full snapshots. These are
deterministic injections representing consumer stalls, not timed native UI pauses.
All 100/250 ms cases and the
96 kHz/500 ms cases retain all samples. Every 192 kHz/500 ms case intentionally
exceeds capacity: **96,000 produced = 30,464 rejected + 65,536 discarded during
recovery**, with zero pending and successful fresh-window recovery. This is
expected loss and explicit recovery, not loss-free operation.

The historical macOS 139,648 rejected samples are a separate queue-overflow event.
That 2.6-second observation supplied only 63.8% of nominal and did not correlate
high-rate consumer timestamps with each rejection. It did not encode screenshots
during the high-rate phase. The new 10-second telemetry uses a different event-pump
sampling cadence (`settle(1)` instead of one `settle(2600)`). Thus the old loss count
and new no-loss rows are **not an otherwise identical A/B performance comparison**.
The controlled finite-queue corpus provides the independent capacity evidence.

## Measurement definitions and limits

- Analyzer equivalent CPU is `mean complete update wall time × 25 / 10000`, with
  time in microseconds and the result in percent of one core. All 160 measured
  calls per configuration count, including no-FFT calls. This is a derived cost,
  not a DAW CPU meter or the whole application's process CPU.
- Native `process_cpu_percent` includes synthetic audio production, the full
  audio route, UI and other process threads. It can exceed 100% when multiple
  cores are used. It must not be substituted for analyzer-only cost.
- Native rows run for 10 seconds per 96/192 kHz × FFT configuration, with no PNG
  or CSV writes while the producer is active. They retain actual production,
  dequeue, reset discard, rejection and pending counts, consumer intervals,
  update/FFT cost and missing diagnostic-update counts.
- Every sample-flow row checks `initial pending + produced = dequeued + discarded
  + rejected + final pending`. Dequeued includes a staged snapshot later
  invalidated by discontinuity. Pending at capture end is not a lost sample.
- The original long 48 kHz native captures and their snapshot-inclusive CPU
  remain separately recorded. Component rendering and file decoding do not
  establish physical display scanout, DPI readability, actual DI or DAW behavior.
- Functional test PASS and a callback p99 below its deadline do not mean zero
  deadline overruns. The raw maxima and overrun counts are retained.
- Hosted observations explicitly retain `performance_acceptance=NOT_ASSESSED`.
  No causal CPU regression is inferred by comparing different hosted workers.

## Audio, compatibility and release boundary

[The source audit](audio-state-scope-audit.json) confirms that processor/DSP code,
the complete GraphicalEQ processing class, EQ parameter/coefficient definitions,
and version/channel/release policy are identical to the original verified source.
The FIFO constant is the only production audio-side change; the per-sample push
implementation is unchanged. No parameter IDs, ordinals, state fields, coefficient
formulas, automation behavior or audio delay are added or changed.

Nine whole-processor comparisons per platform cover 48/96/192 kHz and all initial
FFT sizes, both capture banks, toggles, resolution transitions, impulses and
intentional overflow. They check finite bit-identical output and unchanged
host-reported latency. The existing 42-sample latency in this fixture remains
42 samples: **zero additional latency**, not zero total plugin latency.
The pre-existing EQ/Gate suites retain parameter, host-gesture, project, A/B,
legacy migration and factory-default checks.

The local tests were compiled from source bytes matching the tested repair,
but CMake was configured before that commit and its generated preview identity
still says `360a2d8`. The local manifest makes that provenance explicit. Each
remote CI build independently stamps and verifies `beb72f5`; use those artifacts
when identifying the submitted development binary. Later evidence-only commits
must not be described as separately compiled code without their own CI evidence.

## Remaining acceptance work

The repaired analyzer is a development candidate. Before final October 23 test
installer acceptance, obtain real host/audio-device evidence at 96/192 kHz across
all FFT sizes, representative blocks and long sessions, including UI stalls and
editor close/reopen. Check actual producer rates, FIFO retention, callback deadline
tails and screen response together. The present macOS underproduction and long
consumer intervals cannot be waived because the functional tests passed.

Actual installer loading, saved-state/automation compatibility in the intended
hosts, guitar/bass DI and listening, physical-DPI review, and #53's separate
October 23 interaction scope remain outside what this repair's synthetic CI proves.
A dedicated analysis worker is a follow-up if real host sessions show analysis
work or queue draining blocked by UI activity; decimation or dual-window analysis
requires its own calibrated frequency/observation-time design. Neither is silently
introduced into this narrowly verified repair.
