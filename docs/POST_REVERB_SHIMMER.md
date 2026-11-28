# POST reverb shimmer

The reverb of every POST slot can hear, beside the note, a copy of it moved
up or down in pitch, and part of that copy moved again. At no amount, the
default, the reverb renders as it did. The dry sound is not touched.

## The controls

| Parameter | ID (slot 1) | Choices or range | Default | What it does |
|---|---|---|---|---|
| Reverb shimmer | `reverbshimmer` | 0 to 1 | 0 | How much of the moved copy the reverb hears. |
| Reverb shimmer interval | `reverbshimmerinterval` | Octave up, Fifth up, Octave down | Octave up | How far the copy is moved. |

Slots 2 to 4 have the same two under `slot2_`, `slot3_` and `slot4_`.

## How it works

What the reverbs hear, after the pre-delay and the freeze, goes through a
pitch shifter: a delay read by two taps half a grain apart, each moving
through a 60 ms grain at the interval's rate and faded in and out over it.
The shifter's output is low-passed at 7 kHz with two poles, added to what
the reverbs hear by the amount, and up to 45% of it goes back into the
shifter, so an octave up also gives two octaves up, lower.

- The loop cannot build: its gain is at most 0.45 and each pass is
  low-passed.
- While FREEZE holds, the reverbs take no new input, so no new shimmer goes
  in; what is already in the reverb is held.
- With the amount at 0 the shifter is not run, and its lines are cleared so
  that an old sound is not heard when it comes back.

The plugin's latency does not change.

## Compatibility

- **Parameters.** 8 parameters appended after every earlier one, at version
  hint 48, slot 1 first. No released parameter moves.
- **Sound.** At no amount the reverb takes the path it took before, whatever
  the interval.
- **Projects.** A project saved before this change opens with no shimmer.

## The ALL window

AMOUNT (knob; a double-click returns it to 0) and INTERVAL (menu) follow
DECAY, SIZE, DAMPING and MIX in the first row, which now holds six
controls. The small line over the two reads SHIMMER. The window keeps its
size.

## Verification (Linux)

- **`ChimeraReverbControlsDSPTests --shimmer`** (new): in five models a
  440 Hz note appears in the reverb at 880, 660 and 220 Hz for the three
  intervals, at least 30 dB over the reverb without shimmer, and at 1760 Hz
  under the 880 for the octave; half the amount gives less; with noise the
  reverb's level rises by 1.1 to 2.4 dB at full; at no amount the render is
  unchanged, sample for sample, whatever the interval; after it is switched
  off a new note's reverb is the plain one; moving the amount and the
  interval under loud noise, frozen as well, stays finite and bounded in
  every model, mono and stereo.
- **`ChimeraSignalOrderTests`** (one new case): the bank of 8 parameters at
  version 48; the amount and the interval act through the processor; the
  interval alone and a switched-off slot's shimmer change nothing, bit for
  bit; saving, loading and an older project.
- **`ChimeraUITests`**: AMOUNT writes its parameter and returns to 0;
  INTERVAL lists its choices and writes its.

The levels are measured within 30 Hz of each frequency: a shifter of this
kind does not put a steady tone on one frequency but on lines one grain's
rate (about 17 Hz) apart around it.

## Not in this change

- A shimmer that follows the tempo shape.
- Other intervals, and a finer shifter.
- Auto Set does not set these controls. Factory presets do not use them.

## Not yet validated

No listening test: every measurement is on synthetic input. The shifter is
a plain two-tap one; on a steady note it beats at the grain's rate, and how
much of that is heard through each reverb was not checked. Octave down was
measured to arrive, not judged for its sound. Not run on macOS or Windows
yet.
