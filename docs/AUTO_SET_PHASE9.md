# Auto Set — phase 9 (cabinet mic)

Phase 9 lets Auto Set place the modeled cabinet mic: how far from the cone's
centre it sits (POSITION, cap to edge) and how far from the grille
(DISTANCE). It runs right after the amp pass, so the mic is placed for the
amp as it is now set, and the Tone EQ pass that follows corrects what is
left. Earlier phases are described in `AUTO_SET_PHASE1.md` …
`AUTO_SET_PHASE8.md`; locks and strength in `AUTO_SET_LOCKS_STRENGTH.md`. The
rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has a **Cabinet mic placement** step,
on by default. It adds one pass of about 6 seconds after the amp pass. Play
across the neck, as for the Tone EQ.

- **The results** get a **Cabinet mic** row: the mic moved, its position,
  distance and level before and after, and how far the balance was from the
  style's target before and after. Its SHOW button opens the cabinet.
- **Strength** scales the position and distance changes: before + strength ×
  change.
- **Lock → Cabinet mic** keeps the mic and skips the pass.
- **UNDO AUTO** restores the position, distance and level.

## Where it applies

- The Classic routing, with its cabinet on and the mic you hear modeled
  (MODELED MIC on). The mic you hear is mic A, or mic B when the A/B blend is
  fully on B.
- Kept, with the reason in the row: Dual and Matrix routing, a switched off
  cabinet, a captured IR or filters-only mic, and two mics blended (a blend
  between A and B).
- If the cabinet or the mic (on, model, speaker, layout, unit, position,
  distance, blend) changes while the pass listens, nothing is changed.

## Measurement

The pass listens to the amp/cab output before the Tone EQ (`Tap::tone`), the
same point as the Tone EQ pass, and measures its third-octave spectrum over
the playing.

The modeled cabinet's continuous response is the curve its finite, tapered IR
is made from (`cabLayout::response`). It is an approximation to that IR,
especially at the lowest frequencies. For every candidate placement, the
prediction is the heard spectrum plus the candidate's response minus the
current one, band by band (the response's power averaged over eight points in
each band). Everything else (the amp, the cuts, the room) stays in the heard
spectrum. The test renders noise through the real IR of two
placements and checks the model's band difference against the rendered one.

## Rules (`analyseCab`, `AutoSetCab.h`)

- **Candidates:** positions in 0.05 steps within 0.35 of the current one,
  and nine distances, log-spaced from half to twice the current one (2 to
  60 cm), on a 0.5 cm grid.
- **Fit:** the distance of the predicted spectrum from the Tone EQ target
  (the same target, alignment and one-octave smoothing as the amp pass,
  80 Hz – 6.3 kHz). The mic moves only when the best candidate improves it by
  more than 0.5 dB.
- **No new notches:** at a distance the cones interfere and cut deep notches.
  A candidate whose bands stray from their neighbours (RMS, 80 Hz – 6.3 kHz)
  by more than 1 dB beyond the current placement is ruled out. A separate
  1/96-octave check from 27.5 Hz to 6.3 kHz rejects an individual local peak
  or notch that grows by more than 3 dB relative to its neighbours at
  +/- 1/6 octave. Strength is checked again at the host-quantized placement
  before any parameter is written; an unsafe intermediate placement is kept.
- **Level:** moving the mic changes the level (farther is quieter). The mic's
  level follows the full captured signal energy, including the 27.5 Hz
  fundamental. Hann-windowed FFT energy is weighted by the response ratio of
  the two actual generated IRs at each bin. This avoids assuming flat energy
  inside a third-octave band near a cancellation notch. The compensation is
  calculated for the placement actually applied, in 0.1 dB steps, within its
  −24 to +12 dB range. This is RMS energy matching, not perceptual LUFS
  matching; a gain-range limit can prevent complete compensation.

## Testing

`ChimeraAutoSetTests`:
- **Rules (`cabRules`), at 44.1, 48 and 96 kHz, on the Guitar 4x12 model:**
  - The model's band difference matches noise rendered through the real IRs
    of two placements within 1.5 dB.
  - Silence reports no playing.
  - On the target, the mic stays.
  - A sound 8 dB too bright above 3 kHz moves the mic to a darker place, and
    one 8 dB too dark moves it toward the cap.
  - Moves stay within the limits, on the grid, and without deeper notches.
  - Moving to 20 cm lowers the level.
- **Controller (`controllerCab`), real processor, a dark fixture fed to the
  capture tap:**
  - The pass runs alone and moves mic A toward the cap as analysed, with the
    level holding the loudness.
  - At 50% strength the mic moves half-way.
  - The lock keeps the mic and skips the pass.
  - Dual routing, a captured IR and blended mics are kept.
  - With the blend on B, mic B is placed.
  - UNDO AUTO restores everything.
- **Full run (`controllerIntegration`):** eight passes and ten result rows on
  processBlock. The default cabinet plays a captured IR, which the pass keeps.
- **Locks (`controllerEveryLock`):** all twelve locks at four strengths.

## Not yet validated

The 0.35 position limit, the halving and doubling of the distance, the 0.5 dB
threshold and the 1 dB notch allowance are starting values. They have been
checked on synthetic signals, including all nine authored cabinet layouts
and Drop A bass tones (see `AUTO_SET_PHASE9_VALIDATION_20261031.md`).
Real guitar/bass DI and listening tests remain unvalidated. The new 3 dB
local-extrema limit is likewise an engineering safety bound, not a
listening-approved threshold.

The pass places one modeled mic. It does not choose the mic, the speaker or
the cabinet, and it does not set a blend between two mics.
