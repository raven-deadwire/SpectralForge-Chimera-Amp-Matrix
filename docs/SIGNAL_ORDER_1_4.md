# Signal order editor 1.4 — common block and POST rack

Status: **implementation on `claude/signal-order`**, covered by
`ChimeraSignalOrderTests`. Roadmap item: 1.4 "first constrained signal-order
editor" (checkpoint 2027-01-15: order-editing preview with save/restore and
undo). Design authority: the PR #36 specification
(`docs/SIGNAL_PATH_EDITOR.md` at `e97f825`), "Constrained order editor".
Source baseline: main `cfeaa53`.

## 1. Scope

The spec lists five first editable regions:

| Region (spec) | State |
|---|---|
| Common pre-POST block: Tone EQ, clean dynamics, multiband | **Implemented**, with the de-esser, which sits in the same block |
| Existing POST slots | **Implemented**: Bus Comp, Preamp, Console EQ, Modulation, Delay, Reverb |
| PRE board: reorder installed pedals | Already reorderable with its own undo (`movePedal`, `preorder`/`gainorder`); unchanged |
| Final block (Final EQ → trim → limiter) | Fixed by the spec |
| Input gate / transpose / tuner, amp and CAB | Not reorderable by the spec |

Each region keeps its boundaries: the common block stays after the rig sum
and post-rig gates and before POST; the POST rack stays before the doubler,
click and Final EQ. Nothing moves across a region boundary or the rig split,
and no parallel bus or feedback is possible.

## 2. Order, identity and state

- Common block identities `toneEQ`, `cleanComp`, `multiband`, `deEsser`;
  released order `toneEQ,cleanComp,multiband,deEsser`.
- POST identities `busComp`, `preamp`, `consoleEQ`, `modulation`, `delay`,
  `reverb` (the rack's section indices 0–5); released order in that sequence.
- Each order is a ValueTree property (`commonOrder`, `postOrder`) on the
  processor state, **not a host parameter**. Moving a stage swaps no parameter
  bank and redirects no automation; every parameter keeps its ID and value
  (tested).
- Projects, A/B slots and the full state carry both properties (the
  comparison slots copy the state tree). A tree without them — every project
  and A/B slot saved before this change — opens in the released orders, also
  into a processor whose orders had been changed. A damaged value (unknown,
  missing or repeated identity) also falls back to the released order.
- Factory presets are authored in the released orders and set them
  explicitly.
- The audio thread reads packed atomic mirrors (3 bits per position).
- POST slots keep their identity everywhere else: native/legacy section
  selection, the per-slot meters (`postModuleLevel(section)`) and editor
  navigation follow the slot, not its position.

## 3. Commands and undo

- `moveCommonStage(stage, ±1)` and `movePostStage(stage, ±1)` swap a stage
  with its neighbour; a move off either end is refused and not recorded.
- `undoOrder()` / `undoOrder(true)` (redo): one 32-step history of order
  changes across both regions, separate from the pedal board's; each step
  restores both orders. A new move clears redo. Restoring a project or A/B
  slot and loading a factory preset clear it.
- Every order change notifies the host of a non-parameter state change.
- UI: in the ROUTING graph, both regions are drawn in their saved order and
  chained in it. Right-click on one of their blocks opens Move earlier / Move
  later / Undo / Redo; Alt+Left / Alt+Right moves the focused block. Other
  blocks offer no order commands. The compact strip is unchanged. Drag and
  drop belongs to 1.5.
- POST page: each slot sits in the row of its actual position, numbered 1–6
  in the left margin, with ▲/▼ buttons that move it (disabled at either end;
  the arrows are not drawn dimmed). The page re-lays itself when the order
  changes in ROUTING, by undo or by a state restore.

## 4. Audio graph and transition

`ChimeraProcessor::process` runs the common block as a loop over its saved
order, and `PostFXChain::process` runs the POST rack over `FXState::postOrder`.
Each common stage keeps its own input tap (Auto-Set capture) and spectrum
capture scope wherever it sits, so a dynamics editor always shows its own
stage's input and output.

A reorder changes which signal feeds every stateful stage. Each region
therefore has its own switch: it fades the region's output out (10 ms), swaps
the order while silent, keeps the output muted for 10 ms while the stages
settle on their new inputs, then fades in (10 ms). A further change while
settling swaps again; a change undone during the fade-out fades back in. With
no change pending the region is processed untouched. Restoring state or a
reset applies the orders at once, without a fade.

Tails: Delay and Reverb keep running through a POST reorder, so their tails
continue in the new order, but they are silent for the ~30 ms transition. The
spec allows this ("do not promise complete preservation of a tail across
every new route"); a seamless tail relocation is not claimed.

Latency: none of the common stages adds latency. In POST only the Preamp
does, a fixed amount in a serial chain, so the total reported latency does not
change with the order.

## 5. Measurements (`ChimeraSignalOrderTests`, 48 kHz, 128-sample blocks)

- Common graph: Tone EQ +12 dB bell at 110 Hz with a hard Clean Comp (−40 dB,
  20:1) on a 110 Hz tone. EQ > Comp −38.97 dB, Comp > EQ −28.42 dB RMS; the
  compressor's input tap reads −14.81 dBFS peak after the EQ and −24.11 dBFS
  when first.
- POST graph: Console EQ +12 dB at 1 kHz and a hard Bus Comp (−48 dB, 12:1,
  legacy modules) on a 1 kHz tone. The Console EQ meter minus the Bus Comp
  meter is 1.5 dB with the compressor first and 28.5 dB with the EQ first.
- Click safety, output second difference relative to the larger steady value
  of the two orders:
  - common block, four reorders: 0.66–1.0× (instant swap: 30.5×, fails);
  - POST, Console EQ +12 dB at 150 Hz and Bus Comp on a 150 Hz tone, four
    reorders: 0.72–1.0× (instant swap: 19–22×, fails); the same with Delay
    and Reverb running: 0.71–1.27×.
- No allocation in the callback across reorders in either region.

## 6. Not done

- Listening on real DI; DAW automation, save and recall with a changed order.
- Pointer and context-menu interaction in a native window (the test drives
  the command handler and the graph's model directly).
- The POST page's move arrows are not drawn dimmed when disabled.
