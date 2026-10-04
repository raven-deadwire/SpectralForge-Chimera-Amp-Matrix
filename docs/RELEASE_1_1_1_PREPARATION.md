# Open Beta 1.1.1 — release preparation

Status: publication authorized by the user on 2026-10-04 at 15:11 KST, conditional on completed validation. The later AMP/PRE/POST and complete factory-bank corrections supersede the previously built candidate. At 2026-10-04 21:24 KST the user explicitly confirmed that the previously listed manual checks were completed. Manual beta acceptance is recorded as owner-reported; no raw measurements or host-version matrix are inferred. Final-source automated CI and package/provenance checks remain required. No additional publication approval is required.

## Identity

- Product / Windows DisplayVersion / native resources: `1.1.1`, from `VERSION`.
- Package and in-app version: `1.1.1-beta.1`, from `VERSION` + `RELEASE_CHANNEL`.
- Display name: `Open Beta 1.1.1`. Every package retains its exact source SHA and CI run ID.
- Candidate artifact: `SpectralForge-Chimera-Open-Beta-1.1.1-Release-Candidate`.
- Intended new tag: `v1.1.1-beta.1`.

## Included scope

Gate Range; dirty/visible UI refresh and scoped meters; E670FE with legacy Ironball state/DSP preserved; Bass signatures and four Guitar snapshots; state/gain/GR regression coverage; Windows VST3 live-instance removal harness; RIGS reference model names without the prefix. Version consistency fixes from PR #13 are applied as a three-way merge, preserving the integrated candidate tests and validation producers.

Transpose low-latency and real-DI quality improvement is deferred by the user's 2026-10-04 14:45 KST decision. See [the follow-up contract](TRANSPOSE_LONG_TERM.md). The underlying compatibility and synthetic regression tests still run.

Additional user-requested scope: lead/high-gain saturation, native factory AMP/PRE/POST recall, all 38 factory output voicings and category navigation, bass EQ specifications, BDDI Blend/EQ routing and Cali76 timing/parallel level behavior. These changes do not certify original hardware curves.

## Verification before publication

1. Require successful Windows/macOS/Linux builds and complete configured CTest inventories on the final source commit; compare exact SHA, not just a green earlier run.
2. Require Windows Setup install/repair/uninstall, version-resource/registry checks, Defender scan, and macOS/Linux package verification. Preserve personal presets/IRs and the existing plugin identity.
3. Verify the five packages, SHA256SUMS, update-beta.json, payload manifests and candidate-source.json agree on version, source and bytes.
4. Keep policy v4 `beta_1_1_1` machine-readable evidence separate from the owner's 21:24 KST manual beta acceptance. The owner confirmed the previously requested DAW close/remove/reopen/soak, DI/Gate/E670FE and timing checks. Record this as owner attestation; do not fabricate numeric reports or claim that CI performed those checks.
5. Review [release notes](OPEN_BETA_RELEASE_NOTES.md), including the transpose limitation and the scope of owner-reported host acceptance, before public release.

The existing `publish-beta11.yml` / `publish_beta11.py` and old release branch are pinned to the already published 1.1.0 beta. Do not use them to publish 1.1.1 or replace old assets. Prepare the 1.1.1 publication action only for the accepted exact source and verified artifact; the user’s conditional publication instruction is already recorded above.

Local Linux CMake product builds and the configured CTest inventory are available for the control/preset correction. See [control audit](CONTROL_AUDIT_1_1_1.md) and its source-hashed evidence. Windows/macOS, Windows resources/installer/Defender and UI probes still require the matching final-source CI; older green runs are not evidence for this correction.
