# Open Beta 1.0 — additional NAM reference audit

Six additional Chimera voices were rendered against downloaded TONE3000 NAM files
using official NAM Core commit `0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842`.
These are comparisons with fixed digital references, not a certification that the
algorithms reproduce physical amplifiers. The neural files are not embedded in
Chimera or included in the release. The T3K pages allow using files and publishing
rendered audio, but require the author's permission to redistribute the data files.

## Reference provenance

| Voice | Exact reference | Scope and calibration |
|---|---|---|
| Chime 30 | [VOX AC30 CH](https://www.tone3000.com/tones/vox-ac30-ch-hyper-accuracy-31267), slamminmofo; Top Boost V5 / TC4 / B5 / T5 | Actual head, Suhr Reactive Load; master maximum, attenuation/effects off. File input calibration **12.2418 dBu**. |
| Orange Crown | [Orange Rockerverb 50 MKIII](https://www.tone3000.com/tones/orange-rockerverb-50-mkiii-head-94818), hlrossato | Actual head and Torpedo Captor, no cabinet. G5.5 / B5 / M5 / T6 / V4. Input calibration unknown. This is the 50 W version. |
| Bassman Valve | [Fender Super Bassman](https://www.tone3000.com/tones/fender-super-bassman-74042), payday | 2011 300 W head **DI output**, overdrive channel. Exact controls and input calibration unknown. This does not measure the speaker-level power-stage output. |
| Subway Clean | [Mesa Subway D-800+ DI Flat](https://www.tone3000.com/tones/mesa-subway-d-800-di-flat-41431), markhopkinsbass | 2017 **D-800+**, flat EQ, DI output. Other controls and input calibration unknown. |
| Match Chime | [Ceriatone DC30 clone](https://www.tone3000.com/tones/matchless-dc30-clone-4638), raksha / Tor KV | A physical **Ceriatone clone**, not original Matchless hardware. EF86 Ch2, BRT, Cut2, Volume35%, low input, full power/master bypass. Page states 12.1 dBu; downloaded A2 file omits calibration, so this run retains unity digital input and marks calibration unknown. |
| Silk ODS | [ODS #102 Ford clone](https://www.tone3000.com/tones/dumble-ods-102-ford-hyper-accuracy-30435), slamminmofo; OD Smooth | Creator's physical **Dumble-style clone**, not a verified original Alexander Dumble amplifier. Normal input. File input calibration **12.2418 dBu**; complete numeric controls unavailable. |

**Reference quality limit:** the selected VOX and ODS NAM files contain source
training metadata `checks.passed=false` and `ignore_checks=true`. They decode and
render with the official core, but their creator's source-data checks were
bypassed. Those flags are retained in the manifest; low spectral error does not
validate the capture itself. The calibration pages round to 12.4 dBu, while this
run uses the exact 12.2418 dBu present in the downloaded files.

An exact EICH T900 capture was not located in this TONE3000 audit. A related
[TecAmp Blackjag 900](https://www.tone3000.com/tones/tecamp-blackjag-900-45191) file
was downloaded and hashed for provenance inspection, but it is a different amp
and is **not** used as proof of EICH T900 behavior. No exact Vanderkley Aurora
capture was located. No substituted Vanderkley measurement is claimed.

## Measurement method

The source manifest is [open-beta-nam-manifest.json](reference/open-beta-nam-manifest.json).
It pins exact filenames, SHA-256 hashes, creator, hardware/clone lineage, known
controls, architecture and calibration/training limitations. The validator rejects
a hash mismatch. Actual NAM files remain in the private reference workspace.

Both engines process the same deterministic eight-second synthetic DI at 48 kHz.
Chimera runs the compiled C++ amp renderer at 4x oversampling, Drive 0.4 and flat
user EQ. Calibrated NAM input uses a common 11.5 dBu source convention; missing
metadata uses unity digital gain and remains explicitly unknown. The metric is
RMS log-power-spectrum difference over 65 Hz–8 kHz Welch bins above −50 dB relative
to the NAM spectral peak, after whole-render RMS matching. It is not NAM training
ESR or an audio quality score. Lag/polarity/correlation remain diagnostics, not a
license to align away audible differences.

Three additional broad filters were fitted on the single-note fixture only:
100 Hz low shelf Q0.707, 500 Hz peak Q0.65, 2 kHz high shelf Q0.707. A different
chord/fundamental/amplitude fixture is held out. Existing baseline voicing and
fixed output trim remain unchanged. Added filters apply only to these six voices;
other model indices retain their existing signal path.

Baseline values and fit proposals are in
[open-beta-nam-before.json](reference/open-beta-nam-before.json).
Python fit predictions are not presented as measurements of the shipped plugin.
The final comparison below uses a new render of the compiled C++ implementation.

For A/B listening exports, both guitar signals use the same factory V30/SM57 IR
resampled to 48 kHz, and both bass signals use the same personal Ampeg 8×10/MD421
IR. Cabinet onset delay is retained. Comparison WAVs are RMS matched and receive
a common peak trim; the plugin itself does not automatically loudness-match.

## Compiled C++ comparison results

Final measurements below were rendered again from the C++ implementation after applying the broad filters. Lower values mean a smaller log-spectrum mismatch on these fixed fixtures only. Units are dB RMS log-spectrum error after whole-render RMS matching; this is not an accuracy percentage or a listening score.

| Voice | Pluck before → after | Held-out chord before → after |
|---|---:|---:|
| Chime 30 | 8.59 → 8.38 | 8.06 → 5.45 |
| Orange Crown | 12.29 → 5.66 | 13.41 → 5.27 |
| Bassman Valve | 8.43 → 3.77 | 9.05 → 3.80 |
| Subway Clean | 3.57 → 1.83 | 5.38 → 2.23 |
| Match Chime | 8.97 → 6.01 | 9.99 → 5.32 |
| Silk ODS | 7.79 → 4.70 | 12.18 → 6.41 |

All six held-out chord results improved relative to the preceding implementation. Residual mismatch remains material, particularly on Chime 30 and Silk ODS. Full measured values, calibration details and hashes are in [open-beta-nam-after.json](reference/open-beta-nam-after.json). EICH is excluded from this comparison.

## Remaining acceptance work

These fixtures do not cover all amplifier controls, pickups, input levels, playing
techniques, low tunings or real performer transients. The new fit does not correct
all nonlinear dynamics, phase, attack/recovery or level differences. Source capture
training flags, clone lineage and unknown input calibration remain limits after
any measured spectral improvement. Calibrated real-instrument listening remains
an acceptance step.
