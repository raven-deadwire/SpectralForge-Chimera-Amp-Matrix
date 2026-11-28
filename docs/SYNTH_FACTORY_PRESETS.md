# SYNTH: factory presets

Status: implemented on the `claude/synth-factory-presets` branch. Measured by
`ChimeraSynthTests` on Linux. **Nobody has listened to these presets.** They
were written from the parameter definitions and checked by level
measurements only; whether they sound good, or like their names, is not
known. Not tried in a DAW, and not on Windows or macOS.

## Why

The preset browser listed four factory presets (Init and the three engine
patches), and none of them used a wavetable, filter 2, the distortion, the
step sequencer, a macro or the effect rack. This step adds seventeen that do.

## The presets

They follow the first four in the list, in this order.

| Preset | Type | What it is | Macros |
|---|---|---|---|
| Iron Sub | Bass | Classic saw and a square sub through a low-pass 24, Tube distortion after the filters, low shelf +3 dB | Macro 1 opens the filter, Macro 2 adds drive |
| Growl Bass | Bass | Growl table, LFO 1 in time (1/8) moving the position, Tube distortion, sine sub | Macro 1 position, Macro 2 cutoff |
| Step Bass | Bass | Pulse table; the step sequencer (16 steps, 1/16) moves the cutoff | Macro 1 cutoff, Macro 2 resonance |
| Fold Bass | Bass | Saw Fold table swept by envelope 2, Wavefold before the filters, filter 2 as a 30 Hz high-pass | Macro 1 position, Macro 2 distortion mix |
| Sync Lead | Lead | Sync Saw table on A and an octave below on B, position swept by envelope 2, dotted-eighth delay, reverb | Macro 1 position; mod wheel bends the warp |
| Vowel Lead | Lead | Vowels table swept by LFO 1 (0.25 Hz), glide, chorus, quarter-note delay | Macro 1 position, Macro 2 closes the filter |
| Razor Lead | Lead | Five detuned saws over a square an octave down, Hard clip after the filters, quarter-note delay | Macro 1 detune, Macro 2 drive |
| Frost Pad | Pad | Harmonics table (five voices) and Drawbars an octave up, slow attack, chorus, 5 s reverb | Macro 1 cutoff, Macro 2 position of B |
| Comb Pad | Pad | Five detuned saws through a low-pass and a Comb + that follows the note, in parallel; 4 s reverb | Macro 1 filter balance, Macro 2 comb resonance |
| Drift Pad | Pad | Vowels and Growl tables moved by a slow, smoothed sequence (8 steps of a half note), phaser, 6 s reverb | Macro 1 cutoff |
| Bell Pluck | Pluck | Metal FM table struck by envelope 2, no filter, ping-pong eighth delay, reverb | Macro 1 position; velocity adds brightness |
| Bit Pluck | Pluck | Bit Steps table, Bit crush before a low-pass 24 with a fast envelope, sixteenth delay | Macro 1 position, Macro 2 crush |
| Comb Pluck | Pluck | Noise and a saw into a Comb + that follows the note (resonance 90 %): a plucked string | Macro 1 shortens the ring |
| Drawbar Keys | Keys | Drawbars table, organ envelope, light Tube distortion, fast chorus (3 Hz), eight voices | Macro 1 pulls more drawbars, Macro 2 drive |
| Soft Keys | Keys | Harmonics table with a sine an octave up, piano-like decay, velocity opens the filter, chorus, short reverb | Macro 1 position |
| Step Sweep | FX | Sync Saw table; the sequencer (16 steps, 1/16) moves the position and the cutoff; phaser, ping-pong delay | Macro 1 resonance |
| Storm Drone | FX | Growl an octave down and Metal FM detuned, noise, low-pass and band-pass in parallel, sample-and-hold on filter 2, 9 s reverb | Macro 1 noise |

- Every one of the seventeen uses at least one macro; Macro 1 always does
  something.
- Three use the step sequencer (Step Bass, Drift Pad, Step Sweep). It follows
  the host tempo.
- Like every preset, they leave SYNTH ON, SOURCE, INSERT, LEVEL and THRESH as
  they are.
- None uses a sample or an imported wavetable.

## Compatibility

- No parameter was added or changed.
- The first four factory presets keep their places and their settings.
- A user preset with the same name as a factory preset is still listed as its
  own preset.
- A project that shows a preset name keeps it: names are matched, and none of
  the first four changed.

## Not done

- No listening, by me or anyone else (see Status).
- No presets made for a guitar or bass as the source: the levels were
  measured with MIDI notes only.
- No preset uses oscillator C, FM, a sample or Sample mode.
- The types Lead, Keys and FX have two or three presets each; there is no
  larger library.
- The manual does not list the factory presets.

## Measurements

From `ChimeraSynthTests` on Linux (x86-64), 48 kHz, MIDI source, LEVEL at its
default (-6 dB), 120 BPM. All in dBFS. "A3" is one note held for 2 s; "chord"
is C3-G3-C4-E4 held for 2 s (C2-G2-C3-E3 for a bass). "Macro 1 change" is the
RMS of the difference between Macro 1 at 0 % and at 100 % over the first
second of A3.

| Preset | A3 peak | A3 RMS 0.05-0.5 s | A3 RMS 1.5-2 s | Chord peak | DC | Macro 1 change |
|---|---|---|---|---|---|---|
| Iron Sub | -6.4 | -13.1 | -12.4 | 4.0 | -55.3 | -11.6 |
| Growl Bass | -8.6 | -16.6 | -16.1 | 1.8 | -36.3 | -24.0 |
| Step Bass | -6.5 | -12.7 | -12.7 | 4.4 | -65.2 | -13.2 |
| Fold Bass | -8.8 | -14.7 | -15.1 | 2.5 | -67.5 | -11.8 |
| Sync Lead | -9.9 | -22.3 | -22.6 | -2.1 | -95.8 | -20.2 |
| Vowel Lead | -7.6 | -21.3 | -19.6 | -2.7 | -98.2 | -25.3 |
| Razor Lead | -9.6 | -17.8 | -17.1 | -0.4 | -63.6 | -19.2 |
| Frost Pad | -9.8 | -26.3 | -19.0 | -3.7 | -89.7 | -35.6 |
| Comb Pad | -7.7 | -22.2 | -19.7 | -0.6 | -84.3 | -26.0 |
| Drift Pad | -10.8 | -32.3 | -23.2 | -3.0 | -82.4 | -38.2 |
| Bell Pluck | -10.4 | -28.3 | -74.8 | -1.8 | -72.8 | -25.0 |
| Bit Pluck | -10.6 | -33.3 | -155.8 | -1.4 | -79.5 | -40.1 |
| Comb Pluck | -10.5 | -19.6 | -76.8 | -1.0 | -69.0 | -28.2 |
| Drawbar Keys | -10.8 | -21.5 | -22.0 | -4.5 | -76.3 | -29.1 |
| Soft Keys | -12.4 | -22.9 | -27.9 | -4.6 | -72.3 | -35.6 |
| Step Sweep | -14.6 | -26.5 | -24.3 | -7.8 | -81.6 | -24.5 |
| Storm Drone | -9.3 | -35.0 | -21.1 | -1.8 | -92.1 | -35.8 |

Limits the test holds every one of the seventeen to:

- every parameter it names exists, is one a preset may hold, is named once,
  and has a value its control can hold;
- its name is unique and usable as a file name, and its type is one of the
  seven;
- one note peaks above -18 dBFS and its RMS is above -40 dBFS in one of the
  two windows;
- a bass: one note peaks below -3 dBFS. The others: the four-note chord peaks
  below full scale. (Four bass notes at once reach +1.8 to +4.4 dBFS before
  the synth's safety stage, which holds the output at +6 dBFS.)
- DC of the chord below -30 dBFS;
- Macro 1 at 100 % changes the sound by more than -50 dBFS RMS.

Two settings were changed because of these measurements: Growl Bass and Fold
Bass first put out DC at about -25 dBFS (a distortion on a lopsided wave).
Growl Bass now uses Tube, whose DC the voice removes (-36 dBFS); Fold Bass
uses filter 2 as a 30 Hz high-pass (-68 dBFS).

Every factory preset at full load (8 voices, unison 7 on three oscillators,
character, drive and resonance at 100 %): peak 2.0 (the safety stage's limit),
no allocation on the audio thread.
