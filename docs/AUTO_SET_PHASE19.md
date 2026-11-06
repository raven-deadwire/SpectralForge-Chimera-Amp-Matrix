# Auto Set — phase 19 (Dual rig levels)

Phase 19 lets Auto Set match the loudness of the two rigs in the **Dual**
routing before it sets their blend. In Dual Blend the BLEND is the balance
between rig 1 and rig 2. If rig 2 plays 6 dB quieter, a 50% blend is mostly
rig 1, and phase 13 can only make up for it within its 30% limit. This step
first brings rig 2 to rig 1's level, so the blend step starts from two rigs
heard alike. Earlier phases are described in `AUTO_SET_PHASE1.md` …
`AUTO_SET_PHASE18.md`; locks and strength in `AUTO_SET_LOCKS_STRENGTH.md`.
The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has a **Dual rig level matching**
step, on by default, listed before **Dual blend and crossover**.

- **When it runs:** in the Dual pass (phase 13's), when the routing is Dual
  and the Dual type is Blend as Auto Set starts. It adds no pass: it uses the
  same capture, before the blend step. With Classic or Matrix routing the row
  says "kept (Classic or Matrix routing: no Dual rigs)". With Dual Crossover
  it says "kept (Dual crossover: each rig plays its own band)", since there
  the rigs play different bands and their levels are part of the tone, which
  the crossover step sets.
- **What it sets:** rig 2's level. On a native head that is the Dual B
  head's **OUTPUT LEVEL** (the engine then ignores LEVEL 2); on a legacy head
  it is **LEVEL 2**. The row names which.
- **What it leaves alone:** rig 1 (the reference), the amps and cabs,
  polarity, mute and solo. The BLEND is the blend step's.
- **The results:** a **Dual rig levels** row, before the **Dual blend** row,
  shows the level before and after, how far apart the rigs were, and the
  mix's level change at the current blend. The Final EQ and output pass that
  follows restores the loudness. The row's SHOW button opens the routing
  split.
- **The blend step after it** hears rig 2 at the level this step applied.
- **Strength:** scales the change: before + strength × change. The blend
  step hears the level actually applied.
- **Lock → Dual rig levels:** keeps rig 2's level. LEVEL 2 is also the
  Matrix MID lane's level, so the Matrix levels lock keeps it too. Locking
  both Dual rig levels and Dual blend skips the Dual pass.
- **UNDO AUTO:** restores it.

## Measurement

The step uses the Dual pass's taps (`Tap::lane1`/`lane2`: each rig after its
amp, cab and level, before the blend; see `AUTO_SET_PHASE13.md`). Rig 2's head
switching between native and legacy while the pass listens voids both steps
(it moves which control is rig 2's level); so does the routing or the Dual
type changing. A level the player edits while the pass listens is kept.

## Rules (`analyseRigLevels`, `AutoSetDual.h`)

The first 4 seconds of the captures are used.

- **Loudness:** each rig's power in 80 Hz – 6.3 kHz, the range of the Tone EQ
  fit, from the same third-octave bands as the earlier phases. The rigs
  together decide whether there is playing and whether it is bass or guitar.
- **Reference:** rig 1. Rig 2's level moves by the difference.
- **Threshold:** only when the rigs differ by more than 1 dB.
- **Grid and limits:** 0.5 dB steps, at most 12 dB either way, within the
  control's range (LEVEL −24…+12 dB, OUTPUT LEVEL ±24 dB). When the limit or
  the range cuts the change short, the row says so.
- **Exact:** both level controls are linear (checked in phase 17: the legacy
  LEVEL scales the lane after the cab, a native OUTPUT LEVEL after the head's
  nonlinearity), so rig 2 at its new level is the capture scaled. The blend
  step and the reported mix level use that.
- **Silent rig:** a rig that is not heard keeps the level, and the row names
  the rig.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --rigs`):

- **Rules (`rigsRules`), 44.1, 48 and 96 kHz.** The fixtures are two
  independent on-target noise sources, rig 2 set a given number of dB from
  rig 1.
  - Rig 2 6 dB down comes up by 6 dB (48 kHz: rig 1 47.4 dB, rig 2 41.4 dB,
    +6.0 dB); 6 dB over comes down by 6 dB. Changes are on the 0.5 dB grid.
  - The rig levels are the fit-range power, and the mix level change equals
    the mix's measured change (+2.0 dB at 50% blend).
  - Applied, a second run moves nothing. Rigs 0.8 dB apart are kept.
  - Rig 2 stops at its control's range, and at 12 dB; at the top of its
    range it is not raised. Both are reported as limited.
  - A silent rig 2 keeps the level, and silence reports no playing.
- **Controller (`controllerRigs`), real processor, fixtures fed to the lane
  taps:**
  - Alone (blend step off), the step runs in the Dual pass. On native heads
    (the default) rig 2's OUTPUT LEVEL matches the analysis (0 → +6.0 dB);
    LEVEL 2, rig 1 and the BLEND stay. On a legacy head LEVEL 2 moves.
  - 50% strength moves the level half-way.
  - The lock, Dual Crossover and Classic routing have no pass and say why.
  - With the blend step: the rig row comes first, and the blend step's
    analysis equals that of a run fed rig 2 already scaled by the applied
    level (the blend moved 20 → 50%).
  - The routing, the Dual type or rig 2's head engine changing while
    listening voids the step; a level edited while listening is kept.
  - A silent rig 2 keeps the level. UNDO AUTO restores it.
- **Full run (`controllerIntegration`, Classic):** twenty result rows, with
  the rig levels row (kept, Classic routing) before the Dual row.
- **Locks (`controllerEveryLock`):** all twenty-two locks at four strengths.

## Not yet validated

The 1 dB threshold and the 12 dB limit are starting values. They have been
checked on synthetic noise, not on real Dual rigs, a real DI or listening
tests. The loudness is fit-range power, not a perceptual loudness model; two
rigs with very different tone can match in power and still sound unequal,
which the blend step that follows then weighs against the Tone EQ target.
Rig 1 is always the reference, so a rig 1 that is the odd one out is matched
by moving rig 2 to it.
