# POST expansion — 1.7

Status: **implemented on `claude/post-expansion`**, stacked on
`claude/filter-modulation`. Roadmap 1.7: nine POST models, three each for BUS
COMP, PREAMP and EQUALIZER. Covered by `ChimeraPostExpansionTests`,
`ChimeraPostNativeTests` and the native UI tests.

Owner's decision (2026-10-28): the working names below, as proposed.

## Models

The new models are native POST models 3–5 of their section. Released models
0–2 keep their indices, IDs and sound: the cached-DSP parity test still
compares them bit for bit against the reference implementation.

| Section | Index | Chimera name | Reference | Main controls |
|---|---|---|---|---|
| BUS COMP | 3 | Polar Press | Eventide Omnipressor | Input Cal, Function, Threshold, Attack, Release, Atten Limit, Gain Limit, Bass Cut, Output, Meter, In |
| BUS COMP | 4 | Thrust Bus | API 2500 | Threshold, Attack (0.03–30 ms), Ratio (1.5:1–∞:1), Release (0.05–2 s, Var), Knee, Thrust, Type New/Old, L/R Link, Output Gain, In |
| BUS COMP | 5 | Diode Bridge | Neve 33609 | Compressor threshold / ratio (1.5–6:1) / recovery (100 ms–1.5 s, A1, A2) / attack, Gain, In. Limiter threshold / recovery / attack, In |
| PREAMP | 3 | Discrete Lift | API 512c | Input Mic / Line·Inst, Gain, Pad −20 dB, Phase, +48 V (disabled) |
| PREAMP | 4 | Desk Two | Chandler TG2 | Input Mic / DI / Line, Coarse +20…+60 in 5 dB steps, Fine ±10 dB, Output, Load (disabled), Phase, +48 V (disabled) |
| PREAMP | 5 | Clear Line | Grace m101 | Input Mic / Hi-Z, 12-step Gain (5 dB), Trim 0–10 dB, HPF 75 Hz 12 dB/oct, Ribbon (disabled), +48 V (disabled) |
| EQUALIZER | 3 | Stepped EQ | API 550A | LF / MF / HF frequency and stepped gain (±2/4/6/9/12 dB), LF/HF shelf switches, 50 Hz–15 kHz filter, In |
| EQUALIZER | 4 | Air Band | Mäag EQ4 | 10 Hz, 40, 160, 650 Hz (−4.5…+15 dB, Q 0.4), 2.5 kHz shelf, AIR gain (0–20 dB) and frequency (Off, 2.5–40 kHz), In |
| EQUALIZER | 5 | Curve Shaper | Chandler Curve Bender | Treble (shelf/bell), Presence 1, Presence 2, Bass (shelf/bell), stepped frequencies, HPF, LPF, In |

### Sources

Manufacturer pages are not reachable from the build environment. Each panel
was checked against reviews, retailer specification sheets and plug-in
manuals, listed in `Preparation/rack_controls.py`. All nine are recorded as a
partial review.

Known gaps:
- **API 550A:** frequency lists differ between sources; the hardware specification sheet is used.
- **Curve Bender:**
  - Bass and Presence frequencies and the HPF values come from published manuals.
  - The Treble and LPF positions are Chimera's choice.
  - The Multiply switch is omitted.
- **Physical-only controls** (phantom power, Load, Ribbon) are shown disabled. They do nothing on recorded audio.

## Selection and parameters

- **Selector:** the released `pn_*_model` selector stays at 0–2. Each section gains an appended `pn_*_modelExt` (0–15).
  - 0 defers to the released selector; 1–3 select models 3–5.
  - Choosing a released model writes the selector and clears the extension.
  - Choosing a new model writes only the extension.
- **Native path:** models 3–5 exist only natively. Selecting one engages the native path for that section even if the section was on its legacy engine.
- **Parameters:** each new bank (bypass, trim, level and its controls) and the three extension selectors append after the filter bank, at version hint 22.

## Signal design

All coefficients and laws are authored, not measured. Thresholds are panel
dB re −18 dBFS.

- **Polar Press:** one slope applies across the threshold.
  - Function sets the slope: 2.5 at ¼, 1 at ½ (neutral), 0 at ¾ (infinite compression) and −1 at full (reversal).
  - The gain is held between −Atten Limit and +Gain Limit.
  - Input Cal shifts the threshold scale by 10/20/30 dB.
  - Bass Cut high-passes the detector at about 150 Hz.
- **Thrust Bus:** per-channel RMS detectors.
  - Type New detects the input (feed-forward); Old detects the output (feedback).
  - Thrust subtracts half (Med) or all (Loud) of a ~120 Hz low-pass from the detector.
  - Link blends each detector towards the louder one.
  - Knee: hard, 6 dB or 12 dB.
- **Diode Bridge:** a compressor and a limiter in series on one linked detector.
  - A1/A2 recover fast after short peaks and slowly after sustained reduction.
  - Odd-order colour grows with the total gain reduction.
- **Preamps:** the panel gain is offset so the default setting is unity on recorded audio.
  - Discrete Lift: slightly asymmetric discrete clipping.
  - Desk Two: earlier, more asymmetric transformer saturation, with low weight and a softened top. Output follows the saturation.
  - Clear Line: high headroom, almost no colour.
- **EQs:** biquad bands with coefficient interpolation.
  - Stepped EQ narrows its bandwidth as the gain grows (proportional Q).
  - The AIR shelf at 40 kHz is limited to 0.44 of the processing rate.

## Measurements

### `ChimeraPostExpansionTests` (48 kHz)

- **Polar Press:**
  - Output change per input dB from −30 to −20 dBFS: 1.0006 neutral, 0.0000 at ¾, −1.0006 reverse, 2.5015 at ¼.
  - With Atten Limit 6 dB, a loud tone in reversal is held at −6.00 dB.
- **Thrust Bus:**
  - 4:1 hard knee: +2.49 dB out for +10 dB in.
  - A quiet right channel is reduced 5.36 dB at link 100 % and 0.0003 dB at link 0 %.
  - At 60 Hz the gain is −12.3 dB at Thrust NORM and −7.4 dB at LOUD.
- **Diode Bridge:**
  - 2:1: +4.90 dB out for +10 dB in.
  - The limiter at panel 8 holds a 0 dBFS-peak tone at −10.07 dBFS RMS.
  - THD at 200 Hz: −106 dB with little reduction, −86 dB with heavy reduction.
- **Preamps:**
  - Default small-signal gain: −0.005 dB (Discrete Lift), −0.04 dB (Desk Two) and −0.002 dB (Clear Line).
  - Pad −20.00 dB. Coarse and gain steps +5.00 dB.
  - Clear Line HPF: −3.02 dB at 75 Hz.
  - THD at 200 Hz with about +10–12 dB of drive: Desk Two −31.7 dB, Discrete Lift −37.0 dB, Clear Line −42.4 dB.
- **EQs:**
  - Stepped EQ +12 dB at 1.5 kHz measures 12.00 dB. Half an octave up keeps 41 % of the gain at +12 dB, against 81 % at +2 dB.
  - Air Band: the 650 Hz bell measures +15.00 dB. AIR 10 kHz at +20 dB gives +19.0 dB at 20 kHz and +0.18 dB at 1 kHz.
  - Curve Shaper: Presence 1 measures +15.00 dB at 2.8 kHz; the 80 Hz HPF measures −3.01 dB at its corner.

### `ChimeraPostNativeTests` (18 models)

- Every connected control changes the audio: 138 audio controls and 4 meter controls.
- Bypass is exact, results do not depend on the host block size, and there is no stereo leakage.
- Rate-switch stress is stable, and reset leaves no residue.
- The cached-DSP parity test still matches the released nine models bit for bit.

## Artwork

`Tools/generate_post_rack_faces.py` draws nine original rack faces
(`r<name>.png`) with the working names and no logos.

## Not done

- Checking the panels against the manufacturers' manuals.
- Sound matching to the hardware.
- Listening, and CPU in a DAW.
- The Curve Bender Multiply switch.
- 2500 vs 2500+ differences.
