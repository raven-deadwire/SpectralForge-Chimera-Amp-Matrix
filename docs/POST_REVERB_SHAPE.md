# POST reverb tempo shape — the reverb's level follows the tempo

Adds three controls to the reverb of every POST slot: a shape that the
reverb's level follows, how deep it goes, and how long one pass of it is.
With the shape Off, the default, the reverb is not touched. This builds on
`docs/POST_REVERB_CONTROLS.md` and on the reverb's ALL window that shows
those controls.

## The controls

| Parameter | ID (slot 1) | Choices or range | Default | What it does |
|---|---|---|---|---|
| Reverb shape | `reverbshape` | Off, Rise, Rewind, Chop, Tide, Duck | Off | The form the reverb's level follows. |
| Reverb shape depth | `reverbshapedepth` | 0 to 1 | 1 | 0 leaves the reverb as it is; 1 applies the whole shape. |
| Reverb shape length | `reverbshapelength` | 1/4, 1/2, 1 bar, 2 bars | 1 bar | One pass of the shape: 1, 2, 4 or 8 quarter notes. |

Slots 2 to 4 have the same three under `slot2_`, `slot3_` and `slot4_`.

| Shape | The reverb's level through one pass |
|---|---|
| Rise | rises smoothly from nothing to full |
| Rewind | rises like a decay played backwards, then cuts |
| Chop | full for the first half, silent for the second |
| Tide | one rise and one fall |
| Duck | ducked at the start, recovering quickly |

Only the reverb is shaped. The dry sound is not touched.

## Tempo and position

- **While the host plays**, the shape is locked to the host's position: a
  pass starts wherever the position in quarter notes is a multiple of the
  length. "1 bar" is four quarter notes counted from the start of the song;
  the host's time signature and bar offsets are not read.
- **While the host is stopped, or gives no position**, the shape runs freely
  at the tempo Chimera already uses for its delay, from 20 to 999 BPM.
- The level is followed over 3 ms, so the Chop and the end of Rewind do not
  click.

## Compatibility

- **Parameters.** 12 parameters appended after every earlier one, at version
  hint 44, slot 1 first. No released parameter moves.
- **Sound.** With the shape Off the reverb takes the path it took before.
  The render comparison of `docs/POST_REVERB_CONTROLS.md` (the reverb module
  before the controls and now, 108 configurations) is still bit-identical.
- **Projects.** A project saved before this change opens with the shape Off.

## The ALL window

The reverb's ALL window has a third row: SHAPE (menu), SHAPE DEPTH (knob,
double-click returns it to 1) and SHAPE LENGTH (menu). The window is 740 by
598. The reverb's row on the POST page is not changed.

## Verification (Linux)

- **`ChimeraReverbControlsDSPTests --shape`** (two new cases): every shape
  stays within 0 to 1, repeats and has its form; on the reverb, in every
  model, each shape gives its level through the bar, depth scales it, length
  sets its period, the host's position locks it, the Gated Room's own gate
  and the shape's both apply, and after Off a new note's reverb is the plain
  one.
- **`ChimeraSignalOrderTests`** (one new case): the bank of 12 parameters at
  version 44; the Chop through the processor; a switched-off slot's shape
  changes nothing, bit for bit; saving, loading and an older project.
- **`ChimeraUITests`**: the two menus list their choices and write their
  parameters, the knob writes its, and no control of the window overlaps
  another.
- The whole test suite, with the interface tests: 116 of 116 pass.

Measured on the module with steady noise at 120 BPM and a length of one
bar, against the same reverb without a shape:

| Shape | Where in the bar | Reverb level |
|---|---|---|
| Chop | open half | -0.0 dB |
| Chop | closed half | below -270 dB |
| Rise | start / middle / end | -15 to -16 dB / -6.1 dB / -0.1 dB |

The five models measured (the two rooms, the spring, Hall Prime and Void
Space) agree to the figures shown. Through the whole processor the Chop
leaves the open half within -102 dB of the plain reverb and takes the reverb
out of the closed half.

The parameter-count checks of thirteen test files count the new bank, and
the two slot checks of `SignalOrderTests.cpp` know version 44.

## Not in this change

- Time signatures other than four quarter notes to the bar, dotted and
  triplet lengths, and drawing a shape by hand.
- A picture of the shape in the window.
- Auto Set does not set these controls. Factory presets do not use them.

## Not yet validated

No listening test: every measurement is on synthetic input. Not run in a
host: the lock to the host's position is tested with a position handed to
the processor by the test, not with a real transport, loop or tempo change.
The shape names are working names. Not run on macOS or Windows yet.
