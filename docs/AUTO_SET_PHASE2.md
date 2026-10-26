# Auto Set — phase 2 (Tone EQ)

Phase 2 adds a Tone EQ step to Auto Set (phase 1: `AUTO_SET_PHASE1.md`). It
listens to the amp/cab output and moves the Tone EQ toward a target tone
balance for guitar or bass. It is rule-based like phase 1.

## Use

Dynamics → **COMPRESSOR** or **GATE / EXPANDER**. The options button next to
**AUTO SET** (it shows e.g. `Auto / Rock`) chooses:

| Option | Effect |
|---|---|
| Instrument: Detect from playing (default) / Guitar / Bass | Target used by the Tone EQ (and de-esser) |
| Style: Rock (default) / Clean / Metal | Shifts the targets (phase 3, `AUTO_SET_PHASE3.md`) |
| Steps: Tone EQ | Off = phase 1 behaviour for the tone: input, gate and compressor only |

With the Tone EQ step, Auto Set listens in about 6 s per pass. Play across
the neck in the Tone EQ pass so every register is heard. **UNDO AUTO**
restores everything Auto Set changed.

## Order

The passes follow the signal path, because each stage feeds the next:

1. **Input + gate** at the input tap.
2. **Tone EQ** from the amp/cab output *before* the Tone EQ, at the new input
   gain (first 300 ms discarded). Measuring before the Tone EQ means existing
   Tone EQ moves are not part of the reading.
3. **Compressor** after the new Tone EQ.

The audio thread copies the mono (channel average) signal into a lock-free
ring while pass 2 listens; allocation-free, idle otherwise. The FFT and fit
run on the message thread.

## Rules (`analyseTone`)

| Step | Rule |
|---|---|
| Spectrum | Hann FFT (~6 Hz bins, 50 % overlap), playing frames only (within 15 dB of the 90th-percentile frame level), power-averaged into 24 third-octave bands, 50 Hz–10 kHz |
| Instrument (auto) | Bass when 35–71 Hz holds more than −12 dB relative to 71 Hz–1.4 kHz (below a guitar's low E, where guitar cabs also roll off) |
| Target | Instrument curve aligned to the measurement over 250 Hz–1 kHz |
| Ignored bands | More than 45 dB under the strongest band, or more than 20 dB short of the target (no content to bring up) |
| Correction | 70 % of the difference, one-octave smoothed, ±9 dB |
| Moves | Low shelf 80 Hz, bells at 160 / 315 / 630 / 1250 / 2500 Hz (Q 1), high shelf 5 kHz; least-squares fit on the real filter responses; ±6 dB, 0.5 dB steps, moves under 1 dB dropped |
| Applied | Replaces the static Tone EQ curve and turns the Tone EQ on; unused bands off. Bands assigned to a Dynamic EQ slot keep the user's node. No moves → Tone EQ untouched |

Target curves (third-octave level relative to 250 Hz–1 kHz):

| Hz | 50 | 63 | 100 | 160 | 250 | 500 | 1k | 2k | 3.15k | 5k | 8k |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Guitar | −16 | −12 | −5 | −1 | 0 | −0.5 | −1 | −3 | −5 | −10 | −18 |
| Bass | −2 | 0 | 0 | −1 | −3 | −6 | −10 | −15 | −20 | −25 | −32 |

## Verification

`ChimeraAutoSetTests`:
- Silence is rejected.
- Noise shaped exactly like each target is left alone (≤ 1 move).
- The same noise 8 dB darker or brighter above 3 kHz is corrected in the right
  direction, with the fit error reduced and every move within ±6 dB in 0.5 dB
  steps, at 44.1/48/96 kHz.
- Guitar and bass notes are detected correctly, and a chosen instrument
  overrides detection.
- The mono tone tap is allocation-free.
- A three-pass run on the real processor: host gestures balanced, the
  Dynamic EQ node left untouched, undo, and the optional steps off giving two
  passes.

`CommonDynamicsUITests` checks the options' placement and defaults, and that
listening and cancelling write no parameters.

## Not yet validated

The target curves, the 70 % strength and the bass threshold are starting
values, not tuned against reference recordings or listening tests.

- Detection can mistake a bass played only above the open D string for
  guitar, and a drop-tuned guitar through a full-range cab for bass. Choose
  the instrument in the options then.
