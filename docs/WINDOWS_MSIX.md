# RavenForge / Chimera MSIX

Publisher display name: **RavenForge Luthier Intelligence**.

## What this package installs

The MSIX contains the x64 standalone instrument, both factory cabinet IRs, RavenForge
artwork and notices. Windows 10 version 2004 (19041) or later is required for the
release manifest. `win32App` / `mediumIL` preserves normal desktop audio operation
and the existing external user IR/preferences locations; the app is not converted
into an AppContainer. `runFullTrust` and microphone use are declared.

MSIX deployment does not copy its private files into an unrelated DAW's global
`Common Files/VST3` folder. Use the existing Setup installer for the VST3 plugin.
The MSIX does not silently launch an elevated second installer or claim that a
VST3 hidden under WindowsApps is discoverable by Studio One.

In the standalone app, use **IR LIBRARY > IMPORT PERSONAL ZIP** to import the
existing personal IR archive once, or **ADD FOLDER** to register another folder.
The private TONE3000 captures are excluded from Store/public packages. The private
EXE installer companion bundle still installs those six IRs automatically.

## Current review package

`Chimera-1.0.0.0-x64-review-unsigned.msix` is a real SDK-generated package for
inspection, not an authenticated customer release. It has no Store assignment
and no trusted signature, so ordinary double-click installation is not promised.
No customer certificate/root installation or disabling of Windows protections is
part of this delivery.

Build with the tested Windows portable artifact as input:

```powershell
./Tools/Build-WindowsMSIX.ps1 -Stage C:/Chimera/validated-package
```

`MakeAppx pack` performs manifest schema/semantic validation and SHA256 block-map
generation. CI verifies the checksum, round-trip executable bytes, branding and
package scope, then installs/activates/removes the same payload under a distinct
Microsoft-documented unsigned test identity on its disposable Windows runner.
That test package is deleted and never delivered. The test does not grant a
trusted publisher or constitute Windows App Certification Kit / Store approval.

## Microsoft Store signing route

Microsoft re-signs accepted MSIX packages after Store certification. Purchasing
a separate code-signing certificate is not required for this route.

Reserve the app in the publisher's Partner Center account and copy the exact
values from **Product management > Product identity** into a local JSON file:

```json
{
  "Name": "COPY_PACKAGE_IDENTITY_NAME_FROM_PARTNER_CENTER",
  "Publisher": "COPY_PACKAGE_IDENTITY_PUBLISHER_FROM_PARTNER_CENTER",
  "PublisherDisplayName": "RavenForge Luthier Intelligence"
}
```

These are non-secret package identity fields; do not supply passwords, certificate
private keys or account recovery codes. The review identity is not invented Store
registration. The build refuses Store mode without the actual identity file.

```powershell
./Tools/Build-WindowsMSIX.ps1 -Stage C:/Chimera/validated-package -Mode Store -IdentityFile C:/Chimera/partner-identity.json -Version 1.0.0.0
```

Run the Windows App Certification Kit on the resulting package and a supported
interactive Windows client, verify real audio input/output, then submit that exact
package through the matching app in Partner Center. Explain `runFullTrust`: this
is a native JUCE audio instrument using desktop audio devices and local IR files;
it does not install drivers/services or require administrator rights to play.

Account identity, listing/privacy/support information, certification and release
have not been submitted or approved. No Store listing or commercial acceptance
is implied by the local packaging tests.

## Direct MSIX distribution

Outside the Store, a trusted code-signing certificate is still needed:

```powershell
$env:CHIMERA_SIGNING_THUMBPRINT = '40_HEXADECIMAL_CHARACTERS_FROM_THE_ISSUED_CERTIFICATE'
./Tools/Build-WindowsMSIX.ps1 -Stage C:/Chimera/validated-package -Mode Signed
```

This derives the full manifest Publisher distinguished name from the certificate,
requires its verified display identity to match RavenForge Luthier Intelligence,
signs the executable and MSIX, timestamps them and verifies trust. A missing or
invalid identity stops the release. See `WINDOWS_SIGNING.md`.

Primary sources checked 2026-09-25:
- https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/app-package-requirements
- https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-manual-conversion
- https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-behind-the-scenes
- https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-f-application
- https://learn.microsoft.com/en-us/windows/msix/package/unsigned-package
