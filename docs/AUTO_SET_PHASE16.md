# Auto Set — phase 16 (bus compressor)

Phase 16 lets Auto Set set the POST rack's bus compressor as **glue**: a few
dB of gain reduction on the loud passages. It uses a slow attack, so picking
transients pass, and an auto release. The compressor set is the native
**Console VCA** (POST section 1, model 0), the default bus model.

It adds no listening time. It is a result step of the de-esser and limiter
pass (phase 3), which now also captures the POST rack's input (`Tap::post`).
That pass runs just before the Final EQ and output pass, so the output trim
hears the new compressor. Earlier phases are described in `AUTO_SET_PHASE1.md`
… `AUTO_SET_PHASE15.md`; locks and strength in `AUTO_SET_LOCKS_STRENGTH.md`.
The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has a **Bus compressor (glue)** step,
on by default.

- **The results:** a **Bus compressor** row follows the de-esser and limiter
  row. It shows the settings (or the threshold and make-up change) and the
  glue before and after, against its target. Its SHOW button opens the POST
  bus section.
- **Strength:** scales the glue target. At 50% the compressor aims for half
  the gain reduction.
- **Lock → Bus compressor:** keeps it as it is.
- **UNDO AUTO:** restores it, including its bypass.
- **When the pass runs:** the de-esser and limiter pass runs when either
  step does. Locking only the de-esser and the limiter keeps the pass running
  for the bus compressor.

## Where it applies

- **Engine and model:** the POST bus section on the native engine with the
  Console VCA selected. The legacy POST engine, another bus model, and the
  EXTERNAL SIDE-CHAIN switch on are kept, and the row says why.
- **Edits while listening:** if the bus section's engine or model changes
  while the pass listens, nothing is changed.

## Measurement (`analyseBus`, `AutoSetBus.h`)

**The render.** The first 4 seconds of the POST input are rendered offline
through the plug-in's own Console VCA (`PostNativeDSP`), on both channels,
with make-up at 0.

**The two measures.** Both are taken per 20 ms block, after a 0.5 s warm-up,
from the input level minus the output level:
- **Glue:** the mean gain reduction over the blocks within 10 dB of the
  loudest playing (95th percentile).
- **Mean reduction:** over all playing blocks. It sets the make-up, so the
  loudness holds.

## Rules

- **Target glue:**

  | Clean | Rock | Metal |
  |---|---|---|
  | 1.5 dB | 2.5 dB | 3 dB |

  The target is × strength.
- **A compressor that is out (bypassed):** it is switched in with glue
  settings:

  | | Clean and rock | Metal |
  |---|---|---|
  | Ratio | 2:1 | 4:1 |
  | Attack | 30 ms | 10 ms |
  | Release | AUTO | AUTO |

- **A compressor that is in:** it keeps your ratio, attack and release.
- **The threshold:** it is bisected (eight steps, −48 to 0 dB, snapped to
  0.5 dB) to reach the target. If the compressor is in, this happens only when
  its glue is more than 0.5 dB from the target. A target the threshold cannot
  reach is reported.
- **Make-up:** the mean reduction at the new threshold, snapped to 0.5 dB,
  0–20 dB.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --bus`):

- **Rules (`busRules`), 44.1, 48 and 96 kHz, plucked notes with soft and hard
  picking:**
  - Gain reduction rises as the threshold falls.
  - For each style, an off compressor is switched in with that style's
    settings, its glue within 0.5 dB of the target, and make-up on the 0.5 dB
    grid.
  - A compressor already at that threshold is kept.
  - A squashing one (−40 dB, 10:1) gets a higher threshold, keeping its
    ratio, attack and release.
  - Strength halves the target.
  - Silence reports no playing.
- **Controller (`controllerBus`), real processor, fixtures fed to the
  de-esser, output and POST taps:**
  - The step runs alone in the de-esser and limiter pass. The bypass, the
    controls and the make-up match the analysis.
  - 50% strength lightens the glue.
  - Its own lock keeps it and skips the pass. The de-esser and limiter locks
    alone keep the pass for it.
  - The legacy engine, another model, the side-chain and a model change while
    listening are all kept.
  - UNDO AUTO restores everything.
- **Full run (`controllerIntegration`):** seventeen result rows, with the Bus
  compressor row after the de-esser and limiter row.
- **Locks (`controllerEveryLock`):** all nineteen locks at four strengths.

## Not yet validated

The glue targets, the style settings and the 10 dB loud window are starting
values. They have been checked on synthetic plucked notes, not on real
guitars, basses or listening tests.

**Approximations**
- The POST input is captured during the same pass as the de-esser, before the
  de-esser and limiter step applies its own changes.
- The render feeds the same mono signal to both channels.
- A POST order that puts the preamp or EQ before the bus compressor is heard
  through the tap at the POST input, ahead of those modules.
