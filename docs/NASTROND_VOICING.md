# Náströnd reference-informed voicing checkpoint

2026-10-05 KST. Development-only follow-up to the [26-NAM baseline](NASTROND_REFERENCE_COMPARISON.md)
at `87683f753a18de96102bc9676fa5800a7e83aa81`. This is one original amp, not five
hardware clones. Default GAIN remains **7.2**, and all five authored preset gain
values are retained. Production catalog/routing, actual instrument-DI listening
and release acceptance remain pending.

## Changes and reason

The baseline already compressed weak input strongly, but its harmonic balance
differed materially from the head references. Increasing the same drive was not
enough to resolve that difference. This revision changes the response around
the distortion stages and adds a bounded broad output contour:

- IMPACT retains its low-frequency contribution after power saturation and uses
  an input-relative attack detector, with a floor to avoid amplifying near-silence.
  The additional body path excludes subsonic content.
- ROT uses each stage's bias polarity and bounds blocking-induced bias shift.
  It changes even-harmonic content and recovery rather than acting as a volume control.
- BLOOM broadens interstage low-frequency response and reduces the old fixed
  160 Hz, long-decay resonance. Its resonator now spans 95–135 Hz with a 4–44 ms
  time constant; CLANK also participates in interstage tightening.
- The fitted contour is a 100 Hz low shelf **+1.95 dB**, 500 Hz bell **−2.11 dB**
  (Q 0.65), and 2 kHz high shelf **+6 dB**. Coefficients are computed at the actual
  oversampled rate. The fit was bounded to ±6 dB; it does not try to force a clone.
- A fixed **0.886** output multiplier preserves baseline default pluck RMS
  independently of input/stage drive. Extreme EQ/depth/master settings exposed
  peaks above the previous headroom guard, so output above magnitude 1.5 has a
  continuous-slope soft knee bounded by 3.5. The measured default and preset
  outputs remain below the knee.

## Fit protocol and independent comparison

The structural revision was rendered before adding the contour. Only its
synthetic pluck segment was fitted, using 42 logarithmically spaced bands from
65–8000 Hz and equal total weight for each of four head-reference families.
Each reference has an independent output offset; no reference input gain was
invented. The selected set contains 11 files: VH4 CH3/4 (3), unboosted Twin Jet
CH2 (2), uploader-labeled Granophyre (3), and max-gain Matamp (3). Related NAM
training variants share their family's weight. Meshuggah amp+cab captures,
boosted Twin Jet, CH1 crunch and half-gain Matamp were excluded from the fit.

The synthetic chord segment was not used to choose the coefficients. The
implemented DSP was then rerendered through the real JUCE 4x wrapper for all
26 NAM references and 17 Original states, including the additional VH4
metadata-adjusted comparisons. The table uses the same RMS-matched Welch
spectrum metric as the baseline, averaged per selected family:

| Reference group | Baseline chord difference | Revised chord difference | Reduction |
|---|---:|---:|---:|
| VH4 CH3/4 | 6.87 dB | 4.17 dB | 39% |
| Twin Jet CH2, unboosted | 6.86 dB | 3.92 dB | 43% |
| Granophyre, provenance unverified | 8.44 dB | 4.79 dB | 43% |
| Matamp max gain | 5.20 dB | 3.37 dB | 35% |

These are smaller differences between authored tones, **not fidelity percentages**.
Residual pluck differences remain substantial. A macro's low-frequency or
temporal role is not equivalent to moving every spectral score monotonically
toward its named reference. Meshuggah still cannot establish amp-only spectral
parity; Granophyre's physical provenance and four families' input calibration
are still missing. See the unchanged [source manifest](reference/nastrond-nam-manifest.json).

## Gain and regression observations

400 Hz, identical digital input; output growth uses −48→−24 dBFS peak and THD
uses harmonics 2–12 at −36 dBFS. Output is not matched for these measurements.

| State | Output growth for +24 dB input | THD |
|---|---:|---:|
| Default | 1.30 dB | 59.9% |
| Fenrir | 1.77 dB | 54.7% |
| Surtr | 0.53 dB | 64.8% |
| Níðhöggr | 1.76 dB | 57.5% |
| Fimbulvetr | 2.63 dB | 51.5% |
| Ragnarök | 0.72 dB | 62.7% |

This verifies retained weak-input saturation in these probes. The THD change
from the baseline default's 40.0% is affected by filtering; it is not a claim
of 50% more gain or a substitute for the owner's rhythm/lead feel.

Local CTest passed **3/3**: core, actual JUCE integration, and renderer. Coverage
includes 12 sample-rate/oversampling routes, stereo isolation, zero observed
audio-thread allocation, exact block-partition agreement, state/preset routing,
extreme controls, reset and silent-tail decay. Added behavioral checks use two
input levels 20 dB apart: IMPACT increases normalized LF fundamental weight;
ROT increases even/odd harmonic ratio; BLOOM adds broad LF weight while its
50–150 ms release window stays below the ringing-tail guard. All macro response
vectors remain distinct under the existing normalized-response check.

The restored baseline reproduced the prior default response RMS readings with
zero difference on this environment. Cross-platform CI belongs to the resulting
PR commit and must be read separately from these local results.

## Reproduction and evidence

The final comparison uses the same command documented in the baseline report.
To reproduce the fit stage, start a separate checkout at the baseline commit,
apply [structural.patch](evidence/nastrond-voicing-20261005/structural.patch), build
its renderer, and run `Tools/compare_original_nam.py`. Keep the resulting default
float32 render and comparison JSON, as well as the baseline default render.
Then use this branch's `Tools/fit_original_voicing.py`:

```sh
python Tools/fit_original_voicing.py --manifest docs/reference/nastrond-nam-manifest.json --comparison /path/to/structural/comparison.json --pre-contour /path/to/structural/default.f32 --baseline /path/to/baseline/default.f32 --renders /path/to/structural --out /path/to/fit.json
```

[Fit and per-reference proposal](evidence/nastrond-voicing-20261005/fit.json) ·
[Final measured comparison](evidence/nastrond-voicing-20261005/comparison.json) ·
[Before/after summary](evidence/nastrond-voicing-20261005/summary.json) ·
[Gain CSV](evidence/nastrond-voicing-20261005/gain-summary.csv) ·
[CTest](evidence/nastrond-voicing-20261005/ctest.log) ·
[Source hashes](evidence/nastrond-voicing-20261005/source-hashes.json)

NAM weights and rendered audio remain private measurement inputs and are not
distributed. Next acceptance work is actual low-tuned guitar DI, common-cab
listening, and production integration; this checkpoint does not release 1.2.
