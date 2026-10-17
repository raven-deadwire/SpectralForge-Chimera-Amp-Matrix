# Release continuation authorization — 2026-10-17 (Asia/Seoul)

## Specific owner authorization
In the release conversation, the owner answered “승인해” to the explicit request to prepare a fresh source-bound approval record and publication verification, retire RELEASE_REVIEW_PENDING, and continue merges, release and homepage deployment. This supersedes the unanswered gate-retirement request; it does not certify any unperformed test.

## Verified precursor
Product source: 356c0c23a6e27dbc75b21e42ef4f8c6bdb6b355e.
Open Beta Build and Test run 38953439314: all six jobs succeeded, including macOS universal package, Windows/Linux, assembly and consolidation.
Focused run 38953439353: all six native-navigation and precision-worker jobs succeeded.
Graphical EQ run 38953439424 remains FAILED: its macOS tests passed, but artifact upload failed with CreateArtifact ENOTFOUND. It is not represented as a passed workflow. Earlier timing failures remain historical failures.

## Scope and remaining publication conditions
Retire the preparation marker under this specific authorization. No DSP, UI behavior, validation threshold, expiry, required check inventory or waiver changes are authorized by this document.
The marker-free resulting source must obtain fresh exact-source required CI and package evidence. The official publisher must verify that evidence and a newly generated weekly publication receipt bound to the final tested product SHA. Historical 1.3.1 receipts must not be reused.
This document is not that final publication receipt and is not a declaration of STRICT_PASS. Manual DAW/audio-device, real DI/listening, signing/notarization and other missing acceptance evidence remain unverified and must be disclosed under the existing beta publication policy; no missing check may be promoted to PASS.
Publish 1.3.2-beta.1 only through the official publisher after its checks succeed. Then update and verify the official homepage and real downloads, preserving the approved product-description design and using actual final-build screenshots.
