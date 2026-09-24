param(
    [string]$Installer = "dist/Chimera-Amp-Matrix-1.0.0-test-win64-Setup.exe",
    [string]$Stage = "dist/Chimera-Amp-Matrix-1.0.0-test-win64",
    [string]$LogDirectory = "build/installer-verification"
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
# Installs/removes the product at its real Windows paths. Ephemeral CI only.
if ($env:GITHUB_ACTIONS -ne "true" -or !$IsWindows -or ![Environment]::Is64BitProcess) {
    throw "Installer verification is restricted to an ephemeral x64 Windows GitHub runner."
}
$installerPath = (Resolve-Path -LiteralPath $Installer).Path
$stagePath = (Resolve-Path -LiteralPath $Stage).Path
New-Item -ItemType Directory -Force -Path $LogDirectory | Out-Null
$logPath = (Resolve-Path -LiteralPath $LogDirectory).Path
$app = Join-Path ([Environment]::GetFolderPath("ProgramFiles")) "SpectralForge\Chimera Amp Matrix"
$vst = Join-Path ([Environment]::GetFolderPath("CommonProgramFiles")) "VST3\Chimera Amp Matrix.vst3"
$startMenu = Join-Path ([Environment]::GetFolderPath("CommonPrograms")) "SpectralForge\Chimera Amp Matrix"
$registry = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\SpectralForge.ChimeraAmpMatrix_is1"
foreach ($path in @($app, $vst, $startMenu, $registry)) {
    if (Test-Path -LiteralPath $path) { throw "Refusing to overwrite an existing installation: $path" }
}
$report = [Collections.Generic.List[string]]::new()
function Pass([string]$Message) {
    Write-Host "PASS: $Message"
    $report.Add("PASS: $Message")
}
function Assert([bool]$Condition, [string]$Message) {
    if (!$Condition) { throw $Message }
}
function Run-SetupProcess([string]$Executable, [string[]]$Parameters) {
    $process = Start-Process -FilePath $Executable -ArgumentList $Parameters -PassThru
    if (!$process.WaitForExit(90000)) {
        $process.Kill($true)
        throw "Installer process exceeded 90 seconds: $Executable"
    }
    Assert ($process.ExitCode -eq 0) "Installer process failed with exit code $($process.ExitCode)"
}
function Install([string]$Name, [string]$Destination, [string]$Components) {
    Run-SetupProcess $installerPath @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/SP-", "/LANG=korean",
        "/TYPE=custom", "/COMPONENTS=$Components", "/DIR=`"$Destination`"", "/LOG=`"$logPath/$Name.log`"")
}
function Equal-File([string]$Source, [string]$Destination) {
    Assert (Test-Path -LiteralPath $Destination -PathType Leaf) "Missing installed file: $Destination"
    Assert ((Get-FileHash -LiteralPath $Source).Hash -eq (Get-FileHash -LiteralPath $Destination).Hash) "Installed bytes differ: $Destination"
}
function Equal-Tree([string]$Source, [string]$Destination) {
    foreach ($file in Get-ChildItem -LiteralPath $Source -Recurse -File) {
        Equal-File $file.FullName (Join-Path $Destination ([IO.Path]::GetRelativePath($Source, $file.FullName)))
    }
}
function Check-Payload([string]$Destination, [bool]$Vst3, [bool]$Standalone, [bool]$Reference) {
    if ($Vst3) { Equal-Tree (Join-Path $stagePath "VST3/Chimera Amp Matrix.vst3") $vst }
    else { Assert (!(Test-Path -LiteralPath $vst)) "Unselected VST3 was installed" }
    $exe = Join-Path $Destination "Chimera Amp Matrix.exe"
    if ($Standalone) {
        Equal-File (Join-Path $stagePath "Standalone/Chimera Amp Matrix.exe") $exe
        $shortcut = Join-Path $startMenu "Chimera Amp Matrix.lnk"
        Assert (Test-Path -LiteralPath $shortcut) "Start Menu shortcut is missing"
        $shell = New-Object -ComObject WScript.Shell
        Assert ($shell.CreateShortcut($shortcut).TargetPath -eq $exe) "Start Menu shortcut points at the wrong executable"
    } else {
        Assert (!(Test-Path -LiteralPath $exe)) "Unselected standalone app was installed"
        Assert (!(Test-Path -LiteralPath (Join-Path $startMenu "Chimera Amp Matrix.lnk"))) "Unselected standalone shortcut was created"
    }
    foreach ($file in Get-ChildItem -LiteralPath $stagePath -File) {
        if ($file.Extension -eq ".txt") { Equal-File $file.FullName (Join-Path $Destination $file.Name) }
        if ($file.Extension -eq ".md") { Equal-File $file.FullName (Join-Path $Destination "Documentation/$($file.Name)") }
    }
    Equal-Tree (Join-Path $stagePath "reference") (Join-Path $Destination "Documentation/reference")
    if ($Reference) {
        Equal-Tree (Join-Path $stagePath "ReferenceTools") (Join-Path $Destination "ReferenceTools")
        Equal-Tree (Join-Path $stagePath "reference-audio") (Join-Path $Destination "reference-audio")
    } else { Assert (!(Test-Path -LiteralPath (Join-Path $Destination "ReferenceTools"))) "Unselected reference tools were installed" }
    Assert (Test-Path -LiteralPath $registry) "Windows uninstall entry is missing"
    $entry = Get-ItemProperty -LiteralPath $registry
    Assert ($entry.DisplayName -eq "Chimera Amp Matrix") "Wrong Windows app name"
    Assert ($entry.InstallLocation.TrimEnd([char]92) -eq $Destination.TrimEnd([char]92)) "Wrong registered install location"
}
function Uninstall([string]$Name, [string]$Destination) {
    $uninstaller = Join-Path $Destination "Uninstall/unins000.exe"
    Assert (Test-Path -LiteralPath $uninstaller) "Uninstaller is missing"
    Run-SetupProcess $uninstaller @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/LOG=`"$logPath/$Name.log`"")
    Assert (!(Test-Path -LiteralPath $registry)) "Uninstall entry remains"
    Assert (!(Test-Path -LiteralPath (Join-Path $Destination "Chimera Amp Matrix.exe"))) "Standalone app remains"
    Assert (!(Test-Path -LiteralPath (Join-Path $vst "Contents/x86_64-win/Chimera Amp Matrix.vst3"))) "VST3 binary remains"
    Assert (!(Test-Path -LiteralPath (Join-Path $startMenu "Chimera Amp Matrix.lnk"))) "App shortcut remains"
}
try {
    Install "01-full-install" $app "vst3,standalone,reference"
    Check-Payload $app $true $true $true
    Pass "Full install: standard VST3 path, app, documentation, shortcuts and uninstall registration; payload hashes match"

    # Inspect the imports of all shipped native binaries, not just this runner's installed runtimes.
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/Installer/vswhere.exe"
    $vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    Assert ($LASTEXITCODE -eq 0 -and ![string]::IsNullOrWhiteSpace($vsRoot)) "Visual C++ tools were not located"
    $toolset = Get-ChildItem -LiteralPath (Join-Path $vsRoot "VC/Tools/MSVC") -Directory | Sort-Object Name -Descending | Select-Object -First 1
    $dumpbin = Join-Path $toolset.FullName "bin/Hostx64/x64/dumpbin.exe"
    $vstBinary = Join-Path $vst "Contents/x86_64-win/Chimera Amp Matrix.vst3"
    foreach ($binary in @((Join-Path $app "Chimera Amp Matrix.exe"), (Join-Path $app "ReferenceTools/ChimeraRender.exe"), $vstBinary)) {
        $imports = & $dumpbin /DEPENDENTS $binary
        Assert ($LASTEXITCODE -eq 0) "Cannot inspect dependencies: $binary"
        $imports | Add-Content -LiteralPath (Join-Path $logPath "runtime-dependencies.txt")
        $externalCrt = $imports -match '(?i)^\s*(?:(?:MSVCP|VCRUNTIME|CONCRT)\d[^\s]*|ucrtbase|api-ms-win-crt-[^\s]*)\.dll\s*$'
        Assert (!$externalCrt) "External Visual C++ runtime dependency: $binary"
    }
    Pass "Standalone, VST3 and reference renderer use the static MSVC runtime"

    $module = [Runtime.InteropServices.NativeLibrary]::Load($vstBinary)
    try { Assert ([Runtime.InteropServices.NativeLibrary]::GetExport($module, "GetPluginFactory") -ne [IntPtr]::Zero) "VST3 factory export is missing" }
    finally { [Runtime.InteropServices.NativeLibrary]::Free($module) }
    Pass "Installed VST3 loads through the Windows loader and exports GetPluginFactory"

    $application = Start-Process -FilePath (Join-Path $app "Chimera Amp Matrix.exe") -WorkingDirectory $app -PassThru
    try {
        for ($attempt = 0; $attempt -lt 60; ++$attempt) {
            $application.Refresh()
            if ($application.HasExited -or $application.MainWindowHandle -ne [IntPtr]::Zero) { break }
            Start-Sleep -Milliseconds 250
        }
        Assert (!$application.HasExited -and $application.MainWindowHandle -ne [IntPtr]::Zero) "Installed standalone app did not open its window"
        Pass "Installed standalone app starts and creates its main window"
    } finally {
        if (!$application.HasExited) {
            $null = $application.CloseMainWindow()
            if (!$application.WaitForExit(10000)) { $application.Kill($true); $application.WaitForExit() }
        }
    }

    # A repeated same-version install must repair package files without claiming user files.
    $userFolder = Join-Path $app "UserFiles"
    New-Item -ItemType Directory -Path $userFolder | Out-Null
    $preset = Join-Path $userFolder "연주 참고.chimera"
    $userIR = Join-Path $userFolder "personal-cab.wav"
    "user-owned preset fixture" | Set-Content -LiteralPath $preset
    "user-owned IR fixture" | Set-Content -LiteralPath $userIR
    $presetHash = (Get-FileHash -LiteralPath $preset).Hash
    $irHash = (Get-FileHash -LiteralPath $userIR).Hash
    Remove-Item -LiteralPath (Join-Path $app "ReferenceTools/ChimeraRender.exe")
    "damaged documentation fixture" | Set-Content -LiteralPath (Join-Path $app "README.txt")
    Install "02-repair" $app "vst3,standalone,reference"
    Check-Payload $app $true $true $true
    Assert ((Get-FileHash -LiteralPath $preset).Hash -eq $presetHash -and (Get-FileHash -LiteralPath $userIR).Hash -eq $irHash) "Repair modified user files"
    Pass "Same-version reinstall repairs files and preserves personal presets/IRs"
    Uninstall "03-full-uninstall" $app
    Assert ((Get-FileHash -LiteralPath $preset).Hash -eq $presetHash -and (Get-FileHash -LiteralPath $userIR).Hash -eq $irHash) "Uninstall modified user files"
    Pass "Uninstall removes owned binaries, shortcut and Windows registration; preserves user files"

    $custom = Join-Path $env:RUNNER_TEMP "Chimera 설치 검증"
    Assert (!(Test-Path -LiteralPath $custom)) "Custom test path already exists"
    Install "04-vst3-only" $custom "vst3"
    Check-Payload $custom $true $false $false
    Uninstall "05-vst3-uninstall" $custom
    Pass "VST3-only selection and removal; Unicode custom app path"
    Install "06-standalone-only" $custom "standalone"
    Check-Payload $custom $false $true $false
    Uninstall "07-standalone-uninstall" $custom
    Pass "Standalone-only selection and removal; no VST3 installed"
} catch {
    $report.Add("FAIL: $($_.Exception.Message)")
    throw
} finally {
    $report | Set-Content -LiteralPath (Join-Path $logPath "InstallerVerification.txt")
}
