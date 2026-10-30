# 1.10 tracking verification

Status: **prepared on `claude/tracking-1-10`**, stacked on
`claude/six-band-1-5`.
- The roadmap's 1.10 "integrate qualified tracking work" needs evidence from
  recorded instruments. That evidence is still open.
- This branch provides the measurement and a synthetic corpus, and fixes
  what the corpus found:
  - a pitch bias on high notes;
  - double attacks on fast-locking notes.
- A recording can be checked as soon as it exists. The commands are in
  **Checking a recording**.

Covered by `ChimeraTrackingTests` (synthetic corpus) and `ChimeraSynthTests`
(tracker).

## What is measured

The SYNTH layer's own tracker and guitar gate run over the audio (one frame
per 64 samples). For each labelled note:

| Measure | Definition |
|---|---|
| Lock | First frame within ±50 cents of the label with clarity above 0.8, after the note start |
| Gate | First frame with the guitar gate open, after the note start |
| Lag, median and 95 % cents | Pitch error after the lock until 10 ms before the end, against the label delayed by the lag (0–40 ms) that fits best. Steady notes come out at 0 lag; bends and vibrato show how late the tracker follows |
| Octave | Frames off by more than 600 cents |
| Dropout | Frames with the gate shut while the level is still above SENSITIVITY. A note that has decayed below it may close |
| Attacks | New guitar-voice starts from 5 ms before the note to its end. A picked note should have exactly one; a hammer-on or pull-off at most one |
| False triggers | Gate open outside every note and its 120 ms release |

**Targets** for notes in the tracker's range (55–1400 Hz):
- lock within 60 ms;
- median error 10 cents or less, at a lag of 25 ms or less;
- octave errors 2 % or less, dropouts 5 % or less;
- the attack counts above;
- at most 50 ms of false triggers per recording.

Notes outside the range, and chords, are measured and reported as "out of
scope".

## Synthetic corpus

`Tools/Tracking/SyntheticInstrument.h` renders the corpus. It is not a guitar
model. It is an additive string whose instantaneous frequency is known at
every sample, so bends and vibrato have exact labels. The string has:
- inharmonic partials (B = 0.00012), a pluck-position comb and per-partial decay;
- a 4 ms pick-noise burst;
- 50 Hz hum at −62 dBFS and hiss at −78 dBFS under everything.

All 24 notes in range meet the targets:

| Recording | Lock ms | Lag ms | Median cents | Attacks |
|---|---|---|---|---|
| B1 (7-string low B), 61.7 Hz | 34.7 | 0 | 7.6 | 1 |
| A1 (drop A), 55 Hz | 40.0 | 0 | 7.2 | 1 |
| E2 (open low E) | 29.3 | 0 | 6.4 | 1 |
| E4 (open high e) | 13.3 | 0 | 1.2 | 1 |
| E6 (24th fret), 1319 Hz | 2.7 | 0 | 0.4 | 1 |
| G3 bend +2 semitones over 150 ms | 18.7 | 17 | 2.2 | 1 |
| E4 vibrato ±30 cents at 6 Hz | 13.3 | 18 | 1.5 | 1 |
| A3 picked, B3 / C#4 hammer-ons, B3 pull-off | 18.7–26.0 | 0 | 2.1–3.4 | 1 each (the gate stays open) |
| E2 palm-muted 16ths at 120 BPM (90 ms) | 30.0–34.7 | 0 | 2.9–3.6 | 1 each |
| 16ths at 160 BPM (94 ms), A2 to E3 | 27.7–35.7 | 0 | 5.5–8.8 | 1 each |
| A-string 12th-fret harmonic (220 Hz, even partials only) | 18.7 | 0 | 6.3 | 1 |

No note has octave errors or dropouts. Hum and hiss alone open no gate.

- **Out of scope:** E1 (41.2 Hz, bass open E) and B0 (30.9 Hz, five-string
  bass low B) are below the 55 Hz range and are not tracked. An E2 + B2
  power chord is not tracked either (the mono tracker finds no clear period).
- **Inharmonicity:**
  - The low notes read 6–9 cents sharp. That comes from the string's
    inharmonicity: the stretched upper partials shorten the period both
    trackers find.
  - With B = 0 the same notes read within 0.1–0.2 cents.
  - Real wound strings are inharmonic too, so recorded low notes may show the
    same bias. Whether the synth should follow the fundamental more closely is
    a listening question.

## Tracker changes found by the corpus

- **High notes:**
  - At the 12 kHz analysis rate, E6 has a period of about 9 lags, and the
    parabola over whole lags read it 10.8 cents sharp.
  - For periods under 48 lags (notes above about 250 Hz), the tracker now
    refines the minimum over ±0.25 lags of the raw difference, with
    Hermite-interpolated delays.
  - E6 is now within 0.4 cents, and the existing tracker test improved to a
    worst median of 0.28 cents.
- **Double attacks:**
  - A high note locks while its attack is still rising (E6 locked at 2.7 ms).
    The next frame's level then passed twice the onset level and counted as a
    second pick.
  - A new attack is now ignored for 25 ms after the last one, while the level
    still rises.

## Checking a recording

Record a clean DI:
- 48 kHz, 24-bit, mono;
- no effects;
- peaks between −12 and −6 dBFS;
- at least half a second of silence between phrases.

Useful takes, matching the corpus:
- each open string held;
- the lowest note of the instrument (7-string B, drop tunings, bass E and B);
- a whole-tone bend and slow and fast vibrato on the G and B strings;
- hammer-ons and pull-offs;
- palm-muted 16ths at 120 BPM;
- alternate-picked 16ths at 160 BPM;
- 12th-fret natural harmonics;
- high notes above the 15th fret;
- a power chord;
- a few seconds with the instrument connected and silent (hum and hiss).

```
ChimeraTrackingCheck --labels take1.csv take1.wav     # with a label file
ChimeraTrackingCheck take2.wav take3.wav              # labels from the offline reference
ChimeraTrackingCheck --sensitivity -46 take4.wav      # the SYNTH page's SENSITIVITY
ChimeraTrackingCheck --frames frames.csv take1.wav    # every frame, for a closer look
ChimeraTrackingCheck --synthetic --write corpus/      # the corpus as WAV + CSV files
```

- **Label file:** one note per line, as `start_s,end_s,pitch[,name]`.
  - The pitch is in Hz (written with a decimal point, or above 127) or a MIDI
    note number.
  - Lines starting with `#` are comments.
  - `--synthetic --write` produces examples.
- **Offline reference:** used for a recording without a label file. It runs
  YIN at a 24 kHz analysis rate, with a 42 ms window and a non-causal median.
  - It cuts the recording into notes where the pitch holds within 40 cents for
    40 ms. On the corpus it found all 22 steady notes.
  - It labels steady notes only, not bends or vibrato.
  - It shares the inharmonicity bias, so it is a cross-check, not ground
    truth.
- **Report:** a Markdown table, as above, and the targets.

## Not done

- **Recorded instruments:** guitar and bass recordings, run through
  `ChimeraTrackingCheck`, with listening. This is the remaining 1.10 gate.
- **Range:** notes below 55 Hz (bass E1 and B0) need a longer analysis window
  or a different method. Polyphonic tracking stays out of scope.
