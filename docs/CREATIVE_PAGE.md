# CREATIVE page

Status: implemented on the `claude/creative-page` branch. Measured by
`ChimeraCreativeTests` on Linux. Not heard through a real instrument yet, and
not compared against any hardware or third-party plugin.

## Why

The 1.9 CREATIVE slot (docs/CREATIVE_1_9.md) was a row on the POST page with a
dialog of six knobs called A, B and C. The owner's brief was different:

- CREATIVE is its own page, like SYNTH.
- The slicer plays rhythmic patterns, not one on/off rate.
- The Laser is a pitch effect, not a filter.
- A creative effect can run before the amps or after them.

This change does those four things and keeps every released parameter and
every saved project working.

## The page

CREATIVE is a stage next to SYNTH in the stage strip. The strip shows the
models that are on, or BYPASSED.

- SLOT 1 / SLOT 2 choose which slot's controls are on screen. This is display
  only: it changes no parameter.
- Nine model tiles on the left select the model.
- The centre shows the model: the step grid for the slicer, the sweep graph
  for the Laser, the model's face and description for the others.
- The right side has the model's three controls under their real names (RATE,
  DEPTH, DUTY and so on), SYNC, MIX, OUTPUT and ON.
- PLACEMENT at the bottom chooses where the slot runs.

The CREATIVE rows on the POST page stay: they are the handle that sets the
slot's position inside the POST rack. ALL on a row opens the CREATIVE page on
that slot.

## Placement

`creativeInsert` (and `slot2_creativeInsert`):

| Choice | Where the slot runs |
|---|---|
| Amp input | Before the amps, after PRE, like a pedal. The amp distorts the effect. |
| Before tone / dynamics | After the amps and cabinets are merged, before TONE EQ and the dynamics block. |
| POST rack (default) | At the CREATIVE block's position in the POST rack. This is the released behaviour. |
| After POST | After the whole POST rack. |

A slot runs at one point only. Changing the placement fades the effect out
where it was (20 ms), moves it, and fades it in at the new point from clean
state. A slot that is off costs nothing and changes nothing at any point.

The routing graph keeps the CREATIVE block in the POST rack and says in its
detail text where the slot actually runs. Drawing the block at the other three
points in the graph is not done.

## Slicer patterns

The Gate Slicer steps through a pattern. Each step opens the gate to that
step's level for DUTY of the step, then closes it by DEPTH.

- `creativePattern`: Straight (the released gate), Offbeat, Gallop, Reverse
  gallop, Three-three-two, Clave, Dotted, Accent four, Ramp up, Ramp down,
  Syncopated, Burst, Custom.
- Custom reads sixteen step levels (`creativeStep1` … `creativeStep16`, 0–100 %)
  and a length of 2–16 steps (`creativeLength`).
- Dragging a bar of a built-in pattern copies that pattern into the Custom
  steps and switches to Custom, so any pattern is a starting point.
- `creativeAttack`: how fast a step opens, 2 ms at 0 % (released) to 42 ms at
  100 %. Steps always close in 2 ms.
- `creativeStereo`: Off, or Alternate (even steps open the left channel, odd
  steps the right).
- SYNC is the length of one step. With SYNC on a note value and the host
  playing, the step comes from the host's song position, so a pattern starts
  on the grid wherever playback starts. Without a playing host the pattern
  free-runs as before.

Straight with attack 0 % is the released gate sample for sample.

## Laser

The model is now called Laser Zap (it was Zap Sweep). `creativeLaserMode`:

| Mode | What a picked note does |
|---|---|
| Pitch dive (default for new instances) | Starts RANGE × 24 semitones above its pitch and falls onto it over the SWEEP time. |
| Pitch rise | Starts the same distance below and rises onto it. |
| Filter sweep | The released 8 kHz → 150 Hz band-pass sweep. Control C is RESONANCE. |

The pitch curve is `±24 × RANGE × (1 − t / sweep)²` semitones. After the sweep
the input passes unshifted (10 ms blend). SWEEP is 20–600 ms, TRIGGER is the
onset threshold, as before.

The pitch modes use the same two-head delay-line shifter as the harmonizer,
with 25 ms windows. At the start of a full-range dive the shifted sound is up
to 75 ms behind the input. This is how a delay-line shifter reaches a high
ratio; it is audible as a soft attack at large ranges.

## Compatibility

- All new parameters are appended at version hint 36 (22 per slot, 44 total).
  No released parameter changed its ID, range, default or choice list.
- A project saved before this change restores POST rack placement, the
  Straight pattern and the Laser's Filter sweep in both slots, so it sounds
  the same. Only a new instance starts with the Laser on Pitch dive.
- Two display names changed: "Zap Sweep" is "Laser Zap", and the slicer's
  description mentions patterns. Parameter IDs did not change.
- One behaviour change for released settings: a synced slicer under a playing
  host now takes its phase from the song position. Before, the phase depended
  on when the slot was last reset.

## Not done

- No tape-stop or other new glitch modes; Glitch Stutter is unchanged.
- No playhead on the step grid.
- No per-step pitch or filter (the "harmonic" slicer types of some pedals).
- The routing graph does not move the CREATIVE block to the chosen placement.
- Factory presets do not use the new features.
- DE / KO manual text for the page is not written.

## Measurements

From `ChimeraCreativeTests` on Linux (x86-64), this branch. See the test output
for the exact numbers of a run.

- Straight pattern equals the released gate formula on every sample of 1 s.
- Built-in patterns (Offbeat, Gallop, Clave, Ramp up) and a 3-step Custom
  pattern: each step's level within 0.01 of the pattern.
- Alternate: even steps left only, odd steps right only.
- Host position: starting 2.5 steps into the bar plays the right step at once.
- Laser dive and rise: measured pitch within 8 % of the law at 50–90 ms and 5 %
  at 180–220 ms; after the sweep the output equals the input within 1e-4.
- Filter sweep mode equals the released model on every sample.
- Placement: one point at a time; a move completes within 60 ms with no step
  larger than the signal's own; off is sample-exact at all four points.
