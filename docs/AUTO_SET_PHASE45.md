# Auto Set — phase 45 ("no playing heard" names the band in the crossover passes)

Phase 44 (`AUTO_SET_PHASE44.md`) found that a pass which hears one band of a
crossover (a Dual Crossover rig, phases 37 to 39; a Matrix MID or HIGH lane,
phases 40 to 42) reports **"no playing heard"** when the playing does not
reach that band, although the guitar is playing. Phase 45 makes that reason
say so. No rule, capture or audio-thread code changes; the head is kept as
before.

## What changes

In those passes only, the channel, GAIN and input trim steps now read:

- "kept (no playing heard in rig 1's band: play notes that reach it, or move
  the crossover)" (Dual Crossover, rig 1 or 2),
- "kept (no playing heard in the MID lane's band: play notes that reach it,
  or move the crossover)" (Matrix, MID or HIGH).

Everywhere else the reason stays "no playing heard": the Classic amp pass and
the Dual Blend rig passes hear the whole input, so there it does mean that
nothing was played.

The step cannot tell an empty band from no playing at all (it hears the band
only), so the reason names the band and both remedies rather than claiming
which it is.

## Testing

`ChimeraAutoSetTests --rig-pass-engine` (the real engine, through
`processBlock`), added to phase 44's test:

- Pure 82 and 98 Hz notes in the Matrix: the MID and HIGH lanes are kept,
  their GAIN is not written, and each row names its band.
- The same notes in a Dual Blend, where each rig hears the whole input: the
  playing is heard and the reason is not used.

## Correction to phase 44's note

Phase 44 says the first test signal (110 to 165 Hz notes with one harmonic)
made the Matrix MID pass report "no playing heard". Run again with a slightly
different timing, the same signal was heard and the lane was set: that signal
sits at the edge of what the MID band passes, so the outcome depends on which
notes fall in the pass. The test here uses notes that are clearly outside the
band instead.

## Not yet validated

No real DI or listening test. How often a real guitar leaves a HIGH lane
below the playing threshold is not measured. A Dual Crossover rig is not run
with an empty band here (the reason comes from the same code path).
