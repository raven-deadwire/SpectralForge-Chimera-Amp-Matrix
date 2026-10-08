# 1.2 development and remaining updates

## Current roadmap update — 2026-10-08 KST

Open Beta 1.2 was [published on 2026-10-05](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.2.0-beta.1).
Versions 1.1.1, 1.1.2 and 1.2 must not be rescheduled as unshipped work.
The historical checkpoint below retains the evidence and limitations recorded
on October 5; it is not the current release status.

Add **1.3.x CAB panel** as a shared feature alongside the Niflheimr cycle:
per-lane two-microphone editing, measured position/distance, blend/phase tools
and preserved personal IR management. Target 7 guitar cabinets, 5 bass cabinets
and 8 shared mic types, separately counted from the 132-model roadmap.
See [CAB scope, target dates, inventory and acceptance](CABINET_PANEL_ROADMAP.md).

| CAB milestone | Internal target, KST |
|---|---|
| Specification and capture-bank feasibility | 2026-10-08–10-11 |
| Two-slot engine, state migration and CAB panel | 2026-10-12–10-25 |
| Qualified-grid alpha and factory starting presets | 2026-10-26–11-08 |
| Niflheimr integration, product validation and 1.3.x candidate | 2026-11-09–12-04 |

The daily development and weekday Astra queues both include this workstream.
Dates depend on qualified audio assets and evidence; independent work continues
when a particular input is missing. PR #24's current Niflheimr/IR work is a
dependency to inspect and preserve, not an already merged implementation.

## Historical 1.2 checkpoint — 2026-10-05

Status: Open Beta 1.2 release preparation, 2026-10-05 KST. The owner authorized
publication at 19:10 KST after the final channel-level and Fimbulvetr clarity
corrections. VERSION is 1.2.0 and RELEASE_CHANNEL is beta.1. Final-source
platform, installer and immutable-package verification is required before
publishing. Earlier preview measurements below are historical checkpoints;
current channel/preset corrections are in NASTROND_CHANNEL_FEEDBACK.md.

## Immediate preset correction

The owner requested lead/high-gain presets to sustain at least the drive of
their Melodic Death Rhythm example. The subsequent Orange Heavy screenshots
provide a separate reference for that preset. The screenshot UI says CUSTOM;
the owner supplied the preset identity. Displayed positions map to 0–1 controls.

| Reference | AMP | PRE, in signal order |
|---|---|---|
| Melodic Death Rhythm | Tight 515, LEAD, HIGH GAIN INPUT, PRE GAIN 8.6, POST GAIN 4.0; LOW 10, other visible EQ/power knobs 5 | Variable Mu at visible noon controls, 0.6 s, COMPRESS; Treble Lift 5; Green Drive 3 / 6 / 5 |
| Orange Heavy | Orange Crown, DIRTY, Gain 7.6, Treble 5, Middle 6.2, Bass 4.3, Volume 5, Reverb 0, Attenuator 5.2, FULL | Studio FET IN/OUT/DRY/RATIO/ATTACK/RELEASE 4 / 5.4 / 2.8 / 2 / 2.2 / 5; Treble Lift 5; Green Drive 10 / 10 / 5 |
| Tight Rhythm | Tight 515, RHYTHM, HIGH GAIN INPUT, PRE GAIN 7.6, POST GAIN 6.3, BRIGHT/CRUNCH ON; offscreen EQ retained | Optical COMP/EQ/VOL 5 / 10 / 5, EQ IN and HI-CUT ON; Treble Lift bypassed; Green Drive 10 / 10 / 10 |
| G+G Tight / Wide | DUAL BLEND 65:35; Tight 515 LEAD/HIGH GAIN INPUT + Wide Rect CH3/SOLO ON; gain knobs are offscreen, use authored 9.5 settings with +3 dB native input trims | Variable Mu at visible noon controls, 0.6 s, COMPRESS; Treble Lift 5; Green Drive 0.4 / 6.8 / 4.5 |

Q Sweep and Big Sustain are bypassed in all four supplied examples. Their hidden
settings are not inferred. The reference captures are settings evidence, not
audio recordings. Cabinet assets differ between the screenshots; private IRs
are not bundled or replaced. Existing NAM calibration coefficients remain intact.
The four examples also recall the visible rack settings: Console VCA,
Iron Colour +5 dB / inverted phase / −5 dB output for the Tight 515 sounds,
and Blue Channel INST with Passive Tube EQ for Orange Heavy. Instrument mode
uses its instrument gain; the displayed main-channel gain/trim do not add drive
to that path. End-of-chain output is rebalanced independently of the screenshots.

The affected bank covers Tight Rhythm, Filter Lead, Fuzz Texture, Orange Heavy,
Melodic Death Rhythm, Melodic Lead, G+G Tight / Wide, Dumble Smooth Lead and all
four guitar signatures. Preset IDs remain stable. At the owner's request,
ID30 is renamed **Modern Clean** and its public description uses Taste Punch.
Historical evidence retains the name that was actually tested at the time. Guitar signatures
retain their dual/matrix architecture and clean LOW foundation. Output trims
are adjusted separately after PRE/AMP voicing; project restore does not call
factory recall and does not replace a user's saved gain settings.

The gain regression resolves actual preset state and renders the production
PRE and native AMP engines at 4x. It compares weak/strong harmonic growth on
the same tone stack, and output growth over a 30 dB input span at 150/300/600 Hz.
Tight Rhythm uses the owner's later RHYTHM/BRIGHT/CRUNCH reference as its target.
The Dual reference uses its own lower-drive pedal settings, with released 1.1.2
amp settings as a lower bound for offscreen knobs; those values are explicitly
assumed, not read from the image. Other lead/high-gain paths use the Melodic Death
LEAD reference. These different recipes are not reduced to one shared knob value.
These measurements separate saturation from final output volume; they do not
certify identical timbre or replace an instrument-DI listening comparison.
Envelope-filter sweeps are bypassed only in this isolated gain probe because
their level-dependent EQ changes harmonic ratios. Full-preset level and recall
tests run their actual enabled filters, cabinets, rack and spatial effects.

Final local regression passed all **38 full-chain presets** and **17 isolated
gain paths**. Four guitar signatures restore 3,891 parameters with zero measured
audio difference. The synthetic preset RMS range is −29.67 to −25.69 dBFS;
worst nominal peak is −9.97 dBFS and worst +6 dB input peak is −4.81 dBFS.
The 26 presets without gain revoicing retain their prior fixture levels within
0.001 dB, including the renamed Modern Clean. The complete per-preset review and
source hashes are in [development evidence](evidence/development-1.2-20261005/README.md).

## Náströnd integrated in the 1.2 Preview

| Area | Preview implementation | Remaining acceptance |
|---|---|---|
| Catalog | Append-only model ID24, 24 active amps / 84 active models | Public release remains separate |
| DSP | Same reference-informed Original core in the production native oversampler | Instrument-DI listening and real-host CPU |
| Controls / UI | 13 controls, Hz MID FREQ, Original head artwork, compact and ALL panels | Owner evaluation |
| Routing | Classic, both Dual lanes and all three Matrix bands; existing latency and switching path | Target DAW audition |
| State | 90 appended production parameters; prior 3,891 indices unchanged | Old project smoke test in target DAW |
| Presets | Five complete Original rigs at indices38–42; 43 selectable presets | Instrument-specific adjustment |

Production uses the native bank's existing engine switch, plus 13 Original
controls and two structural selector parameters per context. The isolated
84-parameter development harness remains a separate test fixture, not the
production parameter layout. See [Preview integration](NASTROND_PREVIEW.md).

The five Original presets are five sounds of one amp. They do not replace the
existing Factory31, bass signatures3 or guitar signatures4. Development tests
cover control response, weak-note gain, stereo isolation, state roundtrip,
inactive-context preservation, block partitioning and allocation behavior.

Reference roles remain Meshuggah-family CLANK, VH4 CRUSH, Uberschall **Twin Jet**
IMPACT, Omega Granophyre ROT and Matamp GT1 BLOOM. The October 5 follow-up acquired
and measured 26 distinct NAM files from those five named families against the
actual development wrapper. See [Náströnd reference comparison](NASTROND_REFERENCE_COMPARISON.md).
Meshuggah captures include cabinets; an exact head-only anchor is still missing.
Granophyre physical provenance is uploader-asserted, and only the VH4 set has
absolute input calibration metadata. The observations show strong default
saturation but unresolved harmonic and spectral differences. They do not certify
five-head hardware matching or final musical acceptance. Evil Pumpkin and
Uberschall Rev Blue are not substituted for the requested anchors.

The subsequent [reference-informed voicing revision](NASTROND_VOICING.md) retains
default GAIN 7.2, strengthens IMPACT/ROT/BLOOM behavior, and fits a bounded broad
contour on plucks from four eligible head-reference families. Independent
synthetic chords show 35–43% smaller average spectrum differences. The final
26-NAM / 17-state comparison is complete; local core/integration/renderer tests pass.
This remains a development checkpoint, not instrument-DI or listening acceptance.

## Historical remaining work at the October 5 checkpoint

| Priority | Item | Actual status / next action |
|---|---|---|
| 1.2 | Náströnd integration | Catalog, routing, production panel, saved state/A-B and five presets connected. Complete exact-commit installer verification and owner listening |
| 1.2 | Gain acceptance | Reference-informed DSP voicing applied; default gain retained and 26 NAM / 17 states remeasured. Synthetic held-out spectrum differences reduced; instrument-DI comparisons and listening acceptance remain pending |
| 1.2 | Original reference coverage | Meshuggah head-only NAM and Granophyre physical provenance still needed; retain missing input calibration for four families |
| Validation | Exact reference coverage | EICH T900 exact NAM remains missing; retain the recorded limitation |
| Validation | Existing AMP references | 1.1.2 reviewed 174 distinct NAM files across 22 families. Preserve provenance; ZUTA whole-head/amp+cab metadata conflict is not erased |
| Validation | Hardware/control fidelity | Exact tapers, revision identity and physical circuit equivalence remain unverified where already qualified |
| QA | Host lifecycle / CPU | Existing live-removal and VBlank fixes are implemented; repeat relevant host/CPU coverage after 1.2 integration |
| Future | Transpose | Long-term quality/latency work; current 42.7–46.4 ms pitch path does not meet ≤10 ms and low-B downshift quality is unresolved |
| Future | NativeSearch | Distinguish local indexed content from a whole remote library; implementation pending |
| Future | Dual Mono | Separate feature from current Dual rig; implementation pending |
| Distribution | Signing | macOS signing/notarization and Windows code signing remain separate outstanding distribution work |

Historical issue text is not a fresh blocker: M104/JB-2 save/restore checks,
Gate Range, UI refresh improvements and VBlank shutdown fixes already exist.
Ironball is retired from new selection; keep serialized ID16 for saved sessions.

## Version scope, with October 8 CAB addition

| Version | Scope | Target total models |
|---|---|---:|
| 1.2 | Náströnd | 84 |
| 1.3 | Niflheimr bass amp | 85 |
| 1.3.x | CAB panel + qualified factory cabinet/mic banks | 85 |
| 1.4 / 1.5 | PRE expansion A / B | 91 / 97 |
| 1.6 | Reference AMP expansion | 102 |
| 1.7 / 1.8 | POST dynamics/tone / motion/space | 111 / 120 |
| 1.9 | Creative FX | 129 |
| 1.10 | Synth Layer | 132 |
| 2.0 | Integration / RC | 132 |

The owner authorized continuing through a test installer on October 5 KST.
That instruction permits early production integration for this 1.2 Preview.
Public release, update feed and homepage promotion remain separate. The
five-pedal limit and all released parameter identities remain unchanged.
Preview active count is AMP24 + PRE39 + POST21 = 84, with 25 serialized AMP IDs
including retired Ironball. No original NAM weights are bundled in the product.
