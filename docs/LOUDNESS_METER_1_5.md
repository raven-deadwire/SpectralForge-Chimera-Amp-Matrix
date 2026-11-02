# Studio loudness and true-peak meter — 1.5 candidate

Status: **implemented on `claude/loudness-meter`** (based on `main`).
Roadmap: "Target a Studio true-peak/LUFS candidate in 1.5, independently of
Live limiter acceptance." Covered by `ChimeraLoudnessMeterTests`.

This is a **meter**. It never changes the audio, adds no parameter and no
latency. The limiter's Live mode remains a sample-peak ceiling without
lookahead or a true-peak claim; its Studio mode holds the true peak this meter
reads ([TRUE_PEAK_LIMITER_1_5.md](TRUE_PEAK_LIMITER_1_5.md)).

## What it measures

`Source/LoudnessMeter.h` follows ITU-R BS.1770-4 and the EBU Tech 3341
minimum requirements.

- **K-weighting:**
  - The two BS.1770 stages (high shelf, then RLB high-pass), with coefficients derived from their analogue prototypes for any sample rate.
  - At 48 kHz they equal the standard's tables (largest difference 9e−16).
- **Momentary:** 400 ms. **Short-term:** 3 s. Both update every 100 ms.
- **Integrated:**
  - Gated over 400 ms blocks every 100 ms: an absolute gate at −70 LUFS, then a relative gate 10 LU below the mean of the blocks above it.
  - Blocks are kept in a fixed histogram (0.01 LU bins from −70 to +10 LUFS), so memory does not grow with the measuring time.
- **Loudness range (EBU Tech 3342):**
  - Built from the short-term values, one every 100 ms once 3 s are available.
  - Values above −70 LUFS, then those within 20 LU of their power mean, count.
  - LRA is their 95th minus 10th percentile, kept in a fixed histogram like the integrated value.
- **True peak:**
  - The maximum of the samples and of 4× polyphase interpolation (2× from 176.4 kHz).
  - The interpolator is a 64-tap Hann-windowed sinc (16 taps per phase); one of its four phases is the sample itself.
  - Readings are in dBTP.
- **Channels:**
  - Up to two, each weighted 1.0 (BS.1770 L/R).
  - A mono buffer is measured as one channel, so a −23 dBFS mono tone reads −26.0 LUFS.

The meter reads exactly what leaves the plugin: after the limiter, the output
trim and the lifecycle gain (`ChimeraProcessor::processBlock`).

**Reset** clears the readings and keeps the filter and interpolator state. A
reset during a programme therefore adds no step to the signal it sees.

## Display

The OUTPUT panel shows one line: `I −23.0 LUFS  TP −1.0 dBTP`.

- The tooltip adds the momentary and short-term values and the loudness range.
- Clicking the line resets the meter.
- `--.-` means nothing has been measured since the reset.

## Measurements (`ChimeraLoudnessMeterTests`)

- **K-weighting:** coefficients at 48 kHz match BS.1770-4 Tables 1 and 2 (9e−16).
- **EBU Tech 3341 cases 1–5** (stereo 1 kHz tones; levels, steps and gates as specified):
  - At 44.1 kHz, the largest error is 0.024 LU.
  - At 48 kHz, it is 0.021 LU.
  - At 96 kHz, it is 0.031 LU.
  - The requirement is 0.1 LU. Cases 1 and 2 also check momentary and short-term.
- **EBU Tech 3342 cases 1–4** (−20/−30, −20/−15, −40/−20 and −50/−35/−20/−35/−50 dBFS steps):
  - LRA within 4e−6 LU of 10, 5, 20 and 15 LU at 44.1/48/96 kHz (requirement ±1 LU).
  - The values are this exact because the 10th and 95th percentiles fall on the steady tone levels themselves.
  - A steady tone reads 0 LU.
- **True peak, steady sines at −6 dBTP:**
  - Test set: 997 Hz to 15 kHz and fs/4, three start phases, at 44.1/48/96 kHz.
  - The largest error is 0.060 dB.
  - fs/4 at 45° reads 0.011 dB from the true peak; its samples alone sit 3.01 dB below it.
  - 15 kHz at 44.1 kHz over 48 start phases reads 0 to +0.006 dB.
  - EBU allows +0.2/−0.4 dB.
  - A single cycle could read up to 0.31 dB short at 15 kHz / 44.1 kHz, the 4× point-spacing limit. This was not measured.
- **Gating and reset:**
  - A −90 dBFS passage after −60 dBFS moves the momentary value but not the integrated one (absolute gate).
  - Reset clears the readings, and the meter measures normally afterwards.
- **No allocation:** none while metering, including across a reset.

## Development notes

- **Reset step:** a first version reset the interpolator history with the readings. The resulting step read +0.39 dB too high on a 15 kHz tone.
- **Interpolation phases:** a half-sample-centred interpolator put its 2× phases at 0.25/0.75 of a sample. It read −0.69 dB on fs/4 at 96 kHz. That is why the centre is now an integer and 4× is used up to 96 kHz.

## Not done

- EBU 3342 cases 5 and 6 (programme files, not available here) and the remaining EBU 3341 cases: other channel layouts, the burst tests for momentary and short-term, and the published true-peak test files (not available here).
- A selectable target or offset (e.g. −14/−16/−23), a history graph, and true-peak per channel.
- Comparison against an external reference meter on programme material.
