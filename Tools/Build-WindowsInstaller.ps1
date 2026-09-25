param(
    [string]$Stage = "dist/SpectralForge-Chimera-1.0.0-beta.1-win64",
    [string]$OutputDirectory = "dist",
    [switch]$Sign,
    [string]$CertificateThumbprint = $env:CHIMERA_SIGNING_THUMBPRINT,
    [string]$ExpectedPublisher = "RavenForge Luthier Intelligence"
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
if ($Sign -and $CertificateThumbprint -notmatch '^[0-9A-Fa-f]{40}$') {
    throw "A verified public code-signing certificate is required for a signed release."
}
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
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$outputPath = (Resolve-Path -LiteralPath $OutputDirectory).Path
$isccCommand = Get-Command ISCC.exe -ErrorAction SilentlyContinue
$iscc = if ($isccCommand) { $isccCommand.Source } else {
    Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6/ISCC.exe"
}
if (!(Test-Path -LiteralPath $iscc)) { throw "Inno Setup 6.3 or newer is required to build the installer." }
$script = Join-Path $PSScriptRoot "../Installer/Chimera.iss"
$compilerArguments = @("/DStageDir=$stagePath", "/DOutputPath=$outputPath")
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
$installer = Join-Path $outputPath "SpectralForge-Chimera-1.0.0-beta.1-win64-Setup.exe"
if (!(Test-Path -LiteralPath $installer)) { throw "Installer compiler did not produce the expected Setup executable." }
if ($Sign) {
    & $signScript -Path $installer -CertificateThumbprint $CertificateThumbprint -ExpectedPublisher $ExpectedPublisher -VerifyOnly
}
$hash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $([IO.Path]::GetFileName($installer))" | Set-Content -LiteralPath ($installer + ".sha256.txt")
Write-Host "Installer SHA256: $hash"
