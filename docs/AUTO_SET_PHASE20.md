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
step, listed before **Amp gain and tone**. It is **off by default** (since
2026-11-07, see *Safety review*): turn it on to let Auto Set switch channels.

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
- **Strength:** a channel is switched or kept, but the choice counts only
  the part of the GAIN move the amp step will make at that Strength.
- **Amp gain and tone off or locked:** the amp step will not set GAIN, so each
  channel is judged at its own knobs only.
- **Lock → Amp channel:** keeps the channel. Locking Amp, Presence +
  resonance and Amp channel skips the amp pass.
- **UNDO AUTO:** restores the channel.

## Measurement (`analyseChannel`, `AutoSetAmp.h`)

The amp pass's capture (phase 8): the first 3 seconds of the playing entering
the amp, and the amp/cab output.

- **Each channel** of the head is rendered offline through a fresh copy of
  the native amp: at the channel's own knobs, and with its GAIN at 0, 5, 15,
  40, 70 and 100% of its range. Each channel's knobs are its own stored values
  (per-channel heads keep a set for each channel).
- **Drive:** phase 8's measure (how much of the playing's level spread the
  amp removes), on each render. A render too quiet to count as playing is left
  out: many heads are nearly silent with GAIN near zero, and silence would
  read as fully flattened. The lowest and highest drive of a channel's
  audible renders are its reach. A channel counts when any render is audible,
  even if its own stored GAIN is too quiet.
- **Target:** phase 8's drive target for the instrument and style (guitar
  15% clean, 45% rock, 65% metal; bass 10%, 30%, 50%), with its ±8% tolerance.

## Rules

- **Each channel is judged by the drive the amp step would leave it at.**
  - With the amp step running at 100%, that is the reachable drive closest to
    the target.
  - At a lower Strength, it is that fraction of the way from the channel's
    drive at its knobs.
  - With the amp step off or locked, it is the drive at its knobs.
- **Kept** when that leaves the current channel within ±8% of the target.
- **Otherwise** the closest channel is chosen, and only when it is closer than
  the current one by more than 8%. Ties keep the lower channel number. When
  the chosen channel still misses the target, the row says "still short: the
  closest channel".
- **Kept, saying why, when there is not enough to judge:**
  - the playing changes less than 3 dB between soft and hard notes (the
    drive measure reads 0 there, which is not the same as clean);
  - the current channel has fewer than two audible GAIN settings.
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

## Safety review (2026-11-07)

A review of this phase for a Drop A band (DEADWIRE) found:
- **Voicing is not preserved.** A channel is chosen by one drive number. Heads
  like Náströnd and Niflheimr differ per channel in asymmetry, stage gain
  distribution, low-end coupling, sag and the Niflheimr clean/low blend. A
  channel with a similar drive can still change palm-mute tightness, tremolo
  separation and lead sustain, which later EQ cannot restore.
- **The choice depended on GAIN moves that might not happen.** It assumed the
  full GAIN move even when Amp gain and tone was off, locked or at a lower
  Strength.
- **Information gaps.**
  - A channel was dropped when its stored GAIN was quiet.
  - GAIN below 10% was never probed.
  - "No soft/hard difference" read the same as "clean".

Changes:
- **The step is off by default.** Turn it on in the options when a switch is
  wanted. Lock → Amp channel still keeps the channel when the step is on.
- **The choice uses the drive the amp step will actually leave:** it accounts
  for Amp off or locked and for Strength, and probes GAIN at 0–100% in six
  points. A quiet stored GAIN no longer drops a channel.
- **It holds, and says why**, when the playing has too little soft/hard
  difference or the current channel has fewer than two audible GAIN settings.
  A switch that still misses the target is labelled.

**Still open:** the drive target per style is the only criterion. There is no
check of channel voicing, and channel ranges are measured only for Tight 515
and Blue Storm (synthetic playing). For finished tones, keep the step off or
lock Amp channel, and choose Guitar or Bass for down-tuned instruments.
