# Auto Set Phase 11 independent validation — PR #239

## Decision

Do not apply the onset-gap median as a musical quarter-note tempo. In the
original head `439a513a4b5e3678d722a4138003b0853420b369`, regular subdivisions,
shuffle, syncopation and sustained notes can all produce incorrect BPMs. A
regular gap alone cannot distinguish, for example, quarter notes at 240 BPM
from eighth notes at 120 BPM. Adding an interval-consistency score would not
resolve that ambiguity.

The patch therefore keeps the candidate only as diagnostic data, removes it
from the displayed result and defers automatic free-time delay activation.
A new delay requires the user's existing DELAY SYNC selection and a usable
manual/TAP or HOST reference. Auto Set never selects SYNC or HOST itself.
Peak one-second onset density additionally blocks fast phrases hidden by rests.
This is a conservative activation fix, not a newly validated beat tracker.

## Independent synthetic protocol

`Tests/AutoSetSpaceFixtures.h` is the signal and score source. There are eight
scores at each of 90, 120 and 150 BPM: straight eighths, straight sixteenths,
eighth-note triplets, 2:1 shuffle, syncopation, sixteenth-note bursts separated
by a long rest, uneven picking strengths, and sustained notes. Signals are six
seconds long with a 100 ms lead-in. Syncopation uses beat offsets
`0, 0.75, 1.5, 2.5, 3.25` in a four-beat bar. Rest fixtures omit events between
1.25 and 4.75 seconds. Uneven amplitudes repeat `0.30, 0.035, 0.12, 0.06, 0.22`.
Sustain notes last 0.9 seconds; other notes last 0.07 seconds. Each note has a
1 ms attack, exponential decay and 196/392 Hz components. A fixed integer PRNG
adds low-level noise. These are synthesized test fixtures, not guitar recordings.

The independent score defines BPM, event times and event count before analysis.
The actual product `Capture` class reduces audio to 10 ms windows, followed by
`measurePlayingFeel`/`analyseSpace`. Sample rates are 44.1, 48 and 96 kHz.
The baseline used 256-sample callbacks (72 cases); the regression also uses
64 and 1024 samples (216 cases). No expected BPM is derived from the estimator.

## Baseline findings

Raw results: [baseline.csv](evidence/auto-set-phase11-20261031/baseline.csv).
The original C++ header was executed directly using
[baseline-probe.cpp](evidence/auto-set-phase11-20261031/baseline-probe.cpp), not
translated or reimplemented. All 72 onset counts matched these fixture scores.
The original rule enabled 45 delays; 36 of those differed from the score BPM by
more than 5%. These counts describe this constructed matrix, not an estimated
failure rate for real music.

Representative 48 kHz results at **120 BPM** (quarter note = 500 ms):

| Score | True notes/s over 6 s | Candidate BPM | Error | Original delay |
|---|---:|---:|---:|---|
| Eighths | 3.833 | 240.000 | +100.0% | 250 ms |
| Sixteenths | 7.667 | 125.000 | +4.2% | Off due to density |
| Triplets | 5.833 | 176.471 | +47.1% | Off due to density |
| Shuffle | 3.833 | 181.818 | +51.5% | 330 ms |
| Syncopation | 2.500 | 157.895 | +31.6% | 380 ms |
| Long rests + fast bursts | 3.000 | 125.000 | +4.2% | 480 ms despite fast bursts |
| Uneven picking | 3.833 | 240.000 | +100.0% | 250 ms |
| Sustained notes | 1.000 | 60.000 | −50.0% | 1000 ms |

Ten-millisecond quantization also affects the folded interval: 125 ms
sixteenths can yield a 120 ms median, doubled twice into 480 ms. Folding does
not establish a metrical reference. Sustained notes at half-note intervals
produce the opposite octave error.

## Patched synthetic result

The 216-case matrix passed all score-count and activation assertions. None of
216 untrusted free-time candidates enabled a new delay. With each fixture's
reference BPM supplied as a selected sync tempo, 108 eligible cases enabled
exact quarter-note times; fast cases were rejected. For the 120 BPM rest
fixture, the global average is 3 notes/s but the peak is 8 notes in one second,
so it no longer enables a new delay. Raw results:
[regression.csv](evidence/auto-set-phase11-20261031/regression.csv).
These results establish guard behavior on the declared fixtures; they do not
establish that a musical BPM can now be inferred from playing.

## Changes and regression coverage

- Untrusted free-time activation is rejected even for perfectly regular gaps.
  The UI explains that onset gaps do not establish tempo.
- New synced delay time is `60000 / BPM`; its source is explicitly manual/TAP
  or HOST, with a 40–240 BPM finite range. `delaytime` remains the user's saved
  free-time value. The existing echo supports up to 2000 ms, including the
  1500 ms quarter note at 40 BPM.
- HOST requires usable data from the latest real audio callback. A new atomic
  validity value distinguishes host data from the processor's manual fallback.
  Missing position/BPM, zero, negative, NaN, infinity and out-of-range values
  cannot trigger a new Auto Set delay. Re-prepare clears this validity value.
- A half-open one-second window must contain at most four detected starts.
  Global average density remains available for the existing reverb rules.
- The first three windows no longer discard an initial attack. Nonfinite RMS
  or invalid window duration is rejected before interval arithmetic.
- Controller tests preserve active effect type, free time, feedback, sync,
  tempo, reverb size and damping with both SYNC states. UNDO checks all original
  parameters. Existing strength/lock/input-pass tests remain.
- Real-processor tests supply a mock AudioPlayHead at 40/90/120/150/240 BPM,
  plus missing/invalid host cases. A separate 120 → 90 → 150 BPM sequence tests
  host changes. The real EchoModule's impulse delay is checked to within 1.1
  samples at 44.1/48/96 kHz. This is component integration, not a real DAW session.

## Reproduction

Build and run the normal `ChimeraAutoSetTests` target as documented in
[AUTO_SET_PHASE11.md](AUTO_SET_PHASE11.md). The default run contains all new
regressions. `--space-regression` runs the focused suite, `--space-report`
writes the 216-row current matrix, and `--space-fixtures` exports 72 WAV files
plus `events.csv` with exact score event times for independent replay.

To replay the old header without modifying the worktree, put the output of
`git show 439a513a4b5e3678d722a4138003b0853420b369:Source/AutoSetSpace.h` in a
separate include directory. Compile `baseline-probe.cpp` with that directory
before `Source`, plus `Tests` and JUCE 8.0.8's `modules` directory. Link the
JUCE core and compilation-time modules with pthread/dl/rt on Linux. Define
`JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1`, `JUCE_USE_CURL=0` and
`JUCE_WEB_BROWSER=0`. The probe includes the real JUCE colour implementation
required by the source headers. Its stdout is the baseline CSV. Example from
the repository root (set `phase11_juce` to a local JUCE 8.0.8 checkout):

```sh
phase11_baseline=$(mktemp -d)
git show 439a513a4b5e3678d722a4138003b0853420b369:Source/AutoSetSpace.h > "$phase11_baseline/AutoSetSpace.h"
c++ -std=c++20 -O2 -pthread -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 \
  -DJUCE_USE_CURL=0 -DJUCE_WEB_BROWSER=0 \
  -I"$phase11_baseline" -ISource -ITests -I"$phase11_juce/modules" \
  docs/evidence/auto-set-phase11-20261031/baseline-probe.cpp \
  "$phase11_juce/modules/juce_core/juce_core.cpp" \
  "$phase11_juce/modules/juce_core/juce_core_CompilationTime.cpp" \
  -ldl -lrt -o "$phase11_baseline/probe"
"$phase11_baseline/probe" > "$phase11_baseline/baseline.csv"
```

## Validation status and limits

The complete `ChimeraAutoSetTests` suite passed in 143.62 seconds
([CTest record](evidence/auto-set-phase11-20261031/ctest.xml)).
Local focused native tests also passed (`--space-regression`), including the 216
signal cases, controller/ownership cases and nine echo impulse checks. All 72
WAVs were exported and their format and six-second sample counts checked;
[events.csv](evidence/auto-set-phase11-20261031/events.csv) is the score and
[fixture-wav-manifest.csv](evidence/auto-set-phase11-20261031/fixture-wav-manifest.csv)
records their hashes. Exact source hashes and environment are in
[provenance.json](evidence/auto-set-phase11-20261031/provenance.json), with the
[focused native log](evidence/auto-set-phase11-20261031/focused-tests.log).
CI results must be checked on the updated PR head; previous-head green checks
do not cover this patch.

**Synthetic validation only.** No real guitar/bass DI was supplied or recorded
for this review. No human listening test, external DAW, real host transport,
or musical suitability approval is claimed. Sustained/overlapping weak attacks,
legato, envelope modulation, pitches outside this fixed synthesis and events
inside the 80 ms refractory interval remain unvalidated. Ring and pause metrics
are envelope heuristics, not musical note lengths or annotated score rests.

For the next real-DI pass, replay the same score categories with a click and
retain click BPM/event annotations separately from the dry recording. Include
clean guitar and bass, ghost notes, legato and rapid attacks; audit onset errors,
then listen to dry/wet A/B at matched level. Assess reverb amount and delay mix
separately from mathematical timing. Automatic free-time inference should not
be restored based only on these synthetic results.
