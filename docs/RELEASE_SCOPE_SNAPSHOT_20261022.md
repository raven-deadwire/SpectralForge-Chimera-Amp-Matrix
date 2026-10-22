# Release-scope snapshot — 2026-10-22

A point-in-time summary of what is inside the authorized release scope on `main`
and what is explicitly outside it. This is a reconciliation snapshot, not a new
approval: it records the scope that other committed policies and decisions
already define (see the Sources below) and does not promote any BLOCKED check to
PASS or widen any owner authorization.

Product identity at this snapshot: **1.3.2-beta.1** (`VERSION` = 1.3.2,
`RELEASE_CHANNEL` = beta.1). Approved expiry **2027-02-11T14:59:00Z**
(2027-02-11 23:59 KST); rebuilding never extends it.

## In authorized release scope

### 1. 1.3.2-beta.1 integrated product
Owner authorized merge-and-release **after qualification** (see
`RELEASE_1_3_2_CANDIDATE.md`, `RELEASE_APPROVAL_20261016.md`). Included:

- PRE / HEAD / CAB / TONE EQ / POST / FINAL EQ signal-path navigation and the
  independent precision-tuner workspace (strobe/needle, Buzz Feiten, True
  Temperament, custom offsets, A4 calibration).
- Focused EQ editing (add/delete/undo/redo, selectable analyzer resolution,
  IN/OUT display, snapshot freeze).
- Common dynamics: four shared dynamic-EQ nodes, clean compressor, DI
  gate/expander, final live sample-peak limiter (`COMMON_DYNAMICS_1_3_2.md`).
- Beta lifecycle with active-instance grace and state preservation; committed
  expiry above.

**Not yet published.** Outstanding gates before publication:
- Native **macOS** qualification and **exact-source** package/CI evidence.
- `RELEASE_REVIEW_PENDING` must clear.
- No failed/unexecuted check is promoted to PASS; a release instruction is not a
  statement that the omitted physical/DAW checks passed.

### 2. beta.1 factory preset maintenance
`Validation/preset-maintenance-policy.json` authorizes scope
`preset-only-beta1-maintenance-installers-and-preset-pack` (release tag
`presets-2026.10.19.1`):

- Five rebuilt installer packages plus the optional 48-preset `.chimera` pack.
- **Engine bytes frozen at beta.1** — only the revised factory dynamics/EQ
  preset recipe changes; serializer, product identity, expiry and existing
  limitations are retained.
- Publishes only after the exact-source CI, the five verified packages and the
  three-platform export / fresh-restore succeed. `beta2_or_unfinished_p0_approved`
  and `new_validation_gaps_accepted` are both **false**.

## Explicitly out of release scope (deferred / rejected / beta.2)

- **Transpose quality/latency mode** (merged #115) — deferred out of 1.1.1
  (`TRANSPOSE_LONG_TERM.md`). Present on `main` as a feature-branch spike only.
  The required isolated-DI bypass-vs-transpose listening/measurement evidence is
  **not yet captured**, so it must not be promoted toward a release and must not
  be advertised as low-latency transpose. #116 added the evidence **tooling**
  (`Tools/transpose_measure.py`, `Tools/blind_listening.py`,
  `TRANSPOSE_EVIDENCE_PROTOCOL.md`); the evidence itself is still to be gathered.
- **Multiband bypass candidate** — `REJECT_CANDIDATE`
  (`MULTIBAND_BYPASS_CANDIDATE_20261021.md`); pre-echo worsened. No beta.2
  deployment.
- **AAX native targets / Standalone ASIO** (#101 / #113) — opt-in / development
  targets; not a released-and-qualified claim.

## Retained limitations (carried, not resolved — do not claim)

- Strict product gate remains **BLOCKED** (21 PASS / 107 unexecuted-BLOCKED,
  including 42 blocked hard gates); see `VALIDATION_STATUS.md`. Hard gates accept
  PASS only and are never waived to N/A.
- No actual-DAW acceptance, no DI/listening acceptance, no Peterson-equivalent
  physical tuner accuracy, no realtime-safety guarantee — all BLOCKED.
- Windows is **unsigned**; macOS is **unnotarized**.
- Expiry is fixed; rebuilding does not extend it.

## One-line summary

Shippable today is only "the 1.3.2-beta.1 product on the frozen beta.1 engine,
plus the revised-preset maintenance distribution" — and even those publish only
after macOS and exact-source qualification. Transpose, the multiband bypass
candidate, and AAX/ASIO are beta.2-class and have not passed their own
evidence/verdict gates, so they are outside the current release scope.

## Sources

`RELEASE_1_3_2_CANDIDATE.md`, `RELEASE_APPROVAL_20261016.md`,
`VALIDATION_STATUS.md`, `Validation/preset-maintenance-policy.json`,
`TRANSPOSE_LONG_TERM.md`, `TRANSPOSE_EVIDENCE_PROTOCOL.md`,
`MULTIBAND_BYPASS_CANDIDATE_20261021.md`.
