# Auto Set — phase 49 (Auto Set run again and again on the real engine)

Phase 47 noted that a second run can set what the first one kept, and phase
48 that one step of PRESENCE does not reach the target. Phase 49 runs Auto Set
six times in a row on the same session and checks that the runs settle. It is
tests and this document only: no source file changes.

## What is tested

`ChimeraAutoSetTests --rig-pass-engine` (`rigPassEngineRepeatedRuns`), with
played notes going through the real processor and both Dual Blend rigs through
their cabinets. Only the presence step (phase 34) is turned on. Each run starts
on the session the last one left: every parameter is carried over.

Three sessions: the dark rigs of phase 48 in metal and in rock, and phase 47's
rigs with PRESENCE at its top in rock. For each:

- every run finishes and hears both rigs;
- each run starts where the last one ended (PRESENCE and the rig's measured
  share above 3 kHz);
- PRESENCE is never moved back: no run undoes an earlier one;
- a kept rig does not move;
- some run before the sixth keeps both rigs, and every run after it keeps them
  too;
- the two rigs, which have the same head, knobs and cabinet, end on the same
  PRESENCE.

`runRigPassEngine` takes one more optional argument for this, a function
called with the processor after the run.

## Measured (48 kHz, block 256, default cabinets)

PRESENCE of rig 1 / rig 2 after each run:

| Session | Run 1 | Run 2 | Run 3 | Run 4 | Run 5 | Above 3 kHz at the end |
|---|---|---|---|---|---|---|
| dark metal | 0.30 / 0.30 | 0.60 / 0.60 | kept | | | -22.70 dB |
| dark rock | 0.30 / 0.30 | kept | | | | -23.90 dB |
| bright rock | 0.70 / 1.00 | 0.70 / 0.70 | 0.40 / 0.40 | 0.10 / 0.10 | kept | -22.63 dB |

## Found while writing the test

- **The difference between two identical rigs goes away.** In the bright
  session run 1 lowers rig 1's PRESENCE and keeps rig 2's (phase 47's
  finding); run 2 lowers rig 2's to the same value, and from then on the rigs
  move together.
- **It takes up to four runs to settle.** A run moves PRESENCE by at most
  0.30, so a rig that starts far from the target needs several runs. Whether
  one run should go further is a product question, not answered here.
- **"Near the target" leaves a gap.** The bright session settles with the bite
  still about 3 dB over the target (run 4: +4.7 > +3.1 dB) and the row says
  "already near the target". This is phase 12's rule as written: a move must
  gain more than it costs. It was not checked whether a listener would call
  that near.

None is changed here.

## Not yet validated

No real DI or listening test: the signal is synthetic, at one sample rate and
block size. Only the presence step was repeated: GAIN, the channel and the
trim steps were off, and Dual Crossover and Matrix have no presence step. The
same notes are played in every run, so run-to-run differences in the playing
are not covered. Not run on macOS or Windows yet.
