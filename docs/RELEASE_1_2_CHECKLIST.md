# Open Beta 1.2 release

The owner authorized release after the final channel-level and Fimbulvetr clarity corrections on 2026-10-05 at 19:10 KST. No additional publication approval is required. This is publication authorization, not a new measured instrument-DI or commercial-DAW attestation.

## Final-source gates

- VERSION 1.2.0, RELEASE_CHANNEL beta.1; package/tag 1.2.0-beta.1 / v1.2.0-beta.1.
- Windows, macOS universal and Linux build/CTest/package jobs plus release-candidate assembly all succeed on the exact source SHA.
- The exact-source Windows candidate/preparation workflow succeeds, including install, launch, VST3 loading, custom paths, repair, uninstall and Defender checks.
- Five binary packages, SHA-256 sidecars, three-platform update manifest, manual and source/run provenance agree before publication.
- No personal IRs, NAM weights or capture audio in public packages. Do not overwrite old tags/assets.
- Publish only from release/open-beta-1.2.0. publish_beta12.py reuses immutable artifacts of the latest successful exact-head build; stale, pending or failed verification cannot fall back to an older run.
- Verify the public tag/assets, update discovery and homepage/manual links after publication.

Release-policy hard gates remain in place. The separate full_release verdict and outstanding DI/host/reference checks are not relabeled PASS. The existing two deferred transpose goals remain tracked separately. Open Beta retains the documented transpose, signing, notarization and reference limitations.

Historical 1.1.2 release notes are preserved in OPEN_BETA_1_1_2_RELEASE_NOTES.md; its publisher remains pinned to 1.1.2.
