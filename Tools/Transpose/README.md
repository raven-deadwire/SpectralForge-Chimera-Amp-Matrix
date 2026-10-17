# Issue #8: isolated Transpose laboratory

This standalone build never adds a product parameter, changes a factory preset,
alters Classic/Dual/Matrix routing, or enters the release build. It compares the
**actual production** `Source/PolyPitch.h` with explicitly experimental engines.
`RelativePhasePitch.h` is a research-only snapshot of production at
`aa9e7b9a9f3889f1e42d83de264491e673c38018`, with only the output-bin phase locking
rule changed. Do not replace the production shifter with any of these candidates
on the strength of the synthetic results.

## Pinned dependencies

- JUCE 8.0.8: `d6181bde38d858c283c3b7bf699ce6340c050b5d`
- Signalsmith Stretch: `a670068d9aeb64913331d5cc29337b19a457a7df`
- Signalsmith Linear 0.6.4: `de55e6a50ffcf6f8f43f649692d94691c7025151`
- Python: NumPy and SciPy (versions in the result metadata).

The external engines are comparison dependencies, not bundled with Chimera.
Use local checkouts at these revisions; the lab does not fetch or modify them.

```sh
cmake -S Tools/Transpose -B build/transpose -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DJUCE_SOURCE=/absolute/path/to/JUCE \
  -DSTRETCH_SOURCE=/absolute/path/to/signalsmith-stretch \
  -DLINEAR_SOURCE=/absolute/path/to/signalsmith-linear
cmake --build build/transpose -j 3
ctest --test-dir build/transpose --output-on-failure
python Tools/Transpose/measure.py \
  --renderer build/transpose/TransposeLab_artefacts/Release/TransposeLab \
  --work build/transpose/detail-audio --report build/transpose/detail.json
python Tools/Transpose/measure.py \
  --renderer build/transpose/TransposeLab_artefacts/Release/TransposeLab \
  --work build/transpose/matrix-audio --report build/transpose/matrix.json --matrix
```

The default suite contains 103 renders, including 3 gate settings. The matrix
contains 405 renders: five engines × 3 rates × 3 block sizes × 9 intervals.
The imported PitchRegression, Stability and Gate CTests run unchanged repository
tests. They do not turn research engines into approved product implementations.

## Engines

| ID | Configuration | Purpose |
|---|---|---|
| `baseline` | Actual production PolyPitch | Fixed reference |
| `relative` | Same full window/hop; lock to mapped source phase differences | Isolate transient phase loss |
| `signalsmith-matched` | Same window/hop, no split-computation latency | Compare another spectral reconstruction |
| `signalsmith-default` | Upstream default 120 ms block / 30 ms interval | Longer-window quality control |
| `dual-delay-8ms` | Two causal interpolated read heads, 8 ms range, two-sample minimum | Low-delay time-domain negative control |
| `wrapper` / `bypass` | Actual Transposer, processor-style nonzero enable rule | Confirm bypass and active settled delay |

**No reduced-FFT-window candidate is used.** The dual-delay control has a variable
read age, not a reportable fixed latency. Its impulse delay cannot establish a
usable pitch shift; check the frequency measurements before interpreting it.

## Inputs and metrics

All inputs are generated, not recordings. Stereo is `R = -0.65 L`, as in the
existing pitch regression. The 27.5 Hz / 0.2-amplitude sine is extended from the
existing 1.5-second regression to 3 seconds for analysis. The separate 30.867706 Hz
fixture is an analytical B0 example, **not an assumption about the user's string,
fret or tuning**. Harmonics use eight specified partials. Synthetic decays and
75/125 ms repeated attacks are additional diagnostic fixtures, not Azhi Dahaka.
Input SHA-256 and frame count identify the exact shared input of every comparison.

- **Latency**: declared core latency; measured energy centroid relative to each
  of eight well-separated impulses at different hop phases. Also record the
  5–95% energy interval. A pitch shifter is not an LTI pure delay.
- **Smear**: width of the impulse 5–95% energy interval. This is not subjective
  attack quality, and cannot establish note separation or pitch accuracy alone.
- **Target gain**: Hann-windowed coherent projection at the exact shifted
  fundamental, analysis 0.75–2.75 s. A large loss can mean detuning or modulation,
  not destruction of all low-frequency energy.
- **Local fundamental-band gain**: power within 0.8–1.2 times the target frequency,
  referred to input fundamental RMS. Report this alongside target projection.
- **Level-normalized target gain**: target gain minus overall RMS gain. The raw
  WAVs retain their levels; do not silently peak-normalize the measurements.
- **Dominant pitch**: Hann projection search within 0.65–1.4 times target frequency.
  Strong modulation can produce multiple peaks; do not treat one as a stable note.
- **Warble proxy**: p95–p5 instantaneous cents from 6 Hz low-passed complex
  demodulation, excluding 400 ms at both analysis edges. It is unreliable as a
  standalone measure if the intended component is weak or detuned. Amplitude
  modulation is the corresponding p95/p5 magnitude ratio.
- **Repeated attacks**: 5 ms RMS envelope valley/peak ratio per expected event.
  Compensation uses nominal core delay (4 ms only as an approximate display
  alignment for the variable-delay control). Lower residual is better separation,
  but an untransposed control can score well; use pitch accuracy as a separate gate.
- **Gate**: actual NoiseGate/PostRigGate with Full range. Default parameters
  (-65 dB, 80 ms release, 20 ms hold), moderate (-36/60/15) and deliberate stress
  (-24/5/0), each pre/post. Post applies the clean-input envelope delayed by core
  latency. No amp/cab, saturation or host is included in these chain renders.
- **CPU**: core and gate-plus-core wall time, mean/p95/p99, max and deadline-miss
  count. This shared Linux container is not controlled hardware; counts are
  observations, not a real-time or DAW acceptance gate. File I/O and metric
  extraction are outside the timed processing section.

Native tests additionally exercise bypass, transitions, warm history, mono/stereo,
nonstandard block sizes and post-gate envelope alignment. Real instrument DI,
polyphonic guitar, slides, amp/cab interaction, host PDC/automation/project recall,
commercial DAWs and end-to-end roundtrip remain separate unmeasured requirements.
