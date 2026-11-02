# PRE board drag order — 1.5

Status: **implemented on `claude/pedal-drag-order`** (based on `main`).
Roadmap: 1.5 "drag-order UX". Covered by `ChimeraUITests` (pedal menus and
board state).

## Behaviour

- Each occupied card's title strip (the top 22 px, marked by six grip dots on
  the right) is a drag handle. The pointer becomes a dragging hand over it.
- Press and drag sideways. After 6 px the drag starts: the source card dims,
  a tag with the pedal's name follows the pointer, and an amber bar marks the
  slot the pedal will take. It sits before the target when moving left and
  after it when moving right. The notice line names the slot and repeats the
  LOW TAP warning.
- Release over a slot to move the pedal there. The pedals in between shift
  over by one, so this inserts rather than swaps.
- Releasing over the pedal's own slot, or outside the board, changes
  nothing. The notice says "Move cancelled." for a release outside.
- A press without 6 px of movement is a click and does not move anything.
- Empty slots and the detail view have no handle.
- The `<` / `>` buttons stay as the keyboard and precise alternative.

## Processor

`ChimeraProcessor::movePedalTo(owner, destination)` removes the owner from
the order and inserts it at the destination:

- Only the order positions that change are written, inside one
  `boardEditSequence` edit.
- It is one undo step, and owners keep their parameter banks and MIDI
  bindings.
- `movePedal(owner, ±1)` now calls it; an adjacent insert is the same as the
  old swap.
- Invalid owners or destinations, and a move onto the pedal's own slot, do
  nothing and record no undo step.

## Tests

- **Board state:**
  - Insert-move 0 → 3 shifts positions 1–3 left.
  - Owner banks are unchanged.
  - A move onto the pedal's own slot is a no-op.
  - 4 → 0 shifts the pedals right.
  - Each insert-move is one undo step.
  - Invalid moves leave the board unchanged.
- **Pedal menus**, with real pointer events on the panel:
  - The handle is not covered by a child component.
  - Dragging over slot 4 targets it, and the drop inserts the pedal there and confirms it in the notice.
  - Release outside the board, release over the pedal's own slot and a 3 px wobble change nothing.
  - Dragging back to slot 1 restores the order.
  - The drag is one undo step.
  - An empty slot cannot be dragged.
  - One mid-drag picture is written to `ui-snapshots/PRE-board-drag.png`.

## Not done

- **Not checked in a DAW:** a real mouse and the host's undo. Pointer events in the tests are synthesised.
- **Touch and pen input:** not tested.
- **Escape cancel:** not available (the board does not take keyboard focus during a drag).
