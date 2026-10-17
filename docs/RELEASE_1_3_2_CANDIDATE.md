# 1.3.2-beta.1 integrated candidate

Latest owner instruction (October 16, 2026, Asia/Seoul): fix macOS errors and
proceed through release and deployment. The preceding test-file review granted
release approval after the tuner-button highlight correction, which passed its
Windows UI regressions. General release approval is complete. Native macOS and
exact-source package qualification remain required; no failed/unexecuted check
is promoted to PASS. See RELEASE_APPROVAL_20261016.md and TUNER_MAC_WAKE_20261016.md.
The review marker remains until exact-source qualification is complete; the
publisher also requires the final source-bound receipt and verified CI/package evidence.

The remaining dated sections preserve the earlier qualification history.

Publication and deployment were requested by the owner on October 14, 2026
(Asia/Seoul), including the 1.3.2 roadmap scope and the signal/tuner update.
This candidate is not yet published. Existing released tags/assets stay immutable.

Current preparation update (October 16, 2026, Asia/Seoul): the owner requested
including the PR #71 tuner timing follow-up in release preparation. This update
is local and pending integration; it does not authorize merging or publishing.
This historical restriction was superseded by the owner instruction on October 16: **머지 릴리즈 진행해**. Merge and release are authorized after qualification.

## Included changes

- Primary PRE / HEAD / CAB / TONE EQ / POST / FINAL EQ signal navigation with
  pictograms; a prominent utility dock and independent tuner workspace.
- Precision estimator, linear virtual strobe/needle, Buzz Feiten electric and
  True Temperament targets, custom offsets and existing A4 calibration.
- Focused EQ editing, add/delete/undo/redo, selectable analyzer resolution,
  IN/OUT display and snapshot freeze integrated from the EQ development branch.
- Four shared dynamic EQ nodes, clean compressor, DI gate/expander improvements
  and final live sample-peak limiter. See [common dynamics](COMMON_DYNAMICS_1_3_2.md).
- Beta lifecycle with active-instance grace, state preservation and explicit
  publication policy. The owner approved expiry at **2027-02-11 23:59 KST**
  (**2027-02-11T14:59:00Z**) on October 14, 2026 (Asia/Seoul), in direct
  response to the proposed deadline. The committed lifecycle policy records
  this approval for 1.3.2-beta.1.
- AUDIO / PEAK labels explain callback deadline usage; paired tuner timings
  collect wall time and the same callback's thread clock without hiding spikes.

## Qualification status

Prior branch tests and old source CI are historical evidence only. The complete
integrated source must pass fresh platform checks. New dynamics, GUI integration
and tuner performance are being qualified; no Peterson-equivalent physical
accuracy, realtime safety, signing/notarization or DAW validation claim is made.

The existing 576-case synthetic tuner test passed after reducing redundant
fractional-delay evaluations; worst error remained 0.0592086 cents. This narrow
result does not replace instrument recordings or audio-interface calibration.

The approved deadline is used by both normal and qualification configurations;
rebuilding does not extend it. Previously built qualification-only packages do
not acquire approval retroactively and must not be published. Publication
still requires the existing version/source-specific acceptance policy and exact
source workflow/package evidence. `RELEASE_REVIEW_PENDING` remains until that
review is complete. A release instruction is not a statement that omitted
physical/DAW checks passed.

At the deadline, already active instances keep processing until destroyed;
new instances have muted output. Projects, presets, automation, A/B states and
embedded IRs remain loadable and saveable. The warning starts 14 days before
expiry. This offline policy requires no account or payment server.

## October 14 qualification follow-up

At source `3c0fb8e2a536c6ee1f21bf58844a50b3ae3c29c3`, Windows and Linux
integration, EQ and package jobs passed. The macOS jobs failed the same EQ UI
assertion: a reused node's neutral gain was approximately -0.000001 dB after
host normalization, rather than exactly zero. The test now compares against
the parameter's canonical normalized zero and also bounds its distance from
physical 0 dB; no DSP code or audio performance threshold is changed.
Fresh platform results are required for this correction and the approved policy.

The #69 tuner reconciliation is now included in #68. At source
`1a4a16d48d6ca21c8a3386beaa354d3744e667bf`, all three platforms passed the
32-route worker, 108 paired kernel, 576 accuracy and 18 stress cases, including
stalled-reader expiry. The previously failing macOS acquisition/settling gate
passed unchanged (maximum 269.19 / 240.47 ms against the 290 ms limits).

A further regression reproduced old queued input briefly reappearing when a
paused worker resumed. The follow-up stamps input at publication, instead of
queue consumption, and checks expiry both during and after the pause. See
`evidence/132-tuner-input-age-20261014/` for the controlled failure and correction.
The tuner panel is opaque and limits dynamic repainting to its reading region.
The latest complete source still requires fresh platform/package results and
callback qualification. Expiry approval does not waive performance failures.

The callback investigation also found repeated invariant arithmetic in the native
POST chain. Pure gain/exponential and filter-control caches now avoid that work
while retaining every DSP/meter state update. A frozen pre-cache oracle matches
1,028,160 samples exactly, including transitions and re-prepare. Local narrow
POST benchmarks and unchanged functional contracts are recorded in
`evidence/132-post-cache-20261014/`. This optimization does not by itself establish
that full-processor callback overruns are resolved.

An additional paused-worker case found that one new block could revive a stale
analysis window even when block-end input timestamps were retained. The worker
now clears both retained history and the pending FIFO snapshot before consuming
input after an expired gap, then reacquires from subsequent fresh samples.
Controlled failure/pass and mixed-queue recovery evidence is in
`evidence/132-tuner-gap-20261014/`. Source-specific checks must include this fix.

## October 16 tuner timing follow-up — release preparation

Include the local follow-up based on PR #71 at
`f22b8cd48c0254518df79e070bdeb524ae97f2e7` in the next integrated candidate.
It records per-route selected input-window completion, worker wake/service
delay, estimator wall time and publish age under the existing test-only hooks.
Production callback behavior, polling, priority, FIFO, estimator output and
freshness/generation policy remain unchanged. See
[timing definitions and reproduction](TUNER_TIMING_ATTRIBUTION.md).

Local Windows x64 / MSVC / JUCE 8.0.8 verification:

| Check | Result |
| --- | --- |
| 32 native routes, original acquisition/settling/accuracy gates | PASS |
| Maximum acquisition / settling | 197.9728 / 205.9741 ms; both <290 ms |
| Maximum absolute final route error | 0.00005144126528 cent; <0.1 cent |
| Timing observations retained / dropped | 1,170 / 0; decomposition audit PASS |
| 108 integration kernel cases and existing stale/lifecycle/channel checks | PASS |
| Incomplete window, paused first activation, attribution and trace reset | PASS |
| Original 576 pitch + 18 stress cases; 1,105 frozen comparisons | PASS |

The standalone kernel and timing arithmetic checks also passed with Zig Clang.
These are direct native worker/kernel builds, not full CMake/Product/plugin/DAW
qualification. The [source hashes and raw evidence](evidence/132-tuner-timing-20261016/verification.json)
identify the tested local changes; the PR base SHA alone does not identify them.

Release preparation checklist:

- [x] Include timing implementation, focused tests and CTest registrations in
  the local candidate change set; retain raw route and attribution records.
- [x] Verify local Windows worker gates without relaxing acquisition, accuracy,
  >250 ms stale-input invalidation, first-activation/channel-change or shutdown
  requirements.
- [ ] Integrate the prepared changes into the intended release branch when
  authorized; record the resulting exact source SHA. No remote integration yet.
- [ ] Run fresh focused and full Product verification for that exact source on
  Windows, Linux and macOS. PR #70's reported macOS 3/32 focused and 2/32 Product
  failures are still unresolved; historical passes above do not supersede them.
- [ ] Verify callback performance including retained 96k/64 overruns, real DI
  and listening, actual DAW/device behavior, and exact-source packages under the
  existing release acceptance policy.
- [ ] Complete source/version-specific release review. Keep
  `RELEASE_REVIEW_PENDING`; expiry approval and local test passes do not clear it.

No version, expiry, acceptance policy, release tag, updater or publication state
is changed by this preparation update.

### macOS correction candidate after attribution

The subsequent request to fix the macOS failure produced draft PR #71 source
`4eef53d5b9e8614f59baa2538655315f8cfc7210`. The attribution-only statements above
describe the preceding local revision. This candidate additionally requests
USER_INITIATED QoS on the macOS tuner worker while active, restoring DEFAULT
QoS on disable. The callback, FIFO, 10 ms wait interval, estimator and all
acceptance thresholds remain unchanged. Windows/Linux priority is unchanged.

Instrumented normal-QoS source `4524893` passed the macOS focused matrix, but
publication-to-next-service gaps had median 27.451 ms, p95 52.075 ms and maximum
84.775 ms, compared with a requested 10 ms wait. This motivated the narrow
active-worker correction. The macOS same-binary normal-QoS control is retained
separately, and the candidate focused suite requires three consecutive passes
(until-fail, not retry-until-pass), with every timing population retained.

At this checkpoint the correction's macOS and Product qualification is pending:
[focused run 38870568528](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38870568528),
[Product run 38870568364](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38870568364).
The correction's local Windows 32-route and lifecycle checks passed. Do not mark
the macOS failure resolved or release review complete before native results.

The original #71 source `f22b8cd` did finish its October 14 CI: macOS focused
passed, while macOS Product retained three tuner timing failures; Windows
Product failed CAB expansion integration. These supersede the earlier pending-CI
description and do not erase #70's historical failures. Superseded diagnostic
Product runs are cancelled, not passed. PR #71 is updated remotely and remains
Draft; #70/#68/main are not merged or modified and no release is published.


### Follow-up checkpoint: initiated QoS insufficient

Source 4eef53d passed the Windows and Linux focused suites three times, but
macOS focused run 38870568528 failed 48k/256/27.5Hz acquisition at 293.274ms.
The pitch publication was 282.002ms; observer delivery was 293.274ms.
The USER_INTERACTIVE follow-up is prepared locally with unchanged gates and
passing Windows compilation and focused timing checks. It is NOT macOS-verified.
Remote branch advancement was blocked by automatic approval review because
the original request prohibited publishing. Explicit authorization for a Draft
PR push (without merge or release) is required for its native CI verification.
RELEASE_REVIEW_PENDING remains in effect.

Owner subsequently authorized the push. Draft PR #71 now points to fd1f222219ee0d90b8ed6b989bd512829b5442a4. Focused CI 38875904343 and Product CI 38875904365 are pending; no merge or release occurred.


### Merge and release authorization

The owner explicitly requested merge and release on October 16, 2026 (Asia/Seoul). PR #71 was marked ready for review. The current source is fd1f222219ee0d90b8ed6b989bd512829b5442a4; focused run 38875904343 and Product run 38875904365 remain pending. The stack is #71 into #70's branch, #70 into #68's release branch, then #68 into main. Publication uses the existing exact-source workflow and acceptance receipt. Authorization is recorded; no unexecuted acceptance is marked PASS and no merge or publication has occurred at this checkpoint.
