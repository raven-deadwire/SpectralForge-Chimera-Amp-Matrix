# Precision tuner development — 2026-10-13 (UTC)

Development branch based on main `b4930839a7384a411e6c37e3570d77069f41a024`.
This change is a tuner implementation and measurement prototype, not a release
approval or a claim of Peterson-equivalent real-instrument accuracy.

## Implementation and response budget

The frozen baseline class is in `Tests/Fixtures/Tuner20261013.h`. Its 2048 samples
at approximately 12 kHz give a 171 ms window at 48/96/192 kHz (186 ms at 44.1 kHz).
The 40 ms worker wait adds scheduling delay. The first-dip YIN search covers
25–1400 Hz; cubic fractional-delay refinement has interpolation and envelope
bias. A strong second harmonic can cause an octave error.

The new worker uses 4096 samples at approximately 24 kHz: **the same window
duration**, with a 10 ms wait. Intended analysis range is 16–2000 Hz. The callback
still takes clean input before the gate/transpose/effects and writes a bounded
SPSC queue. It does not allocate or estimate pitch and adds no audio-path delay.
Two one-pole anti-alias filters remain ahead of decimation (3 kHz cutoff). Their
limited stopband is a remaining qualification item, not an ideal anti-alias claim.

YIN seeds the period. Eight-point Lagrange interpolation plus normalized
correlation removes constant gain/DC differences between delayed segments.
The refinement search covers the biased coarse minimum of a decaying note.
Single-period residuals check 2×/3× alternatives before up-to-four-period
refinement; this avoids allowing an even number of half-periods to conceal a
second-harmonic octave error. A pure missing fundamental is intrinsically
ambiguous; the detector does not invent the absent string identity.

The former local UI/tuner prototype used an 8192-sample window, approximately
341 ms. Its tuner portion was reused and revised here; its signal-navigation,
footer redesign and PR #60 routing modifications are not part of this branch.
That overlapping UI/tuner work is now Draft PR [#66](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/66).
This branch isolates detector/latency work against main; it is not independently
mergeable alongside #66 without reconciling shared tuner files, parameter order
and UI ownership. No changes are pushed into #66, and none of its release
authorizations or passing measurements qualify this source.

On disable, queue overflow, input-channel change or invalid input, generation
checks prevent publishing an estimate across discontinuous histories. A stopped
input stream expires after 250 ms; stopping/preparing the worker clears readings.
Tuner-off and output mute behavior retain their existing host parameter meanings.

## Estimator options considered

| Approach | Decision for this increment |
| --- | --- |
| Existing YIN + cubic single-period error | Frozen comparator; retains upper-register and decay bias in measured fixtures |
| YIN + normalized fractional-delay refinement | Implemented: reuses acquisition logic, handles constant envelope gain changes, and permits source-matched A/B tests |
| Interpolated spectral peak / harmonic fit | Future cross-check; needs an explicit rule for weak/missing fundamentals and inharmonic partials before replacing the time-domain result |
| Phase tracker / PLL after acquisition | Candidate for faster updates on sustained notes; must validate pitch steps, vibrato and reacquisition before assigning a precision or latency specification |
| Direct sample-phase strobe | Separate future display architecture; present bands are frequency-estimate driven and must not be described as a phase-domain instrument |

This is an engineering selection, not a measured comparison of unimplemented
algorithms. A longer fixed frame was rejected for this increment because it
unnecessarily doubled acquisition time. Low-note period observation and actual
pluck settling remain distinct from the worker polling interval.

## Display and target identity

`TUNER VIEW` opens the existing nonmodal inspector at 1140×420 without changing
enable/mute. Existing TUNER toggle and signal-path entry remain. The inspector
provides STROBE/NEEDLE, A4 calibration, note/octave, measured Hz, cents, target Hz,
offset, profile and explicit string selection. The compact needle also evaluates
the selected profile; it must not silently display equal temperament in a
compensated session.

The original, estimated-frequency visual strobe integrates `(measuredHz -
targetHz) * dt` at 1×/2×/4×. It is not an optical or direct sample-phase strobe,
and does not copy Peterson artwork or implementation. Fine motion has no cent
dead band. Each displayed frame is limited to 0.2 stripe periods at large errors
to prevent apparent direction reversal from frame aliasing; numeric cents/Hz are
unchanged. Invalid readings dim the bands and show LISTENING. Display refresh is
30 Hz; kernel and worker measurements do not include GUI/monitor latency.

Target identity includes profile, MIDI note/octave, physical string and position:
open = fret 0; BFTS fretted octave = fret 12; chromatic/custom sounding-note
entries = unspecified fret (`-1`). Automatic selection chooses the nearest
available string target within 150 cents. A manually locked string deliberately
shows octave errors. Frequency alone cannot infer which string/fret was played.

| Profile | Data actually implemented | Explicit boundary |
| --- | --- | --- |
| Equal temperament | Zero-offset chromatic | Default for ordinary instruments and old projects |
| Equal / 5-string bass | G2 D2 A1 E1 B0, high to low | Zero offsets; not a compensated bass system |
| Equal / 6-string bass | C3 G2 D2 A1 E1 B0 | Same boundary; use Custom for BEAD/drop A |
| TT chromatic | Thidell Formula 1 published pitch-class values | Separate from open-string table |
| TT 6/7/8 strings | Published note+octave targets | High E and low E remain distinct |
| BFTS electric / Anderson open | Six standard-tuning electric-guitar values | Only the manufacturer's documented electric setup |
| BFTS electric / Anderson fretted 12 | Its six fretted-octave values | Never substitute harmonics or open offsets |
| Custom / target per string | Eight explicit sounding MIDI note + cent targets | User data; not certified BFTS/TT support |

No acoustic/12-string/BFTS bass, TT bass, alternate historical TT temperament,
unsourced alternate tuning or arbitrary fret table is supplied. Do not transpose
a manufacturer's compensation table into a different tuning and assume validity.
A later position-map importer should require schema/version, instrument/tuning,
string count, string+fret+MIDI keys, offsets, source URL/revision and permission
status; unmatched keys must remain unavailable rather than fall back to zero.

## Primary sources and licensing boundary

Retrieved 2026-10-13 UTC:

- [Peterson AutoStrobe 490](https://www.petersontuners.com/products/autoStrobe490/)
  and [support](https://www.petersontuners.com/support/): published 0.1-cent
  accuracy and directional/stationary strobe behavior define the reference goal,
  not a measured Chimera specification. No proprietary Sweetener database or
  firmware is copied.
- [True Temperament official tuning guide](https://truetemperament.com/how-to-tune/):
  published chromatic offsets and distinct 6/7/8-string open targets. C-based
  chromatic sequence: `+2,-4,+2,-4,-2,0,-4,+4,-4,0,-4,-1`. High-to-low open
  sequence: E4 −1, B3 −1, G3 +4, D3 +2, A2 0, E2 −2, B1 −4, F#1 −8 cents.
  These figures identify Thidell Formula 1, not every TT system.
- [Anderson Guitarworks](https://www.andersonguitarworks.com/tuning-tips):
  standard electric open E4/B3/G3/D3/A2/E2 offsets `0,+1,-2,-2,-2,-2`; fretted
  octave offsets `0,0,+1,+1,0,0`. Its inconsistent Drop-D octave line is excluded.
- [Peterson BFTS guide](https://www.petersontuners.com/myInstrument/buzzFeiten):
  open and fretted targets are different; use fretted notes for intonation.

Published numerical instructions establish the limited targets above; they do
**not** establish a license, endorsement or permission to redistribute a vendor's
full proprietary profile database. No commercial license was obtained or verified
here. The BFTS site timed out; no inference is made about current licensing terms.
Before commercial branded compatibility claims or additional proprietary tables,
obtain the relevant vendor's data/terms and verify instrument-specific behavior.
This is an unresolved product prerequisite, not a statement that all numeric
facts necessarily require a license. Ordinary frets/nuts are not converted to
compensated hardware by selecting these profiles.

## Host compatibility and validation

Existing `tuneron`, `tunermute`, `tunerref` IDs, indices, defaults, A4 range
430–450 Hz, serialization and processing tap are retained. Nineteen new parameters
are appended after the released registration prefix with AU version hint 11.
`restoreCore` already supplies declared defaults for missing values; old sessions
must return to Equal rather than inherit the currently selected compensation.
The parameter-prefix checker and native old-state/recall test cover this.

- Pure kernel: 576 frequency/rate/detune/harmonic/noise conditions, plus 18 weak
  fundamental, exponential decay and DC conditions; silence/DC/noise rejection,
  target identity, invalid inputs, calibration and strobe direction/dead-band.
- Native integration: frozen baseline and candidate at 44.1/48/96/192 kHz,
  blocks 64/256, A0/B0/E2/A4. Measures first acquisition within 0.5 cent and
  settling after a 10-cent step (three consecutive audio-block observations).
  These repeated observations are not three independent analysis frames.
- Native UI: actual panel and production processor, synthetic A0, view-only
  opening/closing, settings recall, old-state defaults and worker teardown.
- Three-platform CI adds tuner tests to the existing native workflow. A new
  source requires its own CI result; no earlier PR's macOS result is inherited.

See [measurement evidence](evidence/tuner-20261013/README.md). Remaining acceptance:
recorded guitar/5-/6-string bass DI, changing attack pitch, inharmonicity, hum,
clipping, vibrato, ambiguous fundamentals, sample-clock accuracy, callback
scheduling, installed DAW/AU/VST3 recall and practical accessibility. A 100 ppm
sample-clock error alone is approximately 0.173 cent. Synthetic correctness on a
nominal clock does not certify an interface or an instrument at ±0.1 cent.
