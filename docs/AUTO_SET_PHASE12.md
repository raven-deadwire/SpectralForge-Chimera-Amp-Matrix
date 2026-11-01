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
| **Low end** (RESONANCE) | 63–160 Hz against 400 Hz–1.6 kHz | 25–100 Hz against 400 Hz–1.6 kHz |

Bass uses a Phase 12-only spectrum with additional 25, 31.5 and 40 Hz
third-octave centres. The original shared spectrum starts at 50 Hz: its former
nominal 40–100 Hz selection actually contained only 50, 63, 80 and 100 Hz.
The new bass FFT has approximately 1.5 Hz bins and quarter-window hops so
A0 (27.5 Hz) and B0 (30.8677 Hz) are measured consistently at 44.1, 48 and
96 kHz. Knob decisions can still differ at the hard 1 dB acceptance
boundary; see the sample-rate comparisons in the validation report.
The original short-window playing gates still decide whether there
is sufficient playing; the longer FFT does not impose a new note-duration
requirement. Guitar and the other Auto Set phases keep their existing spectra.

The existing bass target is reused, including its constant extrapolation below
40 Hz. This is a provisional, synthetic-tested target extension, not a
listening-approved bass voicing. The upper edge remains 100 Hz rather than
including additional 125/160 Hz body bands.

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
  pays for that difference too. In addition, the sum of both weighted errors
  may not increase. Staying within the other balance's tolerance does not
  authorize a worse combined result.
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

### Extended-range bass regression

`ChimeraAutoSetTests --power-bass-rules` covers known-amplitude A0/B0 and
800 Hz references, neutral and positive correction controls, a coupled-control
counterexample, one second of playing followed by two seconds of silence, and
native Niflheimr inputs with a strong fundamental and weak harmonics 2–8.
All of these run at 44.1, 48 and 96 kHz. Removing the combined-error guard
makes the deliberately worsening interaction control fail.

`ChimeraAutoSetTests --power-bass-controller` runs 57 synthetic controller
cases on Tight 515 and Niflheimr: A0/B0, the three sample rates, 25/50/100%
Strength, independent Power and Amp locks, UNDO AUTO, manual edits during and
after the capture, and a newly selected undo lock. Both regression groups also
run in the full test executable.

`ChimeraAutoSetPowerBassProbe` produces an observational CSV using all six
heads whose default channel exposes both controls, plus Niflheimr's other
four channels. It compares dry native output and an authored Bass 4x10 cabinet
kernel, and independently measures the fundamental, 25–40/40–100/100–250 Hz
power, mid reference, RMS/peak/crest factor and control interaction. These are
synthetic fixtures, not instrument DI or measured-cabinet recordings.

The reproducible baseline/candidate results, alternative-band comparison and
limitations are recorded in
[`AUTO_SET_PHASE12_BASS_VALIDATION_20261101.ko.md`](AUTO_SET_PHASE12_BASS_VALIDATION_20261101.ko.md).

## Not yet validated

The bands, the 1.5 dB tolerance, the 1 dB threshold, the 30% move limit and
the doubled low-end cost are starting values. They have been checked on
synthetic plucked notes through native heads (without a cabinet in the
controller fixture), not on real guitars, basses or listening tests.

The result row's dB values describe the full proposed knob move; at partial
Strength (or after a manual edit prevents a write), they are not a new render
of the actual applied values. The controller regression checks the applied
parameter interpolation and ownership separately.

PRESENCE also changes how hard some heads' power stages clip; the step judges
the spectrum only, not the drive. The RMS-spread and crest measurements do not directly count internal clipping.
The offline render runs at the session rate
without the amp's oversampling. The analysis adds up to 14 renders of 3 seconds
(two references and at most six candidates per knob) to the end of the amp
pass, on the message thread.
