# Signal navigation and precision tuner — 2026-10-13

Owner request: make the signal stages the main editor navigation; improve the
utility dock and tuner visibility; develop a Peterson-referenced precision/strobe
tuner with Buzz Feiten and True Temperament targets.

## Editing workflow

The main strip contains PRE → HEAD → CAB → TONE EQ → POST → FINAL EQ.
Original vector pedal, head, speaker, EQ-slider and rack pictograms accompany text.
Each stage has a 44-design-pixel hit area, keyboard navigation and a selected state.
The redundant top RIGS/PRE/POST/EQ buttons and room/controls toggle are removed.
HEAD and CAB directly select their respective current-mode workspaces. ROUTING
remains a separate detailed inspector, including utilities and actual DSP branches.
The condensed strip omits Input, Gate, Transpose, Tuner, Doubler, Click and Output.
This is a presentation change; processing order and utility controls are preserved.

The high-contrast footer keeps TUNER, MIDI, TAP, tempo, HOST SYNC, METRONOME and
SETTINGS visible. TUNER opens an independent workspace without changing audio.
START TUNING enables detection; MUTE OUTPUT controls the existing tuner mute.
An active tuner is labelled ON or MUTED in the footer, including after navigating
back to a signal stage. Navigation itself never changes mute or other parameters.

## Measurement and display

- Clean input tap precedes gate/transpose/effects, as in the previous product.
- Audio callback: existing two-pole low-pass, decimation to approximately 24 kHz,
  and a bounded SPSC queue. No new allocation or pitch estimation in processBlock.
- Worker: 4096-sample analysis, 10 ms polling, YIN fundamental selection and
  gain/DC-normalized eight-point fractional-delay correlation. Weak-fundamental
  checks precede multi-period refinement; repeated search evaluations are reused. Intended range 16–2000 Hz.
- A full analysis window is roughly 171 ms at 24 kHz. This is tuner acquisition
  time, not added audio-path latency. Real pluck settling may take longer.
- Queue discontinuity, channel changes, nonfinite input or enable transitions reset the worker's history. Readings
  expire after 250 ms without fresh samples, including while the worker is stalled
  and after it resumes with old queued input. Age is measured from input-block
  publication, not delayed analysis. Silence/DC/nonperiodic noise do not lock.
- Virtual strobe motion integrates measured minus target Hz at 1×, 2× and 4× rates.
  It is an estimated-frequency visual strobe, not an optical/phase-domain clone.
  No artificial cent dead band, snapped phase or false stationary in-tune animation.
  Large per-frame steps are limited to 0.2 stripe periods to avoid apparent reversal.
- Needle view, note/octave, measured Hz, cent error, target Hz and offset are shown.
  Existing A4 calibration range (430–450 Hz) remains unchanged for host compatibility.

## Compensation profiles and scope

| Profile | Targets | Scope |
| --- | --- | --- |
| Equal | Chromatic, zero offsets | Ordinary instruments |
| True Temperament chromatic | Published 12 pitch-class offsets | Thidell Formula 1 chromatic mode |
| True Temperament 6/7/8 | Explicit note+octave per string | High/low E and extended low strings remain distinct |
| Buzz Feiten electric open | Six open strings | Electric guitar fitted with the system |
| Buzz Feiten electric 12th | Six fretted octave targets | Separate from open-string offsets; no harmonic substitution |
| Custom | Eight editable MIDI-note and cent pairs | Alternate tuning and technician-specified targets |

String order is high to low. Auto matches the nearest available target within
150 cents; manual string selection locks the target and exposes octave mistakes.
Invalid string/profile combinations must not silently substitute an unrelated
target. All profile/display/string/custom values are appended host parameters
(AU hint 11), saved through existing project/reference/A/B serialization. Old
projects obtain declared defaults, never the current session's custom offsets.

Manufacturer references do not make an ordinary nut/fretboard into a compensated
instrument. Buzz Feiten acoustic, bass, 12-string and alternative TT temperaments
are not supplied as factory profiles in this change. Add each only against its
own primary numeric source and matching instrument tests; custom entry is available.

## Primary references, retrieved 2026-10-13

- [Peterson StroboStomp HD](https://www.petersontuners.com/products/stroboStompHD/):
  0.1-cent stated accuracy is the comparison target, not a Chimera specification.
- [True Temperament tuning guide](https://truetemperament.com/how-to-tune/):
  chromatic and explicit 6/7/8-string open-note offsets. These are separate tables.
- [Anderson Guitarworks programming guide](https://www.andersonguitarworks.com/tuning-tips):
  electric open targets E4 0, B3 +1, G3 −2, D3 −2, A2 −2, E2 −2 cents;
  fretted-octave offsets 0, 0, +1, +1, 0, 0 in that same string order.
  The guide's inconsistent Drop-D octave line is deliberately excluded.
- [Peterson Buzz Feiten guide](https://www.petersontuners.com/myInstrument/buzzFeiten):
  distinguishes open and fretted intonation targets and instructs use of fretted
  notes for intonation rather than harmonics.

## Acceptance

The pure C++ test covers 576 conditions: 22.05/24/24.5 kHz analysis rates,
16.35–1975.53 Hz tones, detuning, harmonics and seeded noise. Thresholds are
0.1 cent for clean/harmonic synthetic signals and 0.3 cent for noisy signals.
It also tests rejection, profile targets, custom calibration and strobe direction.
The native test exercises the actual decimator/worker with synthetic A0 (27.5 Hz),
four UI scales, audio-neutral navigation, recall, legacy defaults and teardown.
The existing signal-path tests retain mode/state assertions and deadlines.

Do not infer real-instrument ±0.1-cent accuracy from synthetic results. Remaining
qualification includes a recorded clean-DI corpus (low B/A, attack/decay, weak
fundamental, hum, vibrato), measured lock time/octave errors, worker CPU, audio
callback deadlines, sample-clock calibration, installed DAW recall and accessible
navigation on Windows/macOS/Linux. Preserve the inherited PR #60 macOS failure
until the modified complete source passes; this development is not release approval.

## October 14 reconciliation with #67 and #68

The embedded workspace and signal/utility navigation remain owned by #66/#68.
The normalized estimator, shorter window, generation invalidation, bass targets,
display anti-alias and frozen-baseline acquisition tests are taken from #67
(`36e41a50a9179b52eba21c3e8d5f99d0aad5bbef`). The #68 single-publication
SPSC callback and separate analysis worker are preserved. The worker uses normal
priority, below the host's realtime audio callback, to avoid utility-priority
deferral on macOS. There is one engine, one
parameter bank and one UI workspace. No floating tuner window or redundant
navigation is reintroduced. Custom note names are applied after attachments.

The 19 tuner parameters still precede the 40 common dynamics parameters.
New zero-offset bass choices append to the unpublished profile choice list;
all released host IDs, ranges, defaults, indices and hints remain unchanged.
BFTS choices identify the published Anderson electric open/fretted-12 targets.
The two bass choices are equal temperament, not proprietary compensation.

The historical [#67 evidence folder](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/tree/36e41a50a9179b52eba21c3e8d5f99d0aad5bbef/docs/evidence/tuner-20261013) retains its failures and original source
hashes in the existing remote commit; it is not duplicated in this change. It does not qualify this integrated source. The 290 ms native acquisition
and pitch-step gates and 600 ms UI delivery test remain in the candidate CI.
The embedded UI now carries #67's bounds/legacy-offset checks as well as #66's
four-scale, audio-neutral navigation and project recall checks.

Real DI, physical sample-clock calibration, stopped/discontinuous input,
Windows/macOS/Linux source-matched builds, callback deadlines and installed
DAW acceptance remain separate release conditions. Two-pole 3 kHz filtering is
not an ideal anti-alias filter. Synthetic sub-cent results do not certify
Peterson-equivalent hardware or instrument accuracy.

Stereo channel selection now retains the current input until the other channel
is at least 3 dB stronger. This prevents small balance fluctuations from repeatedly
invalidating the analysis window. A genuine switch still invalidates immediately.
The native regression covers alternating near-equal input levels and acquisition
of a new dominant channel, alongside the unchanged timing gates.

After an expired input gap, the worker discards its pending FIFO snapshot and
retained analysis history before consuming new input. A fresh block therefore
cannot refresh an old analysis window; subsequent input must fill a new window.
