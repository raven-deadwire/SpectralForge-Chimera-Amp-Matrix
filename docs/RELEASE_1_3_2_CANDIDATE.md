# 1.3.2-beta.1 integrated candidate

Publication and deployment were requested by the owner on October 14, 2026
(Asia/Seoul), including the 1.3.2 roadmap scope and the signal/tuner update.
This candidate is not yet published. Existing released tags/assets stay immutable.

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
