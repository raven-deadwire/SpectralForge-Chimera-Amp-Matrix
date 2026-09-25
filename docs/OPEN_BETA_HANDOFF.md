# Open Beta 1.0 candidate checkpoint — 2026-09-25

## Exact state

- Product: SpectralForge Chimera, `1.0.0-beta.1`.
- Development branch: `v1.0/rebuild`; existing draft PR: #2.
- Reviewed implementation: `01d30dfa0d85ed01a2f5c2726b977afb546cf06e`.
- Last observed remote implementation: `d685f4c0255e8b309196a50f0688da8449306b5a` (successful build #130).
- Candidate implementation changes: 107 files; seven added amp voices, updated branding, presets, IR catalog, update support and platform packaging.
- This checkpoint adds documentation only. No candidate binaries have been produced by the new CI workflow yet.

## Local verification completed

The existing DSP suite was rebuilt and passed on Linux with the cached JUCE 8.0.8 sources. These results do not certify a Windows binary.

- Routing and live EQ at 44.1, 48, 96 and 192 kHz.
- All 15 amp voices: oversampling, stereo isolation, EQ extremes, invalid indices and block partition checks.
- Both boost/overdrive orders: distinct sound, preserved clean DI/latency and identical output across tested block partitions.
- All 46 selectable FX models; crossover/phase/alignment and automation checks.
- All 31 factory presets: finite, non-silent, unclipped test output at 44.1 and 48 kHz.
- Rebuilt release-support tests passed: semantic versions, update manifests, platform selection, official URL restrictions, draft exclusion, SHA-256 corruption rejection and diagnostic privacy.
- The application importer and decoder accepted all 26 private IRs. Collection scan found 28 available captures including the two embedded factory captures, with 15 private bass IRs and seven Karnivore IRs. Labels were unique and bounded.
- Twelve separate external-pack reference entries remained explicitly unavailable in this private-pack-only check; they were not counted as installed audio.
- Archive WAV hashes, rates, frame counts and nonzero sample data matched all 26 catalog entries.
- Python packaging scripts compiled; the Linux install script passed shell syntax validation; `git diff --check` passed.
- Changed-file scope contained no new raw NAM/IR audio or archive payloads. Common GitHub-token/private-key patterns were absent from those files. This is a scoped inspection, not a security certification.
- JUCE VST3 replacement mode remains disabled; the unchanged manufacturer/plugin codes continue to determine VST3 class IDs. Old normalized model-selection automation still requires the review described in the release notes.

## Blocked next step

Automatic approval review rejected pushing the candidate source and artwork to the existing GitHub branch, citing insufficient explicit authorization to export that payload to the destination repository. No alternate upload route was used. Prior records confirmed continued development and Windows-build instructions but did not establish an explicit payload-upload approval.

The pending action is a normal fast-forward push to `raven-deadwire/SpectralForge-Chimera-Amp-Matrix`, branch `v1.0/rebuild`, followed by the existing pull-request build workflow. Private capture files remain outside this source change.

## Resume after authorization

1. Check the remote branch for concurrent changes, then push the reviewed development commits without force.
2. Follow the new run for this exact revision; keep build #130 separate as historical evidence.
3. Resolve any CI failures and verify the new Windows UI snapshots, install/repair/uninstall, MSIX review and Defender reports.
4. Confirm macOS/Linux packaging and the aggregate candidate manifest; download the matching artifacts and recompute hashes.
5. Assemble the user's Windows package with the already verified private IR pack only after the candidate installer passes.
6. Complete instrument/DAW listening gates and report remaining signing/notarization limits before public release. No release/tag publication is included in this checkpoint.
