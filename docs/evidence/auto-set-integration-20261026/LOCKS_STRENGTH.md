# Auto Set lock/strength integration review

Prerequisite: PR #156 including event-based parameter ownership. Original PR
#182 head: `757ce48e830b4c0573da41a25e0b7383d125edac`.

## Contract

| Control | 25%, 50%, 75%, 100% |
| --- | --- |
| Input | Scale the gain delta, quantised to 0.5 dB |
| Gate | Full detection/range; threshold follows the actual applied constant gain |
| Tone EQ | Scale band gains; preserve selected nodes, frequency, Q, type |
| Compressor | Parallel mix follows strength; other suggested controls unchanged |
| Multiband | Parallel mix follows strength; crossovers/band controls unchanged |
| De-esser | Scale maximum reduction; preserve detection/attack protection |
| Limiter | Full protection settings, unless explicitly locked |

If input gain changes during the listening pass (including A -> B -> A),
retain the current gate: the measurement contains incompatible gain histories.
The user can rerun with a constant input. This conservative exception supersedes
#182's previous claim that every edited input capture can be corrected by a
single threshold shift.

Each of the seven stage locks excludes its parameters from apply and undo.
Locks selected after a run also protect their stages from UNDO; unlocking a
stage that was locked during the run does not create an undo entry. Run options
are frozen at start. New strength, instrument, style, optional-stage and lock
choices apply on the next run. These remain editor-session options, not saved
project parameters.

## Added native regressions

- 25/50/75/100% on identical capture fixtures, with active EQ, multiband and
  de-esser positive controls and all unscaled controls compared.
- Seven individual locks x four strengths, non-default stage values, edits
  during listening, and host edits before undo. All stage parameter IDs are
  checked independently of the controller's touched-parameter list.
- Options changed while running, newly selected undo locks, existing combined
  locks/all-locked rejection, and normal undo of unlocked stages.
- 36 controller attack/lock cases: three styles x four strengths x unlocked,
  de-esser locked, limiter locked. Protected attacks must remain unde-essed.
- Prerequisite regression: manual/host A -> B -> A, active same-value gestures,
  synchronous host changes at begin/value/end callbacks, independent host
  notification thread, and allocation-free edit observation.
- Existing 72 protected attacks, 18 sustained-brightness controls, 36 explicit
  instrument checks, nine ambiguity checks, full processBlock five-pass flow,
  Dynamic EQ node preservation and native dynamics suites remain enabled.

## Merge and approval boundaries

Merge #156 into main first, preserving ancestry, then retarget #182 to main.
Recheck the final head's CI and mergeability before #182 integration. Do not
merge #182 into the prerequisite branch. No force push, check bypass or release
publication is part of this review.

Native synthetic tests do not approve real DI sound or actual DAW automation.
JUCE parameter notifications do not provide an atomic host/Auto Set transaction
for arbitrary simultaneous writes; actual host ordering/Read/Touch/Latch/Write
acceptance remains open. Refer to the prerequisite review for this limitation.
Real guitar/bass DI listening, especially rapid muted attacks, bright sustain,
soft tails and 25% versus 100% gain reduction, remains separately unapproved.

## Local result

Linux x86_64, GCC 13.3, CMake Release, JUCE 8.0.8, qualification-only build.
Auto Set passed in 31.79 s; Common Dynamics and Multiband Core also passed:
**3/3, 36.21 s total**. `ctest182.txt` and `native182.txt` preserve the results.
The run tested the resolved combination of original #182 plus prerequisite
`0c0a817de47e35705039197a7d14455280a2cf2a`, including the new lock/strength
regressions and controller changes. The following merge commit stores that
source plus this evidence. Source merge is still gated on final-head CI.
