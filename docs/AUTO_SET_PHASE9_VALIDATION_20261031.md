# Phase 9: bass and cabinet acoustic regression — 2026-10-31

PR #232 was merged as `14c730079bed39016a2c357ec55f48f8974f0414`.
This follow-up was developed on main `472b93e` (also includes phase 10).
It fixes two reproduced Auto Set problems without changing the authored
cabinet responses, IR generator, saved parameter IDs, or tone targets.

## Reproduced defects

1. The old level estimate summed only 80 Hz–6.3 kHz and assumed energy was
   spread across each third-octave band. A post-cab signal with amplitudes
   0.1 at 27.5 Hz, 0.02 at 55 Hz and 0.01 at 990 Hz exposes both blind spots.
   With Bass 4x10 / Vector 10 / Anchor 20, unit 4, moving from 0.5 / 10 cm
   to 0.85 / 20 cm at 48 kHz gives a **6.04352 dB level prediction error**.
   Bass 6x10 unit 1 gives **5.73788 dB** on the same fixture.
2. Bass 6x10 / Foundry 10 / Anchor 20, unit 6, moving from 0.5 / 10 cm to
   0.85 / 5 cm reduces the old average roughness from **0.854247 to
   0.616586 dB**, yet adds **6.38769 dB** of local notch depth near
   **4308.67 Hz**. An overly bright synthetic bass spectrum selected this
   placement. An RMS average alone cannot enforce the no-new-notch intent.

Level compensation now uses Hann-windowed, approximately 1.5 Hz FFT-bin
energy and the ratio of the actual finite IRs. The tone search retains its
existing model-band approximation. A new local-extrema check supplements
the unchanged 1 dB average-roughness limit: at 1/96-octave spacing over
27.5 Hz–6.3 kHz, no individual peak or notch may grow more than 3 dB relative
to its +/- 1/6-octave neighbours. This also runs before applying a rounded
Strength placement. The test still requires useful, >0.5 dB tone improvement;
rejecting every candidate is not a passing result.

## Actual repository configurations

All nine layouts in `Source/CabLayoutModel.h` are covered. Drivers are
mounting-compatible entries in `Source/CabExpansionModel.h`, not inferred
from third-party product names. Guitar tests use Needle 57; bass tests use
Anchor 20. Tweeters are off and backs closed in this matrix.

| Layout | Driver | Units (one-based) |
|---|---|---|
| Guitar 1x12 | Ember 30 | 1 |
| Guitar 2x12 | Ember 30 | 1, 2 |
| Guitar 4x12 | Ember 30 | 1, 4 |
| Bass 1x15 | Monolith 15 | 1 |
| Bass 2x10 | Foundry 10 | 1, 2 |
| Bass 4x10 | Vector 10 | 1, 4 |
| Bass 6x10 | Foundry 10 | 1, 6 |
| Bass 1x12 | Clarity 12 | 1 |
| Bass 2x12 | Chimera Depth 12 | 1, 2 |

## Reproducible tests

Build `ChimeraAutoSetTests` normally, then run:

```sh
cmake --build build --target ChimeraAutoSetTests ChimeraCabLayoutModelTests --parallel 2
xvfb-run -a build/ChimeraAutoSetTests_artefacts/Release/ChimeraAutoSetTests --cab-regression
ctest --test-dir build -R '^ChimeraCabLayoutModelTests$' --output-on-failure
```

The same CAB tests also run in the default `ChimeraAutoSetTests` suite;
no workflow change or new compiler flag is needed. Test code is in
`Tests/AutoSetCabRegression.h` and `Tests/AutoSetTests.cpp`.

### Acoustic oracle (no DI required)

The oracle evaluates the **actual generated finite IR** by complex DTFT,
independently of the continuous model response. Steady sinusoid outputs
use that IR's exact complex transfer, including phase; their sample RMS
provides an independent energy reference. These are synthetic fixtures,
not real instrument recordings or an acoustic measurement of a physical cab.

At 44.1, 48 and 96 kHz:

- **90 IR placement pairs:** 15 layout/unit configurations x 3 rates x
  two moves from 0.5 / 10 cm (to 0.15 / 5 cm and 0.85 / 20 cm).
- Third-octave power differences from 50 Hz through 6.3 kHz use eight
  frequency points per band. Maximum model/IR **change** error:
  **0.232348 dB**, below the unchanged 1.5 dB bound.
- Separate exact frequencies: **27.5, 55, 82.5, 110, 137.5, 165 and 220 Hz**.
  Maximum model/IR change error: **0.750016 dB**, below 1.5 dB.
- **270 energy cases:** fundamental alone; fundamental with three low
  harmonics; sub-bass-dominant signal with weak 990 Hz content. Maximum
  new level prediction error: **0.010091 dB**, below the 0.25 dB bound
  before host gain rounding.
- **18 selected moves:** bright/dark target-shaped spectra on Guitar
  4x12, Bass 4x10 and Bass 6x10 at all three rates. Maximum local-extrema
  increase evaluated on the generated IR: **2.9698 dB**. All 18 still
  improve the predicted fit by more than 0.5 dB. The known 6x10 notch
  placement is explicitly rejected.

These figures are local GCC 13.3 / JUCE 8.0.8 Release results. The full
processor-based `--cab-regression` also passes headlessly on production
code commit `1f97278`,
including the original CAB rules/controller and the new bass matrix below.
This is processor/controller validation, not native-window or real-DAW testing.
`ChimeraCabLayoutModelTests` passes (3.94 s). Remote CI results are reported
on the follow-up PR, tied to its current head rather than inherited from #232.

### FFT endpoint contract

The PR's automated review suggested that JUCE returns Nyquist in float
element 1. That is vDSP's internal packed representation, not JUCE's public
output. JUCE 8.0.8 `juce_FFT.h` documents `2*N` floats with interleaved
complex bins, and `AppleFFT::mirrorResult` explicitly moves Nyquist to
complex bin `N/2` and clears DC's imaginary part. Thus floats `N` and `N+1`
are valid, and float 1 is DC's imaginary component. No production change
was made for that suggestion. A deterministic alternating-sample regression
checks the public FFT result and the CAB energy measurement at all three
rates, so the packed-layout substitution would fail the test.

### Controller preservation

The additional bass matrix uses 1x15, 6x10 and 2x12, heard mic A and B,
Strength 25/50/75/100%, and a 27.5 Hz component added to the captured fixture.
It checks 24 moves, applied-position level compensation, the untouched
other mic, six locked cases, and all-parameter snapshot equality after undo.
The existing silence, target, movement-limit, Dual, captured-IR and blended-mic
checks remain. This does not expand Phase 9 into Dual or Matrix routing.

## Limits and remaining validation

- Real Drop A guitar and bass DI, note attacks/ghost notes and listening
  comparisons remain **not performed**. Test results do not approve the
  target curves or thresholds perceptually.
- This is a selected-driver/mic, closed-back/no-tweeter matrix, not every
  combination of 14 drivers, 20 microphones, tweeters and cabinet rears.
- Matching total RMS energy does not guarantee an unchanged fundamental,
  every harmonic, or perceptual loudness. It corrects the measured level
  estimate; the tone search still intentionally changes spectral balance.
- The gain remains limited to -24..+12 dB. At a rail, complete level matching
  may be impossible. The 0.1 dB host step is separate from predictor error.
- The continuous response and finite IR need not have the same absolute
  27.5 Hz gain. Only their placement **differences** are bounded here. The
  generator is preserved to avoid changing existing sessions' sound.
- The grid-based local guard does not prove the absence of every possible
  sub-grid notch for untested configurations.

GCC 15 pixel reproducibility (#227) and the approved tuner fixture/hash
maintenance (#222) are separate work. This follow-up changes neither the
compiler flags, pixel expectations, tuner wait times, nor protected hashes.
