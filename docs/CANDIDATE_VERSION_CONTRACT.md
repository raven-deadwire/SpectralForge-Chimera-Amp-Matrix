# 1.3 release identity

`VERSION` remains the only numeric product version source and is `1.3.0` for this release preparation. `RELEASE_CHANNEL` selects `beta.1`, so the package and in-app identity are `1.3.0-beta.1`; `preview` retains the exact-SHA suffix for development builds. CMake, native resources, manifests and Setup read this contract. Configured build metadata also checks the package version to reject a stale channel build. Changing this identity requires rebuilding and verifying the resulting 1.3 packages.

## Candidate version authority

`VERSION` is the numeric product version source (currently `1.3.0`). A preview
package identity is derived as `<VERSION>-preview.<first 10 source SHA chars>`.
The full 40-character SHA remains the provenance check; ten characters are only
the display/build ID. The existing AppId, VST3 identity and user-data paths stay
stable across previews.

## Failure traced

PR #12 run 37137539317 and PR #13 run 37138006486 both built the product and
Setup, then failed the installed registry/payload comparison. Previously:

| Surface | Authority | Value |
| --- | --- | --- |
| JUCE app/VST3 resources | CMake `project(VERSION ...)` | 1.1.0 |
| Inno AppVersion / Windows DisplayVersion | literal in `Chimera.iss` | 1.1.0 |
| Candidate payload package version | mutation of `package_release.VERSION` | 1.0.1-preview.SHA |
| Candidate payload installer version | literal in candidate packager | 1.0.1 |
| Settings/product release label | `Source/ReleaseInfo.h` | 1.1.0-beta.1 |

## Current flow

| Surface | Derived from |
| --- | --- |
| CMake PROJECT_VERSION / JUCE numeric resources | `cmake/ChimeraVersion.cmake` reads `VERSION` |
| Settings release label | CMake-generated `ChimeraBuildVersion.h`, VERSION + RELEASE_CHANNEL + configured SHA for preview builds |
| Portable and Setup payload manifests | `Tools/chimera_version.py`, VERSION + RELEASE_CHANNEL, with full checked-out SHA for provenance |
| Inno AppVersion / DisplayVersion / Setup resources | validated `/DProductVersion`; no Inno fallback literal |
| Inno preview label and package filename | validated `/DPackageVersion` and optional BuildId |
| MSIX / macOS / Debian current build packages | same numeric source and derived package identity |

Numeric Windows versions are `1.3.0` (`1.3.0.0` in PE/MSIX resources); preview
identity is `1.3.0-preview.SHA`, and the prepared beta is `1.3.0-beta.1`. Registry DisplayVersion deliberately remains
the numeric product version, while AppVerName, manifests and Settings identify
the selected channel and package identity. No release/tag is created by either candidate workflow.

Both CMake and Python reject malformed or Windows-overflow version components.
Staging rejects a configured product version or SHA that differs from current
source. Setup compilation rejects missing/mismatched manifest identity and PE
resource versions before Inno runs. The installer verifier retains strict
registry comparison and additionally checks Setup and selected installed PE
resources on every install/repair. Its JSON receipt records version identity,
observed DisplayVersions, binary hash, source SHA and run ID. Transfer requires
that same version identity in the successful receipt.

JUCE 8.0.8 `extras/Build/juce_build_tools/utils/juce_ResourceRc.cpp` emits a fixed
FILEVERSION and a ProductVersion string, but omits fixed PRODUCTVERSION. The
Windows checker therefore requires matching numeric file fields and the actual
product-version string, and validates fixed product fields when populated (as
Inno does). Zeroed absent fields are not treated as the JUCE product version.
PowerShell fixtures cover both layouts and reject stale/missing/mixed fields.
Inno pads ProductVersion strings in its prebuilt loader; only trailing resource
padding (space/NUL) is removed before comparison. Registry DisplayVersion is
still compared directly. The Windows pre-build test also compiles a small real
Inno Setup (never executes it) and checks its emitted PE resources.

The common build workflow resolves filenames from the same channel/package identity
and checks out the PR head explicitly. Its current packages no longer claim
the historical 1.1.0-beta.1 identity. Historical publishing scripts retain their
explicit release-specific guards. The 1.3 publisher requires the exact
`1.3.0-beta.1` identity, current source and consolidated release evidence;
a preview cannot be repackaged as a release without rebuilding and verification.

## Regression coverage

Run `python Tests/test_packaging_version.py` (Python 3.12 and CMake >= 3.22).
CTest also registers `ChimeraPackagingVersionTests`; Windows candidate executes
it before the native build. Tests cover a non-current 2.17.4 fixture, actual
CMake project/header generation, candidate staging and file hashes, missing or
mixed manifest fields, wrong SHA/BuildId, stale configured builds, numeric
validation and immutable shared packager state. Windows additionally executes
the actual PowerShell builder with bad manifests and a relabelled Python PE;
both must fail before creating the Setup output directory.

Local fixture tests are not install evidence. Acceptance requires the
`Chimera update candidate` run on each updated PR's exact head, including real
install/repair/uninstall, custom Unicode paths, component-only installs,
remembered/moved VST3 folders, and preserved personal presets/IR fixtures.
