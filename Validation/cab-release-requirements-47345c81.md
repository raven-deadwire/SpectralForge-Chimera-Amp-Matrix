# 47345c81 release gate: definition and producer inventory

This is a **read-only static audit of two exact-source files**, not a new result for the still separately audited aggregate gate. Source `47345c81db8aefbf9ae51a2684d546119bb9ae32`, tree `80525e0dda220cbc87c229b3e5234d2ddc38fe78`, policy version 4.

## What the policy actually requires

The `beta_1_3` profile contains **128 required IDs**. The current CI map owns **21 IDs**: A12.01–07 (seven CTest checks), A13.01–07 (three full native platform matrices, Windows installer/MSIX/Defender, and assembly), E1.DSP/STATE_UI/LEGACY (three implementation contracts), and I1.PITCH/GATE/STATE_GAIN_GR/LIVE_REMOVE (four synthetic integration checks).

The remaining **107 IDs have no entry in this producer map**. This count alone is not a runtime verdict: a mapped producer still needs successful current-source execution; an unmapped check must not be marked PASS merely because a related test exists. Of the 63 required hard gates, 42 lack a mapped producer (27 legacy + 15 other). Required checks that are not hard gates still remain required.

## The 86 legacy numbered IDs

| Stage | Policy title | Number of required IDs |
| --- | --- | ---: |
| A1 | Installer consistency | 6 |
| A2 | Signature definition | 8 |
| A3 | Signature APVTS recall | 9 |
| A4 | Factory compatibility | 5 |
| A5 | Hartke IR import | 8 |
| A6 | Marshall IR import | 8 |
| A7 | IR chain of trust | 7 |
| A8 | IR fallback policy | 4 |
| A9 | Signature benchmark calculator | 9 |
| A10 | Signature hard and secondary gates | 6 |
| A11 | Benchmark JSON | 9 |
| A14 | Documentation sync | 7 |

These are exactly A1–A11 plus A14. The two reviewed files provide their stage titles and numbered IDs, but **no individual assertion, fixture, metric, threshold or producer** for these 86 IDs. Their detailed semantics cannot be recovered by assigning a stage title to every numbered check.

This is principally a definition and producer-ownership gap. Installer/document/JSON/recall/fallback/chain-of-trust checks can often be automated once their precise assertions and input fixtures are defined. The Hartke/Marshall import stages need an identified appropriate IR fixture; the policy does not name those files or their expected results. Benchmark/calculator stages need their defined inputs and scoring assertions. Existing A13 installer or A12 IR/state PASS records cannot silently be relabelled as these separate legacy IDs. None of this establishes that all 86 checks require physical hardware.

The next concrete step for these IDs is to define each retained check's assertion, fixture and evidence producer, then bind its actual execution to the current source. This audit makes no such definition or policy change and creates no missing PASS record.

## The remaining 21 IDs

| IDs | Count | Defined scope and next evidence needed |
| --- | ---: | --- |
| B1, B2, B3, B4.H1–H4, B5, B6 | 9 | Same-DI fixture → renders → measurements → hard/secondary gates → score. Supply identified DI/reference files and candidate renders, then explicit metrics/limits and a producer. These files do not state those limits. Once inputs and assertions exist, much of this chain can be automated. |
| B8.RESULT | 1 | Listening A/B. Needs a recorded listening comparison and acceptance result tied to the relevant source/audio/reference. Numeric equivalence alone does not assert a listening result. |
| B10.RESULT | 1 | Audio save/restore roundtrip. Needs a defined scenario and before/after audio/state evidence. Potentially automatable; existing synthetic state IDs do not automatically satisfy it. |
| B11.RESULT / B12.RESULT | 2 | Windows host / Studio One 8 validation. Needs the applicable actual host/version and source-bound execution/assertions. Only B12 explicitly names Studio One 8. The files do not prescribe manual-only execution for every host check. |
| B13.RESULT | 1 | A defined continuous **30-minute soak**, complete input/environment/run records and assertions. Potentially automatable; a CI job taking over 30 minutes is not itself this soak. |
| B14.RESULT | 1 | Release candidate gate. Detailed decision contract and producer are absent here. This is not inherently a hardware-only requirement. |
| E2.TD_EQ / CALIBRATION / SAME_DI | 3 | E670FE reference and sound acceptance. Needs defined reference/equivalence and calibration targets plus same-DI evidence. These files do not expand TD_EQ or specify its metric/limits, so the audit does not invent them. |
| I2.GATE_DI / UI_AUDIO_TIMING / LIVE_REMOVE_DAW | 3 | Real-input and host acceptance. Needs identified input-gate behavior, UI/audio timing and live-removal observations in the applicable DAW, with explicit workloads/assertions. Mapped I1 synthetic checks are separate. |

The “next evidence” descriptions above are inferences from the policy titles and IDs. They do not introduce thresholds or assert that an unexecuted scenario passed. Real-DI can be supplied as a recorded fixture; appropriate host checks may be scripted. Only the explicitly named listening stage necessarily calls for a listening judgment in this static inventory.

## Deferred transpose checks are separate

`I2.PITCH_LIVE` and `I2.PITCH_DI` are listed under the separate `T1` / `transpose_followup` profile and are explicitly deferred from `beta_1_3`. They are **not part of the 128 / 107 / 21 counts above**. The policy says deferred does not mean PASS or NOT_APPLICABLE; the follow-up profile still requires evidence.

## Sources and limits

- [release-policy.json at source 47345c81](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/47345c81db8aefbf9ae51a2684d546119bb9ae32/Validation/release-policy.json) — SHA256 `d69a54063661caa136c6bef5c5e960e0215f55cb1aed2c9658e345d7c28c6812`.
- [ci-check-map.json at source 47345c81](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/47345c81db8aefbf9ae51a2684d546119bb9ae32/Validation/ci-check-map.json) — SHA256 `4cbbd8fdcc720ae6d7b8a4fbe81d4e13e6a2e01b9f9d73570d2a9039d7754249`.
- `release-gate-definition-audit.json` in this folder contains all exact required/unmapped IDs and the count assertions.

No policy/code changes, producer executions, native tests, artifact downloads or release actions were made for this audit. The final native aggregate verdict must be taken from its actual source-bound gate artifact, not inferred from this static map.
