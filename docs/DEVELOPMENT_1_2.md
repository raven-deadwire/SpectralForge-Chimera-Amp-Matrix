# 1.2 development and remaining updates

Status: development checkpoint, 2026-10-05 KST. Public release remains
**1.1.2-beta.1** (source `88b62738fdbd9bcdca34f7b6e1092d6ed411eb94`, merged
main `a9cfb68035688c3f9d356bd4c6ba27587e27da92`). This branch is not a 1.2 release.

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

## Náströnd implemented in the development module

| Area | Implemented | Production integration still required |
|---|---|---|
| Definition | Independent `OriginalAmpDefinition`, stable ID and 13 controls | Append catalog selector/model ID; active total 83 → 84 only at integration |
| DSP | Four serial nonlinear cells, interstage coupling, dynamic tightening, bias/blocking memory, power response, feedback, sag and bloom | Final musical voicing and reference-informed review |
| Controls | GAIN/BASS/MID/TREBLE/MID FREQ/PRESENCE/DEPTH/MASTER; CLANK/CRUSH/IMPACT/ROT/BLOOM | Match final production panel/artwork and contextual visibility |
| Processing | Stereo, smoothing, 1x/2x/4x/8x JUCE wrapper, bounded block chunks, zero allocation after prepare | Production engine selection, latency handoff, real-host/CPU checks |
| State | Six contexts, 84 append-only development parameters, versioned codec | Append after all released parameters; full-rig undo/A-B/preset wiring |
| UI | Real JUCE development panel with attachments and five preset recalls | Place panel in the product's RIG selection and preset navigation |
| Original presets | Fenrir, Surtr, Níðhöggr, Fimbulvetr, Ragnarök | Final level calibration and production recall tests |

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

## Remaining work, reconciled against current code

| Priority | Item | Actual status / next action |
|---|---|---|
| 1.2 | Náströnd integration | Finish catalog, processor routing, production panel, state/undo/A-B and five Original preset wiring; then host and listening acceptance |
| 1.2 | Gain acceptance | 26 NAM / 17 Original states compared; default gain already saturates strongly but reference-character differences remain. Instrument-DI comparisons and listening acceptance are pending |
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

## Version scope

| Version | Scope | Target total models |
|---|---|---:|
| 1.2 | Náströnd | 84 |
| 1.3 | Niflheimr bass amp | 85 |
| 1.4 / 1.5 | PRE expansion A / B | 91 / 97 |
| 1.6 | Reference AMP expansion | 102 |
| 1.7 / 1.8 | POST dynamics/tone / motion/space | 111 / 120 |
| 1.9 | Creative FX | 129 |
| 1.10 | Synth Layer | 132 |
| 2.0 | Integration / RC | 132 |

The owner authorized early 1.2 development on October 5 KST. The saved roadmap's
October 12 production-model boundary remains intact: this checkpoint does not
register Náströnd in the released catalog, change the release feed or publish
new installers. The existing five-pedal limit and released parameter identities
remain unchanged. Current active production count is AMP23 + PRE39 + POST21 = 83,
with 24 serialized AMP IDs including retired Ironball.
