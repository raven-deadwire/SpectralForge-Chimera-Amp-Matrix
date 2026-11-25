# POST: four slots per section, add an effect anywhere

Status: implemented on the `claude/post-four-slots` branch, stacked on the
CREATIVE page (`claude/creative-page`). Measured by `ChimeraSignalOrderTests`
on Linux. Not heard through a real instrument yet, and not checked in a DAW.

## Why

The owner's brief for POST: a new effect can be inserted anywhere in the
rack, and more than two of a kind can be stacked. The second slots
(docs/POST_SECOND_SLOTS.md) gave two of each kind. The owner chose "up to four
of each kind" for this step.

## What it is

Every POST section (POST dynamics, POST preamp, POST EQ, MODULATION, DELAY,
CREATIVE, REVERB) now has **four slots**. Slots 3 and 4 work exactly like
slot 2: their own model, controls, ON switch and DSP state, off by default.

The POST rack therefore orders **28 blocks**. Block `b` is section `b % 7` in
slot `b / 7`.

## Adding an effect

- **ROUTING graph:** `+ ADD POST EFFECT` opens a list of the seven kinds, with
  how many of each are in use. Choosing one switches on the next free slot of
  that kind and puts it at the end of the POST chain.
- **Insert anywhere:** right-click a POST block and choose "Insert an effect
  after this block". The new block goes right after it.
- **POST page:** `+ ADD` does the same as the graph's button and switches the
  row to the new slot.
- **Remove:** right-click a block of slot 2-4 and choose "Remove this block".
  This switches the slot off. Its settings stay, so adding it again brings
  them back.
- **Limit:** four of each kind. The menu greys out a kind that is full.

Adding is two ordinary edits: one order change (one undo step in the order
history), then the slot's ON switch. The block is placed before it is switched
on, so it never sounds at its old position.

## ROUTING graph

- Slot 1 blocks are always drawn. A block of slot 2-4 is drawn **only while it
  is on**. This is a change from 1.5, where a bypassed "DELAY 2" was drawn.
  The chain now shows what is in use.
- Drag, the placement menu and Alt + arrows work on the drawn blocks. Earlier /
  later step over unused slots, and the menu numbers positions by drawn block.
- Up to 27 blocks in the lower area use nine 108-wide columns, as before. With
  more, the columns get narrower so three rows always hold everything (tested
  with all 28 POST blocks on: no overlap, nothing outside 1140 x 700).

## POST page

- The slot switch on every row and on the CREATIVE strip has four buttons,
  `1 2 3 4`. A green light on a number: that slot is on.
- The switch is 66 px wide (it was 42), so row titles are 116 px wide (they
  were 140). Long titles are drawn slightly narrower.
- The CREATIVE page has four slot tabs.

## Parameters and compatibility

- **Version hint 37:** 582 parameters appended at the very end, after the
  CREATIVE page bank (hint 36).
  1. `orderPost15`-`orderPost28`. Not automatable, defaults 14-27.
  2. For slot 3, then slot 4: the same copy of every POST parameter as slot 2,
     with the prefix `slot3_` / `slot4_`, followed by that slot's CREATIVE
     page parameters. 284 parameters per slot.
- The plugin now has 7150 parameters.
- No released parameter ID was renamed, renumbered or removed.
- **One range change:** `orderPost1`-`orderPost14` (not automatable) accept a
  block number 0-27 instead of 0-13. Stored values keep their meaning. A
  project that places a slot 3 / 4 block among the first 14 positions, opened
  in 1.5, has an invalid order there and loads 1.5's default order.
- **Older projects** have no `slot3_` / `slot4_` values and no
  `orderPost15`-`28`. They load with slots 3 and 4 off, after everything else,
  and sound the same (tested: a 1.5 project with DELAY 2 moved and on is
  bit-identical).
- Slots 3 and 4 start with the Laser on Pitch dive, like any new slot. The
  "older project keeps the filter sweep" rule applies to slots 1 and 2 only,
  because only those existed before the CREATIVE page.

## Audio

- The processor runs four POST racks and four CREATIVE instances, interleaved
  block by block in the 28-block order.
- A slot that is off is skipped: no cost, no latency, signal untouched
  (tested sample-exact with slots 3 / 4 moved and their settings changed).
- **Latency:** each extra preamp adds its own latency only while it runs
  (tested: 42, 48, 54, 60 samples with 0-3 extra preamps at 48 kHz). The
  change happens at the same 5 ms silent switch as an order change.
- **Sidechain key:** a keyed POST dynamics block hears the key delayed by
  every preamp ahead of it (0 to 4).
- **CPU:** a slot that is on costs about as much as its slot 1 counterpart,
  so fourteen more blocks all on is a real load. Measured by
  `ChimeraWholeProductTests` on a shared Linux machine whose speed varies
  (one run, Matrix, OVERSAMPLING 4x, every module on with random settings):
  - 48 kHz / 64: about 84 % of one core with all four slots, about 70 % with
    slots 1 and 2 only.
  - 96 kHz / 128: 181-197 % with all four slots, 116-134 % with slots 1 and 2.
  - The suite's CPU gates (real time at 48 kHz, appended modules under three
    quarters of a core at 96 kHz) still apply to slots 1 and 2. The four-slot
    case is printed, not gated: it would not pass the 96 kHz budget.
- **Idle floor:** the released reverbs keep an idle floor near -114 dBFS
  (measured alone in slot 1, unchanged by this work). Several racks of gain
  after a reverb can lift it. The whole-product decay check therefore runs
  with slots 3 and 4 off; with them on it checks finite output under the
  limiter ceiling.
- **Memory and load time:** two more racks are prepared with the plugin. This
  was not measured.

## Not done

- The POST page still shows six rows, one slot per section at a time. It is
  not a free list of blocks.
- The CREATIVE strip's "AFTER … / BEFORE …" note names its neighbours in the
  full 28-block order, so it can name a slot that is off.
- Order presets order the sections; slots 2-4 follow in the same section
  order.
- No factory preset uses slots 3 or 4.
- DE / KO manual text is not written.
- Not checked in a DAW: parameter-list length (7150), automation of the new
  parameters, latency compensation when extra preamps switch.

## Measurements

From `ChimeraSignalOrderTests` on Linux (x86-64), this branch.

- Bank: 582 parameters at version 37 (14 order slots, 284 per slot), each with
  slot 2's range, default and automation flag.
- Four delays in series (300 / 120 / 200 / 60 ms): each slot adds its own echo.
- Unused slots 3 / 4, moved and re-set: largest sample difference 0.
- 1.5 project: largest sample difference 0 against the untouched project.
- Add: the next free slot, right after the chosen block; a fifth of a kind is
  refused; graph commands skip unused slots.
