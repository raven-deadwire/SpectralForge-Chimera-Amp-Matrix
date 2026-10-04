# Open Beta 1.1.2 day-one release

The owner explicitly instructed on 2026-10-05 at 00:17 KST: release this work immediately as the 1.1.2 day-one patch once verification completes. Publication is authorized; no additional approval is required. The known NAM coverage limits remain disclosed, not marked PASS.

## Required final-source gates

- `VERSION=1.1.2`, `RELEASE_CHANNEL=beta.1`, package/tag `1.1.2-beta.1` / `v1.1.2-beta.1`.
- Final-source Windows, macOS universal and Linux build/CTest/package checks all succeed.
- The exact-source `Chimera update candidate` workflow succeeds, including preparation contracts, Windows installed app/VST3, custom paths, repair and uninstall.
- Five binary packages, matching SHA-256 sidecars, three-platform update manifest, manual and candidate provenance are verified before public publication.
- No NAM weights or personal IR/audio are in the public packages. Existing tags and assets are never overwritten.
- Read `NATIVE_NAM_CALIBRATION.md`: 22 families / 174 distinct NAM files compared, 22 families corrected; EICH T900 remains without an exact NAM. Supplied modified SVT-CL and 1998 SUNN references do not establish stock/1970s circuit equivalence. Synthetic evidence is not human listening acceptance.
- Verify the public tag, all assets, updater discovery and homepage/manual links after publishing.

`publish-beta112.yml` builds the exact release-branch SHA and publishes only after the required checks. It is pinned to `release/open-beta-1.1.2`. The previous 1.1.1 checklist and owner attestation remain historical in `RELEASE_1_1_1_CHECKLIST.md` and `RELEASE_1_1_1_PREPARATION.md`.

Transpose delay/smearing, Windows publisher signing and macOS notarization remain disclosed limitations. Existing native sessions can change tone after updating even though stored parameter values remain unchanged.
