# SpectralForge Chimera — Open Beta 1.0 updates and support

The installed binary version is `1.0.0-beta.1`; the release tag is `v1.0.0-beta.1`.
The existing plugin IDs and `SpectralForge/Chimera` data directory remain compatible.

## User behavior

- **Manual** opens the bundled `MANUAL.html` offline. The embedded copy belonging to
  the current binary is extracted to the application data directory before opening
  it in the default browser. Installed documentation is a fallback.
- **Report bug** opens a GitHub issue draft. The user reviews and submits it in their
  browser. Nothing is submitted by the plugin. The draft includes version, build
  revision, OS, architecture, plugin format, sample rate, block size, channel counts
  and reported latency. It excludes audio, IRs, device names, account details,
  project names, host paths and machine identifiers.
- **Check updates** checks the official repository on a background worker. The
  audio thread and UI thread do not perform network transfers.
- **Automatic checks** are initially off. The persisted setting enables a check
  when a support service is opened, at most once every 24 hours per saved settings
  file. Failed requests count towards this startup interval; a manual check remains
  available immediately. An explicit opt-in also starts a check immediately.
  GitHub receives the normal connection IP and product/version user-agent; there is
  no additional telemetry. Separate already-open plugin instances can each perform
  one check if opened simultaneously before the saved timestamp is written.
- A newer compatible release offers **Download update**. Downloads are explicit,
  are limited to 1 GiB, and are checked against the manifest's exact size and
  SHA-256. Incomplete or mismatching temporary files are deleted.
- **Cancel** stops an in-progress check or download; partial downloads are discarded.
- **Show installer** rechecks the downloaded file and reveals it in Explorer/Finder
  or the desktop file manager. Close all DAWs and standalone Chimera instances
  before running the installer. The plugin never launches an installer, requests
  elevation, replaces its own loaded binary or silently executes downloaded code.
  On Linux the `.deb` is preferred; the archive is available for other distributions.

This is an update notification and verified-download workflow with an explicit OS
installation step. It is not silent plugin self-replacement. SHA-256 validates the
asset against the official HTTPS manifest; it does not replace Windows code
signing or Apple Developer ID signing/notarization. Release signing remains a
separate packaging requirement.

## Release channel and manifest

Only the fixed repository is queried:

`https://api.github.com/repos/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases?per_page=30`

The highest semantic version among non-draft `beta`, `rc` and final releases with
an `update-beta.json` asset is selected. Tags must start with `v`; beta.10 sorts
later than beta.2. Final 1.0.0 sorts later than 1.0.0-beta.1. Alpha releases are not
offered on this channel. Assets and release pages must belong to the same exact
repository and tag. A draft release cannot be discovered until it is published.
An empty published release list is displayed as no published update; a missing or
private repository is an explicit channel-unavailable error, not “up to date.”

Example schema (hash and size must be replaced by the release assembly tool):

```json
{
  "schema": 1,
  "version": "1.0.0-beta.1",
  "channel": "beta",
  "releaseUrl": "https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.0.0-beta.1",
  "assets": [
    {
      "platform": "windows",
      "arch": "x86_64",
      "name": "SpectralForge-Chimera-1.0.0-beta.1-win64-Setup.exe",
      "url": "https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.0.0-beta.1/SpectralForge-Chimera-1.0.0-beta.1-win64-Setup.exe",
      "sha256": "REPLACE_WITH_64_HEX_CHARACTERS",
      "size": 123456
    }
  ]
}
```

Supported targets: `windows/x86_64`, `windows/arm64`, `macos/x86_64`,
`macos/arm64`, `macos/universal`, `linux/x86_64`, `linux/arm64`. A target must have an
actually built asset; unsupported architectures receive a clear message. Windows
assets must be `.exe`; macOS `.pkg`; Linux `.deb` or `.tar.gz`. A universal macOS
installer serves both supported CPU architectures. File names permit only ASCII
letters, digits, hyphens, underscores and dots; directory traversal is rejected.

Redirects are disabled in the HTTP client and processed explicitly. Up to five
HTTPS redirects are permitted only to GitHub's `release-assets.githubusercontent.com`,
`objects.githubusercontent.com` and `github-releases.githubusercontent.com` hosts.
URLs with user-info, non-default explicit ports, backslashes or newline characters
are rejected. API responses are capped at 2 MiB; manifests at 256 KiB. Hashes,
sizes, versions, channels and compatible OS/CPU are validated before downloading.

## UI integration

`Source/ReleaseSupport.h` provides a worker that can be owned by the editor or a
longer-lived processor support controller. Never construct/call it in
`processBlock`. Poll `snapshot()` from a UI timer; it is a small locked copy and
contains state, message, available version, release URL, progress and installer.

```cpp
spectralforge::release::ReleaseSupport support;
support.checkNow();
support.setAutomaticChecksEnabled(true); // only after the user's choice
support.downloadUpdate();                // explicit button
support.revealVerifiedInstaller();       // explicit button; does not run package
support.cancel();
```

`openManual(embeddedData, embeddedSize)` accepts the embedded HTML resource.
`bugReportUrl(Diagnostics{...})` returns a safely encoded GitHub URL. Display it in
the browser only after the user's Report bug action. No automatic submission or
credential handling exists in this component.

`revealVerifiedInstaller` hashes once more immediately before revealing the file;
it may briefly occupy the UI thread for a large package. Download, first hash,
release polling and network work are on the dedicated low-priority worker.

## Build and validation

Add `Source/ReleaseSupport.cpp` to the product and link `juce_cryptography`.
`Tests/ReleaseSupportTests.cpp` is a standalone test executable linked with
`Source/ReleaseSupport.cpp`, `juce_core` and `juce_cryptography`.

On Linux, set `JUCE_USE_CURL=1`; the non-curl JUCE Linux backend does not implement
TLS. Use the linked libcurl implementation and package its runtime dependency.
Windows and macOS use their native HTTPS backends. Do not disable certificate
validation. Every release CI platform must compile and run the support tests.

Offline regression coverage includes SemVer prerelease ordering, invalid versions,
manifest schema/version/channel checks, HTTPS origin restrictions, traversal and
extension rejection, wrong-OS/CPU rejection, universal macOS selection, Linux DEB
preference, GitHub draft/alpha filtering, empty/API-error responses, actual SHA-256
validation against a known fixture, same-size corruption rejection and URL-encoded
privacy-limited issue drafts. Public-channel availability, actual CDN downloads,
OS package installation and code signing must additionally be verified during
release staging; these offline tests do not claim to perform those operations.
