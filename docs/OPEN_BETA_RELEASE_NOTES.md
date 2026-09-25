# SpectralForge Chimera — Open Beta 1.0

Release candidate: `1.0.0-beta.1` · target tag: `v1.0.0-beta.1`.

The product is now named **SpectralForge Chimera**, with CHIMERA as the prominent interface wordmark. Existing plugin IDs and state identifiers remain stable. Model-selector automation from earlier development builds must still be reviewed when the choice count has changed.

## Changes prepared for this beta

- Applied the user-supplied emblem to application/installer icons; generated a new SpectralForge/CHIMERA wordmark for the renamed product.
- Amp model names paired with original equipment references, matching the effect-model presentation.
- Short IR display labels with full source details retained; official bass IR pack import support and an expanded private 26-capture collection (15 bass captures). Public embedded IRs remain the two licensed guitar captures; additional external bass files require import.
- Fifteen independently voiced amp heads (seven new: Chime 30, Orange Crown, Bassman Valve, Subway Clean, Match Chime, Silk ODS, Taste Punch); 31 recommended starting presets by instrument and role.
- Rack artwork adapted to the actual on-screen panel proportions, with live controls kept separate.
- PRE gain-order selection between Fuzz → Boost → Overdrive and Fuzz → Overdrive → Boost, plus Envelope/Compressor order and clear gain/headroom guidance.
- SETTINGS → Manual / Bug report / Updates: bundled Korean/English offline manual, user-initiated bug reports, and beta update discovery/download integrity checking.
- Windows Setup, macOS universal PKG (app/VST3/AU) and Linux x86_64 DEB/TAR packaging with manifests and SHA256.

## Distribution boundaries

Only licensed public factory IR data is embedded. Personal TONE3000/NAM/IR captures are not part of public installer payloads. Existing personal IR packs remain usable locally.
Match Chime and Silk ODS target Matchless DC-30 and Dumble ODS, with any clone capture references labeled as clones. Six added voices (8–13) were rendered against actual NAM files; see [OPEN_BETA_NAM_VALIDATION.md](OPEN_BETA_NAM_VALIDATION.md) for measurements, calibration limits and clone disclosures. Taste Punch targets EICH T900; no exact T900 NAM reference was found. Algorithmic amp and effect models are reference-inspired, with no claim of circuit-identical hardware reproduction or manufacturer endorsement.

Windows installers are unsigned unless an explicitly verified public signing identity is supplied to the signing pipeline. The current preparation does not provide one. macOS binaries have ad-hoc code seals; Developer ID signing/notarization is pending. MSIX remains an internal review artifact.
The updater can discover this version only after a matching official GitHub prerelease and its validated `update-beta.json` are published. Preparation alone does not publish a release.

## Validation and remaining beta checks

CI must build the exact candidate revision on Windows x64, macOS universal and Linux x86_64, pass DSP tests, and complete platform packaging checks. Windows additionally renders the actual editor, tests install/repair/uninstall and scans distribution bytes with Defender. See each run's logs; this file is a scope statement, not a passing test certificate.

Before public release, complete listening and DAW smoke tests: Studio One VST3 on Windows, one native Apple Silicon and one Intel/Rosetta host on macOS, and a Linux VST3 host. Verify each mode, all new heads/presets, IR recall after moving source files, audio-device changes, parameter automation, and update cancellation/failure.

Known limits: transpose can add latency/artifacts; CPU varies with oversampling and instance count; unsigned installers may be blocked or warned about; host-specific automation migration needs review for old development sessions. Beta reports should include exact reproduction steps and technical settings.
