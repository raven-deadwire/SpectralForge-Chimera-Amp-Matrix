# PR #219 Phase 6 verification — 2026-10-30 KST

Base: `0b5ed0bcd5867d3a980716b5fab4056ee5b81c83` (open PR #219).
The local qualification-only build includes the working-tree files hashed in
`provenance.json`. This is Linux/GCC 13/JUCE 8.0.8, native headless execution;
it is not Windows/macOS CI, editor UI, DAW, real-DI or listening acceptance.

## Reproduced defects and changes

1. A normal decaying/distorted E2 synthetic sustain received an 80 Hz node with
   a 9 dB **range limit** at 44.1/48/96 kHz. This is an unwanted proposed node,
   not a claim that 9 dB was actually applied to the note. Absolute band decay
   was mistaken for band-specific motion. The fixed detector also requires
   P95-minus-median ≥6 dB of time-paired `band RMS - full RMS` contrast. Existing
   excess/jump thresholds, ratios, Q, attack/release and range limits remain.
2. Selecting the Dynamic EQ lock after applying and then using UNDO left slot
   target 1 assigned while band 1 became disabled and Tone EQ bypassed. New
   Tone/Dynamic locks and newer edits now preserve each complete automatic node
   (slot + band + shared bypass). During-listening A-B-A edits and synchronous
   host callback edits also cannot leave a partial automatic node active.

`baseline-reproduction.txt` comes from a local native probe with the original
`analyseDynamicEQ` and Controller bodies extracted from the base commit (only
function/class names changed to coexist with the candidate). To reproduce a
full before/after regression, check out that base in a separate worktree, copy
only the three new/current `Tests/AutoSet*.{cpp,h}` test files into it, build
`ChimeraAutoSetTests`, and run `--dynamic-signals` / `--dynamic-ownership`.
The baseline should fail those assertions; the candidate passes.

## Results

| Check | Result / evidence |
| --- | --- |
| Entire native Auto Set test target, existing Phases 1–6 plus new regressions | PASS, 60.74 s; `ctest.xml`, `ctest.txt`, `native-autoset.txt` |
| Normal sustain/common-envelope controls | 60 cases PASS across 3 rates; A0/B0/fifth-fret bass and A1/E2 guitar, clean/distorted |
| Even extended-range scales | 12 cases PASS; no nodes |
| Existing +12 dB/Q8/250 Hz positive control | Detected at all 3 rates; preserved after the fix |
| Actual GraphicalEQ application at 25/50/75/100% | 12 cases PASS; positive reduction, correct range cap and release; A0 extra loss 0 dB in the independent low-note replay |
| Existing static Tone EQ, Tone/Final dynamic slots, bypassed curve, full slots | PASS; free-slot allocation and full Undo preserved |
| New locks, newer edits, A-B-A and reentrant host edits | PASS; complete nodes preserved or placement safely abandoned |
| Native synthetic amp/cab captures | 9/9 PASS; Tight 515 sustain CAB off/on (bundled V30) and Metro Clean A0 dynamics CAB off, all 3 rates; no nodes, 0 dB dynamic reduction; `synthetic-captures.json` |
| Python input contracts | 4 unittest methods PASS; `input-tests.txt` |
| Nine planned real DI takes | MISSING_INPUT, all nine WAVs absent; `di-plan.json` |
| Real bass cabinet IR / listening / actual DAW | NOT RUN |

The first capture preparation rejected over-full-scale PCM output. The final
frozen profiles use `nativeAmp_c0_outputLevel = -18 dB` on **both** members of
all pairs, preserving drive and avoiding export clipping. The legacy lane
`level1` is ignored when native amps are enabled and is not used as the trim.
No source recording, analysis threshold or test expectation was normalized or
relaxed to obtain the result. These are synthetic preparation controls; no
recorded-DI result was selected or omitted.

## Limits / next evidence

- Effective search starts at **80 Hz**. The configured bass lower bound is
  50 Hz, but 50/63 Hz centres have no required lower neighbours and are skipped.
  The documentation now states this; adding sub-80 Hz detection needs separate
  reference/support-band design and low-bass validation.
- A post-rig spectrum cannot prove whether a resonance originated in an
  instrument, nonlinear amp or cabinet. Use the paired captures and additional
  varied-pitch notes in `docs/AUTO_SET_PHASE6_DI_REGRESSION.md`.
- Existing nine takes can be reused for low-note, dynamics, attack and sustain
  guards. They do not replace diverse-pitch cabinet identification or listening.
- The thresholds remain initial values. Real DI, real bass IR, stereo/phase,
  Dual/Matrix and host/editor acceptance have not been completed.
- No merge, public release or deployment was performed by this verification.
