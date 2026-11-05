# Auto Set — phase 15 (cabinet low and high cuts)

Phase 15 lets Auto Set set the cabinet's **LOW** and **HIGH** cuts on the
Classic routing's rig. These are the filters around the cab that keep rumble
and fizz out before the Tone EQ.

It adds no listening time. It is a result step of the cabinet mic pass (phase
9, `AUTO_SET_PHASE9.md`) and uses the same capture after the cab. It runs
after the mic placement, so the Tone EQ pass that follows hears the new cuts.
Earlier phases are described in `AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE14.md`;
locks and strength in `AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in
the earlier phases.

## Use

The options button next to **AUTO SET** has a **Cabinet low and high cuts**
step, on by default.

- **The results:** a **Cabinet cuts** row follows the cabinet mic row. It
  shows each cut moved, before and after, and how far the low end and the top
  sit from the target, before and after. Its SHOW button opens the cabinet.
- **Strength:** scales each cut's change in Hz.
- **Lock → Cabinet cuts:** keeps both cuts.
- **UNDO AUTO:** restores them.
- **When the pass runs:** the cabinet mic pass runs when either the mic step
  or this step runs. Locking only the mic keeps the pass running for the cuts.

## Where it applies

- **Routing and cabinet:** the Classic routing with its cabinet on. The cuts
  apply to captured IRs and filters as well as to modeled mics. Dual and
  Matrix routing, and a switched-off cabinet, keep the cuts, and the row says
  why.
- **Edits while listening:** if the cabinet or mic changes while the pass
  listens, nothing is changed. The same goes for a cut you edit while it
  listens.

## Rules (`analyseCuts`, `AutoSetCabCuts.h`)

**How a new cut is predicted.** The cuts are second-order Butterworth filters
(`Cab::setCuts`), so a new cut's effect is known exactly. Each third-octave
band moves by the new cut's power response minus the current one. This is the
digital filter's own response, which bends near Nyquist, averaged over eight
points per band. The heard spectrum plus those changes predicts what the Tone
EQ pass will hear.

**The mic move.** If phase 9 moved the mic in the same pass, its band change
(and the level that holds the loudness) is added first.

**What each cut is judged on.** The target is the Tone EQ target, aligned at
250 Hz – 1 kHz. Each cut is judged on the RMS distance from it over its own
bands:

| Cut | Bands | Range | Limit |
|---|---|---|---|
| LOW | guitar 50–160 Hz, bass 50–100 Hz | 20 Hz up | guitar 80 Hz (metal 100 Hz), bass 40 Hz |
| HIGH | 5–10 kHz | guitar from 4 kHz, bass from 2.5 kHz | 20 kHz, or 0.45 × rate |

**The search.**
- **Candidates:** nine log-spaced values from half to twice the current cut,
  inside the range. The LOW cut snaps to 5 Hz and the HIGH cut to 100 Hz.
- **A cut above its limit** can come down to the limit, even if that is less
  than half.
- **When it moves:** a cut moves only when its region's fit improves by more
  than 0.5 dB. Each run moves a cut by an octave at most. A large excess
  settles over a few runs.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --cuts`):

- **Rules (`cutsRules`), 44.1, 48 and 96 kHz:**
  - The band model matches noise rendered through JUCE's cut filters within
    1 dB per band. This includes the bilinear warping of a 6 kHz HIGH cut at
    44.1 kHz.
  - A +12 dB boom below 100 Hz raises a 30 Hz LOW cut within the guitar
    limit, and re-runs settle.
  - A 160 Hz LOW cut comes down to the limit.
  - +12 dB fizz above 5 kHz lowers a 14 kHz HIGH cut by up to an octave, on
    the 100 Hz grid. A 3 kHz HIGH cut rises.
  - Metal and bass keep their LOW limits.
  - A mic move that lifts the lows is counted.
  - Silence reports no playing.
- **Controller (`controllerCuts`), real processor, fixtures fed to the cab
  tap:**
  - The step runs alone in the cabinet mic pass, and both applied cuts match
    the analysis.
  - 50% strength moves them half-way.
  - Its own lock keeps them and skips the pass. The mic lock alone keeps the
    pass for the cuts.
  - A cut edited while listening is kept.
  - Dual routing and a switched-off cabinet are kept.
  - UNDO AUTO restores everything.
- **Full run (`controllerIntegration`):** sixteen result rows, with the
  Cabinet cuts row after the cabinet mic row.
- **Locks (`controllerEveryLock`):** all eighteen locks at four strengths.

## Not yet validated

The band regions, the limits, the octave move and the 0.5 dB threshold are
starting values. They have been checked on synthetic noise with filtered
boom, thin lows, fizz and dull tops, not on real guitars, basses or listening
tests.

The LOW cut is judged against a target spectrum, not against the notes
actually played. A drop-tuned guitar's low notes below 80 Hz are protected
only by the limit.
