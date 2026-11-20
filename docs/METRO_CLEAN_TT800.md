# Metro Clean — Subway TT-800 remodel

Model 11 keeps its working name **Metro Clean**. Its reference changes from the
Mesa Subway D-800+ (one channel) to the **Mesa Subway TT-800**. The source of
truth is the official TT-800 owner's manual (PDF, created 2020-05-20): the
front-panel drawing, the overview and the control descriptions. The shell
artwork is unchanged; only the knob layout on its existing blank plate, the
panel controls, the DSP and the documentation change.

Runtime labels use Chimera aliases, as for every other model: the manual's
SUBWAY channel is shown as **MODERN** and its BOOGIE channel as **CLASSIC**.

## Panel

| Section | Controls (native key) | Notes |
|---|---|---|
| Shared input | GAIN SWITCH HIGH/LOW (`hw.input_type`), DEEP (`hw.deep`), BRIGHT (`hw.bright`), MUTE (`hw.mute`) | J-FET buffer and V1 are fixed stages, common to both channels |
| MODERN (Subway, channel 0) | GAIN (`hw.input`), HIGHPASS (`hw.high_pass`), BASS (`hw.bass`), MID (`hw.lo_mid`), MID FREQ (`hw.lo_mid_frequency`), TREBLE (`hw.treble`), VOLUME (`hw.master`) | Active Baxandall EQ, ±12 dB |
| CLASSIC (Boogie, channel 1) | GAIN, HIGHPASS, BASS, MID, MID FREQ (NORM→SHIFT), TREBLE, VOLUME (`hw.classic.*`) | Passive Bass 400/400+-style tonestack, 0–10 |
| Power | O/D SYMMETRY (`hw.od_symmetry`), SYM→ASYM | Class-D power stage soft clip |

Channel SELECT is the native channel selector (and footswitchable through the
host like every native channel parameter). Not simulated: FX loops and
return-as-input, both DI outputs, tuner/aux, headphone cab emulation, the
impedance selector and its tracking damping, tube type (12AT7/12AU7).

The head face shows the active channel's seven knobs plus O/D SYMMETRY on the
existing two-row plate: GAIN · HIGHPASS · BASS · MID / MID FREQ · TREBLE ·
VOLUME · O/D SYM. The four switches stay in the switch strip below the head.
The compact head dimensions stay at the measured D-800+ envelope: the TT-800
manual lists no dimensions (see CAB_HEAD_DIMENSIONS.md).

## DSP (authored, not a measured TT-800 fit)

Signal order per channel: input trim → HIGH/LOW gain (LOW = −10 dB; the manual
gives no figure, so the released ACTIVE pad is kept) → fixed 10 Hz two-pole
sub-sonic stage → channel HIGHPASS → BRIGHT / DEEP → tube stages → channel EQ →
channel VOLUME → shared class-D power soft clip → (MODERN only) the fixed D-800+
DI capture calibration.

- **HIGHPASS (both channels):** two-pole, linear 25–125 Hz as the manual states;
  noon = 75 Hz. The variable corner is pre-warped so the pair's combined −3 dB
  point is the panel frequency. Measured −2.96 dB at 75 Hz (noon) and −3.00 dB at 125 Hz (max) on both channels.
- **DEEP:** "mild boost in the very low frequency response while also lowering
  the effective high pass": +4 dB low shelf at 55 Hz and the HIGHPASS corner ×0.7
  (floor 18 Hz). Both values are assumptions; the manual gives none.
- **BRIGHT:** "rising response at higher frequencies": +5 dB shelf at 1.7 kHz
  (the released D-800+ value).
- **MODERN:** GAIN keeps the released ±18 dB taper into two lightly driven
  stages (V3 overdrives late, "more neutral"). Baxandall shelves at 50 Hz and
  4 kHz (gentle slope), ±12 dB. MID is a ±12 dB bell on a linear 150 Hz–2 kHz
  sweep (the panel legend; the manual text says 2.5 kHz) with Q 2.1, i.e. about
  1/3 octave each side. Measured span 24.0 dB at the set frequency; BASS/TREBLE spans 22.0 / 23.7 dB at 20 Hz / 16 kHz.
- **CLASSIC:** three stages with higher stage gain, so it overdrives more
  readily than MODERN at the same GAIN ("already a higher gain channel").
  Tonestack after V2: BASS low shelf 80 Hz, −10…+14 dB; TREBLE high shelf
  2.5 kHz, −10…+16 dB ("quite a bit of high-frequency boost"); MID is an
  inherent dip at 500 Hz (MID 0 = −12 dB, ~flat near 3:30); MID FREQ moves it
  from 500 Hz to 1 kHz, widens it (Q 1.3→0.6) and halves its depth (NORM is the
  narrow, steep classic position; SHIFT is flatter and more modern). A fixed
  −6 dB authored level trim offsets the hotter driven path; it is not a
  measured level match between the channels.
- **O/D SYMMETRY:** offsets the output soft clip from the released near-symmetric
  0.02 bias (SYM) to 0.32 (ASYM), adding even harmonics and a softer entry into
  clipping. It is subtle until the output stage is driven, as the manual says (measured
  second harmonic at a driven 110 Hz: −46 dB SYM, −23 dB ASYM).
- The fixed NAM capture calibration of the D-800+ DI output (measured with
  VOICING 0 / HIGH PASS 0) stays on the MODERN channel only. It is not a TT-800
  capture; CLASSIC is uncalibrated (identity).

## Saved projects and presets

Model 11's released host parameters (`nativeAmp_c{0..5}_m11_*`, version hint 1)
keep their IDs, order, ranges and defaults, so the released parameter contract
is unchanged. Released D-800+ sessions had one channel; their stored channel 0
now selects **MODERN**, the channel the manual calls the D-800+ descendant.

| Released D-800+ parameter | TT-800 control |
|---|---|
| `hw_input` INPUT | MODERN GAIN (same taper) |
| `hw_high_pass` HIGH PASS | MODERN HIGHPASS (knob position kept; now linear 25–125 Hz instead of 30–150 Hz exponential) |
| `hw_bass` BASS | MODERN BASS |
| `hw_lo_mid` LO MID | MODERN MID |
| `hw_lo_mid_frequency` LO MID FREQUENCY | MODERN MID FREQ (knob position kept; now linear 150 Hz–2 kHz) |
| `hw_treble` TREBLE | MODERN TREBLE |
| `hw_master` MASTER | MODERN VOLUME |
| `hw_deep`, `hw_bright`, `hw_mute` | shared DEEP, BRIGHT, MUTE |
| `hw_input_type` PASSIVE/ACTIVE | shared GAIN SWITCH HIGH/LOW (same index, same −10 dB) |
| `hw_voicing`, `hw_hi_mid_frequency`, `hw_hi_mid` | **retired**: still registered and saved, never read (no TT-800 control) |

The CLASSIC controls and O/D SYMMETRY are 48 new parameters (8 × 6 contexts)
appended after every earlier bank with version hint **34**. A session without
them restores them at their defaults: CLASSIC is not selected and O/D SYMMETRY
at SYM is the released output stage, so the MODERN channel plays the stored
D-800+ values. `ChimeraGateProcessorTests` (MetroCleanStateTests.h) loads such a
session into an instance whose TT-800 controls were moved and checks IDs,
values, the MODERN channel, the defaults and that the retired values do not
reach the engine.

Sound differences for old sessions are limited to the remodelled functions:
the retired VOICING/HI MID no longer apply (the released default VOICING at
noon was a mild 550 Hz scoop), HIGH PASS / LO MID FREQUENCY positions map to the
TT-800 sweeps, Deep follows the TT-800 description and the EQ corner
frequencies follow the Baxandall description.

Factory presets using Metro Clean (Slap Studio, Wool Bass Fuzz, G+B Low Anchor,
B+B Warm / Definition, B+B Clean / Grind and others) stay on MODERN. Their MID
FREQ is set to the released lo-mid centre (301 Hz / 406 Hz) on the new sweep;
their factory VOICING offsets were small (0.08 / 0.20 of the scoop) and their
HI MID offsets are dropped. The legacy (non-native) engine for sessions saved
before the native engine is unchanged.
