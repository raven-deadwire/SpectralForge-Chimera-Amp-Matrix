# Auto Set integration review: PR #156 prerequisite

Review scope: `a5d24601aa7880f009d741263ba3b4b9fbebb7f7` (#156),
then `757ce48e830b4c0573da41a25e0b7383d125edac` (#182).

## Findings and changes

- Equality of the current parameter and snapshot is not an ownership test.
  A manual gesture or host automation can change A -> B -> A while listening,
  or return to Auto Set's applied value before UNDO. Both must retain ownership.
- Add a lifetime-bound parameter listener with lock-free edit revisions and
  active-gesture tracking. An external edit revokes that parameter for the run.
  Each Auto Set notification is suppressed only on its own thread and only once;
  concurrent host notifications are not suppressed by a global writing flag.
- Check ownership again after beginning the host gesture, and do not reclaim
  writes if a host edits during the value or end-gesture callback.
- Freeze run options at start; later option changes affect the next run.
- If input gain is edited during its capture, retain the gate. Mixed-gain
  percentiles cannot safely be corrected using a single end-of-pass gain shift.
  The input summary reports the actual retained/applied value.
- Preserve the #156 note-attack analysis, uncertainty policy, thresholds and
  positive controls without changing their acceptance limits.

## Validation boundaries

Native JUCE notifications, synchronous callback reentrancy and an independent
host notification thread are testable here. These are not actual DAW automation
acceptance. JUCE's public parameter API has no compare-and-set transaction for
an arbitrary simultaneous host write; event guards must not be described as a
sample-accurate arbitration guarantee. Actual DAW Read/Touch/Latch/Write modes,
transport, save/reload, and host-specific concurrent ordering remain acceptance
work. No audio-thread lock or allocation is added by the observer.

Real guitar/bass DI listening remains pending: normal/rapid picking, palm mutes,
sustained bright notes, soft tails, active existing de-esser settings, and
manual instrument selection. Do not infer audio-quality or release approval
from synthetic or native test results.

## Integration order

1. Test the updated #156 source and require its relevant platform CI.
2. Merge #156 into main using a merge commit (preserves stacked ancestry).
3. Bring that prerequisite into #182, retarget #182 to main, recheck the diff
   and relevant platform CI at the final head, then merge #182.
4. Keep real-DI listening and actual DAW acceptance separate from source merge
   and release approval. No release is authorized by this review.

## Reproduction

`before.txt` records the new native regression against the original #156:
`FAIL: Auto Set overwrote a manual A-B-A edit during listening` (exit 1).
The test binary used original #156 production objects and the added regression.

## Local result

Linux x86_64, GCC 13.3, CMake Release, JUCE 8.0.8. Qualification-only build;
no distributable release. `ChimeraAutoSetTests` passed in 18.81 s;
`ChimeraCommonDynamicsTests` and `ChimeraMultibandDynamicsCoreTests` also passed
(3/3, 22.83 s total). Full native output: `native156.txt`; CTest: `ctest156.txt`.
Tests ran on the changed working tree rooted at the original #156 head; the
following commit stores that tested source plus this evidence. The original
#156 cross-platform CI passed, but cannot certify this new source. Updated
Windows/macOS/Linux CI must be checked before merge.
