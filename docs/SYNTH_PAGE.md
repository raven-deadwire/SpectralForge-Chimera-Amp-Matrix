# SYNTH page and engine

Status: **implemented on `claude/synth-page`**, stacked on
`claude/creative-art` (the integration branch). This replaces the 1.10
three-layer synth (`docs/SYNTH_1_10.md`). That synth was not released, so its
eleven parameters were redesigned. The three layers survive as factory
patches. Covered by `ChimeraSynthTests` and the signal-path UI tests.

> **Extended by `docs/SYNTH_V2.md`:** three engine types (Dvergr Analog,
> Seidr Dynamic, Mimir Impact), a third oscillator, sample oscillators and
> direct wave picking. The parameter count there is 100. The two-oscillator
> layout described below is superseded.

## Owner's decisions (2026-10-29)

- **Page:** a dedicated SYNTH page in the editor's stage tabs, in the manner of
  Serum or Omnisphere rather than a dialog.
- **Engine:** a Serum-style subtractive engine:
  - two oscillators with unison, plus a sub oscillator and noise;
  - a multi-mode filter;
  - two ADSRs and two LFOs;
  - a modulation matrix;
  - polyphonic MIDI, while the guitar plays one voice.
- **Visuals:** oscillator wave displays, a filter response curve, ADSR and LFO
  graphs, and a tracking / output scope.
- **Insert point:** four points, after a survey of how other guitar synths are
  placed. See "Where it joins the signal".

## Where it joins the signal

The tracker always reads the clean input after the input gain. Where the synth
output joins the guitar is set by `synthInsert`:

| Point | Processing after it | Typical use |
|---|---|---|
| Amp input | The amps and cabinets (wet path; Matrix LOW keeps its clean DI), then everything else | Like a synth pedal in front of an amp |
| Before Tone EQ | Tone EQ, the common dynamics, POST, Final EQ | Blended with the guitar as one sound |
| **Before POST (default)** | The POST rack (dynamics, EQ, MOD, DELAY, CREATIVE, REVERB), Final EQ, limiter | Keeps the synth's own tone and shares the effects |
| After POST | Doubler / click, Final EQ, limiter | A dry synth beside the guitar |

- **Rendering:** the synth renders once per block and is added only at its
  point. Moving the point crossfades over 20 ms.
- **Signal path:** the SYNTH node's edge goes to RIG INPUT, TONE EQ, the first
  POST section or DOUBLER / CLICK.
- **Latency:** the synth is not delayed to match the rig.
  - The processor reports 42 samples of latency at the defaults, the same for
    every point (`ChimeraSynthTests`, 48 kHz).
  - At Amp input, the synth also takes the amps' share of that latency.
- **Separate output:** a synth-only output (like the SY-1000's sub outputs or
  the GR-55's Guitar Out) would need an extra host output bus. It is left for
  the external-sidechain bus work.

### What other products do (survey, 2026-10-29)

- **Boss SY-1000:** three instrument blocks beside the normal guitar signal.
  Their order can be changed and they can run in parallel; there are main and
  sub outputs.
- **Roland GR-55:** a choice of a parallel structure (synth and guitar
  processed separately) or a serial one (shared effects), plus a separate
  Guitar Out.
- **Boss SY-1 / SY-200:** usually placed first, for a clean tracking signal.
  Wet and dry levels are separate, and the SY-200 has a parallel loop.
- **Line 6 Helix / Fractal Axe-Fx:** the synth is a block placed anywhere,
  often on a parallel row.
  - Helix users report better tracking with a compressor in front of the
    synth, or with the synth after the amp/cab block. These are forum reports,
    not vendor guidance.
- **Jam Origin MIDI Guitar:** converts the guitar to MIDI for any synth plugin,
  separately from the guitar's processing.

Sources are listed in the session notes. Most are manuals, reviews or forum
posts rather than vendor specifications.

## Engine

- **Source and voices:**
  - Guitar + MIDI (a held MIDI note wins), Guitar, or MIDI.
  - MIDI: 1–8 voices. With 1 voice, the synth is mono with last-note priority
    and legato glide. With more, the oldest voice is stolen. Pitch bend is
    ±2 st; CC 1 is the mod wheel.
  - The guitar has a dedicated voice. Its pitch and level come from the 1.10
    YIN tracker (lock 11–37 ms, ≤ 3.1 cents; see the 1.10 document), and a
    6 dB rise re-triggers it.
- **Oscillators A and B:**
  - Wave position 0–4: sine, triangle, saw, square, 12.5 % pulse. Positions in
    between crossfade two neighbours. Saw and the pulses are polyBLEP
    band-limited.
  - Octave ±3, semitones ±12, fine ±100 ct.
  - Unison: 1–7 voices, detune up to ±50 ct, spread across the stereo width.
  - Level per oscillator.
  - FM: B modulates A's phase (index up to 8).
- **Sub and noise:**
  - Sub: −1 or −2 octaves, sine or band-limited square.
  - Noise: white through a 500 Hz–20 kHz colour low-pass.
- **Filter:**
  - Types: low-pass 12 / 24, high-pass 12 / 24, band-pass, notch.
  - Zero-delay-feedback SVFs; Q runs from 0.5 to 10.
  - Drive is tanh, normalised at full scale.
  - Key tracking, and an Env 2 amount of ±5 octaves.
- **Envelopes:** ENV 1 sets the amplitude; ENV 2 drives the filter or
  modulation. Attack is linear; decay and release cover 60 dB of their span
  in the set time.
- **LFOs:** sine, triangle, saw, square and sample & hold; free 0.02–30 Hz, or
  tempo-synced from 1/1 to 1/8 T. They are global (shared by all voices).
- **Modulation matrix:** eight slots of source → target × amount (±100 %).
  - Sources: Env 2, LFO 1 / 2, velocity, guitar level, note, mod wheel.
  - Targets and full-scale depths:

    | Target | Depth at 100 % |
    |---|---|
    | Pitch A / B | 24 st |
    | Wave A / B | 4 positions |
    | FM | index 8 |
    | Cutoff | 6 octaves |
    | Resonance | full range |
    | Levels (A, B, sub, noise), amp, detune | full range |

- **Master:** level, global octave, glide (a time constant), stereo width, FM
  amount, and the tracking threshold.
- **Safety stage:** linear up to 0 dBFS, then softly bounded to +6 dBFS. At
  full load (8 voices × 2 × 7 unison, every slot, drive and resonance at
  100 %), the test's peak sits at that bound.
- **Off (the default):** the synth adds nothing, the tracker does not run, and
  no point is fed.
- **Parameters:** 78 automatable parameters at version hint 27.
- **Patches:**
  - Init (the defaults), Dvergr Analog, Seidr Dynamic and Mimir Impact.
  - Loading a patch from the page writes every synth parameter, one host
    gesture each.
  - It never touches on / source / insert / level / threshold.

## Page

- **Location:** stage tab SYNTH, after FINAL EQ.
- **Top band:** ON, SOURCE, INSERT, PATCH, VOICES, OCTAVE, THRESHOLD.
- **Panels:**
  - OSC A and OSC B: wave display plus WAVE / OCT / SEMI / FINE / UNISON /
    DETUNE / LEVEL.
  - SUB / NOISE.
  - FILTER: type, response curve, CUTOFF / RES / DRIVE / KEY / ENV 2.
  - ENV 1 and ENV 2: graph plus A D S R.
  - LFO 1 / 2: shape, sync, rate, graph with a live position dot.
  - MOD MATRIX: eight rows.
  - SCOPE / MASTER: tracked note, clarity, sounding voices, input level and the
    output waveform, plus LEVEL / GLIDE / WIDTH / FM.
- **Filter curve:** drawn from `synthFilterMagnitude`, the same response the
  engine runs. Dragging sets cutoff (horizontal, log axis) and resonance
  (vertical).
- **Envelope graphs:** dragging the attack point sets attack; the decay /
  sustain point sets decay and sustain; the release end sets release.
- **Host gestures:** each drag is one gesture per parameter. Opening the page
  writes nothing.

## Measurements (`ChimeraSynthTests`, 48 kHz)

- **Tuning:**

  | Setting | Measured |
  |---|---|
  | A3 | 220.00 Hz |
  | Oscillator octave +1 | 440.00 Hz |
  | +7 semitones | 329.66 Hz |
  | +50 cents | 226.40 Hz |
  | Global octave −1 | 110.00 Hz |
  | Bend +2 st | 246.9 Hz |

- **Wave shapes** (harmonics 2 / 3, relative to the fundamental):

  | Wave | Measured | Design |
  |---|---|---|
  | Sine | < −180 dB | none |
  | Triangle | < −270 / −19.08 dB | −∞ / −19.08 dB |
  | Saw | −6.02 / −9.55 dB | −6.02 / −9.54 dB |
  | Square | < −300 / −9.54 dB | −∞ / −9.54 dB |
  | Saw → square, 50 % | −15.6 / −9.54 dB | between the two |

  The wave display draws the same shapes the engine plays.
- **Unison, sub and noise:**
  - Five voices at 100 % detune and 100 % width: side/mid energy 0.30. At
    width 0 %, L and R are identical.
  - Sub −1 oct: +1.94 dB relative to an 80 % oscillator (the level ratio).
  - Noise alone: −14.7 dBFS.
- **Filter:** each type, at cutoff 1 kHz and resonance 30 %, matches its
  analytic response to 0.000 dB at every saw harmonic from 110 Hz to 4 kHz.
- **Amp envelope** (A 100 ms / D 200 ms / S 50 % / R 300 ms):

  | Stage | Measured | Expected |
  |---|---|---|
  | Attack, to 98 % | 97 ms | 98 ms |
  | Level at 250 ms | −5.98 dB | −5.97 dB (graph formula) |
  | Sustain | −5.99 dB | −6.02 dB |
  | Release, to −60 dB | 302 ms | 300 ms |

- **Modulation:**
  - LFO 1 → Pitch A at 10 %: ±2.39 st (design 2.4).
  - LFO 2 synced to 1/4 at 120 BPM → Amp: 2.00 Hz.
  - Env 2 → Cutoff: 1.76 kHz relative to 110 Hz falls from −32 to −85 dB.
  - FM 25 %: the 660 Hz sideband rises from < −180 dB to −7.0 dB.
- **Voices:**
  - A three-note chord gives three equal peaks (within 0.001 dB).
  - Five notes into four voices leave four sounding.
  - Legato glide (100 ms time constant): 270 Hz in the first 25 ms, 440.0 Hz
    settled.
- **Guitar and MIDI:**
  - With the threshold at −40 dBFS, a −50 dBFS input stays silent and a
    −20 dBFS input plays at 110 Hz.
  - With a MIDI A4 held, 440 Hz plays and the guitar's 110 Hz is absent.
- **Contract:**
  - Off: the buffer is untouched at all four points.
  - Every patch at full load stays finite; the peak is 2.0 (the safety bound).
    There are no allocations in the callback.
  - Factory patches at A3: Dvergr Analog −9.4 dBFS, Seidr Dynamic −11.1 dBFS,
    Mimir Impact −25.1 dBFS.
- **Insert points** (in `ChimeraProcessor`, silent input plus MIDI A3):

  | Point | Output |
  |---|---|
  | Amp input | −26.2 dBFS, through the amp; up to 0.22 different from Before POST |
  | Before Tone EQ | −20.9 dBFS |
  | Before POST | −20.9 dBFS |
  | After POST | −20.9 dBFS |

  The signal path shows each point's edge.
- **Page:**
  - Every control is present and attached.
  - A filter-curve drag lands at 3542 Hz (pointer 3557 Hz) and 90 %, with one
    gesture each for cutoff and resonance.
  - An attack drag writes only attack, in one gesture.
  - Patches apply without switching the synth on; Init restores the defaults.

Fixes made while testing:
- The test harness iterated a temporary patch list (a crash).
- Pitch was measured on too low a note, and glide before it had settled.
- Unbounded resonance build-up at full load led to the Q cap (10) and the
  safety stage.

## Not done

- **Engine:**
  - Per-voice LFOs.
  - Wavetable import.
  - An effects section inside the synth.
  - A synth-only output bus.
- **Tracking evidence:** recorded guitar and bass, which the 1.10 tracking gate
  still requires.
- **Latency:** an alignment option.
- **Checks:** listening tests, and CPU measurement in a DAW. The full-load case
  above is the stress test, not a CPU budget.
