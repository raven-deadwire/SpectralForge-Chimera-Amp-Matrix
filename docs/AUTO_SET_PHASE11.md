# Auto Set — phase 11 (delay and reverb)

Phase 11 lets Auto Set set the POST delay and reverb from how you play. It
adds no listening time: it reads the playing the input pass already heard
(10 ms windows after input gain). It runs just before the last pass (Final EQ
and output level), once the instrument is known, so that pass hears the wet
sound. Earlier phases are described in `AUTO_SET_PHASE1.md` …
`AUTO_SET_PHASE10.md`; locks and strength in `AUTO_SET_LOCKS_STRENGTH.md`.
The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has a **Delay and reverb** step, on by
default.

- **The results** get a **Delay + reverb** row with what changed and what the
  playing showed: detected notes per second, peak one-second density, and the share of pauses. Its
  SHOW button opens the reverb.
- **Strength** scales the mix changes.
- **Lock → Delay + reverb** keeps both effects.
- **UNDO AUTO** restores them.

## Measurement (`measurePlayingFeel`, `AutoSetSpace.h`)

- **Onsets:** a window at least 6 dB above the quietest of the three before it,
  within 24 dB of the 95th-percentile RMS window, at most one every 80 ms.
- **Notes per second:** detected onsets over the 6 seconds. The first attack is counted even without pre-roll.
- **Peak density:** the maximum detected onset count in any half-open one-second interval. Long rests cannot make a fast phrase eligible for a new delay.
- **Diagnostic pulse candidate (`beatMs`):** the median gap between onsets, doubled or halved into 250 ms – 1 s. This is not a reliable musical beat and is never used to enable or time a new delay. Even regular eighth notes and quarter notes cannot be distinguished from these gaps alone.
- **Pauses:** the share of windows more than 30 dB below the loudest playing.
- **Ring:** the median time from a note's peak until it falls 20 dB.

Playing is **dense** above 6 notes per second and **sparse** below 2.5.

## Rules (`analyseSpace`)

| | Clean | Rock | Metal |
|---|---|---|---|
| Reverb mix, guitar | 0.20 | 0.12 | 0.07 |
| Reverb mix, bass | 0.08 | 0.05 | 0.03 |
| Reverb size | 0.50 | 0.35 | 0.25 |
| Reverb damping | 0.45 | 0.55 | 0.65 |
| Delay | mix 0.15, feedback 0.30 | mix 0.10, feedback 0.22 | none |

- Dense playing takes 0.6 × the reverb mix and 0.1 less size, so notes do not
  wash into each other. Sparse playing takes 1.25 × the mix and 0.1 more size.
- The reverb is switched on when its mix is at least 0.03.
- A new delay is for guitar, clean or rock, with no more than 4 detected onsets
  in any one-second interval, **and DELAY SYNC already selected by the user**.
  Its time is one quarter note of the selected tempo (40–240 BPM).
  With HOST off this is the manual/TAP tempo. With HOST on, the latest audio
  callback must have received a finite in-range BPM from the host. Missing or
  invalid host data does not authorize Auto Set to enable a delay at the fallback
  manual tempo. Auto Set does not change SYNC, HOST or the saved free-time value.
  Sparse playing takes 1.2 × its mix.
- With DELAY SYNC off, onset timing alone is insufficient confidence. The delay
  stays off and the result explains why. There is no new listening pass.
  This intentionally defers automatic free-time tempo inference rather than
  assigning a confidence score to an ambiguous median interval.
- **Your effects stay yours:** a delay or reverb you already have on keeps its
  type, time, feedback, size and damping. Only its mix moves toward the rule.
  A delay you have on is never switched off; dense playing only lowers its mix.
  Host loss does not disable an existing user effect; the existing processor
  tempo fallback still applies. The new validity guard controls Auto Set activation only.
- An effect that is off is switched on with the whole rule, and its mix is
  scaled by strength.
- Without the input pass (Input gain and Gate both locked) there is no playing
  to judge, and the step is kept with that reason.
- An unknown instrument (Detect without a Tone EQ pass) is treated as guitar.

## Testing

The original fixed-interval rules, controller, locks, strength and UNDO tests
remain. Their positive delay cases now explicitly select a manual sync tempo.
`Tests/AutoSetSpaceFixtures.h` adds independently scored six-second mono signals:
straight eighths/sixteenths, triplets, 2:1 shuffle, syncopation, fast phrases
separated by long rests, uneven picking strength, and sustained notes. The
matrix covers 90/120/150 BPM, 44.1/48/96 kHz and 64/256/1024-sample callbacks.

`Tests/AutoSetSpaceRegression.h` checks onset counts against the score, density,
rejection of untrusted free-time candidates, synced quarter-note times, burst
rejection, bass/metal exclusions, initial attacks and invalid inputs. Real
processor tests supply an AudioPlayHead, verify changing/missing/invalid host
BPM, preserve active delay/reverb non-mix state, and check UNDO. EchoModule impulse
timing is measured separately; this is not a full DAW render or a listening test.

Reproduce after building `ChimeraAutoSetTests`:

```sh
build/ChimeraAutoSetTests_artefacts/Release/ChimeraAutoSetTests --space-regression
build/ChimeraAutoSetTests_artefacts/Release/ChimeraAutoSetTests --space-report /absolute/path/space.csv
build/ChimeraAutoSetTests_artefacts/Release/ChimeraAutoSetTests --space-fixtures /absolute/path/fixtures
```

Windows uses the corresponding `.exe`. On Linux, use the repository's
`Tools/run_linux_ui_tests.py --` wrapper when an X server is required.
The full default Auto Set test run includes these regressions, so the existing
Graphical EQ CI matrix runs them on Linux, Windows and macOS.

See [independent validation](AUTO_SET_PHASE11_VALIDATION_20261031.md) and its raw CSV.

## Not yet validated

All supplied audio is synthetic. No recorded guitar/bass DI or human listening
session was performed. Amounts, dense/sparse thresholds and perceptual usefulness
still require that validation. The onset detector retains its 80 ms refractory
period and 24 dB relative threshold: very weak, overlapping, legato or faster
attacks can be missed, and envelope modulation can be counted as attacks. Peak
density is a conservative guard on detected events, not a certified note counter.
Ring time is truncated by the next detected attack and the observation window;
a low-level tail can count as a pause. These measurements are not score duration
or literal rests. Automatic free-time BPM is deferred until ambiguity and real
DI performance can be established.
