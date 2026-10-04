# Open Beta 1.1.1 release checklist

Target: **SpectralForge Chimera 1.1.1-beta.1**.  
Policy: `Validation/release-policy.json` v4, profile `beta_1_1_1`.

This checklist is fail-closed. A missing artifact, unavailable DAW, missing DI, or stale commit is **BLOCKED**, not PASS. The only deferred 1.1.1 items are `I2.PITCH_LIVE` and `I2.PITCH_DI`; they remain tracked by `transpose_followup`.

## 1. Freeze one exact source

- [ ] Choose one final source SHA after all 1.1.1 fixes, documentation and publisher changes are integrated.
- [ ] Confirm `VERSION=1.1.1`, `RELEASE_CHANNEL=beta.1`, package version `1.1.1-beta.1`.
- [ ] Do not reuse a green workflow from an older SHA.
- [ ] Keep released parameter IDs/ranges/defaults, plugin identity and installer AppId stable.

## 2. Automated integration gates

- [ ] Windows full configured CTest inventory PASS.
- [ ] macOS universal full configured CTest inventory PASS.
- [ ] Linux full configured CTest inventory PASS.
- [ ] Universal PRE factory recall matches the visible native board contract.
- [ ] M104 / Distortion+ and JB-2 dedicated binary, inactive-bank, A/B and project-recall regressions PASS.
- [ ] Gate Range, full Guitar state/gain/GR, pitch synthetic regression and Windows live-instance removal PASS.
- [ ] Paired callback diagnostic protocol tests PASS. Diagnostic PASS does not by itself satisfy real callback acceptance.

## 3. Product-quality audio acceptance

Use fixed, hashed guitar and bass DI. Record raw measurements separately from listening notes.

- [ ] Level-match baseline/candidate before tonal judgement.
- [ ] Check clean, crunch, lead/high-gain and bass models for pick attack, palm-mute recovery, low-end tightness, chord separation, gain compression and volume-rolloff response.
- [ ] Check the four Guitar signatures and Deadwire Bass signatures in their intended musical roles.
- [ ] Check Gate pre/post with isolated guitar and bass DI: hiss attenuation, fast palm mute, sustain, volume rolloff, chatter/pumping and attack loss.
- [ ] Complete E670FE reference acceptance: T.D. EQ ownership/response, calibration boundary and same-DI comparison.
- [ ] Compare representative tones against a current commercial reference such as Neural DSP under the same DI/IR/loudness conditions. The goal is release-quality feel and mix usability, not superficial EQ matching.

Transpose low-latency/real-DI quality is not a 1.1.1 release blocker, but the known ~43–46 ms STFT latency and low-B / -2 semitone smearing limitation must remain disclosed.

## 4. Commercial host acceptance

At minimum use the exact final Windows VST3 in Studio One 8. Cubase and Sonar evidence should be retained when available because Issue #3 was reported there.

- [ ] Scan/load plugin.
- [ ] Open and close the editor repeatedly.
- [ ] Remove a UI-opened instance while another instance remains alive.
- [ ] Save, close and reopen the project.
- [ ] Write/read automation for existing parameters and Gate Range.
- [ ] A/B and user/factory preset recall.
- [ ] Close project after editor use.
- [ ] Exit the DAW after editor use.
- [ ] 30-minute soak with editing, preset changes, playback and repeated UI open/close.
- [ ] No freeze, deadlock, crash, stuck process or silent state corruption.

## 5. Callback / performance acceptance

- [ ] Run the paired A/B -> B/A callback probe with the closed-editor control.
- [ ] Retain raw per-pair reports, source/binary SHA-256, p99 and deadline-miss deltas.
- [ ] Separate runner scheduling noise from candidate-specific movement.
- [ ] Confirm acceptable behavior on a controlled audio workstation; synthetic/shared-runner diagnosis alone does not satisfy `I2.UI_AUDIO_TIMING`.

## 6. Exact-source packages

After all code/test fixes are on the final SHA:

- [ ] Windows Setup build PASS.
- [ ] Windows install / repair / uninstall PASS.
- [ ] Windows Defender scan PASS.
- [ ] Unsigned MSIX review verification PASS where applicable.
- [ ] macOS universal PKG verification PASS.
- [ ] Linux DEB and TAR.GZ verification PASS.
- [ ] Aggregate `SpectralForge-Chimera-Open-Beta-1.1.1-Release-Candidate` exists for the same SHA.
- [ ] `SHA256SUMS.txt`, sidecars, `update-beta.json`, payload manifests and `candidate-source.json` all agree on bytes, version, tag, run ID and revision.
- [ ] No private NAM / personal IR payload leaked into public packages.

Expected public binaries:

- `SpectralForge-Chimera-1.1.1-beta.1-win64-Setup.exe`
- `SpectralForge-Chimera-1.1.1-beta.1-win64.zip`
- `SpectralForge-Chimera-1.1.1-beta.1-macos-universal.pkg`
- `SpectralForge-Chimera-1.1.1-beta.1-linux-x86_64.deb`
- `SpectralForge-Chimera-1.1.1-beta.1-linux-x86_64.tar.gz`

## 7. Publication lock

1. Fast-forward/create `release/open-beta-1.1.1` only to the accepted final SHA.
2. Run **Build and publish Open Beta 1.1.1** manually on that branch.
3. Supply the exact 40-character accepted SHA.
4. Enter `publish-v1.1.1-beta.1` only after the retained manual and automated gates above are accepted.
5. The workflow rebuilds all three platforms on that exact SHA before publication.
6. The publisher refuses mismatched candidate revision/run ID/version/hash, an existing divergent tag/release, missing assets, or private capture content.
7. Never move or replace an existing public tag/assets with different bytes. Issue a new beta version instead.

## 8. Post-public verification

- [ ] Release is a prerelease, not latest/stable.
- [ ] Public tag resolves to the accepted source SHA.
- [ ] Five binary package hashes match the reviewed candidate.
- [ ] Public download URLs in `update-beta.json` resolve to the published assets.
- [ ] Previous beta updater discovers the new beta, rejects checksum mismatch, supports cancellation, and hands off installation only after session save.
- [ ] Release notes clearly retain unsigned/notarization status and the known Transpose and commercial-DAW validation boundaries.

## Builder maintenance

The binary matrix currently uses macOS 15, Ubuntu 22.04 and Windows latest. Keep the Linux compatibility baseline intentional when GitHub-hosted images change. macOS ad-hoc code seals are not Developer ID signing/notarization, and unsigned Windows packages must not be presented as publisher-signed.
