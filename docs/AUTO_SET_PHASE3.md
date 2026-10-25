# Auto Set — phase 3 (style, de-esser, limiter)

Phase 3 completes Auto Set (phases 1 and 2: `AUTO_SET_PHASE1.md`,
`AUTO_SET_PHASE2.md`). It adds a playing style that shifts the targets, and
a last pass that sets the de-esser and the output limiter. It is rule-based
like the earlier phases.

## Use

Dynamics → **COMPRESSOR** or **GATE / EXPANDER**. The options button next to
**AUTO SET** (it shows e.g. `Auto / Rock`) opens a menu:

| Section | Choices |
|---|---|
| Instrument | Detect from playing (default), Guitar, Bass |
| Style | Rock (default), Clean, Metal |
| Steps | Tone EQ (on), De-esser and limiter (on) |

With every step on, Auto Set listens in four passes of about 6 s each, in
signal order:
1. Input and gate.
2. Tone EQ.
3. Compressor.
4. De-esser and limiter: keep playing, with some hard picking.

**UNDO AUTO** restores everything the run changed. It restores only parameters actually written by that run, and preserves later
manual/host edits. Parameters edited while listening are not overwritten.
Skewed parameters such as
the de-esser frequency come back within float precision.

## Order

Pass 4 listens to two taps at once, after the compressor and Tone EQ
settings are in place:

- **De-esser input** (after the clean compressor and multiband): mono
  samples, sharing the lock-free sample ring with the Tone EQ tap.
- **Limiter input** (after the output trim): 10 ms peak/RMS windows.

The audio thread only copies or reduces samples, allocation-free. Capture
listens through a tap bitmask, so one window tap can run alongside the
sample tap.

## Style

The style adds an offset to the instrument's target curve (phase 2), and
sets the de-esser release and the limiter release:

| | Target offset | De-esser release | Limiter release |
|---|---|---|---|
| Rock | none | 80 ms | 100 ms |
| Clean | +1 dB at 2.5 kHz, +2 dB from 5 kHz (more air) | 120 ms | 150 ms |
| Metal (guitar) | −3 dB to 80 Hz (tight), −2 dB at 500–800 Hz, +1.5 dB at 2.5–4 kHz, −3 dB at 8 kHz (fizz) | 50 ms | 60 ms |
| Metal (bass) | −2 dB at 50 Hz and 500 Hz, +2 dB from 2 kHz (growl) | 50 ms | 60 ms |

## De-esser rules (`analyseFinish`)

| Step | Rule |
|---|---|
| Spectrum | Third-octave spectrum from original, contiguous FFT frames outside protected note attacks; at least eight eligible playing frames required |
| Instrument uncertainty | Explicit Guitar/Bass uses its original style target. Detect mode requires spectral excess above both aligned targets; an inferred Tone EQ label is not treated as a user choice |
| Split frequency | Lower edge of the band between 2.5 and 8 kHz that is furthest above the target, 2.5–9 kHz, 100 Hz steps |
| Spectral excess | Mean level above the target from the split to 10 kHz |
| Note attacks | A ≥3 dB full-band rise AND ≥6 dB body rise (1.5 kHz, 24 dB/oct low-pass), versus the preceding 50 ms median, protects the previous 10 ms window and the current + following five windows. Startup is also protected |
| Burst excess | Outside protected attacks, pair each high-band rise with the same window's nonnegative full-band rise, then take the 95th percentile. Require 20 eligible windows and high-band RMS above the loud full-band reference minus 45 dB |
| On when | Spectral excess ≥ 2 dB (clean 3, metal 1.5), or burst excess ≥ 4 dB. Otherwise existing de-esser settings, including enabled state, are left unchanged |
| Settings | Threshold at the 80th-percentile high-band window peak (−60…−6 dB); range = max(spectral, burst − 2) + 2 dB, 3–9 dB; ratio 4:1, attack 1 ms, knee 6 dB, release by style |

The spectral limits, range, ratio, threshold percentile and style releases are
unchanged. Overall attack-inclusive excess is retained as a diagnostic, but
cannot independently enable the de-esser. Fast palm-muted playing may leave
insufficient sustained evidence; Auto Set then abstains from spectral correction.
HF-only clicks over a steady body remain detectable. This is a conservative
rule, not a validated perceptual classifier.

## Limiter rules

The limiter is a sample-peak safety at **−1 dBFS**, turned on with the
style's release. The summary reports the 99.9th-percentile window peak at
the limiter input. Auto Set does not change the output trim: loudness stays
the player's choice.

## Verification

`ChimeraAutoSetTests`:
- Style offsets reach the target.
- Silence is rejected.
- A steady noise signal shaped like the target is left alone.
- The same signal 8 dB brighter above 4 kHz is de-essed.
- Short, high-frequency clicks over a steady body are still de-essed.
- 72 synthetic coincident note-attack cases (two seeds, timing offset, palm
  mute, soft clipping, three styles and sample rates) are left unchanged.
- 18 sustained-brightness positive controls and nine ambiguous auto-target
  controls retain explicit style/instrument behavior.
- 36 explicit-instrument checks cover Drop A, Low B and high-register bass,
  with/without soft clipping. These do not establish auto-classification accuracy.
- Manual enabled de-esser/listen values survive a no-excess result; apply/undo
  emits balanced host gestures; newer edits and untouched stages survive undo.
- Every setting stays within its limits at 44.1/48/96 kHz.
- The limiter's peak percentile, over-ceiling share and style release are
  correct.
- The two-tap capture works and is allocation-free.
- A four-pass run on the real processor: limiter and de-esser applied as the
  analysis says, host gestures balanced, undo, and the optional steps off
  giving two passes.

`CommonDynamicsUITests`:
- Option defaults and the button's text.
- With the optional steps off, Auto Set runs two passes.
- Listening and cancelling write no parameters.

## Not yet validated

The style offsets, the de-esser limits (2/3/1.5 dB spectral, 4 dB burst),
the ranges and the releases are starting values. They have not been tuned
against reference recordings or listening tests. Genre presets beyond these
three styles, loudness targets and the multiband compressor are not part of
Auto Set.

## Audit evidence (2026-10-25)

See [attack-safety audit](evidence/auto-set-attack-safety-20261025/README_KO.md).
Synthetic tests do not grant real-DI listening or DAW acceptance. The instrument
heuristic remains ambiguous for extended range and distorted inputs.
