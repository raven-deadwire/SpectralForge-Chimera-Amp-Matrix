# Náströnd macro-direction voicing pass

Measured 2026-10-05 KST. The request cited `87683f7`; the live PR had already
advanced to `54eded7e3d666c866787b742bf249ee9f7f6d4cf`, including the first
[voicing pass](NASTROND_VOICING.md) and [Preview integration](NASTROND_PREVIEW.md).
This change builds on that code. It does not replace the earlier work.

**Synthetic endpoint direction improves for IMPACT, BLOOM and ROT. Core,
JUCE integration and renderer CTest pass 3/3. Hardware matching and final
voicing remain BLOCKED pending actual instrument-DI listening.**

## What changed

The first pass improved the default spectrum but IMPACT still moved away from
the two unboosted Twin Jet CH2 examples: mean chord distance increased from
3.792 to 4.042 dB when the knob moved 0→1. Its broad LF lift also left excess
120–250 Hz energy relative to those captures. BLOOM's mean chord direction was
nearly flat, 3.332→3.322 dB, despite improved pluck response. ROT's asymmetric
cells remained effective, but its pluck distance increased 10.220→10.403 dB.

The additional change is restricted to the shared core's power/output voicing:

- IMPACT keeps its post-saturation LF body, attack detector and feedback damping.
  A moving 100 Hz shelf and 180 Hz bell separate the low hit from low-mid buildup;
  the upper shelf restores edge that broad LF weighting otherwise obscured.
- BLOOM keeps the existing coupling, supply sag and bounded LF resonator.
  The 180 Hz bell adds body and a broad 1.4 kHz bell relaxes the upper mids.
- ROT keeps its bias polarity, asymmetric cells and recovery. A small upper-shelf
  contribution exposes their upper harmonics. CLANK and CRUSH mappings are unchanged.

All 13 controls, IDs, ranges, defaults, authored preset values, stage drive and
master trim remain unchanged. In particular default GAIN is still **7.2**.
There is no family selector or capture-specific correction. Two biquads are
added; the existing shelves become macro-dependent. Coefficients use the actual
oversampled rate and the existing 20 ms control ramp / 16-sample update cadence.

Let `i = IMPACT − 0.65`, `b = BLOOM − 0.35`, `r = ROT − 0.40` (stored float
defaults are used exactly). The authored dB mappings are:

| Filter | Gain | Q |
|---|---:|---:|
| 100 Hz low shelf | 1.95 + 4i | existing shelf slope |
| 500 Hz bell | −2.11, unchanged | 0.65 |
| 2 kHz high shelf | 6 + 3i + 3r | existing shelf slope |
| 180 Hz bell | −5i + 3b | 0.8 |
| 1.4 kHz bell | −3b | 0.6 |

The full 37-second **default output is bit-identical** to the incoming PR's
default render. Centering preserves that neutral setting; it also changes both
ends of each macro. Endpoint improvement must therefore be read alongside
absolute endpoint scores, not as a blanket improvement to every setting.

## Same-source comparison

All 26 SHA-256-pinned NAM files, all 17 Original states, and the unchanged
48 kHz / JUCE 4x protocol were rerun. The NAM outputs were freshly rendered
once for the candidate comparison; subsequent analysis reused hash-checked
outputs. Both `87683f7` and `54eded7` were rebuilt and their 17 states rerendered.
Every Original and NAM output hash reproduced the archived measurements exactly;
[baseline-reruns.json](evidence/nastrond-macro-direction-20261005/baseline-reruns.json)
records this check. No reference input offsets or capture weights were changed.

Below are arithmetic means for the same selected head groups: VH4 CH3/4 (3),
unboosted Twin Jet CH2 (2), Granophyre (3, provenance unverified), and max-gain
Matamp (3). Each cell is **macro 0 → 1**, in RMS-matched Welch spectrum-distance
dB over 65–8000 Hz; lower is closer for this probe. Full per-file results,
other captures and VH4 metadata-adjusted variants remain in the JSON.

| Macro / reference | Probe | Original `87683f7` | Incoming `54eded7` | This pass |
|---|---|---:|---:|---:|
| CRUSH / VH4 | chord | 14.997 → 5.354 | 10.449 → 3.735 | 10.449 → 3.735 |
| IMPACT / Twin Jet | chord | 6.895 → 6.845 | 3.792 → 4.042 | **4.927 → 3.660** |
| ROT / Granophyre | chord | 8.504 → 8.176 | 4.833 → 4.705 | **5.407 → 4.038** |
| BLOOM / Matamp | chord | 5.093 → 5.364 | 3.332 → 3.322 | **3.630 → 2.941** |
| CRUSH / VH4 | pluck | 19.074 → 10.801 | 15.253 → 7.942 | 15.253 → 7.942 |
| IMPACT / Twin Jet | pluck | 10.730 → 10.563 | 7.487 → 8.240 | **9.118 → 7.575** |
| ROT / Granophyre | pluck | 13.675 → 12.579 | 10.220 → 10.403 | **10.895 → 9.470** |
| BLOOM / Matamp | pluck | 7.591 → 7.801 | 5.912 → 3.703 | **5.733 → 3.768** |

Every selected capture individually improves by at least 0.1 dB from macro 0
to 1 on both probes (22 per-capture/probe endpoint checks). This is an explicit
synthetic direction gate, not a hardware-fidelity tolerance. BLOOM at 1 has a
small **0.065 dB mean pluck regression** against the incoming pass while gaining
0.381 dB on chords; that tradeoff is retained in the evidence. Intermediate knob
positions and other macro combinations are not claimed to be monotonic.

Both plucks and chords informed this pass: **neither is a held-out listening
test**. The bands were chosen from measured residuals, not fitted per capture.
These differences do not measure likeness percentages, low-tuned playing feel
or audible quality. Meshuggah still includes cabinets and cannot support an
amp-only CLANK direction verdict. Granophyre provenance and missing absolute
input calibration remain qualified by the [unchanged manifest](reference/nastrond-nam-manifest.json).

## Gain, roles and regressions

- Default 400 Hz THD remains **59.86%**, and −48→−24 dBFS input growth remains
  **1.302 dB**, unchanged from the incoming PR. Original `87683f7` values were
  approximately 40.0% / 0.62 dB. Filtering affects THD; this is not more input gain.
- At input levels 20 dB apart, IMPACT's normalized 80 Hz fundamental weight
  increases **1.322× / 1.253×**; ROT's even/odd ratio increases **8.617× / 3.713×**;
  BLOOM's LF weight increases **2.242× / 2.047×**. BLOOM's normalized release-tail
  readings stay **0.00247 / 0.00212**, below the unchanged 0.006 guard.
- Default audio is identical; five preset pluck RMS changes range from
  **−0.408 to +1.174 dB** (Fimbulvetr is the largest increase). No preset drive or
  output trims were changed. The largest peak across the 17 probe states is
  **−3.494 dBFS**. These are isolated amp measurements, not full-rig loudness acceptance.
- Fresh CMake **3/3 PASS**: core, actual JUCE integration, renderer. Tests cover
  12 sample-rate/oversampling routes, state/preset routing, exact block-partition
  agreement, stereo isolation, no observed audio-thread allocation, extreme
  controls and silence/reset. Added core coverage repeatedly retargets all five
  macros before their ramps finish, exercising moving filters at all 12 routes.
- The exact renderer used for measurements also passes the renderer regression
  independently. Source, binary and evidence hashes are recorded below.

The Preview already routes through this shared core. This report does not
certify commercial DAW acceptance, callback timing, a new installer or release.
Cross-platform CI belongs to the resulting commit, separately from local CTest.

## Reproduce and review

Run the unchanged acquisition/comparison commands in
[NASTROND_REFERENCE_COMPARISON.md](NASTROND_REFERENCE_COMPARISON.md) with a renderer
built from this source. Then check matching provenance and endpoint directions:

```sh
python Tools/audit_nastrond_macros.py \
  --baseline docs/evidence/nastrond-nam-20261005/comparison.json \
  --incoming docs/evidence/nastrond-voicing-20261005/comparison.json \
  --candidate /path/to/measurement/comparison.json \
  --out /path/to/measurement/summary.json
ctest --test-dir /path/to/build -R 'ChimeraOriginal(AmpTests|AmpIntegrationTests|RendererTests)' -V
```

[Full comparison](evidence/nastrond-macro-direction-20261005/comparison.json) ·
[Direction audit](evidence/nastrond-macro-direction-20261005/summary.json) ·
[Gain CSV](evidence/nastrond-macro-direction-20261005/gain-summary.csv) ·
[CTest](evidence/nastrond-macro-direction-20261005/ctest.log) ·
[Measured renderer regression](evidence/nastrond-macro-direction-20261005/measured-renderer-regression.log) ·
[Source hashes](evidence/nastrond-macro-direction-20261005/source-hashes.json).

NAM weights and rendered audio are not committed. Actual low-tuned instrument
DI and common-cab listening remain the next acceptance step. **No hardware-match
or final-voicing PASS is assigned.**

The subsequent [five-channel high-gain revision](NASTROND_CHANNELS.md) adds the
owner-requested channel banks and stronger midpoint; earlier tables above remain
frozen baseline evidence.
