# RavenForge Luthier Intelligence — Chimera Amp Matrix

**Chimera Amp Matrix** is a guitar and bass amp & effects suite built around a flexible multi-amp architecture.

It works as a conventional single-amp simulator in **Classic Mode**, blends two complete amp rigs or splits low/high in **Dual Mode**, or splits the instrument signal into multiple frequency bands in **Matrix Mode**, blending a compressed LOW DI with a selectable head/cab beneath independent MID/HIGH rigs.

With integrated **Pre FX, Post FX, amp models, cabinet/IR processing, and flexible signal routing**, Chimera is designed to cover everything from traditional guitar and bass tones to extended-range, modern metal, and experimental hybrid amp configurations.

**Core Modes**

* **Classic** — Traditional single amp and cabinet signal chain
* **Dual** — Choose adjustable full-range Blend or a two-band LR4 Crossover
* **Matrix** — Split the spectrum into LOW COMP + DI/head blend and MID/HIGH amp/cab rigs

**RavenForge Luthier Intelligence**
*Forge your signal. Build your amp.*

## Current development build

Implemented signal path: **input/gate/transpose -> PRE pedalboard -> rigs/cabinets -> merge -> POST rack -> doubler/output**. The chromatic tuner taps the input before the gate and transpose; optional auto-mute silences the output while tuning.

- Eight independently voiced amp algorithms with 1x/2x/4x/8x oversampling (4x default).
- Matrix LOW: one-knob VCA-style COMP, Level, Band Tone and DI/AMP blend. The head drive is fixed at 0 and hidden; Classic/Dual drive settings are preserved. LOW receives the clean tap before Fuzz/Boost/Overdrive. Its DI and selected amp/cab branch have matching algorithmic delay. MID/HIGH retain Drive, Level and Band Tone. Classic/Dual Blend retain full-range EQ; Dual Crossover uses two Band Tone controls.
- PRE pedalboard: Compressor, Envelope Filter, Fuzz, Boost, Overdrive. POST rack: Bus Compressor, Preamp, three-band EQ, Chorus, Delay, Reverb. Gate and Transpose remain permanently accessible in the top strip.
- A/B snapshots and Save/Load reference files include parameters and embedded IRs. Repeatable synthetic DI and RMS-matched amp/cab A/B WAVs accompany Windows builds.
- Two bundled guitar cabinet IRs, plus a WAV/AIFF loader and drag-and-drop per rig. User IR audio is embedded in the DAW project, so moving the original file does not break recall. Cabinet bypass, cuts, status, mute/solo and polarity are exposed.
- Global input/output gain and peak meters; gate threshold/hold/release; polyphonic transpose (-12 to +12 semitones); tuner with A4 calibration and auto-mute.
- Factory starting presets and file-based references, input Stereo/Mono L, stereo Doubler; bottom tuner, persistent MIDI CC Learn, Tap/manual/host BPM and a practice metronome. Delay can follow quarter-note tempo.
- Generated RavenForge hardware imagery with live native controls, continuous resizing and 75/100/125/150% size choices. Host automation and project recall cover all audio controls.

Amp names describe target voicings. **Hardware reproduction accuracy has not been established.** See [validation scope and limits](docs/AMP_VALIDATION.md), [third-party credits](docs/THIRD_PARTY_NOTICES.md) and the test log included in Windows packages. See the [IR placement, effects references and mode decisions](docs/FX_AND_IR_DESIGN.md).

## RavenForge interface and cabinet collection

Publisher: RavenForge Luthier Intelligence. The interface uses fourteen generated
RavenForge raster assets with live JUCE controls, rather than flat color plates.
IR LIBRARY shows two factory IRs and six documented personal references, including
Ampeg 8x10/MD421 and Bassman CTS 2x15/SM57, with explicit installed/missing states.
Import the existing personal ZIP once, add a folder, or install a private companion
pack alongside Setup. Those personal captures are not in this public repository.

This remains a development build, not a claim of commercial release readiness or
verified hardware equivalence. Windows signing is prepared in `Tools/Sign-WindowsArtifact.ps1`
and `Tools/Build-WindowsInstaller.ps1 -Sign`; a CA-validated publisher identity is
still required. See `docs/WINDOWS_SIGNING.md` for the concrete signing path and the
difference between publisher identity and SmartScreen reputation.
