# SpectralForge — Chimera Amp Matrix

**Chimera Amp Matrix** is a guitar and bass amp & effects suite built around a flexible multi-amp architecture.

It works as a conventional single-amp simulator in **Classic Mode**, blends two complete amp rigs or splits low/high in **Dual Mode**, or splits the instrument signal into multiple frequency bands in **Matrix Mode**, keeping a clean compressed LOW DI foundation under independent MID/HIGH amp and cabinet rigs.

With integrated **Pre FX, Post FX, amp models, cabinet/IR processing, and flexible signal routing**, Chimera is designed to cover everything from traditional guitar and bass tones to extended-range, modern metal, and experimental hybrid amp configurations.

**Core Modes**

* **Classic** — Traditional single amp and cabinet signal chain
* **Dual** — Choose adjustable full-range Blend or a two-band LR4 Crossover
* **Matrix** — Split the spectrum into LOW DI + compression and MID/HIGH amp/cab rigs

**SpectralForge**
*Forge your signal. Build your amp.*

## Current development build

Implemented signal path: **input/gate/transpose -> PRE pedalboard -> rigs/cabinets -> merge -> POST rack -> doubler/output**. The chromatic tuner taps the input before the gate and transpose; optional auto-mute silences the output while tuning.

- Eight independently voiced amp algorithms with 1x/2x/4x/8x oversampling (4x default).
- Matrix LOW: clean DI, one-knob VCA-style COMP, Level and Band Tone. Fuzz, Boost, Overdrive, amp and IR are bypassed on LOW with matching processing delay. MID/HIGH retain Drive, Level and Band Tone. Classic/Dual Blend retain full-range EQ; Dual Crossover uses two Band Tone controls.
- PRE pedalboard: Compressor, Envelope Filter, Fuzz, Boost, Overdrive. POST rack: Bus Compressor, Preamp, three-band EQ, Chorus, Delay, Reverb. Gate and Transpose remain permanently accessible in the top strip.
- A/B snapshots and Save/Load reference files include parameters and embedded IRs. Repeatable synthetic DI and RMS-matched amp/cab A/B WAVs accompany Windows builds.
- Two bundled guitar cabinet IRs, plus a WAV/AIFF loader and drag-and-drop per rig. User IR audio is embedded in the DAW project, so moving the original file does not break recall. Cabinet bypass, cuts, status, mute/solo and polarity are exposed.
- Global input/output gain and peak meters; gate threshold/hold/release; polyphonic transpose (-12 to +12 semitones); tuner with A4 calibration and auto-mute.
- Factory starting presets and file-based references, input Stereo/Mono L, stereo Doubler; bottom tuner, persistent MIDI CC Learn, Tap/manual/host BPM and a practice metronome. Delay can follow quarter-note tempo.
- Vector UI with continuous resizing and 75/100/125/150% size choices. Host automation and project recall cover all audio controls.

Amp names describe target voicings. **Hardware reproduction accuracy has not been established.** See [validation scope and limits](docs/AMP_VALIDATION.md), [third-party credits](docs/THIRD_PARTY_NOTICES.md) and the test log included in Windows packages. See the [IR placement, effects references and mode decisions](docs/FX_AND_IR_DESIGN.md).
