# Windows publisher signing

Publisher: **RavenForge Luthier Intelligence**. The installer metadata uses this name.
This is not a digital signature. The current CI builds remain explicitly unsigned test builds.
No certificate or signing-service identity has been provided or connected.

## Prepared release path

Use a publicly trusted CA-issued code-signing certificate with a hardware token or HSM.
Install its provider on the signing Windows machine so its certificate appears in
`Cert:\CurrentUser\My` and SignTool can access the private key. Keep the private key on
the token/HSM; do not place a PFX, PIN or password in this repository or chat.

The exact CA-verified subject must be RavenForge Luthier Intelligence for Windows to
display that publisher. If the CA validates a different legal identity, the publisher
will be that identity. A product/company string cannot substitute for identity verification.

After downloading and unpacking the tested portable Windows artifact:

```powershell
$env:CHIMERA_SIGNING_THUMBPRINT = '40_HEXADECIMAL_CHARACTERS_FROM_THE_ISSUED_CERTIFICATE'
./Tools/Build-WindowsInstaller.ps1 -Stage C:/Chimera/validated-package -OutputDirectory C:/Chimera/signed -Sign
```

The script signs and verifies every shipped EXE/DLL/VST3 before packaging. Inno then
signs its generated uninstaller and Setup with the same identity. SHA-256 signatures
and RFC 3161 timestamps are required. A missing certificate, mismatched publisher,
untrusted chain, missing timestamp or failed verification stops the signed build.
It never silently falls back to an unsigned release. `-VerifyOnly` can audit returned
artifacts on a separate Windows machine without access to the private key.

Unsigned PR builds do not access signing credentials. Signing belongs on a trusted
release workstation/runner after the exact revision and artifacts have been reviewed.
The private personal IR archive is not published as part of the public build.

## Identity and SmartScreen are different

A trusted signature removes "Unknown publisher" by displaying the validated signer.
It does not promise that every new file will avoid SmartScreen reputation warnings.
New files and new publishers can still be unrecognized, including with EV certificates.
Do not install a self-signed root on customer PCs or disable Windows protections.

Microsoft currently limits Artifact Signing public-trust enrollment to organizations
in the US, Canada, EU and UK, and individuals in the US and Canada. For a Korean
individual or business, select a CA that verifies that legal identity and provides a
compatible token/cloud HSM. No purchase or identity application has been made here.
SignPath Foundation is a possible alternative only for qualifying open-source projects;
it is not a substitute for this product's requested commercial publisher identity.

Primary references checked 2026-09-25:
- https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/code-signing-options
- https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation
- https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool
- https://jrsoftware.org/ishelp/topic_setup_signtool.htm
- https://jrsoftware.org/ishelp/topic_setup_signeduninstaller.htm
