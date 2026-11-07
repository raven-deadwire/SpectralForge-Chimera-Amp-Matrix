# Auto Set — phase 24 (Amp input trim)

Phase 24 lets Auto Set set the native head's **INPUT TRIM**. Phase 8
(`AUTO_SET_PHASE8.md`) sets GAIN toward the style's drive target and says
"out of this knob's reach" when GAIN at its end cannot get there; phase 20
(`AUTO_SET_PHASE20.md`) first looks for a channel that can. INPUT TRIM moves
the level into the head, so it reaches past GAIN's ends; OUTPUT LEVEL then
holds the head's loudness. Earlier phases are described in
`AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE23.md`; locks and strength in
`AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has an **Amp input trim past the GAIN
range** step, on by default, listed after **Amp gain and tone**.

- **When it runs:** in the amp pass, after the amp step and before the
  presence + resonance step. It adds no listening time; it alone also starts
  the pass. Where it applies is as for phase 8: the Classic routing, the amp
  switched on, the head panel (native engine) on.
- **What it sets:** the Classic head's INPUT TRIM, and its OUTPUT LEVEL to
  hold the loudness.
- **What it leaves alone:** the knobs, the channel, the head, its input jack.
- **The results:** an **Amp input trim** row, between the **Amp gain + tone**
  and **Presence + resonance** rows: "INPUT TRIM +0.0 dB > +12.0 dB, OUTPUT
  LEVEL +0.0 dB > -10.0 dB; drive 2% > 21% (target 45%, still short at 12
  dB), loudness held", or why it kept the trim. Its SHOW button opens the
  amp head.
- **Strength:** scales the INPUT TRIM change in dB, as for Amp; OUTPUT LEVEL
  holds the loudness for the trim applied.
- **Lock → Amp input trim:** keeps INPUT TRIM and OUTPUT LEVEL. Locking Amp,
  Presence + resonance, Amp channel and Amp input trim skips the amp pass.
- **UNDO AUTO:** restores both.

## Rules (`analyseTrim`, `AutoSetTrim.h`)

- **Rendered:** the amp pass's capture entering the amp (its first 3 s, as
  phase 8), through a fresh copy of the native head at the knobs the amp step
  proposed at full strength (so a lower Strength is not made up by the trim)
  and at a trim.
- **Drive:** phase 8's measure (how much of the playing's level spread the
  head removes) against phase 8's target for the instrument and style
  (guitar 15% clean, 45% rock, 65% metal; bass 10%, 30%, 50%).
- **Kept** when the drive at the current trim is within phase 8's ±8%.
- Otherwise the trim is bisected toward the target, within 12 dB of where it
  is and within the control's ±24 dB, on a 0.5 dB grid (a render too quiet
  to be heard as playing counts as too little drive). It moves only when the
  drive comes closer to the target by more than 4% (half the tolerance).
- **Loudness:** the mean power of the playing blocks (within 30 dB of the
  loudest 20 ms block, after the warm-up) of the two renders; OUTPUT LEVEL
  moves by minus the difference, on a 0.1 dB grid. OUTPUT LEVEL is after the
  head's nonlinearity, so it does not change the drive.
- The presence + resonance step then renders the head at the trim and level
  now set.
- **Kept, with the reason:** locked; the amp unavailable (as phase 8); INPUT
  TRIM or OUTPUT LEVEL edited while listening; no playing heard; the head
  renders no playing; within the target; no trim within 12 dB comes closer;
  the trim edited while applying.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --trim`):

- **Rules (`trimRules`, 44.1, 48 and 96 kHz, the synthetic amp):**
  - GAIN low for rock: the trim rises (48 kHz: 0 > +12 dB, drive 1% > 14%,
    loudness +11.1 dB), on the 0.5 dB grid, within 12 dB; the reported drive
    and loudness are the rendered ones.
  - GAIN high for clean: the trim falls (0 > -7.5 dB, drive 46% > 15%).
  - At the reached trim: kept. From +20 dB the trim stays within +24 dB.
    GAIN at zero for metal is reported short of the target.
  - A silent head and silence are kept.
- **Controller (`controllerTrim`, real processor, model 2 on RHYTHM with GAIN
  at its bottom, rock):**
  - The step runs alone in the amp pass: INPUT TRIM +0.0 > +12.0 dB, OUTPUT
    LEVEL +0.0 > -10.0 dB, drive 2% > 21%. Rendered on the head as set, the
    drive is the analysed one and the loudness is within 0.002 dB of before.
  - 50% strength applies half the trim, with the level held for it.
  - With the amp step: GAIN reaches the rock drive on RHYTHM, so the trim
    stays; the amp, trim and presence rows come in that order. With Amp
    locked, the trim moves.
  - The lock keeps both; the trim edited while listening and Dual routing are
    kept. UNDO AUTO restores both.
- **Full run (`controllerIntegration`):** twenty-five result rows, with the
  Amp input trim row after the Amp gain + tone row.
- **Locks (`controllerEveryLock`):** all twenty-seven locks at four strengths.

## Not yet validated

Checked on renders of synthetic playing through the native heads, not with a
real DI or a listening test. A large trim changes how the head responds to
picking beyond the drive measure; the 12 dB limit per run keeps the move
bounded. The 4% minimum gain is a starting value.
