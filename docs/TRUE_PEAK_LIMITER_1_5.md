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
| **Studio true peak** | True peak, dBTP (4× interpolated; 2× from 160 kHz) | 1 ms | 1 ms + 16 samples (64 at 48 kHz, 60 at 44.1 kHz) |

- **Live** is the existing limiter, unchanged. With Live selected, the audio is bit-identical to a build without this feature (tested).
- The **ceiling and release** controls are shared by both modes.
- **Parameter:** `limiterMode` (choice: Live / Studio true peak), appended after every other bank at version hint 32. Old sessions open in Live.

## How Studio works

`Source/TruePeakLimiter.h`, stereo linked.

1. **Detector:** the interpolator the loudness meter uses, now shared in `Source/TruePeak.h`. For each input sample it gives the largest true-peak point within one sample of the input 8 samples back. The meter's arithmetic is unchanged, and its tests still pass.
2. **Gain need:** the ceiling, 0.01 dB lower, divided by that peak. Below the ceiling the need is 1.
3. **Running minimum** of the need over 1 ms + 18 samples. It covers every detector point within 9 samples on both sides of the sample the gain is applied to, so the gain stays flat across the whole span an inter-sample peak is interpolated from. The first version used only the nearest three points; its gain still sloped inside that span, so fast releases overshot by up to 0.006 dB.
4. **Release:** the gain may only rise, exponentially at the release time. The fall happens in the next step.
5. **1 ms moving average.** Every value it averages is a minimum covering the same span. So the averaged gain never exceeds what that span needs: the ramp down is finished before the span begins, and it is linear and has no step.
6. **Audio delay:** the audio is delayed by 1 ms + 16 samples, which is the reported latency.

With no reduction the gain is exactly 1. The output is then the input delayed, bit for bit (tested).

**Why the 0.01 dB margin:** a reserve for float rounding and the gain's slope at the far ends of the flat span. The measured worst case sits 0.005 dB under the ceiling.

## Latency and switching

Like Saturation's Studio 4×, the latency is reported while Studio is selected, whether the limiter is on or off. A mode change works like this:

1. The limiter output fades out over 10 ms.
2. The mode swaps and the lookahead line is cleared.
3. Into Studio, the output stays muted until the lookahead line has refilled (1 ms + 16 samples), then fades in over 10 ms from the delayed audio. The first version faded in at once over the still-empty line, which made a step on Live to Studio. Into Live, it fades in at once.

The processor reports the new latency from the block after the swap. Host bypass and the SYNTH bus delay follow the reported latency.

## Measurements (`ChimeraTruePeakLimiterTests`)

**Ceiling:** the worst output true peak is −0.0052 dB relative to the ceiling, measured with the loudness meter over the whole output from silence. The test requires it never to exceed the ceiling. The sweep covers:
- 3 releases: 5 ms (the minimum), 20 and 50 ms.
- 2 drives: the programmes as written, and 12 dB hotter.
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

**Live:** with Live selected, `CommonDynamics` is compared bit for bit with the released limiter algorithm, kept in the test as a reference, over 122,880 samples. The run covers 5 ceilings, 5 and 180 ms releases, and switching off mid-release.

**Processor:**
- Latency at 48 kHz is 42 samples with Live and 106 with Studio (+64).
- With +12 dB output gain and a −3 dBTP ceiling, the final output read −3.010 dBTP on the processor's own meter.
- **Switching, on a pure 220 Hz sine through the limiter:** the largest sample step in both directions equals the sine's own, 0.0173. Before the refill wait, Live to Studio gave a step of 0.060.

**Cost:** 0.4% of one core for the stereo Studio limiter at 48 kHz, on 10 s of loud noise. It allocates nothing while processing.

## Limits

- The lookahead is fixed at 1 ms. There is no user lookahead or attack control, and no oversampled gain stage: the gain is computed and applied at the host rate.
- True peak is the meter's estimate: 4× (2× at 160 kHz and above), as BS.1770-4 asks. A downstream converter with a different reconstruction filter may differ by a few hundredths of a dB.
- The **A/B level match** runs after the limiter and only cuts, so it cannot raise a peak.
- The **SYNTH bus** is not limited. It is delayed by the reported latency to stay aligned with the main output.
- A mode change dips the output for about 20 ms, plus about 1.3 ms into Studio. The SYNTH bus delay follows the latency at the swap, so it can step at that moment.
