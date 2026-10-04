# Native amp NAM comparison and calibration candidate

This work audits the **23 active amp models** against locally held NAM references. Ironball (serialized ID 16) is retired and is explicitly excluded from calibration. Its compatibility slot remains intact for existing projects.

**This is a candidate, not an all-model fidelity pass.** Twenty model families have 55 reference files available. EICH T900, stock Ampeg SVT-CL full-head, and ENGL E670FE lack an exact reference. The SUNN capture's generation is unresolved and no correction is applied to it. Existing 1.1.1 hardware/listening acceptance does not certify this new voicing.

## Implementation

- Compare the actual `Amp::setNative` path at 48 kHz and 4x oversampling. Earlier legacy-engine reports do not validate these panel-controlled circuits.
- Raise the high-gain channel defaults; ZUTA starts on CH3. Rectifier CH3 starts in Modern, CH2 in Vintage. Existing serialized channel/mode values remain authoritative.
- Add channel-scoped input calibration and three broad output bands (100 Hz shelf, 500 Hz bell, 2 kHz shelf), limited to ±6 dB per band. Level compensation is independent of distortion generation. These are original reduced-order DSP corrections, not NAM weights or recovered circuit coefficients.
- Keep unmeasured channels at identity. The Modern Bass composite correction is active only when its B7K distortion is engaged; the clean path is not fit against a driven capture. Correction coefficients and level slew when its switch changes.
- Revoice heavy factory tones and all four Raven guitar signatures. Remove superseded gain writes so the effective settings are clear. Rebalance factory output trims; Wild Hunt also reduces its upper lane and uses a faster bus attack to retain transient headroom after the amp correction.
- Extend the offline renderer with a native-state JSON argument. Fix output replacement: rerendering must truncate an existing WAV, not append another RIFF stream and read stale samples.

## Comparison method

The baseline is main commit `6fc857e387694ff5323956420e3190c997b166b9`. Official NeuralAmpModelerCore commit `0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842` renders the source files using A2 Full (`--slim 1`). Every NAM file is SHA-256 pinned in [the manifest](reference/native-nam-manifest.json). No NAM weights or third-party audio are included in this repository.

Where metadata provides input calibration, NAM input is adjusted by `11.5 - input_level_dbu` dB. Otherwise only identical digital input is compared; absolute voltage calibration is unknown. The broad tone fit uses deterministic harmonic plucks, with separate chords reserved for comparison. The two signals use different notes and input levels. Scores are phase-independent, RMS-matched log-spectrum errors from 65 Hz to 8 kHz, excluding bins 50 dB below the reference maximum. Each capture's arbitrary recording level is matched independently. Signed waveform correlation and required gain are also recorded, not treated as fidelity certificates.

Input-response probes use 100, 400 and 1200 Hz sines at −48, −36, −24 and −12 dBFS peak. Each segment is 500 ms; the last 250 ms supplies RMS and harmonics 2–12. The input-offset sweep selects conservative channel adjustments from −12 through +12 dB. SVT-VR's proposed −12 dB adjustment was rejected because it worsened the held-out spectrum. Tone fitting is restricted to ±6 dB to control peak growth. A separate weak-input default test requires audible saturation and at least 4:1 level compression over a 24 dB input range; this is a functional high-gain test, not an exact NAM match.

## Reference limits

- Several older source NAM training checks are failed/ignored. Their metadata is preserved. Those renders are comparison evidence, not independently certified captures.
- Twin: Vibrato channel, volume 5, author's “Cranked” EQ. Exact numeric tone settings are unavailable; no Normal-channel correction is inferred.
- JTM45: reissue, high-treble input, V5/LoCut/P5. The LoCut tone recipe is not a fully specified hardware setting.
- 6505 and Rectifier: the exact original head-only SRL NAM files are reused; capture index numbers are not asserted to be gain-knob readings.
- Modern Bass: B7K Ultra **plus DB751**, not either component in isolation. Numeric settings and absolute input calibration are unavailable.
- Super Bassman and D-800+: DI-output references do not validate their speaker-output stages.
- Prism Chime: Ceriatone DC30 clone. The reference state uses EF86, low input, second-brightest tone position, Cut 2, Volume 35%, and master bypass. It is not an original Matchless capture.
- Silk Lead: physical ODS #102-style clone. The old `OD_SMOOTH_S` file was unavailable; the separately hashed `OD_SMOOTH_2_S` is explicitly a new reference, not a byte-identical revalidation of the old file.
- ZUTA: the author describes whole-head DI while NAM metadata says `amp_cab`. That conflict remains unresolved. Both load variants and all four channels are compared.
- VH4: only `[AMP]` whole-head files are used; preamp/power-only and multi-amp blends are excluded.
- Fortin: whole-head EP/KK files only, two load variants. No claim is made for the unmeasured clean channel.
- SLO-100: calibrated BAD reissue. The prior LTD revision is not established.
- Bogner: single Uberschall Rev Blue head references. The three-amplifier BLEND archive is excluded.
- SUNN: the “Lucky Number 7 / No Cab” capture does not identify the amp generation. It is compared for transparency, with no calibration applied to the 1970s target.

## Reproduction

Supply the user-owned files at the relative paths in the manifest; build `ChimeraRender` from the baseline and candidate revisions, and build the pinned NAM Core `render` executable. Python needs NumPy and SciPy.

```sh
python Tools/validate_native_nam.py --models /private/refs \
  --manifest docs/reference/native-nam-manifest.json \
  --nam-render /private/nam-core/build/tools/render \
  --baseline /private/baseline/ChimeraRender \
  --candidate /private/candidate/ChimeraRender --out /private/tone-results
python Tools/validate_native_gain.py --models /private/refs \
  --manifest docs/reference/native-nam-manifest.json \
  --nam-render /private/nam-core/build/tools/render \
  --baseline /private/baseline/ChimeraRender \
  --candidate /private/candidate/ChimeraRender --out /private/gain-results
```

`--fit` on the tone tool proposes broad contours for the supplied candidate; it does not rewrite production coefficients or certify hardware fidelity. Both tools record manifest and executable hashes. The native renderer accepts `channel`, `input_route`, `input_trim_db`, `output_level_db`, and a `controls` map keyed by native control ID. It rejects unknown control keys. Source input and output are mono WAV; output is float32, so above-unity internal amp peaks remain inspectable.

Real guitar/bass DI, matched cabinets and level-matched listening remain necessary for final tone acceptance. Exact reference files are still required for the three missing models. The measurements below describe these fixed capture settings and synthetic fixtures only.

<!-- RESULTS -->

## Measured results

Chords are the held-out stimulus. Lower log-spectrum error is better; these are not pass/fail fidelity thresholds. Values are averaged over each family’s available captures.

| Active model | NAM files | Baseline error (dB) | Candidate error (dB) | Scope |
|---|---:|---:|---:|---|
| Glass / Twin | 1 | 13.92 | 4.70 | Compared and adjusted |
| Brit Edge / JTM45 | 1 | 9.05 | 3.51 | Compared and adjusted |
| Tight 515 / 6505 | 1 | 10.78 | 3.62 | Compared and adjusted |
| Wide Rect / Dual Rectifier | 1 | 9.64 | 4.27 | Compared and adjusted |
| Liquid Lead / Mark IV | 1 | 9.67 | 6.08 | Compared and adjusted |
| Iron Tube / SVT-VR | 1 | 2.82 | 2.63 | Compared and adjusted |
| Solid Punch / GK800RB | 1 | 4.91 | 2.44 | Compared and adjusted |
| Modern Bass / B7K + DB751 | 1 | 9.94 | 6.74 | Composite driven chain |
| Chime 30 / AC30 | 1 | 10.53 | 4.36 | Compared and adjusted |
| Orange Crown / Rockerverb | 1 | 7.68 | 3.38 | Compared and adjusted |
| Vintage Valve / Super Bassman | 1 | 10.22 | 6.28 | DI output reference |
| Metro Clean / D-800+ | 1 | 5.28 | 1.86 | DI output reference |
| Prism Chime / DC30 | 1 | 10.34 | 6.03 | Clone reference |
| Silk Lead / ODS | 1 | 5.24 | 4.56 | Clone reference |
| Taste Punch / EICH T900 | 0 | — | — | Exact reference missing |
| Cinder 120 / ZUTA | 8 | 11.41 | 4.21 | Author/metadata conflict retained |
| Fourfold / VH4 | 5 | 9.47 | 4.63 | Compared and adjusted |
| Classic Tube / SVT-CL | 0 | — | — | Exact reference missing |
| Monolith / SUNN | 1 | 9.61 | 9.61 | Generation unresolved; comparison only |
| Night Harvest / Fortin | 8 | 8.24 | 4.24 | Compared and adjusted |
| Hot Lead / SLO100 | 3 | 7.55 | 4.29 | Compared and adjusted |
| Blue Storm / Uberschall | 16 | 7.27 | 4.30 | Compared and adjusted |
| Special Edition / E670FE | 0 | — | — | Exact reference missing |

At −36 dBFS peak / 400 Hz, the **actual defaults** produce the following THD (harmonics 2–12 divided by fundamental). The separate compression measurement prevents output gain from masquerading as saturation.

| Default | THD before | THD after | Output growth for −48 → −24 dBFS input, after |
|---|---:|---:|---:|
| Tight 515 / 6505 | 0.280 | 0.824 | 1.04 dB |
| Wide Rect / Dual Rectifier | 0.057 | 0.829 | 1.60 dB |
| Liquid Lead / Mark IV | 0.248 | 0.377 | 0.22 dB |
| Orange Crown / Rockerverb | 0.254 | 0.658 | 0.77 dB |
| Cinder 120 / ZUTA | 0.003 | 0.688 | 3.12 dB |
| Fourfold / VH4 | 0.217 | 0.810 | 1.24 dB |
| Night Harvest / Fortin | 0.263 | 0.657 | 1.53 dB |
| Hot Lead / SLO100 | 0.291 | 0.606 | 1.77 dB |
| Blue Storm / Uberschall | 0.327 | 0.737 | 2.03 dB |
| Special Edition / E670FE | 0.239 | 0.339 | 0.88 dB |

ZUTA’s default comparison includes the requested CH1 → CH3 change. Rectifier’s includes RAW → Modern. E670FE’s stronger default is functional voicing only; it has no exact NAM calibration claim. All 23 active defaults are present in the gain evidence, including the three reference gaps.

The 38-preset integrated fixture ranges from -29.86 to -25.77 dBFS RMS. Worst nominal peak is -9.65 dBFS; worst +6 dB-input peak is -4.81 dBFS. Full recall, inactive-bank isolation, performance-control preservation, signature snapshots and processor GR checks pass locally. These are synthetic level checks, not LUFS or recorded-DI acceptance.

The native suite covers control responses, inactive-bank isolation, 44.1/48/96 kHz routes, oversampling integration, transitions and zero audio-thread allocations. The serialized Ironball slot still participates in compatibility tests but contributes no NAM calibration. A regression also verifies repeat renders replace WAV data and reject unknown native controls.

Saved native parameter values retain their identity; existing sessions adopt the corrected native DSP and may therefore sound different when reopened. The updated factory trims apply on factory recall.

Detailed evidence: [tone comparisons](evidence/native-nam-20261004/tone.json), [gain curves](evidence/native-nam-20261004/gain.json), [applied coefficients](evidence/native-nam-20261004/calibration.json), and [integrated checks](evidence/native-nam-20261004/integrated-final.log).
