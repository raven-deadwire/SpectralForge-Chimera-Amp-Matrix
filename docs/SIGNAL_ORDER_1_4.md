# Signal order editor 1.4 — first region: the common block

Status: **first implementation on `claude/signal-order`**, covered by
`ChimeraCommonOrderTests`. Roadmap item: 1.4 "first constrained signal-order
editor" (checkpoint 2027-01-15: order-editing preview with save/restore and
undo). Design authority: the PR #36 specification
(`docs/SIGNAL_PATH_EDITOR.md` at `e97f825`), "Constrained order editor".
Source baseline: main `cfeaa53`.

## 1. Scope of this step

The spec lists five first editable regions. This step implements one:

| Region (spec) | This step |
|---|---|
| Common pre-POST block: Tone EQ, clean dynamics, multiband | **Implemented**, with the de-esser, which sits in the same block |
| PRE board: reorder installed pedals | Already reorderable with its own undo (`movePedal`, `preorder`/`gainorder`); unchanged |
| Existing POST slots | Not yet |
| Final block (Final EQ → trim → limiter) | Fixed by the spec |
| Input gate / transpose / tuner, amp and CAB | Not reorderable by the spec |

The block keeps its boundaries: after the rig sum and the post-rig gates,
before POST. Nothing moves across the rig split, and no parallel bus or
feedback is possible.

## 2. Order, identity and state

- Four stages with stable identities `toneEQ`, `cleanComp`, `multiband`,
  `deEsser`. The released order is `toneEQ,cleanComp,multiband,deEsser`.
- The order is a ValueTree property `commonOrder` on the processor state,
  **not a host parameter**. Moving a stage swaps no parameter bank and
  redirects no automation; every parameter keeps its ID and value (tested).
- Projects, A/B slots and the full state carry the property (the comparison
  slots copy the state tree). A tree without it — every project and A/B slot
  saved before this change — opens in the released order, also into a
  processor whose order had been changed. A damaged value (unknown, missing or
  repeated identity) also falls back to the released order.
- Factory presets are authored in the released order and set it explicitly.
- The audio thread reads a packed atomic mirror (2 bits per position).

## 3. Commands and undo

- `moveCommonStage(stage, ±1)` swaps a stage with its neighbour; a move off
  either end is refused and not recorded.
- `undoCommonOrder()` / `undoCommonOrder(true)` (redo): a 32-step history of
  order changes, separate from the pedal board's. A new move clears redo.
  Restoring a project or A/B slot and loading a factory preset clear it.
- Every order change notifies the host of a non-parameter state change.
- UI: in the ROUTING graph, the four common-block blocks are drawn in their
  saved order and chained in it. Right-click opens Move earlier / Move later
  / Undo / Redo; Alt+Left / Alt+Right moves the focused block. Other blocks
  offer no order commands. The compact strip is unchanged. Drag and drop
  belongs to 1.5.

## 4. Audio graph and transition

`ChimeraProcessor::process` runs the block as a loop over the saved order.
Each stage keeps its own input tap (Auto-Set capture) and spectrum capture
scope wherever it sits, so a dynamics editor always shows its own stage's
input and output.

A reorder changes which signal feeds every stateful stage. The change
therefore fades the block's output out (10 ms), swaps the order while
silent, keeps the output muted for 10 ms while the stages settle on their new
inputs, then fades in (10 ms). A further change while settling swaps again; a
change undone during the fade-out fades back in. With no change pending the
block is processed untouched. Restoring state or a reset applies the order at
once, without a fade. None of the four stages adds latency, so the reported
latency does not change.

## 5. Measurements (`ChimeraCommonOrderTests`, 48 kHz, 128-sample blocks)

- Graph: Tone EQ +12 dB bell at 110 Hz with a hard Clean Comp (−40 dB, 20:1)
  on a 110 Hz tone. EQ > Comp −38.97 dB, Comp > EQ −28.42 dB RMS; the
  compressor's input tap reads −14.81 dBFS peak after the EQ and −24.11 dBFS
  when first.
- Click safety: four successive reorders on that held tone keep the output's
  second difference within 0.66–1.0× the larger steady value of the two
  orders. An instant swap (fade removed) measures 30.5× and fails the test.
- No allocation in the callback across reorders.

## 6. Not done

- POST slot reordering, and its tail and latency policy.
- Listening on real DI; DAW automation, save and recall with a changed order.
- Pointer and context-menu interaction in a native window (the test drives
  the command handler and the graph's model directly).
