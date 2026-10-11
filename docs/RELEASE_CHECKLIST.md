# Open Beta 1.3.1 release checklist

Product version: **1.3.1**. Package/tag: **1.3.1-beta.1 / v1.3.1-beta.1**.

On 2026-10-11 the owner requested the full amplifier-knob audit and then instructed: **“수정되면 배포랑 업데이트까지 해”**. This authorizes the corrected beta's integration, publication and update distribution after the required verification. It does not report personal testing or complete Studio One acceptance. See the [1.3.1 pipeline and evidence boundary](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/ce3d3b43a4958b1aba55226fb081276578967704/docs/RELEASE_1_3_1_PIPELINE.md). The published 1.3.0 release remains intact.

## Source and user-facing scope

- Integrate direct-head controls, revised knob shapes, labels, head framing, Signal Path and Tone/Final EQ with the CAB realtime, grille, IR and state fixes shipped in 1.3.0.
- Cover 26 catalog head layouts while keeping 25 selectable amps; the retired Ironball remains retired. Verify Niflheimr's 14 knobs/five channels and Brit Edge's six knobs/five input paths, all other model controls, and Classic/Dual/Matrix navigation.
- Preserve parameter identities, host automation, channel recall, A/B, existing projects and personal files. Two new EQ stages default to bypass.
- Derive product, platform package and manifest versions from VERSION and RELEASE_CHANNEL. Use new packages built from the final source.
- Retain the licensed IR allowlist and exclusion of research/private capture audio from public payloads.
- Keep the release notes and EN/DE/KO manual current; preserve the [1.3.0 notes](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/ce3d3b43a4958b1aba55226fb081276578967704/docs/OPEN_BETA_1_3_RELEASE_NOTES.md) as history. Label actual native screenshots, software previews and illustrations accurately.

## Final-source evidence

These boxes remain open until the corresponding evidence is available. A source change that affects a gate requires evidence for the changed source.

- [ ] Windows, macOS and Linux full product builds, CTest, package verification and candidate assembly succeed on the final product commit.
- [ ] Native head controls pass on all three platforms: complete model/channel layouts, gestures, value-entry visibility, navigation and screenshots. Review idle and adjusting states at supported scales, including dense Niflheimr controls.
- [ ] Native Signal Path and graphical EQ tests pass on all three platforms, including audio, state recall, routing and navigation.
- [ ] Native CAB/state/layout/visual contracts and separate realtime checks pass on all three platforms with the retained 1.3.0 fixes.
- [ ] All 48 factory CAB recipes, first-sample state restoration and IR distribution checks pass without relaxing existing thresholds.
- [ ] The Windows update candidate passes install, launch, VST3/custom paths, repair, uninstall, personal-file preservation and Defender checks.
- [ ] Five binary packages, checksums, three-platform update manifest, embedded manual/release notes and installer receipts match their exact source, workflow run and attempt.
- [ ] Strict raw evidence is consolidated accurately. The 107 unexecuted baseline checks remain disclosed as blocked; full product validation and Studio One/listening acceptance are not asserted.
- [ ] The separate 1.3.1 beta-publication decision passes its fixed policy. Missing/stale evidence, an executed failure, changed thresholds, reduced platform coverage or new waivers block publication.
- [ ] Final product source SHA is bound in the version-specific publication policy after review; required latest workflow runs for that SHA have succeeded.

## Publication and update

Use the manual `publish-beta131.yml` workflow on `release/open-beta-1.3.1`. It defaults to read-only review. Publication requires `publish=true`, the final product source SHA and `confirm=publish-v1.3.1-beta.1`. The publisher checks the current source, controller ancestry, required workflow results and all asset identities before publishing; preparation pushes cannot publish. See the [complete pipeline](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/ce3d3b43a4958b1aba55226fb081276578967704/docs/RELEASE_1_3_1_PIPELINE.md).

- [ ] Publish the verified prerelease and its strict validation report, raw evidence and separate beta decision without overwriting historical tags/assets.
- [ ] Verify unauthenticated release visibility, public downloads, hashes and `update-beta.json` against the staged package set.
- [ ] Verify Standalone beta discovery and the VST3/AU release-page link. Download and installation remain explicit user actions.
- [ ] Update the homepage, version text, manual and actual screenshot links after public release verification.

Keep the existing two deferred transpose checks, signing/notarization limitations and all disclosed manual-validation gaps. The user's publication authorization does not turn them into passed tests.
