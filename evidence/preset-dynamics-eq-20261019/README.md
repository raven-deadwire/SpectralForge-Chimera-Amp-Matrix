# Factory preset dynamics/EQ local verification

These are local Linux measurements for the 48-preset maintenance revision. Start
with [summary.json](summary.json), [final-gates.csv](final-gates.csv),
[level-comparison.csv](level-comparison.csv) and
[dynamics-comparison.csv](dynamics-comparison.csv). Test provenance and source
file hashes are recorded in the summary and the two input manifests.

The final strict Integrated test passed in 319.64 seconds. The final exporter
passed in 44.23 seconds and verified all 48 files through the product serializer.
The six core EQ/UI/editing/dynamics/gate/export CTests passed on the initial
recipe. Their structure was unchanged by the subsequent threshold adjustment;
they were rebuilt but not redundantly rerun locally. The remote full matrix is
required on the actual committed source before distribution.

The first remote Linux signal-path job exposed an old UI assertion that all
factory EQ banks must remain bypassed. The corrected UI contracts passed both
targeted local CTests: common dynamics in 6.52 seconds and signal path in 110.24
seconds. The latter completed all 48 recalls and the final automation, A/B,
project-restoration and editor-recreation checks. See the
[UI follow-up](ui-verification-followup.md),
[JUnit results](ui-followup-ctest.xml) and
[follow-up input manifest](ui-followup-input-sha256.json) for the exact scope
and build provenance. The superseding source must still pass the complete
required remote matrix before publication.

The render fixture contains six deterministic, eight-harmonic plucks over
1.2 seconds at 48 kHz, with 128-sample blocks. It checks nominal and +6 dB input,
including a separate render with the new limiter bypassed. All 192 peak
conditions and the existing nominal RMS gates passed. The baseline had no new
common finishing. The first authored candidate and the calibrated final
candidate are distinct measurements, not two versions of the baseline.

Dynamic mean values are block-end meter averages. Common compressor GR is the
compressed branch before its parallel mix. Quiet high-frequency detectors are
expected for this limited harmonic fixture; their thresholds were not lowered
just to force activity. The low-A/B response is the added EQ's linear response
at static and maximum-cut settings, not complete amp-path hardware fidelity.

The new project/A-B helper initially compared a noncanonical bool host alias
(`0.81`, still meaning true) with its canonical value. The final helper uses
canonical, changed dirty values and keeps its strict error bound.
The existing direct `0.81` factory-recall regression remains in place. This
test-fixture correction did not change the product serializer or DSP.

The local build used baseline `ec6fd639e2958d34605bfb04bb81ca02c1cfb38f` with the
listed working files overlaid. The final recipe SHA-256 is
`ebd2e57b8dc554f813814a5c1bf3292e53d56cf3955a7634b026e2abce9764bd`.
The local export's embedded baseline SHA is not a public release identity.
The maintenance publication receipt and attached remote CI/package artifacts
must identify the actual committed product source. These measurements do not
claim real-instrument listening or actual DAW acceptance.
