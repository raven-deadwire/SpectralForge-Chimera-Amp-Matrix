# Open Beta 1.0 release handoff

Target: **SpectralForge Chimera**, `1.0.0-beta.1`, tag `v1.0.0-beta.1`.
The preparation workflows create downloadable build artifacts only. They do not create tags, draft releases or public releases.

## Candidate preparation

1. Review the source diff and brand assets. Confirm plugin unique IDs, state types and installer AppId are unchanged.
2. Push the reviewed revision to the open development PR, or run **Open Beta 1.0 · Prepare release candidate** on the exact intended revision. Both routes build all three platforms and assemble one candidate without publishing.
3. Require green Windows, macOS and Linux jobs and the aggregate manifest job. Do not reuse binaries from a different commit or an earlier run.
4. Inspect Windows UI galleries at minimum/default size. Confirm all text and control targets, amp references, short IR titles, and rack aspect ratio.
5. Check DSP, installer, MSIX-review and Defender reports. MSIX is not a downloadable public installer. Do not treat a missing report as a pass.
6. Read third-party notices and factory IR provenance. Confirm no personal capture data appears in installer payloads.
7. Download the aggregate candidate artifact. Recompute `SHA256SUMS.txt` and compare each manifest asset's filename, size, hash, repository, tag and architecture.
8. Perform actual instrument listening and DAW smoke tests listed in the release notes. Record hardware, OS, host version and pass/fail; resolve remaining release blockers.

## Concrete publication package

- `SpectralForge-Chimera-1.0.0-beta.1-win64-Setup.exe`
- `SpectralForge-Chimera-1.0.0-beta.1-macos-universal.pkg`
- `SpectralForge-Chimera-1.0.0-beta.1-linux-x86_64.deb`
- Optional Windows portable ZIP and Linux TAR.GZ, with their runtime/install documentation
- `SHA256SUMS.txt`, per-file SHA256 sidecars, `update-beta.json`
- `OPEN_BETA_RELEASE_NOTES.md`, `INSTALLATION.md`, `MANUAL.html`, `THIRD_PARTY_NOTICES.md`
- Matching verification reports, with remaining limitations stated

Keep an unsigned status label unless publisher signing is actually completed and verified. Mac ad-hoc signatures are not notarization. If binaries are signed later, regenerate every affected hash and `update-beta.json` from the final bytes and rerun relevant verification.

## Publish only after release authorization

Create a prerelease for the exact commit and tag, upload the reviewed assets, and include release notes and unsigned/notarization status. Do not mark beta as the stable/latest release inadvertently. Once assets are public, check the updater from the preceding beta/build: discovery, download, cancellation, checksum rejection, and install handoff after session save. The update manifest URLs intentionally point at the future official tag; no server claim is made until publication succeeds.

For a rollback, unpublish/replace the incorrect release only through a deliberate maintainer action. Do not silently reuse the same version with different bytes; issue a new beta version and regenerate manifests.

## Builder maintenance

The macOS universal job pins `macos-15`; the macOS 14 hosted image enters scheduled brownouts in October 2026 and retires on 2026-11-02 ([official runner notice](https://github.com/actions/runner-images/issues/13518)). The Linux binary builder intentionally remains Ubuntu 22.04 for its glibc baseline; migrate it to a newer hosted runner with a pinned Ubuntu 22.04 build container before that hosted image retires on 2027-04-17 ([official runner notice](https://github.com/actions/runner-images/issues/14254)). The artifact-only aggregation job uses Ubuntu 24.04 and does not influence binary compatibility.
