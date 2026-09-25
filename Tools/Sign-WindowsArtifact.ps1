param(
    [Parameter(Mandatory)][string[]]$Path,
    [string]$CertificateThumbprint = $env:CHIMERA_SIGNING_THUMBPRINT,
    [string]$ExpectedPublisher = "RavenForge Luthier Intelligence",
    [string]$TimestampUrl = "http://timestamp.digicert.com",
    [switch]$VerifyOnly
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
if ($env:OS -ne "Windows_NT") { throw "Authenticode signing requires Windows and a trusted code-signing identity." }
if ($CertificateThumbprint -notmatch '^[0-9A-Fa-f]{40}$') {
    throw "A verified public code-signing certificate is required. Set CHIMERA_SIGNING_THUMBPRINT to its 40-character thumbprint; see docs/WINDOWS_SIGNING.md."
}
$command = Get-Command signtool.exe -ErrorAction SilentlyContinue
$signTool = if ($command) { $command.Source } else {
    Get-ChildItem -Path "${env:ProgramFiles(x86)}/Windows Kits/10/bin/*/x64/signtool.exe" -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (!$signTool) { throw "Install the Windows SDK Signing Tools component." }
if (!$VerifyOnly) {
    $certificate = Get-Item -LiteralPath "Cert:/CurrentUser/My/$CertificateThumbprint" -ErrorAction Stop
    if (!$certificate.HasPrivateKey -or $certificate.NotAfter -le (Get-Date) -or $certificate.NotBefore -gt (Get-Date)) {
        throw "The code-signing certificate must be current and its token/HSM private key accessible."
    }
    if ($certificate.Subject -eq $certificate.Issuer) { throw "Self-signed certificates are not accepted for distribution." }
    if ($certificate.GetNameInfo([Security.Cryptography.X509Certificates.X509NameType]::SimpleName, $false) -cne $ExpectedPublisher) {
        throw "Certificate identity does not exactly match the expected publisher. Metadata cannot override a CA-validated publisher."
    }
    $chain = [Security.Cryptography.X509Certificates.X509Chain]::new()
    try {
        $chain.ChainPolicy.RevocationMode = [Security.Cryptography.X509Certificates.X509RevocationMode]::Online
        $chain.ChainPolicy.ApplicationPolicy.Add([Security.Cryptography.Oid]::new("1.3.6.1.5.5.7.3.3"))
        if (!$chain.Build($certificate)) { throw "The code-signing certificate does not chain to a trusted, valid public identity." }
    } finally { $chain.Dispose() }
}
foreach ($item in $Path) {
    $target = (Resolve-Path -LiteralPath $item).Path
    if (!$VerifyOnly) {
        & $signTool sign /sha1 $CertificateThumbprint /s My /fd SHA256 /tr $TimestampUrl /td SHA256 /d "Chimera Amp Matrix" $target
        if ($LASTEXITCODE -ne 0) { throw "Signing or timestamping failed: $target" }
    }
    & $signTool verify /pa /all /tw $target
    if ($LASTEXITCODE -ne 0) { throw "Authenticode trust/timestamp verification failed: $target" }
    $signature = Get-AuthenticodeSignature -LiteralPath $target
    if ($signature.Status -ne "Valid" -or !$signature.TimeStamperCertificate -or !$signature.SignerCertificate) {
        throw "A valid, timestamped embedded signature is required: $target"
    }
    if ($signature.SignerCertificate.Thumbprint -ine $CertificateThumbprint -or
        $signature.SignerCertificate.GetNameInfo([Security.Cryptography.X509Certificates.X509NameType]::SimpleName, $false) -cne $ExpectedPublisher) {
        throw "Signed publisher does not match the pinned release identity: $target"
    }
    Write-Host "Verified publisher: $ExpectedPublisher | $target"
}
