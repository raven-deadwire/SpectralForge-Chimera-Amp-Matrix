# PR #231 review follow-up

This follow-up is based on `d531b81d92811750cb4106050f17d3e6eb7c05fc`.
It addresses the two review findings without changing the loudness formula,
reference tolerance, EQ strength, trim limits, peak ceiling, locks or Undo rules.

## Reviewed defects

- [Final EQ lookup](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/231#discussion_r4425684508): confirmed direct dereference of a potentially null parameter. The range helper now resolves gain, frequency and Q before dereferencing any of them. An unavailable range rejects the complete new move; the controller excludes that move from rendering, output compensation and parameter writes, and reports the skipped correction. Available parameters use the same `snapToLegalValue` operations as before.
- [Reference ownership](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/231#discussion_r4425684518): confirmed that a failed frame-add operation threw before destruction. The real comparison function now acquires the state into a `unique_ptr` with the libebur128 deleter immediately. A null initialization never reaches add/measure; success, frame-add failure and measurement failure destroy an allocated state exactly once.

## New regression coverage

- Five missing-lookup cases: each of gain/frequency/Q individually, all three together, and a real failed APVTS lookup for an absent band. No partially quantized proposal is accepted.
- Twenty-four normal range cases compare the resulting values with the original range operation, including four strengths, fractional values and range endpoints/clamps.
- Four reference lifetime cases use real libebur128 allocation/destruction with injected init/add/measure failures. They assert destruction counts and that later operations are not called after a failure.
- Dedicated CTest entries `ChimeraAutoSetParameterSafetyTests` and `ChimeraAutoSetReferenceOwnershipTests` report these results independently of the full controller suite. The Auto Set workflow runs parameter safety on all three OSes and reference ownership on Linux.

## Local verification

Ubuntu 24.04, GCC 13.3.0, CMake 3.28.3, Release, qualification-only beta metadata.
The working-tree source files below were built before publishing the follow-up
commit; the build revision string still identifies the parent. File digests in
[provenance](review-followup-provenance.json) identify the tested source.

| CTest target | Result | Duration |
|---|---|---:|
| ChimeraAutoSetTests | PASS | 122.75 s |
| ChimeraAutoSetParameterSafetyTests | PASS | 0.08 s |
| ChimeraAutoSetLoudnessReferenceTests | PASS | 5.69 s |
| ChimeraAutoSetReferenceOwnershipTests | PASS | 0.00 s |

**4/4 targets passed; 0 failures; 128.53 s total.** Full output:
[test log](review-followup-tests.log), [JUnit](review-followup-tests.xml).
The full controller suite confirms six supported mono/stereo captures and
three rejected mono-to-stereo bus contracts; 30 actual Final EQ/trim renders
including antiphase, picking, bass and abrupt peaks; 28 seeded user-curve,
bypass, lock, available-slot and strength cases; repeated runs, capture edits,
grouped Undo and synchronous host edits. Existing phase 1-6 tests also pass.

After configuring with `CHIMERA_LOUDNESS_REFERENCE_TESTS=ON`, build
`ChimeraAutoSetTests` and `ChimeraAutoSetLoudnessReferenceTests`, then run:

```sh
ctest --test-dir build -C Release \
  -R '^ChimeraAutoSet(Tests|ParameterSafetyTests|LoudnessReferenceTests|ReferenceOwnershipTests)$' \
  --output-on-failure --no-tests=error
```


Reference rerun: libebur128 1.2.6, **144/144 pass**, maximum finite error
**0.000001331909317 LU**. All CSV rows equal the original checked-in
`reference-loudness.csv`. This includes 44.1/48/96 kHz and mono, dual-mono,
left-only, antiphase, uncorrelated and unequal stereo.

The four lifetime cases also pass AddressSanitizer + UndefinedBehaviorSanitizer
with `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`. LeakSanitizer itself could not
complete because this execution environment denies its `/proc/.../task` access;
no LeakSanitizer pass is claimed. Explicit destruction-count assertions passed.

## CI and merge assessment (separate from the two fixes)

At the original PR head `d531b81`, the Auto Set workflow passed Linux and macOS
but Windows `ChimeraAutoSetTests` terminated with a segmentation fault. The
Windows candidate, Graphical EQ and Open Beta workflows also failed on that
same test. These logs do not establish that the crash was caused by either
reviewed issue, and the two fixes must not be reported as having fixed it.

- [Original Auto Set run](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41085868211)
- [Original candidate run](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41085868419)
- [Original Graphical EQ run](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41085868577)
- [Original Open Beta run](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41085867886)

The original Ubuntu native UI jobs also failed `ChimeraAmpHeadControlTests`
and `ChimeraCabPanelStateTests`; these are separate from the Auto Set safety
regressions. Other original workflow results included successful CAB panel
contracts, cabinet WAV round trip, Niflheimr measurements/DI contracts, beta
lifecycle, standalone contracts, dynamics workspaces, NAM validation, CAB
realtime, AAX Native Development and release-publisher contracts. The factory
preset publication workflow was skipped, not passed.

A read-only merge trial against main
`472b93e68d20bcad07a028cc67a17b50f8171838` also found conflicts in
`Source/AutoSet.h` and `Source/AutoSetController.h`. Main adds amp/boost capture
and later Auto Set stages while this PR changes the output capture. Those
changes need a deliberate integration and rerun, not choosing one side.

**Hold merge.** Require passing checks for the updated head, investigation of
the Windows crash, and a tested integration with current main. The two review
findings can be fixed while those independent merge blockers remain.
Real DI/listening/DAW and true-peak qualification remain outside these tests;
the existing peak constraint remains a captured-signal sample-peak prediction.
