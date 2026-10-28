# Phase 5 integration follow-up — 2026-10-28 KST

## Reproduced regression

PR #156 head `caa231dc63fdf61b0a288d3ff6ee5ac87eeb94cd` merges main
`e09f59143d25fbc7ea89a4dc2d9b929cbdfeca08`, including #169 Gate 2 / Expander.
The existing input revision guard kept the main gate when input gain changed
while listening, but `finishGateStages` subsequently used the same invalid
input/rig capture. A soft plan could rewrite the main gate threshold and
Expander; a hard plan could change gate placement and Gate 2.

Added native controller regression: clean/rock/metal × unchanged/manual A-B-A/
host notification-only A-B-A (9 conditions), feeding both production input and
rig capture taps. The unchanged controls must apply the expected soft/hard
plan. Edited cases require exact preservation of all 21 tracked gate-stage
values at apply and undo. Normal apply/undo controls allow 0.001 for parameter
normalization round-trip error. These are synthetic captures, not a DAW test.

The new regression against unmodified `caa231d` fails:
`Gate stages used mixed-gain evidence after input A-B-A`.
See [before log](phase5-before.log).

## Fix and validation

Recheck the input edit revision at entry to `finishGateStages`, after callbacks
from the main gate writes. Abstain and report the reason if the shared capture
contains external input edits. Existing per-parameter ownership and undo guards
remain in use; no atomic arbitration of arbitrary concurrent host writes is claimed.

Linux Release, JUCE 8.0.8, GCC, synthetic qualification build:

- ChimeraAutoSetTests: PASS, 21.90 s, including all 9 new conditions, existing
  72 protected attacks / 18 bright controls, event ownership and production integration.
- ChimeraCommonDynamicsTests: PASS, 2.51 s.
- ChimeraMultibandDynamicsCoreTests: PASS, 1.99 s.
- Total: 3/3 PASS, 26.39 s. [Verbose after log](phase5-after.log).

Commands (Linux development dependencies and Xvfb required):

```sh
cmake -S . -B build-autoset -DCMAKE_BUILD_TYPE=Release -DCHIMERA_BETA_QUALIFICATION_ONLY=ON
cmake --build build-autoset --target ChimeraAutoSetTests ChimeraCommonDynamicsTests ChimeraMultibandDynamicsCoreTests --parallel 3
xvfb-run -a ctest --test-dir build-autoset -R '^Chimera(AutoSet|CommonDynamics|MultibandDynamicsCore)Tests$' -V
```

The initial local build had incomplete generated artwork objects; rebuilding
those generated objects restored linking without changing product sources or
assets. Only the completed native runs above are validation evidence.

## Remaining integration gates

- Await Linux/Windows/macOS Graphical EQ and all required CI on the new exact
  PR head. Earlier `1c8a319`, `caa231d`, or #182 `f4c8a3c` CI does not certify it.
- Merge #156 to main with its expected head, then retarget #182 to main.
- #182 still predates #169. Its integration must include the new Gate 2 and
  Expander parameters in the Gate lock, preserve the independent `gates` option
  alongside strength/locks, and extend lock/strength and attack regression
  coverage before final exact-SHA CI/merge. Do not merge #182 to #156's branch.
- Real guitar/bass DI listening, actual DAW Read/Touch/Latch/Write acceptance,
  and release approval remain incomplete. Native notification tests are not
  equivalent to these approvals.

Tested source file SHA256:
- `Source/AutoSetController.h`: `10cc35021c5db59e1fc72ade045dc7db533dba36985d27a992052ab9ddeddd32`
- `Tests/AutoSetTests.cpp`: `4d00d1dfc85113032061f630063a4f32f71c7716d5dadfdd86d107bbeb64afee`
