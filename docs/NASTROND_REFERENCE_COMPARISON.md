# Náströnd original-reference comparison

Measured 2026-10-05 KST against the development DSP at `f844c1ae502982cda035f1c91d43146403ff176f`.

**Acquisition and measurement are complete for 26 distinct NAM files from five named reference families. Exact original-head coverage is incomplete: Meshuggah captures include cabinets, and the Granophyre uploaders do not document the physical capture chain. These results do not certify hardware matching or final musical acceptance.**

## Source coverage

| Intended role | Reference | Files | Available evidence / limitation |
|---|---|---:|---|
| CLANK | [Fortin Meshuggah Plexi](https://www.tone3000.com/tones/fortin-meshuggah-86564), [Dark Clouds](https://www.tone3000.com/tones/fortin-meshuggah-plexi-dark-clouds--1621) | 8 | Amp + cab. Four unboosted noon/mid-gain, two boosted, two heavy-context captures. No head-only or input calibration; amp-only spectral comparison is blocked. |
| CRUSH | [Diezel VH4](https://www.tone3000.com/tones/diezel-vh4-64455) | 3 | Complete head DI, two CH3 and one CH4; speaker load and input/output calibration documented. Exact knob positions unavailable. |
| IMPACT | [Uberschall Twin Jet KT88](https://www.tone3000.com/tones/bogner-uberschall-twin-jet-kt88-74483) | 6 | Three unboosted, three boosted. CH1 crunch and CH2 high gain kept distinct. Page/model names identify Twin Jet; embedded gear metadata incorrectly names a pedal. No input calibration. |
| ROT | [Omega Granophyre KT88](https://www.tone3000.com/tones/omega-granophyre-kt88-39398), [2000 epochs](https://www.tone3000.com/tones/omega-granophyre-2000-epochs-39716) | 3 | Labeled amp-head by uploaders; input calibration, knob settings, physical provenance and capture chain unverified. Related training variants are not independent hardware observations. |
| BLOOM | [Matamp GT1 mkIII MV](https://www.tone3000.com/tones/matamp-gt1-mk-iii-100w-mv-mode-max-gain-68501) | 6 | Direct head through modified Captor 8; Voice 3/4, half/max gain and Auto A2 variants. No absolute input calibration. Six files do not mean six independent capture sessions. |

Every local file parsed and matched its SHA256. Full A2 inference (`--slim 1`) was used. NAM weights and audio are not distributed in the repository. The manifest preserves source URLs, hashes, architecture, metadata conflicts and exclusions. Evil Pumpkin, Uberschall Rev Blue, a modified Yerasov, Salvation Unnamed, and DAW-created Meshuggah-style profiles are not substituted for the requested original heads.

## Measurement method

- Actual `OriginalAmpProcessor`, 48 kHz input and 4x JUCE oversampling; default, five presets, maximum gain, and each macro at 0/1: 17 states.
- Identical digital input: 100/400/1200 Hz at −54, −48, −42, −36, −30, −24 and −12 dBFS peak. Each step settles for 750 ms; the final 250 ms is measured. No PRE, cabinet or POST is added.
- RMS output growth is measured for a 24 dB input increase (−48→−24). Low growth indicates compression/saturation; negative growth can occur with level-dependent filtering or blocking and is not automatically an error.
- THD is the RMS ratio of harmonics 2–12 to the fundamental at −36 dBFS peak. It depends on tone filtering and can exceed 100%; it is not a universal gain-quality score. Meshuggah values include the cabinet response.
- VH4 is additionally rendered with −7.495 dB input adjustment from its 18.995 dBu metadata to the existing 11.5 dBu comparison convention. This does not establish a measured physical calibration for Náströnd. Other families remain digital-input-only comparisons.
- Deterministic plucks/chords are synthetic, not actual guitar DI. Amp-head spectral differences use only output RMS matching, 65–8000 Hz Welch spectra. No parameter fit or contour correction is applied. Full-rig Meshuggah is excluded from amp-only spectral scores.

## Gain observations

400 Hz summary below; the CSV and JSON retain all three frequencies, every capture and all parameter states. Ranges are observations of the selected captures, not manufacturer specifications.

| State / selected reference group | Output growth for +24 dB input | THD at −36 dBFS |
|---|---:|---:|
| Náströnd default | 0.62 dB | 40.0% |
| Náströnd gain_max | 0.29 dB | 41.4% |
| Náströnd fenrir | 1.03 dB | 38.2% |
| Náströnd surtr | 0.19 dB | 42.0% |
| Náströnd nidhoggr | 0.85 dB | 38.8% |
| Náströnd fimbulvetr | 1.74 dB | 34.4% |
| Náströnd ragnarok | 0.28 dB | 41.7% |
| Meshuggah unboosted, cab included | 0.11–0.53 dB | 61.18–94.92% |
| VH4 CH3/CH4, metadata-adjusted | -0.50–0.04 dB | 60.74–89.04% |
| Twin Jet CH2, unboosted | 1.52–1.58 dB | 60.60–64.95% |
| Granophyre, undocumented settings | 1.78–2.60 dB | 99.42–106.52% |
| Matamp max-gain variants | 0.80–1.12 dB | 38.40–43.05% |

**Finding:** default gain 7.2 already produces strong saturation. At 400 Hz, increasing it to 10 changes THD only from about 40.0% to 41.4%, while the measured VH4, Twin Jet and Granophyre captures have materially different harmonic ratios. Turning up the same gain alone is not evidence that those reference characters have been reproduced. No final gain-acceptance PASS is assigned.

At 100 Hz, the five presets span approximately −0.11 to 3.23 dB output growth, and at 400 Hz 0.19 to 1.74 dB. This establishes distinct synthetic compression behavior; it does not establish the owner’s low-tuned rhythm feel.

## Remaining character differences

| Reference group | Default chord-spectrum RMS difference | Macro 0 → 1 observation |
|---|---:|---|
| VH4 CH3/CH4 | 6.67–7.26 dB | CRUSH: 14.54–15.56 → 5.10–5.65 dB |
| Twin Jet CH2 unboosted | 6.72–7.01 dB | IMPACT: 6.75–7.04 → 6.70–6.99 dB |
| Granophyre | 8.05–8.91 dB | ROT: 8.12–8.98 → 7.79–8.64 dB |
| Matamp max gain | 4.67–6.19 dB | BLOOM: 4.57–6.07 → 4.83–6.36 dB |

These are distances between different authored tones, not a required cloning score. They expose unvalidated design assumptions: CRUSH moves the broad spectrum toward these VH4 examples, but a large residual remains; IMPACT has only a small spectral effect; ROT makes a modest change; increasing BLOOM does not move the broad spectrum toward these Matamp captures. Temporal bloom/sag still needs separate transient and decay evaluation with instrument DI.

## Disposition for 1.2

| Item | Status | Next action |
|---|---|---|
| 26 NAM files acquired, hashed and rendered | PASS — measurement execution | Preserve exact-source manifest and local input files |
| Default/preset/macro comparison | COMPLETE — observations only | Use measured dynamics and spectrum together when revising stage drive, coupling, asymmetry and power response |
| Exact five original heads as amp-only anchors | BLOCKED | Obtain documented Meshuggah head-only capture; strengthen Granophyre hardware provenance |
| Absolute input calibration across all five | BLOCKED | Obtain missing capture voltage/chain data; do not invent input offsets |
| OriginalAmp core, integration and renderer regressions | PASS — local CTest 3/3 | Cross-platform build remains a separate CI result |
| Owner-level high-gain and reference-character acceptance | PENDING | Real DI, low-tuned rhythm/lead listening and final voicing |

This checkpoint records the measured baseline for subsequent voicing work. The source under test is the isolated development module; production integration and release acceptance are still pending.

Follow-up: [reference-informed voicing and remeasurement](NASTROND_VOICING.md)
records the subsequent DSP revision. The tables above remain the original baseline.
The next [macro-direction pass](NASTROND_MACRO_DIRECTION.md) compares this baseline,
the first voicing revision, and the revised IMPACT/BLOOM/ROT mapping on the same NAMs.

## Reproduction and evidence

Build `ChimeraOriginalAmpRender` with the project’s JUCE/CMake setup. Download inputs into a private directory outside the repository, then run:

```sh
python Tools/fetch_nastrond_references.py --manifest docs/reference/nastrond-nam-manifest.json --out /path/to/private-nam
python Tools/compare_original_nam.py --manifest docs/reference/nastrond-nam-manifest.json --models /path/to/private-nam --nam-render /path/to/nam-core/build/tools/render --original-render /path/to/ChimeraOriginalAmpRender --out /path/to/measurement --workers 2
```

[Reference manifest](reference/nastrond-nam-manifest.json) · [Full results](evidence/nastrond-nam-20261005/comparison.json) · [Gain CSV](evidence/nastrond-nam-20261005/gain-summary.csv) · [Source hashes](evidence/nastrond-nam-20261005/source-hashes.json) · [CTest](evidence/nastrond-nam-20261005/renderer-ctest.log)
