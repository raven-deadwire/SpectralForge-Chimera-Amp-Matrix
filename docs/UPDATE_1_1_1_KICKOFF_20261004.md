# 1.1.1 immediate kickoff — 2026-10-04 KST

> Integration update: [Draft PR #14](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/14)
> combines the E670FE/Bass, Gate Range, UI refresh and producer/review branches.
> All four Guitar Signatures are implemented with complete 3891-parameter
> snapshots. Linux full CTest passed 15/15. Pitch low-bin energy and outgoing
> bypass fades are corrected; <=10 ms latency and actual DI/DAW acceptance are
> still blocked. Release policy is now v3. The original kickoff status below is
> historical; see `INTEGRATION_1_1_1_20261004.md` for the current implementation
> and `Validation/Integrated111` for local evidence.

User direction: prioritize E670FE, begin the entire 1.1.1 scope including the
Astra track immediately. E670FE is the sole exception to the frozen-model rule;
later 1.2+ expansion dates and the active 132-model target are unchanged.

Branch: `codex/1.1.1-e670fe-kickoff-20261004`, based on
`9df4493` (PR #7 plus the non-Astra validation producer work). The alternate
catalog-contract branch is not blindly cherry-picked over its competing checker.
The existing Factory 31 and Bass Signature 3 definitions remain unchanged.

| Workstream | Current execution state | Next evidence / action |
|---|---|---|
| E670FE 5 paths, native DSP and 32 controls | IMPLEMENTED, synthetic DSP PASS | Windows UI/state execution; E2 calibration and T.D. EQ ownership |
| Ironball compatibility | Fixed-fixture DSP PASS; old control definitions unchanged | Windows binary/A-B regression newly added |
| CI producer / count contract | Checker and producer tests wired into build.yml | Complete A1–A14 producer/artifact mapping; no blanket PASS |
| Studio One/Cubase/Sonar close/exit P0 | OPEN; existing lifecycle code retained | Reproduce on current Windows VST3; interpret actual lifecycle logs |
| Transpose latency / smearing / CPU | OPEN; prioritized before feature expansion | Fixed-DI + synthetic onset/phase/latency/CPU baselines, then targeted optimization |
| Level loss / post-rig gate / compressor GR | OPEN | Verify gain staging, gate detector and gain-reduction meter from actual audio |
| Four Guitar Signatures and full-state format | SPECIFIED; not yet implemented | Full APVTS+IR+metadata snapshot, append-only identity, audio roundtrip |
| Bass3 / Factory31 preservation | Existing definitions retained | Current-head same-DI and fallback checks |
| IR / installer / manual | OPEN | Current-head import/hash/restore and Windows package acceptance |
| Astra same-DI / reference / listening | BLOCKED where input/host evidence absent | Digital captures + DI fixture, raw and matched renders; no direct reamping |
| Release | BLOCKED | All required current-commit policy-v2 gates |

The four Guitar display names remain **A Path To Alsatia**, **Feel My Wrath**,
**Blackhearted**, **Dark Matters of Throne**. The saved two-track instructions
retain the previously specified Dual/Matrix/PRE/POST chains, same-IR starting
point and Classic A/B fallback. Do not put these in the 32-entry Factory table.
Native amp operation sets legacy lane level to zero in processBlock; new
Signature levels must bind `nativeAmp_c*_outputLevel`, not only `level1/2/3`.
Dual blend and lane attenuation are separate gains; do not interpret a ratio and
an additional dB cut as equivalent settings or silently apply both twice.

Both saved roadmap prompts were updated with this scope. After the initial
automatic approval review rejection, the user explicitly approved the exact
two immediate automation runs on 2026-10-04 KST. Both requests were accepted:
`6abdd368c41481918bc159ea6496fbaf` (development) and
`6abdd39455c88191a779a25b8c2322b0` (Astra). Acceptance confirms an asynchronous
run request, not execution completion, model availability or delivered results.
Existing recurring schedules remain unchanged.

No external captures, user recordings or copyrighted IR audio are included in
this branch. Missing actual DAW/capture/DI evidence remains BLOCKED.

## External execution boundary

The source, tests, artwork and evidence are committed locally. The latest main
inventory update was merged locally without conflict. After the initial
automatic approval review rejection, the user explicitly approved publication
of this review branch, a Draft PR, Windows CI and the two immediate automation
runs on 2026-10-04 KST. Main merge and public release remain outside that approval.
The terminal has no GitHub credentials, so publication uses the connected
GitHub app. Its commit metadata may differ from the local commits; the complete
Git tree must match before opening the Draft PR. CI results must be checked at
the resulting remote commit, not inferred from the earlier local evidence.

Published review: [Draft PR #10](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/10),
stacked on PR #7. Initial remote commit `cfab2a6` has the exact local tree
`e0010ecfbb8d833cbcba8720335275e7869245fa`. Build run 37135391488 and candidate
run 37135391502 started. The candidate's preparation check found the new E670FE
panel missing from its separate silent-prototype inventory; the follow-up adds
it and preserves disabled legacy Ironball recall. That prototype now stores
24 amp descriptors / 81 total descriptors, with 23 active amps; its historical
FX inventory is not the production AMP/PRE/POST catalog. Locally, 73 descriptor
tests and 30 amplifier-state cases pass. Actual CI completion is still pending;
the PR and Actions logs track the latest head and results.
