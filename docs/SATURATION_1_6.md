# Three-band saturation — 1.6

Status: **implemented on `claude/three-band-saturation`** (stacked on
`claude/multiband-stereo`). Roadmap: 1.6 "an independent three-band saturation
processor" (Saturn 2 reference: band-specific saturation, drive/mix, low-band
clean preservation, a small set of authored types). Covered by
`ChimeraSaturationTests` and the common-dynamics UI tests.

Owner's decision (2026-10-28): **Live with no added latency, Studio with 4×
oversampling.**

## Placement and controls

The stage sits after the multiband dynamics and before the de-esser. Its
editor is the SATURATION page of the common-processor workspace: a header
entry and a tab, with a spectrum, draggable crossover markers and a band
selector.

The parameters append after every released parameter, at version hint 19.

| Parameter | Range | Default |
|---|---|---|
| `satOn` | off / on | off |
| `satMix` | 0–100 % | 100 % |
| `satOutput` | −24 to +12 dB | 0 dB |
| `satQuality` | Live, Studio 4x | Live |
| `satCrossover1`, `satCrossover2` | 40 Hz–1 kHz, 500 Hz–12 kHz (kept ≥ 1.5× apart) | 200 Hz, 2.5 kHz |
| `satBandN_type` (N = 1–3) | Clean, Tape, Tube, Clip | low Clean, mid Tube, high Tape |
| `satBandN_drive` | 0–36 dB | 12 dB |

Each curve has slope 1 at zero and is applied as f(g·x)/g. Drive therefore
changes harmonics and compression, not the small-signal level.

- **Tape:** tanh, soft and symmetric.
- **Tube:** tanh on the positive half, tanh(1.6 u)/1.6 on the negative half.
  - The two halves have different ceilings, so the curve stays asymmetric however hard it is driven.
  - Its DC is removed by a 5 Hz high-pass on the band's change.
- **Clip:** the cubic u − u³/3, held at ±2/3 beyond ±1.
- **Clean:** passes the band unchanged.

## Signal structure

Two Linkwitz-Riley (24 dB/oct) crossovers split the signal. The low band is
all-pass compensated at the second crossover, so the three bands add up to the
input through two second-order all-passes: levels are kept and the phase is
rotated around the crossovers.

- **When on:** `out = sum(band) + mix * sum(f(band) - band)`.
- **When off:** the input is untouched, sample-exact. Old projects are unaffected.

### Switching

Mixing the input with its phase-rotated band sum would cancel frequencies near
the crossovers: a plain crossfade dips a 2.5 kHz tone to 27 %. Instead, turning
on goes through three steps:

1. The output crossfades over 5 ms from the input to the input through its own pair of Q 0.707 all-passes tuned to 0.5 Hz. At that frequency they barely rotate anything audible.
2. The all-pass frequencies glide to the crossovers over 20 ms, where their phase equals the band sum's.
3. The output crossfades to the processed sum over 10 ms.

Turning off reverses these steps.

An earlier attempt moved the all-pass Q from 1000 to 0.707 instead. It failed:
a high-Q all-pass rings for hundreds of cycles at its frequency.

### Quality

- **Live:** runs at the host rate and adds no latency.
- **Studio 4x:**
  - Runs the same structure at 4× (JUCE polyphase IIR) and reports that latency (6 samples at 48 kHz).
  - The latency stays reported while Studio is selected, whether the stage is on or off. When off in Studio, the signal passes through the oversampling filters.
- **Changing quality:** the output fades out (10 ms), the quality swaps, and the output fades back in.

## Measurements (`ChimeraSaturationTests`, 48 kHz)

- **Off:** sample-exact.
- **On with Mix 0, or with every band Clean:** the largest level change is 4e−8 dB at 100 Hz, 1 kHz and 6 kHz. Only the phase rotates.
- **Small signals:** at −80 dBFS with 36 dB drive in every band, the level does not change measurably.
- **Clean low band**, with Tube in the mid at 30 dB drive and 60 Hz + 1 kHz at −6 dBFS:
  - 60 Hz changes by −0.068 dB. This is the share of 60 Hz that leaks into the mid band (−42 dB through the 200 Hz LR4) and is compressed with it.
  - 1 kHz is reduced by 20.8 dB.
  - Its 2nd harmonic is −39 dB re 1 kHz.
- **Harmonics**, 1 kHz at −12 dBFS, 6 / 30 dB drive:

  | Type | 3rd harmonic (re 1 kHz) | 2nd harmonic |
  |---|---|---|
  | Tape | −34.2 / −10.1 dB | none |
  | Clip | −33.1 / −9.8 dB | none |
  | Tube | −29.8 / −10.0 dB | −31.4 / −30.1 dB |

- **Band isolation:** a 6 kHz tone in a Clean high band changes by −0.03 dB while the mid band is driven with Clip at 36 dB.
- **Switching on and off**, measured in 2 ms RMS windows:
  - With a 2.5 kHz tone at the crossover and all bands Clean, the level stays between 0.341 and 0.370 (steady 0.354). A plain crossfade measured 0.095.
  - With Clip at 36 dB on the mid band, the level stays between the off and on levels within 6 %.
  - The remaining ±4 % ripple appears while the all-pass frequency glides. It most likely comes from that brief frequency shift against the 2 ms windows; it is not a cancellation.
- **Studio 4x vs Live**, with Clip at 24 dB on a 7 kHz tone:
  - The 5th harmonic's alias at 13 kHz is −15.2 dB re tone in Live and −61.6 dB in Studio.
  - Latency is 0 samples in Live and 6 in Studio.
- **Parameters:** 12, appended at version 19.
- **No allocation** while processing.

## Not done

- Per-band level and band solo, modulation (Saturn's), and more types.
- A crossfade-free quality change. Today a quality change fades the output out and back in.
- Listening tests, and CPU measurement in a DAW.
