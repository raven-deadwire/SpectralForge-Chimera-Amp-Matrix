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

## Update 2026-10-22 — feature-branch quality/latency mode

The user reversed the deferral for a feature-branch spike only (branch `claude/ecstatic-dirac-le0t7d`, not the 1.1.1 release-preparation commit) and asked for a user-selectable 3-way transpose quality/latency mode:

- **Classic** — the released ~32 ms window. This is the default and uses PolyPitch's window override 0, i.e. the exact released code path, so the `PolyPitchV110` regression fixture and the released-parameter contract stay intact.
- **Low-Latency** — half the default window (~21 ms at 44.1/48 kHz).
- **Tight** — a 10 ms analysis window (~11 ms before 2026-10-22), intended for guitar leads; low-register body is reduced.

This deliberately trades low-register quality for a smaller FFT window, which §Follow-up acceptance (2) says not to do **without comparison evidence**. That evidence is **not yet captured**: the required isolated-DI bypass-vs-transpose listening/measurement comparison must be gathered on a real build before any of this is promoted toward a release. Until then, per §Release scope, this stays out of the 1.1.1 release-preparation commit and must not be advertised as low-latency transpose in 1.1.1.

Implementation: three pre-prepared `PolyPitch` instances switched allocation-free in `Transposer` (`Source/GlobalDSP.h`); a new appended `transposeQuality` `AudioParameterChoice` (host index ≥ 3686, so the frozen contract is untouched — `GateProcessorTests` `expectedParameterCount` bumped +1); a UI `ComboBox` in the transpose cluster (`Source/PluginEditor.cpp`). The default (Classic) leaves released behaviour, latency and the `PolyPitchV110` fixture unchanged.

## Update 2026-10-22 — Tight mode meets the ≤10 ms algorithmic target

Tight previously used a quarter of the power-of-two default window: 512 samples at 44.1/48 kHz (11.6 / 10.7 ms) and 1024 at 96 kHz (10.7 ms), just above the ±1/±2-semitone ≤10 ms added-latency target. `PolyPitch` now accepts an analysis window shorter than its FFT: the window is zero-padded into the next power-of-two FFT and latency follows the window. Tight uses the largest multiple of 8 samples within 10 ms:

| Rate | Before | Now |
|---|---|---|
| 44.1 kHz | 512 (11.61 ms) | 440 (9.98 ms) |
| 48 kHz | 512 (10.67 ms) | 480 (10.00 ms) |
| 96 kHz | 1024 (10.67 ms) | 960 (10.00 ms) |

Classic and Low-Latency keep window = FFT length, so their arithmetic, latency and the `PolyPitchV110` fixture are unchanged; `ChimeraPitchRegressionTests` asserts this and the Tight latency/target-pitch checks. This is the transposer's own algorithmic delay only. Host buffering, round-trip latency, attack/smear quality and the isolated-DI comparison against Neural DSP (#97 P0-1) are still not measured; the correlation-splice research candidates (#100/#112) are not integrated.
