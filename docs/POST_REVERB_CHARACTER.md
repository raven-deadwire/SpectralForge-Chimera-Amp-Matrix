# POST reverb character — converter, dirt and drift

Three controls that give the reverb of every POST slot the sound of an early
digital unit. They act on the reverb only; the dry sound is not touched. At
their defaults the reverb renders as it did.

## The controls

| Parameter | ID (slot 1) | Choices or range | Default | What it does |
|---|---|---|---|---|
| Reverb converter | `reverbconverter` | Off, Early, Middle, Late | Off | The bandwidth and sample rate of an older converter. |
| Reverb dirt | `reverbdirt` | 0 to 1 | 0 | Fewer bits: from 16 down to 7 at 1. |
| Reverb drift | `reverbdrift` | 0 to 1 | 0 | A slow drift of the reverb's pitch. |

Slots 2 to 4 have the same three under `slot2_`, `slot3_` and `slot4_`.

| Converter | Low-pass | Sample rate |
|---|---|---|
| Early | 6 kHz | 16 kHz |
| Middle | 9 kHz | 24 kHz |
| Late | 13 kHz | 32 kHz |

- **Converter.** The reverb is low-passed, held at the lower sample rate and
  low-passed again, each with one pole. The hold is not filtered steeply, so
  some of what it folds back stays in: that is part of the sound.
- **Dirt.** The reverb is rounded to the word length. There is no dither:
  a tail that falls under half a step goes silent.
- **Drift.** The reverb passes through a delay that swings by up to 1.5 ms
  each way at 0.7 Hz, a quarter of a turn apart between the two channels. At
  full that is about 0.7% of pitch each way. The reverb is up to 3.5 ms later
  while it is on. The plugin's latency does not change.

## Compatibility

- **Parameters.** 12 parameters appended after every earlier one, at version
  hint 47, slot 1 first. No released parameter moves.
- **Sound.** At the defaults the reverb takes the path it took before.
- **Projects.** A project saved before this change opens with the defaults.

## The ALL window

CONVERTER (menu) ends the third row, after the shape's TYPE, UNIT, LENGTH
and SECONDS; DIRT and DRIFT (knobs; a double-click returns them to 0) end
the fourth, after DEPTH, CURVE and MORPH. Both rows now hold five controls
and the shape's display beside them is narrower. The small line over the
three reads CHARACTER, where the shape's controls read SHAPE. The first row
stays DECAY, SIZE, DAMPING and MIX. The window keeps its size.

## Verification (Linux)

- **`ChimeraReverbControlsDSPTests --character`** (new): in five models each
  converter takes the top off the reverb, the earliest the most, and leaves
  the whole band within 6 dB; at full DIRT every sample of the reverb is on a
  step of 7 bits; DRIFT keeps the level within 1.5 dB and swings a 1 kHz tone
  by the amount its delay gives; at the defaults the render is unchanged,
  sample for sample; after all three are switched off a new note's reverb is
  the plain one; moving all three at once stays finite and bounded in every
  model, mono and stereo.
- **`ChimeraSignalOrderTests`** (one new case): the bank of 12 parameters at
  version 47; each control acts through the processor; a switched-off slot's
  character changes nothing, bit for bit; saving, loading and an older
  project.
- **`ChimeraUITests`**: the converter menu lists its choices and writes its
  parameter; DIRT and DRIFT write theirs and return to 0.

Measured on the module, the reverb above 10 kHz against the plain reverb:

| Model | Early | Middle | Late |
|---|---|---|---|
| Room | -10.7 dB | -7.0 dB | -4.4 dB |
| Hall | -8.3 dB | -5.3 dB | -3.3 dB |
| Spring | -7.5 dB | -4.9 dB | -3.0 dB |
| Hall Prime | -5.2 dB | -3.3 dB | -1.8 dB |
| Void Space | -4.5 dB | -2.9 dB | -1.6 dB |

DRIFT at full on a 1 kHz tone: 993.4 to 1006.6 Hz (6.6 Hz each way
expected).

## Not in this change

- The companding, pre-emphasis and noise of particular units.
- The converter on what the reverb hears: it is on what the reverb gives.
- Auto Set does not set these controls. Factory presets do not use them.

## Not yet validated

No listening test: every measurement is on synthetic input. In the two
darker models the converter measures small, because they have little above
10 kHz to take; whether the three converters are far enough apart by ear was
not checked. Dirt without dither cuts a quiet tail off; whether that is
wanted was not asked. The names are Chimera's own and stand for no
particular unit. Not run on macOS or Windows yet.
