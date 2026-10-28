# PR #183 A/B MATCH safety audit — 2026-10-26 KST

Scope: [PR #183](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/183),
original head `368760a0b47f0495e3cd633e95d78118ad54f775`, based on main
`cfeaa5338c9ba3027651278290b7f5650eb363db`.

**Disposition: fixes implemented and focused Linux audio regression passed;
merge remains blocked pending native UI and current-commit platform CI.**
No release, actual DAW certification or listening approval is implied.

## Findings and changes

| Finding | Evidence | Change |
| --- | --- | --- |
| `prepare()` retained slot statistics across device/rate/channel changes | Original code failed the new reprepare test | Every prepare clears both slots, queued commands, filter history and gain, and disarms MATCH |
| Factory preset replacement inherited the previous active slot's loudness | Original code failed the preset test | Invalidate only the replaced slot; preserve the other reference and remeasure after holdoff |
| Project restore into an already-used instance left MATCH enabled | Source inspection; added used-instance restore regression | `forget()` disarms MATCH, clears pending COPY, and clears old gain/statistics before the next output; reference files use the same restore path |
| Offline rendering printed the comparison cut | Original code failed a paired real-processor output comparison | Host non-realtime or recording flags force unity from the first callback sample; block rearming and remain off afterward |
| Transition holdoff rounded to whole blocks; rapid A/B/A could go unnoticed | Source inspection and switch regression | Count holdoff samples exactly; explicitly restart filter/holdoff on switch/COPY/invalidation |
| “Monitoring only” wording obscured effect on output | Source and UI review | Output warning on first enable per editor session, precise tooltip/docs, protected `SAFE` display; disable on Easy view/editor close |

The output guard runs for normal **and bypassed** host callbacks. It does not
change any saved DSP parameters or project schema. Measurements pause during
reported printing. Immediate unity at a print boundary intentionally overrides
the normal 50 ms release; otherwise the beginning of a recording would still
be attenuated. A level step at that boundary is possible and needs listening
acceptance. Turn MATCH off before printing whenever possible.

COPY/preset command ordering is covered: an older pending COPY cannot restore
a replaced preset's stale measurement. Ordinary A/B switches retain both
estimates. Manual knob edits continue to update the running estimate.

## Executed validation

Environment and SHA-256 fingerprints of every tested source are in
[evidence manifest](evidence/ab-level-match-20261026/manifest.json).

- [Original-code reproduction](evidence/ab-level-match-20261026/baseline.txt):
  an initial extension of the safety suite failed reprepare, preset invalidation
  and offline-output identity against the original production code. Existing
  matching/meter checks and the 50 ms ramp matrix passed.
- [Final focused CTest run](evidence/ab-level-match-20261026/linux-ctest.txt):
  **2/2 CTest targets passed**. `ChimeraABLevelMatchTests` contains eleven
  regression groups; `ChimeraHostBypassTests` also passed.
- Ramps and 24 dB cap: **24 combinations** of 44.1/48/96 kHz,
  1/64/257/1024-sample blocks and mono/stereo. ON, OFF and both A/B directions
  checked sample by sample. OFF byte identity includes signed zero.
- Actual production processor: sample-rate/reprepare, preset/COPY/A/B,
  used-instance project/reference restore without prepare, recording and
  offline flags, bypass, missing playhead, rearm prevention and no automatic
  resume. Paired recorded/rendered output is byte-identical from its first
  protected sample to an otherwise identically driven MATCH-off instance.
- Measured processor match: **-5.99176 dB cut**, with approximately
  **0.004 dB** residual A/B RMS difference in the deterministic sine test.
- [Packaging contracts](evidence/ab-level-match-20261026/packaging.txt):
  **24 passed / 3 Windows-only checks skipped** (27 total).
- CI validation ownership map and [CTest evidence producer contracts](evidence/ab-level-match-20261026/ctest-producer.txt) passed.

Reproduction commands (normal supported Linux build dependencies required):

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCHIMERA_BETA_QUALIFICATION_ONLY=ON
cmake --build build --target ChimeraABLevelMatchTests ChimeraHostBypassTests --parallel 3
ctest --test-dir build -R '^Chimera(ABLevelMatch|HostBypass)Tests$' --verbose
python Tools/run_linux_ui_tests.py -- ctest --test-dir build -R '^ChimeraABLevelMatchUITests$' --verbose
```

## Unverified / blocked

- **Native UI execution:** this workspace could not start an accessible X11
  display. An initial combined UI probe reached JUCE's no-display monitor
  lookup and crashed. The dedicated UI test now fails explicitly before
  opening a window when no display exists; it is **not counted as passed**.
  [Recorded result](evidence/ab-level-match-20261026/ui-unavailable.txt).
  The new `ChimeraABLevelMatchUITests` CTest entry checks warning/Cancel/enable,
  the SAFE state, and disarming when controls become hidden or close.
- **GitHub CI:** the original head's Open Beta Build and Test and other
  applicable workflows remained queued throughout the audit. They had no
  passing Windows/macOS result to rely on. The PR must obtain successful
  results for its updated head; previous-source results cannot be inherited.
- **Full local suite:** not rerun. The original author's 64/65 report and CAB
  timing rerun are historical author evidence, not fresh results from this
  audit. No performance threshold was relaxed and no failure was hidden.
- **Actual DAWs/devices/listening:** not performed. Verify Windows/macOS
  VST3/AU/AAX flag delivery, punch-in and record starts, offline bounce/freeze,
  realtime exports, actual A/B playing, and the unity step on print entry.
- **Undetectable capture:** realtime bounces, OBS capture, external recording
  and hosts that omit recording flags can still capture a MATCH cut. Automatic
  detection is impossible from the available host flags. The explicit warning
  and manual OFF procedure remain necessary; this is not a true monitor bus.
