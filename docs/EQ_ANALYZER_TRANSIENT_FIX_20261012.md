# EQ analyzer: continuous transient coverage and separate FIFO recovery

Development follow-up for [PR #52](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/52).
Baseline: `360a2d843981691cc8670f292610dbd7c2816bd7`.

## Scope and current disposition

This change repairs analysis only. It does not merge or publish the public beta.
It retains the existing 2048/4096/8192-point, full-host-rate Hann analysis,
stereo power calibration, IN/OUT taps and smoothing choices.

The original analyzer and an otherwise unchanged original analyzer with a larger
FIFO both fail the new short-attack detection assertion. The repair in
[`beb72f5`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/beb72f520721685f3698883297371be7def92198)
passes all four required product tests and the temporal/partition regressions on
Windows, macOS and Linux in [run 38224717793](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38224717793).
The [complete measurement bundle](evidence/eq-transient-fix-20261012/README.md)
retains CSVs, logs, source identities, artifact digests and the limitations.

This functional repair is eligible for a development integration-test candidate.
**Final October 23 installer/DAW acceptance is withheld.** Windows's native
192 kHz producer reaches only 71.44–75.59% of nominal; macOS reaches 42.31–43.84%
at 96 kHz and 54.84–55.92% at 192 kHz. Their zero-drop rows do not qualify the
requested rate. Linux CI reaches 98.97–99.998% of nominal, but 192 kHz analysis
costs 21.76–25.18% of one core equivalent and full-queue replay takes up to
86.40 ms. These are measured limits, not waived by functional test success.

## Two different failures

The old consumer drained all pending input into an N-sample ring, then ran one
FFT on the final window. With more than N input samples per UI update, the
prefix was overwritten before any FFT saw it. Even without FIFO overflow, a
384-sample attack at offset 64 in a 40 ms batch disappeared at 96 kHz/2048,
192 kHz/2048 and 192 kHz/4096.

The original 8192-frame FIFO independently held only 42.67 ms at 192 kHz. The
nominal 25 Hz UI timer left 2.67 ms of queue headroom. A delayed consumer could
therefore overflow even when the FFT calculation itself was fast. The old
macOS 192 kHz/8192 observation rejected 139,648 samples, but did not record
consumer intervals at those exact moments. Its actual production rate was
122,566 samples/s, or 63.8363% of nominal. The separate 48 kHz timing record
cannot establish the precise cause of every high-rate rejection. In particular,
the high-rate phase did not take screenshots or encode PNGs.

## Design choice

| Alternative | Benefit | Cost / qualification work | Decision |
|---|---|---|---|
| Larger FIFO alone | More scheduling tolerance | Does not analyze skipped windows | Rejected as a transient repair; retained separately for queue headroom |
| 50% overlapping Hann windows | Covers the input with roughly half the 75%-overlap FFT work | More attack-position weighting variation near window edges | Viable lower-cost alternative if the measured 75% cost is unacceptable |
| 75% overlapping Hann windows on the existing consumer | Continuous chronological coverage, lower position sensitivity, existing thread ownership | More FFT work per update; native UI still owns analysis | Implemented |
| Dedicated analysis worker | Separates draining/FFT from native paint and message stalls | Requires reset epochs, parameter/configuration handoff, result snapshots and close/reopen lifecycle verification; does not reduce FFT arithmetic | Follow-up if native queue or responsiveness measurements require it |
| Antialiased lower-rate analysis stream | Better low-frequency bin spacing at high host rates with less FFT work | Needs filter passband/stopband, effective-rate labeling, gain and delay calibration; decimation after the raw FIFO alone does not protect that FIFO | Separate design work |
| Parallel short/long analysis windows | Fast attack observation and long low-frequency observation | Additional transforms and a defined display/calibration crossover | Separate design work |

For a narrow event in a fully primed continuous stream, with sufficient subsequent
observation, the largest Hann weight among complete 75%-overlap windows is at
least approximately 0.854, compared with 0.5 at 50% overlap. These are window-
geometry bounds, not a promise about arbitrary musical signals or sample-zero
startup attacks. The tested coverage fixtures explicitly prime N real samples.

### Implemented processing contract

1. Copy/pop one finite FIFO snapshot into preallocated staging before starting
   FFT work, so the audio producer regains queue headroom promptly.
2. Acquire the first complete real N-sample window. Do not fabricate startup
   data or transform a partially filled window.
3. Process every subsequent complete `H = N / 4` hop in sample order, retaining
   the residual hop across UI updates.
4. Advance the existing attack/release power envelope after every window using
   elapsed audio sample time. Rebuild paths only once after the batch.
5. Preserve latest-window semantics for `rawDB()`. The visible smoothed spectrum
   retains an earlier attack even if the last raw FFT is silent.
6. Check overflow before copying, after copying and after FFT processing. Any
   observed discontinuity invalidates history and requires a new complete window.
7. Keep explicit UI reset statistics separate from in-update recovery statistics,
   so recovery cannot erase the expensive FFT work that actually occurred.

For M continuous samples following a reset, the number of executed complete
windows is zero for `M < N`, otherwise `1 + floor((M - N) / H)`.
No complete scheduled window is skipped to satisfy a work-count cap.
Each update is bounded by the finite snapshot, at most approximately
128/64/32 windows for FFT 2048/4096/8192 with the new queue.

The FIFO is now 65,536 frames (341.33 ms at 192 kHz). This is finite scheduling
tolerance, not an unlimited-stall guarantee. The two FIFOs plus two panel staging
arrays use approximately 4 MiB; existing history and FFT buffers are additional.
The affected test's FIFO now lives on the heap to avoid a 1 MiB stack object.
The audio producer retains the same allocation-free, lock-free per-sample push.

## Resolution and visible response

Overlap improves coverage; it does not improve the frequency resolution of N
samples at the host rate. All frequencies below are measured FFT spacing, not
interpolated pixel spacing. Hann equivalent noise bandwidth is 1.5 bins.

| Host rate | FFT | Bin spacing | Observation window | Hop interval |
|---:|---:|---:|---:|---:|
| 96 kHz | 2048 | 46.875 Hz | 21.333 ms | 5.333 ms |
| 96 kHz | 4096 | 23.4375 Hz | 42.667 ms | 10.667 ms |
| 96 kHz | 8192 | 11.71875 Hz | 85.333 ms | 21.333 ms |
| 192 kHz | 2048 | 93.75 Hz | 10.667 ms | 2.667 ms |
| 192 kHz | 4096 | 46.875 Hz | 21.333 ms | 5.333 ms |
| 192 kHz | 8192 | 23.4375 Hz | 42.667 ms | 10.667 ms |

At 48 kHz/8192 the bin spacing remains 5.859375 Hz and the full window remains
170.667 ms. Neither that setting nor high-rate interpolation promises to resolve
low B from A0. The GUI still paints nominally at 25 Hz; a 2.667 ms analysis hop
is not a 2.667 ms screen-refresh promise.

A late transient can initially be near a Hann edge and become visible at a later
scheduled endpoint. The regression therefore records both the first 40 ms
display and a bounded additional observation tail. For example, the independent
96 kHz/8192 late-burst probe reads about -79.49 dBFS at the first 40 ms boundary
and -31.44 dBFS within the additional tail. This is explicitly distinguished
from permanently dropping the burst. A dual-resolution design would address
that separate observation-time tradeoff.

## Verified independent controls

All attack fixtures start after a complete silent window and have zero FIFO
loss. Original and FIFO-only enlarged controls produced byte-identical CSVs.
Both compiled negative controls exit 1 at the actual lost attack, rather than
failing solely because a new window-count assertion differs from the old design.

| Early burst | Original / FIFO-only display at 40 ms | Fixed display at 40 ms |
|---|---:|---:|
| 96 kHz / 2048 | -120 / -120 dBFS | -24.0521 dBFS |
| 192 kHz / 2048 | -120 / -120 dBFS | -26.8510 dBFS |
| 192 kHz / 4096 | -120 / -120 dBFS | -29.9840 dBFS |

All 18 early/late cases and 90 rate/FFT/smoothing/boundary partition cases pass
in the isolated probe. The latter compare 25 Hz, per-hop and irregular consumers
at identical audio sample positions; maximum raw/envelope/pixel differences are
exactly zero in that run. The weakest observed peak in the 90 cases is -36.845
dBFS, above the -60 dBFS detection gate.

The 24 separate finite-queue tests inject rate × duration samples without a
consumer, deterministically representing a backlog rather than a timed native UI
pause. They retain the complete 100/250 ms backlogs at 96/192 kHz and 500 ms at
96 kHz. A deliberately excessive 500 ms backlog at 192 kHz contains 96,000 samples:
30,464 are rejected, 65,536 are discarded during discontinuity recovery and zero
remain pending. Fresh-window recovery passes for all FFT sizes. This behavior
must not be relabelled as loss-free operation.

Probe timings were taken during compilation and are excluded from performance
acceptance. The final full-build/CI measurements provide the CPU evidence.

## Product and platform verification protocol

The existing four required EQ/Gate CTest suites remain required. The analyzer
suite now includes the temporal/partition gates, 24 controlled FIFO cases,
48/96/192 kHz calibration, sample-clock smoothing and nine whole-processor
audio-invariance/host-latency configurations. It also measures 36 EQ callback
configurations with 600 measured callbacks each and callback allocation checks.

The native UI test observes 96/192 kHz with all three FFT sizes for 10 seconds
each, without screenshots or CSV writes during audio production. It records
actual production rate, consumption, rejected and discarded samples, pending
queue, observed consumer intervals and FFT/update costs. It explicitly records
unobserved telemetry updates; queue high-water is an observed lower bound.

After the producer joins, every case checks:

`initial pending + produced = dequeued + discarded + rejected + final pending`.

Here dequeued includes a staged snapshot later invalidated by overflow, and
executed FFT windows include work invalidated during recovery. Neither counter
by itself establishes retained display coverage. Whole-process CPU includes the
audio route, UI and fixture; isolated analyzer CPU is reported separately.

No-loss rows with an underproducing source do not qualify the nominal host rate.
The hosted synthetic native observations retain `performance_acceptance =
NOT_ASSESSED`; the numerical CSVs must be assessed independently of executable
PASS status. Ten-second samples are stronger diagnostics than the old 2.6-second
probe, but they do not certify a long DAW session.

## Audio, state and release boundary

The [source audit](evidence/eq-transient-fix-20261012/audio-state-scope-audit.json)
checks the complete GraphicalEQ processing class and parameter/coefficient
definitions against the baseline, separately from the FIFO capacity edit.
Processor/DSP source and version/channel files in that audit are identical.
No parameter ID, host ordinal, preset field or latency calculation is changed.

The October 23 installer decision also depends on #53's remaining interaction
work, actual installer/DAW loading and saved-state tests, real guitar/bass DI,
physical display readability and representative extended sessions. This repair
must not imply those independent requirements have been completed.

The [final measurement report](evidence/eq-transient-fix-20261012/README.md) and
[machine-readable assessment](evidence/eq-transient-fix-20261012/assessment.json)
record the exact-source product/CI results, development-candidate decision and
uncompleted installer/host acceptance. The later evidence commit changes only
documentation; its executable source remains byte-identical to the tested repair.
