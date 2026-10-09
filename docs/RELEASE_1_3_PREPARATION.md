# 1.3 release preparation

Updated 2026-10-09 KST. The owner requested preparation of the current CAB/Niflheimr build as **1.3.0**, using the existing open-beta package convention **1.3.0-beta.1**. Public publication has not occurred.

## Source integration

Starting source: PR #34 at **40d5e8a5ea466eaccc1044ccefce1bd33e6ac222**. This includes the accumulated CAB/Niflheimr work, 14 speakers, 20 microphones, nine explicit layouts, diameter compatibility, Crimson 12 / Nocturne 100 and the realtime convolution correction.

Main at **aa9e7b9a9f3889f1e42d83de264491e673c38018** has two documentation commits missing from that branch. Preserve their Original-first direction in CABINET_PANEL_ROADMAP, CATALOG_ROADMAP, DEVELOPMENT_1_2 and FX_AND_IR_DESIGN.

Separate NAM-training and live-signing drafts are not represented as included or accepted. Selected release evidence producer/consumer changes from PR #23 are reviewed within this preparation. Their presence does not mark unexecuted checks complete.

## Previous-source evidence

These results belong to the starting 1.2 preview, **not the changed 1.3 source**:

| Run | Observed result |
|---|---|
| [Product 37903125012](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37903125012) | Windows 38/38, Linux 35/35, macOS 35/35 CTest; packages and candidate assembly succeeded. |
| [CAB 37903124961](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37903124961) | 16/16 CAB contracts on all three platforms. |
| [Candidate 37903124963](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37903124963) | Windows candidate and preparation contracts succeeded. |
| [Setup 11605490358](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37903125012/artifacts/11605490358) | 1.2.0-preview.40d5e8a5ea, 118,728,193 bytes. This is not a 1.3 installer. |

The old Windows log also contains two publisher UnicodeDecodeErrors masked by later command success. Its complete release-policy verdict was BLOCKED. Recorded macOS deadline misses and actual DI/host acceptance limits remain visible in PR #34.

## Preparation changes

1. Use 1.3.0 / beta.1 consistently and archive the historical 1.2 notes.
2. Pin publisher text I/O to UTF-8 and stop preflight blocks on native command failures.
3. Replace the unshipped layout-7 8×10 prototype with a 2×3 Bass 6×10 (0.63 × 0.94 × 0.40 m; 178.5 L), inheriting existing driver/mic models and per-driver sealed loading. Recalculate the six-source field and shorter enclosure modes; raw units 6/7 resolve to 4/5 in the same column. Existing 8×10 captured IRs remain intact.
4. Frame only displayed rigs, reclaim the unused microphone margin in room view, restore cabinet exterior detail and remove duplicate visible configurations. Retain one common world scale and all existing stored layout ordinals. The deliberately replaced preview 8×10 sound is not preserved.
5. Produce and consume source/run/attempt-bound validation. The pinned 1.3 publisher recomputes the actual verdict before any public write; preparation scope cannot publish.
6. Record direct head-front knob control for the next update after 1.3.

## Validation and remaining acceptance

Local contract results and final-source CI links are recorded in the integration PR after source commit. A version change needs a new build; old package bytes are never relabeled.

The consolidated pipeline is implemented, but existing undefined and unexecuted policy checks remain unresolved. Green platform builds alone are not a complete release verdict. See [pipeline evidence coverage](RELEASE_1_3_PIPELINE.md) for the concrete remaining coverage and [release checklist](RELEASE_CHECKLIST.md) for publication conditions.
