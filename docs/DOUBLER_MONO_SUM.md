# Doubler: comb filtering and mono sum

The Doubler (`Source/PerformanceUtilities.h`) runs after the amp and cabinet
on a stereo bus. It mixes into each channel a delayed copy of itself: left
delayed by 0.55 × spread, right by 1 × spread (1–20 ms, default 6 ms), both
modulated ±0.25 ms at 0.13 Hz, at 35 %. A delayed copy mixed into the same
signal is a comb filter, and summing the channels to mono combines two
combs. This note measured what the original design did, and what the
revised one does.

**Revised design (options 1 and 2 below):** only the band above 200 Hz
(2nd-order Butterworth high-pass) is delayed and mixed, so the low band
passes dry and centred; and the mixed band is scaled by
g = 1 / √((1 − m)² + m²) (≈ +2.6 dB at m = 0.35) so its power, and the
overall level, stays the same when the Doubler is turned on. Per channel:
out = x + h·(g·(1 − m) − 1) + g·m·delayed(h), with h the high band of x.
Off (m = 0) is still bit-identical to the input.

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

## Results before the change (2026-10-26, original design)

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

## Results after the change

| Spread ms | Worst band L / R / mono (dB @ Hz) | 40–160 Hz L / R / mono dB | Mean dB (mono) | Ripple dB L / R / mono | L/R corr. | Attack L / mono dB |
|---|---|---|---|---|---|---|
| 1 | −1.9 @ 1000 / −3.7 @ 630 / −2.4 @ 630 | −0.0 / 0.3 / 0.1 | −0.2 | 2.5 / 3.7 / 2.6 | 0.84 | 1.5 / 1.4 |
| 3 | −4.6 @ 397 / −3.6 @ 250 / −2.2 @ 500 | 0.6 / 0.7 / 0.7 | −0.3 | 4.6 / 3.5 / 1.9 | 0.84 | 0.0 / −0.1 |
| **6** (default) | −3.0 @ 500 / −1.0 @ 125 / −1.8 @ 250 | 0.7 / −0.0 / 0.3 | −0.3 | 2.9 / 2.1 / 1.5 | 0.84 | 0.0 / −0.0 |
| 10 | −2.4 @ 315 / −0.5 @ 16000 / −1.1 @ 315 | −0.0 / 0.3 / 0.1 | −0.3 | 2.4 / 1.2 / 1.5 | 0.84 | −0.1 / −0.0 |
| 15 | −1.4 @ 198 / −0.8 @ 125 / −0.8 @ 16000 | 0.3 / 0.1 / 0.2 | −0.3 | 1.7 / 1.0 / 1.6 | 0.84 | −0.1 / 0.1 |
| 20 | −1.1 @ 250 / −0.5 @ 16000 / −0.8 @ 16000 | 0.1 / 0.3 / 0.2 | −0.3 | 1.6 / 0.6 / 1.3 | 0.84 | −0.1 / −0.1 |

At the default 6 ms: the worst 1/3-octave band goes from −9.5 to −3.0 dB,
the 40–160 Hz band from −4.7 dB to within ±0.7 dB, the level change on
turning it on from −3.4 to −0.3 dB, and the pick attack from −4 dB to 0 dB.
Width (L/R correlation 0.84) is unchanged.

What remains, by design: above 200 Hz each channel is still a comb (that is
the doubling effect). Inside a band the comb still reaches
20·log10(g·(1 − m) − g·m) ≈ −7.8 dB at its exact notches and +2.6 dB at its
peaks (g = 1.355 instead of 1.0 before); 1/3-octave averages hide that at
high frequencies, where many notches share a band. The peaks are new: a
steady tone that sits on one can come out up to 2.6 dB louder. The Doubler
runs before the output dynamics and limiter. Just below the crossover the
compensation adds up to +0.7 dB.

## Findings (original design)

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

## Options

From least to most change (1 and 2 are now implemented):

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
