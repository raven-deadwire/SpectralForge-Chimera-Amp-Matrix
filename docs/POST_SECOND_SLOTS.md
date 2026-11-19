# POST second slots

Status: **implemented on `claude/post-second-slots`**, stacked on the POST
page drag (`claude/post-rack-drag`). Product decision "option A": every POST
section gets a second slot. Covered by `ChimeraSignalOrderTests` (the
`secondSlots…` cases) and the parameter-inventory checks in the existing
tests.

## What it is

The POST rack has seven sections: POST dynamics, POST preamp, POST EQ,
MODULATION, DELAY, CREATIVE and REVERB. Each now has **two slots**.

- **Independent:** slot 2 is a separate instance of the section. It has its
  own model, its own controls, its own ON / bypass switch and its own DSP
  state. Nothing is shared with slot 1. It can use any model of its section.
- **Example:** two delays at once (a short slap and a long echo), or a chorus
  before the delay and a rotary after the reverb.
- **Default:** every second slot starts **off** (bypassed), with its
  section's default model.

## On the POST page

- **1 / 2 switch:** every rack row has a small "1 | 2" switch at the right end
  of its title. The CREATIVE strip has one left of its caption. The switch only
  chooses which slot the row shows. It writes nothing and is not saved.
- **Green light:** a small green light on the "2" shows that the second slot
  is on, even while slot 1 is shown.
- **Slot 2 row:** it looks and works like slot 1, with the title "DELAY 2",
  "BUS COMPRESSOR 2", "CREATIVE 2" and so on. ALL opens slot 2's full
  controls.
- **Row order:** the rows keep the order of their slot 1 units, so switching
  between 1 and 2 never moves a row.
- **Drag:** a row's grip drags the unit that the row shows. Dropping it on
  another row takes the place of the unit shown there. For example, show
  DELAY 2, then drag it onto POST EQ: DELAY 2 now runs where POST EQ was.
- **Layout:** the layout keeps the existing six rows and the CREATIVE strip,
  so the page does not grow. It is tested at 75, 100, 125 and 150 % and in
  the small-screen viewport.

## Signal order

- **14 blocks:** the POST region of the order editor now holds 14 movable
  blocks: slot 1 and slot 2 of each section. Block `b` is section `b % 7` in
  slot `b / 7`.
- **Default order:** the released order, followed by the second slots in the
  same section order: DYN, PRE, EQ, MOD, DLY, CRE, REV, then DYN 2 … REV 2.
  - **Why at the end:** an old project then loads exactly as saved. Its
    seven stored positions keep their meaning, and the seven new positions
    take their defaults. No conversion is needed.
  - **No sound change:** bypassed second slots change nothing, wherever they
    sit.
- **Order presets:** a preset orders the seven sections. The second slots
  follow in the same section order.
- **Commands:** drag (POST page and ROUTING graph), the placement menu,
  Alt + arrows / Home / End, undo / redo (64 steps) and Reset all work on the
  14 blocks.
- **Stale slots:** a half-written order (two positions holding the same
  block) keeps the last valid order, as before.
- **ROUTING graph:** it shows all 14 blocks, so the chain now snakes over
  three rows. The graph is 1140 × 700. A second-slot block reads, for
  example, "DELAY 2", with its model as the status line, or BYPASSED.
  Clicking it opens the POST page with that row switched to slot 2.

## Parameters and compatibility

- **Version hint 35:** all new parameters are appended at the very end, after
  the Metro Clean TT-800 bank (hint 34, `docs/METRO_CLEAN_TT800.md`).
- **The bank:** 269 parameters (`postSecondSlotParameterCount()`).
  1. `orderPost8`–`orderPost14`. Not automatable, defaults 7–13 (the
     second slots).
  2. A copy of every POST parameter under the same ID with the prefix
     `slot2_`, for example `slot2_delaytime`, `slot2_pn_bus_m0_threshold` and
     `slot2_creativeOn`. This covers:
     - the released bus compressor, preamp, EQ, modulation, delay and reverb
       fields, including delay sync;
     - their model choices and model extensions;
     - the native POST dynamics, preamp and EQ banks (all six models each);
     - CREATIVE.
- **Same definitions:** each copy has slot 1's range, default and automation
  flag (tested). Host names end in "(slot 2)".
- **Released IDs:** no released parameter ID is renamed, renumbered or
  removed. Automation of existing parameters still targets slot 1.
- **One range change:** `orderPost1`–`orderPost7` (version 29, not
  automatable) now accept a block number 0–13 instead of 0–6. Stored values
  0–6 keep their meaning.
  - **Older versions:** a project saved with a second slot placed among
    `orderPost1`–`7`, then opened in an older version, gets an invalid order
    there, so that version loads its default order.
- **Old projects and presets:** they have no `slot2_` values and no
  `orderPost8`–`14`.
  - They load with every second slot off and the released order (plus the
    second slots after REVERB).
  - The audio is bit-identical (tested: largest sample difference 0).
- **Native engine:** the second slot's native engine switch starts on, as
  for a new project. Only slot 1 gets the old-project "legacy engine"
  migration.

## Audio

- **Two racks:** the processor runs two POST racks (`postFX`, `postFX2`)
  and two CREATIVE instances, interleaved block by block in the 14-block
  order.
- **Bypassed slot:**
  - A second slot that is off is skipped: it costs nothing and adds no
    latency, and the signal passes untouched (tested sample-exact).
  - After a slot is switched off it keeps running for 100 ms, so its own
    fade-out finishes. It is then skipped.
  - When it is switched on again it starts from silence (its state is
    cleared), then fades in through its own ramps.
- **Tails:** delay and reverb tails behave as in slot 1. While a slot is on,
  its tail rings on in place. Switching it off fades its output out.
- **Order changes:** they keep the 5 ms fade-out / switch / fade-in.
- **Latency:**
  - The only POST block with latency is the preamp (its oversampling,
    6 samples at 48 kHz).
  - The second preamp adds that latency only while it is on, and the plugin
    reports it then.
  - Because the timing shifts, switching the second preamp on or off goes
    through the same 5 ms fade as an order change. The latency change then
    happens in silence.
- **Sidechain key:** a keyed POST dynamics block hears the key delayed by
  every preamp that runs ahead of it (0, 1 or 2 preamps).
- **CPU:**
  - With every second slot off, the extra work per block is reading the
    second slot's parameters and a few switch checks. It is too small to
    show above timing noise in the tests.
  - Each second slot that is on costs about as much as its slot 1
    counterpart.
- **Meters:** each slot 2 row has its own output meter. The bus compressor
  spectrum in slot 1's detail view analyses slot 1 only.

## State

- **Recall:** project save / load, A/B snapshots, COPY and user presets
  store the second slots like any other parameter (tested round trip,
  sample-exact).
- **MATCH:** it adjusts the A/B levels only and does not touch the slots.
- **Factory presets:** they reset every parameter, so they start with every
  second slot off and in the default order.
- **Undo:** order changes, including second-slot moves, share the 64-step
  order undo. A recall clears it, as before.

## Auto Set

Auto Set leaves the second slots alone. Its passes write only slot 1 (the
POST bus compressor and slot 1 delay / reverb).

- **Bus pass:** the POST bus pass still requires POST dynamics (slot 1) to
  be the first of the 14 blocks. If a second slot is placed ahead of it,
  Auto Set keeps the bus settings and says why, as it does when another
  section is placed first.
- **Measurement:** Auto Set's output and loudness measurements hear
  everything that plays, including second slots that are on.

## Not done

- **Moving rows:** on the POST page a row cannot move by its slot 2 alone.
  The rows follow slot 1's order. Use the ROUTING graph to see where every
  slot 2 sits.
- **Easy view:** it shows slot 1 only.
