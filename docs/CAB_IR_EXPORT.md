# Exporting a rig's cabinet as an IR

Each rig's cabinet menu (the IR selector next to the cabinet) has
**Export this cabinet as WAV...** at the bottom. It writes the cabinet as
that rig hears it now, as an impulse response file:

- the captured IR or the modeled cabinet on mic A,
- mic B with its blend, gain, delay and polarity, and its own cuts,
- the rig's cabinet low and high cuts,
- cabinet off: a plain impulse.

Amp, effects, EQ and dynamics are not part of the file. A modeled cabinet's
IR is its linear response only.

## File

- WAV, 32-bit float, at the session's sample rate (48 kHz when the plug-in
  has not been prepared).
- Mono when both channels are identical (the usual case), otherwise stereo.
- Not normalised: the level is the cabinet's own, so the file sounds the
  same as the rig in another IR loader that does not normalise.
- The silent tail (below −120 dB re the peak) is trimmed, keeping at least
  10 ms; the longest render is 2 s. Mic delays and pre-delay are kept.

## How it is rendered

`ChimeraProcessor::renderCabinet` builds an offline copy of the three
cabinets with their own IR library (`IRLibrary`, restored from the project's
IRs, so a project IR exports without the user folder). The same parameter
mapping as the live engine (`applyCabinetParameters`) sets it up, the
library builds every requested kernel synchronously, and an impulse goes
through the rig's cabinet. The running engine and its worker are not
touched, so exporting during playback is safe; it takes about half a second
on the message thread.

## Tests

`Tests/CabExportTests.cpp` (`ChimeraCabExportTests`):

- Modeled (preset 0) and captured-blend lanes give a finite, non-empty,
  deterministic IR.
- Noise through the cabinet copy equals the noise convolved with the
  exported IR (−85 dB modeled, −91 dB captured blend).
- The export follows the lane: changing the mic blend changes it, inverting
  mic B flips its sign, cabinet off exports a plain impulse.
- The WAV reads back bit-identical as 48 kHz 32-bit float mono, and
  Chimera loads it as a project IR.

Not covered: a check against the live engine's own cabinet during playback
(the mapping is shared code, not a separate test), stereo-different cabinets,
and loading the file in other hosts' IR loaders.
