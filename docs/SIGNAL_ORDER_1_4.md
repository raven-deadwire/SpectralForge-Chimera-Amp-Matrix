# 1.4 constrained signal-order editor

Status: **implemented on `claude/signal-order-editor`**, stacked on
`claude/integration-1-10`; **1.5 drag** on `claude/drag-order-ux`, stacked on
it (see "Drag (1.5)"). This is the 1.4 "constrained order editor" stage
of [SIGNAL_PATH_EDITOR.md](SIGNAL_PATH_EDITOR.md) and
[DEVELOPMENT_ROADMAP_2026_2027.md](DEVELOPMENT_ROADMAP_2026_2027.md) (1.4:
"first constrained signal-order editor"). Covered by `ChimeraSignalOrderTests`
and the updated signal-path UI tests.

## What can move

Two serial regions can be reordered. Everything else keeps its fixed place,
as the specification's first editable regions require.

| Region | Modules | Fixed boundaries |
|---|---|---|
| **Tone / dynamics block** | Tone EQ, clean compressor, multiband, saturation, filter, de-esser | After the rig sum, the optional post-rig gate and the Room return; before POST |
| **POST rack** | POST dynamics, POST preamp, POST EQ, modulation, delay, CREATIVE, reverb | After the tone / dynamics block; before the doubler / click, Final EQ, output trim and limiter |

- **Fixed blocks:** the input gate, Transpose, PRE, amps, cabinets, Room sends
  and return, Final EQ, output trim and the limiter cannot be moved.
- **PRE board:** pedals keep the PRE board's own reorder, which existed
  before.
- **Out of scope:** moving across the rig split, parallel buses and feedback
  loops.
- **Default:** the released order, in both regions.

## How

The ROUTING graph's movable blocks take the order commands.

| Action | Result |
|---|---|
| Right-click a movable block | Placement menu: Move earlier / later, Move to the start / end, Place at position (lists the current order), Undo / Redo order change, Reset the block's order |
| Alt + Left / Up, Alt + Right / Down | Move the focused block earlier / later |
| Alt + Home / End | Move it to the start / end of its block |
| Ctrl (Cmd) + Z, Ctrl (Cmd) + Shift + Z | Undo / redo order changes |

- **Clicks:** a click still only opens the block's editor. Navigation never
  writes a parameter (the test checks this).
- **Fixed blocks:** they have no placement menu and ignore the keys.
- **Display:** the graph draws both regions in their saved order. Each block
  keeps its ID and editor wherever it sits.
- **New blocks:** SATURATION and FILTER now appear in the path. The editor and
  the path were missing them, although the audio ran them.
- **SYNTH insert:** "Before tone / dynamics" (formerly "Before Tone EQ") joins
  the first block of the region, whichever module that is.

## Drag (1.5)

The specification's 1.5 stage is "drag with valid drop targets, preview
placement, cancel, use keyboard and undo/redo; calls the already tested
order command; no DSP rebuild during hover".

- **Drag:** drag a movable block in the ROUTING graph more than 6 pixels.
  - Every position of its own block is outlined.
  - The position under the pointer is highlighted, with a caption such as
    "CLEAN COMPRESSOR > POSITION 5".
- **Drop:** releasing over a highlighted position is one placement command —
  the same command as the menu — and one undo entry.
- **Cancel:** the order stays as it was when the block is released away from
  its block's positions (over a fixed block such as FINAL EQ, over another
  region, or in empty space), or when Escape is pressed. The release after
  Escape does nothing.
- **No writes while hovering:** hovering writes no parameter and does not
  touch the audio.
- **Clicks:** a press that moves less than 6 pixels is still a click and opens
  the block's editor; a drag does not.
- **Keyboard and undo:** unchanged from 1.4 — Alt + arrows / Home / End, and
  Ctrl + Z / Ctrl + Shift + Z.
- **Evidence:** `SIGNAL_ORDER_SNAPSHOT=<dir>` makes `ChimeraSignalOrderTests`
  write the graph mid-drag.

## State, automation and undo

- **Parameters:** 13 slot parameters (`orderTone1`–`6`, `orderPost1`–`7`) at
  version hint 29, appended last.
  - Each slot holds the module in that position.
  - They are **not automatable** (order is not an automation target).
- **Stable identities:** a module keeps its own parameter bank. Moving it
  never exchanges banks or redirects automation. A move writes only slot
  parameters, each as one complete host gesture, and no other parameter
  changes (tested).
- **Recall:** project state, A/B slots, factory presets and undo all go
  through the slots.
  - A project or preset without order parameters loads the released order.
  - Restoring a project or preset clears the order undo history, as it does
    the PRE board's.
- **Undo / redo:** up to 64 order changes. Each command is one entry,
  including Reset.
- **Partial writes:** a region is applied only when its slots form a
  permutation. While an edit is half written (two slots briefly hold the same
  module), the audio thread keeps its previous order.

## Audio

- **Order:** the processor runs each region's modules in their slot order.
  - In the tone block, the Auto Set taps stay at each module's input, so Auto
    Set measures what each module hears.
  - In the POST rack, each stage meter reads its own module's output. DELAY's
    meter includes CREATIVE only when CREATIVE directly follows it, as before.
- **Click-safe change:** when a region's order changes during playback, its
  output fades out (5 ms, raised cosine). The order switches at the next block
  boundary in silence, then the output fades back in (5 ms).
  - Nothing else is interrupted: a POST reorder does not touch the tone block,
    and the other way round.
  - Before any audio has played, or while a recall holds the output muted, the
    order applies at once.
- **Tails:** tails already inside a module continue from that module's new
  position. The editor does not promise to carry every tail seamlessly to the
  new route, as the specification allows.
- **Latency:** no position changes the reported latency.

## Measurements (`ChimeraSignalOrderTests`, 48 kHz)

| Check | Result |
|---|---|
| Tone block: Tone EQ +12 dB at 1 kHz with a hard clean compressor, clean rig | EQ → comp −51.5 dBFS; comp → EQ −40.1 dBFS (11.4 dB, the boost) |
| POST rack: POST EQ +12 dB at 1 kHz with a hard bus compressor | comp → EQ (released) −34.2 dBFS; EQ → comp −45.6 dBFS (11.4 dB) |
| Neutral move (de-esser off), 220 Hz | Envelope below half its level for 1.6 ms. Second difference 4.6e-5 during the switch vs 4.2e-5 steady |
| Audible move (EQ after a hard compressor) | Second difference 3.2e-5 vs 2.6e-5 steady. A raw switch would step by about 0.023 |
| Move | Writes only slot parameters, each as a complete gesture; every other parameter unchanged. Moving past the end is refused |
| Undo / redo / reset | Exact |
| Project round trip | Same order, and the same audio sample for sample |
| A/B | Each slot keeps its own order |
| Old project (no order parameters) | Released order |
| Stale slots (a duplicate) | The audio keeps its last valid order |
| Drag | Hover previews positions 4 then 5 with no parameter writes; the drop lands at 5 as one undo step; Escape, FINAL EQ and empty space cancel; a 2-pixel press still opens the editor |
| Signal path | Edges follow the order. No block overlaps after a move. SYNTH joins the first block. Commands, Alt + Right and Ctrl + Z work; navigation writes nothing; fixed blocks have no menu |

The specification calls for a measurement of the transition's cost: the fade
costs one multiply per sample per channel in the switching region, and only
while switching.

## Not done

- **Interaction:** dragging on the compact strip (it has no per-module
  blocks), and touch-specific long-press handling.
- **Placements:** advanced ones, such as a whole-wet compressor after POST
  (1.9).
- **Checks:** pointer checks in hosts, listening to real tails during a move,
  and the EN/DE/KR manual pages.
