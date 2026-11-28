# More of the POST reverb shape — seconds, curve and tone morph

Four more controls for the tempo shape of every POST slot's reverb, on top of
`docs/POST_REVERB_SHAPE.md`. At their defaults the shape is what it was, and
with the shape Off none of them does anything.

## The controls

| Parameter | ID (slot 1) | Choices or range | Default | What it does |
|---|---|---|---|---|
| Reverb shape unit | `reverbshapeunit` | Bars, Seconds | Bars | Whether one pass of the shape is a length in bars or a time. |
| Reverb shape seconds | `reverbshapeseconds` | 0.1 to 10 s | 1 s | The length of one pass when the unit is Seconds. |
| Reverb shape curve | `reverbshapecurve` | -1 to +1 | 0 | Bends the shape's level around its middle: hard above 0, soft below. |
| Reverb shape morph | `reverbshapemorph` | -1 to +1 | 0 | The reverb's tone follows the shape. |

Slots 2 to 4 have the same four under `slot2_`, `slot3_` and `slot4_`.

- **Seconds.** The shape runs freely at its own length. The tempo and the
  host's position are not read, and the length in bars is not used.
- **Curve.** Above 0 the level moves away from the middle, towards a switch;
  below 0 it stays near the middle for longer. The ends and the middle of the
  shape stay where they are, and a rising shape still rises. Chop is a
  switch already and is not changed.
- **Morph.** Above 0 the reverb is brighter where the shape is high and
  darker where it is low, by up to the whole range of TONE at +1; below 0
  the other way round. It adds to TONE and the sum is held within TONE's
  range. With DEPTH at 0 the level stays and only the tone moves.

## Compatibility

- **Parameters.** 16 parameters appended after every earlier one, at version
  hint 46, slot 1 first. No released parameter moves.
- **Sound.** At the defaults the shape takes the values it took before, and
  with the shape Off these controls are not read.
- **Projects.** A project saved before this change opens with the defaults.

## The ALL window

The shape now has two rows, with its display beside both:

- third row: SHAPE, UNIT, LENGTH, SECONDS. The length not in use (LENGTH in
  seconds, SECONDS in bars) is greyed;
- fourth row: DEPTH, CURVE, MORPH. A double-click returns CURVE and MORPH
  to 0.

The display draws the level with the curve applied. In seconds it shows the
time in place of the length in bars, `FREE` in place of the tempo, and four
parts in place of quarter notes. Morph is not drawn. The window is 740 by
762; on a screen too low for it the window scrolls, as the other large
windows do.

## How it works

The curve is `v^k / (v^k + (1-v)^k)` on the shape's level `v`, with
`k = 1 + 4c` for a curve `c` above 0 and `1 / (1 - 4c)` below. Morph moves
the tone shelf of `docs/POST_REVERB_CONTROLS.md` once a block, as TONE itself
is set.

## Verification (Linux)

- **`ChimeraReverbControlsDSPTests --shape-more`** (new): the curve leaves
  the level alone at 0, keeps the ends and the middle, never makes a rising
  level fall, and bends the right way; in five models a Chop one second long
  renders the same at 60 and 180 BPM and with a host position, and opens and
  closes each second; a hard and a soft Rise differ from the Rise as drawn
  as they should; morph moves the highs and the lows opposite ways with the
  shape; with the shape Off the render is unchanged, sample for sample.
- **`ChimeraSignalOrderTests`** (one new case): the bank of 16 parameters at
  version 46; a shape two seconds long is the one-bar shape at 120 BPM
  through the processor, sample for sample; CURVE and MORPH act through the
  processor; with the shape Off, and in a switched-off slot, nothing
  changes, bit for bit; saving, loading and an older project.
- **`ChimeraUITests`**: UNIT enables one length and greys the other; SECONDS,
  CURVE and MORPH write their parameters; the display shows seconds and
  draws the curve.

Measured on the module with steady noise:

| What | Result |
|---|---|
| Hard Rise (+1), early in the bar, against the Rise as drawn | about -64 dB |
| Soft Rise (-1), early in the bar, against the Rise as drawn | about +11 dB |
| Morph +1 with a Chop at no depth, open half against closed half, above 4 kHz | +6.6 to +10.9 dB by model |
| The same, below 300 Hz | -7.0 to -10.8 dB by model |

## Not in this change

- Dotted and triplet lengths, time signatures, and drawing a shape by hand.
- A shimmer that follows the shape.
- Auto Set does not set these controls. Factory presets do not use them.

## Not yet validated

No listening test: every measurement is on synthetic input. Morph is set
once a block, so with a large block and a fast shape the tone moves in
steps; whether that is heard was not checked. A hard curve at +1 is close to
a switch and was not checked for clicks beyond the 3 ms the level is
followed over. Not run in a host. Not run on macOS or Windows yet.
