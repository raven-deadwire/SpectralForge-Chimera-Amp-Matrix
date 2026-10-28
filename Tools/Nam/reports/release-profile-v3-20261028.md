# Náströnd A2 release profile v3, 2026-10-28

[한국어](release-profile-v3-20261028.ko.md)

## Why

Profile v2 asks for full ESR below 0.01 and active-window p95 and worst below
0.01 and 0.02. Two findings make those limits unreachable for the high-gain
channels in TONE3000:

- TONE3000 loads only A2 models, width 3 or 8
  ([tone3000-a2-only-20261028](tone3000-a2-only-20261028.md)).
- At width 8, every lever has been tried: data (now −3.6% per doubling,
  extrapolated floor about 0.017 mean test full ESR), training budget, LR
  schedule, memory, loud crops, amplitude, peak and loss weighting
  ([channel-loss-20261028](channel-loss-20261028.md)). Even width 32, which
  TONE3000 cannot load, left active p95 near 0.07
  ([capacity-diagnostic-20261026](capacity-diagnostic-20261026.md)).

The owner chose to redefine the release criterion for A2 rather than drop
TONE3000.

## What changes

v3 (`RELEASE_V3` in `quality_profile.py`) is a release gate added beside v2.
v2 is unchanged and still reported as the reference-fidelity profile. v3 uses
the same measurements (`fidelity()`: v2 windows, frozen anchors, memoryless
rule) and changes only the active and full-ESR limits, per channel class:

| Limit | v2 | v3 high-gain (Fenrir, Surtr, Nidhoggr, Ragnarok) | v3 bloom (Fimbulvetr) |
|---|---:|---:|---:|
| Full ESR | < 0.01 | ≤ 0.025 | ≤ 0.01 |
| Active median | ≤ 0.005 | ≤ 0.02 | ≤ 0.0075 |
| Active p95 | ≤ 0.01 | ≤ 0.12 | ≤ 0.03 |
| Active worst | ≤ 0.02 | ≤ 0.40 | ≤ 0.20 |
| Quiet residual RMS / peak | −70 / −60 dBFS | unchanged | unchanged |
| Level RMS / peak | ±0.5 / ±1 dB | unchanged | unchanged |

Rules:

- **Both splits must pass**: validation and the reserved test signal.
- **Release also needs the owner's instrument-DI listening acceptance.** The
  numbers say how far a model is from the native DSP, not whether the
  difference is audible on a real guitar.
- Fimbulvetr already meets v2's full-ESR limit, so v3 keeps it.

## Limits of this decision

These limits were set after the results were known, using the measured A2
floor. They certify "as close as A2 gets", not "indistinguishable from the
native DSP". The margins are small for the worst window (Nidhoggr test 0.376
against 0.40) and the high-gain full ESR (Nidhoggr test 0.0224 against 0.025).
The quiet and level limits were not relaxed.

## Verdict for the packaged set

The set in the current TONE3000 test package
([tone3000-a2-only-20261028](tone3000-a2-only-20261028.md)):

| Channel | Validation | Test | v3 numbers |
|---|---|---|---|
| Fenrir | **fails quiet** | pass | **FAIL** |
| Surtr | pass | pass | PASS |
| Nidhoggr | pass | pass | PASS |
| Fimbulvetr | pass | pass | PASS |
| Ragnarok | pass | pass | PASS |

**Fenrir fails the unchanged quiet limit on validation.** One quiet window
at 23.51 s (start sample 1,128,651), a note release just after the input
stops, still inside the model's memory, has residual RMS −70.8 dBFS (limit −70)
and peak −53.3 dBFS (limit −60). No Fenrir checkpoint in any of the last five
runs (280 validation evaluations) passed this window. On test, Fenrir's quiet
residual is −88.8 / −81.9 dBFS.

No channel is release-approved yet: listening acceptance has not been done.
