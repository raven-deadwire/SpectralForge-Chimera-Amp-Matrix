# Chimera 1.3.1 static graphical EQ baseline

Delivery note, **2026-10-12 KST**: the static EQ foundation described below is
included in public
[1.3.1-beta.1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.3.1-beta.1)
at product source 7b20cd9d1f90e3e2969c250712b199e893cf50cd, merged by PR #48.

The owner subsequently requested substantially better analyzer presentation
and the missing editing workflow. Those are explicitly tracked in
[EQ analyzer and workflow upgrade](EQ_ANALYZER_WORKFLOW_UPGRADE.md) and the
[remaining development schedule](DEVELOPMENT_ROADMAP_2026_2027.md).
The earlier prototype originated on codex/1.3.1-graphical-eq from 1.3 preparation
source b7e4f18efa295f0babf2278ca63117fbccb54143; the details below describe
that delivered foundation, not completion of all FabFilter reference features.
Native-host and listening acceptance retain their recorded scope.

## Implemented

| Feature | Behavior |
| --- | --- |
| Tone EQ | After merged rigs and post-rig gate, before the POST rack |
| Final EQ | After POST and stereo doubler/utility processing, before output trim and tuner mute |
| Instances | Two independent banks, each with twelve persistent nodes |
| Controls | Band selector, graph Hz/dB drag, wheel Q, editable Hz/dB/Q boxes, band enable, filter choice, module bypass |
| Filters | Bell, low/high shelf, 12 dB/octave high/low pass, notch, constant-peak band pass |
| Host state | 122 appended APVTS parameters, stable IDs, AU version hint 10, automatable, A/B and project snapshots |
| FFT | Independent input/output spectra per EQ, 2048-point Hann, power-averaged stereo; opposite polarity does not cancel |
| CPU | EQ-only callback budget average/peak in each panel and diagnostics; pair benchmark CSV |

No old host ordinal, ID, range or AU version hint is repurposed. EQ starts
bypassed with disabled bands. Missing EQ parameters in older projects and A/B
slots use definition defaults rather than dirty current-session values. Existing
factory presets remain bypassed. PRE graphic EQ and POST character EQ remain.

Graph drag edits frequency and gain with balanced host gestures. Q uses a wheel
gesture or numeric input. Gain is retained but disabled for pass, notch and band
pass types, since their gain is determined by the filter shape. Overlapping
nodes remain individually reachable through the twelve-band selector.

## DSP and UI boundaries

The filters are independently authored double-precision RBJ biquads, not
FabFilter code or a claimed Pro-Q 4 replica. Host ranges are 20–20,000 Hz,
−24–+24 dB, Q 0.1–18. Frequency is internally capped below Nyquist at low sample
rates. Frequency/gain/Q use 15 ms ramps on a 32-sample control clock. Enable and
module bypass use 10 ms wet ramps; type changes crossfade separate filter states.
Settled bypass is an exact dry copy. IIR poles contribute a conservative tail
estimate and do not add latency.

The audio callback reads atomic parameters and processes fixed-size states.
The bounded 8192-frame SPSC FIFO transfers paired stereo input/output samples;
when full it drops new analyzer frames without waiting. FFT and graph building
run only on the message thread. Panels refresh at 25 Hz, repaint only when their
data changes, and suspend capture/analysis when hidden or destroyed. FFT is a UI
preference, not an automatable sound parameter. Analyzer drops affect display
continuity only. No callback locks, FFTs or ordinary heap allocations are added.

CPU percentages describe elapsed EQ time as a fraction of the audio buffer
deadline. They are not whole-computer utilization. UI idle CPU and FFT work are
measured separately. Shared CI runners are not certified audio workstations.

## Validation

`ChimeraGraphicalEQTests` checks real filter responses, four sample rates,
mono/stereo, callback allocation instrumentation, rapid automation, state and
factory defaults, and production signal placement in Classic, Dual blend,
Dual crossover and Matrix. The placement oracle processes captured Tone output
through the existing POST rack and stereo utilities and compares it with actual
Final input. `ChimeraGraphicalEQUITests` uses a real native peer, edits all 24
numeric nodes, verifies drag/wheel host gestures and stereo FFT calibration,
renders 75/100/125/150% screenshots, and checks hidden/editor-close suspension.
`ChimeraGateProcessorTests` retains the frozen legacy parameter contract.

The dedicated `graphical-eq.yml` checks exact PR head source on Linux, Windows
and macOS; existing product and CAB workflows stay enabled with their existing
gates. Results must be read per run, not inferred from another branch's success.
See `GRAPHICAL_EQ_1_3_1_VALIDATION.md` for measured results and outstanding checks.

## Follow-up scope, not implemented

Dynamic EQ, compressor/limiter expansion, independent four-band dynamics,
linear-phase/zero-latency mode selection, per-band stereo/M-S processing,
external sidechains and automatic EQ matching are not implemented here.
Dynamic EQ belongs to 1.3.2 and independent multiband dynamics to 1.4.0 under the
current roadmap. The October follow-up adds analyzer controls, larger focused
editing, plot-based band creation/removal, multi-selection, audition and EQ
Undo/Redo; these are not implemented by this baseline.
Native DAW loading and instrument listening remain separate release acceptance.
