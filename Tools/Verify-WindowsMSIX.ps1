param(
    [string]$Package = "dist/msix/Chimera-1.0.0.0-x64-review-unsigned.msix",
    [string]$OutputDirectory = "build/msix-verification"
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
# Install/remove tests are confined to a disposable CI runner, never a customer's PC.
if ($env:GITHUB_ACTIONS -ne "true") { throw "MSIX install verification must run in disposable GitHub Actions Windows CI." }
$packagePath = (Resolve-Path -LiteralPath $Package).Path
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$outputPath = (Resolve-Path -LiteralPath $OutputDirectory).Path
$report = Join-Path $outputPath "MSIXVerification.txt"
"MSIX verification; Windows $([Environment]::OSVersion.Version); Store certification is not performed by this test." | Set-Content -LiteralPath $report
function Pass([string]$Message) { "PASS: $Message" | Tee-Object -FilePath $report -Append | Write-Host }
$makeAppx = Get-ChildItem -Path "${env:ProgramFiles(x86)}/Windows Kits/10/bin/*/x64/makeappx.exe" |
    Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
$unpacked = Join-Path $outputPath ("payload-" + [guid]::NewGuid().ToString("N"))
$testName = "RavenForge.Chimera.CI" + $env:GITHUB_RUN_ID
$installed = $null
$launched = $null
$marker = $null
try {
    $expectedHash = ((Get-Content -LiteralPath ($packagePath + ".sha256.txt") -Raw).Trim() -split '\s+')[0]
    if ((Get-FileHash -LiteralPath $packagePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedHash) { throw "MSIX checksum mismatch." }
    & $makeAppx unpack /p $packagePath /d $unpacked
    if ($LASTEXITCODE -ne 0) { throw "MakeAppx could not unpack/validate the MSIX." }
    [xml]$manifest = Get-Content -LiteralPath "$unpacked/AppxManifest.xml" -Raw
    if ($manifest.Package.Properties.PublisherDisplayName -cne "RavenForge Luthier Intelligence" -or
        $manifest.Package.Identity.ProcessorArchitecture -ne "x64") { throw "MSIX publisher/architecture mismatch." }
    if ($manifest.Package.Applications.Application.GetAttribute("RuntimeBehavior", "http://schemas.microsoft.com/appx/manifest/uap/windows10/10") -ne "win32App") {
        throw "IR/user preferences must not be silently moved to virtualized AppData."
    }
    if (@(Get-ChildItem -LiteralPath $unpacked -Recurse -File | Where-Object { $_.Extension -in ".vst3", ".nam", ".wav" }).Count) {
        throw "Standalone MSIX must not imply global VST3 registration or bundle private captures."
    }
    $sourceExe = "dist/Chimera-Amp-Matrix-1.0.0-test-win64/Standalone/Chimera Amp Matrix.exe"
    if ((Get-FileHash -LiteralPath "$unpacked/App/Chimera Amp Matrix.exe").Hash -ne (Get-FileHash -LiteralPath $sourceExe).Hash) {
        throw "Packaged app differs from the DSP/UI-validated standalone."
    }
    Pass "SHA256, MakeAppx unpack, RavenForge identity, x64 architecture and exact validated application payload"
    Pass "Full-trust Win32 manifest; no virtualized VST3 install or private NAM/IR files"

    $blocked = $false
    try { & "$PSScriptRoot/Build-WindowsMSIX.ps1" -Mode Store }
    catch { if ($_.Exception.Message -notmatch "real Partner Center identity JSON") { throw }; $blocked = $true }
    if (!$blocked) { throw "Store packaging accepted an absent account identity." }
    $blocked = $false
    try { & "$PSScriptRoot/Build-WindowsMSIX.ps1" -Mode Signed -CertificateThumbprint "" }
    catch { if ($_.Exception.Message -notmatch "verified public code-signing certificate") { throw }; $blocked = $true }
    if (!$blocked) { throw "Direct distribution accepted an absent signing certificate." }
    Pass "Store and signed release modes reject missing identity instead of creating a misleading release"

    # Microsoft-documented Windows 11+ unsigned test identity. This changed package
    # is CI-only, is not uploaded, and cannot impersonate the release identity.
    if (Get-AppxPackage -Name $testName) { throw "An existing test package must not be overwritten." }
    $manifest.Package.Identity.SetAttribute("Name", $testName)
    $manifest.Package.Identity.SetAttribute("Publisher", "CN=RavenForge Luthier Intelligence, OID.2.25.311729368913984317654407730594956997722=1")
    $manifest.Save("$unpacked/AppxManifest.xml")
    foreach ($file in @("AppxBlockMap.xml", "[Content_Types].xml", "AppxSignature.p7x")) {
        $generated = Join-Path $unpacked $file
        if (Test-Path -LiteralPath $generated) { Remove-Item -LiteralPath $generated -Force }
    }
    $testPackage = Join-Path $outputPath "CI-only-unsigned.msix"
    & $makeAppx pack /d $unpacked /p $testPackage /h SHA256 /o
    if ($LASTEXITCODE -ne 0) { throw "CI-only package could not be created." }
    Add-AppxPackage -Path $testPackage -AllowUnsigned
    $installed = Get-AppxPackage -Name $testName
    if (!$installed -or $installed.Status -ne "Ok") { throw "MSIX deployment did not register a healthy package." }
    $aumid = $installed.PackageFamilyName + "!Chimera"
    $installedExe = Join-Path $installed.InstallLocation "App/Chimera Amp Matrix.exe"
    Start-Process explorer.exe -ArgumentList "shell:AppsFolder\$aumid" | Out-Null
    for ($attempt=0; $attempt -lt 60; ++$attempt) {
        $launched = Get-Process -Name "Chimera Amp Matrix" -ErrorAction SilentlyContinue |
            Where-Object { $_.Path -eq $installedExe -and $_.MainWindowHandle -ne 0 } | Select-Object -First 1
        if ($launched) { break }
        Start-Sleep -Milliseconds 500
    }
    if (!$launched -or $launched.MainWindowTitle -notlike "*Chimera*") { throw "Package activation did not open the actual Chimera window." }
    Pass "CI-isolated unsigned identity installs and launches the real application through its registered AppUserModelID"
    $launched.CloseMainWindow() | Out-Null
    if (!$launched.WaitForExit(5000)) { $launched.Kill(); $launched.WaitForExit() }
    $launched = $null

    $external = Join-Path ([Environment]::GetFolderPath("ApplicationData")) "SpectralForge/Chimera/IRs"
    New-Item -ItemType Directory -Path $external -Force | Out-Null
    $marker = Join-Path $external ("msix-preserve-" + [guid]::NewGuid().ToString("N") + ".txt")
    "external user IR library marker" | Set-Content -LiteralPath $marker
    Remove-AppxPackage -Package $installed.PackageFullName
    $installed = $null
    if (Get-AppxPackage -Name $testName) { throw "MSIX uninstall left the test registration behind." }
    if (!(Test-Path -LiteralPath $marker)) { throw "MSIX uninstall removed external user-library data." }
    Pass "Uninstall removes only the CI package registration and preserves the external user IR library"
} catch {
    "FAIL: $($_.Exception.Message)" | Add-Content -LiteralPath $report
    throw
} finally {
    if ($launched -and !$launched.HasExited) { $launched.Kill() }
    $leftover = Get-AppxPackage -Name $testName -ErrorAction SilentlyContinue
    if ($leftover) { Remove-AppxPackage -Package $leftover.PackageFullName }
    if ($marker -and (Test-Path -LiteralPath $marker)) { Remove-Item -LiteralPath $marker -Force }
    if (Test-Path -LiteralPath $unpacked) { Remove-Item -LiteralPath $unpacked -Recurse -Force }
    $testPackage = Join-Path $outputPath "CI-only-unsigned.msix"
    if (Test-Path -LiteralPath $testPackage) { Remove-Item -LiteralPath $testPackage -Force }
}
