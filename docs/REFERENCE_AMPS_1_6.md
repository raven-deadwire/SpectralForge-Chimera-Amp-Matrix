# Reference AMP Expansion — 1.6

Status: **implemented on `claude/reference-amps`**, stacked on
`claude/three-band-saturation`. Roadmap 1.6: "Reference AMP Expansion", five
models. Covered by `ChimeraReferenceAmpTests` and the existing native-amp, head
and catalog tests.

Owner's decisions (2026-10-28):
- the bass head is the Vanderkley Aurora;
- the working names are as proposed below.

## Models

They are appended after the two native originals as stable models 26–30.

| Model | Chimera name | Reference | Channels | Panel controls | Input routes |
|---|---|---|---|---|---|
| 26 | Crystal 120 | Roland JC-120 | CH1 / NORMAL, CH2 / EFFECT | Per channel: Volume, Treble, Middle, Bass, Bright. CH2 adds Distortion, Reverb, Speed, Depth and EFFECT (OFF / CHORUS / VIBRATO) | HIGH / LOW |
| 27 | Bluestack 100 | Marshall 1987X | single | Loudness I (High Treble), Loudness II (Normal), Treble, Middle, Bass, Presence | High Treble HIGH/LOW, Normal HIGH/LOW, JUMPED |
| 28 | Eightyfire | Marshall JCM800 2203 | single | Presence, Bass, Middle, Treble, Master Volume, Pre-amp Volume | HIGH / LOW |
| 29 | Fortress 103 | Hiwatt DR103 | single | Normal Volume, Bright Volume, Bass, Treble, Middle, Presence, Master Volume | Normal HI/LO, Bright HI/LO, JUMPED |
| 30 | Northlight | Vanderkley Aurora | single | Pad (0 / −12 dB), EQ on, Low, Low Mid, Mid, High Mid, High, Damping, Master | INPUT |

Each panel follows the controls the reference publishes. Each DSP is an
original Chimera design on the shared native engine, not a measured circuit
or a capture.

### Sources

The manufacturers' sites are not reachable from the build environment, so the
panels were checked against the following sources:
- **Crystal 120:** a retail listing and an interview. These say Speed and Depth work only on the vibrato and the chorus has fixed settings.
- **Fortress 103:** the Hiwatt Custom Shop owner's manual (britampco.co.uk). It labels the inputs Normal and Bright: earlier units' "Brilliant", with the Hi input brighter and louder than the Lo.
- **Bluestack 100 and Eightyfire:** the well-documented panel layouts, recorded as a partial review.
- **Northlight:** stays *pending review*.
  - Public sources disagree on its band count (four or five) and publish no band frequencies.
  - The five bands at 40 Hz (shelf), 250 Hz, 800 Hz, 2.5 kHz and 8 kHz (shelf), ±15 dB, are Chimera's choice.
  - The Damping law is also Chimera's.

## Signal design

- **Crystal 120:**
  - The two transistor channels run their stages at half drive and restore the level at the output. They stay clean at playing levels.
  - Distortion opens a third stage on CH2, with up to +30 dB of drive.
  - Reverb uses the engine's spring algorithm.
  - The effect is a modulated delay after the reverb:
    - **Chorus:** a fixed 0.8 Hz sweep of 7 ± 1.5 ms, mixed 50 %. The second output's sweep is inverted.
    - **Vibrato:** 3 ms ± (Depth × 2 ms) at 1–10 Hz (Speed), fully wet. Its added 3 ms of delay is not reported as latency.
  - The hardware sends dry sound to one speaker and the chorused sound to the other. Chimera mixes both into each output instead, so a mono path still hears the chorus.
  - The effect mix fades on a 12 ms slew, so the selector does not click.
- **Bluestack 100:**
  - The JTM45 panel's parallel Normal / High Treble paths, with a hotter preamp and power stage.
  - The JUMPED route mixes both paths, and the LOW inputs are −6 dB.
  - There is no master volume.
- **Eightyfire:**
  - Pre-amp Volume drives four stages, the second biased cold, using the 36 dB drive taper the other drive channels use.
  - The tone stack and Presence follow the preamp, and Master Volume follows them.
- **Fortress 103:** the same two-path input structure, with a stiff supply and a low power drive. It keeps clean headroom.
- **Northlight:**
  - There is no tube preamp stage. The five-band EQ (bypassed when EQ is off) feeds a soft power limit at half drive.
  - Lower Damping adds up to +6 dB of low resonance at 75 Hz; full Damping is flat.

## Easy view

Easy GAIN edits the knob the selected input plays through, checked against the DSP:
- **Bluestack 100:** Loudness I on the High Treble and JUMPED inputs, Loudness II on the Normal inputs, as on the JTM45 panel.
- **Fortress 103:** Normal Volume on the Normal inputs, Bright Volume on the Bright and JUMPED inputs.
- **Eightyfire:** Pre-amp Volume.
- **Northlight:** Master, because the head has no gain stage.
- **Crystal 120:** the channel Volume.

## Stereo

Crystal 120's default effect is the chorus, so its two outputs differ by design. `AmpVoiceTests` exempts it from the rule that identical stereo inputs give identical outputs; `ReferenceAmpTests` measures the spread instead. Every other head still meets that rule.

## Parameters

Each model has a channel, an input route and its panel controls in all six
amp contexts, appended after the saturation bank at version hint 20: 318
parameters. `appendNewAmpNativeParameters` takes the version hint as an
argument; the released groups keep their hints.

## Head artwork

`Tools/generate_reference_head_shells.py` draws each complete head shell
procedurally:
- `head-shell-26.png` … `head-shell-30.png`;
- the compact fascia rasters (`acrystal.png` and the others);
- the provenance entries, the metadata rows and the reviewed plate fixtures.

The fascias are original Chimera designs carrying the working name, with no manufacturer logo or trade dress. They are simpler than the photographic shells 0–25. Northlight is a compact desktop unit with no handle. Live knobs and labels remain JUCE controls on the blank plate.

## Measurements (`ChimeraReferenceAmpTests`, 48 kHz)

- **Crystal 120:**
  - THD at 440 Hz, 0.1 peak input: −65.7 dB on CH1 and −65.5 dB on CH2, and −30.7 dB with Distortion at maximum.
  - Chorus: Speed and Depth change nothing (difference 0). The left/right difference is 1.04 × the left output's RMS.
  - Vibrato on a 1 kHz tone, peak deviation:
    - 0.0004 Hz at zero depth;
    - 12.58 Hz at 1 Hz with full depth (analytic 12.57);
    - 125.6 Hz at 10 Hz with full depth (analytic 125.7).
  - Effect switching OFF → CHORUS → VIBRATO while playing:
    - The largest second difference is 6.7e−5, against 4.2e−5 in the settled states. This is the slope change as the mix slew starts.
    - A version without the slew measured 0.036, and the test fails on it.
- **Bluestack 100:**
  - Small-signal 3 kHz re 200 Hz: +6.5 dB through High Treble, −11.3 dB through Normal.
  - The Low input is −6.02 dB.
- **Eightyfire**, 220 Hz at 0.01:
  - THD −41.8 dB with Pre-amp Volume at 0.2, and −12.4 dB at 0.9.
  - The 2nd harmonic is −22.5 dB (from the cold-biased stage).
- **Fortress 103:**
  - 220 Hz at 0.1, Normal volume 0.6: THD −24.6 dB, against −12.2 dB for Bluestack 100 at the same settings.
  - Small-signal 3 kHz re 200 Hz: Bright +7.8 dB, Normal −10.0 dB.
- **Northlight:**
  - With the EQ centred, 60 Hz–8 kHz stays within 0.05 dB of 1 kHz. The same holds with the EQ off and bands at maximum.
  - Band span, minimum to maximum:
    - 30.0 dB at 250 Hz, 800 Hz and 2.5 kHz;
    - 24.9 dB at 25 Hz (low shelf);
    - 29.4 dB at 16 kHz (high shelf).
  - Pad −12.00 dB; Damping 0 adds 6.0 dB at 75 Hz.
  - THD at 41.2 Hz, 0.25 peak: −54.2 dB.
- **Native-engine contracts (`ChimeraAmpNativeTests`):** every new control changes the audio in its engaged route. Every input route is audible. There is no allocation in `set`/`tick` and no stereo crosstalk.

A measurement fix during development: an earlier version of the THD helper used a different DFT window per harmonic. That leaked the fundamental into the 2nd-harmonic bin and read −32 dB at any level. All harmonics now share the fundamental's window.

## Not done

- Checking the panels against the manufacturers' manuals, especially the Aurora's.
- Matching the hardware's sound. The coefficients are authored, with no capture or circuit fit.
- Listening on real DI, and DAW automation/recall.
- Photographic head artwork like models 0–25.
