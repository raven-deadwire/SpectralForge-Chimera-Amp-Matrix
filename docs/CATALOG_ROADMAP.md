# Chimera catalog roadmap

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
| 1.3 | Niflheimr | 85 |
| 1.3.x | CAB panel and qualified factory cabinet/mic banks | 85 |
| 1.4 | PRE Expansion A | 91 |
| 1.5 | PRE Expansion B | 97 |
| 1.6 | Reference AMP Expansion | 102 |
| 1.7 | POST Dynamics & Tone | 111 |
| 1.8 | POST Motion & Space | 120 |
| 1.9 | Creative FX | 129 |
| 1.10 | Synth Layer | 132 |
| 2.0 | Integration / RC | 132 |

## CAB workstream — added 2026-10-08

Replace the primary single-slot IR editing surface with a per-lane CAB panel,
two independent mic/IR slots, measured position/distance controls and preserved
USER IR management. Reuse the convolution backend. Target inventory:
**7 guitar cabinets + 5 bass cabinets; 8 shared microphone types**, subject to
actual measured-bank availability and redistribution evidence.

Specification and asset feasibility: October 8–11; engine/state/panel prototype:
October 12–25; qualified-grid alpha: October 26–November 8; Niflheimr-cycle
integration and candidate validation: November 9–December 4, all KST.
These are internal targets with explicit asset/validation dependencies, not
promised public release dates. Existing Niflheimr work continues now.

The 1.4–1.10 model sequence and 132 target do not change. CAB/microphone/IR
position counts are separate inventory dimensions. Detailed scope, factory
targets, acquisition boundaries and acceptance:
[CAB panel roadmap](CABINET_PANEL_ROADMAP.md).

The obsolete “no 1.2 model before October 12” hold is superseded by the owner's
early integration/publication and the actual October 5 release. Sonic
calibration, listening and real-host acceptance remain evidence-specific and
are not implied by catalog counts or publication history.
