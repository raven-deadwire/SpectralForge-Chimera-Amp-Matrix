# Doubler: comb filtering and mono sum

**Follow-up audit (2026-10-27 KST):** see [validation, CPU and compatibility evidence](DOUBLER_VALIDATION_20261027.md). The original table below is retained as a historical band-average measurement, not a guarantee for each note or waveform.

The Doubler (`Source/PerformanceUtilities.h`) runs after the amp and cabinet
on a stereo bus. It mixes into each channel a delayed copy of itself: left
delayed by 0.55 × spread, right by 1 × spread (1–20 ms, default 6 ms), both
modulated ±0.25 ms at 0.13 Hz, at 35 %. A delayed copy mixed into the same
signal is a comb filter, and summing the channels to mono combines two
combs. This note measured what the original design did, and what the
revised one does.

**Revised design (options 1 and 2 below):** a 200 Hz, 2nd-order Butterworth
high-pass attenuates lows in the delayed path. Its finite slope and phase
mean that frequencies below 200 Hz are not exactly dry. Identical mono
input stays nearly centred at A0/A1; existing stereo lows are not folded
to mono. The mixed high band uses g = 1 / √((1 − m)² + m²)
(≈ +2.6 dB at m = 0.35), a normalization for uncorrelated paths, not a
guarantee of constant RMS, loudness or peak level for arbitrary input. Per channel:
out = x + h·(g·(1 − m) − 1) + g·m·delayed(h), with h the high band of x.
Off (m = 0) is still bit-identical to the input.

## Method

`Tests/DoublerMonoProbe.cpp` (`ChimeraDoublerMonoProbe`) drives the real
`PerformanceUtilities` at 48 kHz:

- **Response:** 8 s of white noise, identical on both channels. Welch power
  spectra (8192-point Hann, 50 % overlap, first 0.1 s skipped) of the left,
  right and mono-sum (L+R)/2 outputs against the input, per 1/3 octave from
  31.5 Hz to 16 kHz. Reported: the worst band and its level, the mean over
  40–160 Hz (Drop A fundamentals and the palm-mute body), the arithmetic mean of band dB values (not broadband RMS or loudness), and the ripple (largest band deviation from that mean).
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
the mean of 40–160 Hz bands from −4.7 dB to within ±0.7 dB, the equal-band
dB mean from −3.4 to −0.3 dB, and this synthetic pluck attack from −4 dB
to 0 dB. These are not individual-frequency, broadband RMS or real-DI
guarantees. White-noise L/R correlation remains approximately 0.84.

What remains: the upper band still combs. At frequencies well above the
crossover its ideal notch/peak approach −7.8/+2.6 dB, but the complex
high-pass crossover permits a **+2.816 dB** frozen-phase sine response near
318 Hz. Arbitrary transient peaks are not bounded by that sine figure.
The 40–160 Hz band averages hide individual responses down to about −2.32 dB
and up to +2.12 dB (dense frozen-phase calculation; see the follow-up for
actual time-varying renders). A0 is outside the old 40–160 Hz average.

The actual downstream order is Final EQ → output trim/tuner mute → output
limiter. The compressor, multiband and De-esser are upstream of Doubler.
The limiter is **off by default**; only when enabled does it enforce its
sample-peak ceiling. Doubler uses floating-point audio and does not itself
clip or limit values above 1.0. Headroom must be checked before an integer
export or physical output. This limiter is not a true-peak limiter.

The follow-up caches normalization coefficients when the smoothed mix
changes and caches click-envelope decay at prepare time. It preserves
#192 audio (apart from preserving signed zero at complete bypass).

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
