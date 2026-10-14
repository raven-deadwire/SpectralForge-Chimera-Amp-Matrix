# Focused EQ and live analyzer prototype

Date: 2026-10-12 KST. Development work based on public-beta main
`3817246f779a50f19b5378ec9661a2275e6b91b2`.

This is the first implementation stage of
[the EQ workflow specification in PR #50](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/50).
The specification revision inspected for this implementation is
[`ceb9c1a9`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/ceb9c1a9510a733a05618d65f6eecb934b504f86/docs/EQ_ANALYZER_WORKFLOW_UPGRADE.md).
This development branch is separate from the published `v1.3.1-beta.1` tag,
assets, release feed and website. It is not acceptance of the complete October
23 or October 30 scope.

## Implemented scope

- Select Tone EQ or Final EQ directly. The selected bank uses the complete
  1140 by 412 logical workspace. Its graph is 1028 by 242, compared with the
  former 484 by 212 graph in each compulsory split panel.
- The inactive view stays alive to retain its band selection and display
  preferences. Only the selected visible bank captures and analyzes samples.
  Both EQ banks continue their existing audio processing regardless of view.
- Independently show IN and OUT. A dashed cyan input outline and solid amber
  output outline accompany restrained gradient fills. The summed EQ response
  is drawn above both spectra. EQ gain and spectrum dBFS have separate axes.
- Use the existing single-band inspector for frequency, gain, Q, filter type
  and enable, preserving numeric entry, graph dragging and wheel Q gestures.
- Select a real 2048, 4096 or 8192-point Hann FFT and Fast, Balanced or Smooth
  temporal response. Resolution changes start a fresh observation; changing
  response speed retains the current observation.
- The EQ tab returns to the previously selected bank. Classic/Dual/Matrix
  changes and utility-path navigation retain that chosen bank.

No additional or renamed audio parameters, band-slot migration, new filters,
EQ-local comparison or sound-state fields are introduced by this stage.

## Analysis and latency design

The existing paired stereo SPSC tap remains the only audio-callback work.
It copies each bank's actual input/output samples into a bounded 8192-frame
queue. The producer, filter processing, parameter layout and sound-state code
are unchanged. Two consumer-only FIFO inspection/discard methods were added.

The selected panel's existing 25 Hz message-thread timer consumes one bounded
queue snapshot and performs at most one set of four transforms. Each set
analyzes L/R separately at IN and OUT, then averages channel powers. An
opposite-polarity stereo signal therefore remains visible. The analyzer uses
the newest complete rolling window, with overlap after startup, instead of
replaying a queue of old display frames. A detected overflow, sample-rate
change, view change or resolution change clears partial observation history.

FFT plans and sample, transform and power arrays are allocated before analysis.
FFT, temporal smoothing, logarithmic pixel aggregation and path construction
run outside the audio callback. Painting consumes cached paths. EQ response
paths are rebuilt only when EQ parameters or sample rate change, rather than
when the audio CPU meter fluctuates. Whole-editor repaint cadence is unchanged.

| Resolution | FFT size | Bin spacing at 48 kHz | Hann noise bandwidth | Observation window |
|---|---:|---:|---:|---:|
| Fast | 2048 | 23.4375 Hz | 35.1563 Hz | 42.6667 ms |
| Normal (default) | 4096 | 11.7188 Hz | 17.5781 Hz | 85.3333 ms |
| Detailed | 8192 | 5.8594 Hz | 8.7891 Hz | 170.6667 ms |

These are analysis quantities, not added audio latency. The first display
needs a complete window plus the next timer opportunity. Once primed, all
three resolutions can update at the same 25 Hz cadence. Larger windows spread
transients over more observed time. Log-frequency interpolation makes the
display continuous between measured bins; it does not create additional
low-frequency resolving power. Pixel aggregation retains contained bin peaks.

Calibration is coherent sinusoidal amplitude: a bin-centered full-scale sine
in both channels is 0 dBFS; the same signal in only one of two channels is
-3.0103 dBFS. DC and Nyquist use single-sided endpoint normalization. Broadband
noise levels depend on FFT noise bandwidth; this is not a loudness meter or
a noise-density display.

| Response | Attack | Release |
|---|---:|---:|
| Fast | 8 ms | 120 ms |
| Balanced (default) | 35 ms | 300 ms |
| Smooth | 90 ms | 650 ms |

Smoothing averages power with a time coefficient derived from consumed sample
time, independently of the number of UI callbacks. The first complete frame
initializes the display. If audio production stops entirely, a 120 ms grace
period precedes release decay without further transforms; resumed input needs
a fresh complete window. The deliberate LIVE switch suspends capture.
IN/OUT visibility controls only the respective traces.

Final EQ OUT is the output of that EQ **before output trim and tuner mute**.
It is not a replacement for the main output meter.

## Verification evidence

The local Linux Release build and all four targeted suites passed across
recorded runs: existing Gate/EQ contracts, the new numerical analyzer suite,
and the native editor suite. Exact source hashes, original logs/JUnit statuses,
CPU CSVs and a timing-correct animation are in the
[measurement report](evidence/eq-analyzer-prototype-20261012/README.md).

The whole processor retained its existing 42-sample host latency with
bit-identical output when capture, FFT size and FIFO overflow changed.
At 48 kHz, measured complete analyzer-update medians were 134.21, 233.95 and
581.78 microseconds for 2048, 4096 and 8192 respectively. These exclude drawing.
The native suite verified both banks and all resolutions, independent trace
visibility, numeric/host/state behavior, and 75–150% layout.

The 32-frame motion proof drives the complete production processBlock route
with a synthetic harmonic pluck; global audio meters and actual FFT spectra
respond together. No physical device, DAW, real instrument DI or finished
October 23 test installer is claimed. Measurement definitions and remaining
platform/host acceptance are explicit in the linked report.

### Reproduce the development checks

Use the platform dependencies in `.github/workflows/graphical-eq.yml`, then:

```sh
cmake -S . -B build-eq -DCMAKE_BUILD_TYPE=Release -DCHIMERA_EQ_DEVELOPMENT_PREVIEW=ON
cmake --build build-eq --config Release --target ChimeraGraphicalEQTests ChimeraGraphicalEQUITests ChimeraEQAnalyzerTests ChimeraGateProcessorTests --parallel 2
python3 Tools/run_linux_ui_tests.py -- ctest --test-dir build-eq -C Release -R '^(ChimeraGraphicalEQ.*Tests|ChimeraEQAnalyzerTests|ChimeraGateProcessorTests)$' --output-on-failure --no-tests=error --output-junit eq-results.xml
```

On Windows/macOS run the same `ctest` command directly. The dedicated workflow
retains timing CSVs, native captures, JUnit output and build identity. After its
Windows checks pass, it also builds a separate Standalone preview artifact.
This is a development executable, not the October 23 test installer.

`CHIMERA_EQ_DEVELOPMENT_PREVIEW` is opt-in and defaults to `OFF`. The focused
workflow enables it to display `1.3.1 EQ Preview <revision>` and identify its
package as `1.3.1-preview.<revision>`. The frozen `VERSION`, `RELEASE_CHANNEL`
and publication policies remain unchanged.

## Remaining work and ownership boundary

| Stage | Work still required |
|---|---|
| Remaining October 23 minimum | Snapshot Freeze; persistent-slot graph add/delete; EQ Undo/Redo. Five-block signal navigation is separate work. |
| Remaining October 30 editing/analysis | Multi-selection; momentary band audition; EQ-local A/B/copy; peak hold; display range/tilt; readout/low-frequency zoom; fine/axis-constrained editing. |
| Later processing | Dynamic EQ; selectable HP/LP slopes; channel/sidechain and phase-processing extensions. |
| Product acceptance | Actual instrument DI/listening and DAW-host acceptance; qualified package/platform testing and release gates. |

Disabled/bypassed existing slots remain visible and reachable in this prototype.
The future occupancy migration must distinguish a deleted slot from a bypassed
one without renumbering any released parameter. No inactive controls for those
future features are exposed here. Weekly version reservations remain owned by
PR #50's release calendar; this prototype does not reserve or publish a new
public beta tag.
