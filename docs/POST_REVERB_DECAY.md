# POST reverb decay time and shape display

Two additions to the reverb of every POST slot, on top of
`docs/POST_REVERB_CONTROLS.md` and `docs/POST_REVERB_SHAPE.md`:

- **DECAY**: the reverb's decay time in seconds, set apart from SIZE.
- **A display of the tempo shape** in the reverb's ALL window.

## DECAY

| Parameter | ID (slot 1) | Range | Default | What it does |
|---|---|---|---|---|
| Reverb decay | `reverbdecay` | SIZE, then 0.2 to 20 s | SIZE | The time the reverb takes to fall 60 dB. |

Slots 2 to 4 have the same under `slot2_`, `slot3_` and `slot4_`.

At the bottom of its range (anything under 0.2 s) the control reads `SIZE`
and is not there: SIZE sets the decay as it always did. From 0.2 s up DECAY
sets the decay and SIZE keeps only what else it did:

| Model | With DECAY set, DECAY does | With DECAY set, SIZE still does |
|---|---|---|
| Room | sets the decay | nothing |
| Hall | sets the decay | the hall's own short delay before the reverb |
| Spring | sets the decay | the length of the spring |
| Hall Prime, Void Space | sets the decay | the size of the space |
| Gated Room | how long the gate stays open, up to 5 s | nothing |

The time is the decay of the low end. DAMPING still makes the highs die
sooner, so a decay measured over the whole band is shorter than the figure
set, by 10 to 20% at the default DAMPING.

### Compatibility

- **Parameters.** 4 parameters appended after every earlier one, at version
  hint 45, slot 1 first. No released parameter moves.
- **Sound.** With DECAY at `SIZE`, the default, the reverb takes the values
  it took before. The render comparison of `docs/POST_REVERB_CONTROLS.md`
  (the reverb module before the controls and now, 108 configurations) is
  still bit-identical.
- **Projects.** A project saved before this change opens with DECAY at
  `SIZE`.

### How it works

A loop that takes `t` seconds to go round falls 60 dB in the decay time when
its gain is `10^(-3 t / decay)`. The two rooms get that gain through
`juce::Reverb`'s room size (its eight combs average 1378 samples at
44.1 kHz), the spring through its feedback (limited to 0.995), and the three
feedback delay networks are handed the time directly.

## The shape display

The third row of the reverb's ALL window shows, to the right of the shape's
three controls:

- the level the reverb follows through one pass of the shape, at the set
  depth;
- a line for every quarter note of the pass;
- the tempo, and the shape's name and length;
- while a shape runs, where the pass is now.

With the shape Off it shows a level line and `SHAPE OFF`. The display takes
no clicks. The position is published by the audio thread once a block and
read by the display 30 times a second.

DECAY is the first knob of the first row; a double-click returns it to
`SIZE`. The window keeps its size, 740 by 598.

## Verification (Linux)

- **`ChimeraReverbControlsDSPTests --decay`** (new): in the five decaying
  models the measured decay is the one set, at a small and a large SIZE; the
  Gated Room's gate follows it; under its minimum the render is unchanged,
  sample for sample, and SIZE still sets the decay; moving it while a tail
  rings stays bounded, mono and stereo.
- **`ChimeraSignalOrderTests`** (one new case): the bank of 4 parameters at
  version 45; it reads `SIZE` or seconds; it lengthens and shortens the
  reverb through the processor; a switched-off slot's decay changes nothing,
  bit for bit; saving, loading and an older project.
- **`ChimeraUITests`**: DECAY reads `SIZE` at its lowest, writes and reads
  seconds, and returns on a double-click; the display draws the chosen shape,
  depth and length, shows Off as level, is inside the panel and covers no
  control; the window shows every row without scrolling.

Measured on the module, below 300 Hz, after a 50 ms burst of noise (set ->
measured at SIZE 0.2 / 0.8):

| Model | 0.5 s | 2 s | 6 s |
|---|---|---|---|
| Room | 0.51 / 0.51 | 2.03 / 2.03 | 6.11 / 6.11 |
| Hall | 0.54 / 0.54 | 2.02 / 2.02 | 6.09 / 6.07 |
| Spring | 0.45 / 0.48 | 1.82 / 1.82 | 5.35 / 5.47 |
| Hall Prime | 0.47 / 0.49 | 1.95 / 1.98 | 5.72 / 5.70 |
| Void Space | 0.56 / 0.54 | 1.96 / 1.95 | 5.79 / 5.81 |

The test allows 20% either way.

The tests' parameter-count checks now count the reverb banks after the
shape's through one constant, `postReverbAfterShapeParameterCount`, so a
later reverb bank is added there and those checks are not edited again.

## Not in this change

- DECAY on the POST page's reverb row: it is in the ALL window only.
- Decay times for the highs and lows apart.
- Auto Set does not set DECAY. Factory presets do not use it.

## Not yet validated

No listening test: every measurement is on synthetic input. The decay is
set once a block and is not glided: moving DECAY fast while a loud tail
rings was checked to stay bounded, not to be free of zipper noise. The
display's position was not checked against a real host transport. Not run
on macOS or Windows yet.
