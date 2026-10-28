# Exporting a rig's cabinet as an IR

Use **Export this cabinet as WAV...** in a rig's cabinet selector. The WAV
contains the settled, linear cabinet response: mic A and B (captured/user IRs
or models), blend, gains, polarity, signal delay and each microphone's cuts.
CAB off exports a unity impulse. Amps, effects, EQ, dynamics and routing/band
splits are excluded. This is a cabinet response, not an entire amp capture.

## Format and replay

- 32-bit float WAV at the host/session sample rate; 48 kHz before preparation.
- Mono only when both rendered channels are exactly identical. Independent
  left/right IRs stay stereo. Numerical differences in modeled stereo
  convolution can also retain two channels; they are not averaged away.
- No normalization or leading-silence trimming. Mic delay remains in the file.
- At least 10 ms and at most 2 s. Tail trimming bounds the **sum** of discarded
  absolute samples by `peak * 1e-7`, so a long quiet filter tail cannot accumulate
  a large frequency/phase error. The former per-sample -120 dB cutoff did not
  provide that guarantee.
- A standard BWF description identifies a complete Chimera cabinet response.
  It is an ordinary WAV; other readers may ignore the description.

When this tagged WAV is imported on Mic A, Chimera preserves its level, selects
Mic A only, enables CAB, and resets Mic A gain/delay/polarity to neutral. The
file already includes both microphones and all their processing. **CUTS OFF**
is enabled to prevent applying the filters twice. The button is visible in
both CAB panel views; turn it off deliberately to add new filtering. When
imported on Mic B, B's level/delay/polarity and cuts are neutralized, but the
existing blend is retained. Ordinary untagged IR imports keep the existing
normalization policy and enable that microphone's cuts.

Tagged exports accept up to 2 s, including intentional silence from complete
A/B cancellation. Ordinary IRs retain their 1 s limit and silent-file rejection.
The WAV bytes and tag are embedded in project state, with the cut-bypass
parameters appended after the released parameter prefix (version 15). Existing
presets/projects default to cuts enabled. Removing the WAV metadata removes
this automatic replay behavior.

In an external loader, disable normalization, automatic trimming/minimum-phase
conversion, extra filters and added gain/delay. Its stereo and maximum-length
support must also be checked. External loader/DAW compatibility is not certified
by the native tests below.

## Implementation and save safety

`renderCabinet()` creates separate cabinets and an IR library restored from the
project's embedded assets. It builds only the selected rig's requested models,
uses the production parameter mapping, and reapplies mic B cuts **after**
`Cab::prepare()` resets the filters. It never resets the live engine's histories.
Export describes settled parameter values, not a time-varying crossfade or an
automation trajectory in progress. Rendering remains on the message thread.

A complete WAV is encoded in memory, written to a sibling temporary file, and
its buffered write/flush status checked before a same-directory filesystem
rename. There is no destructive copy fallback. Errors retain the old target
and clean up the temporary file. If appending/replacing `.wav` changes the
selected path to another existing file, the editor asks about that actual
file before saving.

## Regression contract

`ChimeraCabExportTests` prepares the real `ChimeraProcessor`, runs production
`processBlock()` parameter mapping, then taps its owned `engine.cabinet(lane)`.
It does not use another export copy as its oracle. CAB-only isolation excludes
the other processing stages by design.

The matrix covers 44.1/48/96 kHz, three lanes and 64/257/1024-sample blocks:
modeled A/B (including the >512-block fallback), captured A-only/B-only/blend,
mono and independently different stereo user IRs, source-rate resampling from
48 kHz, long IRs, CAB off, and A/B cancellation. Tests compare the actual CAB
with the offline copy, exported WAV, and Chimera-reloaded IR using impulses and
independent-channel noise. They also exercise embedded project restoration
with the source WAV removed and verify uninterrupted live history across export.

Acceptance limits, with **no gain, phase or delay fitting**:

- Relative null residual below -75 dB and absolute RMS level difference <0.005 dB.
- Frequency response difference <0.1 dB and phase difference <0.5 degrees,
  evaluated from 20 Hz to 0.45 Fs where the reference is above -60 dB of its
  spectral peak. Phase at zeros/vanishing response is excluded.
- Identical impulse peak sample (0-sample shift).
- WAV sample rate, channel count and IEEE 32-bit floating-point data.
- Successful replacement, failed directory/parent destinations, injected short
  write/flush failures preserving existing bytes, temporary-file cleanup, and
  final-extension overwrite confirmation predicate.

The dedicated `Cabinet WAV round trip` workflow builds this target first on
Linux, Windows and macOS; a timeout building an unrelated target cannot prevent
its own CTest from running. Actual native file dialogs, physical audio devices,
commercial DAWs and third-party IR loaders still require platform acceptance.
