# Offline: never calls Azure, a private key, SignTool or Inno.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot '../Tools/Windows-SigningContract.ps1')
function Assert-True($Value, [string]$Message) { if (!$Value) { throw $Message } }
function Assert-Rejected([scriptblock]$Action, [string]$Pattern) {
    try { & $Action } catch {
        if ($_.Exception.Message -notmatch $Pattern) { throw }
        return
    }
    throw "Expected rejection: $Pattern"
}
function New-Signature([string]$Thumbprint, [string]$Subject = 'CN=Verified Publisher, O=Verified Publisher, C=KR') {
    $cert = [pscustomobject]@{ Thumbprint = $Thumbprint; Subject = $Subject; Publisher = 'Verified Publisher' }
    $cert | Add-Member -MemberType ScriptMethod -Name GetNameInfo -Value { param($Type, $Issuer) $this.Publisher }
    [pscustomobject]@{ Status = 'Valid'; SignatureType = 'Authenticode'; TimeStamperCertificate = 'timestamp fixture'; SignerCertificate = $cert }
}
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('Chimera-Signing-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
$saved = @{}
foreach ($key in @('GITHUB_EVENT_NAME', 'GITHUB_REF', 'GITHUB_HEAD_REF')) {
    $saved[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
}
try {
    # Parse entry points too; these tests must work without the Windows-only APIs.
    foreach ($name in @('Build-WindowsInstaller.ps1', 'Sign-WindowsArtifact.ps1', 'Windows-SigningContract.ps1')) {
        $tokens = $null; $parseErrors = $null
        [void][Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot "../Tools/$name"), [ref]$tokens, [ref]$parseErrors)
        Assert-True ($parseErrors.Count -eq 0) "PowerShell parse error in $name : $parseErrors"
    }
    $thumbprint = 'A' * 40
    $ca = Get-ChimeraSigningConfiguration -CertificateThumbprint $thumbprint -ExpectedPublisher 'Verified Publisher'
    Assert-True (($ca.SignArguments -join '|') -eq "sign|/sha1|$thumbprint|/s|My|/fd|SHA256|/tr|http://timestamp.digicert.com|/td|SHA256|/d|Chimera Amp Matrix") 'CA signing arguments changed'
    Assert-Rejected { Get-ChimeraSigningConfiguration -ExpectedPublisher 'Verified Publisher' } 'verified public code-signing certificate is required'
    Assert-Rejected { Get-ChimeraSigningConfiguration -SigningProvider 'Unknown' } 'ValidateSet|does not belong|validation'
    Assert-Rejected { Get-ChimeraSigningConfiguration -CertificateThumbprint $thumbprint -ExpectedPublisher 'Verified Publisher' -TimestampUrl 'file:///tmp/stamp' } 'timestamp endpoint'

    $dlib = Join-Path $fixture 'Azure.CodeSigning.Dlib.dll'
    $metadataPath = Join-Path $fixture 'metadata with spaces.json'
    Set-Content -LiteralPath $dlib -Value 'non-executable fixture'
    $metadata = @{ Endpoint = 'https://krc.codesigning.azure.net'; CodeSigningAccountName = 'fixtureAccount'; CertificateProfileName = 'fixtureProfile' }
    $metadata | ConvertTo-Json | Set-Content -LiteralPath $metadataPath
    $argsArtifact = @{
        SigningProvider = 'ArtifactSigning'; ExpectedPublisher = 'Verified Publisher'
        ExpectedSubject = 'CN=Verified Publisher, O=Verified Publisher, C=KR'
        ArtifactSigningDlib = $dlib; ArtifactSigningMetadata = $metadataPath
    }
    $artifact = Get-ChimeraSigningConfiguration @argsArtifact
    Assert-True (($artifact.SignArguments -join '|') -eq "sign|/dlib|$dlib|/dmdf|$metadataPath|/fd|SHA256|/tr|http://timestamp.acs.microsoft.com|/td|SHA256|/d|Chimera Amp Matrix") 'Artifact Signing arguments or timestamp changed'
    Assert-Rejected { Get-ChimeraSigningConfiguration @argsArtifact -CertificateThumbprint $thumbprint } 'cannot be combined'
    $badArgs = $argsArtifact.Clone(); $badArgs.ExpectedSubject = ''
    Assert-Rejected { Get-ChimeraSigningConfiguration @badArgs } 'full certificate subject'
    $badArgs = $argsArtifact.Clone(); $badArgs.ArtifactSigningDlib = ''
    Assert-Rejected { Get-ChimeraSigningConfiguration @badArgs } 'installed dlib'
    $metadata.Endpoint = 'https://attacker.example/codesigning.azure.net'
    $metadata | ConvertTo-Json | Set-Content -LiteralPath $metadataPath
    Assert-Rejected { Get-ChimeraSigningConfiguration @argsArtifact } 'account region'
    $metadata.Remove('CertificateProfileName')
    $metadata | ConvertTo-Json | Set-Content -LiteralPath $metadataPath
    Assert-Rejected { Get-ChimeraSigningConfiguration @argsArtifact } 'requires CertificateProfileName'
    # Credential-free verification must not read dlib/metadata or require today's leaf.
    $audit = Get-ChimeraSigningConfiguration -SigningProvider ArtifactSigning -ExpectedPublisher $artifact.ExpectedPublisher -ExpectedSubject $artifact.ExpectedSubject -VerifyOnly
    Assert-ChimeraSignedIdentity (New-Signature $thumbprint) $ca 'ca.exe'
    Assert-ChimeraSignedIdentity (New-Signature ('B' * 40)) $audit 'rotated-leaf.exe'
    Assert-Rejected { Assert-ChimeraSignedIdentity (New-Signature ('B' * 40)) $ca 'wrong-ca.exe' } 'pinned release identity'
    Assert-Rejected { Assert-ChimeraSignedIdentity (New-Signature $thumbprint 'CN=Verified Publisher, O=Other, C=KR') $audit 'wrong-subject.exe' } 'pinned release identity'
    $wrongPublisher = New-Signature $thumbprint; $wrongPublisher.SignerCertificate.Publisher = 'Other'
    Assert-Rejected { Assert-ChimeraSignedIdentity $wrongPublisher $audit 'wrong-publisher.exe' } 'pinned release identity'
    foreach ($mutation in @(@{ Status = 'NotTrusted' }, @{ Status = 'HashMismatch' }, @{ SignatureType = 'Catalog' }, @{ TimeStamperCertificate = $null }, @{ SignerCertificate = $null })) {
        $signature = New-Signature $thumbprint
        foreach ($key in $mutation.Keys) { $signature.$key = $mutation[$key] }
        Assert-Rejected { Assert-ChimeraSignedIdentity $signature $audit 'invalid.exe' } 'timestamped embedded signature'
    }

    $env:GITHUB_REF = ''; $env:GITHUB_HEAD_REF = ''
    foreach ($event in @('pull_request', 'pull_request_target', 'merge_group')) {
        $env:GITHUB_EVENT_NAME = $event
        Assert-Rejected { Assert-ChimeraSigningContext } 'forbidden'
        foreach ($provider in @('Certificate', 'ArtifactSigning')) {
            Assert-Rejected { & (Join-Path $PSScriptRoot '../Tools/Build-WindowsInstaller.ps1') -Sign -SigningProvider $provider } 'forbidden'
            Assert-Rejected { & (Join-Path $PSScriptRoot '../Tools/Sign-WindowsArtifact.ps1') -Path 'never-opened.exe' -SigningProvider $provider } 'forbidden'
        }
    }
    $env:GITHUB_EVENT_NAME = 'workflow_dispatch'; $env:GITHUB_REF = 'refs/pull/42/head'
    Assert-Rejected { Assert-ChimeraSigningContext } 'forbidden'
    $env:GITHUB_REF = 'refs/heads/main'; $env:GITHUB_HEAD_REF = 'untrusted-branch'
    Assert-Rejected { Assert-ChimeraSigningContext } 'forbidden'
    $env:GITHUB_HEAD_REF = ''
    Assert-ChimeraSigningContext
    Assert-Rejected { & (Join-Path $PSScriptRoot '../Tools/Build-WindowsInstaller.ps1') -Sign -SigningProvider Certificate -CertificateThumbprint '' } 'verified public code-signing certificate is required'

    # Exercise the real wrapper's dispatch and native exit-code handling using
    # an inert SignTool stand-in. No Windows signing API or credential is used.
    $stub = Join-Path $fixture 'signtool-stub.ps1'
    @'
param([Parameter(ValueFromRemainingArguments = $true)][object[]]$Arguments)
[void]$global:ChimeraSigningTestCalls.Add(@($Arguments))
$global:LASTEXITCODE = if ($Arguments[0] -eq 'sign') { $global:ChimeraSigningTestSignExit } else { $global:ChimeraSigningTestVerifyExit }
'@ | Set-Content -LiteralPath $stub
    $global:ChimeraSigningTestTool = $stub
    $global:ChimeraSigningTestSignature = New-Signature ('B' * 40)
    $global:ChimeraSigningTestCalls = [Collections.Generic.List[object]]::new()
    $global:ChimeraSigningTestSignExit = 0; $global:ChimeraSigningTestVerifyExit = 0
    $oldOS = $env:OS
    try {
        function Get-Command {
            [CmdletBinding()]param([string]$Name)
            if ($Name -ne 'signtool.exe') { throw "Unexpected command lookup in signing test: $Name" }
            [pscustomobject]@{ Source = $global:ChimeraSigningTestTool }
        }
        function Get-AuthenticodeSignature {
            param([string]$LiteralPath)
            $global:ChimeraSigningTestSignature
        }
        $env:OS = 'Windows_NT'
        $metadata.Endpoint = 'https://krc.codesigning.azure.net'; $metadata.CertificateProfileName = 'fixtureProfile'
        $metadata | ConvertTo-Json | Set-Content -LiteralPath $metadataPath
        $target = Join-Path $fixture 'never-signed fixture.exe'; Set-Content -LiteralPath $target -Value 'fixture'
        $wrapper = Join-Path $PSScriptRoot '../Tools/Sign-WindowsArtifact.ps1'
        & $wrapper -Path $target @argsArtifact -CertificateThumbprint ''
        Assert-True ($global:ChimeraSigningTestCalls.Count -eq 2) 'Wrapper did not sign then verify'
        Assert-True (($global:ChimeraSigningTestCalls[0] -join '|') -eq (($artifact.SignArguments + @($target)) -join '|')) 'Wrapper lost provider arguments or target quoting'
        Assert-True (($global:ChimeraSigningTestCalls[1] -join '|') -eq "verify|/pa|/all|/tw|$target") 'Wrapper weakened trust/timestamp verification'
        $global:ChimeraSigningTestCalls.Clear(); $global:ChimeraSigningTestSignExit = 1
        Assert-Rejected { & $wrapper -Path $target @argsArtifact -CertificateThumbprint '' } 'Signing or timestamping failed'
        Assert-True ($global:ChimeraSigningTestCalls.Count -eq 1) 'Failed signing continued'
        $global:ChimeraSigningTestSignExit = 0; $global:ChimeraSigningTestVerifyExit = 2
        Assert-Rejected { & $wrapper -Path $target @argsArtifact -CertificateThumbprint '' } 'trust/timestamp verification failed'
        $global:ChimeraSigningTestVerifyExit = 0; $global:ChimeraSigningTestCalls.Clear()
        & $wrapper -Path $target -SigningProvider ArtifactSigning -ExpectedPublisher $artifact.ExpectedPublisher -ExpectedSubject $artifact.ExpectedSubject -CertificateThumbprint '' -VerifyOnly
        Assert-True ($global:ChimeraSigningTestCalls.Count -eq 1 -and $global:ChimeraSigningTestCalls[0][0] -eq 'verify') 'VerifyOnly attempted to sign'
    } finally {
        $env:OS = $oldOS
        Remove-Item Function:Get-Command, Function:Get-AuthenticodeSignature
        Remove-Variable -Name 'ChimeraSigningTest*' -Scope Global
    }

    $stage = Join-Path $fixture 'stage'; New-Item -ItemType Directory -Path $stage | Out-Null
    $binary = Join-Path $stage 'payload.exe'; Set-Content -LiteralPath $binary -Value 'reviewed payload'
    $entry = @{ path = 'payload.exe'; bytes = (Get-Item $binary).Length; sha256 = (Get-FileHash $binary -Algorithm SHA256).Hash }
    $manifest = Join-Path $stage 'payload-manifest.json'
    @{ files = @($entry) } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifest
    Assert-ChimeraUnsignedPayload $stage
    Set-Content -LiteralPath $binary -Value 'tampered payload'
    Assert-Rejected { Assert-ChimeraUnsignedPayload $stage } 'hash/size differs'
    Set-Content -LiteralPath $binary -Value 'reviewed payload'
    $extra = Join-Path $stage 'unlisted.dll'; Set-Content -LiteralPath $extra -Value 'extra'
    Assert-Rejected { Assert-ChimeraUnsignedPayload $stage } 'Unlisted file'
    Remove-Item -LiteralPath $extra
    @{ files = @($entry, $entry) } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifest
    Assert-Rejected { Assert-ChimeraUnsignedPayload $stage } 'Invalid signed payload inventory'
    $entry.path = '../outside.exe'
    @{ files = @($entry) } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifest
    Assert-Rejected { Assert-ChimeraUnsignedPayload $stage } 'Invalid signed payload inventory'
    Write-Host 'PASS: provider selection, native failure propagation, verify-only, leaf rotation, identity/timestamp rejection, PR denial and payload integrity (offline only)'
} finally {
    foreach ($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key, $saved[$key], 'Process') }
    Remove-Item -LiteralPath $fixture -Recurse -Force
}
