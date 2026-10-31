# Studio true-peak output limiter — 1.5 candidate

Status: **implemented on `claude/integration-1-10`**. Covered by `ChimeraTruePeakLimiterTests`.

Roadmap: "Target a Studio true-peak/LUFS candidate in 1.5, independently of
Live limiter acceptance." The [loudness meter](LOUDNESS_METER_1_5.md) was the
first half of that target. This is the second half: a limiter mode whose
ceiling holds the true peak the meter reads.

## What changes

The output limiter (DYNAMICS → LIMITER) has a mode:

| Mode | Ceiling | Lookahead | Latency |
|---|---|---|---|
| **Live** (default) | Sample peak, dBFS | None | 0 |
| **Studio true peak** | True peak, dBTP (4× interpolated; 2× from 160 kHz) | 1 ms | 1 ms + 8 samples (56 at 48 kHz, 52 at 44.1 kHz) |

- **Live** is the existing limiter, unchanged. With Live selected, the audio is bit-identical to a build without this feature (tested).
- The **ceiling and release** controls are shared by both modes.
- **Parameter:** `limiterMode` (choice: Live / Studio true peak), appended after every other bank at version hint 32. Old sessions open in Live.

## How Studio works

`Source/TruePeakLimiter.h`, stereo linked.

1. **Detector:** the interpolator the loudness meter uses, now shared in `Source/TruePeak.h`. For each input sample it gives the largest true-peak point within one sample of the input 8 samples back. The meter's arithmetic is unchanged, and its tests still pass.
2. **Gain need:** the ceiling, 0.01 dB lower, divided by that peak. Below the ceiling the need is 1.
3. **Running minimum** of the need over 1 ms + 2 samples.
4. **Release:** the gain may only rise, exponentially at the release time. The fall happens in the next step.
5. **1 ms moving average.** Every value it averages is a minimum covering the same three detector points. So the averaged gain never exceeds what those points need: the ramp down is finished when the peak arrives, and it is linear and has no step.
6. **Audio delay:** the audio is delayed by 1 ms + 8 samples, which is the reported latency.

With no reduction the gain is exactly 1. The output is then the input delayed, bit for bit (tested).

**Why the 0.01 dB margin:** the gain moves between the samples an inter-sample peak is built from. Without the margin, outputs measured up to 0.0066 dB over the ceiling.

## Latency and switching

Like Saturation's Studio 4×, the latency is reported while Studio is selected, whether the limiter is on or off. A mode change works like this:

1. The limiter output fades out over 10 ms.
2. The mode swaps and the lookahead line is cleared.
3. The output fades back in over 10 ms.

The processor reports the new latency from the swap. Host bypass and the SYNTH bus delay follow the reported latency.

## Measurements (`ChimeraTruePeakLimiterTests`)

**Ceiling:** worst output true peak −0.0034 dB relative to the ceiling, measured with the loudness meter over the whole output from silence. The sweep covers:
- 5 rates: 44.1, 48, 88.2, 96 and 192 kHz.
- 3 ceilings: −0.1, −1 and −3 dBTP.
- 6 programmes:
  - fs/4 at 45° (true peak 3 dB above its samples);
  - +6 dBFS sines;
  - loud noise;
  - full-scale square bursts;
  - a 0.23·fs sine;
  - AM 8 kHz / 11.025 kHz tones.

**Why Live is not a true-peak ceiling:** the same fs/4 sine with its samples at −1 dBFS, which a −1 dBFS sample-peak ceiling passes untouched, reads +2.13 dBTP.

**Transparency:** below the ceiling, the output is the input delayed by exactly the reported latency, bit for bit (44.1/48/96/192 kHz). After a release the gain returns to exactly 1.

**Processor:**
- Latency at 48 kHz is 42 samples with Live and 98 with Studio (+56).
- With +12 dB output gain and a −3 dBTP ceiling, the final output read −3.010 dBTP on the processor's own meter.
- Switching Studio to Live: the largest sample step was 0.0360, the same as the programme alone.

**Cost:** 0.4% of one core for the stereo Studio limiter at 48 kHz, on 10 s of loud noise. It allocates nothing while processing.

## Limits

- The lookahead is fixed at 1 ms. There is no user lookahead or attack control, and no oversampled gain stage: the gain is computed and applied at the host rate.
- True peak is the meter's estimate: 4× (2× at 160 kHz and above), as BS.1770-4 asks. A downstream converter with a different reconstruction filter may differ by a few hundredths of a dB.
- The **A/B level match** runs after the limiter and only cuts, so it cannot raise a peak.
- The **SYNTH bus** is not limited. It is delayed by the reported latency to stay aligned with the main output.
- A mode change dips the output for 20 ms. The SYNTH bus delay follows the latency at the swap, so it can step at that moment.
