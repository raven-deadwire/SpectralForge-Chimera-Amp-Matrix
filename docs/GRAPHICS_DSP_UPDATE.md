# Graphics and DSP update

This test update separates visual identity, audio behavior and reference evidence. It contains 46 FX choices: five models in each of the five PRE families and 21 models across the six POST modules. The eight amplifier voices remain available. Hardware names identify design references; generated artwork and signal differences do not establish an exact hardware replica.

## Model artwork

There are 57 individual model assets: 24 pedal bodies for 25 PRE choices, 21 rack faces, eight amplifier heads and four cabinet illustrations. The two Mu-Tron modes share the same hardware body. The existing RavenForge workbench and emblem remain part of the interface. The production manifest is [reference/artwork-manifest.json](reference/artwork-manifest.json), with generation prompts and provenance in [ARTWORK_PROMPTS.json](ARTWORK_PROMPTS.json).

Pedal and rack layouts stay rectangular. Material, grille, trim, panel and knob details distinguish models without reproducing every physical case shape. Native controls provide the interactive knobs, labels, switches and values. Cabinet artwork represents cabinet style, not a photograph of a particular IR recording setup. Source product photos and manufacturer logos are not bundled.

## PRE order and models

New instances use TOUCH: Envelope -> Compressor -> Fuzz -> Boost -> Overdrive. The filter detector receives the playing-level variation before PRE compression. SUSTAIN reverses the first two modules to give the detector a more level-controlled input. Older states without the order parameter retain Compressor first. Gate and Transpose stay globally available before either sequence.

| Family | Two additions | Implemented distinction |
|---|---|---|
| Compressor | Studio FET; Variable Mu | Fast detector with parallel dry contribution; progressive ratio and recovery memory |
| Envelope | Bass Envelope; Dynamic Wah | Band-pass with retained dry lows; faster vocal-band tracking |
| Fuzz | Wool Bass; Gated Factory | Bass-weighted gating; stronger asymmetric gating |
| Boost | EP Lift; Linear Power | Low-mid emphasis with softened top; broadband lift with a mild contour |
| Drive | Bass DI; Micro Bass | Bass-preserving blend and mid scoop; clean lows with upper-band clipping |

Reference mappings and all model names are in [MODELS_AND_REFERENCE.md](MODELS_AND_REFERENCE.md). Physical pickup loading and pedal input impedance are not simulated. The digital Fuzz Face reference therefore does not reproduce every guitar-volume interaction of an unbuffered analog pedalboard.

## DSP and routing

PRE Fuzz, Overdrive and POST preamp use fixed 4x oversampling. Amplifier quality selects 1x, 2x, 4x or 8x while retaining the common algorithmic delay. Drive model filters transition smoothly, overdrive tone changes are smoothed, and an 8 Hz DC blocker removes asymmetric-clipping offset. Fuzz and POST preamp tone-filter poles now ramp over 20 ms, advancing once per oversampled frame for both channels. Optical compression recovery is evaluated per sample so it does not change with host block size. Compressor meters report applied gain reduction; POST peak meters read their respective module stages.

Matrix LOW retains the clean tap after PRE Envelope/Compressor and before Fuzz/Boost/Overdrive. LOW COMP processes the low band before it branches into DI and the selected amp/cab. DI/AMP defaults to 0% amp. The hidden head Drive is fixed at zero, and the DI is aligned to the head's algorithmic delay. Captured IR onset and phase are retained. The six POST modules process the merged signal once.

## IR delivery

The personal pack has 13 real WAV captures, including two bass captures and seven Eminence Karnivore variants. Together with the two embedded factory IRs, the complete personal collection has 15 available captures. Installed files are selectable directly from CAB. The library still distinguishes missing catalog references from loadable files.

The private installer ZIP carries `Chimera-Personal-IRs` beside Setup. Extract the entire ZIP before installing. The public bare Setup includes the two factory audio assets and reference metadata; it does not include the user's restricted captures. The updated personal ZIP can also be imported into an existing installation. See [WINDOWS_INSTALL.txt](WINDOWS_INSTALL.txt).

Karnivore microphone identities come from filenames. Unknown cone position, microphone distance and angle are retained as unknown; the Fredman label is not converted into a guessed microphone pair or angle.

## Compatibility and verification boundary

The first three raw PRE model indices retain their meanings. Normalized host automation written for the earlier three-choice parameters can map differently after expansion to five choices; review and re-record those model-selection lanes in older test sessions. State recall alone does not prove host automation compatibility.

Local DSP verification covers all 46 variants, both PRE detector orders, the Matrix clean tap, model-switch smoothing, DC removal, return-to-bypass alignment and host block-size consistency, in addition to the existing amp/crossover/IR tests. The added fuzz/preamp tone-automation checks passed for all eight voices: smooth onset, matching stereo channels, identical results at 17- and 511-sample host blocks, and return to the original response. Windows build, UI and installer results belong to the corresponding CI run and packaged verification reports; this document is not evidence that an unrun platform check passed. NAM validation remains limited to the fixed head captures and fixtures documented in [NAM_REFERENCE_RESULTS.md](NAM_REFERENCE_RESULTS.md). Real guitar/bass listening and calibrated hardware equivalence are separate acceptance criteria.
