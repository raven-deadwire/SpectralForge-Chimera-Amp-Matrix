# Doubler: comb filtering and mono sum

The Doubler (`Source/PerformanceUtilities.h`) runs after the amp and cabinet
on a stereo bus. Each channel becomes 65 % dry plus 35 % of a delayed copy of
itself: left delayed by 0.55 × spread, right by 1 × spread (1–20 ms, default
6 ms), both modulated ±0.25 ms at 0.13 Hz. A delayed copy mixed into the
same signal is a comb filter, and summing the channels to mono combines two
combs. This note measures what that does, as the basis for deciding whether
the Doubler's structure should change. It changes no product code.

## Method

`Tests/DoublerMonoProbe.cpp` (`ChimeraDoublerMonoProbe`) drives the real
`PerformanceUtilities` at 48 kHz:

- **Response:** 8 s of white noise, identical on both channels. Welch power
  spectra (8192-point Hann, 50 % overlap, first 0.1 s skipped) of the left,
  right and mono-sum (L+R)/2 outputs against the input, per 1/3 octave from
  31.5 Hz to 16 kHz. Reported: the worst band and its level, the mean over
  40–160 Hz (Drop A fundamentals and the palm-mute body), the mean over all
  bands, and the ripple (largest band deviation from that mean).
- **Width:** left/right correlation of the noise output.
- **Attack:** A1 palm-mute plucks every 160 ms; mean peak of the first 3 ms of
  each pluck against the dry plucks.
- **Gates** (CTest): Doubler off is bit-identical to the input, and the
  strongest lag between input and output is 0 (the dry path is not delayed).

## Results (2026-10-26)

| Spread ms | Worst band L / R / mono (dB @ Hz) | 40–160 Hz L / R / mono dB | Mean dB (mono) | Ripple dB L / R / mono | L/R corr. | Attack L / mono dB |
|---|---|---|---|---|---|---|
| 1 | −5.5 @ 794 / −7.8 @ 500 / −5.9 @ 630 | −0.1 / −0.4 / −0.3 | −2.5 | 3.5 / 5.4 / 3.4 | 0.84 | −0.6 / −0.9 |
| 3 | −8.9 @ 315 / −9.3 @ 157 / −7.1 @ 198 | −1.2 / −4.1 / −2.5 | −3.1 | 6.2 / 6.3 / 3.9 | 0.84 | −3.0 / −4.3 |
| **6** (default) | −9.5 @ 157 / −9.4 @ 79 / −7.1 @ 99 | −4.7 / −4.8 / −4.8 | −3.4 | 6.4 / 6.1 / 3.7 | 0.84 | −4.1 / −3.9 |
| 10 | −8.4 @ 99 / −9.7 @ 50 / −7.2 @ 62 | −5.0 / −4.6 / −5.1 | −3.6 | 5.2 / 6.4 / 3.6 | 0.84 | −3.8 / −3.6 |
| 15 | −9.9 @ 62 / −8.2 @ 31 / −6.8 @ 39 | −4.6 / −2.7 / −3.7 | −3.5 | 6.7 / 5.2 / 3.3 | 0.84 | −3.5 / −3.1 |
| 20 | −8.9 @ 50 / −7.4 @ 31 / −6.9 @ 31 | −4.1 / −2.8 / −3.7 | −3.5 | 5.6 / 4.5 / 3.4 | 0.83 | −3.1 / −3.8 |

The first notch of each channel sits at 1 / (2 × delay): at the default 6 ms,
83 Hz on the right and 152 Hz on the left. A 65/35 mix limits a notch to
20·log10(0.65 − 0.35) ≈ −10.5 dB; the measured −9.4 to −9.9 dB is that,
averaged over a 1/3-octave band and the modulation.

## Findings

1. **Each channel is combed, not only the mono sum.** Every spread puts
   −7 to −10 dB notches in each channel; from 3 ms up they fall in the low
   band. At the default 6 ms the 40–160 Hz region drops 4.7–4.8 dB in each
   channel and in mono, about 1.3 dB more than the broadband mean: Drop A
   fundamentals and palm-mute body lose weight when the Doubler is on.
2. **Turning the Doubler on makes the sound about 3 dB quieter** (mean −2.5 to
   −3.6 dB): 0.65² + 0.35² of the power for uncorrelated content, with no
   compensation. Pick attacks drop 3–4 dB from 3 ms spread up. In an A/B or
   on/off comparison the Doubler therefore sounds smaller as well as wider.
3. **Mono compatibility is not the weak point.** The mono sum has less ripple
   (3.3–3.9 dB) than either channel (4.5–6.7 dB) because the two combs fill
   each other's notches, and its notches are shallower (−6.8 to −7.2 dB).
4. **The width is modest:** L/R correlation stays at about 0.84 for every
   spread, since both channels keep 65 % of the same dry signal. Spread moves
   the notches, not the width.
5. **The dry path is undelayed** and Doubler off is bit-identical, as
   intended.

Compared with the benchmark (pre-amp, independent takes with their own
timing, pitch and amp processing): this Doubler is a short-delay stereo
decorrelator on the finished sound, so it cannot produce two performances
and its cost shows up as comb filtering and level loss.

## Options (not implemented)

From least to most change:

1. **Level-compensate the wet mix** (scale by 1 / √(0.65² + 0.35²) ≈ +2.6 dB
   with a safety limit) so on/off compares width, not loudness.
2. **Keep the low band out of the delayed copy** (high-pass the delayed path
   around 150–250 Hz, or split and keep the lows dry and centred). This
   removes the low-band notches where Drop A lives, at the cost of width
   below the split.
3. **Decorrelate instead of delaying** (all-pass or short random-phase
   filters per channel) to widen without regular comb notches.
4. **Pre-amp doubling** like the benchmark: a second take path with its own
   timing/pitch variation and its own amp instance. Real doubling, but it
   doubles the amp CPU and is a larger design change.

## Running

```
ChimeraDoublerMonoProbe
```

Runs in about 1.5 s and is part of CTest.
