# Independent Transpose follow-up (2026-10-18 KST)

Research executables only. No production source, parameter, state, preset,
automation ID, routing, bypass, PDC, release policy, or published package changes.
No Neural DSP binary/source was available or used. Do not label this work a
reproduction of Neural DSP's algorithm or a released Transpose fix.

## Build and reproduce

Requires Python with NumPy/SciPy, GCC 13+ (C++20), zlib development headers,
and JUCE 8.0.8 at `d6181bde38d858c283c3b7bf699ce6340c050b5d`.
From the repository root:

```sh
python Tools/TransposeNext/build.py --juce /path/to/JUCE --out build/transpose-next
build/transpose-next/CorrelationTests
build/transpose-next/PitchRegressionTests
build/transpose-next/StabilityTests
build/transpose-next/GateTests
python Tools/TransposeNext/evaluate.py --renderer build/transpose-next/TransposeProbe --out build/transpose-next/detail
python Tools/TransposeNext/evaluate.py --renderer build/transpose-next/TransposeProbe --out build/transpose-next/matrix --matrix
```

Do not run the functional tests/build concurrently with the dedicated timing
comparison. Detail/matrix callback times are diagnostic wall time, including
scheduling disturbances, not audio-driver callback certification.

## Candidates

- `baseline`: actual released `Source/PolyPitch.h`, unchanged.
- `relative`: unchanged research control from PR #45, commit
  `ff9ca84c1cb53eaf7a054965ec4eee5e084aa60f`, `Tools/Transpose/RelativePhasePitch.h`.
- `corr10`: independent variable-rate read head with bounded 10 ms history age,
  waveform-similarity splice search and 2 ms crossfade. A bound on read-head age
  is not a measured output delay and does not imply correct transposition.
- `corr40`: same independent engine with a 40 ms age bound. The longer search can
  cover a low-B period. It is still **variable delay**, not a fixed PDC contract.
- `corr40-onset`: same 40 ms engine, with optional downshift-only attack reset.
  A fast/slow high-band energy detector reanchors the read head with a crossfade.
  The entire waveform is still resampled. There is no dry tonal attack bypass.
  Upshifts retain the unmodified correlation engine. Missed/false detections,
  gain dependence, dense chords and continuous note changes remain risks.

Search uses both channel energies without summing opposite-polarity waveforms.
All vectors are prepared off the process path; processing has no I/O, locks,
FFT, allocation or external calls. The search has bounded but bursty work;
measure callback tails rather than inferring cost from average complexity.
Linear interpolation is an experimental choice: high-frequency upshift alias
rejection has not been qualified. Do not deploy this core as a full-range shifter.

## Measurement contracts

Inputs are deterministic synthetic float32 stereo. Neither note labels nor known
frequencies enter the native engine. The offline scorer alone knows target notes.
Sine, eight-harmonic B0, weak fundamental, close dyad, eight impulse phases and
75/125 ms repeated plucks are distinguished from actual instrument DI.

`legacy_metrics.py` preserves the PR45 tone/impulse metric functions, recovered
from `Chimera_Transpose_Comparison_2026-10-13.zip` (SHA-256 recorded in the report).
The unused original CLI was removed. Tone metrics use two seconds after startup:
dominant frequency, coherent target amplitude, broad local fundamental power,
and a 6 Hz demodulation warble proxy. Target attenuation or neighbouring notes
can make that proxy misleading. Ideal shifted controls must pass before using
each metric family; the close dyad is diagnostic and blocked if its ideal control
fails. These are not onset/acquisition-time estimators.

Impulse delay includes the full event's energy centroid and 5–95% distribution.
The attack scorer separately finds a 2 ms RMS envelope peak in the 1–8 kHz band,
within 70 ms after each known onset. Report input-envelope bias and variable event
jitter; do not subtract the nominal delay to conceal it. Valley/peak is a high-band
separation proxy, not proof of perceived bass clarity. Read-head min/max are
internal provenance and must not replace acoustic/event latency measurements.

The provisional screen is pitch within 10 cents, warble <=20 cents, coherent and
local fundamental within 1 dB, impulse spread <=2 ms, and delay <=10 ms. These
research screens do not modify existing release approval rules. Missing real DI,
DAW PDC/automation/recall and controlled hardware testing remain unqualified.

## Public technical basis

General resampling, ring-buffer splicing and waveform similarity are public DSP
techniques. Haghparast/Penttinen/Välimäki, DAFx 2007, explains the latency/period
trade-off for monophonic signals: <https://dafx.labri.fr/main/papers/p007.pdf>.
This implementation uses its own normalized squared error, causal IIR detector,
search bounds, stereo linking, crossfade and optional onset rule, not copied code.
The paper is a conceptual reference, not evidence about Neural DSP internals.

Neural DSP's official Gojira X page confirms a +/-12 semitone Transpose feature:
<https://neuraldsp.com/plugins/archetype-gojira>. It does not provide the raw
measurements needed to claim a measured latency or algorithm equivalence here.
