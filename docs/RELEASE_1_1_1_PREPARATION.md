# Open Beta 1.1.1 — release preparation

Status: prepared source and release notes; publication is pending validation. No public tag, GitHub Release, main merge or update-channel publication is authorized by the preparation request.

## Identity

- Product / Windows DisplayVersion / native resources: `1.1.1`, from `VERSION`.
- Package and in-app version: `1.1.1-beta.1`, from `VERSION` + `RELEASE_CHANNEL`.
- Display name: `Open Beta 1.1.1`. Every package retains its exact source SHA and CI run ID.
- Candidate artifact: `SpectralForge-Chimera-Open-Beta-1.1.1-Release-Candidate`.
- Intended tag when publication is authorized: `v1.1.1-beta.1`.

## Included scope

Gate Range; dirty/visible UI refresh and scoped meters; E670FE with legacy Ironball state/DSP preserved; Bass signatures and four Guitar snapshots; state/gain/GR regression coverage; Windows VST3 live-instance removal harness; RIGS reference model names without the prefix. Version consistency fixes from PR #13 are applied as a three-way merge, preserving the integrated candidate tests and validation producers.

Transpose low-latency and real-DI quality improvement is deferred by the user's 2026-10-04 14:45 KST decision. See [the follow-up contract](TRANSPOSE_LONG_TERM.md). The underlying compatibility and synthetic regression tests still run.

## Verification before publication

1. Require successful Windows/macOS/Linux builds and complete configured CTest inventories on the final source commit; compare exact SHA, not just a green earlier run.
2. Require Windows Setup install/repair/uninstall, version-resource/registry checks, Defender scan, and macOS/Linux package verification. Preserve personal presets/IRs and the existing plugin identity.
3. Verify the five packages, SHA256SUMS, update-beta.json, payload manifests and candidate-source.json agree on version, source and bytes.
4. Evaluate the policy v4 `beta_1_1_1` profile with current-commit evidence. DAW close/remove/reopen, audio callback timing, real DI/level/Gate listening and E670FE reference acceptance are still outstanding unless new concrete evidence is supplied. The user's general satisfactory-function feedback does not certify untested cases.
5. Review [release notes](OPEN_BETA_RELEASE_NOTES.md), including the transpose limitation and unconfirmed commercial-DAW shutdown resolution, before public release.

The existing `publish-beta11.yml` / `publish_beta11.py` and old release branch are pinned to the already published 1.1.0 beta. Do not use them to publish 1.1.1 or replace old assets. Prepare the 1.1.1 publication action only for the accepted exact source and verified artifact; public publication requires a separate instruction.

Local verification covers Python release-gate, narrow-scope and validation-producer tests, plus the available packaging unit tests. This environment has no CMake or native Windows runner; CMake/native product and Windows resource checks are delegated to the existing CI, not reported as locally passed.
