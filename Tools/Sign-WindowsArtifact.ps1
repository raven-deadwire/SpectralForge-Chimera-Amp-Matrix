param(
    [Parameter(Mandatory)][string[]]$Path,
    [string]$CertificateThumbprint = $env:CHIMERA_SIGNING_THUMBPRINT,
    [ValidateSet('Certificate', 'ArtifactSigning')][string]$SigningProvider = $(if ($env:CHIMERA_SIGNING_PROVIDER) { $env:CHIMERA_SIGNING_PROVIDER } else { 'Certificate' }),
    [string]$ExpectedPublisher = $(if ($env:CHIMERA_SIGNING_PUBLISHER) { $env:CHIMERA_SIGNING_PUBLISHER } else { 'RavenForge Luthier Intelligence' }),
    [string]$ExpectedSubject = $env:CHIMERA_SIGNING_SUBJECT,
    [string]$ArtifactSigningDlib = $env:CHIMERA_ARTIFACT_SIGNING_DLIB,
    [string]$ArtifactSigningMetadata = $env:CHIMERA_ARTIFACT_SIGNING_METADATA,
    [string]$TimestampUrl = $env:CHIMERA_SIGNING_TIMESTAMP_URL,
    [switch]$VerifyOnly
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'Windows-SigningContract.ps1')
if (!$VerifyOnly) { Assert-ChimeraSigningContext }
$configuration = Get-ChimeraSigningConfiguration -SigningProvider $SigningProvider -CertificateThumbprint $CertificateThumbprint `
    -ExpectedPublisher $ExpectedPublisher -ExpectedSubject $ExpectedSubject -ArtifactSigningDlib $ArtifactSigningDlib `
    -ArtifactSigningMetadata $ArtifactSigningMetadata -TimestampUrl $TimestampUrl -VerifyOnly:$VerifyOnly
if ($env:OS -ne "Windows_NT") { throw "Authenticode signing requires Windows and a trusted code-signing identity." }
$command = Get-Command signtool.exe -ErrorAction SilentlyContinue
$signTool = if ($command) { $command.Source } else {
    Get-ChildItem -Path "${env:ProgramFiles(x86)}/Windows Kits/10/bin/*/x64/signtool.exe" -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (!$signTool) { throw "Install the Windows SDK Signing Tools component." }
if (!$VerifyOnly -and $SigningProvider -eq 'Certificate') {
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
        $signArguments = $configuration.SignArguments + @($target)
        & $signTool @signArguments
        if ($LASTEXITCODE -ne 0) { throw "Signing or timestamping failed: $target" }
    }
    & $signTool verify /pa /all /tw $target
    if ($LASTEXITCODE -ne 0) { throw "Authenticode trust/timestamp verification failed: $target" }
    $signature = Get-AuthenticodeSignature -LiteralPath $target
    Assert-ChimeraSignedIdentity $signature $configuration $target
    Write-Host "Verified publisher: $ExpectedPublisher | $target"
}
