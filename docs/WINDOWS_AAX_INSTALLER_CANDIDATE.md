# Windows x64 AAX installer candidate

This opt-in installer flavor is not the public publisher, updater, or a new
release of 1.5.0-beta.1. Normal Setup builds and feeds remain unchanged. macOS,
certificate issuance, actual signing and retail Pro Tools qualification are
not covered by this change.

## Inputs and build

Use an exact product-source checkout and its normal Windows package stage.
In a separate input directory, place the final signed
`SpectralForge Chimera.aaxplugin` bundle and `aax-release-receipt.json`.
Obtain the receipt and its SHA-256 out of band from the authorized verifier.
A receipt created by the installer builder from an unknown bundle is not
independent evidence.

The receipt is an object with `schema` set to
`spectralforge.chimera.windows-aax-release.v1`, `platform` to `windows`,
`architectures` to `["x86_64"]`, `source_sha` to the full product commit,
`version` and `product_version` to the exact package and numeric identities,
and `files` mapping the following relative paths to lowercase SHA-256 hashes:

- `Contents/x64/SpectralForge Chimera.aaxplugin`
- `Plugin.ico`
- `desktop.ini`

Record `pace_verification: "PASS"` only after verifying the exact **final**
bundle with the authorized PACE workflow. Authenticode verification is a
separate check; a trusted Windows certificate does not prove the PACE step.
The build checks the expected certificate subject/thumbprint and timestamp
on the copied binary in VerifyOnly mode. It does not apply another signature
to the AAX payload. It also checks the PE architecture, product version,
exact source/version, and full recorded inventory. Do not use the unsigned
development receipt or change any bundle bytes after verification.

```powershell
./Tools/Build-WindowsInstaller.ps1 -Stage $Stage -Sign `
  -CertificateThumbprint $ReleaseThumbprint -ExpectedPublisher $VerifiedPublisher `
  -SignedAaxInput $VerifiedInput -AaxReceiptSha256 $IndependentlyVerifiedReceiptSha256
```

Output: `SpectralForge-Chimera-<version>-win64-AAX-Setup.exe` and its SHA-256
sidecar. Do not rename this for use with the stock publisher or update feed.
The signed wrapper and signed AAX must use the same approved public signer.

## Windows qualification (not performed by the build)

1. On a clean, native x64 Windows test PC, establish Setup trust and verify its
   SHA-256. Close Pro Tools. Start from no Chimera AAX in the common Avid
   plug-in folder. Install into the fixed 64-bit Avid path.
2. Re-run `python Tools/aax_windows_release.py <verification-directory> <pin>`
   using the same receipt and a copy of the installed bundle. Compare all file
   hashes and verify the installed binary with `Sign-WindowsArtifact.ps1
   -VerifyOnly` and the same expected certificate identity.
3. Open ordinary retail Pro Tools (no developer mode). Verify the scan, create
   a track and insert Chimera, play audio and save/reopen the session. Record
   Pro Tools version, Windows build, exact commit, installer and installed
   bundle digests. Do not report retail PASS from installation alone.
4. Exit Pro Tools, uninstall and confirm that only the installer-owned files
   disappear, other AAX plug-ins are unchanged, and reinstall works. Check
   rejection with a pre-existing hand-installed/development bundle. The first
   AAX installer deliberately does not overwrite existing AAX bundles;
   in-place AAX upgrades are not qualified.
5. Build normal Setup without AAX inputs; confirm the usual filename, no AAX
   payload, and unchanged Windows installer and updater smoke tests.

Without a new certificate purchase, the Python contract tests can already
exercise pinned-inventory and wrong-source rejection. A signed-input
verification test may use an existing independently verified same-source
production artifact; VerifyOnly does not need a private key. Do not substitute
a self-signed certificate in the release path. A self-signed PACE/installer
lab smoke or vendor sandbox, if separately available, cannot qualify public
trust or retail distribution.
