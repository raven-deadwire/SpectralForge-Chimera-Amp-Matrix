# SpectralForge Chimera — Open Beta 1.3.2

**SpectralForge Chimera** is a guitar and bass amp & effects suite built around a flexible multi-amp architecture.

**[Published builds — Windows / macOS / Linux](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases)** · **[Manual · EN / DE / KR](docs/MANUAL.html)**

As of 2026-10-18 KST, the public version is [`1.3.2-beta.1`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.3.2-beta.1) (product `1.3.2`, source `3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58`). Its approved expiry is **2027-02-11T14:59:00Z / 2027-02-11 23:59:00 KST**. The SETTINGS countdown improvement on this branch is not yet published; the beta.1 date does not approve any future version. See the [lifecycle policy](docs/BETA_LIFECYCLE.md). Direct head controls, revised model-specific knobs, Signal Path navigation and two graphical EQ stages extend the published 1.3.0 feature set. It contains **25 active amp heads, 39 PRE models and 21 POST models — 85 models in those three sections — plus 48 factory and signature presets**. Náströnd and Niflheimr each provide five Original channels. The expanded CAB section adds 14 speaker designs, 20 microphones, three tweeters and nine explicit enclosure layouts. The releases page lists available downloads and their validation records; the published 1.3.0 tag and packages remain unchanged. Product-wide manual acceptance remains incomplete; see the [release scope](docs/OPEN_BETA_RELEASE_NOTES.md). Model controls and musical roles use Chimera naming throughout the app and manual; [naming and saved sounds](docs/PUBLIC_PRODUCT_NAMING.md) explains the current presentation. The offline EN/DE/KO manual is available under SETTINGS → Manual. Windows publisher signing and macOS Developer ID signing/notarization remain pending.

It works as a conventional single-amp simulator in **Classic Mode**, blends two complete amp rigs or splits low/high in **Dual Mode**, or splits the instrument signal into multiple frequency bands in **Matrix Mode**, blending a compressed LOW DI with a selectable head/cab beneath independent MID/HIGH rigs.

With integrated **Pre FX, Post FX, amp models, cabinet/IR processing, and flexible signal routing**, Chimera is designed to cover everything from traditional guitar and bass tones to extended-range, modern metal, and experimental hybrid amp configurations.

**Core Modes**

* **Classic** — Traditional single amp and cabinet signal chain
* **Dual** — Choose adjustable full-range Blend or a two-band LR4 Crossover
* **Matrix** — Split the spectrum into LOW COMP + DI/head blend and MID/HIGH amp/cab rigs

**RavenForge Luthier Intelligence**
*Forge your signal. Build your amp.*

## Feature overview (includes the 1.3.1 baseline)

Implemented signal path: **input → transpose / PRE pedalboard → rigs/cabinets → merge → Tone EQ → POST rack → doubler/metronome → Final EQ → output**. The gate can operate before transpose or after the rig merge, before Tone EQ. The chromatic tuner taps the input before the gate and transpose; optional auto-mute silences the output while tuning.

- **Easy view** (not yet published) is a simpler start screen for new players: a preset menu, six large knobs (GAIN, BASS, MIDDLE, TREBLE, REVERB, VOLUME), Auto Set and the tuner. Each knob edits the same setting as the full editor, so switching views never changes the sound. Chimera opens in Easy view the first time and afterwards in the view you last chose. See [Easy view](docs/EASY_VIEW.md).
- Select a head in Classic, Dual or Matrix and open its enlarged front panel. Niflheimr has 14 knobs and five channels; Brit Edge has six knobs and five input paths. Direct control layouts cover all 26 catalog heads, including the retired Iron Compact reference; the selectable amp inventory remains 25. DETAILS opens the complete existing control panel.
- Model-specific knobs follow the reference hardware’s shape, grip, cap and pointer, with distinct Original designs for Náströnd and Niflheimr. Larger labels and revised knob spacing improve legibility. Numeric values appear while adjusting or entering a value, keeping the idle head face clear.
- Signal Path provides six editing stages: PRE → HEAD → CAB → TONE EQ → POST → FINAL EQ. HEAD opens the current mode's existing amp controls; CAB opens its inline cabinet room, where you can choose a lane for detailed editing. ROUTING opens the complete path with global utilities, branches and sums. Existing global controls remain available in their established locations, and navigation preserves the sound settings.
- Tone EQ and Final EQ each provide 12 adjustable bands, seven filter types, a response curve and input/output spectrum display. Tone EQ shapes the merged rigs before POST; Final EQ shapes the result after POST and stereo width. Both start bypassed.
- Twenty-five active amplifier models with model-specific controls, channels/input paths and 1x/2x/4x/8x oversampling (4x default). Náströnd retains 13 controls per channel; Niflheimr provides 14 controls and five bass voicings.
- Matrix LOW: one-knob VCA-style COMP, Level, Band Tone and DI/AMP blend. The compact view prioritizes Band Tone, LOW DI COMP and DI/AMP; ALL exposes the full amplifier controls. Classic/Dual settings are preserved. LOW receives the clean tap before Fuzz/Boost/Overdrive. Its DI and selected amp/cab branch have matching algorithmic delay. MID/HIGH retain Drive, Level and Band Tone. Classic/Dual Blend retain full-range EQ; Dual Crossover uses two Band Tone controls.
- PRE: five freely arranged slots, 39 models grouped into exclusive categories, immediate selection, duplicate/reorder, and model-specific parameter banks. Model captions use Chimera names; four knobs use 2×2 and five knobs use 3+2 layouts. Older sessions retain their internal compatibility processing until the new board is edited. POST: Bus Compressor, Preamp, EQ, Modulation, Delay and Reverb in six rows with ALL detail panels; compressor/preamp/EQ choices open directly without an intermediate category.
- A/B snapshots and Save/Load reference files include parameters and embedded IRs. Synthetic validation evidence is kept separately from public installer payloads.
- CABINET provides modeled speakers, enclosure behavior, tweeters and independently selected Mic A/B models and target speakers. IR LOADER provides two embedded factory IRs with verified redistribution licenses plus WAV/AIFF import and drag-and-drop per rig. Unverified external and private capture catalogs are excluded from the public library. User IR audio is embedded in the DAW project for recall after moving the original file. Cabinet bypass, cuts, status, mute/solo and polarity are exposed.
- Global input/output gain and peak meters; gate threshold/range/hold/release; polyphonic transpose (-12 to +12 semitones); tuner with A4 calibration and auto-mute.
- 48 presets: 31 factory starting points, three Deadwire bass signatures, four guitar signatures, five Original Náströnd rigs and five Original Niflheimr bass rigs. Each now recalls an authored CAB recipe for its rigs while retaining preset IDs, names and bank order; intentional dry-DI lanes keep CAB bypassed. See the [48-preset guide](docs/PRESETS.md). File-based references, input Stereo/Mono L, stereo Doubler, tuner, persistent MIDI CC Learn, Tap/manual/host BPM and a practice metronome remain available. Delay can follow quarter-note tempo.
- User-supplied emblem icon, SpectralForge/CHIMERA wordmark, model-specific hardware artwork and native control panels, continuous resizing and 75/100/125/150% size choices. Audio controls expose host automation and project recall; older normalized PRE model-selection automation needs review after the three-to-five-choice expansion.

Amp names describe target voicings. **Hardware reproduction accuracy has not been established.** See [release validation scope and limits](docs/OPEN_BETA_RELEASE_NOTES.md), [third-party credits](docs/THIRD_PARTY_NOTICES.md) and the verification record included with the package. The [manual](docs/MANUAL.html) describes routing and each model’s controls.

## Interface and cabinet collection

Publisher: RavenForge Luthier Intelligence. The interface uses model-specific artwork plus the workbench background and brand assets, with live JUCE controls. Pedals
and racks keep rectangular layouts, interpreting each reference's material,
panel and knob details. The Band Sweep and Reverse Sweep modes intentionally share one body.

CABINET offers **8 guitar and 6 bass speaker designs, 20 microphones (9 dynamic,
3 ribbon, 8 condenser) and 3 tweeters**. The nine layouts are guitar **1×12,
2×12 and 4×12**, and bass **1×15, 2×10, 4×10, 6×10, 1×12 and 2×12**. Guitar 4×12
has one explicit menu entry. Speaker choices follow the enclosure type and
diameter; changing a driver does not resize its enclosure or microphones.

The modeled bass 6×10 uses two columns and three rows, reusing the existing
speaker/response models with a six-driver layout and its designed enclosure
volume. Unpublished CAB preview states that used the modeled 8×10 layout now
resolve to 6×10; old Mic A/B target 7 maps to 5 and target 8 maps to 6, preserving
the column and original saved target values. Imported 8×10 IR capture
names, metadata and recorded audio retain their original identity.

Heads, cabinets, speakers and microphones use one common physical scale within
each view. Framing follows active rigs and enabled microphones, so unused lanes
and disabled microphones do not reserve empty margins. Each speaker has its
own front-facing design; Crimson 12 retains its red finish. IR LOADER shows
independent A/B capture artwork. Matrix LOW blends DI with AMP+CAB, separately
from the cabinet's Mic A/B blend.

The enlarged head is a working control surface. Drag a knob, hold Shift for fine
adjustment, or double-click / press Enter to type a value. Switch channels using
the named buttons and use DETAILS for the full controls. Head handles, top corners
and enclosure depth are framed together, with controls sized for the faceplate.
Hardware references guide the visual design; illustrations are not product photos
and do not establish sonic equivalence.

IR LIBRARY starts with **two factory IRs distributed under CC BY 4.0**, plus
the files you import with **OPEN IR** or an extracted folder with **ADD FOLDER**.
The public release excludes unverified external/private capture catalogs,
private research documents and reference-audio payload folders. Existing imported IRs
and their stored project/A·B audio remain available. See the
[IR distribution policy](docs/IR_DISTRIBUTION.md).

The 48 factory recipes define **71 active CAB lanes**, including twelve 6×10
lanes, with six intentional dry LOW lanes. Mic A starts enabled and Mic B is
configured but disabled; 6×10 recipes target unit 3 in the middle row. Factory
recall selects the modeled CAB stage without loading A/B IR files. Existing
project and A/B files continue to restore their own saved parameters and IRs.
These recipes are authored starting points; final level and listening acceptance
remain part of release validation. See the [preset guide](docs/PRESETS.md).
See [installation instructions](docs/INSTALLATION.md)
and [model and workflow guide](docs/MANUAL.html).

Open Beta 1.3.1 publication requires its own three-platform product builds, native head/Signal Path/EQ checks, CAB checks, package checks and installer evidence. The strict product validation inventory still includes unexecuted checks; the separate beta decision and remaining gaps are disclosed in the [1.3.1 publication pipeline](docs/RELEASE_1_3_1_PIPELINE.md). Actual instrument listening and Studio One operation have not been accepted for this version. Windows shutdown changes address a reproduced JUCE VBlank failure loop and release shared image/IR resources earlier. The maintainer’s October 4 acceptance applied to 1.1.1; synthetic voicing measurements do not establish new recorded-DI listening acceptance. Existing sessions retain saved values but may sound different with revised native DSP and the modeled enclosure change described above. See [1.3.1 release validation and known limits](docs/OPEN_BETA_RELEASE_NOTES.md) and [shutdown diagnostics](docs/STUDIO_ONE_TEARDOWN.md). Hardware equivalence is not claimed. **Transpose latency remains about 43–46 ms, and low-B playing at -2 semitones can smear note body and attack; further pitch-engine improvements are deferred.** Windows signing is prepared in `Tools/Sign-WindowsArtifact.ps1`
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

- [Open the candidate EN / DE / KR manual](docs/MANUAL.html) — routing, model-family comparisons, illustrated PRE/AMP/POST model cards, IR use, presets, workflows, troubleshooting and support.
- [Platform installation](docs/INSTALLATION.md) — Windows Setup, macOS universal PKG (app/VST3/AU), Linux x86_64 DEB/TAR.
- [Beta 1.3.1 release notes](docs/OPEN_BETA_RELEASE_NOTES.md), [amp controls](docs/MANUAL.html#amps), [factory and Deadwire preset guide](docs/PRESETS.md).
- [Maintainer release checklist](docs/RELEASE_CHECKLIST.md) and [1.3.1 publication pipeline](docs/RELEASE_1_3_1_PIPELINE.md). Publication uses newly built packages and a hash-based update manifest from the verified source.
- [Archived Beta 1.3.0 release notes](docs/OPEN_BETA_1_3_RELEASE_NOTES.md).

SETTINGS → Manual / Bug report / Updates provides an embedded offline manual and user-reviewed bug reports. Hosted VST3/AU opens the releases page without an HTTP updater; Standalone retains beta update checks. Downloads are verified against same-repository release metadata, byte size and SHA256; users save/close their host before installation. Publisher signing and macOS notarization remain pending. The published prerelease includes `update-beta.json` for beta-channel discovery. Windows Setup supports separate app and VST3 paths. This free beta may become paid; free support may end after beta.

---

Copyright © 2026 RavenForge Luthier Intelligence. All rights reserved.

Third-party software and assets are subject to their respective copyright notices and license terms.
