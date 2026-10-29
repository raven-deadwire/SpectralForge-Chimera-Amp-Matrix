# POST motion and space — 1.8

Status: **implemented on `claude/post-motion-space`**, stacked on
`claude/post-expansion`. Roadmap 1.8: nine POST models, three each for
MODULATION, DELAY and REVERB. Room Advanced, the other 1.8 item, is not part
of this branch. Covered by `ChimeraPostMotionSpaceTests` and the native UI
tests.

Owner's decision (2026-10-28): the working names below, as proposed.

## Models

These families keep their three shared controls (MODULATION: Rate, Depth,
Mix; DELAY: Time, Feedback, Mix; REVERB: Size, Damping, Mix). Each new model
gives those controls its own meaning.

| Family | Index | Chimera name | Reference | What the shared controls do |
|---|---|---|---|---|
| MODULATION | 6 | Spin Cabinet | Leslie rotary speaker | Rate = rotor speed: the horn follows over about 0.6 s, the drum (×0.85) over 2.5 s. Depth = Doppler and rotation amount |
| MODULATION | 7 | Throb Vibe | Uni-Vibe | Four staggered all-passes swept by a skewed lamp curve. Rate = sweep, Depth = range. A Mix around half gives the chorus-mode blend |
| MODULATION | 8 | Ring Bell | Ring modulator | Depth = carrier, 40 Hz–2.56 kHz. Rate = ±10 % carrier wobble |
| DELAY | 3 | Tap Cloud | Eventide UltraTap | Time = span of six evenly spaced taps. Feedback = Taper: minimum fades the taps up, centre is equal, maximum fades them down |
| DELAY | 4 | Bloom Delay | Eventide Rose | Time drifts by 1.2 ms × (sin 0.6 Hz + ½ sin 0.13 Hz). The feedback loop is 120 Hz–3.5 kHz with soft saturation |
| DELAY | 5 | Dual Digital | Strymon DIG | Two recirculating lines, at Time and ¾ Time, with a 12 kHz top |
| REVERB | 3 | Hall Prime | Bricasti M7 (reference) | Eight-line feedback delay network, 20 ms predelay, light modulation. Size = 1.2–6 s decay |
| REVERB | 4 | Void Space | Eventide Blackhole | Longer network with wider modulation. Size = 6–60 s decay |
| REVERB | 5 | Gated Room | Non-linear gated reverb | A 1 s network gated after each attack. Size = 100–600 ms hold, then a 15 ms close |

For the reverbs, Damping sets the loop low-pass, from 12 kHz (Damping 0) down
to 2 kHz (Damping 1). The design decay times are low-frequency times, below
the damping. All laws and coefficients are authored, not measured hardware
responses. The UltraTap and Rose panels were checked against retailer and
manual excerpts. No other model has a panel review: none has controls of its
own beyond the three shared ones.

## Selection and parameters

- **Host choices:** the released `delaymodel` / `reverbmodel` / `modmodel` choices are automatable. Their option counts stay as released, so saved automation keeps its meaning.
- **Extensions:** appended `delaymodelExt` / `reverbmodelExt` / `modmodelExt` (0–15, version hint 23, not automatable) select the new models. 0 defers to the released choice.
- **Selectors:** JUCE's ComboBoxAttachment maps by item index over the item count, so it cannot drive a longer list. `ExtendedEffectModelSync` keeps the editor and ALL-panel selectors in step with both parameters instead.
- **Menus:** each new model gets its own submenu: Multi-tap / Modulated / Dual, Studio hall / Ambient / Gated, and Rotary / Vibe / Ring.

## Released models are unchanged

- **No overhead:** the new voices run only while their crossfade weight is non-zero. A voice that falls silent clears its state, so re-selecting it never replays an old tail.
- **Parity:** a copy of the released module (`Tests/reference/SpatialModulesReleased.h`) is compared sample by sample. All nine released models and two parameter sets match bit for bit (1,152,000 samples).

## Measurements (`ChimeraPostMotionSpaceTests`, 48 kHz)

- **Spin Cabinet:**
  - With Rate 2 Hz, the dominant level modulation is at 1.95 Hz. The left/right correlation is 0.30.
  - Switched from 0.7 Hz to 6 Hz, it modulates at 2.4 Hz in the first 0.25 s and 5.35 Hz 1–1.5 s later.
- **Throb Vibe:** on a 440 Hz tone, the level stays within 0.007 dB and the instantaneous pitch sweeps 428.7–451.3 Hz.
- **Ring Bell:** a 1 kHz tone into the 320 Hz carrier comes out −3.01 dB overall, as a multiply by a unit sine should. The 1 kHz component is at −65.9 dB.
- **Tap Cloud:** first and last of the six taps are 0.071 / 0.428 fading up, 0.2499 / 0.2498 equal, and 0.428 / 0.071 fading down.
- **Dual Digital:** with Time 400 ms, the first echoes arrive at 300.0 and 400.0 ms.
- **Bloom Delay:**
  - A single echo of 1 kHz is pitch-modulated between 995.3 and 1004.9 Hz.
  - The repeats darken: the fifth is 30× less bright than the first.
- **Hall Prime:** low-frequency T60 is 1.16 s at Size 0 and 5.68 s at Size 1 (designs 1.2 and 6 s).
- **Void Space:** T60 is 32.9 s at Size 0.5 (design 33 s).
- **Gated Room:** at Size 0.5 the wet level is −28 dBFS during the hold and exactly zero at 400–600 ms.
- **Stability:** extreme settings stay finite and bounded.

The test caught a bug during development. Bloom Delay's 0.13 Hz LFO was
derived from the wrapped 0.6 Hz phase, so the delay time jumped once per
cycle and a click resulted. The slow LFO now has its own phase.

## Not done

- **Room Advanced (1.8):** early/late balance, diffusion, wet filters, width, dimensions, materials and placement.
- **Per-model controls:** UltraTap's Slurm and Chop, Rose's modulation sources, DIG's ratio options, Blackhole's Gravity and Feedback, and a rotary slow/fast switch.
- **Checks:** listening tests, and CPU measurement in a DAW.
