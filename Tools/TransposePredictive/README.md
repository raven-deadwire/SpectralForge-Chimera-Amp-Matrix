# Predictive spectral Transpose research — issue #97

Independent, non-production experiment. No Neural DSP binary, source code or
measured reference output was available. This is not reverse-engineering
completion, a released fix, a fixed-latency engine or a passed musical trial.

## Reproduce

Dependencies: GCC 13+, Python 3 with NumPy/SciPy, zlib development files and JUCE
8.0.8 at `d6181bde38d858c283c3b7bf699ce6340c050b5d`.

```sh
python3 Tools/TransposePredictive/build.py --juce /path/to/JUCE --out build/predictive
build/predictive/PredictiveTests
build/predictive/PitchRegressionTests
build/predictive/StabilityTests
build/predictive/GateTests
python3 Tools/TransposePredictive/evaluate.py --renderer build/predictive/Probe --out build/predictive/evidence
```

The benchmark has 144 renders: four engines, nine fixtures, four shifts at
48 kHz/128 samples. Native processing receives only audio and the shift, never
fixture frequencies or note labels. 20 ideal-tone controls test the scorer;
a failed control invalidates that metric family's PASS, never relaxes a threshold.
All tones are scored after settling, so those scores do not certify note-onset
or reacquisition performance. Synthetic 75/125 ms plucks are not Azhi Dahaka DI.

## Model and limitations

An original, bounded 64-track oscillator bank uses causal Hann FFT analysis at
N=1024/2048/4096, H=128. Peak interpolation estimates amplitude; inter-frame phase
differences estimate frequency. Tracks are associated by nearest frequency and
retain synthesis phase. New tracks extrapolate input phase from the past window
centre to the next output sample. Only the shifted oscillators are output.
There is no unshifted dry-tone transient path and no future-sample access.

There is also **no residual/noise model**, and the magnitude estimate still
depends on past audio. Consequently immediate synthesis does not imply a 0 ms
or <=10 ms audible delay. `latency()` returns -1 (undefined fixed PDC), and
centroid, impulse width, attack delay/loss and pitch results are separate.
The current bank is an experiment in separating spectral resolution from output
buffer scheduling. It has not qualified bass attacks, chords, host automation,
PDC, audible transitions, full-range shifting or realtime callback deadlines.

Memory is reserved in prepare. C++ new interception checks the candidate process
path; it is not a proof that all possible allocators/host activity are absent.
Callback timings are diagnostic shared-machine wall time, recorded under other
load, and **must not be interpreted as controlled CPU acceptance**.

## Scoring and audit

`legacy_metrics.py` retains PR #45's unchanged fixture and tone/impulse functions,
recovered from `Chimera_Transpose_Comparison_2026-10-13.zip`; only its CLI is
omitted. The package SHA-256 is in the report. Tone measures retain absolute gain:
coherent target projection, local target-band energy, nearby dominant-frequency
fit and 6 Hz demodulated warble. A low target amplitude invalidates interpretations
of a pleasing frequency number. Joint provisional screens prevent that shortcut.
The close dyad is measured by separate target projections, not the single-tone
warble scorer, because adjacent notes interfere with its demodulation passband.

`out_of_target_power_fraction` is power outside +/-2 Hz of expected partials
between 15 Hz and 16 kHz in the final 2 seconds. It includes detuning/modulation
and is not a perceptual audio-quality score. The attack scorer compares 1–8 kHz,
2 ms RMS envelope peaks with the input peak (including input bias); events losing
over 20 dB are marked unusable for timing. Impulse energy loss is also retained.
RMS-matched WAVs are secondary auditions; absolute WAVs/gain scores remain primary.

## Real DI and licensed reference capture

Provide a native 48 kHz isolated, pre-effects DI of Azhi Dahaka's Low B passage,
including a short lead-in, repeated attacks, sustained notes and note changes.
For lawful external Neural DSP measurement, record exact product/version,
plugin format, OS/DAW, buffer/PDC, input/output gain, Gate and every other module's
bypass state, transpose value, original DI hash and render hash. Capture bypass,
0, -2, -1, +1, +2 with identical routing and no normalization. Record reported
latency separately from measured event delay and an actual loopback roundtrip.
No decompilation, protection bypass or proprietary-code copying is part of this
workflow. Captures are required before claiming any Neural DSP equivalence.

```sh
python3 Tools/TransposePredictive/render_wav.py --input /path/to/DI.wav \
  --renderer build/predictive/Probe --out build/predictive/di \
  --description 'Azhi Dahaka, instrument/tuning/take, pre-effects DI provenance'
```

This creates absolute and RMS-matched local renders and hashes. Operator-provided
descriptions do not automatically verify DI provenance or confer listening PASS.
Do not publish the user's private raw recordings without separate authorization.

## Public conceptual sources

- Gómez et al., DAFx-2003, *Content-Based Melodic Transformations of Audio Material
  for a Music Processing Application*, section 4.2.1: general sinusoidal/residual
  modeling and pitch transformation, https://www.dafx.de/paper-archive/2003/pdfs/dafx41.pdf
- Wells and Murphy, DAFx-2006, *High Accuracy Frame-By-Frame Non-Stationary
  Sinusoidal Modelling*, https://www.dafx.de/paper-archive/2006/papers/p_253.pdf
- Neural DSP official Darkglass Ultimate manual, utility-bar Transpose control:
  https://neuraldsp.com/manual/darkglass-ultimate . This is feature documentation,
  not independent latency, quality or internal-algorithm evidence.

The code is independently written from general DSP principles. It does not
implement either referenced paper verbatim and is not claimed to match Neural DSP.
