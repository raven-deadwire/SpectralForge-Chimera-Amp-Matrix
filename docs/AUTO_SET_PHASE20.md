# Auto Set — phase 20 (Amp channel)

Phase 20 lets Auto Set choose the amp head's channel. A channel's GAIN knob
covers only part of the drive range: a clean channel cannot reach a metal
drive, and a lead channel cannot stay clean. Phase 8 (`AUTO_SET_PHASE8.md`)
sets GAIN on the channel that is selected, and says "out of this knob's
reach" when the target is beyond it. This step first moves to a channel that
can reach the style's drive, so phase 8 then sets GAIN and tone there.
Earlier phases are described in `AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE19.md`;
locks and strength in `AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as
in the earlier phases.

## Use

The options button next to **AUTO SET** has an **Amp channel selection**
step, on by default, listed before **Amp gain and tone**.

- **When it runs:** in the amp pass, before the amp step. It adds no
  listening time. Where it applies is as for phase 8: the Classic routing,
  the amp switched on, the head panel (native engine) on. Otherwise the row
  says why. A head with one channel says "this head has one channel".
- **What it sets:** the head's CHANNEL.
- **What it leaves alone:** the knobs (the amp and presence + resonance steps
  set them, on the channel this step chose), the head, its input jack.
- **The results:** an **Amp channel** row, before the **Amp gain + tone**
  row, shows the channel before and after, the drive each reaches, and the
  style's drive target. When the channel stays, the row gives the current
  channel's reach. Its SHOW button opens the amp head.
- **Strength:** not applied. A channel is switched or kept.
- **Lock → Amp channel:** keeps the channel. Locking Amp, Presence +
  resonance and Amp channel skips the amp pass.
- **UNDO AUTO:** restores the channel.

## Measurement (`analyseChannel`, `AutoSetAmp.h`)

The amp pass's capture (phase 8): the first 3 seconds of the playing entering
the amp, and the amp/cab output.

- **Each channel** of the head is rendered offline through a fresh copy of
  the native amp: at the channel's own knobs, with its GAIN at the top of its
  range, and with GAIN at 10% of its range. Each channel's knobs are its own
  stored values (per-channel heads keep a set for each channel).
- **Drive:** phase 8's measure (how much of the playing's level spread the
  amp removes), on each render. A render too quiet to count as playing is left
  out: many heads are nearly silent with GAIN near zero, and silence would
  read as fully flattened. The lowest and highest drive of a channel's renders
  are its reach.
- **Target:** phase 8's drive target for the instrument and style (guitar
  15% clean, 45% rock, 65% metal; bass 10%, 30%, 50%), with its ±8% tolerance.

## Rules

- **Kept** when the current channel reaches the target (within ±8%).
- Otherwise the channel whose reach comes closest to the target is chosen,
  and only when it is closer than the current one by more than 8%. Ties keep
  the lower channel number.
- The amp step then sets GAIN and tone on the new channel, and the presence +
  resonance step its PRESENCE and RESONANCE. Both still estimate the cabinet
  from the head as it played (the amp/cab output minus the old channel
  rendered), so the switch does not count as a cabinet change.
- If the head, its channel or input jack changes while the pass listens,
  nothing is changed.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --channel`):

- **Rules (`channelRules`), 44.1, 48 and 96 kHz,** on the synthetic amp of
  phase 8's tests, with channels made by limiting its GAIN range:
  - A clean channel (GAIN 0–3, drive 0–3% at 48 kHz) moves to a lead channel
    (GAIN 7–10, drive 75–97%) for metal, and the lead channel moves to the
    clean one for clean.
  - A channel that reaches the target stays; rock on the lead channel stays,
    since the clean channel is not closer.
  - The reach is the rendered drive, and contains the channel's own drive.
  - A silent channel is never chosen; one channel never moves; of three
    channels the closest is chosen. Silence reports no playing.
- **Controller (`controllerChannel`), real processor, the default head
  (model 2: RHYTHM reaches 5–75% drive, LEAD 42–90%):**
  - Alone, clean on LEAD moves to RHYTHM in the amp pass; rock on LEAD stays.
  - 50% strength still switches.
  - With the amp step: the channel row comes first, and the amp step works on
    RHYTHM (its drive before is RHYTHM's).
  - The lock keeps the channel and skips the pass. A one-channel head and Dual
    routing are kept with the reason. The channel edited while listening voids
    the step.
  - Metal on model 22's CLEAN (0–34%) moves to LEAD (8–67%).
  - UNDO AUTO restores the channel.
- **Full run (`controllerIntegration`, Classic, rock):** twenty-one result
  rows, with the channel row after the boost row. The applied channel matches
  the analysis, and the amp step sets GAIN on that channel. In this run the
  input pass raises the input 10.5 dB and the boost engages, so LEAD reaches
  only 83–98% drive; the step moves to RHYTHM (36–98%), and the amp step then
  brings the drive from 81% to 50% (target 45%).
- **Locks (`controllerEveryLock`):** all twenty-three locks at four strengths.

## Not yet validated

The ±8% tolerance, the 10% GAIN probe and the rule "switch only when the
current channel cannot reach the target" are starting values. They have been
checked on a synthetic amp, the default head and one other head, with a
synthetic DI, not on every head, a real DI or listening tests. A channel is
judged by its drive only: channels also differ in voicing, which the tone
knobs and the Tone EQ then work on.
