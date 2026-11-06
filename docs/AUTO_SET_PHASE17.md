# Auto Set — phase 17 (Matrix lane levels)

Phase 17 lets Auto Set balance the three lanes of the **Matrix** routing.
Matrix splits the input into LOW, MID and HIGH bands at X1 and X2. Each band
plays through its own amp and cab, and the three lanes are added after their
level and polarity. Until now Auto Set left Matrix alone: the amp and cabinet
steps (phases 8–12, 15) set the Classic routing only, and phase 13 sets Dual.
Earlier phases are described in `AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE16.md`;
locks and strength in `AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as
in the earlier phases.

## Use

The options button next to **AUTO SET** has a **Matrix lane levels** step, on
by default.

- **When it runs:** only when the routing is Matrix as Auto Set starts. It
  adds one pass of about 6 seconds, after the Dual step's place and before the
  Tone EQ pass. With Classic or Dual routing there is no pass, and the row
  says "kept (Classic or Dual routing: no Matrix lanes)".
- **What it sets:** the MID and HIGH lanes' level. On a lane running a native
  head that is the head's **OUTPUT LEVEL** (the engine then ignores the lane
  LEVEL); on a legacy head it is the lane **LEVEL**. The row names which.
- **What it leaves alone:** the LOW lane, X1 and X2, polarity, mute and solo,
  the amps and the cabs.
- **The results:** a **Matrix levels** row shows each moved level, before and
  after, how far the combined sound sits from the Tone EQ target, before and
  after, and the level change. The Final EQ and output pass that follows
  restores the loudness. The row's SHOW button opens the routing split.
- **Strength:** scales each change: before + strength × change.
- **Lock → Matrix levels:** keeps both levels and skips the pass.
- **UNDO AUTO:** restores them.

## Measurement

The pass hears each lane on its own:

- **What the taps carry:** `Tap::lane1` (LOW), `Tap::lane2` (MID) and the new
  `Tap::lane3` (HIGH): each lane's output after its amp, cab, level, polarity
  and mute, as the engine adds them. Phase 13's Dual taps are the same
  `lane1`/`lane2`; `lane3` has a ring of its own.
- **How they are filled:** the engine fills them only while Auto Set listens
  and only for blocks that actually played the routing the pass listens to:
  three lanes for the Matrix pass, two for the Dual pass. A routing switch
  while listening never mixes Dual rigs with Matrix lanes.

If the routing changes, or a MID or HIGH lane switches between a native and a
legacy head while the pass listens, nothing is changed. A level the player
edits while the pass listens is kept.

## Rules (`analyseMatrix`, `AutoSetMatrix.h`)

- **Mix:** low + g(MID) × mid + g(HIGH) × high, computed sample by sample from
  the first 4 seconds of the captures. The prediction is the mix itself, so
  cancellation between the lanes at the crossovers is included exactly. This
  holds because both level controls are linear: the legacy LEVEL scales the
  lane after the cab, and a native head's OUTPUT LEVEL scales its output after
  the head's nonlinearity (checked on the real processor: +6 dB OUTPUT LEVEL
  gives +6.00 dB on the lane).
- **LOW is the reference:** the Tone EQ target is a shape, aligned to the
  level heard, so one lane's level is free. LOW is kept because on a native
  head its OUTPUT LEVEL would also move the LOW lane's DI/amp mix. MID and
  HIGH move relative to it.
- **Fit:** the Tone EQ target fit of phases 8, 9 and 13 (one-octave
  smoothing, 80 Hz – 6.3 kHz).
- **Search:** coordinate descent. Each of MID and HIGH tries 1 dB steps across
  its range, then 0.5 dB steps around the best, up to three rounds each. Each
  moves at most 6 dB either way, within its control's range (LEVEL −24…+12 dB,
  OUTPUT LEVEL ±24 dB). Ties keep the smaller move.
- **Threshold:** the levels change only when the fit improves by more than
  0.5 dB.
- **Cancellation:** in each third-octave band (80 Hz – 6.3 kHz, within 30 dB
  of the strongest), the mix is compared with the lanes' power sum. If it is
  more than 3 dB below in any band, the row adds "the lanes partly cancel at
  the crossovers": check polarity. A level change cannot fix polarity.
- **Silent lane:** a lane that is not heard (muted or soloed away) keeps the
  levels, and the row names the lane. That includes LOW: without the
  reference, the balance would be set for a sound the player does not
  normally hear (MID and HIGH soloed, say).

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --matrix`):

- **Rules (`matrixRules`), 44.1, 48 and 96 kHz.** The fixtures are three
  independent on-target noise sources band-limited at 150 Hz and 1.2 kHz.
  - The predicted error equals the measured error of the mix, before and after
    a move.
  - A MID lane 6 dB low comes up against HIGH (by at least 4 dB, relative),
    and a HIGH lane 6 dB hot comes down against MID; the fit improves by more
    than 0.5 dB. The target is a shape, so the balance between the lanes is
    what is checked.
  - Every move is within 6 dB and on the 0.5 dB grid.
  - A MID level at the top of its range is not raised.
  - Applying the result and running again settles (no move over 1 dB).
  - A silent MID lane, or a silent LOW lane with MID and HIGH out of balance,
    keeps the levels, and silence reports no playing.
  - One source split three ways reports no cancellation in polarity; with MID
    inverted it reports over 3 dB (13.5 dB at 48 kHz).
- **Capture (`matrixLaneCapture`):**
  - The engine's captured lanes, after level and polarity, sum to its Matrix
    output.
  - The processor feeds `lane1`–`lane3` in Matrix routing for the Matrix pass
    only. It feeds no taps for the Dual pass in Matrix, or the Matrix pass in
    Dual, and the Dual pass is unchanged.
  - A native head's OUTPUT LEVEL scales its lane by its dB (+6 dB: 5.998 dB).
- **Controller (`controllerMatrix`), real processor, fixtures fed to the lane
  taps:**
  - The pass runs alone. On native heads (the default) the MID and HIGH OUTPUT
    LEVEL match the analysis; LEVEL and the LOW head stay.
  - On legacy heads the lane LEVEL moves.
  - 50% strength moves the level half-way.
  - The lock keeps the levels and skips the pass.
  - Classic routing has no pass and says why.
  - Switching the routing, or a lane's head engine, while listening voids the
    pass; a level edited while listening is kept.
  - A silent HIGH lane, and a silent LOW lane, keep the levels.
  - UNDO AUTO restores the levels.
- **Full run (`controllerIntegration`, Classic routing):** eighteen result
  rows, with the Matrix row (kept, Classic routing) after the Dual row.
- **Locks (`controllerEveryLock`):** all twenty locks at four strengths.

## Not yet validated

The 0.5 dB threshold and the 6 dB limit are starting values. They have been
checked on synthetic band-limited noise, not on real Matrix rigs, a real DI
or listening tests.

The search moves MID and HIGH only; a sound whose LOW lane is the odd one out
is corrected by moving the other two together, and the Final EQ and output
pass restores the overall level. The level change is reported in the row.
X1 and X2 are not moved: a new split changes what each lane's amp receives,
so the result could not be computed from the captures.
