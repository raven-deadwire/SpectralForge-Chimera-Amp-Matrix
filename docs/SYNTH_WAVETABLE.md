# SYNTH: wavetable oscillators

Status: implemented on the `claude/synth-wavetable` branch, stacked on
`claude/post-four-slots`. Measured by `ChimeraSynthTests` on Linux. Not heard
through a real instrument or a keyboard yet, and not compared with Serum,
ArgWave or any other synth.

## Why

The owner's direction for SYNTH is a synth in the class of Serum or ArgWave 3.
Compared with those, the released SYNTH page lacks wavetable oscillators, a
second filter, a step sequencer and macros, its own effect rack and a preset
browser. The owner chose wavetable oscillators as the first step. The rest is
not in this change.

## What it is

Each oscillator (A, B, C) in Wave mode now has a **table**:

- **Classic** (default): the released oscillator, five shapes that MORPH
  blends. Nothing about it changed (tested sample for sample).
- **Nine wavetables**, each sixteen single-cycle frames. The wave knob, now
  labelled POSITION, moves through the frames; neighbouring frames are
  blended.

| Table | Frames go from … to … |
|---|---|
| Harmonics | a sine to a saw with 64 harmonics |
| Pulse | a square to a 3 % pulse |
| Vowels | A, E, I, O, U formant spectra |
| Metal FM | two-operator FM at 3:1, index 0 to 8 |
| Saw Fold | a saw through a wavefolder, drive 1 to 6 |
| Sync Saw | a hard-synced saw, slave ratio 1 to 8 |
| Drawbars | organ registrations, one drawbar after another |
| Bit Steps | a sine in 64 steps down to 3 |
| Growl | odd harmonics under a formant that moves from 180 Hz to 1.4 kHz |

The tables are computed when the first synth is created. Nothing is loaded
from disk, and the tables are not copies of any product's wavetables.

## Warp

A table has a **warp** and a **warp amount** (0-100 %). The warp reshapes the
phase the table is read at.

| Warp | Effect at 100 % |
|---|---|
| Bend + / Bend - | the wave is pushed to the end / the start of the cycle |
| Squeeze | the first half of the wave fills 5 % of the cycle |
| Sync | the wave restarts eight times per cycle |
| Mirror | the second half plays backwards |
| Quantize | the read steps through 4 positions instead of 64 |

0 % is no warp (tested). Classic ignores the warp.

## Modulation

The matrix targets Wave A / B / C move the position of a table, as they morph
the classic shapes (tested with velocity). The warp amount is not a matrix
target yet: adding one would change the released target list.

## Modulation matrix (page only)

The owner found the matrix hard to read: eight rows of "Off / Off / slider /
0 %" with no labels. The parameters and the engine are unchanged; the page
shows them differently.

- **Titles:** SOURCE, MOVES TARGET, AMOUNT above the rows. An empty menu reads
  "Source ..." or "Target ..." instead of "Off".
- **Rows in use only:** the panel shows the rows up to the last one in use (at
  least one). `+ ADD MODULATION` adds a row; `x` clears a row.
- **Amount:** a bar that grows from the centre, right for up and left for down,
  with the value as "+50 %" or "-40 %". Double-click sets 0.
- **From a knob:** right-click a knob the matrix can move (oscillator pitch,
  wave / position, level and detune, FM, cutoff, resonance, sub and noise
  level, master level) and choose the source. This fills the first free row
  at +50 %. The same menu removes every modulation of that knob.
- **Marks:** a modulated knob shows a dot and an arc from its value towards
  the side the modulation pushes it. The arc's length follows the summed
  amount; it is an indicator, not the exact range.

Not done: dragging a source onto a knob, more than eight rows, the warp
amount or the filter drive as targets, and an animated value on the knob.

## On the page

- A menu in each oscillator's header chooses the table (Wave mode).
- With a table, the display draws six of its frames stacked back to front and
  the current frame, through the warp, in front. Dragging across the display
  sets the position.
- The shape strip is replaced by the warp menu and the warp amount bar.
- The wave knob reads POSITION instead of MORPH.

## Sound quality

- Every frame is stored band-limited at 21 levels, half an octave apart
  (1024, 724, 512 … 1 harmonics). A note reads the first level whose
  harmonics stay under half the sample rate, blended with the next level so
  the tone does not step while the pitch moves.
- Measured at 48 kHz:
  - C8 (4186 Hz) as a full saw: folded harmonics 6 and 7 are below -165 dB re
    the fundamental; harmonic 4 (16.7 kHz) is at -12.0 dB, as in a saw.
  - A3 (220 Hz): harmonic 60 (13.2 kHz) is at its saw level.
- The blend fades the harmonics of the top half octave below half the sample
  rate, so a table is slightly darker at the very top than the classic saw.
- Warps other than Quantize raise the level by their steepest slope, so they
  do not fold. Sync and Quantize have steps in the wave that are not
  band-limited; they can alias on high notes. This was not measured.
- Memory: about 25 MB for the table set, shared by every instance in the
  process. Load time was not measured.

## Parameters and compatibility

- Nine parameters appended at version hint 38, after the POST slot 3 / 4 bank:
  `synthOscATable`, `synthOscAWarp`, `synthOscAWarpAmount`, and the same for
  B and C. All automatable.
- Defaults are Classic, Off and 0 %, which is the released oscillator. An
  older project has no values for them and sounds the same.
- No released parameter changed. The wave parameter keeps its 0-4 range; with
  a table, 0-4 spans the sixteen frames.

## Not done

- No wavetable import, no audio-to-wavetable, no table editor.
- No second filter, step sequencer, macros, effect rack, preset browser or
  on-screen keyboard.
- No factory patch uses a table.
- The display is 48 px high; the stacked frames are small.
