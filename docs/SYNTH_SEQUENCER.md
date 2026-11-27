# SYNTH: step sequencer, macros and eight more matrix rows

Status: implemented on the `claude/synth-sequencer` branch, stacked on
`claude/synth-filters`. Measured by `ChimeraSynthTests` on Linux. Not heard
through a real instrument or a keyboard yet, not checked in a DAW, and not
compared with Serum, ArgWave or any other synth.

## Why

The owner's direction for SYNTH is a synth in the class of Serum or ArgWave 3.
After the wavetables and the second filter, the modulation side was the gap:
two LFOs and an envelope as the only moving sources, and filter 2, the
distortion and the warp out of the matrix's reach. This step adds a step
sequencer, four macros, and matrix rows that know them and the newer targets.
The effect rack, the preset browser and wavetable import are not in this
change.

## Step sequencer

- Up to **16 steps**, each from -100 % to +100 %. It is a modulation source,
  "Step seq": it moves whatever a matrix row connects it to. It plays no notes.
- **Step length:** 1/1, 1/2, 1/4, 1/8, 1/16 (default), 1/32, 1/8 T, 1/16 T, in
  time with the host tempo (120 BPM when the host gives none).
- **STEPS:** how many steps play before the pattern repeats (1-16).
- **SMOOTH:** 0 % jumps from step to step. Higher values glide; at 100 % the
  glide takes the whole step.
- **RETRIG** (on by default): a note played while nothing is held starts the
  pattern at step 1. Off: the pattern keeps running.
- One sequencer for the whole synth, like the LFOs: every voice hears the
  same step.
- It follows the tempo, not the host's position: after a stop or a jump in
  the host it is not re-aligned to the bar. RETRIG aligns it to the playing.

## Macros

- Four controls, **MACRO 1-4**, 0-100 %. Each is a modulation source. Connect
  one to several targets and it moves them together.
- They are ordinary host parameters, so they can be automated or mapped to a
  controller in the host.
- A macro change is smoothed over 5 ms.

## Eight more matrix rows

- The matrix has **16 rows**. Rows 1-8 are the released ones, with the
  released sources and targets. Rows 9-16 are new and also offer:
  - sources: **Step seq**, **Macro 1-4**;
  - targets: **Cutoff 2**, **Resonance 2**, **Filter balance**, **Dist
    drive**, **Dist mix**, **Warp A / B / C**.
- Rows 1-8 cannot offer these: a longer list would change what their saved
  automation values mean.
- `+ ADD MODULATION` and a knob's right-click menu use rows 9-16 first, so a
  new row offers everything. Rows 1-8 are used when 9-16 are full, and are
  shown when a project or a factory patch uses them.
- The right-click menu on a knob now lists Step seq and the macros. Filter
  2's CUTOFF and RES have the menu too.
- Depth at 100 %: Cutoff 2 six octaves (as Cutoff); the others the control's
  whole range.

## On the page

- The LFO panel has three views: **LFO**, **SEQ**, **MACRO**.
  - SEQ: the steps as bars around a centre line. Drag across them to draw;
    double-click a step for 0. Steps beyond STEPS are dimmed; the step that
    is playing is lit. Below: STEPS, step length, RETRIG, SMOOTH.
  - MACRO: four bars.
- The matrix shows the rows in use, in a list that scrolls when they do not
  fit (about eight fit).
- Narrow menus (matrix rows, step length) draw smaller text and a smaller
  arrow, so names such as "Dist drive" are not cut.

## Parameters and compatibility

- Forty-eight parameters appended at version hint 40, after the filter bank:
  `synthSeqSteps`, `synthSeqRate`, `synthSeqSmooth`, `synthSeqRetrigger`,
  `synthSeqStep1`-`16`, `synthMacro1`-`4`, and `synthModB1`-`8` `Source` /
  `Target` / `Amount`. All automatable.
- Defaults: every step 0, every macro 0, every new row off. With no new row
  in use the voice is unchanged (tested sample for sample with steps and
  macros set).
- A released source on a new row behaves as on a released row (tested: Env 2
  to Cutoff on row 3 and on row 13, difference 0).
- No released parameter changed.

## Not done

- The sequencer does not follow the host's bar position, and has no swing,
  no per-step glide and no pattern presets.
- Bars on the page (warp amount, the distortion's DRIVE and MIX, the filter
  balance) have no right-click menu; choose them as a target in a row.
- Modulated bars show no mark; only knobs do.
- Rows 1-8 keep their short lists.
- No factory patch uses the sequencer or a macro.
- DE / KO manual text is not written.

## Measurements

From `ChimeraSynthTests` on Linux (x86-64), 48 kHz, this branch.

- Steps and macros set, no row connected: largest sample difference 0.
- Step seq to Amp, steps 0 % / -100 %, 1/16 at 120 BPM (6000 samples a
  step): step 1 -10.9 dBFS, step 2 silent, step 1 again -10.9 dBFS. At
  60 BPM the first step lasts twice as long. Three steps and 1/32 behave the
  same way.
- SMOOTH 100 %: just after the step change the level is 1.9 dB under step 1;
  at the end of the step it is 31 dB under.
- RETRIG, a note 160 ms in (inside step 2): on, the note sounds at once; off,
  it is silent until step 1 comes round.
- Macro 1 at 100 % with an amount, against the control set there by hand,
  largest sample difference: Cutoff 2 0, Resonance 2 1e-7, Dist drive 1e-6,
  Dist mix 0, Warp A 0, Filter balance 0.
- Macro 1 to Amp at -100 %, the macro jumped from 0 to 100 % inside a note:
  largest step between samples 0.012 (no click), then silence.
- Page at 1140 x 438: the step graph is 126 x 93 px. A drag from step 4
  (upper quarter) straight to step 7 (lower quarter) writes steps 4-7 as
  51 / 17 / -17 / -51 %, one gesture per step: the steps passed over are
  filled on a line. With all sixteen rows in use the list (288 px) scrolls
  inside its 139 px panel.

Screenshots: `docs/evidence/synth-sequencer-20261126/`.
