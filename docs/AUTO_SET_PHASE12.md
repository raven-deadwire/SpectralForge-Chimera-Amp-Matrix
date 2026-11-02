# Auto Set — phase 12 (amp presence and resonance)

Phase 12 lets Auto Set set the amp head's power-amp knobs: **PRESENCE** (the
upper-mid bite) and **RESONANCE** (or **DEPTH**, the low-end weight). It adds
no listening time. It uses what the amp pass (phase 8, `AUTO_SET_PHASE8.md`)
already captured, and runs right after the amp's GAIN and tone knobs are set,
on the knobs phase 8 left. Earlier phases are described in
`AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE11.md`; locks and strength in
`AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has an **Amp presence and resonance**
step, on by default.

- **The results** get a **Presence + resonance** row, right after the
  **Amp gain + tone** row. It lists each knob moved (before and after), then
  how far the bite and the low end sit from the style's target, before and
  after. Its SHOW button opens the amp head.
- **Strength** scales each knob change: before + strength × change.
- **Lock → Presence + resonance** keeps both knobs. With the Amp step on,
  the amp pass still runs; with both locked (or the Amp step off and this
  step locked), the amp pass is skipped.
- **UNDO AUTO** restores both knobs.
- The amp pass runs when either step runs. Its prompt says which knobs it
  listens for.

## Where it applies

As for phase 8: the Classic routing, with the amp switched on and its head
panel (native engine) on. Dual and Matrix routing, the classic amp engine and
a switched off amp are kept, and the row says why. If the amp, channel or
input jack changes while the pass listens, nothing is changed.

- **The knobs:** the first visible knob on the current channel labelled
  PRESENCE (else LEAD PRESENCE), and RESONANCE (else DEPTH).
- **Heads without them:** a knob the channel does not have is left alone. A
  head with neither says so. Heads with PRESENCE A/B behind a selector, and
  pull-shift switches, are not set.

## Measurement (`analysePower`, `AutoSetPower.h`)

The first 3 seconds of the playing entering the amp are rendered offline
through a fresh copy of the native amp, once for every candidate.

- **The amp's knobs:** each render uses the knob values the amp step applied
  (GAIN, tone), plus the candidate PRESENCE and RESONANCE.
- **The cabinet:** the amp/cab output minus the amp rendered at the knobs
  that were playing. A candidate's spectrum plus this difference predicts
  what is heard, as in phase 8.

Two balances are measured on the predicted third-octave spectrum, each
against the same difference of the style's Tone EQ target:

| | Guitar | Bass |
|---|---|---|
| **Bite** (PRESENCE) | 2–5 kHz against 400 Hz–1.6 kHz | 1.25–3.15 kHz against 400 Hz–1.6 kHz |
| **Low end** (RESONANCE) | 63–160 Hz against 400 Hz–1.6 kHz | 40–100 Hz against 400 Hz–1.6 kHz |

With guitar at rock or metal, too much low end counts double: a loose low end
smears the distortion.

## Rules

- **Order:** PRESENCE first, then RESONANCE with the new PRESENCE.
- **Tolerance:** a balance within 1.5 dB of the target keeps its knob.
- **Search:** otherwise the knob steps toward the target in 5% steps, up to
  30% of its range from where it started.
- **Shared reference:** both balances use the same 400 Hz–1.6 kHz reference,
  so moving one knob also moves the other balance a little. A candidate that
  pushes the other balance past the tolerance, or past where it already was,
  pays for that difference too.
- **Threshold:** the knob moves only if the best candidate improves its
  balance by more than 1 dB.
- **Snap:** values snap to 1% of the knob's range.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --power`):

- **Rules (`powerRules`)**, on four native heads (Tight 515, Cinder 120,
  Night Harvest and Hot Lead), at 44.1 and 48 kHz:
  - On each head, PRESENCE raises the bite and RESONANCE raises the low end.
  - Silence reports no playing.
  - A +9 dB shelf above 2.5 kHz never raises PRESENCE, and a −9 dB shelf
    never lowers it.
  - A +9 dB shelf below 120 Hz never raises RESONANCE.
  - Every move stays within the limit and brings its own balance closer. The
    two balances together never get worse.
  - A knob missing from the channel is left alone.
  - The loose low-end weighting applies to driven guitar only.
- **Controller (`controllerPower`)**, real processor, Tight 515, fixtures fed
  to the capture taps:
  - The step runs alone in the amp pass. PRESENCE at maximum is lowered, and
    the applied value matches the analysis.
  - At 50% strength the knob moves half-way.
  - Its own lock keeps the knobs and skips the pass. The Amp lock alone keeps
    GAIN but still runs this step.
  - With the Amp step on, both rows come from one pass in order, and this
    step renders on the new GAIN.
  - Dual routing is kept, and UNDO AUTO restores everything.
- **Full run (`controllerIntegration`):** thirteen result rows, with the
  Presence + resonance row after the amp row.
- **Locks (`controllerEveryLock`):** all fifteen locks at four strengths.

## Not yet validated

The bands, the 1.5 dB tolerance, the 1 dB threshold, the 30% move limit and
the doubled low-end cost are starting values. They have been checked on
synthetic plucked notes through native heads (without a cabinet in the
controller fixture), not on real guitars, basses or listening tests.

PRESENCE also changes how hard some heads' power stages clip; the step judges
the spectrum only, not the drive. The offline render runs at the session rate
without the amp's oversampling. The analysis adds up to 14 renders of 3 seconds
(two references and at most six candidates per knob) to the end of the amp
pass, on the message thread.
