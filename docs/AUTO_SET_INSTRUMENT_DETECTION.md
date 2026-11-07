# Auto Set: instrument detection

Status: prepared on `claude/autoset-instrument-detect`. Checked on a
synthetic corpus only; accuracy on real recordings is **not measured yet**
(see *Checking a recording*).

## What changed

Before, Auto Set told guitar from bass with one number: the energy at
35–71 Hz against the 71 Hz–1.4 kHz body. It had no notion of a voice, keys or
drums. A singer or a drum machine routed into Chimera was set up as a guitar:
amp gain, cabinet mic, boost pedal and guitar EQ curves were all applied.

Now the input pass also listens to the signal going into the amps
(`Tap::amp`, after any pedals). `Source/AutoSetInstrument.h` classifies those
6 s as one of **Guitar, Bass, Vocal, Keys / synth, Percussion or Unknown**,
with a confidence:

| Cue | How it is measured |
|---|---|
| Pitch | McLeod NSDF on a 12 kHz copy, 10 ms hop. A long frame (107 ms, down to 25 Hz so a bass's A0 is heard) reads the range (10th/50th/90th percentile); a short frame (32 ms) follows vibrato |
| Is the pitch real? | Share of each pitched frame's power at its own pitch (Hann-windowed single-bin DFT). Single notes read −20 to 0 dB; a chord's period reads −60 dB or less, so chords do not count as low notes |
| Attack | 2 ms envelope frames, 20 dB below the peak to 3 dB below it |
| After the peak | Level slope over 100–600 ms: a plucked string falls 15+ dB/s, a voice or organ holds; share of hits gone (−20 dB) within 250 ms |
| Pitched hits | Share of onsets whose first 100 ms are pitched (snare and cymbals are not; a palm-muted chug is) |
| Pitch movement | Residual around a 150 ms moving mean inside held notes (vibrato, jitter); net glides of 80+ cents |
| Spectrum | The old low-energy ratio; the 3–8 kHz noise floor against the body (clipping fills it, clean keys do not) |

Rules (`classifyInstrument`):
- **Vocal:** slow attacks (18–40 ms+), notes that hold, and pitch that
  wavers (vibrato or jitter of 4–9+ cents) or glides; pitch 70 Hz–1.1 kHz.
- **Keys / synth:** notes that hold at a steady pitch with a clean 3–8 kHz
  band.
- **Percussion:** most hits unpitched and gone within 250 ms.
- Otherwise **guitar or bass**:
  - **Bass:** notes below 40 Hz. A Drop A guitar stops at A1 = 55 Hz and an
    8-string in drop E at 41 Hz.
  - **Guitar:** median at 220 Hz or above, or 90th percentile at 600 Hz or
    above.
  - **Between those:** the old low-energy cue decides, exactly as before. A
    low line here is equally a bass (A1, E1) or a down-tuned guitar (Drop A
    chugs on A1).
- A vocal, keys or percussion verdict must score at least 0.6 **and** beat the
  guitar/bass score by 0.25; when in doubt the answer is guitar or bass.

## What Auto Set does with it

- The input step says what it heard, e.g. `heard Vocal (detected, 94%)`, and
  the status line shows it during the input, Tone EQ and multiband passes.
- **Vocal or Percussion** (with *Instrument: detected*), Auto Set skips the
  steps made for guitar and bass. Each one is reported as
  `kept (Vocal heard: this step is for guitar and bass; choose Guitar or Bass
  in the options to run it)`:
  - boost pedal, amp channel, amp gain/tone, presence/resonance;
  - cabinet mic, cabinet cuts, the two-mic blend;
  - Dual rig levels and blend, Matrix lanes;
  - the Tone EQ target curve, and the Final EQ target curve (the output
    level still runs).
- **Steps that still run:** input gain, the gates and Expander, the Dynamic EQ,
  compressor, multiband, de-esser, limiter, bus compressor, delay/reverb,
  doubler and output level.
- **Keys / synth** is reported but keeps every step:
  - organ and Rhodes are often played through amps;
  - a guitar through a drive or compressor pedal holds its notes like keys.
  Only a voice or drums make Auto Set leave the amp and cabinet alone
  (`keepsAmpSteps`).
- **Guitar or Bass chosen in the options** overrides the detection; every step
  runs as before.
- **When the pitch range settles guitar vs bass**, that answer is passed to
  the amp, channel, power, routing and EQ analyses. Otherwise they keep their
  own estimate, as before. The Auto Set test DI (110–165 Hz) is in the
  in-between range, so its runs are unchanged.

## Tests

`ChimeraAutoSetInstrumentTests` runs a synthetic corpus of 61 cases. Each case
is checked over 8 s and over its first 6 s (Auto Set's window).

The corpus:
- **Guitar:** Karplus-Strong single-note lines, strummed chords, palm-muted
  drop-D / 7-string riffs, Drop A riffs on A1 (55 Hz) with power chords and
  leads, a guitar through heavy clipping with finger vibrato.
- **Bass:** 4-string, 5-string, slap, Drop A on A0 (27.5 Hz).
- **Vocal:** male and female vowels made of glottal pulses through formants,
  with vibrato, jitter, glides and breath.
- **Keys:** organ, synth pad, synth lead, piano (piano may read as guitar).
- **Drums:** two patterns.
- **Other:** Auto Set's own test DI, silence, a noise floor, and 44.1/96 kHz.

Results:
- Every case is classified as accepted; there are 0 misses on both windows.
- Drop A guitar always reads Guitar, and pitch never calls a guitar bass.
  Drop A bass reads Bass by pitch.
- 4-string, 5-string and slap lines that stay above 40 Hz are decided by the
  old low-energy cue, which calls most of these synthetic basses Guitar.
  Either answer is accepted, because both keep every step.
- No guitar or bass case loses its amp steps.
- The highest vocal or percussion score on any guitar or bass case is 0.0.
- Piano is detected as guitar. That is allowed, because it keeps every step.

`ChimeraAutoSetInstrumentControllerTests` runs Auto Set on the real processor:
- **A voice:**
  - it is detected as Vocal;
  - the guitar/bass steps are reported kept;
  - no amp, cabinet, pedal, routing or Tone EQ parameter moves;
  - input, gates and dynamics run.
- **The same voice with Guitar chosen** runs the guitar steps.
- **A guitar** keeps every step.

## Checking a recording

```
ChimeraAutoSetInstrumentTests --classify take1.wav take2.wav
```

This prints the verdict, confidence and every cue for each 6 s piece and for
the whole file. Use DI recordings (or the signal going into the amp).

## Limits

- **Synthetic corpus only.** Real voices, keys and drums vary far more:
  - straight-tone singing without vibrato or glides may read as keys or
    guitar;
  - a guitar played with volume swells and wide vibrato, or an e-bow, may read
    as vocal.
  In both cases the user can choose Guitar or Bass in the options.
- **Guitar or bass between 40 and ~220 Hz** is decided by the old low-energy
  cue, as before. On the synthetic corpus it calls most normal-tuned bass
  lines Guitar.
- **Down-tuned guitar and bass (e.g. Drop A):** choose Guitar or Bass in the
  options. Detection does not replace that choice there.
- **Detection uses the input pass only (6 s).** If it ran with no audio on the
  amp tap, the result is unknown and Auto Set behaves as before.
