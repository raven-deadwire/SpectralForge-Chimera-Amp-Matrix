# Auto Set — phase 8 (amp gain and tone)

Phase 8 lets Auto Set set the amp head itself: the knobs Easy view calls
GAIN, BASS, MIDDLE and TREBLE. The amp shapes everything after it, so this
pass runs right after the input pass, at the new input gain, and the Tone EQ
pass that follows corrects what is left. Earlier phases are described in
`AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE7.md`; locks and strength in
`AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has an **Amp gain and tone** step, on
by default. It adds one pass of about 6 seconds after the input pass. Play as
you normally would, soft and hard notes, so the pass hears your dynamics.

- **The results** get an **Amp gain + tone** row: each knob moved (before and
  after), then how much the amp flattens your dynamics, before, after and the
  style's target. Its SHOW button opens the amp head.
- **Strength** scales each knob change: before + strength × change.
- **Lock → Amp** keeps the knobs and skips the pass (it still runs for the
  Presence + resonance step, phase 12, `AUTO_SET_PHASE12.md`).
- **UNDO AUTO** restores the knobs.

## Where it applies

- The Classic routing, with the amp switched on and its head panel (native
  engine) on. Dual and Matrix routing, the classic amp engine and a switched
  off amp are kept, and the row says why.
- The knob each role edits is the one Easy view binds for the head, channel
  and input jack (`EasyMode.h`). A role missing from the current channel is
  left alone.
- If the amp, channel or input jack changes while the pass listens, nothing is
  changed.

## Measurement

The pass captures two signals at the same time:

- **Amp input** (`Tap::amp`, a new sample tap with a ring of its own): the
  signal entering the amp, after the pre-amp effects.
- **After the cab** (`Tap::tone`): the amp/cab output before the Tone EQ.

The first 3 seconds of the amp input are rendered offline through a fresh
copy of the same native amp DSP, once for every candidate setting. Each
decision is therefore checked on your own playing, through the actual head.

- **Drive:** the level spread of the playing (90th − 20th percentile of
  20 ms block levels, blocks within 30 dB of the loudest) at the amp input
  and at the rendered output. Drive = 1 − output spread / input spread: 0 keeps
  your dynamics, 1 flattens them.
- **The cabinet:** the third-octave spectrum after the cab minus the spectrum
  of the amp rendered at the current settings. Everything after the amp
  (cabinet, microphone, level) is in this difference, so a candidate's
  spectrum plus the difference predicts what the Tone EQ pass will hear.

## Rules (`analyseAmp`, `AutoSetAmp.h`)

| | Guitar | Bass |
|---|---|---|
| Drive target, clean | 15% | 10% |
| Drive target, rock | 45% | 30% |
| Drive target, metal | 65% | 50% |

- **GAIN:** when the drive is more than 8 points from the target, bisect
  the knob (7 steps) toward it. It is kept unless the result is closer. A
  target the knob cannot reach is reported ("out of this knob's reach").
- **BASS, MIDDLE, TREBLE:** a coordinate search (steps of 30%, 15% and
  7.5% of the range, each knob within 30% of where it started). It minimises
  the distance of the predicted spectrum from the Tone EQ target (the same
  target, alignment and one-octave smoothing, 80 Hz – 6.3 kHz). The tone
  knobs change only when the distance drops by more than 0.5 dB.
- **Then GAIN again:** the tone stack also changes the drive, so GAIN is
  re-set with the new tone.
- Knob values snap to 1% of their range.

## Testing

`ChimeraAutoSetTests`:
- **Rules (`ampRules`), at 44.1, 48 and 96 kHz, on a stand-in amp** (tanh
  drive, three tone filters):
  - The drive measure is near 0 for an almost linear amp and above 60% for
    a hard one.
  - Silence reports no playing.
  - From GAIN 0, rock reaches its target. Metal drives harder, and clean backs
    off from GAIN 10.
  - Near the target, GAIN stays.
  - The tone knobs move toward the target, within the limit, and GAIN is
    re-set after them.
  - A role missing from the panel is left alone.
- **Native probe (`ampNativeProbe`):** on Glass, Tight 515, Chime 30 and
  Náströnd the drive rises with GAIN. The full analysis is timed: 0.4 – 2
  seconds per head on the CI machine.
- **Controller (`controllerAmp`), real processor, Tight 515, fixtures fed to
  the capture taps:**
  - The pass runs alone. A low GAIN is raised toward the rock target and the
    applied value matches the analysis.
  - At 50% strength the GAIN moves half-way.
  - The lock keeps the knobs and skips the pass.
  - Dual routing and the classic engine are kept.
  - UNDO AUTO restores everything.
- **Full run (`controllerIntegration`):** seven passes and nine result rows,
  with the amp pass second, on processBlock. The default head's GAIN matches
  the analysis.
- **Locks (`controllerEveryLock`):** all eleven locks at four strengths.

## Not yet validated

The drive targets, the 8-point tolerance, the 30% tone limit and the 0.5 dB
threshold are starting values. They have been checked on synthetic plucked
notes, not on real guitars, basses, other heads or listening tests.

The drive measure looks at dynamics only; it does not tell fizz from warm
saturation. The offline render runs at the session rate without the amp's
oversampling. The analysis runs on the message thread at the end of the pass
and takes up to about 2 seconds on the Náströnd head.
