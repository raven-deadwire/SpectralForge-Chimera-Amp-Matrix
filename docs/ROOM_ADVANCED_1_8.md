# Room Advanced — 1.8, first part

Status: **implemented on `claude/room-advanced`**, stacked on
`claude/room-core`. This covers the room-condition half of roadmap 1.8 Room
Advanced (`docs/CABINET_PANEL_ROADMAP.md`).

**Not in this part:** the spatial stage (modelled dimensions, wall materials,
cabinet placement, room mic). Covered by `ChimeraRoomAdvancedTests`, with
`ChimeraRoomCoreTests` and the parameter-inventory tests updated.

Owner's decision (2026-10-28): first part = the five acoustic controls (early/late
balance, decay, diffusion, wet low/high cut, width). The spatial stage follows
separately.

## Parameters (version hint 24, appended after Room Core)

All six are automatable. Every default reproduces the Room Core room exactly.

| ID | Name | Range | Default | What it does |
|---|---|---|---|---|
| `roomEarlyLate` | Early / late balance | −100…+100 % | 0 | −100 % keeps only the early reflections, +100 % only the late tail. In between, the opposite part is scaled linearly; 0 is the calibrated room |
| `roomDecay` | Decay | 25…400 % | 100 % | Scales the late RT60 and the host tail length |
| `roomDiffusion` | Diffusion | 0…100 % | 0 | Blends the late network's excitation towards a copy smeared by four all-passes per side (g 0.7; 3.1 / 4.7 / 7.1 / 10.9 ms) |
| `roomLowCut` | Low cut | 20…1000 Hz | 20 Hz = off | 12 dB/oct Butterworth high-pass on the wet return |
| `roomHighCut` | High cut | 1…20 kHz | 20 kHz = off | 12 dB/oct Butterworth low-pass on the wet return |
| `roomWidth` | Width | 0…200 % | 100 % | Mid/side scaling of the wet return: 0 % mono, 200 % doubles the side |

The controls live in two extra rows of the room callout in the CAB area.

## Exactness at the defaults

- **Bypass path:** with both cuts at their end stops and width at 100 %, the wet
  return takes the released code path. The model's shaping weights are exactly
  1 / 1 / 0.
- **Parity check:** a copy of the released room
  (`Tests/reference/RoomCoreReleased.h`) is compared sample by sample. Three
  sizes × three absorptions, 432,000 samples, match bit for bit.
- **Snapped defaults:**
  - The test found that the decay range, skewed around 100 %, stores its default
    as 99.99999 %. That changed the late gains by one ulp.
  - Decay and width within half a step of 100 % are now treated as exactly 1×.

## Transitions

- **Ramps:**
  - Balance and diffusion ramp across a block.
  - A decay change ramps the line gains like an absorption change.
  - Cuts fade in and out through a 20 ms mix with a smoothstep curve.
- **A cut switched in from off:** it starts at its own frequency on a cleared
  filter, so it does not sweep up from the end stop.
- **The smoothstep fade:** a linear fade left a slope corner at its end, which
  doubled the second difference of a 220 Hz wet signal. With smoothstep, the
  switch is at the settled level.
- **Fresh models:** a freshly reset model, or one switched in by a size change,
  starts at the current shape and decay with no ramp. Otherwise its first
  block would partly bypass the diffusers.

## Measurements (`ChimeraRoomAdvancedTests`, 48 kHz)

- **Balance:**
  - At −100 %, the tail energy after 200 ms is exactly zero.
  - Early-only plus late-only equals the calibrated room within 1.4e-13.
- **Decay (Medium, 50 % absorption, design 0.7 s):**

  | Setting | Measured T60 | Ratio to 100 % |
  |---|---|---|
  | 50 % | 0.377 s | 0.52 |
  | 100 % | 0.724 s | 1.00 |
  | 200 % | 1.342 s | 1.85 |

  The longer setting falls short of 2×: the damping low-pass removes a larger
  share of a longer tail, and the 1.12× compensation is calibrated at 100 %.
  The host tail grows from 1.69 s to 3.23 s.
- **Diffusion 100 % (late only, impulse):**
  - The summed crest factor in 10 ms frames over 30–120 ms falls from 107.4 to
    41.5, so the build-up is smoother.
  - Total wet energy changes by −6.5e-8 dB: the all-passes are lossless.
  - The energy centroid moves 26 ms later, which is heard as a softer onset.
- **Wet cuts:**
  - Low cut 200 Hz: −3.01 dB at 200 Hz and −12.31 dB at 100 Hz.
  - High cut 5 kHz: −3.01 dB at 5 kHz and −14.33 dB at 10 kHz.
  - Both match the analytic Butterworth response.
- **Width:**
  - At 0 %, L−R is exactly zero.
  - At 200 %, side energy is ×4.00 and mid energy ×1.00.
- **Switching the low cut in:** the largest second difference is 3.57e-5
  against 3.48e-5 settled.
- **Real time:** there are no allocations in the callback with every control
  engaged plus a decay change, and the output stays finite.

## Not done

- **The 1.8 spatial stage:** declared dimensions, wall/floor/ceiling materials,
  cabinet-to-boundary placement, and a room mic independent of Mic A/B.
- **A decay law that holds 2× above 100 %:** the 200 % setting is 1.85×.
- **Checks:** listening tests, and CPU measurement in a DAW.
