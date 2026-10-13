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
- Worker: 8192-sample analysis, YIN fundamental selection and a multi-period,
  eight-point Lagrange fractional-delay refinement. Intended range 16–2000 Hz.
- A full analysis window is roughly 341 ms at 24 kHz. This is tuner acquisition
  time, not added audio-path latency. Real pluck settling may take longer.
- Queue discontinuity or enable transitions reset the worker's history. Readings
  expire after 250 ms without fresh samples. Silence/DC/nonperiodic noise do not lock.
- Virtual strobe motion integrates measured minus target Hz at 1×, 2× and 4× rates.
  It is an estimated-frequency visual strobe, not an optical/phase-domain clone.
  No artificial cent dead band, snapped phase or false stationary in-tune animation.
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
