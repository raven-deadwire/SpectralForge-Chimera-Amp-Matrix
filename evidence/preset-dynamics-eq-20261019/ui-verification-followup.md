# Factory preset UI verification follow-up

The first committed preset source was
`ef74c99ac8679f7d021977b90b86bc4481705143` in PR #108.
Its Linux signal-path run
[39247657645](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39247657645/job/118043427039)
passed 11 of 12 CTests. `ChimeraSignalPathUITests` failed after factory preset 0
because it still required both factory EQ path nodes to be inactive:
`Factory recall left EQ path enabled`.
The retained [CTest log excerpt](remote-first-signal-linux.txt) records the
individual results and the failing assertion.
Only trailing spaces on four timestamp-only lines were removed for the text
archive. The original excerpt SHA-256 is
`ee9a3f1288cb128d398836ed6ae54a0e19f85a1cb6683fb95d0d1b5587956632`.

The same source's separate Linux Graphical EQ job
[118043427366](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/39247657587/job/118043427366)
passed all 9 selected CTests. `ChimeraCommonDynamicsUITests` also passed in the
signal-path job. The latter is extended to exercise an explicitly recalled
factory preset; its original fresh-processor test did not fail.

## Corrected UI contracts

- For every one of the 48 recalled presets, compare the displayed Tone/Final
  EQ active state and enabled-band count with the processor's actual parameter
  state. Also compare common compressor and output limiter activity. Existing
  checks still require navigation to preserve every audio parameter and produce
  no host writes or gestures.
- Establish the later one-band automation fixture explicitly across all 12
  Tone EQ bands. Preserve its existing `ACTIVE / 1 BANDS`, A/B, and project
  restoration assertions.
- Recall factory preset 0 before opening the common dynamics workspace. Check
  that its compressor switch displays the saved state and toggles in both
  directions while every other parameter stays unchanged. Preserve the
  audio-neutral workspace opening, four display scales, and non-destructive
  dynamic-slot assignment checks.

An independent audit of the remaining test headers and C++ files found no
additional factory-finishing assumptions requiring correction. The maintenance
source guard adds only `Tests/SignalPathUITests.h` and
`Tests/CommonDynamicsUITests.h` to its explicit test-file list. New regression
cases also reject adjacent, unapproved test headers. All 17 maintenance
contract tests pass.

## Local follow-up result

Both targeted CTests pass: `ChimeraCommonDynamicsUITests` in 6.52 seconds and
`ChimeraSignalPathUITests` in 110.24 seconds (116.76 seconds total). The latter
completed all 48 preset recalls and the final automation, A/B, project restore,
and editor recreation scenarios. The [JUnit report](ui-followup-ctest.xml) and
[input manifest](ui-followup-input-sha256.json) record the test results and
source hashes. The local UI target was built with native UI probes and executed
under Xvfb/Openbox.
The [complete CTest output](ui-followup-complete-ctest.txt) retains every preset
and restoration phase beyond CTest's truncated successful JUnit output. It is a
byte-identical copy of the input manifest's `ui-followup-complete-ctest.log`,
stored with a `.txt` extension for this evidence archive.

These corrections change test fixtures and their permitted source paths. The
preset recipe remains SHA-256
`ebd2e57b8dc554f813814a5c1bf3292e53d56cf3955a7634b026e2abce9764bd`.
The existing audio measurements, output and restoration bounds, product engine,
parameter schema, and seven required publication workflows are unchanged.

The superseding commit must pass the full required CI matrix and artifact
verification before publication; the failed first source is not a release
candidate approved for distribution.
