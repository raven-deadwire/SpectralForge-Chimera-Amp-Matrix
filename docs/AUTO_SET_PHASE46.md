# Auto Set — phase 46 (the trim and channel steps of the rig and lane passes on the real engine)

Phase 44 (`AUTO_SET_PHASE44.md`) ran the GAIN steps of the rig and lane
passes through `processBlock` and left the input trim and channel steps "not
run on the real engine". Phase 46 runs them. It is tests and this document
only: no source file changes.

## What is tested

`ChimeraAutoSetTests --rig-pass-engine` (`rigPassEngineTrimAndChannel`), for
each of Dual Blend (phases 35, 36), Dual Crossover (phases 38, 39) and the
Matrix MID and HIGH lanes (phases 41, 42), with played notes going through the
real processor:

- **Input trim, and "loudness held" measured.** Rock, both heads on RHYTHM
  with GAIN low. The trim step raises INPUT TRIM and lowers OUTPUT LEVEL, and
  its row says "loudness held". Until now only the step's own calculation was
  checked. Here each rig's or lane's level is measured on the engine's rig tap
  (after the cabinet), on the same two seconds of notes, before and after the
  run, and must stay within 2 dB. Without the level held, a 12 dB trim into
  this head raises the lane by about 10 dB.
- **Channel.** Clean, both heads on LEAD: each moves to RHYTHM.

## Measured (48 kHz, block 256, default cabinets)

| Routing | Lane | INPUT TRIM | OUTPUT LEVEL | Level on the rig tap |
|---|---|---|---|---|
| Dual Blend | rig 1 | +12.0 dB | -10.0 dB | -37.3 > -37.3 dB |
| Dual Blend | rig 2 | +12.0 dB | -10.0 dB | -37.3 > -37.3 dB |
| Dual Crossover | rig 1 | +12.0 dB | -11.1 dB | -39.9 > -39.9 dB |
| Dual Crossover | rig 2 | +12.0 dB | -11.2 dB | -44.7 > -44.7 dB |
| Matrix | MID | +12.0 dB | -8.9 dB | -31.9 > -31.7 dB |
| Matrix | HIGH | +12.0 dB | -10.7 dB | -47.6 > -47.3 dB |

The level moved by at most 0.3 dB.

## Found while writing the test

In the Matrix with GAIN at the very bottom of RHYTHM (the setting the other
two routings use), the trim step **kept** the lane: "drive 0%, target 45%;
INPUT TRIM within 12 dB does not come closer". The band a Matrix lane receives
is quiet enough that 12 dB of trim does not move the drive measure at that
GAIN, so the step, by its rule, does nothing. The Matrix case here therefore
starts with GAIN at 20% of its range, where the trim does help. Not changed:
the step is doing what phase 24's rule says, and the GAIN step (phase 40) is
the one that would raise GAIN first.

## Not yet validated

No real DI or listening test: the signal is synthetic, at one sample rate and
block size. The level is a mean-square over two seconds of notes on the rig
tap, not a loudness measure. Phase 34 (PRESENCE and RESONANCE of the Dual
Blend rigs) is still not run on the real engine. The Dual rig levels and
Matrix levels locks are not exercised here.
