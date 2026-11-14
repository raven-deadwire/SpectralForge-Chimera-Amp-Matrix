# Auto Set — phase 32 (boost pedal in a Dual Blend)

Phase 32 extends phase 10 (`AUTO_SET_PHASE10.md`, a Green Drive in front of a
driven amp whose low end is loose) to the **Dual Blend** routing. Earlier
phases are described in `AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE31.md`; locks
and strength in `AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in the
earlier phases.

## Use

The options button next to **AUTO SET** has **Boost pedal also in a Dual Blend
(both amps)**, under **Boost pedal before the amp**. It is **off by default**,
like the other steps added since phase 24, until it has had a DI and listening
check. It is an option of the boost step: it does nothing while that step is
off, and the menu greys it then.

- **When it runs:** in the boost pass, as phase 10. No new pass and no extra
  listening time.
- **What it judges:** the PRE pedal board sits before both rigs, so one pedal
  boosts both amps. The step renders the captured input through each rig's
  native head, with and without the pedal, and judges the **mix of the two
  amp outputs**, weighted as the Dual blend mixes them (rig 2's polarity
  included; a rig whose amp is off counts as silent). Phase 10's rule then
  applies unchanged to that mix: loose low end, a clear tightening, and no
  drive far past the style's target.
- **What it sets:** the Green Drive in the last free board slot, as phase 10.
- **Kept, with the reason:**
  - the option off in a Dual routing ("Dual and Matrix routing: add a boost in
    the full editor, or switch on the boost's Dual Blend option");
  - Dual **Crossover** and the **Matrix** routing: the rigs or lanes play
    separate bands, so a boost before the split changes each band's drive
    differently;
  - a rig that is on but uses the classic amp engine;
  - both amps off;
  - the Dual type, blend, rig 2's polarity or rig 2's amp changed while
    listening;
  - and phase 10's own reasons (bass, clean style, the board off, a drive
    already on the board, no free slot, a tight low end, no playing heard).
- **The results:** the **Boost pedal** row, with "the rigs' mix:" before the
  low-end figures.
- **Strength, Lock → Boost pedal, UNDO AUTO:** as phase 10.

The Classic routing is unchanged, with the option on or off.

## Rules

Phase 10's (`analyseBoost`, `AutoSetBoost.h`), unchanged. The only new piece is
`blendedBoostRenderer`: each rig is rendered as `nativeBoostRenderer` does and
the two are added with the blend's weights.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --dual-boost`):

- **Rules (`dualBoostRules`):** the blended render is the weighted sum of the
  rigs (one rig alone, two equal rigs, two opposed rigs cancelling); two equal
  driven rigs are judged as one rig.
- **Controller (`controllerDualBoost`, real processor, a DI fed to the taps):**
  off by default, and a Dual routing is then kept with the option named; with
  the option, a Dual Blend of two driven heads gets the pedal and the row says
  "the rigs' mix" (low end 11.6 dB over the target > 4.3 dB); Dual Crossover,
  a rig on the classic engine and the Matrix routing are kept; the Classic
  boost is unchanged by the option; UNDO AUTO.

## Not yet validated

No real DI or listening test. The step renders each amp offline from the
signal entering the amps and ignores what follows them (cabinets, lane cuts),
as phase 10 does for the Classic rig. That both Dual Blend rigs receive that
signal unchanged is read from the engine's code, not measured against the
engine's lane outputs.
