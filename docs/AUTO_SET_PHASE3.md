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
| Steps | Tone EQ (on), Multiband (on, phase 4: `AUTO_SET_PHASE4.md`), De-esser and limiter (on) |

With every step on, Auto Set listens in five passes of about 6 s each, in
signal order:
1. Input and gate.
2. Tone EQ.
3. Compressor.
4. Multiband (phase 4).
5. De-esser and limiter: keep playing, with some hard picking.

**UNDO AUTO** restores everything the run changed. Skewed parameters such as
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
| Spectrum | Same playing-frame third-octave spectrum as the Tone EQ, compared with the aligned instrument + style target |
| Split frequency | Lower edge of the band between 2.5 and 8 kHz that is furthest above the target, 2.5–9 kHz, 100 Hz steps |
| Spectral excess | Mean level above the target from the split to 10 kHz |
| Burst excess | High band (two 12 dB/oct high-passes at the split) against full band: 95th percentile of each 10 ms window's rise over the median of its ±50 ms neighbours. Pick clicks and fizz spikes count; a note's normal decay does not |
| On when | Spectral excess ≥ 2 dB (clean 3, metal 1.5), or burst excess ≥ 4 dB. Otherwise the de-esser is left off |
| Settings | Threshold at the 80th-percentile high-band window peak (−60…−6 dB); range = max(spectral, burst − 2) + 2 dB, 3–9 dB; ratio 4:1, attack 1 ms, knee 6 dB, release by style |

When the Tone EQ (±6 dB per move) cannot remove all the excess brightness,
the de-esser takes the remaining harsh peaks dynamically.

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
- Short, high-frequency pick clicks over it are de-essed, with the metal
  release.
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
three styles and loudness targets are not part of Auto Set. The multiband
compressor was added in phase 4.
