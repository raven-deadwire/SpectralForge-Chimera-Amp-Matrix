# SYNTH layer — 1.10

Status: **implemented on `claude/synth-layer`**, stacked on
`claude/creative-fx`. Roadmap 1.10: Dvergr Analog, Seidr Dynamic and Mimir
Impact. Covered by `ChimeraSynthTests`, with the inventory, native-state and
signal-path tests updated.

Owner's decisions (2026-10-29):
- the guitar (mono tracking) and host MIDI both play the layer;
- it is a parallel layer after RIG SUM;
- the three layers are as proposed.

## Where it sits

- **Input:** the pitch tracker reads the clean input after the input gain, at
  the same point as the precision tuner. It is ahead of PRE, the amps and the
  gates.
- **Output:**
  - The synth voice is added after RIG SUM and the post-rig gates, before
    Tone EQ.
  - It passes Tone EQ, the common dynamics, POST and Final EQ, but no amp or
    cabinet.
  - The guitar's gate does not cut its release.
- **Signal path:** a SYNTH node with a dashed detector edge from INPUT and an
  edge into TONE EQ. Selecting it focuses the SYNTH row's layer selector; ALL
  opens the full panel.
- **Bug found by the UI test:** the node first opened the panel directly, and
  the dialog took the next pointer click from the routing graph.
- **Off (the default):** the tracker does not run and nothing is added.
- **Latency:** the voice is not delayed to match the rig's latency. Tracking
  adds its own lock time (below); the two have not been compared in a host.

## Playing it

- **Source:** `synthSource` is one of:
  - Guitar + MIDI (default): a held MIDI note wins, otherwise the guitar.
  - Guitar only.
  - MIDI only.
- **MIDI:**
  - Monophonic, last-note priority, with velocity.
  - Pitch bend ±2 semitones.
  - Notes are applied at the start of each host block, not at their sample
    offset.
- **Guitar tracking:** monophonic. The voice follows the tracked pitch
  continuously, so bends and vibrato carry over.
  - **Algorithm:** YIN (cumulative mean normalised difference, threshold 0.15),
    with parabolic interpolation.
  - **Rate:** the input is decimated to about 12 kHz behind two biquad
    low-passes. The tracker runs every 64 decimated samples (5.3 ms).
  - **Range:** 55–1400 Hz. A seven-string low B is 61.7 Hz.
  - **Smoothing:** a median of three estimates rejects single-frame octave
    slips.
  - **Note on:** while the tracked level is above THRESHOLD and the clarity is
    above 0.8.
  - **Onset:** a rise of 6 dB over the recent level is a new pick attack.
- **Not supported:** polyphonic tracking (the roadmap leaves it undated).

## Parameters (version hint 27, appended after CREATIVE)

All eleven are automatable.

| ID | Range | Default |
|---|---|---|
| `synthOn` | off / on | off |
| `synthModel` | Dvergr Analog / Seidr Dynamic / Mimir Impact | Dvergr Analog |
| `synthSource` | Guitar + MIDI / Guitar / MIDI | Guitar + MIDI |
| `synthLevel` | −36…+6 dB | −6 dB |
| `synthOctave` | −2…+1 | 0 |
| `synthTone` | 0–100 % (CUTOFF / BRIGHTNESS / STRIKE) | 50 % |
| `synthCharacter` | 0–100 % (DETUNE / DYNAMICS / RATIO) | 50 % |
| `synthAttack` | 1–2000 ms | 10 ms |
| `synthRelease` | 10–4000 ms (60 dB decay) | 300 ms |
| `synthGlide` | 0–500 ms | 0 |
| `synthSensitivity` | −60…−12 dBFS (tracking threshold) | −40 dBFS |

## The layers

The names are Chimera working names with no hardware reference.

| Layer | Sound | TONE | CHARACTER |
|---|---|---|---|
| Dvergr Analog | Two polyBLEP saws plus a sub-octave square into a 24 dB low-pass (two SVFs) that opens with the envelope | Cutoff 150 Hz–12 kHz | Detune 0–30 cents |
| Seidr Dynamic | Pulse with slow PWM, plus a slightly detuned saw. Brightness and level follow the playing level continuously | Base brightness 200 Hz–4 kHz | How far the playing level opens the filter, 0–4 octaves |
| Mimir Impact | Two-operator FM strike: attack, then a 60 dB decay over Release even while held. The FM index decays four times faster, plus a 5 ms noise transient | Peak FM index 0–8 and transient level | Modulator ratio 0.5 / 1 / 2 / 3 / 3.5 / 4 / 5 / 7 |

## Measurements (`ChimeraSynthTests`, 48 kHz)

**Tracker.** The test signals:
- sines;
- a synthetic pluck: ten harmonics at 1/k, each decaying faster than the last;
- the same pluck with its fundamental at 20 %.

| Note | Lock time (sine / pluck / weak fundamental) | Median error |
|---|---|---|
| B1 61.7 Hz | 32 / 32 / 37 ms | 0.01–0.28 cents |
| E2 82.4 Hz | 27 / 27 / 27 ms | ≤ 0.08 cents |
| A2 110 Hz | 27 / 21 / 21 ms | ≤ 0.09 cents |
| G3 196 Hz | 21 ms | ≤ 0.25 cents |
| E4 330 Hz | 16 ms | ≤ 0.47 cents |
| A4 440 Hz | 11 ms | ≤ 1.26 cents |
| E5 659 Hz | 11 ms | ≤ 3.07 cents |

- **Octave errors:** none in any frame after 0.2 s, on any of the 21 signals.
- **Error and pitch:** the error grows with pitch, because the period is
  interpolated at the decimated rate.
- **Lock time:** measured to the first estimate within 20 cents, from the start
  of the note, in 5.3 ms steps.
- **Signals:** these are synthetic. Recorded guitar and bass, including
  palm-muted and low-B notes, remain to be measured.

**Playing.**
- **Threshold:** at −40 dBFS, a −50 dBFS input stays silent and a −20 dBFS input
  plays.
- **MIDI:** A3 plays at 220.0 Hz. Octave −1 gives 110.0 Hz, and bend +2 gives
  246.9 Hz.

**Layers.**
- **Dvergr Analog:**
  - The sub octave is at −7.2 dB relative to the note.
  - Harmonics from 3 to 9 kHz fall by 74 dB from Cutoff 100 % to 0 %.
- **Seidr Dynamic:** raising the input from −30 to −12 dBFS moves the spectral
  centroid from 275 to 413 Hz and the level from −45.0 to −22.7 dBFS.
- **Mimir Impact:**
  - T60 is 0.50 s at Release 0.5 s.
  - The 660 Hz FM sideband falls from −13 dB (first 30 ms) to −80 dB
    (150–250 ms) relative to the carrier.

**Contract.**
- **Off:** the buffer is untouched and the tracker does not run.
- **Switching off while sounding:** the layer releases and is idle after 165 ms
  at Release 100 ms (100 dB at 60 dB per 100 ms).
- **Extremes:** with all three layers, octave +1, maximum tone and character and
  a loud input, the output peaks at 2.5 and stays finite.
- **Allocation:** none in the callback.

**Processor and editor.**
- **Processor:** with a silent input and MIDI A3, `ChimeraProcessor` outputs
  −20.7 dBFS at 220 Hz with the layer on, and nothing with it off.
- **UI:**
  - The SYNTH row on the HEAD page (beside "ONE FULL-RANGE RIG") and the ALL
    panel are attached to their parameters.
  - The TONE / CHARACTER labels follow the layer.
  - The panel shows the tracked note.
- **Signal path:** the SYNTH node feeds TONE EQ.

## Not done

- **Tracking evidence:** recorded guitar/bass at low B, palm mutes and fast
  passages. The roadmap's tracking research gate is still open.
- **Timing:** sample-accurate MIDI timing, and latency alignment with the rig.
- **Range:** polyphonic tracking, and a mod wheel or aftertouch.
- **Panel:** rack or panel artwork for the layers.
- **Checks:** listening tests, and CPU measurement in a DAW.
