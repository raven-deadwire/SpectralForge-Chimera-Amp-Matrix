# SYNTH: effect rack

Status: implemented on the `claude/synth-effects` branch, stacked on
`claude/synth-sequencer`. Measured by `ChimeraSynthTests` on Linux. Not heard
through a real instrument or a keyboard yet, not checked in a DAW, and not
compared with Serum, ArgWave or any other synth.

## Why

The owner's direction for SYNTH is a synth in the class of Serum or ArgWave 3.
Those have their own effects after the voices. Until now the synth's sound
could only be treated by the guitar chain it joins, and not at all on the
SYNTH output bus. This step adds five effects that belong to the synth. The
preset browser and wavetable import are not in this change.

## The rack

Five effects after the voices and the LEVEL control, before the synth's
safety stage, always in this order:

1. **Chorus** - RATE 0.05-5 Hz, DEPTH, MIX. A 12 ms delay swept by up to
   6 ms; the right channel is a quarter cycle behind the left.
2. **Phaser** - RATE 0.02-8 Hz, DEPTH, FEEDBACK 0-90 %, MIX. Six all-pass
   stages around 800 Hz, swept by up to two octaves each way; the right
   channel a quarter cycle behind.
3. **Delay** - time, FEEDBACK 0-95 %, PING-PONG, MIX.
   - Time: Free (TIME, 1 ms to 2 s) or in time with the host tempo: 1/2, 1/4,
     1/4 D, 1/8 (default), 1/8 D, 1/16, 1/4 T, 1/8 T. 120 BPM when the host
     gives none. Never longer than 2 s.
   - PING-PONG: the repeats alternate between left and right.
   - Each repeat loses treble above 8 kHz. A change of time glides over about
     50 ms.
4. **Reverb** - DECAY 0.2-12 s (the time to fall 60 dB), DAMP (how much
   faster the treble dies), MIX.
5. **EQ** - LOW, a shelf at 200 Hz, and HIGH, a shelf at 4 kHz, each
   -12 to +12 dB.

- MIX is the effect's share: 0 % is the sound without it, 100 % is the effect
  alone.
- Every effect is off by default. With all five off the rack is not run and
  the synth's output is unchanged (tested sample for sample).
- Switching an effect on or off, or moving MIX, takes effect over up to
  20 ms, so nothing clicks. An effect that is switched off forgets what it
  held: switched on again it starts empty.
- The effects are on the synth only. The guitar is not sent through them.
  They are in the sound on both outputs (the guitar mix and the SYNTH bus).

## On the page

- **FX** in the top bar shows the rack in the lower row, in place of the
  envelopes, the LFOs, the matrix and sub / noise. FX again brings them
  back. The oscillators, the filters and the master section stay.
- The effects run whichever view is shown. The view is not saved with the
  project: the page opens on the envelopes.
- The delay's TIME is dimmed while the delay follows the tempo.
- To make room for FX in the top bar the engine buttons and the PATCH menu
  are narrower.

## Parameters and compatibility

- Twenty-two parameters appended at version hint 41, after the sequencer
  bank: `synthFxChorusOn` / `Rate` / `Depth` / `Mix`, `synthFxPhaserOn` /
  `Rate` / `Depth` / `Feedback` / `Mix`, `synthFxDelayOn` / `Sync` / `Time` /
  `Feedback` / `PingPong` / `Mix`, `synthFxReverbOn` / `Decay` / `Damp` /
  `Mix`, `synthFxEqOn` / `Low` / `High`. All automatable.
- No released parameter changed.
- Loading a patch sets every effect to its default (off), as it does every
  other synth parameter.

## Not done

- The order of the effects is fixed, and there is one of each.
- No effect is a target in the modulation matrix.
- No compressor, flanger or multi-mode filter in the rack. The distortion
  stays in the voice (docs/SYNTH_FILTERS.md).
- The delay has no separate left and right times and no control for its
  treble loss.
- The reverb has no size and no pre-delay control. Its level on a single
  held sine depends on the note (see the measurements).
- The plugin does not report the delay's or the reverb's tail to the host.
  While the synth is on the tail plays out; a host that stops calling the
  plugin as soon as its input is silent may cut it.
- The delay follows the tempo, not the host's bar position.
- No factory patch uses an effect.
- DE / KO manual text is not written.

## Measurements

From `ChimeraSynthTests` on Linux (x86-64), 48 kHz, this branch. Sine notes
unless said otherwise.

- Every effect set to an extreme but off: largest sample difference from the
  synth without the rack 0. Chorus, phaser, delay and reverb on at 0 % mix:
  0. EQ on at 0 dB: 0.
- Delay, Free 250 ms, feedback 50 %, mix 50 %, a 21 ms note: repeats at
  250 / 500 / 750 ms at 0 / -6.0 / -12.0 dB re the note; silence between.
  1/8 at 120 BPM repeats at 250 ms, at 60 BPM at 500 ms; 1/8 D at 120 BPM at
  375 ms; 1/2 at 30 BPM (4 s) is held at 2 s.
- Ping-pong: first repeat on the left only, second on the right only, 6 dB
  lower.
- Delay switched on inside a held 110 Hz note: largest step between samples
  0.00577, the note's own 0.00577 (no click). Switched off 0.1 s after a note
  with 500 ms repeats and on again at 0.3 s: silence afterwards.
- Reverb, mix 100 %: level falls 61 dB over one DECAY at 1 s and at 4 s.
  A6 at 2 s: 60 dB with damping 0 %, 119 dB with 100 %. Left-right
  correlation of the tail -0.13.
- Reverb level, mix 100 %, held notes from A1 to C7 in minor thirds, against
  the dry note: mean -2.6 dB, lowest -20.4 dB, highest +8.1 dB. A single sine
  sits on or between the reverb's resonances, so its level varies this much;
  sounds with many partials were not measured.
- Chorus, 1 Hz, depth 100 %, mix 100 %, A4: 423.5 Hz at 1.0 s and 456.5 Hz
  at 1.5 s on the left (the sweep's 3.8 %); 440 Hz on the right at 1.0 s.
  Depth 0 %: the dry note 12 ms late, difference 0.
- Phaser at rest (depth 0 %, no feedback, mix 50 %): G5 (784 Hz) -24.3 dB,
  A4 (440 Hz) -0.07 dB. Swept at 1 Hz, G5's level moves over 25.7 dB.
- EQ, five settings at A1 / A3 / A5 / A7: largest error against the shelf
  curves 7e-8 dB. At +12 / +12 dB the curve gives 55 Hz +11.9 dB, 880 Hz
  +0.08 dB, 12 kHz +11.9 dB.
- Every effect on, eight saw voices, LEVEL +6 dB, feedback at maximum: peak
  2.0 (the safety stage's limit), no allocation on the audio thread.
- Page at 1140 x 438: 22 effect controls in the lower row, none overlapping,
  smallest knob 50 px.
- `ChimeraWholeProductTests` switches everything on with random settings and
  expects silence 4 s after the input stops. A delay at up to 95 % feedback
  or a reverb at up to 12 s cannot do that, so that test now sets the synth
  delay's feedback to 0 and the reverb's decay to 1 s, as it already does for
  the POST delays. Before that change it failed.
- CPU was not measured.

Screenshot: `docs/evidence/synth-effects-20261126/synth-page-effects.png`.
