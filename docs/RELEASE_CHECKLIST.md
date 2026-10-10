# Open Beta 1.3 release preparation

The owner requested preparation of the current CAB build on 2026-10-09 and explicitly chose **version 1.3** at 22:33 KST. Product version: **1.3.0**. Existing open-beta channel: **beta.1**. Proposed package/tag: **1.3.0-beta.1 / v1.3.0-beta.1**.

This is preparation. The October 5 authorization for the published 1.2 release is historical and is not reused as authorization or measured acceptance for 1.3. See [historical checklist](RELEASE_1_2_CHECKLIST.md).

## Prepared source and assets

- Integrate the current CAB/Niflheimr source with the main-only documentation changes; preserve project/automation and personal files.
- Derive application, native resources, Setup, MSIX, macOS, Linux and manifest identities from VERSION and RELEASE_CHANNEL.
- Fix Windows publisher UTF-8 reads and native command-failure propagation.
- Correct whole-room framing, restore cabinet exterior design and remove duplicate visible 4×12/4×10 entries without changing saved parameter values or physical proportions.
- Give all 48 factory snapshots an explicit modeled CAB recipe, preserving intentional dry LOW paths and removing private IR auto-resolution.
- Limit bundled IRs to the attributed, hash-allowlisted assets; exclude unapproved reference catalog rows and research/reference audio from release payloads while preserving user import/recall.
- Prepare release notes, the EN/DE/KR manual and newly built exact-source candidate packages. Existing tags/assets stay intact.

## Final-source verification

- [ ] Three-platform product build, CTest, package verification and candidate assembly succeed on the final 1.3 source.
- [ ] Modeled Bass 6×10 uses six actual drivers, the shorter enclosure, and column-preserving preview target migration; capture IR metadata stays truthful.
- [ ] CAB native/state/layout/visual/timing contracts pass on that source, including the current UI corrections.
- [ ] All 48 new CAB factory recipes pass full-processor dirty/clean recall, unity-OUTPUT level, +6 dB input and state/A-B round-trip checks without reducing their existing limits.
- [ ] Saved mic gain/polarity/delay/blend and native amp selection apply from the first sample on fresh and re-prepared processors; live smoothing remains intact.
- [ ] The IR distribution source/staging policy passes; neither excluded reference folders nor unapproved IR/capture bytes enter the installer or portable packages.
- [ ] Separate Windows candidate/preparation workflow passes: install, app launch, VST3, custom paths, repair, uninstall, personal-file preservation and Defender.
- [ ] Five binary packages, checksums, update manifest, manual, source/run/attempt provenance and installer receipts agree.
- [ ] The beta_1_3 verdict is recomputed from exact-source evidence; missing, stale, malformed or duplicated reports stay blocked.
- [ ] Remaining acceptance requirements are satisfied with their actual evidence. Green compilation does not replace undefined or unexecuted policy checks; see [pipeline status](RELEASE_1_3_PIPELINE.md).
- [ ] Publication is authorized for the reviewed source and the preparation-only scope is updated accordingly.

## Publication operation

Use the version-pinned publish-beta13.yml manual workflow on release/open-beta-1.3.0 after the final-source conditions are satisfied. Preparation branch pushes cannot publish. The publisher rejects the old 1.2 preview, superseded source, stale successful runs, missing CAB/platform jobs or mismatched assets. The historical 1.2 publisher remains pinned to 1.2.

After authorized publication, verify public assets, update discovery and site/manual links. Preserve the two existing deferred transpose checks and signing/notarization limitations.

See [preparation status](RELEASE_1_3_PREPARATION.md), [release notes](OPEN_BETA_RELEASE_NOTES.md) and [head controls after 1.3](AMP_HEAD_CONTROLS_NEXT_UPDATE.md).
