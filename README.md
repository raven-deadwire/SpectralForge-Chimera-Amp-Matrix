# SpectralForge Chimera — Open Beta 1.0

**SpectralForge Chimera** is a guitar and bass amp & effects suite built around a flexible multi-amp architecture.

It works as a conventional single-amp simulator in **Classic Mode**, blends two complete amp rigs or splits low/high in **Dual Mode**, or splits the instrument signal into multiple frequency bands in **Matrix Mode**, blending a compressed LOW DI with a selectable head/cab beneath independent MID/HIGH rigs.

With integrated **Pre FX, Post FX, amp models, cabinet/IR processing, and flexible signal routing**, Chimera is designed to cover everything from traditional guitar and bass tones to extended-range, modern metal, and experimental hybrid amp configurations.

**Core Modes**

* **Classic** — Traditional single amp and cabinet signal chain
* **Dual** — Choose adjustable full-range Blend or a two-band LR4 Crossover
* **Matrix** — Split the spectrum into LOW COMP + DI/head blend and MID/HIGH amp/cab rigs

**RavenForge Luthier Intelligence**
*Forge your signal. Build your amp.*

## Open Beta 1.0 release candidate

Implemented signal path: **input/gate/transpose -> PRE pedalboard -> rigs/cabinets -> merge -> POST rack -> doubler/output**. The chromatic tuner taps the input before the gate and transpose; optional auto-mute silences the output while tuning.

- Fifteen independently voiced amp algorithms with 1x/2x/4x/8x oversampling (4x default).
- Matrix LOW: one-knob VCA-style COMP, Level, Band Tone and DI/AMP blend. The head drive is fixed at 0 and hidden; Classic/Dual drive settings are preserved. LOW receives the clean tap before Fuzz/Boost/Overdrive. Its DI and selected amp/cab branch have matching algorithmic delay. MID/HIGH retain Drive, Level and Band Tone. Classic/Dual Blend retain full-range EQ; Dual Crossover uses two Band Tone controls.
- PRE pedalboard: Envelope Filter, Compressor, Fuzz, Boost, Overdrive, with five models per family. TOUCH puts Envelope first; SUSTAIN puts Compressor first. Gain order switches between Fuzz → Boost → Overdrive and Fuzz → Overdrive → Boost. Older states retain their previous order. POST rack: Bus Compressor, Preamp, three-band EQ, Modulation, Delay, Reverb, with 21 model choices. Gate and Transpose remain permanently accessible in the top strip.
- A/B snapshots and Save/Load reference files include parameters and embedded IRs. Repeatable synthetic DI and RMS-matched amp/cab A/B WAVs accompany Windows builds.
- Two embedded factory IRs, plus a WAV/AIFF loader, direct installed-file CAB menu and drag-and-drop per rig. The private full installer pack contains another 26 real WAV captures, including fifteen bass captures and seven Karnivore variants. User IR audio is embedded in the DAW project, so moving the original file does not break recall. Cabinet bypass, cuts, status, mute/solo and polarity are exposed.
- Global input/output gain and peak meters; gate threshold/hold/release; polyphonic transpose (-12 to +12 semitones); tuner with A4 calibration and auto-mute.
- 31 recommended starting presets grouped by instrument and playing role, and file-based references, input Stereo/Mono L, stereo Doubler; bottom tuner, persistent MIDI CC Learn, Tap/manual/host BPM and a practice metronome. Delay can follow quarter-note tempo.
- User-supplied emblem icon, newly generated SpectralForge/CHIMERA wordmark, model-specific generated hardware imagery with live native controls, continuous resizing and 75/100/125/150% size choices. All audio controls expose host automation and project recall; older normalized PRE model-selection automation needs review after the three-to-five-choice expansion.

Amp names describe target voicings. **Hardware reproduction accuracy has not been established.** See [validation scope and limits](docs/AMP_VALIDATION.md), [six new NAM comparisons](docs/OPEN_BETA_NAM_VALIDATION.md), [third-party credits](docs/THIRD_PARTY_NOTICES.md) and the test log included in Windows packages. See the [IR placement, effects references and mode decisions](docs/FX_AND_IR_DESIGN.md).

## Interface and cabinet collection

Publisher: RavenForge Luthier Intelligence. The interface uses 64 model artwork
surfaces plus the workbench background and brand assets, with live JUCE controls. Pedals
and racks keep rectangular layouts, interpreting each reference's material,
panel and knob details. The two Mu-Tron modes intentionally share one body.

IR LIBRARY distinguishes the two factory IRs and 26 documented personal captures,
including fifteen bass captures and seven Eminence Karnivore guitar captures. Missing catalog entries cannot be loaded. Extract the entire private
installer ZIP so its actual WAV companion folder remains beside Setup, import the
updated personal ZIP, or add an extracted folder. The public bare installer has
only the two factory audio assets and reference metadata. Personal capture data
is not in this repository. See [installation instructions](docs/WINDOWS_INSTALL.txt)
and [graphics/DSP update details](docs/GRAPHICS_DSP_UPDATE.md).

This is an Open Beta 1.0 release candidate. CI passing and manual listening/DAW checks are required before publication; hardware equivalence is not claimed. Windows signing is prepared in `Tools/Sign-WindowsArtifact.ps1`
and `Tools/Build-WindowsInstaller.ps1 -Sign`; a CA-validated publisher identity is
still required. See `docs/WINDOWS_SIGNING.md` for the concrete signing path and the
difference between publisher identity and SmartScreen reputation.

An x64 standalone **MSIX** packaging path is now available in
`Tools/Build-WindowsMSIX.ps1`: unsigned review, Partner Center identity-bound Store
submission, and trusted-certificate direct distribution. MSIX is not itself a
publisher certificate. Microsoft signs a Store package after certification; the
current review package has not received that certification. The DAW VST3 still
uses the standard-folder Setup installer. See [MSIX scope and signing](docs/WINDOWS_MSIX.md).

## Installation, help and release preparation

- [Korean / English offline manual](docs/MANUAL.html) — routing, controls, amp/IR use, presets, troubleshooting and support.
- [Platform installation](docs/INSTALLATION.md) — Windows Setup, macOS universal PKG (app/VST3/AU), Linux x86_64 DEB/TAR.
- [Beta release notes](docs/OPEN_BETA_RELEASE_NOTES.md), [15 amp references](docs/AMP_VOICES_OPEN_BETA.md), [31 preset guide](docs/PRESETS.md).
- [Maintainer release checklist](docs/RELEASE_CHECKLIST.md). The `prepare-release.yml` workflow produces draft-ready artifacts and a hash-based update manifest; it never publishes externally.

SETTINGS → Manual / Bug report / Updates provides an embedded offline manual, user-reviewed bug reports and beta update checks. Downloads are verified against same-repository release metadata, byte size and SHA256; users save/close their host before installation. Code signing and macOS notarization remain separate pending release work. An unposted candidate is not yet available to the updater.
