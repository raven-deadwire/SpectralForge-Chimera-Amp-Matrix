# Auto Set — phase 21 (Dual and Matrix lane polarity)

Phase 21 lets Auto Set fix polarity between the rigs of the **Dual** routing
and between the lanes of the **Matrix** routing. Phases 13 and 17 measure how
far the mix sits below the rigs' or lanes' power sum and only report it ("the
rigs partly cancel: check polarity or delay", "the lanes partly cancel at the
crossovers: check polarity"). This step flips the polarity that removes it,
before the level and blend steps run. Earlier phases are described in
`AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE20.md`; locks and strength in
`AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has a **Dual and Matrix lane
polarity** step, on by default, listed before **Dual rig level matching**.

- **When it runs:** in the Dual pass (Dual routing, Blend or Crossover) or
  the Matrix pass (Matrix routing), first in the pass. It adds no listening
  time. With Classic routing the row says "kept (Classic routing: one rig)".
- **What it sets:** **POLARITY** of Dual rig 2, or of the Matrix MID and HIGH
  lanes.
- **What it leaves alone:** rig 1 and the Matrix LOW lane (the reference:
  flipping every lane at once changes nothing heard), levels, the blend, the
  crossover, X1 and X2, the amps and cabs.
- **The results:** a **Lane polarity** row, before the Dual or Matrix rows,
  shows each lane flipped and the worst-band cancellation before and after.
  When nothing flips it says why: the lanes add up, or no flip reduces the
  cancellation (then delay or the crossover is the cause). SHOW opens the
  routing split.
- **The steps after it** (Dual rig levels, Dual blend or crossover, Matrix
  levels) hear the lanes at the polarity it set.
- **Strength:** not applied. A lane is flipped or kept.
- **Lock → Lane polarity:** keeps POLARITY 2 and 3. Locking it with Dual blend
  and Dual rig levels skips the Dual pass; with Matrix levels, the Matrix pass.
- **UNDO AUTO:** restores it.

## Measurement and rules (`analysePolarity`, `AutoSetPolarity.h`)

The pass's lane captures (`Tap::lane1`–`lane3`: each lane after its amp, cab,
level and polarity, before the blend) are used, the first 4 seconds.

- **Mix:** w0 × lane 0 + Σ sₖ × wₖ × lane k, with sₖ = ±1. The weights are
  the Dual blend (1 − blend, blend), or 1 for Dual Crossover and Matrix. A flip
  is the capture negated, so the prediction is the mix itself.
- **Cancellation:** in each third-octave band (80 Hz – 6.3 kHz, within 30 dB
  of the strongest), the weighted lanes' power sum against the mix; the worst
  band counts, as in phase 17.
- **Decision:** every sign combination of the lanes after the reference is
  tried. A flip is made only when the mix now cancels by more than 3 dB and
  the best combination cancels at least 1 dB less. Ties keep the current
  polarity.
- **Kept, with the reason:** a silent lane; the routing, the Dual type or a
  lane's head changing while listening; a lane polarity edited while
  listening (or while applying).

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --polarity`):

- **Rules (`polarityRules`), 44.1, 48 and 96 kHz:**
  - Dual rigs from one source, rig 2 opposed (−0.8): rig 2 flips, 16.1 → 0 dB
    at 48 kHz. The predicted cancellation equals the flipped mix's measured
    one.
  - In-phase rigs and independent rigs (1.6 dB) are kept.
  - Dual Crossover (weights 1, 1) with the high rig opposed: it flips
    (14.4 → 0 dB).
  - Matrix from one source split at 150 Hz and 1.2 kHz: MID inverted flips MID
    only (13.4 → 0 dB), HIGH inverted flips HIGH only, in polarity nothing
    flips.
  - A silent lane keeps it; silence reports no playing.
- **Controller (`controllerPolarity`), real processor:**
  - Dual, rig 2 opposed: the step runs alone in the Dual pass and sets
    POLARITY 2; POLARITY 1 and 3 stay. 50% strength still flips.
  - With the blend step: the polarity row comes first, and the blend step
    reports no cancellation left.
  - In-phase rigs are kept ("add up"). The lock and Classic routing have no
    pass and say why. POLARITY 2 or the Dual type edited while listening voids
    the step; a silent rig 2 keeps it.
  - Matrix, MID inverted: MID flips alone in the Matrix pass, and with the
    level step on, it reports no cancellation left.
  - UNDO AUTO restores the polarity.
- **Full run (`controllerIntegration`, Classic):** twenty-two result rows,
  with the polarity row (kept, Classic routing) after the mic blend row.
- **Locks (`controllerEveryLock`):** all twenty-four locks at four strengths.

## Not yet validated

The 3 dB and 1 dB thresholds are starting values, checked on synthetic noise
split by the same filters, not on real Dual rigs or Matrix lanes, a real DI or
listening tests. Real rigs differ in delay and phase as well as polarity: a
flip removes an opposed polarity, not a time offset, and the row says when no
flip helps.
