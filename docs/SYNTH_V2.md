# SYNTH v2: three engine types, three oscillators, samples

Status: **implemented on `claude/synth-three-osc`**, stacked on
`claude/synth-page`. It extends the SYNTH page and engine
(`docs/SYNTH_PAGE.md`). The synth has not been released, so its parameters
are redesigned at version hint 27 rather than appended. Covered by
`ChimeraSynthTests`.

## Owner's decisions (2026-10-29)

- **Wave selection:** changing a wave should be more direct.
- **Oscillators:** three oscillators, as in Serum 2.
- **Samples:** oscillators can play loaded audio.
- **Three types:** the 1.10 synth was to have three kinds of synth. They
  return as three **engine types** — Dvergr Analog, Seidr Dynamic and Mimir
  Impact. Each has its own DSP character on a shared voice with three
  oscillators, a filter, a modulation matrix and samples. They are no longer
  only factory patches.
- **Samples:** each oscillator has a Sample mode. A file is loaded by drag and
  drop or with LOAD, and its root note is detected on load. Start position and
  loop are settable, and the sample is saved in the project.

## Engine types

The header selects the type and its **CHARACTER** amount (0–100 %, default
50 %). At 0 % all three types are the plain voice, sample for sample (the
test compares them). The CHARACTER amount is also a modulation target.

| Type | What CHARACTER does (at 100 %) |
|---|---|
| **Dvergr Analog** | Each oscillator drifts slowly in pitch: low-passed noise at 1.5 Hz, about 5 cents RMS. Gentle saturation (`tanh(g·x)/g`, g = 0.5) before the filter, which is unity for small signals: a full-scale peak loses 0.4 dB. |
| **Seidr Dynamic** | The playing level drives three things: the guitar's tracked level, or the MIDI velocity. The filter cutoff moves up to ±1.5 octaves around its setting (3 octaves of range), the waves move up to 1.5 positions towards square / pulse, and the level spans 12 dB (×0.25 at silence). |
| **Mimir Impact** | Each attack strikes an FM index of up to 6 into oscillator A, decaying over 60 ms, plus a 4 ms noise click. The modulator is oscillator B, or an internal sine at twice A's pitch when B is off or in Sample mode. |

- The filter curve on the page draws the set cutoff. Seidr's level-driven
  offset and Dvergr's saturation are not drawn.
- Factory patches, one per type: Dvergr Analog, Seidr Dynamic and Mimir
  Impact. Each sets its type and a character of 60–70 %.

## Oscillators A, B and C

- **Shared controls:** each oscillator has on, mode (Wave / Sample), octave,
  semitone, fine, unison (1–7) with detune, and level.
  - Wave mode adds the wave position (MORPH).
  - Sample mode adds START, LOOP and ROOT.
- **FM:** B modulates A.
- **Matrix targets:** Pitch, Wave and Level of each oscillator; FM; cutoff;
  resonance; sub and noise level; amp; detune; character.
  - That is 17 targets; the Wave targets do nothing in Sample mode.
- **Parameters:** 100 automatable parameters, up from 78.
  - Global: 12.
  - Per oscillator: 12 × 3.
  - Sub 4, noise 3, filter 7.
  - Envelopes 8, LFOs 6.
  - Matrix 24.

## Choosing a wave

- **Shape strip:** sine, triangle, saw, square and pulse as tiles under the
  display. A click selects that shape, and a marker shows the current morph
  position.
- **Display:** the strip doubles as a ruler. Clicking or dragging across the
  display morphs to the shape under the pointer, because each shape sits above
  its tile. The display names the shape, or the blend ("Saw > Square 40 %").
- **Fine control:** scroll on the display for 0.05 steps. Double-click snaps
  to the nearest shape. The MORPH knob remains.
- **Gestures:** every click, drag or scroll is one host gesture.

## Samples

- **Loading:**
  - Drop an audio file on an oscillator's display, or press LOAD. WAV, AIFF,
    FLAC, OGG and the platform's other formats are read.
  - Up to 20 s are kept, and more than two channels are folded into two.
  - Loading switches the oscillator on in Sample mode.
- **Root:** the root note is set from the pitch the synth's own tracker finds
  in the sample: the median of its confident estimates. The strip shows the
  found frequency. When no pitch is found, the root is C4. ROOT can be changed
  by hand, and patches never change it.
- **Playback:**
  - The root note plays at the sample's own speed, so a 44.1 kHz sample in a
    48 kHz session stays in tune.
  - Every attack starts at START. LOOP repeats from START to the end; without
    LOOP, the sample plays once and stops.
  - Reading uses four-point Hermite interpolation.
  - There is no anti-aliasing for large upward transpositions; the speed is
    capped at 16×.
  - Unison voices read the same sample at detuned speeds.
  - A stereo sample keeps its channels.
  - In Sample mode with no sample loaded, the oscillator is silent.
- **Threads:** samples are built and replaced on the message thread. The audio
  thread only tries the lock, so a block that meets a swap in progress plays
  the sample oscillators silent. There are no allocations in the audio
  callback.
- **Project:** samples are saved as 24-bit FLAC in the plugin state and
  restored with it.
  - They belong to the project, not to an A/B slot.
  - A project without samples clears them.
  - X removes a sample and returns the oscillator to Wave mode.

## Page

- **Header:**
  - SYNTH ON.
  - The type buttons and the CHARACTER bar, whose tooltip describes the
    selected type.
  - SOURCE, INSERT, PATCH, VOICES and OCTAVE.
- **Top row:** OSC A, OSC B, OSC C, FILTER.
- **Bottom row:** ENV 1, ENV 2, LFO 1 / 2, MOD MATRIX, SUB / NOISE, and
  SCOPE / MASTER (level, glide, width, FM, tracking threshold).
- **Knobs:** each has its name above and its value below. Double-click the
  value to type one.
- **Evidence:** `SYNTH_PAGE_SNAPSHOT=<dir>` makes `ChimeraSynthTests` write
  the page at 100 % and 150 % with a sample in OSC B.

## Measurements (`ChimeraSynthTests`, 48 kHz)

- **Earlier tests:** all of `docs/SYNTH_PAGE.md`'s measurements still hold,
  using the plain voice (character 0). The values are unchanged.
- **Oscillator C:**
  - +7 semitones: 329.665 Hz (329.628 expected).
  - A / B / C at levels 100 / 50 / 25 %: −6.00 / −12.02 / −18.04 dBFS.
  - Level C at −100 % through the matrix: below −190 dBFS.
- **Engine types:**

  | Check | Measured |
  |---|---|
  | Character 0, all three types | Identical output |
  | Dvergr drift at 100 % | 4.3 cents RMS over 4 s |
  | Dvergr, a quiet voice (5 % level) | Level change 0.0002 dB |
  | Seidr, velocity 0.2 → 1.0 | Level −20.6 → −10.2 dBFS; harmonic 15 re the fundamental −51.9 → −27.4 dB |
  | Seidr, guitar −30 → −12 dBFS | Harmonic 15 −55.5 → −34.7 dB |
  | Mimir, 660 Hz sideband | −1.1 dB in the first 20 ms, −90 dB after 0.5 s |
  | Mimir, click | −20.4 dBFS in the first 5 ms, silent after 100 ms |

- **Samples:**

  | Check | Measured |
  |---|---|
  | Detected roots | C4 (261.7 Hz) and G3 (196.0 Hz) |
  | A C4 sample (44.1 kHz) played at A4 | 440.2 Hz |
  | Loop | Still playing at 1.5–2 s from a 1 s sample (−17.2 dBFS) |
  | One-shot, +1 octave | Ends at 0.500 s (0.5 expected) |
  | One-shot from START 50 % | Ends at 0.250 s |
  | Stereo | Left and right stay separate (below −300 dB crosstalk) |
  | Empty slot | Silent |

- **Project state:** a 0.5 s stereo 24-bit sample round-trips exactly. The
  state is 649 KiB, most of it the rest of the plugin. A project without
  samples clears them, and X clears.
- **Full load:**
  - Every patch: 8 voices × 3 oscillators × 7 unison, all slots, character,
    drive and resonance at 100 %.
  - Peak 2.0, the safety bound, with no allocations.
- **Factory patches at A3:** Dvergr Analog −10.2, Seidr Dynamic −7.5 and
  Mimir Impact −24.2 dBFS.
- **Page:**
  - The square tile sets the wave to exactly 3.
  - A display drag ending above the saw tile lands on 2.00.
  - A type button is one gesture.
  - Sample mode shows LOAD, LOOP, ROOT and START.
  - The Mimir Impact patch sets its type, returns oscillators to Wave mode and
    keeps the sample root.

Fixes made while testing:
- Dvergr's first saturation (g up to 3) lowered its patch by 7 dB at normal
  levels. It is now g = 0.5 at 100 %.
- Seidr's waves move towards square, so the brightness test reads an odd
  harmonic; the 16th vanishes there.

## Artwork

Each engine type has its own strip behind the SYNTH page's top bar,
dimmed under the controls. The type selector names the engine.

| Type | Strip | Colours |
|---|---|---|
| Dvergr Analog | Rivets and glowing vent slots, like a warm valve chassis | Dark bronze, amber |
| Seidr Dynamic | Interlaced waves (a sound that follows the playing) | Slate blue, ice blue |
| Mimir Impact | Concentric rings struck from points (a struck sound) | Indigo, lilac |

- **Source:** `Tools/generate_post_rack_faces.py` draws them (`sdvergr`,
  `sseidr`, `smimir`, 2800 × 56).
- **Name end:** the strips also carry the engine name at their right end
  (`art::synthStrip(..., named=true)`). The page leaves it out: at the page
  width of 1140 px, the top-bar controls cover it.
- **Test:** `ChimeraUITests` checks that each type has its own embedded strip.

## Not done

- **Wavetables:** wavetable import (cutting a sample into single cycles) and
  multi-sample key zones.
- **Sample oscillators:** anti-aliased transposition, a loop end separate
  from the sample end, and crossfaded loops.
- **Modulation:** per-voice LFOs; Sample-mode modulation of START.
- **Checks:** recorded-instrument tracking, listening tests and DAW CPU.
