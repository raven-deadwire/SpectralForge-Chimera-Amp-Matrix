# Auto Set — phase 26 (room ambience)

Phase 26 lets Auto Set set the Room Core room (`ROOM_CORE_1_3_3.md`): one
shared room the rigs are sent into, after the rig sum and before Tone EQ. It
adds no listening time: like phase 11 (`AUTO_SET_PHASE11.md`) it reads the
playing the input pass already heard. Earlier phases are described in
`AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE25.md`; locks and strength in
`AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has a **Room ambience (Room Core)**
step, listed after the cabinet steps. It is **off by default**: it adds an
effect to the rig that was not there, and its amounts have not had a
listening test yet. Turn it on in the options to let Auto Set set the room.

- **When it runs:** right after the input pass and the rig passes (amp,
  cabinet, Dual, Matrix), before the Tone EQ pass, so the Tone EQ, dynamics
  and output passes hear the room. Without the input pass (Input gain and
  Gate both locked) it is kept, with that reason.
- **What it sets:** Room on, size, absorption and return; and, only when every
  send of the routing is at 0, those sends (to 50%).
- **What it leaves alone:** sends the player has set, Room Advanced (balance,
  decay, diffusion, wet cuts, width), the declared space and its geometry, and
  the POST reverb (phase 11).
- **The results:** a **Room ambience** row after the cabinet rows: "room on:
  Large, absorption 40%, return -8.0 dB (guitar clean: 1.0 detected notes/s,
  sparse)", or why it kept the room. Its SHOW button opens the cabinet.
- **Strength:** see below.
- **Lock → Room ambience:** keeps the room.
- **UNDO AUTO:** restores it.

## Rules (`analyseRoom`, `AutoSetRoom.h`)

The playing is measured as in phase 11 (`measurePlayingFeel`): **dense**
above 6 detected notes a second, **sparse** below 2.5.

| | Clean | Rock | Metal |
|---|---|---|---|
| Size, guitar | Medium | Small | Small |
| Absorption, guitar | 40% | 55% | 70% |
| Return, guitar | -10 dB | -14 dB | -20 dB |

- **Bass:** a Small room, 10% more absorption and a return 6 dB lower, so the
  low end stays defined; sparse bass playing gets at most a Medium room.
- **Dense playing:** one size smaller, 15% more absorption, a return 4 dB
  lower, so notes do not blur. **Sparse playing:** one size larger and a return
  2 dB higher.
- **A room the player has on** keeps its size, absorption, sends and Room
  Advanced; only its return moves toward the rule:
  before + strength × (rule − before), to 0.5 dB.
- **A room that is off** is opened with the rule. At a strength below 100% its
  return is lower by 20 log10(strength), the way a mix scales (50%: 6 dB,
  25%: 12 dB); a return that would fall below the control's -36 dB keeps the
  room off.
- **Declared space on:** the declared room's geometry and materials replace
  Size and Absorption in the engine, so the step does not write them.
- **Sends:** a send is the player's. When all of the routing's sends (Classic
  1, Dual 2, Matrix 3) are at 0 the room would be silent, so the new room opens
  those to 50% (the parameter default).
- **Kept, with the reason:** locked; no input pass; too few notes; the strength
  too low to open the room; Room switched by hand during the run; a room that
  is on whose return is already at the rule.
- An unknown instrument (Detect, not settled by the input pass) is treated as
  guitar, as in phase 11.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --room`):

- **Rules (`roomRules`):** silence is not playing; sparse, steady and dense
  guitar and bass in each style get the table's room; the new-room strength
  scaling, the return's range and its rounding.
- **Controller (`controllerRoom`, real processor):**
  - The step is off by default (controller and options panel).
  - Sparse clean guitar opens a Large room, 40%, -8 dB, sends kept; 50%
    strength opens it at -14 dB; 25% on sparse metal bass keeps it off.
  - Sends all at 0 are opened in Classic (send 1) and Dual (sends 1 and 2),
    and a send the player set stays.
  - The player's room keeps its size, absorption and sends, and its return
    moves halfway at 50%. A declared space keeps its size and absorption.
  - A room switched by hand during the run, the lock and a missing input pass
    are kept; the row comes before the Delay + reverb row; UNDO AUTO restores
    the room.
  - **Through the processor (measured; wet relative to the dry rig, the same
    notes with the room off and on):** sparse clean (Large, 40%, -8 dB)
    -15.7 dB, steady rock (Small, 55%, -14 dB) -27.1 dB, dense metal (Small,
    85%, -24 dB) -42.4 dB. The test asserts only the order clean > rock >
    metal.
- **Locks (`controllerEveryLock`):** all twenty-nine locks at four strengths.

## Not yet validated

Checked on synthetic notes through the default rig, not with a real DI or a
listening test. The amounts are starting values; at -42 dB the dense metal
room is close to inaudible on purpose (a dry tone), and may prove too little.
The step does not hear the room it sets: the Tone EQ and output passes that
follow do.
