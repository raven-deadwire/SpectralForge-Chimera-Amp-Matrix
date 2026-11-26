# Auto Set — phase 48 (a raised PRESENCE on the real engine)

Phase 47 ran phase 34 (PRESENCE and RESONANCE of both Dual Blend rigs)
through `processBlock`, but its settings only produced a PRESENCE that was
lowered or kept: the raised direction was listed as not exercised. Phase 48
exercises it. It is tests and this document only: no source file changes.

## What is tested

`ChimeraAutoSetTests --rig-pass-engine` (`rigPassEnginePresenceRaised`), with
played notes going through the real processor and both rigs through their
cabinets.

Both rigs are made dark: GAIN at 10% of its range, TREBLE at its bottom, BASS
and MIDDLE at their top, PRESENCE at its bottom. Two runs, metal and rock. For
each rig:

- the pass hears the rig, and the rig plays under the bite target;
- the presence row shows a PRESENCE move and the knob is higher after the run;
- the share of the rig's energy above 3 kHz, measured on the rig tap before
  and after the run, rises by more than 1 dB. RESONANCE lowered alone raises
  that share by about 0.7 dB, so the limit is set above it.

## Measured (48 kHz, block 256, default cabinets)

| Style | Rig | Result | Bite from the target | Above 3 kHz |
|---|---|---|---|---|
| metal | 1 | PRESENCE 0.00 > 0.30, RESONANCE 0.50 > 0.25 | -4.0 > -2.8 dB | -26.14 > -24.07 dB |
| metal | 2 | PRESENCE 0.00 > 0.30, RESONANCE 0.50 > 0.25 | -3.9 > -2.7 dB | -26.14 > -24.07 dB |
| rock | 1 | PRESENCE 0.00 > 0.30, RESONANCE 0.50 > 0.20 | -1.7 > -0.4 dB | -26.14 > -23.90 dB |
| rock | 2 | PRESENCE 0.00 > 0.30, RESONANCE 0.50 > 0.20 | -1.5 > -0.3 dB | -26.14 > -23.90 dB |

The same rigs with PRESENCE started at its top keep it and lower only
RESONANCE; the share above 3 kHz then rises by 0.69 dB (-21.43 > -20.74 dB).
That run is not part of the test: it was used to check that the test fails
when PRESENCE is not raised, and to place the 1 dB limit.

## Found while writing the test

- **The raised direction needs an unusually dark rig.** With the head's
  default tone the rig plays over the bite target in all three styles, even
  with TREBLE and PRESENCE at their bottom. PRESENCE was raised only with BASS
  and MIDDLE at their top and GAIN low.
- **One step of 0.30 does not reach the target.** In metal the bite is still
  2.7 dB under the target after the run. A second run was not tried here.

Neither is changed here.

## Not yet validated

No real DI or listening test: the signal is synthetic, at one sample rate and
block size. The share above 3 kHz is a crude measure (two one-pole high-passes
on the rig tap). Only Dual Blend has a presence step, so there is no Dual
Crossover or Matrix counterpart. Not run on macOS or Windows yet.
