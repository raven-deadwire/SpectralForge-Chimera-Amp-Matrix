# Release evidence regression fixtures

`e1-ctest-6782.log` contains the complete, unmodified `ChimeraUITests`,
`ChimeraAmpNativeTests` and `ChimeraGateProcessorTests` sections extracted from
the Windows `LastTest.log` in the consolidated artifact for this checkpoint:

- Source: `6782e4b6ead29bf4a31243b2e05a535c38e16745`.
- Product workflow: [run 37942149681, attempt 1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37942149681/attempts/1).
- Artifact: `Chimera-A-stage-release-gate`, ID `11624598393`.
- Original ZIP: 206047 bytes, SHA-256 `ecfc748dd9900dd6479b9f3f6257e84bd04ae6bac88ed17b4acb8773806ad77f`.
- Windows full raw log SHA-256: `8b96853c234ed09b220831b4f4efc4ed088ebd6d62563088cb9ce221b614a7f2`.

The ZIP digest matches the Actions artifact metadata. Its four evidence bundles
also passed source, run, attempt, policy and raw-file hash verification. The
Linux and macOS logs report the same native coverage totals as Windows.

The checkpoint's original consolidated result was **19 PASS / 109 BLOCKED**.
Its completed tests covered 26 serialized / 25 active amps, 407 native controls,
183 channel routes and 104 oversampling paths. The Windows UI covered 26
serialized panels, 122 channel cases and 2246 channel/control visibility cases.
The imported E1 map still required the smaller pre-Niflheimr totals and rejected
those completed assertions. The current map requires these exact larger totals;
its control failures, allocation, gain/default, isolation, completion and legacy
compatibility requirements remain intact.

`e1-ctest.log` retains the earlier excerpts from run `37429791345`. It is now a
negative coverage fixture: a complete old test run cannot certify the appended
amplifier even when its test cases are marked green. Tests also replace each
current coverage total individually to ensure a broad numeric match cannot
silently admit the previous coverage.

These files are regression inputs, not acceptance receipts for a later source.
Tests construct their own synthetic bundle identity and inventory around the
fixed excerpts. Production evidence must come from the exact current CI source,
run and attempt. Replaying the verified checkpoint bundles with the corrected
map yields 21 automatic passes while all 107 unowned or external acceptance
checks stay blocked; it does not replace a fresh CI run or authorize publication.

`release-policy-1.3.0.json` and `release-policy-1.3.1.json` retain the strict
policies used by the historical beta publisher regression suites. The latter
is the unchanged policy for `1.3.1-beta.1`, whose canonical JSON fingerprint is
`b21f8fc7a6f6b5f20a33a4e662d132eb471c54958ffa4801e57eb59cf0f603e2`.
These frozen inputs keep historical approval tests independent of later weekly
version and channel changes. They are test fixtures; the generic publisher reads
the validation policy from the selected product source and requires a separate
approval for that exact version and source.

`weekly-publication-policy-unapproved.json` freezes the initial empty weekly
receipt for synthetic approval tests. The live approval file may legitimately
change in a reviewed controller commit; that change must not alter test setup
or make the publisher's required regression suite demand an unapproved receipt.

## 1.4 catalogue (E1 coverage totals)

The 1.4.0-beta.1 product run
[43100147222](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/43100147222)
on `b7b7c54f00da3715683016433135178345a0524e` completed every test on all three
platforms, but its consolidated result was **19 PASS / 109 BLOCKED**: `E1.DSP` and
`E1.STATE_UI` still required the 1.3 totals (26 serialized / 25 active amps,
407 controls) while the tests printed the 1.4 totals. The map now requires these
exact 1.4 totals:

- `ChimeraAmpNativeTests`: 31 serialized / 30 active models, 450 controls,
  `CONTROL_RESPONSE count=450`, 201 sample-rate/channel routes, 463 isolated
  inactive controls and 124 oversampling paths;
- `ChimeraUITests`: 31 serialized panels, 134 channel cases, 2362 channel/control
  visibility cases and 9 POST models / 306 controls.

Check ownership, producers, platforms, control failures, allocation, gain/default,
isolation, completion and legacy compatibility requirements are unchanged.

`e1-ctest-1.4.log` holds the `ChimeraUITests`, `ChimeraAmpNativeTests` and
`ChimeraGateProcessorTests` sections of `LastTest.log`, unmodified, from a local
Linux Release build (`CHIMERA_UI_PROBES=ON`, Xvfb) whose `Source`, `Tests` and
`CMakeLists.txt` are byte-identical to `b7b7c54`. Excerpt SHA-256
`94959582ef911a0bc3ee505138c4e118551e5c2b02015330aa90c3e7c3a385df`. The CI
artifacts could not be downloaded from this environment, so this fixture is a
local run, not a CI receipt. Like the other excerpts it is a regression input
only and certifies no source.

`ci-check-map-1.3.json` freezes the map the 1.3.x publishers approved
(`d59f7e55…20f`). Historical publishers and the preset-maintenance check of the
published 1.3.2-beta.1 receipt validate against it. The weekly publisher pins the
live map's digest separately, and the 6782 excerpts now serve as a negative
fixture: complete 1.3 output cannot certify the 1.4 catalogue.

`weekly-publication-policy-1.3.2.json` freezes the 1.3.2-beta.1 owner acceptance
receipt, byte-identical to `Validation/weekly-publication-policy.json` at the
preset-maintenance base controller `ec6fd639` (SHA-256 `ab2a7d552edb8662c6260ea206dea5a19964de86a572d98faee49b486c4da9b4`). The live receipt
now approves 1.4.0-beta.1, so `test_preset_maintenance.py` reads the historical
acceptance from this file instead of the working tree.
