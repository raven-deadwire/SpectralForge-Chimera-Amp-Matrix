# Auto Set Phase 7 independent validation

**Review follow-up:** the two defensive fixes and new regression results are in
[REVIEW_FOLLOWUP.md](REVIEW_FOLLOWUP.md). Its merge hold supersedes the initial
assessment below; the original evidence is retained as a historical snapshot.

Review date: 2026-10-31 KST. Baseline: `ad622ceffdd7404b12de4e473349b1104200048f`
(PR #226, merged 2026-10-30 23:17:34 KST). This is a follow-up to that merged
PR, not an update to its closed branch.

## Findings and changes

1. **Channel energy was incorrect.** The master tap averaged L/R before
   K-weighting and multiplied the resulting power by two. Mono read 3.0103 LU
   high; left-only stereo read 3.0103 LU low; opposite-polarity stereo vanished.
   Master capture now retains L/R; the meter sums independently weighted
   channel energies. An absent mono channel contributes zero. Spectral voicing
   also uses channel powers instead of a cancelling mono average.
2. **Trim compensated unapplied EQ.** The previous plan included all suggested
   EQ moves even when EQ was locked, bypassed, only partly placeable, or applied
   at lower strength. The controller now selects its actual free bands and
   strength first, quantises to the actual parameter intervals, then renders
   those new static filters on the captured channels. Loudness and peak are remeasured in the time domain.
3. **Peak limiting was lost after rounding/strength/deadband.** A required
   sub-1 dB reduction could be discarded; rounding up could exceed the ceiling;
   scaling a negative safety correction weakened protection. Peak headroom now
   constrains the final change, after loudness strength and deadband. Required
   attenuation rounds down in 0.5 dB steps. Locks, the per-run +/-12 dB cap and
   the output parameter range (-36 to +12 dB) remain binding; unmet protection
   is reported explicitly. With output locked, new EQ that worsens an unsafe
   peak is skipped. For example, an uncorrected 0.94 peak (-0.537443 dBFS)
   previously survived the 1 dB deadband; a -0.5 dB correction now predicts
   -1.037443 dBFS.
4. **Capture/edit ownership needed coverage.** A Final EQ or output edit during
   capture, including A-B-A, invalidates the level pass. Re-entrant edits during
   EQ placement roll back owned partial writes. Undo preserves a subsequently
   edited new Final EQ node as a group, including its enabled/bypass state.
5. **K-weighting numerator was already correct.** `{1,-2,1}` was retained.
   Dividing it by `a0` would change the reference transfer function. The meter's
   requirement for ten gated blocks was separated from the standard measurement:
   one complete non-silent gated block can be measured, while Auto Set still
   requires detected playing and ten gated blocks before a loudness adjustment.
   Peak-only protective attenuation does not require that loudness evidence.

## Independent reference

- libebur128 **1.2.6**, upstream tag `v1.2.6`, commit
  `67b33abe1558160ed76ada1322329b0e9e058b02`, compiled independently from
  https://github.com/jiixyj/libebur128.
- ITU-R BS.1770-5, Annex 1, Tables 1-3 and equations 2-7:
  https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf
- 144 cases: 3 sample rates x 8 signals x 6 channel layouts. Signals are a
  997 Hz sine, 27.5 Hz sine, bass harmonic mixture, deterministic noise,
  repetitive short picking, a single 20 ms attack, gated level changes and
  silence. Layouts are mono, dual mono, left-only stereo, opposite polarity,
  uncorrelated stereo and unequal stereo. These are generated signals, not DI.
- Tolerance: **0.02 LU**. Corrected meter: **144/144 pass**, maximum finite
  absolute error **0.00000133191 LU**. Silence returns no finite reading.
- Baseline: **111/144 fail** the same reference comparison, including missing
  short-attack and opposite-polarity measurements. Baseline used the exact
  `kWeighting` / `integratedLoudness` function bodies from the pinned source,
  with the original channel-mean capture semantics. It was not a full old
  plug-in run. Raw values: [baseline](baseline-loudness.csv),
  [corrected](reference-loudness.csv).

997 Hz, -20 dBFS peak per driven channel:

| Rate | Layout | libebur128 LUFS | Baseline LUFS |
|---|---|---:|---:|
| 44.1 kHz | Mono | -23.00751331 | -19.99721336 |
| 48 kHz | Mono | -23.01027575 | -19.99997520 |
| 96 kHz | Mono | -23.02759958 | -20.01729965 |
| 48 kHz | Dual mono | -19.99997580 | -19.99997520 |
| 48 kHz | Left only | -23.01027575 | -26.02057648 |
| 48 kHz | Opposite polarity | -19.99997580 | No reading |

The small sample-rate-dependent offset for a sine is shared with libebur128;
this is not a reason to replace the prescribed high-pass numerator.

## Regression scope and reproduction

`Tests/AutoSetLoudnessReferenceTests.cpp` links to libebur128, never the product.
`Tests/AutoSetLevelRegression.h` extends the existing `ChimeraAutoSetTests`:

- Channel-preserving, allocation-free capture, mono reset and ring wrap.
- Nine real processor rate/bus cases: six supported mono/mono and stereo/stereo
  captures, plus three checks that mono/stereo layouts are rejected (the
  current processor contract). Captured limiter input is compared
  sample-for-sample with output while the limiter and MATCH are inactive.
- Sub-1 dB safety corrections at four strengths and all three rates; output
  floor, +/-12 dB limit, locked output, insufficient short-attack evidence.
- 30 rendered cases through the real `GraphicalEQ` and output trim: all three
  rates, mono/antiphase stereo, sustained tone, picking, abrupt peaks and bass.
- 28 seeded user-EQ/strength cases: active or bypassed curve, EQ/output locks,
  full or partially available bands, Dynamic EQ reservations, active Final EQ
  dynamics and exact Undo. New static bands follow every active Dynamic EQ band.
- Repeated runs and immediate pre-run Undo, A-B-A capture edits, later output
  edits, grouped Final EQ Undo and re-entrant host writes during placement.

To reproduce both baseline and corrected CSVs on Linux with a separately
built library:

```sh
python3 Tools/run_auto_set_loudness_reference.py \
  --include /path/to/libebur128/ebur128 \
  --library /path/to/libebur128/build/libebur128.so \
  --out /tmp/phase7-reference
```

Build the reference library first, then configure the product with
`-DCHIMERA_LOUDNESS_REFERENCE_TESTS=ON`. Set `EBUR128_INCLUDE_DIR` and
`EBUR128_LIBRARY` if it is not installed system-wide. Build targets
`ChimeraAutoSetTests` and `ChimeraAutoSetLoudnessReferenceTests`. Run:

```sh
xvfb-run -a ctest --test-dir build -C Release \
  -R '^ChimeraAutoSet(Tests|LoudnessReferenceTests)$' --output-on-failure
```

On Windows/macOS the controller suite runs without Xvfb. The new
`auto-set-level.yml` runs the complete controller suite on all three OSes
and requires the independent reference suite on Linux.

## Verified local result and merge assessment

Final Linux Release run: **2/2 CTest targets passed**, 0 failures, 95.59 s.
The full `ChimeraAutoSetTests` passed in 90.11 s, including existing phase 1-6
rules, controller/lock/strength/ownership tests and the new phase 7 cases.
`ChimeraAutoSetLoudnessReferenceTests` passed in 5.48 s. See [CTest output](ctest.log),
[JUnit results](ctest-results.xml) and [source hashes / environment](provenance.json).

The UI-free DSP/controller suite ran directly without a display. `xvfb-run`
returned a cleanup error in this environment even when the child process
returned 0; the isolated edit-ownership child was also run directly and passed.
This does not validate real editor windows or host UI behavior.

Representative **stereo, 48 kHz** actual Final EQ + trim replay:

| Signal | Before LUFS | After LUFS | After peak dBFS | Applied trim dB |
|---|---:|---:|---:|---:|
| Sustained 997 Hz | -21.9382 | -17.9382 | -17.9382 | +4.0 |
| Short picking | -21.5686 | -20.0108 | -1.29889 | +3.5 |
| Low bass with hard accents | -22.5155 | -21.5918 | -1.26511 | +1.0 |
| Quiet spectrum with abrupt peaks | -31.1382 | -30.5178 | -1.43445 | +1.0 |

The last three deliberately finish below -18 LUFS because peak protection has
priority. A separate quiet signal needing more than 12 dB remains below target
because the per-run trim cap is respected. Targets are not falsely declared met.

The original #226 implementation should not be treated as fully validated:
channel handling and output safety required these corrections. This follow-up
passes local regression and is ready for review. Merge should wait for its
Linux/Windows/macOS CI and any required repository checks; local Linux success
alone does not certify those platforms. Real-DI and listening qualification below
remain a separate release gate, not evidence supplied by this test run.

## Outstanding real-world qualification

- Recorded guitar Drop A A1, bass B0 and bass Drop A A0 at 44.1/48/96 kHz:
  clean and distorted sustain, ghost notes, palm mutes, hard attacks and pauses.
- Real mono and stereo DAW/standalone routing, including any host-side mono
  duplication into a supported stereo bus, compared with an
  external meter at the same **pre-limiter** location. Post-limiter and A/B
  MATCH output are different measurement points and must be identified.
- Listening validation of the -18 LUFS target, EQ target shapes, guitar 70 Hz
  high pass and whether applying repeated corrections remains musical.
- True-peak/intersample measurements, filter switching/smoothing transients,
  stronger future playing, and new static EQ interacting with existing Dynamic
  EQ nodes. The tests validate settled static replay, not these conditions.

The -1 dBFS rule is a **sample-peak** prediction for captured material, not a
-1 dBTP certificate or a guarantee about future audio. Locks and gain limits
can legitimately prevent the loudness or peak target; the result must say so.
