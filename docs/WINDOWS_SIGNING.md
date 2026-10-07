# Windows publisher signing

Publisher: **RavenForge Luthier Intelligence**. The installer metadata uses this name.
This is not a digital signature. The current CI builds remain explicitly unsigned test builds.
No certificate or signing-service identity has been provided or connected.

For the standalone app, the new MSIX/Store route can avoid purchasing a separate
certificate: Microsoft signs the MSIX after Store certification. The reserved app's
Partner Center identity must be supplied first. See `WINDOWS_MSIX.md`; ordinary
direct-download MSIX files still require a trusted signature. VST3 Setup remains
an EXE distribution and is not automatically signed by submitting an MSIX.

## Provider choice and Korea eligibility (checked 2026-10-07)

**South Korean organizations can apply for Artifact Signing Public Trust.** The
service Quickstart lists South Korea explicitly and provides Korea Central at
`https://krc.codesigning.azure.net`. A resource region is not, by itself, proof of
identity eligibility. Individual-developer enrollment remains US/Canada only;
do not assume a Korean individual or sole proprietor qualifies as an organization.

Prepare an Entra tenant, a **paid** Azure subscription, `Microsoft.CodeSigning`
registration, legal business name/address/identifier, business website and two
same-domain organization email addresses. Organization validation includes a
representative's identity check. Complete validation in Azure Portal, then create
a **Public Trust** certificate profile. Requested supporting records must match
the legal entity; approval is not automatic. Budget 1–20 business days or longer.

Some Windows/MSIX overview pages still show the old country list and a three-year
tax-history requirement. The current service Quickstart includes Korea and does
not state that age threshold. Do not reintroduce it as a confirmed current rule,
or promise that a new business qualifies: confirm any case-specific requirement
in the Portal or with Microsoft support before committing to onboarding.

| Path | Chimera integration | Operational choice |
| --- | --- | --- |
| Existing CA token/HSM | Default `Certificate`; pin leaf thumbprint and publisher | Preserve for existing certificates, ineligible applicants, or CA-specific requirements |
| Artifact Signing Public Trust | Explicit `ArtifactSigning`; SignTool dlib + metadata; pin publisher and full subject | Preferred evaluation path for an eligible Korean organization; managed key/certificate lifecycle |

Keep both providers, selecting **one per build**. No automatic fallback or dual
signing is introduced. A service/authentication outage must stop the signed build.
Microsoft recommends Artifact Signing for non-Store distribution, but neither
provider guarantees immediate SmartScreen reputation. No account, identity
application, payment, credential or signing permission has been provisioned here.

## Existing CA release path (unchanged default)

Use a publicly trusted CA-issued code-signing certificate with a hardware token or HSM.
Install its provider on the signing Windows machine so its certificate appears in
`Cert:\CurrentUser\My` and SignTool can access the private key. Keep the private key on
the token/HSM; do not place a PFX, PIN or password in this repository or chat.

The exact CA-verified subject must be RavenForge Luthier Intelligence for Windows to
display that publisher. If the CA validates a different legal identity, the publisher
will be that identity. A product/company string cannot substitute for identity verification.

After the exact-source checks below, on a trusted Windows signing machine:

```powershell
$env:CHIMERA_SIGNING_THUMBPRINT = '40_HEXADECIMAL_CHARACTERS_FROM_THE_ISSUED_CERTIFICATE'
./Tools/Build-WindowsInstaller.ps1 -Stage C:/Chimera/validated-package -OutputDirectory C:/Chimera/signed -Sign
```

## Artifact Signing adapter

Install a compatible Windows SDK SignTool, .NET 8 runtime, VC++ runtime and the
official Artifact Signing client dlib. Match their x64 architectures, and pin and
record the reviewed client/SDK versions on the release machine. The official
Client Tools installer bundles these prerequisites. No tool is downloaded at
build time by these scripts.

Create external `C:/Chimera/signing/metadata.json` with the approved account/profile
(these are placeholders, not provisioned resources). This example deliberately
uses only an existing Azure CLI login through the documented credential chain:

```json
{
  "Endpoint": "https://krc.codesigning.azure.net",
  "CodeSigningAccountName": "REPLACE_WITH_APPROVED_ACCOUNT",
  "CertificateProfileName": "REPLACE_WITH_PUBLIC_TRUST_PROFILE",
  "ExcludeCredentials": [
    "EnvironmentCredential", "WorkloadIdentityCredential", "ManagedIdentityCredential",
    "SharedTokenCacheCredential", "VisualStudioCredential", "VisualStudioCodeCredential",
    "AzurePowerShellCredential", "AzureDeveloperCliCredential", "InteractiveBrowserCredential"
  ]
}
```

The endpoint must match the actual account region. Assign **Artifact Signing
Certificate Profile Signer** only on the selected profile to the signing identity;
keep **Identity Verifier** and provisioning privileges separate. On a trusted
workstation, authenticate with `az login` under that identity. Metadata contains
no private key or token and must not contain credentials.

```powershell
$env:CHIMERA_SIGNING_THUMBPRINT = $null
$env:CHIMERA_SIGNING_PROVIDER = 'ArtifactSigning'
$env:CHIMERA_ARTIFACT_SIGNING_DLIB = 'C:/Chimera/signing/x64/Azure.CodeSigning.Dlib.dll'
$env:CHIMERA_ARTIFACT_SIGNING_METADATA = 'C:/Chimera/signing/metadata.json'
$env:CHIMERA_SIGNING_PUBLISHER = '<exact validated legal common name>'
$env:CHIMERA_SIGNING_SUBJECT = '<exact full X509Certificate2.Subject of the approved identity>'
./Tools/Build-WindowsInstaller.ps1 -Stage C:/Chimera/validated-package -OutputDirectory C:/Chimera/signed -Sign
```

Independently review the profile's verified legal identity and an onboarding
certificate's full subject before setting the pin. Never learn the expected
identity from the release artifact being verified. `RavenForge Luthier Intelligence`
can be the displayed signer only if that name is actually validated. The service
does not allow an arbitrary brand CN/O. Subject changes require explicit review;
short-lived leaf certificate rotation does not. Clear Artifact Signing environment
values (or explicitly select `-SigningProvider Certificate`) to return to the CA path.

The common wrapper uses `/fd SHA256 /tr <RFC3161 URL> /td SHA256`, adding either
`/sha1 <thumbprint> /s My` or `/dlib <dll> /dmdf <metadata>`. Artifact Signing defaults
to `http://timestamp.acs.microsoft.com`; CA signing retains DigiCert's timestamp
endpoint. `-TimestampUrl` / `CHIMERA_SIGNING_TIMESTAMP_URL` can explicitly override
it. Timestamping is essential for Artifact Signing's three-day leaf certificates.

Inno's existing `ChimeraRelease` hook and `SignedUninstaller=yes` are retained.
Payload EXE/DLL/VST3, generated uninstaller and Setup all use the same wrapper and
identity configuration. Paths are resolved before Inno runs; only non-secret
configuration is forwarded through environment variables and restored afterwards.
The Azure credential context is inherited by Inno's child process; it is not put
on the command line. Each signing operation immediately runs
`signtool verify /pa /all /tw` plus embedded-signature, timestamp and identity
checks. Nonzero signing/verification exit codes, absent configuration, wrong
publisher/subject/thumbprint, untrusted signatures or missing timestamps abort.

For a credential-free audit, use the same approved identity pins on another
Windows machine, and pass `-VerifyOnly` to `Tools/Sign-WindowsArtifact.ps1` for each
returned binary, Setup and installed uninstaller. Artifact Signing verification
does not need Azure login, metadata, dlib or a fixed leaf thumbprint.

## Exact source and release boundary

1. Review the full source SHA, successful build run and required consolidated
   release-gate evidence for **that SHA**. Download that run's tested Windows
   artifact and verify its recorded archive hash. Never select a mutable "latest"
   artifact. Retain the original unsigned archive and manifest as evidence.
2. Use a clean checkout of that same SHA with these signing adapters integrated.
   The existing `chimera_version.py --manifest` check still rejects mismatched
   full source SHA, product/package version and BuildId; binary version checks
   remain in place. Manifest hashes are now checked before any signing, including
   rejection of extra files, duplicate paths and paths escaping the stage.
3. Sign a working copy. Refresh file hashes only after payload signatures pass,
   preserving source/version fields. Inno embeds this signed-payload manifest.
   Setup is verified again before its final SHA-256 sidecar is written. An old
   unsigned hash is not evidence for a newly signed file.
4. Run `Verify-WindowsInstaller.ps1` on the signed stage/Setup for install, repair
   and uninstall acceptance; independently audit the installed uninstaller's
   signature before removal. Preserve source SHA, run/artifact IDs, both manifest
   hashes, final Setup hash, provider/profile, signer subjects/thumbprints,
   timestamp and verification logs. Existing publication gates still apply;
   this adapter does not authorize publishing or replace acceptance evidence.

Unsigned PR builds do not access signing credentials. Signing belongs on a trusted
release workstation/runner after the exact revision and artifacts have been reviewed.
The wrappers also refuse signing in PR, PR-target and merge-queue contexts; this
is defense in depth, not an authorization boundary against modified PR code.
CI changes here run offline contract tests only: no Azure login, secrets, protected
signing environment, HSM access or `id-token: write` is added to a PR workflow.

If a separate signing workflow is later enabled, use a protected manual release
workflow with reviewed trusted workflow code, pinned source/artifact IDs and
required environment reviewers/branch restrictions. Scope OIDC federation to that
repository's protected signing environment, and the Azure role to one certificate
profile. Only that signing job may request `id-token: write`; do not use wildcard
PR/ref federation, `pull_request_target`, or an automatic `workflow_run` consumer
that grants credentials to PR artifacts. Do not execute artifact-supplied scripts
or use a PR-capable self-hosted runner with cached Azure credentials or CA keys.
The private personal IR archive is not published as part of the public build.

## Identity and SmartScreen are different

A trusted signature removes "Unknown publisher" by displaying the validated signer.
It does not promise that every new file will avoid SmartScreen reputation warnings.
New files and new publishers can still be unrecognized, including with EV certificates.
Do not install a self-signed root on customer PCs or disable Windows protections.

SignPath Foundation is a possible alternative only for qualifying open-source projects;
it is not a substitute for this product's requested commercial publisher identity.

## Validation status

Offline contract tests cover both provider selections, rotating service leaf
certificates, identity/timestamp rejection, PR denial and input-hash tampering.
They do not prove live Azure authorization, public chain trust, timestamp service
availability or signed Inno installation. Those remain **BLOCKED until exercised
with an approved identity on Windows**; unsigned CI success is not signing approval.

Primary references checked 2026-10-07 (service Quickstart governs the country list):
- https://learn.microsoft.com/en-us/azure/artifact-signing/quickstart
- https://learn.microsoft.com/en-us/azure/artifact-signing/faq
- https://learn.microsoft.com/en-us/azure/artifact-signing/how-to-signing-integrations
- https://learn.microsoft.com/en-us/azure/artifact-signing/tutorial-assign-roles
- https://learn.microsoft.com/en-us/windows/msix/package/signing-package-overview
- https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/code-signing-options
- https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation
- https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool
- https://jrsoftware.org/ishelp/topic_setup_signtool.htm
- https://jrsoftware.org/ishelp/topic_setup_signeduninstaller.htm
