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
  publication policy. No expiration date has been approved or invented.
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

The beta expiry policy deliberately remains unapproved. CI may use clearly
marked qualification-only metadata, which the publisher rejects. Publication
also requires the existing version/source-specific acceptance policy and exact
source workflow/package evidence. `RELEASE_REVIEW_PENDING` remains until that
review is complete. A release instruction is not a statement that omitted
physical/DAW checks passed.
