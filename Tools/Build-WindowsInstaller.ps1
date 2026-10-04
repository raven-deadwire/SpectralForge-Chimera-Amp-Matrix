param(
    [string]$Stage = "",
    [string]$OutputDirectory = "dist",
    [string]$BuildId = "",
    [switch]$Sign,
    [string]$CertificateThumbprint = $env:CHIMERA_SIGNING_THUMBPRINT,
    [string]$ExpectedPublisher = "RavenForge Luthier Intelligence"
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
if ($Sign -and $CertificateThumbprint -notmatch '^[0-9A-Fa-f]{40}$') {
    throw "A verified public code-signing certificate is required for a signed release."
}
$versionTool = Join-Path $PSScriptRoot "chimera_version.py"
$identityJson = & python $versionTool
if ($LASTEXITCODE -ne 0) { throw "Cannot resolve the source product version." }
$identity = $identityJson | ConvertFrom-Json
if (!$Stage) { $Stage = "dist/SpectralForge-Chimera-$($identity.version)-win64" }
$stagePath = (Resolve-Path -LiteralPath $Stage).Path
$required = @(
    "Standalone/SpectralForge Chimera.exe",
    "VST3/SpectralForge Chimera.vst3/Contents/x86_64-win/SpectralForge Chimera.vst3",
    "ReferenceTools/ChimeraRender.exe", "WINDOWS_INSTALL.txt", "Verification.txt", "MANUAL.html", "payload-manifest.json"
)
foreach ($file in $required) {
    if (!(Test-Path -LiteralPath (Join-Path $stagePath $file) -PathType Leaf)) {
        throw "Installer input is missing: $file"
    }
}
$versionArguments = @($versionTool, "--manifest", (Join-Path $stagePath "payload-manifest.json"))
if ($BuildId) { $versionArguments += @("--build-id", $BuildId) }
& python @versionArguments
if ($LASTEXITCODE -ne 0) { throw "Installer payload version/source contract failed." }
# Reject stale binaries before invoking Inno, even if a manifest was relabelled.
. (Join-Path $PSScriptRoot "Windows-VersionContract.ps1")
foreach ($file in @($required[0], $required[1], "ReferenceTools/ChimeraRender.exe")) {
    Assert-ChimeraBinaryVersion (Join-Path $stagePath $file) $identity.product_version
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$outputPath = (Resolve-Path -LiteralPath $OutputDirectory).Path
$isccCommand = Get-Command ISCC.exe -ErrorAction SilentlyContinue
$iscc = if ($isccCommand) { $isccCommand.Source } else {
    Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6/ISCC.exe"
}
if (!(Test-Path -LiteralPath $iscc)) { throw "Inno Setup 6.3 or newer is required to build the installer." }
$script = Join-Path $PSScriptRoot "../Installer/Chimera.iss"
$compilerArguments = @("/DStageDir=$stagePath", "/DOutputPath=$outputPath",
    "/DProductVersion=$($identity.product_version)", "/DPackageVersion=$($identity.version)")
$outputName = "SpectralForge-Chimera-$($identity.version)-win64-Setup.exe"
if ($BuildId) {
    if ($BuildId -notmatch '^[0-9a-f]{10}$') { throw "Candidate BuildId must be the first ten lowercase source commit characters." }
    $candidateManifest = Get-Content -LiteralPath (Join-Path $stagePath "payload-manifest.json") -Raw | ConvertFrom-Json
    if (!$candidateManifest.source_sha.StartsWith($BuildId)) { throw "Candidate installer and payload source revisions differ." }
    $compilerArguments += "/DBuildId=$BuildId"
    $outputName = "SpectralForge-Chimera-update-$BuildId-win64-Setup.exe"
}
if ($Sign) {
    $signScript = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "Sign-WindowsArtifact.ps1")).Path
    $binaries = @(Get-ChildItem -LiteralPath $stagePath -File -Recurse | Where-Object { $_.Extension -in ".exe", ".dll", ".vst3" } | Select-Object -ExpandProperty FullName)
    & $signScript -Path $binaries -CertificateThumbprint $CertificateThumbprint -ExpectedPublisher $ExpectedPublisher
    # Signing changes bytes. Refresh the stage inventory after signatures, before
    # Inno embeds it, so installed documentation describes the actual payload.
    $payloadManifest = Join-Path $stagePath "payload-manifest.json"
    $payload = Get-Content -LiteralPath $payloadManifest -Raw | ConvertFrom-Json
    foreach ($entry in $payload.files) {
        $payloadFile = Join-Path $stagePath $entry.path
        $entry.bytes = (Get-Item -LiteralPath $payloadFile).Length
        $entry.sha256 = (Get-FileHash -LiteralPath $payloadFile -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $payload | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $payloadManifest -Encoding utf8
    # Pass only non-secret identity metadata to Inno's signing subprocess.
    $env:CHIMERA_SIGNING_THUMBPRINT = $CertificateThumbprint
    $env:CHIMERA_SIGNING_PUBLISHER = $ExpectedPublisher
    $pwsh = (Get-Process -Id $PID).Path
    $compilerArguments += "/DSignRelease"
    $compilerArguments += ('/SChimeraRelease=$q{0}$q -NoProfile -File $q{1}$q -ExpectedPublisher $q{2}$q -Path $f' -f $pwsh, $signScript, $ExpectedPublisher)
}
& $iscc @compilerArguments $script
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed with exit code $LASTEXITCODE" }
$installer = Join-Path $outputPath $outputName
if (!(Test-Path -LiteralPath $installer)) { throw "Installer compiler did not produce the expected Setup executable." }
Assert-ChimeraBinaryVersion $installer $identity.product_version
if ($Sign) {
    & $signScript -Path $installer -CertificateThumbprint $CertificateThumbprint -ExpectedPublisher $ExpectedPublisher -VerifyOnly
}
$hash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $([IO.Path]::GetFileName($installer))" | Set-Content -LiteralPath ($installer + ".sha256.txt")
Write-Host "Installer SHA256: $hash"
