# Room Advanced — 1.8 spatial stage

Status: **implemented on `claude/room-space`**, stacked on
`claude/room-advanced` ([first part](ROOM_ADVANCED_1_8.md)). This completes
the 1.8 Room Advanced items in `docs/CABINET_PANEL_ROADMAP.md`:
- declared modelled dimensions
- wall, floor and ceiling materials
- cabinet-to-boundary placement
- a room mic independent of close Mic A/B

Covered by `ChimeraRoomSpaceTests`.

## What the model is, and what it is not

- **The room:**
  - The declared room is a rectangular box.
  - Early reflections come from first- and second-order image sources between
    the cabinet and a spaced pair of room mics. This is the same image-source
    code as Room Core's preset rooms, now fed with declared positions.
  - The late tail is the Room Core feedback delay network:
    - its line lengths are set from the declared room's mean free path (4V/S);
    - its decay is set from the Eyring reverberation time of the declared
      surfaces.
- **Simplifications:**
  - It is not a wave or ray-tracing simulation. There are no room modes,
    scattering, cabinet directivity or mic polar patterns.
  - The early reflections use each material's 500 Hz–1 kHz absorption,
    broadband.
  - The room mic's direct sound is not part of the return: the rigs' close
    mics carry the direct sound.
- **Naming:** these are the "declared space" controls. They are not called
  physical re-miking.

## Parameters (version hint 25, appended after Room Advanced)

All fourteen are automatable. **Space off (the default) is the Size /
Absorption room, bit for bit**, whatever the other thirteen say.

| ID | Range | Default | |
|---|---|---|---|
| `roomSpace` | off / on | off | Use the declared room. Size and Absorption are then inactive (and greyed out) |
| `roomSpaceLength`, `roomSpaceWidth`, `roomSpaceHeight` | 2–30, 2–20, 2–10 m | 7, 5, 3 | Dimensions (the Medium room) |
| `roomFloor`, `roomWalls`, `roomCeiling` | 10 materials | Carpet, Wood panel, Gypsum board | Surfaces |
| `roomCabDistance` | 0.1–20 m | 2.1 | Cabinet from the back wall |
| `roomCabAcross` | 5–95 % | 45 % | Cabinet across the room |
| `roomCabHeight` | 0.1–3 m | 1.2 | Speaker centre above the floor |
| `roomMicDistance` | 0.3–20 m | 2.45 | Room mic from the cabinet, towards the front wall |
| `roomMicAcross` | 5–95 % | 55 % | Room mic across the room |
| `roomMicHeight` | 0.3–4 m | 1.2 | Room mic height |
| `roomMicSpacing` | 0–2 m | 0.2 | Spacing of the pair (left/right ears of the return) |

Positions are kept 0.1 m inside the walls. The default positions are the
Medium room's, so the default declared room has the Medium room's reflection
times. Room Advanced balance, decay, diffusion, cuts and width apply on top.

### Materials

Sabine absorption coefficients as commonly tabulated in room-acoustics
handbooks. These are approximate published values, not measurements of
particular surfaces.

| Material | 125 | 250 | 500 | 1k | 2k | 4k Hz |
|---|---|---|---|---|---|---|
| Concrete | .01 | .01 | .02 | .02 | .02 | .02 |
| Brick | .03 | .03 | .03 | .04 | .05 | .07 |
| Plaster | .01 | .02 | .02 | .03 | .04 | .05 |
| Gypsum board | .29 | .10 | .05 | .04 | .07 | .09 |
| Wood floor | .15 | .11 | .10 | .07 | .06 | .07 |
| Wood panel | .28 | .22 | .17 | .09 | .10 | .11 |
| Carpet | .02 | .06 | .14 | .37 | .60 | .65 |
| Heavy curtain | .14 | .35 | .55 | .72 | .70 | .65 |
| Acoustic tile | .70 | .66 | .72 | .92 | .88 | .75 |
| Glass | .18 | .06 | .04 | .03 | .02 | .02 |

## Laws

- **Reflections:**
  - Gain = 2.5 m / path × √(1 − α) per bounce on each surface, with α the
    surface's 500 Hz–1 kHz mean.
  - Delay = (path − direct path) / 343 m/s.
  - The bounces are counted per image: floor, ceiling and walls.
- **Decay:** Eyring, T60 = 0.161 V / (−S ln(1 − ᾱ) + 4mV).
  - Air absorption m is included at 4 kHz only: about 30 dB/km, at 20 °C and
    50 % relative humidity.
  - The 500 Hz T60 sets the line gains.
  - The 4 kHz T60 sets the loop low-pass, solved for the mean line length. The
    low-pass's own 500 Hz loss is divided out of the line gains.
  - When a room's 4 kHz T60 is longer than its 500 Hz T60, the low-pass is open
    (no high-frequency boost).
- **Levels:**
  - Levels are referenced to the cabinet heard at 2.5 m, the Medium room's
    mic distance, with the Medium room's calibration trims.
  - The diffuse tail follows T60 / V. The network's energy already grows as
    T60 / (mean line length), so the late trim is the Medium trim ×
    √(S_Medium / S).
  - Moving the mic therefore changes the early/late balance, as in a room, not
    the tail level.
- **Changes:**
  - A change of dimensions, positions, surfaces or Space on/off crossfades to
    a second model: 50 ms input handover and 250 ms output fade, as for
    Room Core's size changes. Materials are steps, so they crossfade too
    rather than ramp.
  - A change made during a fade starts the next one.
- **Host tail:** 2.2 × the longer of the two Eyring times × Decay, + 0.25 s.

## Measurements (`ChimeraRoomSpaceTests`, 48 kHz)

- **Space off:** bit-identical to the released Room Core across 3 sizes × 3
  absorptions (432,000 samples), with every space control moved.
- **Default declared room:** the same 45 early-reflection sample positions as
  Medium.
- **Probe room** (10 × 8 × 4 m, cabinet 0.5 m high):
  - All six first-order reflections arrive at their analytic image delays.
  - The floor bounce comes first, at sample 60 (analytic 60).
  - The floor-bounce level matches the law exactly for Concrete (0.1427),
    Carpet (0.1244) and Heavy curtain (0.0871).
- **Mic spacing:** at 0 m, left and right early reflections are identical. At
  1 m, they differ by up to 0.146.
- **T60** (late only; three cascaded octave band-passes; Schroeder T20):

  | Room | 500 Hz measured / Eyring | 4 kHz measured / Eyring + air |
  |---|---|---|
  | Live: concrete / brick / plaster, 12 × 9 × 5 m | 8.01 / 8.08 s | 2.70 / 2.31 s (+17 %) |
  | Default: carpet / wood panel / gypsum, 7 × 5 × 3 m | 0.823 / 0.834 s | 0.409 / 0.407 s |
  | Treated: wood floor / gypsum / acoustic tile, 8 × 6 × 3.5 m | 0.550 / 0.538 s | 0.432 / 0.451 s |

  A one-pole loop filter cannot follow a 3.5:1 low/high ratio exactly, so the
  live room's 4 kHz decay runs 17 % long. The limits are 15 % at 500 Hz and
  20 % at 4 kHz.
- **Level law:** 500 Hz late energy relative to the default room is:
  - live: +4.1 dB, where T60 / V predicts +2.8 dB;
  - treated: −5.0 dB, where it predicts −3.9 dB.

  Deviations are 1.1–1.4 dB, within the 2 dB limit, and in the same direction:
  the network's energy grows a little faster than T60 / V.
- **Host tail:** 18.0 s for the live room (2 × Eyring = 16.2 s).
- **Transitions:** mic 2.45 → 5 m, walls → Heavy curtain, then Space off, on a
  220 Hz tone:
  - Largest second difference: 4.98e-5 at Space off, against 3.44e-5 settled.
  - No allocation in the callback.
  - Before materials crossfaded, the walls change stepped the return by 10×
    the settled level, ramping within one block. That is why they crossfade.
- **UI:**
  - SPACE switches the room.
  - Size is disabled while the space is on, and the space controls are
    disabled while it is off.
  - Dimension, surface and mic controls are attached to their parameters.
  - Every control lies inside the 680 × 214 callout.

## Not done

- **Model refinements:** frequency-dependent early reflections, room modes,
  cabinet directivity and mic polar patterns.
- **Panel:** a top-down room drawing showing the cabinet and mic positions.
- **Checks:** listening tests, and CPU measurement in a DAW.
