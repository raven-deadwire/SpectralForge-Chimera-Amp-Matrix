# PR #222 tuner scheduling and integrity review

Review date: 2026-10-30 KST. **The owner explicitly approved the exact
1200/1500 ms fixture and its digest registration after this review.**
Approval applies to this fixture only; final CI and the independent CAB issue
remain separate merge requirements. See the [approval receipt](evidence/tuner-stall-20261030/owner-approval.json).

## Approved change

Keep #205's 1200 ms forced worker pause and 1500 ms resume deadline. Restore
neither 450 ms nor 500 ms as the final fix. The shorter fixture reproduces a
previous CI race in three independent delay profiles; the longer fixture passes
each profile. Both retain bounded deadlines and still reject an excessive delay.

The owner replied “승인” to the explicit question covering the two scheduling
changes and exact digest registration. Retain the exact
`Tests/PrecisionTunerIntegrationTests.cpp` blob at
`f0ba8e9c9f9eca16553802e50f612d53c2ef51e9` and register:

```text
old SHA256: 1371469c87ec73b0c2edd24d3daebc5342d026d2bdbdcf456be28581bc1ecd60
new SHA256: 8f71de6fc444877dd306d4c9d2483ed5262557d513fdb1c32a489e629298c7fc
```

The reviewed diff to that exact blob is #205's two-line explanatory comment,
`pauseWorkerForTest(450)` -> `pauseWorkerForTest(1200)`, and the resume-loop
`milliseconds(500)` -> `milliseconds(1500)`. No assertions are removed. The
250 ms observation expiry, 280 ms stale wait, 250 ms pause acknowledgement,
<290 ms acquisition/settling, <0.1 cent accuracy and all failure reporting stay.
The two waits are fixture scheduling budgets, not product accuracy/latency gates.

During this review, concurrent commit `10641acd2e7601e97cdcadc61d9c354aa57b505d`
changed the fixture to the long version, registered the new pin, added mutation
checks, and described the investigation request as owner approval. The quoted
approval could not be established from the available original user messages.
Commit `70d2b5b` preserved that long candidate and mutation tests, restored the
last approved pin, and corrected the premature approval claim. Its Python suite
was blocked at the exact-file assertion (22/23); that historical result remains
in the evidence. The owner subsequently supplied the explicit “승인” reply.
The new pin is now registered under that reply, not retroactively under the
earlier investigation request. The post-approval Python run passes 23/23. A
passing diagnostic or agent-authored PR is not a substitute for owner review.

## Source identity and policy boundary

- Reviewed PR #222 head: `9163ec6aa2d9591f592c6b4a8765b70bcc6f07bd`.
- Its recorded base: `2b4bf53a9b3dd94fb0b6e7458633f9651292ea68`.
- #205 head: `f0ba8e9c9f9eca16553802e50f612d53c2ef51e9`.
- The tuner engine headers `GlobalDSP.h`, `PrecisionTuner.h`, and `TunerTiming.h`
  have no diff between these two PR heads. Both fixture variants were compiled
  against #222's same engine, GCC 13.3, JUCE 8.0.8, Linux Release.
- Both historical fixture blobs use CRLF. Their normalized LF hashes also
  differ: this is a real source change, not a checkout line-ending discrepancy.

`Tools/preset_maintenance.py` says: “Permit only this reviewed diagnostic-I/O
change; do not allow arbitrary tuner fixture/gate edits.” Its exact blob digest
is an exception to the **preset-only beta.1 source scope**, not general approval
of every feature on main. `49432103f2974d93cb760c39db5126979e3bb0bd` records the
previous owner's review of the audio-time route fixture. It does not approve
#205's later scheduling change.

Recomputing a hash is mechanical. **Registering changed test semantics as
reviewed is the approval-bearing step.** No arbitrary hash acceptance, generic
fixture allowlist, automatic re-pinning, skip, or weakened failure threshold is
appropriate. The original investigation request preserved this boundary. The subsequent
explicit approval satisfies it for the exact file and two timing values only.

The preset-maintenance workflow was **skipped**, not passed, on both PR heads:
feature PRs without its `preset-maintenance` label do not run that workflow.
Passing its 23 Python unit tests does not demonstrate that current main satisfies
the complete preset-only publication scope, seven-workflow gate, exact-source
approval, or release requirements.

## Historical CI evidence

| Evidence | Observed result |
|---|---|
| [#183 run 40531695044, attempt 1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40531695044/attempts/1), macOS job `122285928713` | Integration test failed after 45.47 s: `FAIL Worker resumed before stale observation check`. The route matrix immediately preceding it reported `failed=0`. |
| Same run, attempt 2 | macOS worker job passed. A successful rerun does not remove the first failure. |
| [#205 run 40643391491](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40643391491) | All three `precision-tuner-worker` platform jobs passed with the longer fixture. |
| [#222 run 40984447101](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40984447101) | All three worker jobs passed the configured repeat-until-fail:3 step with the shorter fixture. Ubuntu native-navigation still failed on CAB appearance. |

The first-attempt macOS artifact was downloaded, not inferred from a review:
artifact `12768324009`, ZIP SHA256
`28fd7bea6d66aa041a5cf2a295113cab1fc030fdc8df67318ec1fc49c555c90c`.
Its `Testing/Temporary/LastTest.log` and `tuner-ctest.log` contain the failure.
The job-log API failed to return that old job; the original artifact supplied
the direct evidence. See the retained [excerpt](evidence/tuner-stall-20261030/historical-macos-failure-excerpt.txt).
No historical Windows resume-timeout failure was established in this review;
that risk is reproduced below rather than asserted as an observed CI incident.

## Controlled scheduling regression

The tool compiles both exact historical fixtures using the native tuner's CMake
flags/JUCE objects. Baseline uses exact fixture bytes. Separate diagnostic copies
inject delays only into the test producer/stale sleep or the test-hook region
immediately after the worker pause ends. They execute the original three
`stalledWorkerFreshness()` scenarios and their assertions. No gate, clock,
frequency/confidence result, tracked source file, or approval pin is replaced.
These are controlled Linux scheduling experiments, not actual macOS/Windows VM
reproduction or a platform flake-rate estimate.

| Profile, three executions each | 450/500 ms | 1200/1500 ms |
|---|---:|---:|
| Exact fixture, normal scheduling | 3/3 pass | 3/3 pass |
| 8 ms added after each queued push (40-block scenario: ~320 ms added) | 0/3 pass; premature resume | 3/3 pass |
| 270 ms overshoot added to the 280 ms stale sleep | 0/3 pass; premature resume | 3/3 pass |
| 400 ms scheduling delay after the worker pause, before completing an iteration | 0/3 pass; resume deadline | 3/3 pass |
| Negative control: 1400 ms overshoot added to the stale sleep | 0/3 pass | 0/3 pass |

All 30 executions matched the specified outcomes; the negative controls remain
failures. The three ordinary slow profiles alone give **short 0/9 vs long 9/9**.
Normal complete integration also passed for both exact fixture variants: each
executed 108 kernel cases and 32 A/B routes plus lifecycle/stale/trace checks.
The short native CTest run took 45.03 s. The long full-suite raw log is retained;
neither result is real DI, GUI/host latency, or listening acceptance.

The nominal stale-check margin grows from `450-280 = 170 ms` to
`1200-280 = 920 ms`, before producer/acknowledgement overhead. The resume timeout
starts after the stale check, while part of the pause remains. Its nominal
additional scheduling margin is therefore roughly `500-(450-280) = 330 ms`
versus `1500-(1200-280) = 580 ms`, not an unconditional extra second. All finite
timeouts can fail on a sufficiently stalled host. A future handshake-based
fixture would be a separate test-semantic change requiring its own review.

Reproduce from a Unix Makefiles Release build:

```sh
cmake -S . -B build-review -DCMAKE_BUILD_TYPE=Release -DCHIMERA_BETA_QUALIFICATION_ONLY=ON
cmake --build build-review --target ChimeraPrecisionTunerIntegrationTests --parallel 2
ctest --test-dir build-review -R '^ChimeraPrecisionTunerIntegrationTests$' --output-on-failure
python3 Tools/run_tuner_stall_regression.py --build-dir build-review --output ../tuner-stall-results --repetitions 3
../tuner-stall-results/long-exact/probe
```

Do not use these diagnostic binaries as product/package artifacts. The output
directory must be outside the repository. [Results and source hashes](evidence/tuner-stall-20261030/results.json)
and [evidence manifest](evidence/tuner-stall-20261030/manifest.json) identify the
tested source; later commits do not inherit whole-PR CI success.

## Integrity regression

| Isolated test condition | `Tools/test_preset_maintenance.py` |
|---|---|
| #222 exact short fixture, existing pin | 23/23 pass |
| #205 exact long fixture, existing pin | 22 pass; exact-reviewed-fixture test fails |
| Long fixture plus proposed exact pin, in a disposable copy only | 23/23 pass |

The isolated candidate row also exercises the existing rejection cases for changed acquisition,
accuracy, omitted failure reporting and clock calculation. It proves the
mechanical candidate preserves those checks; that experiment did **not** grant
approval. The later explicit approval and post-approval test output are recorded
separately in `owner-approval.json` and `approved-policy.log`.
The tracked fixture, production header and policy hashes before/after the
30-run experiment were identical. The subsequent concurrent candidate and
this approval-boundary correction are separately identified above.

## Separate CAB/amp-head appearance failure

The failing assertion compares the rendered head with an independently decoded
authored shell, pixel for pixel, at model 1. It is called from both
`ChimeraCabPanelStateTests` and `ChimeraAmpHeadControlTests`. The assertion's text
mentions an extra roof/feet/handle, but does not itself prove any of those visible
defects; interpolation, render context and exact pixel mismatches still need
isolation.

- #222 Ubuntu signal-path job `123922392202`: CAB panel state fails at that
  assertion; the other 15 tests pass.
- [Base-main amp-head run 40983673701](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40983673701),
  Ubuntu job `123829509748`: identical model-1 assertion in
  `ChimeraAmpHeadControlTests`, before #222's tuner change. Windows/macOS jobs pass.
- The same appearance-test and `CabHeadView.h` bytes are also present at the
  observed later main `a65be23`. The tuner-only rollback does not alter their
  source, asset selection, or test call paths.
- A reduced Linux/GCC13/Xvfb diagnostic loading the same PNG files and running
  `checkHeadAppearance()` passed all 26 models. Model 1 had zero differing pixels
  at 240/640/1120 widths. It used JUCE helper objects and file loading in place of
  embedded resources, so it is **not** the full native product test and cannot
  clear CI. The precise full-build/platform cause remains unresolved.

Impact: the native CAB/amp-head validation remains red and prevents an all-green
merge judgment. It is independent of the tuner rollback; no evidence here shows
a tuner/DSP regression or justifies relaxing pixel equality. The follow-up should
capture actual/expected images and differing-pixel/transform diagnostics in the
failing Ubuntu product build, preserving the assertion and thresholds.

## Merge disposition

**Owner review and exact digest registration are complete.** The owner approved
1200/1500 ms and the linked exact digest after seeing the comparison and failure
reproduction. The Python protection suite passes 23/23 without changing its
acceptance mechanism. Post-approval native validation also passes 2/2
(`ChimeraPrecisionTunerIntegrationTests` and `ChimeraTunerWorkerTimingTests`,
47.39 s total); see [output](evidence/tuner-stall-20261030/approved-native.log). Retain all prior failed-attempt evidence.

Final-source platform CI and the independent CAB appearance failure remain
outstanding merge conditions. Refresh against the then-current main before
merging if needed; no old whole-PR CI result is relabelled as a new-source pass.
No merge, publication, or future test-change approval is implied by this receipt.
