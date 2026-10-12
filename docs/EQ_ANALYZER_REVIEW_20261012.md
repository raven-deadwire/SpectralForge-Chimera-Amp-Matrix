# EQ prototype review — 12 October 2026

Review target: [PR #52](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/52),
original implementation `f1e088f820875605ae17ee812e58c2cac90c5466`.
Remaining product work: [#53](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/53).
Dates below are KST. This review does not change a public-beta release or approve an installer.

## Assessment

The prototype implements a real, independently developed input/output analyzer
and a larger single-bank EQ editor. Its coherent-tone calibration, stereo power
handling and audio/state isolation have substantial automated evidence. It is
useful for the 14 October prototype review, subject to the defects and limits
below. It is not the completed 23 October test-installation candidate.

The inspected animation really contains 32 decoded 1180 × 780 frames spanning
1.87 seconds, with 50–60 ms encoded intervals. Changing harmonic content and
input/output meter activity are visible. This is a roughly 17 captured-frame/s
recording of a synthetic production-processBlock fixture; it does not establish
25 presented frames/s, device latency, or actual instrument listening quality.
The [original evidence](evidence/eq-analyzer-prototype-20261012/README.md) retains
its source hashes and measurement definitions without being relabelled.

## Confirmed defects and action

| Finding | Reproduction and impact | Follow-up in this PR |
|---|---|---|
| Windows test compilation | Build `ChimeraGraphicalEQUITests` on Windows at the original revision. `windows.h` introduces `min`; `std::min` in `percentile()` fails with C2589. Product targets can compile while the test target blocks validation and preview delivery. | Define `NOMINMAX` for the Windows include and use a macro-safe `(std::min)` call. Recheck all four dedicated suites on Windows. |
| Overlapping node selection | Put all bands at the same frequency/gain, select Band 12, then drag its visible node. The old hit-test scans from Band 1 and modifies that slot instead. | Test nodes in painting order, preserving the selected node at an exact overlap. A native regression checks both EQ banks, unchanged unselected parameters and balanced host gestures. |
| macOS evidence acquisition | The original dedicated macOS run fails after the live tests because `settle(1800)` yields too few whole-processor callback samples. The assertion executes before writing any timing row and before later state/layout tests. | Keep the required sample coverage, collect a 600-callback target with a bounded 1.8–8 s observation, and flush count/elapsed/production-rate/timing data before asserting. Run functional recall/layout checks first and retain close checks even if observation fails. The CSV explicitly marks performance acceptance `NOT_ASSESSED`; a slow producer is not labelled real-time capable. |
| Small EQ annotations | Original panel labels are 9.5 logical pixels; at the offered 75% scale they become approximately 7.1 pixels. Small node labels and axes are also difficult to read. | Raise panel annotations to 12 px, axis frequency labels to 12.5 px, and node labels/targets. Rename the analyzer's ambiguous `RESPONSE` label to `SMOOTHING`. Native screenshots still require review at every offered scale and physical display DPI. |

### High-rate transient coverage remains a product issue

The analyzer drains a bounded FIFO snapshot but transforms only its newest
complete N-sample window once per timer opportunity. If a 40 ms interval
contains more than N samples, early samples are overwritten in the rolling
window without entering that update's FFT, even when the FIFO drop counter is
zero. A short burst entirely inside that prefix can disappear from both raw
and smoothed spectra.

| Host rate | FFT | Samples per nominal 40 ms update | Prefix not observed by the newest FFT | Time span |
|---|---:|---:|---:|---:|
| 96 kHz | 2048 | 3840 | 1792 samples | 18.67 ms |
| 192 kHz | 2048 | 7680 | 5632 samples | 29.33 ms |
| 192 kHz | 4096 | 7680 | 3584 samples | 18.67 ms |

`ChimeraEQTemporalCoverageProbe` is an explicit **diagnostic**, not an acceptance
test that calls missing transients correct. It supplies actual FIFO samples to
the production analyzer for 48/96/192 kHz × 2048/4096/8192 × early/late bursts.
The CSV records nonzero source energy, FFT count, peak level and FIFO drops.
The dedicated development workflow retains this CSV with its other artifacts.

Run it after building the analyzer test target:

```sh
cmake --build build-eq --config Release --target ChimeraEQTemporalCoverageProbe
```

On Linux, use the repository's native desktop wrapper around that command.
Alternatively run `ChimeraEQAnalyzerTests --temporal-coverage-probe <directory>`.

A larger FIFO alone does not fix this coverage gap. Candidate designs are
bounded overlapping subwindow analysis with explicitly measured extra cost, or
an independently calibrated, anti-aliased analysis stream at a stable sample
rate. The latter can also preserve low-frequency resolution at high host rates.
Both require new measurement and audio-isolation regression; this review leaves
the existing analysis DSP intact instead of substituting an unmeasured design.
For 14 October, demonstrate and record the limitation. For 23 October, require
a measured fix or an explicit limitation of the qualified analysis modes.

### FIFO stall recovery is a separate limit

The unchanged 8192-frame queue holds only 42.67 ms at 192 kHz versus a nominal
40 ms consumer period. A sufficiently long UI stall overflows it, clears the
display history and requires a new complete window. Audio remains independent,
but an intermittently clearing spectrum is a usability failure. Test controlled
UI stalls separately from missing subwindow transients and from audio xruns.
The original 2.6-second observation does not establish sustained operation.

## Low-frequency accuracy and display interpretation

The bin spacing is `sample rate / FFT size`; the Hann noise bandwidth is
approximately 1.5 bins. Neither is a promise to resolve two closely spaced tones.
The graph's interpolation and future horizontal zoom cannot add measured
frequency information.

| FFT at 48 kHz | Bin spacing | Hann noise bandwidth | Observation window |
|---:|---:|---:|---:|
| 2048 | 23.4375 Hz | 35.1563 Hz | 42.6667 ms |
| 4096 | 11.7188 Hz | 17.5781 Hz | 85.3333 ms |
| 8192 | 5.8594 Hz | 8.7891 Hz | 170.6667 ms |

At 8192 points, 96/192 kHz widen the bin spacing to 11.7188/23.4375 Hz.
Low B (approximately 30.87 Hz) and the same string transposed down two semitones
(27.5 Hz) are only about 3.37 Hz apart. The 48 kHz/8192 analyzer is useful for
low-end shape, but is not a qualified discriminator for those fundamentals.
The existing off-bin test permits one entire FFT bin of frequency error.

The measured calibration is coherent sinusoidal amplitude, not noise density
or loudness. Opposite-polarity stereo is measured by averaging channel powers;
a sine in just one of two channels is 3.0103 dB lower than the same sine in both.
The original core tests include DC/Nyquist normalization and finite recovery.
IN/OUT use the same calibration. The left axis is EQ gain, the right axis is
spectrum dBFS. Final EQ OUT precedes output trim and tuner mute, so its absolute
level need not equal the final output meter.

## Movement, layout and CPU

The graph grows from 484 × 212 per bank to 1028 × 242 for the selected bank:
approximately 2.42 times the area, but only 14.2% more height. The graph occupies
31.0% of the 780-pixel full window and has a 4.25:1 width/height ratio. A dedicated
expanded editing view or a collapsible utility area is the next useful layout
change; horizontal expansion alone does not deliver large vertical gain edits.
Do not conflate current editor scaling with low-frequency zoom.

The original local Linux 8192-point **complete analyzer update** p99 is
724.26 µs. It covers FIFO consumption, four FFTs, normalization and smoothing,
and excludes spectrum paths and paint. The 1.495% figure is mean update time
times 25 per second as a one-core equivalent, not measured whole-plugin CPU.
Native panel painting separately has medians around 2.1–2.2 ms on that host.
Original callback and analyzer timing tests record performance; their PASS
status alone does not assert a universal CPU or no-xrun threshold.

Original dedicated macOS artifact measurements show 93–123 ms median observed
FFT progress intervals and approximately 189–204 ms p99 intervals under forced
snapshots; first complete data takes approximately 271–582 ms. FFT/update
computation itself is much shorter (update p99 at most 415.13 µs). These are
test-run observations, not proof of normal DAW presentation speed, but they
prevent claiming consistently smooth 25 Hz behavior on macOS from Linux data.
Measure native dispatch/paint under normal host operation before acceptance.

Audio comparisons in the original tests produce finite bit-identical output
with both EQ banks, capture/FFT changes and deliberate FIFO overflow. The
reported latency stays at 42 samples for the compared configuration: zero
**additional** latency, not zero total plugin latency. Follow-up edits here do
not change `PluginProcessor`, `ChimeraDSP`, released parameter IDs, the EQ
processing class, the analyzer algorithm, or public release identity files.

## Actual original platform evidence

All rows below refer to `f1e088`, not to the subsequent review fixes. Rebuilt
source results and artifact links are maintained in PR #52 and its checks.

| Platform | Dedicated run 38208784048 | What can be concluded |
|---|---|---|
| Linux, Ubuntu 22.04 | 4/4 suites pass, 116.17 s; job 114678077187 | Original automated numerical/native contract coverage completed. |
| Windows | UI test compilation fails with C2589 | No completed four-suite acceptance or dedicated Standalone preview from this run. |
| macOS 15 | UI suite fails at callback sample collection | Earlier live/calibration/motion checks ran. Later A/B/project/scaling/close checks did not run in that original suite. |

Sources: [dedicated EQ run](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38208784048),
[signal/EQ run](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38208784005),
[candidate run](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38208784071),
[full build run](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38208783979).
The same Windows test error and macOS sample-collection failure recur across
these workflows; repeating them without changing the causes is not useful.

## Independent comparison with Pro-Q workflow

The comparison uses FabFilter's official documentation, not its code, assets,
filter coefficients or visual design. The objective is an efficient
observe/select/edit/compare/recover workflow in Chimera's own UI.

| Workflow | Implemented in #52 | Still required |
|---|---|---|
| Observe EQ input/output | Independent IN/OUT, real resolution choice, time smoothing, separate axes | Qualified high-rate coverage, range/tilt/precise readout, low-frequency zoom |
| Select and edit | Existing 12 persistent bands per bank, frequency/gain drag, wheel Q, numerical fields, seven filter choices | Graph add/delete with stable slot identity; fine/axis-constrained editing; group editing |
| Capture a useful observation | LIVE suspends capture and clears history; it is not Freeze | Snapshot Freeze with explicit frozen state and fresh resume; separate Peak Hold/reset |
| Recover and compare | Existing global A/B and host parameter/state integration | EQ Undo/Redo, later EQ-local A/B/copy and audition |
| Work at useful scale | One selected-bank workspace; existing 75–150% scale | More vertical editing room and physical-DPI readability acceptance |

Pro-Q's documented Freeze accumulates maxima over time. #53's planned Snapshot
Freeze can intentionally hold one complete frame; define that separate behavior
instead of accidentally merging it with Peak Hold. Spectrum Grab also includes
peak labels and creating a new Bell band, so it is more than a frozen trace.
Pro-Q display tilt is a presentation convention, not a required Chimera audio
response. Numeric Q values need not imply identical curves in different EQs.

Official references:

- [Spectrum analyzer](https://www.fabfilter.com/help/pro-q/using/analyzer)
- [Display and workflow](https://www.fabfilter.com/help/pro-q/using/eqdisplay)
- [Band controls](https://www.fabfilter.com/help/pro-q/using/bandcontrols)
- [Spectrum Grab](https://www.fabfilter.com/help/pro-q/using/spectrumgrab)
- [Undo/Redo and A/B](https://www.fabfilter.com/help/pro-q/using/undoredo)
- [Resizing and scaling](https://www.fabfilter.com/help/pro-q/using/fullscreenandresize)

Dynamic/spectral EQ, additional phase modes, sidechains and selectable filter
slopes remain later DSP work. Their absence is not a regression in this P0 PR.

## Milestone evidence required

These are review acceptance recommendations, not a change to #50's calendar.

| Area | 14 October prototype review | 23 October test installer |
|---|---|---|
| Identity and scope | Exact reviewed source, native captures, reproduced defects, implemented/remaining list | Installer and plugin artifacts bound to qualified source and preview identity |
| Platforms | Inspect fresh Windows/macOS/Linux four-suite results; no inferred passes | Platform build/install/launch/uninstall checks for each offered package; no public-beta overwrite |
| Numerical display | Sine gain/cut, anti-phase, silence, off-bin low tones, reset/overflow and temporal coverage evidence | All offered rates/FFT modes qualified; high-rate coverage fixed or explicitly restricted |
| Motion and graphics | Native burst/stop/reopen/bank-switch capture; offered scale screenshots with readable labels and working hit-tests | Actual guitar/bass DI, low-B/down-two-semitone cases, normal DAW paint cadence and physical DPI review |
| Editing and compatibility | Existing 24 slots, numeric/drag/Q/overlap, host gestures, A/B, old/new project recall | #53 minimum: Snapshot Freeze, stable-slot add/delete and coherent Undo/Redo; repeat ID, migration, automation and recall tests |
| Performance | Separate analyzer/path/paint/callback timing and actual producer rate; retain failures | Proposed minimum: 30-minute 96/192 kHz sessions at the minimum supported buffer, editor open/closed, controlled UI stalls and multiple instances; retain xruns, callback tails, drops and recovery counts |
| Audio and host | Repeat finite bit equality and unchanged latency for display-only changes | Verify installed VST3 in Studio One and the other supported host/OS matrix, project close/reopen and shutdown; listening acceptance |

The proposed 30-minute session duration is a minimum review sample, not a proof
of unrestricted stability. The hardware/host matrix and pass budgets must be
recorded before measurement; a zero-overrun short run cannot establish a general
guarantee. A CI Standalone executable is not the 23 October installer gate.

#53's 30 October list remains separate: multiselection/group edits, momentary
audition, EQ-local A/B/copy, Peak Hold, display range/tilt/readout/low-frequency
zoom and fine/axis-constrained editing. Do not report those as implemented by
this review's fixes.
