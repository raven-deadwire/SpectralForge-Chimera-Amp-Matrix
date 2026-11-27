# Auto Set — phase 47 (the presence step and the level locks of the rig and lane passes on the real engine)

Phases 44 and 46 ran the GAIN, channel and input trim steps of the rig and
lane passes through `processBlock`. Phase 47 runs what they left: phase 34
(PRESENCE and RESONANCE of both Dual Blend rigs) and the two level locks with
the trim steps. It is tests and this document only: no source file changes.

## What is tested

`ChimeraAutoSetTests --rig-pass-engine` (`rigPassEnginePowerAndLocks`), with
played notes going through the real processor:

- **Phase 34 on the real engine.** Until now its tests fed each rig's head to
  the rig tap without a cabinet. Here the rigs play through their cabinets.
  Two runs, with both rigs' PRESENCE at its maximum and at its minimum. Each
  pass must hear the rig, and each PRESENCE move is checked against the rig
  itself: the share of the rig's energy above 3 kHz, measured on the rig tap
  before and after the run, must fall when PRESENCE was lowered and rise when
  it was raised.
- **Dual rig levels lock with phase 36.** Rig 2's trim and OUTPUT LEVEL are
  kept with the reason and its measured level does not move; rig 1 is trimmed
  with its level held.
- **Matrix levels lock with phase 42.** Both lanes' trim and OUTPUT LEVEL are
  kept with the reason and their measured levels do not move.

## Measured (48 kHz, block 256, default heads and cabinets, rock)

| PRESENCE at | Rig | Result | Above 3 kHz |
|---|---|---|---|
| maximum | 1 | PRESENCE 1.00 > 0.70, RESONANCE 0.50 > 0.20; bite +8.3 > +6.3 dB | -18.65 > -19.62 dB |
| maximum | 2 | RESONANCE 0.50 > 0.20 (PRESENCE kept) | -18.65 > -17.93 dB |
| minimum | 1 | RESONANCE 0.50 > 0.20 (PRESENCE kept) | -24.64 > -23.92 dB |
| minimum | 2 | RESONANCE 0.50 > 0.20 (PRESENCE kept) | -24.64 > -23.92 dB |

Where only RESONANCE was lowered, the share above 3 kHz rises by 0.7 dB: less
low end, the same top.

## Found while writing the test

- **Two identical rigs were set differently.** With PRESENCE at maximum, rig 1
  lowered it and rig 2 kept it, although the rigs have the same head, knobs
  and cabinet. Each rig's pass listens to a different stretch of the playing,
  so the measured balances differ a little (bite +8.3 dB on rig 1, +8.5 dB on
  rig 2). Phase 12's rule moves a knob only when the gain is clear (more than
  1 dB, counting what the move costs the other balance): rig 1's candidate
  cleared that, rig 2's did not. This is the rule working as written, at its
  threshold. It does mean a second run can set what the first one kept.
- **With PRESENCE at minimum, the step does not raise it.** The bite is still
  over the target with the knob at its bottom, so there is nothing for the
  knob to do in that direction.

Neither is changed here.

## Not yet validated

No real DI or listening test: the signal is synthetic, at one sample rate and
block size. The share above 3 kHz is a crude measure (two one-pole high-passes
on the rig tap), used only for the direction of a move. A PRESENCE that is
raised was not produced by these settings, so that direction is not exercised.
