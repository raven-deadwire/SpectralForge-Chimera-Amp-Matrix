# PR #159 release-gate security and reliability audit

Reviewed on 2026-10-26 KST. No publication workflow, tag creation, draft release,
asset upload or public release operation was executed during this review.

## Scope and findings

Original PR head: `290833ad5f0f4830ecb0206d456a07dd574e1eba`.
Integration base: `d6d701a7b810f15c8146bbecde5b7ce2e4811c6e`.

1. **CI job provenance and rerun race — fixed.** The original weekly publisher
   selected a run/attempt and then queried `jobs?filter=latest`, validating only
   job names and successful completion. A fixture with a different source SHA,
   run ID and attempt was accepted. Jobs now come from the explicit attempt
   endpoint; required jobs must match all three identities and the workflow
   path must match the requested workflow. Run status/attempt is read again
   after collecting jobs and artifacts. Tests fail on the original code and
   pass with this change. This was a verification weakness; no live unauthorized
   publication was attempted or demonstrated.
2. **Gate success during evidence changes — fixed.** The read-only gate now
   rechecks CI run/attempt/artifact identities and the approval receipt after
   ZIP verification. Replacement or a rerun blocks gate success. The publisher
   already rechecked before staging and immediately before public visibility;
   those checks remain intact.
3. **Malformed gate shape — hardened.** Non-object JSON and malformed producer
   objects are explicitly rejected with validation errors instead of incidental
   attribute errors. Both old and new paths stop execution; this was not a
   fail-open finding.

## Regression evidence

`Tools/test_publish_release.py` expands from 41 to 51 tests. New tests cover
selected-attempt API requests, wrong job source/run/attempt (including wrong
numeric types), wrong workflow, state changes during job/artifact lookup,
approval replacement, and archive-to-entrypoint failure paths. The latter mock
remote I/O only: the approval validator, ZIP parser, SHA-256 checks, per-check
recalculation and embedded raw-evidence validators execute normally.

| Failure or policy case | Result |
| --- | --- |
| BLOCKED with STRICT_PASS approval | Gate and publisher reject before candidate extraction or release mutation |
| Exact beta owner-gap receipt and intact raw archive | Gate allows policy exception; strict result stays BLOCKED |
| Reused receipt with different version, SHA or profile | Rejected |
| Missing/false/string/integer approval flags | Rejected |
| Wrong source, CI run or attempt in evidence | Rejected, including with gap approval |
| Expired, duplicate, malformed or wrong-version artifact metadata | Rejected |
| Digest mismatch, missing checks, forged PASS, missing raw evidence | Rejected |
| Pending, queued, waiting, cancelled, skipped or failed newest CI | No fallback to older success |
| Duplicate JSON keys, non-finite constants, invalid JSON and non-object JSON | Rejected before publication |
| Missing publish needs dependency or failure-forcing condition | Workflow contract fails |
| CI change after staging | Existing publisher callback blocks public PATCH |

Local validation on the integrated main tree: 269 tests across the publisher, historical publisher,
beta/weekly approval, publication evidence, consolidator, packaging and preset
maintenance suites; 266 passed, 3 Windows-only packaging tests skipped locally.
The initially missing `cmake` executable was an environment PATH issue; rerun
with the installed CMake binary passed. Fresh GitHub Actions results for the
final PR head must be checked separately; prior-head green runs are not carried
forward as final-head evidence.

## Actual original-head artifact replay

- Build run: [40186536329, attempt 1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40186536329)
- Artifact: `12601082731`, `Chimera-A-stage-release-gate`, 2,078,932 bytes.
- Downloaded ZIP SHA-256 matched the GitHub API digest:
  `eba3d12b77cbf248af17cc01edbe4e1ab46f6318bb37a2115f4e3a13b7214442`.
- Raw-bundle replay reproduced **21 PASS / 107 BLOCKED** with no publication
  decision. The committed historical approval was rejected for this PR source
  with a version/source identity mismatch.
- Original-head workflow inventory: four SUCCESS, one SKIPPED (preset maintenance).
  This is not evidence that five publication workflows executed successfully.

## Policy boundary and other entrypoints

The gap exception is an existing beta policy, not satisfaction of issue #21's
literal strict-PASS requirement 3. It is restricted to the exact beta version,
full product SHA, profile, policy/map fingerprints, 21 passing automatic checks
and the 107 enumerated unexecuted gaps. It cannot cover observed failures,
arbitrary missing checks, RC/stable releases or another source. Strict gate
JSON, check reports and disclosure retain BLOCKED; policy/waivers were not changed.

Approval is scoped to source/version/policy, not one CI run number. Reusing it
for a newer successful attempt of the same source is existing policy, provided
all evidence is recomputed and bound to that new run/attempt. No wall-clock TTL
is currently required; expired artifacts and stale identities are rejected.

The weekly publisher checks the manual invocation, workflow path, default branch,
controller SHA, full confirmation string, and source/controller tree equality
except the approval file. The dedicated gate is read-only; only the dependent
publish job has contents write permission. Its ordinary success dependency is
preserved, without always/failure/cancelled or continue-on-error bypasses.

Historical publishers remain fixed to historical versions/branches/tags and
cannot publish the current target through this weekly entrypoint. Preset
maintenance is a separate, explicitly scoped approval path with `presets-`
tags and retained baseline acceptance; it does not grant weekly product-release
approval. Existing historical source/workflow revisions are not retroactively
secured by changes to main. No existing tags/assets were moved or replaced.

## Remaining requirements and trust boundary

- At review time GitHub reports `main.protected=false` and no repository rulesets.
  The receipt is repository-controlled JSON, not a signed owner identity proof.
  A writer able to change trusted code or approval records can subvert that
  trust boundary. Required owner review / protected workflow and approval paths
  should be configured separately; this audit did not change repository access
  policy or fabricate a new approval.
- Source-bound same-DI, listening, real-instrument, capture and commercial-host
  evidence (B*, E2, I2) remains missing. The 86 undefined A-check assertions and
  their producers remain separate work. Issue #21 stays open; strict acceptance
  is not completed by successful CI or this merge.
- Live publication and live GitHub failure injection were deliberately not run.
  The gate/publish API flow was tested with controlled I/O fixtures, and an
  actual immutable CI artifact was independently replayed. GitHub API snapshots
  cannot provide an atomic lock against a concurrent administrator action.
