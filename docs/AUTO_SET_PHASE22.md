# Auto Set — phase 22 (Delay tempo sync)

Phase 22 lets Auto Set turn on the delay's tempo sync, following the host.
Phase 11 (`AUTO_SET_PHASE11.md`) adds a delay only with a sync tempo: onset
gaps do not establish a metrical tempo, so without DELAY SYNC it keeps the
delay off and says "select DELAY SYNC". When the host reports a tempo, this
step selects it for the player. Earlier phases are described in
`AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE21.md`; locks and strength in
`AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has a **Delay sync to the host
tempo** step, on by default, listed before **Delay and reverb**.

- **When it runs:** inside the delay + reverb step (after the passes, before
  the Final EQ and output pass), first. It adds no listening time. With the
  delay + reverb step off or locked there is nothing to time, and the row
  says so.
- **What it sets:** **DELAY SYNC** on and **Follow host tempo** on.
- **What it leaves alone:** Chimera's own TEMPO, the delay time, feedback and
  mix (phase 11 sets the delay), the reverb.
- **The results:** a **Delay tempo sync** row, before the **Delay + reverb**
  row, shows the host tempo it synced to, or why it kept the settings. The
  delay + reverb row then shows the delay it added ("delay on: host tempo
  90.0 BPM ...").
- **Strength:** not applied. Sync is turned on or kept.
- **Lock → Delay tempo sync:** keeps both switches.
- **UNDO AUTO:** restores them.

## Rules (`analyseTempoSync`, `AutoSetSpace.h`)

- **Host tempo:** the tempo the host reported in the latest audio callback,
  40–240 BPM (the range phase 11 accepts). The processor now records it
  whether or not Follow host tempo is on (`reportedHostTempo`).
- **Turned on** only when all hold:
  - the delay is off, and DELAY SYNC is off (a delay with its own time, or a
    sync the player chose, manual/TAP or host, is kept);
  - the host reports a tempo;
  - phase 11 would add a delay at that tempo, from the playing the input
    pass heard: not bass, not metal, no phrase faster than 4 notes a second.
- The delay + reverb step then uses the host tempo just read (the processor
  reports a followed host tempo from its next callback).
- **Kept, with the reason:** locked; no playing heard; sync already on; a
  delay already on; no host tempo ("set Chimera's TEMPO or TAP and select
  DELAY SYNC"); bass, metal style or a fast phrase; sync edited while
  applying.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --sync`):

- **Rules (`syncRules`):** sparse clean guitar with a 90 BPM host tempo wants
  a delay (667 ms, mix 0.18). No tempo, 30 BPM, 300 BPM and NaN are not used.
  Bass, metal and a fast phrase (5 notes a second) want none. Silence reports
  no playing.
- **Controller (`controllerSync`), real processor with a host that reports a
  tempo:**
  - At 90 BPM, sync and Follow host tempo go on, the row comes before the
    delay + reverb row, and that step adds the delay at 90 BPM.
  - No host tempo: kept, the delay stays off and phase 11 still says "select
    DELAY SYNC".
  - Sync already on, a delay already on, and metal: kept.
  - The lock keeps the switches; with Delay + reverb locked the row says so.
  - UNDO AUTO restores them.
- **Full run (`controllerIntegration`, no host tempo):** twenty-three result
  rows, with the sync row (kept, no host tempo) before the delay + reverb row.
- **Locks (`controllerEveryLock`):** all twenty-five locks at four strengths.

## Not yet validated

Checked with a test host that reports a fixed tempo, not in real DAWs. The
delay follows the host tempo from then on, also when the host changes it,
which is what Follow host tempo does elsewhere in Chimera. A quarter-note
delay is what phase 11 sets; other subdivisions are not chosen.
