# 1.3 release preparation

Updated 2026-10-10 KST. The owner requested preparation of the current CAB/Niflheimr build as **1.3.0**, using the existing open-beta package convention **1.3.0-beta.1**. Public publication has not occurred.

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
6. Revoice all 48 factory snapshots with explicit modeled CAB recipes while preserving identities and navigation. Use compatible cabinets/speakers/mics, keep six intentional dry LOW paths, and remove machine-dependent private-IR target resolution. Existing saved user projects are not rewritten. See [factory CAB assignments](FACTORY_CAB_VOICING_1_3.md).
7. Exclude unapproved external/private reference rows from the public IR loader and research/reference audio from release packages. Retain two attributed, hash-allowlisted CC BY 4.0 factory IRs and manual user import/recall. Source and staged-package checks enforce the distribution policy.
8. Initialize CAB mic gain/polarity/delay/blend/bypass and the complete native amp selection from the saved state before preparing DSP. This removes cold-start differences on fresh/reused processors while retaining live automation smoothing.
9. Replace the obsolete E1 DSP/UI coverage requirements with the actual larger Niflheimr inventories. Replaying verified checkpoint evidence changes only those two automatic checks to PASS; no missing/manual acceptance is invented. The new final-source run must still produce its own evidence.
10. Record direct head-front knob control for the next update after 1.3.

## CAB checkpoint

[Draft PR #35](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/35) integrates the CAB baseline and main documentation at **6782e4b6ead29bf4a31243b2e05a535c38e16745**. [CAB run 37942149861](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37942149861) passed **17/17 tests on each of Windows, macOS and Linux**. The native run includes camera framing, six-driver geometry, state migration, gestures, driver artwork and 75% room layout. The macOS room/gallery snapshots were inspected for cabinet proportions, restored shell detail, overlaps and clipping.

This checkpoint predates the owner's subsequent factory-preset and IR-distribution requirements. Its installer and evidence are not the final 1.3 candidate. The completed revision must rerun product/candidate/CAB validation with those additions.

## Native regression and targeted CAB correction

At source **e05ca7ee9398a063615e6795f8754caa8406bfd6**, [Product run 37951956620](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37951956620) completed Windows **38/39**, macOS **35/36** and Linux **35/36** CTest checks. Each platform failed the same integrated five-channel comparison: Thall Rhythm's driven K-weighted spread was **1.8139 dB**, above the existing **1.6 dB** limit. The revised factory recipe centres only preset 38's Strike microphone and sets its gain to **−5.0 dB**, retaining the Ruin 12 / closed 4×12 cabinet and 6 cm distance. Other preset recipes and acceptance limits are unchanged.

[Candidate run 37951956736](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37951956736) completed **36/39** checks. In addition to the same channel-spread failure, the Original and Expanded CAB tests exceeded the existing p99 block budget on that Windows runner. Those two tests passed on the same-source Product Windows runner. The logs show serial CTest execution, but lack a process inventory or scheduler trace that would establish the cause of the timing difference. These failures remain recorded and require new-source native verification. Installer/repair/uninstall and Defender checks did not run; assembly was skipped and consolidation remained **0 PASS / 128 BLOCKED**.

## Completed local preset and distribution verification

The complete strict default `ChimeraIntegratedProcessorTests` run finished with **exit 0** in **871.09 seconds** at **2026-10-09 16:41:26 UTC**, without diagnostic flags or a preset subset. The local build uses the production factory loader, real audio/state code and JUCE 8.0.8. All **48** presets passed dirty/clean recall, modeled CAB contracts, OUTPUT = 0 dB, RMS > −30 dBFS and nominal/+6 dB-input peaks < 0.95, including every initial sample. The five Niflheimr binary and A/B audio round trips have maximum sample error **0**. Both-mic prepared-state and runtime gain-ramp checks passed. [The complete level table and measured source hashes](FACTORY_CAB_VOICING_1_3.md#completed-local-calibration) distinguish this headless local audio evidence from the forthcoming native editor/installer build.

The corrected source also passed the strict default five-channel comparison: bare/driven K-weighted spreads are **2.32495 / 1.54410 dB**, within the existing **2.5 / 1.6 dB** limits. This uses `FactoryCabVoicing.h` SHA-256 `4b4fc32933ecd22dbb7168a74da24dfb532b1fc09cc9da1d875afc5c447a23e2`; no amp or test threshold was changed. The completed full-suite log has SHA-256 `9fba2f4f5c4bed35977a3f31cdd65bf9965e3e11a3769eaa9ecfadbbec8945f6`.

The public IR source/byte guard passed with exactly two approved embedded assets and no reference entries; its **10** regressions passed. Publisher **21/21**, release preflight **2/2**, and consolidator **26/26** regressions passed. The consolidator replay of the unchanged verified checkpoint bundles changes exactly the two stale E1 requirements and leaves **21 PASS / 107 BLOCKED**, with `ready=false`. Real-source CI and the remaining external/manual acceptance are still required.

## Validation and remaining acceptance

Local contract results and final-source CI links are recorded in the integration PR after source commit. A version change needs a new build; old package bytes are never relabeled.

The consolidated pipeline is implemented, but existing undefined and unexecuted policy checks remain unresolved. Green platform builds alone are not a complete release verdict. See [pipeline evidence coverage](RELEASE_1_3_PIPELINE.md) for the concrete remaining coverage and [release checklist](RELEASE_CHECKLIST.md) for publication conditions.
