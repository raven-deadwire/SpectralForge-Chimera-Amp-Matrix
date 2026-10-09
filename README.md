# SpectralForge Chimera — Open Beta 1.3 preparation

**SpectralForge Chimera** is a guitar and bass amp & effects suite built around a flexible multi-amp architecture.

**[Latest published build: Open Beta 1.2 — Windows / macOS / Linux](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.2.0-beta.1)** · **[1.3 manual · EN / DE / KR](docs/MANUAL.html)** · [Published online manual](https://raven-deadwire.github.io/RavenForge-Luthier-Intelligence/manual/chimera.html)

This branch prepares `1.3.0-beta.1`, with numeric product version `1.3.0`. It contains **25 active amp heads, 39 PRE models and 21 POST models — 85 models in those three sections — plus 48 factory and signature presets**. Náströnd and Niflheimr each provide five Original channels. The expanded CAB section adds 14 speaker designs, 20 microphones, three tweeters and nine explicit enclosure layouts. The download above remains the published 1.2 build while 1.3 completes release validation. Existing native reference limitations remain documented in the [calibration report](docs/NATIVE_NAM_CALIBRATION.md). The offline EN/DE/KO manual is available under SETTINGS → Manual. Windows publisher signing and macOS Developer ID signing/notarization remain pending.

It works as a conventional single-amp simulator in **Classic Mode**, blends two complete amp rigs or splits low/high in **Dual Mode**, or splits the instrument signal into multiple frequency bands in **Matrix Mode**, blending a compressed LOW DI with a selectable head/cab beneath independent MID/HIGH rigs.

With integrated **Pre FX, Post FX, amp models, cabinet/IR processing, and flexible signal routing**, Chimera is designed to cover everything from traditional guitar and bass tones to extended-range, modern metal, and experimental hybrid amp configurations.

**Core Modes**

* **Classic** — Traditional single amp and cabinet signal chain
* **Dual** — Choose adjustable full-range Blend or a two-band LR4 Crossover
* **Matrix** — Split the spectrum into LOW COMP + DI/head blend and MID/HIGH amp/cab rigs

**RavenForge Luthier Intelligence**
*Forge your signal. Build your amp.*

## Prepared Open Beta 1.3 features

Implemented signal path: **input/gate/transpose -> PRE pedalboard -> rigs/cabinets -> merge -> POST rack -> doubler/output**. The chromatic tuner taps the input before the gate and transpose; optional auto-mute silences the output while tuning.

- Twenty-five active amplifier models with model-specific controls, channels/input paths and 1x/2x/4x/8x oversampling (4x default). Náströnd retains 13 controls per channel; Niflheimr provides 14 controls and five bass voicings.
- Matrix LOW: one-knob VCA-style COMP, Level, Band Tone and DI/AMP blend. The compact view prioritizes Band Tone, LOW DI COMP and DI/AMP; ALL exposes the full amplifier controls. Classic/Dual settings are preserved. LOW receives the clean tap before Fuzz/Boost/Overdrive. Its DI and selected amp/cab branch have matching algorithmic delay. MID/HIGH retain Drive, Level and Band Tone. Classic/Dual Blend retain full-range EQ; Dual Crossover uses two Band Tone controls.
- PRE: five freely arranged slots, 39 models grouped into exclusive categories, immediate selection, duplicate/reorder, and model-specific parameter banks. Pedal reference names are small captions; four knobs use 2×2 and five knobs use 3+2 layouts. Older sessions retain their internal compatibility processing until the new board is edited. POST: Bus Compressor, Preamp, EQ, Modulation, Delay and Reverb in six rows with ALL detail panels; compressor/preamp/EQ choices open directly without an intermediate category.
- A/B snapshots and Save/Load reference files include parameters and embedded IRs. Repeatable synthetic DI and RMS-matched amp/cab A/B WAVs accompany Windows builds.
- CABINET provides modeled speakers, enclosure behavior, tweeters and independently selected Mic A/B models and target speakers. IR LOADER provides two embedded factory IRs plus WAV/AIFF import and drag-and-drop per rig. Personal and external captures require local import and are not included in public packages. User IR audio is embedded in the DAW project for recall after moving the original file. Cabinet bypass, cuts, status, mute/solo and polarity are exposed.
- Global input/output gain and peak meters; gate threshold/range/hold/release; polyphonic transpose (-12 to +12 semitones); tuner with A4 calibration and auto-mute.
- 48 presets: 31 factory starting points, three Deadwire bass signatures, four guitar signatures, five Original Náströnd rigs and five Original Niflheimr bass rigs. File-based references, input Stereo/Mono L, stereo Doubler, tuner, persistent MIDI CC Learn, Tap/manual/host BPM and a practice metronome remain available. Delay can follow quarter-note tempo.
- User-supplied emblem icon, SpectralForge/CHIMERA wordmark, model-specific hardware artwork and native control panels, continuous resizing and 75/100/125/150% size choices. Audio controls expose host automation and project recall; older normalized PRE model-selection automation needs review after the three-to-five-choice expansion.

Amp names describe target voicings. **Hardware reproduction accuracy has not been established.** See [validation scope and limits](docs/AMP_VALIDATION.md), [six new NAM comparisons](docs/OPEN_BETA_NAM_VALIDATION.md), [third-party credits](docs/THIRD_PARTY_NOTICES.md) and the test log included in Windows packages. See the [IR placement, effects references and mode decisions](docs/FX_AND_IR_DESIGN.md).

## Interface and cabinet collection

Publisher: RavenForge Luthier Intelligence. The interface uses model-specific artwork plus the workbench background and brand assets, with live JUCE controls. Pedals
and racks keep rectangular layouts, interpreting each reference's material,
panel and knob details. The two Mu-Tron modes intentionally share one body.

CABINET offers **8 guitar and 6 bass speaker designs, 20 microphones (9 dynamic,
3 ribbon, 8 condenser) and 3 tweeters**. The nine layouts are guitar **1×12,
2×12 and 4×12**, and bass **1×15, 2×10, 4×10, 6×10, 1×12 and 2×12**. Guitar 4×12
has one explicit menu entry. Speaker choices follow the enclosure type and
diameter; changing a driver does not resize its enclosure or microphones.

The modeled bass 6×10 uses two columns and three rows, reusing the existing
speaker/response models with a six-driver layout and its designed enclosure
volume. Unpublished CAB preview states that used the modeled 8×10 layout now
resolve to 6×10; old Mic A/B target 7 maps to 5 and target 8 maps to 6, preserving
the column and original saved target values. Imported Ampeg/SVT 8×10 IR capture
names, metadata and recorded audio retain their original identity.

Heads, cabinets, speakers and microphones use one common physical scale within
each view. Framing follows active rigs and enabled microphones, so unused lanes
and disabled microphones do not reserve empty margins. Each speaker has its
own front-facing design; Crimson 12 retains its red finish. IR LOADER shows
independent A/B capture artwork. Matrix LOW blends DI with AMP+CAB, separately
from the cabinet's Mic A/B blend.

The next interface update is planned to add direct knob adjustment inside the
amp-head artwork. Version 1.3 uses the existing amplifier control panels.

IR LIBRARY distinguishes the two factory IRs and documented external/personal
capture metadata. Missing catalog entries cannot be loaded. Add WAV/AIFF files
with **OPEN IR** or an extracted folder with **ADD FOLDER**. The public installer
includes only the two licensed factory audio assets and reference metadata.
Personal capture data is not in this repository or the release downloads.
See [installation instructions](docs/INSTALLATION.md)
and [graphics/DSP update details](docs/GRAPHICS_DSP_UPDATE.md).

Open Beta 1.3 is in preparation. Publication requires its own three-platform builds, package checks and release acceptance evidence. Windows shutdown changes address a reproduced JUCE VBlank failure loop and release shared image/IR resources earlier. The maintainer’s October 4 acceptance applied to 1.1.1; synthetic voicing measurements do not establish new recorded-DI listening acceptance. Existing sessions retain saved values but may sound different with revised native DSP and the modeled enclosure change described above. See [1.3 release validation and known limits](docs/OPEN_BETA_RELEASE_NOTES.md) and [shutdown diagnostics](docs/STUDIO_ONE_TEARDOWN.md). Hardware equivalence is not claimed. **Transpose latency remains about 43–46 ms, and low-B playing at -2 semitones can smear note body and attack; further pitch-engine improvements are deferred.** Windows signing is prepared in `Tools/Sign-WindowsArtifact.ps1`
and `Tools/Build-WindowsInstaller.ps1 -Sign`; a CA-validated publisher identity is
still required. See `docs/WINDOWS_SIGNING.md` for the concrete signing path and the
difference between publisher identity and SmartScreen reputation.

An x64 standalone **MSIX** packaging path is now available in
`Tools/Build-WindowsMSIX.ps1`: unsigned review, Partner Center identity-bound Store
submission, and trusted-certificate direct distribution. MSIX is not itself a
publisher certificate. Microsoft signs a Store package after certification; the
current review package has not received that certification. The DAW VST3 still
uses the standard-folder Setup installer. See [MSIX scope and signing](docs/WINDOWS_MSIX.md).

## Installation, help and release

- [Open the detailed EN / DE / KR manual](https://raven-deadwire.github.io/RavenForge-Luthier-Intelligence/manual/chimera.html) — routing, model-family comparisons, illustrated PRE/AMP/POST model cards, IR use, presets, workflows, troubleshooting and support.
- [Platform installation](docs/INSTALLATION.md) — Windows Setup, macOS universal PKG (app/VST3/AU), Linux x86_64 DEB/TAR.
- [Prepared Beta 1.3 release notes](docs/OPEN_BETA_RELEASE_NOTES.md), [amp controls](docs/AMP_NATIVE_DSP.md), [factory and Deadwire preset guide](docs/PRESETS.md).
- [Maintainer release checklist](docs/RELEASE_CHECKLIST.md). The `prepare-release.yml` workflow produces draft-ready artifacts and a hash-based update manifest; it never publishes externally.

SETTINGS → Manual / Bug report / Updates provides an embedded offline manual and user-reviewed bug reports. Hosted VST3/AU opens the releases page without an HTTP updater; Standalone retains beta update checks. Downloads are verified against same-repository release metadata, byte size and SHA256; users save/close their host before installation. Publisher signing and macOS notarization remain pending. The published prerelease includes `update-beta.json` for beta-channel discovery. Windows Setup supports separate app and VST3 paths. This free beta may become paid; free support may end after beta.

---

Copyright © 2026 RavenForge Luthier Intelligence. All rights reserved.

Third-party software and assets are subject to their respective copyright notices and license terms.
