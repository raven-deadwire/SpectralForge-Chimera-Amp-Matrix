# Auto Set — phase 44 (the rig and lane passes on the real engine)

Phases 33 to 43 were tested with the controller's taps fed directly: the DI on
the amp tap and each rig's rendered head on the rig tap. Each of their
documents lists "not a full run on the real engine" as not validated. Phase 44
adds that run. It is tests and this document only: no source file changes.

## What is tested

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --rig-pass-engine`):

- **Whole runs through `processBlock` (`rigPassEngine`).** Played notes go
  into the real processor; the engine feeds the taps itself (the rig tap one
  rig or lane after its cabinet, the amp tap the whole input or that lane's
  band). For each of:
  - Dual Blend, phase 33's step (rig 1, rig 2),
  - Dual Crossover, phase 37's step (rig 1, rig 2),
  - Matrix, phase 40's step (MID, HIGH),

  with both heads' GAIN far below the rock drive: both passes run, the playing
  is heard, each GAIN is raised, and the drive the analysis measures comes
  closer to the target than it was.
- **A muted rig on the real engine** (phase 43): no pass for it, the reason on
  its row, the other rig set.
- **The engine's rig tap follows MUTE and SOLO (`rigPassEngineMute`).** For a
  Dual rig and a Matrix lane: silent when muted, silent when another lane is
  soloed (in the Matrix, the LOW lane's SOLO counts), heard when it is the one
  soloed. Phase 43 reads the parameters and assumed this.

## Measured (48 kHz, block 256, default cabinets)

| Routing | First | Second |
|---|---|---|
| Dual Blend | drive 4% > 50% | drive 0% > 52% |
| Dual Crossover | drive 2% > 39% | drive 0% > 44% |
| Matrix (MID, HIGH) | drive 2% > 42% | drive 0% > 43% |

The rock target is 45%.

## Found while writing the test

With the first test signal (decaying 110 to 165 Hz notes with one harmonic),
the Matrix MID pass reported **"no playing heard"**: almost none of that
signal reaches the MID band. With notes that have eight harmonics the passes
hear the playing. The step keeps the head and says so, which is safe, but the
message does not say that the lane's band is nearly empty. A real DI with
little content in a lane's band (a HIGH lane above a high crossover, soft
low-register playing) will be kept the same way. Not changed in this phase.

## Not yet validated

No real DI or listening test: the signal is synthetic. One sample rate and
block size. The channel and trim steps (phases 35, 36, 38, 39, 41, 42) and
phase 34 are not run on the real engine here; they use the same captures as
the GAIN steps. The drive is the analysis's own measure, not re-measured from
a second run.
