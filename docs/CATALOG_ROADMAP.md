# Chimera catalog roadmap

## 1.3 preparation — 2026-10-09

The current integrated candidate targets **1.3.0-beta.1**: Niflheimr plus the original CAB engine/panel, 14 speakers, 20 microphones and nine explicit layouts. Active AMP/PRE/POST counts are **25 + 39 + 21 = 85**; 48 presets are selectable. CAB inventory remains separate from the 132-model target. [Preparation status](RELEASE_1_3_PREPARATION.md) records final-source verification. Direct controls inside the amp-head artwork are planned for the next update after 1.3; its number is not assigned.

## Published baseline and retained sequence

Current published baseline is Open Beta 1.2: AMP24 + PRE39 + POST21 = 84 active
models. [v1.2.0-beta.1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.2.0-beta.1)
was published on 2026-10-05. Versions 1.1.1 and 1.1.2 are released history.
The E670FE transition retires Ironball from new selection while preserving its
DSP/state; 1.2 has 25 serialized AMP IDs. The machine-readable count contract is
Validation/catalog-count-contract.json. See
[development checkpoints and remaining updates](DEVELOPMENT_1_2.md).

Existing 31 Factory preset ordinals remain fixed. Crom Cruach, Wild Hunt and
Azhi Dahaka remain the three appended bass signatures; the four guitar
signatures retain IDs34–37. The five Original Náströnd rigs occupy indices38–42.
New model IDs, parameter IDs and presets are append-only.

| Version | Scope | Total active models |
|---|---|---:|
| 1.1.1 / 1.1.2 | Released baseline | 83 |
| 1.2 | Released Náströnd | 84 |
| 1.3 | Niflheimr + original CAB engine/panel | 85 |
| After 1.3 | CAB refinements / qualified optional captures; embedded head controls planned | 85 |
| 1.4 | PRE Expansion A | 91 |
| 1.5 | PRE Expansion B | 97 |
| 1.6 | Reference AMP Expansion | 102 |
| 1.7 | POST Dynamics & Tone | 111 |
| 1.8 | POST Motion & Space | 120 |
| 1.9 | Creative FX | 129 |
| 1.10 | Synth Layer | 132 |
| 2.0 | Integration / RC | 132 |

## CAB workstream — Original-first revision, 2026-10-08

The owner clarified that existing products are to be studied as references,
then Chimera's cabinet and microphone system is to be independently designed.
Replace the primary single-slot IR editing surface with a per-lane CAB panel
and two independent mic/IR paths. **Original / Modeled** is the default factory
engine; **Captured** and **User IR** remain distinct compatible sources.

Model-based Position/Distance must work without a measured commercial-cabinet
grid. Captured movement needs qualified measurements; fixed User IR preserves
its baked-in response. Missing capture data blocks only that bank, not original
sound-engine implementation or the playable CAB prototype.

First deliver one original guitar cabinet and one original bass cabinet with
working mic motion, blend and production state/routing. The earlier
**7 guitar / 5 bass / 8 mic** list remains research and long-term coverage
inventory, not a first-release count requirement or a one-to-one cloning list.
Final Original roles and counts follow verified musical differentiation.

Original architecture and reference-use specification: October 8–11;
playable engine/state/panel prototype: October 12–25; modeled-response refinement,
role expansion and original starting presets: October 26–November 8;
Niflheimr-cycle integration and candidate validation: November 9–December 4,
all KST. These remain internal targets, not promised public release dates.
Existing Niflheimr work continues now; qualified Captured additions run in parallel.

The 1.4–1.10 model sequence and 132 target do not change. CAB/microphone/IR
position counts are separate inventory dimensions. Detailed scope, reference
and training boundaries, task ownership and acceptance:
[CAB panel roadmap](CABINET_PANEL_ROADMAP.md).

The obsolete “no 1.2 model before October 12” hold is superseded by the owner's
early integration/publication and the actual October 5 release. Sonic
calibration, listening and real-host acceptance remain evidence-specific and
are not implied by catalog counts or publication history. This documentation
revision does not implement DSP, execute training or authorize release.
