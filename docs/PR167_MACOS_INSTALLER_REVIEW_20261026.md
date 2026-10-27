# PR #167 macOS installer review — 2026-10-26 KST

## Source and status

- Reviewed original PR head: `05137673db2aa1527e371d8386ee6e8a9ae6a92c`.
- During review, the same original changes were found already merged through
  [PR #166](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/166),
  merge `2105a90a61f35b0d905ebf5110e5f8e51e15924d` (2026-10-26 09:55 KST).
- Main at reconciliation: `d6d701a7b810f15c8146bbecde5b7ce2e4811c6e`.
  Its installer files are identical to #167's original head; only two release
  planning documents differ. Preserve those documents and use #167 for these
  corrective changes rather than duplicate the already integrated feature.
- This review does not approve a release. New macOS PKG/CI acceptance is required
  on the corrective commit before merging it.

## Findings and fixes

1. The Distribution declared `require-scripts="false"` while using JavaScript
   for selection and installation checks. Set it to `true`. Apple's
   [Distribution schema](https://developer.apple.com/library/archive/documentation/DeveloperTools/Reference/DistributionDefinitionRef/Chapters/Distribution_XML_Ref.html)
   requires literal booleans for expression attributes when scripts are disabled.
   A successful happy-path CLI install did not test the refusal/selection cases.
2. The mandatory component had no OBS process guard before overwriting its
   VST3. Both component preinstall hooks now refuse running OBS and process
   query errors, before unpacking. The native document preservation helper
   still runs after the guard. The mandatory component also declares `must-close`.
3. Constrain the product to the startup system volume and macOS 12+ in the
   Distribution. Existing compiler/staging checks target macOS 12 and verify
   both arm64/x86_64 slices; actual macOS 12–14 execution remains unverified.
4. The postinstall now validates a nonzero numeric console UID and an absolute,
   existing home, propagates directory-service failures, and checks the helper's
   code seal before invoking it with `launchctl asuser`, `sudo -u`, and a clean
   environment. No Python is required on the installing user's machine.
5. Verify every staged OBS file after expanding the PKG, and verify the new
   preinstall and native document helper bytes, not only the module/manifest.
6. Correct documentation: macOS deselection preserves an existing filter;
   Windows' deselection/removal policy must not be applied to macOS. Root/MDM
   CLI callers explicitly supply choices because their HOME may differ from
   the console user's. Filter removal is per-user and keeps core products and
   the shared package cache/receipt.

## Verification evidence and boundaries

| Evidence | Result | Scope |
|---|---|---|
| Corrective working tree: `Tests/test_packaging_version.py` | 27 run; 24 passed, 3 Windows-only skipped | Distribution invariants, packaging/version contracts; Linux |
| Corrective working tree: `Tests/test_macos_pkg_hooks.py` | 7 passed | Production shell bodies with temporary OS-command substitutes; production JS with mocked Apple API; not real privilege transitions |
| `OBS/tests/test_install_module.py` locally | 22 run; 21 passed, macOS signing test skipped | Linux fixtures; ordinary-user path uses the harness's temporary root-check substitution in this root container; production root refusal tested separately |
| Python compilation, both `sh -n`, workflow YAML parse, `git diff --check` | Passed | Syntax/format only |
| Original head OBS add-on [run 40250780674](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40250780674) | Passed | macOS arm64 and Intel/Rosetta libobs, standalone installer faults/recovery/signature/removal; does not certify corrective PKG |
| Earlier #166 source `0fec5d9`, [run 40231652737](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40231652737/job/121284985208) | 6 jobs passed | Actual macOS PKG install/upgrade, both architecture loads, removal; raw log inspected |

The corrective `Tools/verify_macos_obs_pkg.py` expands the real macOS CI gate:

- Apple choice evaluation for OBS absent, app present, and an existing filter;
  the core choice remains selected and disabled.
- Explicit OBS off: app, VST3, AU and manual installed, no new OBS payload.
- OBS on: console-user ownership of the entire bundle, code-seal verification,
  actual arm64/Intel-Rosetta libobs loading.
- Explicit off after an existing install: no filter removal or byte changes.
- OBS-named test process running: both choice states fail and preserve existing
  core/filter/package bytes. This is not a real OBS UI test.
- Corrupt packaged update rejected while the installed user filter survives;
  removal preserves core products, scenes, another plugin and a personal preset.
- Per-command logs and `result.json` uploaded even on failure.

These are added gates, not claimed PASS results until a run on the corrective
commit completes. The original #167 Open Beta Build run `40250780675` and its
macOS job `121342324934` were still in progress at inspection.

## Precision tuner failure attribution

Raw [failed job 121341983651](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40250780743/job/121341983651)
on original head `0513767` reports, for 44.1 kHz / 256 / 30.867706 Hz:

- feeder lateness 50.19705334, 53.80013634, 56.26651134 ms; limit 50 ms;
- `ROUTE_TIMING_END,failed=0,feeder_invalid=1,repeated_attempts=2`;
- failure: `Feeder lateness invalidated every attempt of a route; tuner timing not judged`.

`Tests/PrecisionTunerIntegrationTests.cpp` separately counts judged DSP failures
and invalid feeder routes, then fails either category. The PR changes no tuner
DSP, tests or signal-path workflow. The prior passing source `0fec5d9` has
byte-identical PrecisionTuner/MacTunerWakeTimer/TunerTiming sources, integration
test, timing probe and worker workflow. Its
[run 40242464947](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/40242464947)
and macOS worker job `121317007555` are successful. Other product/UI sources
differ between those complete commits, so this is not whole-product equivalence.

Evidence supports a test-feeder scheduling invalidation, not a demonstrated
installer-induced DSP regression. It does not establish why the runner was late
or certify the unjudged route. Keep the gate unchanged; await a valid same-head
worker run (the original head's retry was queued at inspection).

## Remaining limits / merge decision

- Native helper atomic rollback covers the user's OBS filter only. Multi-package
  Apple Installer does not implement a single Chimera-wide rollback transaction;
  an earlier component may be updated when a later one fails. Whole-PKG failure,
  disk-full and power-loss restoration remain unverified and must not be claimed.
- Installer GUI, real OBS application/broadcast/editor operation, physical Intel
  hardware, and macOS 12–14 hardware execution are not verified here.
- Code signatures are ad-hoc integrity seals. No Developer ID publisher identity,
  signed installer or Apple notarization is demonstrated.
- Do not merge based only on the old green runs or these local fixtures. Require
  the corrective commit's macOS package gate and applicable CI to finish, and
  keep the whole-PKG rollback limitation explicit.
