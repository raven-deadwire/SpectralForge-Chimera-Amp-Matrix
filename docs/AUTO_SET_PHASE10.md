# Auto Set — phase 10 (boost pedal)

Phase 10 lets Auto Set put a boost pedal in front of a driven amp. With rock
or metal gain, low notes going into the amp's distortion smear its low end. A
mid boost with a bass cut in front of the amp tightens it. Auto Set uses the
board's **Green Drive** (OVER DRIVE, TONE, LEVEL), which cuts below 180 Hz
before it drives. The pass runs before the amp pass, so the amp pass that
follows sets GAIN and the tone knobs with the boost in place. Earlier phases
are described in `AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE9.md`; locks and
strength in `AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in the
earlier phases.

## Use

The options button next to **AUTO SET** has a **Boost pedal before the amp**
step, on by default. It adds one pass of about 6 seconds after the input
pass. Play as you normally would, with low notes and chords.

- **The results** get a **Boost pedal** row: the slot and the pedal's
  settings, and how far the amp's low end sits over the style's target before
  and after. Its SHOW button opens the pedal.
- **Strength** scales OVER DRIVE and the LEVEL change in dB (LEVEL 0.5 is
  unity).
- **Lock → Boost pedal** keeps the board and skips the pass.
- **UNDO AUTO** takes the pedal off again.

## Where it applies

- The Classic routing, with the amp on and its head panel (native engine)
  on, the PRE pedal board on, and the board's last position (right before the
  amp) empty.
- Guitar, rock or metal. Bass and the clean style get no boost.
- Kept, with the reason in the row: Dual and Matrix routing, a switched off
  amp, the classic amp engine, the board off (the legacy PRE chain), a
  drive, fuzz or boost already on the board, and a last position in use.
- If the board or the amp changes while the pass listens, nothing is changed.

## Measurement

The pass captures the signal entering the amp (`Tap::amp`, after the pedal
board). The first 3 seconds are rendered offline through a fresh Green Drive
and a fresh copy of the native amp at its current settings, once without the
pedal and once for each candidate setting.

- **Loose low end:** the third-octave spectrum of the amp's output, its
  80–200 Hz level relative to 400 Hz – 2.5 kHz, minus the same difference of
  the style's guitar target.
- **Drive:** the amp pass's measure (`ampCompression`), how much the amp
  flattens the playing's dynamics.

## Rules (`analyseBoost`, `AutoSetBoost.h`)

- A boost is considered only when the low end is more than 3 dB over the
  target.
- **Candidates:** OVER DRIVE 0 or 0.2, TONE 0.5, LEVEL 0.6, 0.8 or 1.0 (+3.2,
  +8.2 and +12 dB).
- A candidate that drives the amp more than 15 points past the larger of the
  current drive and the style's target is ruled out. The amp pass pulls GAIN
  back toward the target afterwards.
- The candidate with the tightest low end wins, if it tightens it by more
  than 2 dB. Otherwise nothing changes.

## Testing

`ChimeraAutoSetTests`:
- **Rules (`boostRules`), at 48 kHz, on the Tight 515 head with GAIN at 80%
  and plucked notes from the low E:**
  - Silence reports no playing.
  - The Green Drive at unity lowers the low-end excess.
  - Metal engages a boost that tightens the low end by more than 2 dB within
    the drive limit, on the candidate grid.
  - Clean style and bass get no boost.
  - Notes whose lows are already thin get none.
- **Controller (`controllerBoost`), real processor, fixtures fed to the
  capture taps:**
  - The pass runs alone and puts the Green Drive in slot 5 (last in the
    default order) as analysed. Other slots are untouched.
  - At 50% strength, OVER DRIVE and the LEVEL change in dB are halved.
  - The lock keeps the board and skips the pass.
  - Clean style, a drive already on the board, a chorus in the last position,
    and Dual routing are kept.
  - UNDO AUTO removes the pedal.
- **Full run (`controllerIntegration`):** nine passes and eleven result rows
  on processBlock. The boost row matches the analysis.
- **Locks (`controllerEveryLock`):** all thirteen locks at four strengths.

## Not yet validated

The 3 dB threshold, the 2 dB improvement, the 15-point drive allowance and
the candidate grid are starting values. They have been checked on synthetic
plucked notes and one head, not on real guitars, other heads or listening
tests.

The measure looks at the low end's balance only; it does not tell a tight
attack from a thin one. The offline render runs at the session rate without
the amp's oversampling. The analysis runs on the message thread at the end
of the pass.
