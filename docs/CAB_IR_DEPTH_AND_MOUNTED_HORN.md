# 1.3 IR enclosure depth and mounted bass 4x10 horn

The owner requested two final visual corrections on 2026-10-11 KST and
authorized publication after the corrections, followed by a full 1.3 product
description on the RavenForge homepage (not merely a changelog).

## Corrections

- Captured-IR cards, their library preview and the room use the modeled CAB
  shell material bank: a projected roof, rolled front edge, original tolex,
  metal corners, recessed grille, feet and contact shadow. Both capture cards
  retain a shared physical scale, now accounting for illustrative roof depth.
- IR artwork is still illustrative. Unknown driver configurations remain
  undisclosed, `EXAMPLE` remains explicit, and real 8x10 capture metadata is
  not renamed to the separate modeled 6x10. No capture or audio parameter is
  changed by painting or navigation.
- A bass 4x10's mounted centre HF hardware remains visible when its sound
  level is zero. Previously `CabLayoutView::tweeter` returned immediately at
  zero level, which made a correctly rendered horn disappear in actual
  presets. No HF gain, acoustic geometry or preset value is changed.
- Two review catalogue images finish their native graphics context before
  PNG encoding. Their earlier all-black captures are evidence failures,
  not evidence that the entire product screen was black.

## Verification

The existing 120-case native/software horn/grille regression matrix also
compares zero level with active level and a separate grille-only reference.
The room checks cover both legacy and selectable 4x10 cabinets. IR tests
retain unknown/mixed capture identity, 1x15/8x10 proportions, constant mic
scale, unclipped compact layouts and unchanged parameter state, and inspect
the projected textured roof. The actual output PNGs remain review evidence.

Run the source-specific CAB Panel and Product workflows. Success from parent
47345c81 cannot certify this change. Release preparation PR #35, performance
PR #42 and homepage PR #19 are the related work. Publication authority does
not fabricate unexecuted acceptance receipts or remove the existing release
gate. This change does not alter that policy.
