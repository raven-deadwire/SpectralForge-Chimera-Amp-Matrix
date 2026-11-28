# Auto Set — phase 50 (the rig and lane passes on the real engine at other sample rates and block sizes)

Phases 44 to 49 ran the rig and lane passes through `processBlock` at 48 kHz
with blocks of 256 only; each listed that as not validated. Phase 50 runs them
at 44.1 and 96 kHz and with blocks of 64 and 1024. It is tests, the test's
registration and this document: no source file changes.

## What is tested

`ChimeraAutoSetTests --rig-pass-rates` (`rigPassEngineRates`), registered as
its own test, `ChimeraAutoSetRigPassRatesTests`. It is not part of the whole
binary's run, which would grow by about four minutes.

Four sessions: 44.1 kHz / 256, 96 kHz / 256, 48 kHz / 64, 48 kHz / 1024. In
each, with played notes going through the real processor:

- **GAIN** of Dual Blend, Dual Crossover and Matrix, as phase 44: the playing
  is heard, a low GAIN is raised and the drive comes closer to the target.
- **Input trim** of the same three, as phase 46: the trim rises, OUTPUT LEVEL
  falls and each lane's measured level stays within 2 dB.
- **Presence** of both Dual Blend rigs with PRESENCE at its top, as phase 47:
  the rig is heard and set, PRESENCE is not raised, and where it is lowered
  the rig's share above 3 kHz falls.

`runRigPassEngine` takes the sample rate and block size as two more optional
arguments. Its block counts were written for 48 kHz and blocks of 256; it now
keeps their length in seconds, and the controller's tick about every 21 ms, at
any rate and block size. At the defaults the counts are unchanged.

## Measured (rock, default heads and cabinets)

Dual Blend, rig 1 / rig 2:

| Session | GAIN 0.05 > | Drive after (target 45%) | Lane level, trim run | PRESENCE 1.00 > |
|---|---|---|---|---|
| 48 kHz / 256 (phases 44 to 47) | 0.10 / 0.10 | 47% / 44% | -37.33 > -37.28 dB | 0.70 / 1.00 |
| 44.1 kHz / 256 | 0.11 / 0.11 | 50% / 51% | -37.93 > -37.89 dB | 0.70 / 0.70 |
| 96 kHz / 256 | 0.10 / 0.10 | 50% / 53% | -34.10 > -34.14 dB | 1.00 / 1.00 |
| 48 kHz / 64 | 0.10 / 0.10 | 47% / 49% | -37.33 > -37.28 dB | 0.70 / 1.00 |
| 48 kHz / 1024 | 0.10 / 0.10 | 47% / 46% | -37.34 > -37.29 dB | 0.70 / 1.00 |

Dual Crossover and Matrix: GAIN ends within 0.01 of the 48 kHz / 256 values
in every session, except Matrix's HIGH lane (0.25 at 48 kHz / 256, 0.27 or
0.28 elsewhere); every trim is +12 dB with the same OUTPUT LEVEL as at
48 kHz / 256, and no lane's level moves by more than 0.3 dB.

## Found while writing the test

- **The block size does not matter.** At 48 kHz the three block sizes give the
  same levels to 0.01 dB and the same knob moves.
- **The same rig is about 3.2 dB louder at 96 kHz and 0.6 dB quieter at
  44.1 kHz than at 48 kHz.** The played notes have the same level at all three
  rates (-24.73 dB RMS), so the difference comes from the engine, not from the
  test signal. Which stage it comes from was not looked for. Every lane shows
  it (Matrix MID: -31.85, -32.43 and -28.57 dB).
- **The presence step's decision changes with the rate.** With PRESENCE at its
  top, both rigs are lowered at 44.1 kHz, one of the two at 48 kHz and neither
  at 96 kHz (only RESONANCE is lowered there). The rigs are a little brighter
  at 96 kHz (above 3 kHz: -16.88 dB, against -18.65 dB), and phase 12's rule
  sits at its threshold here, as phase 47 noted.

None is changed here. The level difference between rates is the one to look
at first: a session moved from 48 to 96 kHz would play louder.

## Not yet validated

No real DI or listening test: the signal is synthetic. 88.2 and 192 kHz and
block sizes that are not a power of two were not run. The channel step, the
level locks and the repeated runs of phase 49 were run at 48 kHz / 256 only.
Not run on macOS or Windows yet.
