# Precision tuner: output-preserving final refinement candidate

Development-only follow-up to #70 at `52f7d19e772e6f3321d47c4b46d22b1f7f1946a9`.
The owner requested continued development/optimization, not publication or an alert.

## Production change and invariant

Only `Source/PrecisionTuner.h` changes. The final golden-section search may stop
when both endpoints of its remaining positive period bracket convert to the same
published float frequency AND both frequencies are within 16..2000 Hz. Subsequent
brackets are subsets, so their midpoint frequency converts to that same float.
Near a float-rounding boundary or either range boundary the original 30-iteration
cap remains. This is not a cent tolerance, shortened analysis window, display
snap, changed confidence threshold, or change to weak-fundamental selection.

The first refinement and octave-correction refinements still run the original
search. Confidence is calculated from that unchanged first-stage residual.
The final residual is not used for confidence or octave selection. All strobe,
profile, callback, FIFO, generation, expiry and worker scheduling code is unchanged.

## Source-bound regression

`Tests/reference/PrecisionTuner52f7.h` is the exact original Git blob
`d77eca22470cf8ba16844a26d8a090c472e9c7a4`; namespace renaming happens only through
the preprocessor. The existing PrecisionTunerTests target compares both published
float fields bit for bit, without replacing its existing accuracy assertions.

The 1,105 comparisons comprise the original 576 tonal and 18 stress fixtures,
420 additional rate/window/waveform combinations, 81 float/range-boundary cases,
and 10 invalid/silent/DC cases. Some extra short-window low-note fixtures reject
in both versions: their equality is not additional pitch-accuracy acceptance.

Local Linux x86_64 results, GCC 14.2 and Clang 17:

| Configuration | Original 576 + 18 gates | Exact comparisons |
| --- | --- | --- |
| GCC -O3 -DNDEBUG | PASS, worst 0.0599684 cent | 1105/1105 |
| Clang -O3 -DNDEBUG | PASS, worst 0.0599684 cent | 1105/1105 |
| GCC -O3 -DNDEBUG -march=native | PASS | 1105/1105 |
| Clang -O3 -DNDEBUG -march=native | PASS | 1105/1105 |
| Clang -O1 ASan + UBSan, leak detection enabled | PASS | 1105/1105 |

These are five executions of the same corpus, not 5,525 distinct fixtures.
The existing three-platform focused workflow already runs this test target;
no workflow, native 32-route acquisition/settle limit or accuracy gate changes.

## Bounded local performance experiment

The Linux-only diagnostic builds frozen A and candidate B into one executable.
Each compiler has 48 precomputed fixtures, eight alternating ABBA/BAAB rounds
with deterministically shuffled fixture order, four estimates per measured arm,
1,536 measured rows and all 96 warmup rows retained. Wall and process-CPU clocks
are recorded separately. CSV I/O and fixture generation are outside timed calls.

| Compiler | Median of cellwise wall changes | Faster cells | Worst cell regression |
| --- | ---: | ---: | ---: |
| GCC 14.2 | -0.908834% | 32/48 | +5.799556% |
| Clang 17 | -0.757433% | 28/48 | +4.898119% |

Process-CPU median changes are -0.956588% and -0.811167%, respectively. These are
small, mixed observations on one shared Linux host, not statistical proof of a
speedup, acquisition-latency improvement, or macOS/Windows performance acceptance.
Every regression remains in the raw records. The Linux-only guard added to the
benchmark driver afterwards reproduced byte-identical GCC and Clang executables.

The population auditor accepted both complete records and rejected six corruptions:
missing row, duplicate row, wrong arm, nonfinite clock, changed fixture and missing
warmup. It does not turn valid measurements into a realtime PASS.

An earlier reference-energy/DC-hoisting candidate is REJECTED, not part of the
production delta. Although exact-output tests passed, its cellwise median wall
change was -1.783742% on GCC but +4.862987% on Clang (44/48 cells slower). Its code,
raw timings and logs remain separately labelled in the companion evidence archive.

## Reproduction and provenance

```sh
c++ -std=c++17 -O3 -DNDEBUG -I Source Tests/PrecisionTunerTests.cpp -o tuner-tests
./tuner-tests
c++ -std=c++17 -O3 -DNDEBUG -I Source Tools/tuner_refinement_benchmark.cpp -o tuner-bench
./tuner-bench measurements.csv
python3 Tools/analyze_tuner_refinement_benchmark.py measurements.csv
```

Tested production header SHA-256:
`0da1dcd39288af8bbdb484b6b1ed4621824901be14607206f8fb0837afe2b512`.

Final raw CSV SHA-256:
- GCC: `dbd71ac349974e809a34deb3e46239fb0df402864a865d659b78b352851cf55a`
- Clang: `1035640e44760e87591d4595ec5026f4cb420de74c52d555b2c105eaa7ea80a7`

The raw CSVs, failed-experiment history, compiler logs, source/binary hash ledger
and reproduction sources are supplied in the companion conversation artifact
`chimera-tuner-optimization-20261014.zip`. They are not claimed uploaded as a
GitHub Actions artifact or committed as raw CSVs by this change.

Local source files were retrieved at the pinned GitHub ref and their original Git
blob identities checked before editing. Git DNS access was unavailable locally;
this was a direct standalone kernel build, NOT a complete JUCE/product checkout,
native worker/UI execution, installer verification, or actual DAW session.

## Still unresolved

#70's exact-source macOS focused 3/32 and Product 2/32 route failures remain
failures. This candidate requires its own focused and full Product verification.
Window-completion time, worker wake/service delay, estimator time and publication
age still need per-route attribution. An approximately 1% local median kernel
observation does not explain or close those scheduler-sensitive failures.

The <290 ms acquisition, <290 ms settling and <0.1-cent route requirements,
>250 ms stale-input handling, channel-switch reset and all release/state contracts
remain unchanged. Existing 96k/64 callback overruns, real DI/listening and actual
DAW/device acceptance remain unresolved. No merge, release, tag, package-version,
expiry, approval-policy, live updater, homepage or notification change is made.
