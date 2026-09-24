# Third-party notices

## Factory cabinet impulse responses

Author: **jesterdyne**. License: **Creative Commons Attribution 4.0 International**.
License and legal terms: https://creativecommons.org/licenses/by/4.0/

- `Assets/IRs/guitar_v30_sm57.wav`: original title “Engl Celestion V30 SM57 center-01.wav”, https://freesound.org/people/jesterdyne/sounds/116735/
- `Assets/IRs/guitar_jensen_sm57.wav`: original title “Jensen Cab SM57 center.wav”, https://freesound.org/people/jesterdyne/sounds/116743/

The original WAV files are embedded unchanged. At playback, JUCE resamples and energy-normalises them; leading timing is retained (no trimming). The source pages' titles and generic cabinet descriptions differ, so the short UI names identify the published files without asserting an independently verified cabinet model. No endorsement by the author or equipment manufacturers is implied.

Byte source: OpenSauce/rustortion, commit `1522f4d64f520bd665cfb098601a2cbf0bcae0d8`, `impulse_responses/Jesterdyne/Engl/sm57-center-01.wav` and `impulse_responses/Jesterdyne/Jensen/sm57-center.wav`. Attribution and licensing are based on the original author's Freesound pages, not on the host repository's code license.

## Pitch engine

The shipped pitch engine is original Chimera STFT code (`Source/PolyPitch.h`) using JUCE FFT, instantaneous-frequency scaling and local peak phase locking. The initially evaluated Signalsmith implementation is not included in this build.

## JUCE

JUCE 8.0.8: https://github.com/juce-framework/JUCE/tree/8.0.8
JUCE license: https://github.com/juce-framework/JUCE/blob/8.0.8/LICENSE.md

## Design reference

The information hierarchy and visible signal flow were informed by Odeholm Audio's Slam Amp and thall amp product pages. Chimera's interface is original JUCE vector drawing; no product artwork, logos, fonts, presets, amp captures or commercial IRs from those products are included.

## Offline NAM / TONE3000 references

NeuralAmpModelerCore by Steven Atkinson and contributors was used as an external offline renderer, pinned to `0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842`. It is not linked into the plugin. https://github.com/sdatkinson/NeuralAmpModelerCore

T3K NAM/IR files are not embedded or redistributed with the public plugin. Source/creator/hash records and metadata-only sidecars are in `reference`. Original hardware names are descriptive references, not endorsements. TONE3000 sharing terms: https://www.tone3000.com/guides/tone-sharing-guidelines
