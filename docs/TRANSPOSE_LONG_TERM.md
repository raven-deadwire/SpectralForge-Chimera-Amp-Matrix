# Transpose — long-term improvement track

Decision: 2026-10-04 14:45 KST. The user explicitly moved transpose improvement out of the 1.1.1 release scope. Tracking: [Issue #8](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/8).

## Known limitation

The tested candidate was `e046292c6c85eb5f9a2361b7b80fc398ea38d428`. On **Azhi Dahaka, bass B string, -2 semitones**, the user reports undesirable but not severely disruptive timing, with note body/pitch obscured by smeared, predominantly percussive sound. This is a user listening report; no isolated DI or independent reproduction was supplied. Tuning/fret, sample rate, buffer, preset and Gate settings are not yet recorded. Do not assume an open string or a particular input frequency.

Nominal algorithmic latency remains 2048 samples at 44.1/48 kHz and 4096 at 96 kHz: approximately 46.4/42.7/42.7 ms. The <=10 ms goal has not been achieved. PDC does not remove live input latency.

## Follow-up acceptance

1. Capture isolated DI of the reported B-string/-2 part, including repeated attacks and sustained notes. Compare the same input at matched level through bypass and transpose, with Gate off/on, to isolate pitch and gate interaction.
2. Prioritize audible note body/pitch and separation of repeated attacks, then reduce latency. Do not trade low-register quality for a smaller FFT window without comparison evidence.
3. Retain guitar/bass, mono/stereo, 44.1/48/96 kHz, 64/128/256-sample and 0/±1/±2/±5/±12 regression coverage. Measure reported versus actual delay, transient smear, warble, fundamental energy and callback timing.
4. Preserve bypass/zero-shift, transitions, state/automation IDs and lane alignment. These compatibility and safety checks remain required for 1.1.1.

## Release scope

Policy v4 moves `I2.PITCH_LIVE` and `I2.PITCH_DI` to stage `T1`. The `beta_1_1_1` profile excludes only these two improvement goals; `transpose_followup` and `full_release` continue to require their evidence. Deferral does not mean PASS or NOT_APPLICABLE. Gate listening, real DAW lifecycle, callback timing, E670FE reference acceptance and all other existing release checks are retained.

No transpose DSP change is included in the release-preparation commit. Do not advertise low-latency transpose or resolved bass smearing in 1.1.1.
