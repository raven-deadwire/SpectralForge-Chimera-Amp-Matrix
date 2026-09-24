param(
    [string]$Stage = "dist/Chimera-Amp-Matrix-1.0.0-test-win64",
    [string]$OutputDirectory = "dist"
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$stagePath = (Resolve-Path -LiteralPath $Stage).Path
$required = @(
    "Standalone/Chimera Amp Matrix.exe",
    "VST3/Chimera Amp Matrix.vst3/Contents/x86_64-win/Chimera Amp Matrix.vst3",
    "ReferenceTools/ChimeraRender.exe", "WINDOWS_INSTALL.txt", "Verification.txt"
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
& $iscc "/DStageDir=$stagePath" "/DOutputPath=$outputPath" $script
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed with exit code $LASTEXITCODE" }
$installer = Join-Path $outputPath "Chimera-Amp-Matrix-1.0.0-test-win64-Setup.exe"
if (!(Test-Path -LiteralPath $installer)) { throw "Installer compiler did not produce the expected Setup executable." }
$hash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $([IO.Path]::GetFileName($installer))" | Set-Content -LiteralPath ($installer + ".sha256.txt")
Write-Host "Installer SHA256: $hash"
