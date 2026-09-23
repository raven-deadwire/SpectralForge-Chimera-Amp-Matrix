# Chimera Amp Matrix - MVP Architecture

## Modes
- Classic: Pre -> one amp lane -> cab -> Post.
- Dual: Pre -> two full-range lanes -> aligned blend -> Post.
- Matrix: Pre -> LR4 3-band split -> independent lanes -> aligned sum -> Post.

Sprint 1 proves routing before production amp, cab, and FX modelling.

## Core
PluginProcessor -> ChimeraEngine -> LaneProcessor.
Matrix adds ThreeBandCrossover. Production Pre/Post racks, convolution and latency alignment follow.

## MVP decisions
- 4th-order Linkwitz-Riley crossover.
- Defaults: 150 Hz / 1.2 kHz.
- Three placeholder behaviours: Clean, TightDrive, BassSaturator.
- No topology rebuilding in processBlock.
- Classic, Dual and Matrix are first-class states.

## Acceptance checks
1. All three modes pass audio.
2. Matrix lanes support solo, mute and level.
3. Crossovers are automatable.
4. Split/sum response is measured before nonlinear model work.
5. State restores mode, crossover and lane settings.
6. Test 44.1/48/96 kHz and variable block sizes.
