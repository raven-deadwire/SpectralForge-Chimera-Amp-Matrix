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
