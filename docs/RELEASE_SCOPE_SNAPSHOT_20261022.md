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

## Upcoming updates (planned)

Targets below come from `DEVELOPMENT_ROADMAP_2026_2027.md` and are **internal
work / candidate targets, not guaranteed public release dates**. Each item ships
only after its own evidence and acceptance gates pass; this section grants no
approval and changes no gate.

**Next milestone — 1.3.3 Room Core (target 2026-11-30 – 12-25):** one shared
room environment (Small/Medium/Large Size, Absorption/Damping, Bypass, per-rig
Room Send) with real early reflections and late decay, a defined wet return,
state/automation/A-B, and Room-Off defaults for old projects, operating across
Classic/Dual/Matrix with measured LOW DI isolation. Implementation and
acceptance are tracked in issue #46; merging the plan does not deliver it.

**Version calendar (internal targets, KST):**

| Version | Window | Main deliverable |
|---|---|---|
| 1.3.3 | 2026-11-30–12-25 | Room Core (shared Size/Absorption/Bypass, per-rig Send, early reflections + decay, LOW DI isolation) |
| 1.4 | 2026-12-28–2027-02-05 | Independent four-band dynamics, PRE Expansion A (6), first constrained signal-order editor |
| 1.5 | 2027-02-08–03-05 | PRE Expansion B (6), drag-order UX; separately gated M/S, external sidechain, six-band dynamics |
| 1.6 | 2027-03-08–04-09 | Five reference amps, independent three-band saturation |
| 1.7 | 2027-04-12–05-07 | Nine POST Dynamics/Tone models, common two-filter modulation |
| 1.8 | 2027-05-10–06-18 | Nine Motion/Space models, advanced delay/reverb, Room Advanced |
| 1.9 | 2027-06-21–07-16 | Nine Creative variants, expanded modulation and routing polish |
| 1.10 | 2027-07-19–08-27 | Dvergr Analog / Seidr Dynamic / Mimir Impact synth layers |
| 2.0 | 2027-08-30–09-24 | Whole-product host/audio/state/performance/preset/installer integration and RC |

**Newly in active development scope** (owner instruction 2026-10-20, issue #97;
`DYNAMICS_SPECTRUM_WORKSPACES_20261020.md`): independent four-band dynamics and a
dedicated de-esser, each edited in its own spectrum workspace. Six-band expansion
keeps its separate gate. The owner's remaining FabFilter references are retained
for later creative/filter/reverb/synth tracks.

**Carry-over items that must gather evidence before entering a release:**
- Transpose quality/latency mode — the measurement/listening **tooling** landed
  (#116: `transpose_measure.py`, `blind_listening.py`,
  `TRANSPOSE_EVIDENCE_PROTOCOL.md`); the isolated-DI bypass-vs-transpose evidence
  itself is still to be gathered before it can be promoted or advertised.
- low-B / −2-semitone transpose and NAM/reference work — continue on their own
  tracks with their own evidence.
- Multiband bypass candidate — currently `REJECT_CANDIDATE`; no beta.2 placement
  without a redesign.

Near-term reviewable checkpoints (roadmap): 2026-10-23 first EQ-focused test
installer; 2026-10-30 a 1.3.1 validation candidate if gates pass; 2026-11-20 /
11-27 the first 1.3.2 test installer and candidate; December the Room Core
prototype → first Windows test installer → 1.3.3 candidate.

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
`MULTIBAND_BYPASS_CANDIDATE_20261021.md`, `DEVELOPMENT_ROADMAP_2026_2027.md`,
`DYNAMICS_SPECTRUM_WORKSPACES_20261020.md`.
