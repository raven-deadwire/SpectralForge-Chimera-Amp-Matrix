param(
    [string]$Installer = "",
    [string]$Stage = "",
    [string]$LogDirectory = "build/installer-verification"
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
# Installs/removes the product at its real Windows paths. Ephemeral CI only.
if ($env:GITHUB_ACTIONS -ne "true" -or !$IsWindows -or ![Environment]::Is64BitProcess) {
    throw "Installer verification is restricted to an ephemeral x64 Windows GitHub runner."
}
$versionTool = Join-Path $PSScriptRoot "chimera_version.py"
$identityJson = & python $versionTool
if ($LASTEXITCODE -ne 0) { throw "Cannot resolve the source product version." }
$identity = $identityJson | ConvertFrom-Json
if (!$Stage) { $Stage = "dist/SpectralForge-Chimera-$($identity.version)-win64" }
if (!$Installer) { $Installer = "dist/SpectralForge-Chimera-$($identity.version)-win64-Setup.exe" }
. (Join-Path $PSScriptRoot "Windows-VersionContract.ps1")
$installerPath = (Resolve-Path -LiteralPath $Installer).Path
$stagePath = (Resolve-Path -LiteralPath $Stage).Path
New-Item -ItemType Directory -Force -Path $LogDirectory | Out-Null
$logPath = (Resolve-Path -LiteralPath $LogDirectory).Path
$app = Join-Path ([Environment]::GetFolderPath("ProgramFiles")) "SpectralForge\Chimera Amp Matrix"
$vst = Join-Path ([Environment]::GetFolderPath("CommonProgramFiles")) "VST3\SpectralForge Chimera.vst3"
$startMenu = Join-Path ([Environment]::GetFolderPath("CommonPrograms")) "SpectralForge\SpectralForge Chimera"
$sharedIRs = Join-Path ([Environment]::GetFolderPath("CommonApplicationData")) "SpectralForge/Chimera/IRs"
$companion = Join-Path (Split-Path -Parent $installerPath) "Chimera-Personal-IRs"
$registry = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\SpectralForge.ChimeraAmpMatrix_is1"
# Optional OBS Studio filter task (GPL module, Issue #131) and its install path.
$obsPlugin = Join-Path ([Environment]::GetFolderPath("CommonApplicationData")) "obs-studio\plugins\chimera-obs"
# OBS user data and another plugin that Setup must never touch (stage E): a
# scene collection holding a Chimera filter state, and a third-party plugin.
$obsScene = Join-Path ([Environment]::GetFolderPath("ApplicationData")) "obs-studio\basic\scenes\Chimera Setup verification.json"
$otherObsPlugin = Join-Path ([Environment]::GetFolderPath("CommonApplicationData")) "obs-studio\plugins\chimera-setup-verification-other"
$obsUserFixtures = @{}
$chimeraUserPaths = @(
    (Join-Path ([Environment]::GetFolderPath("ApplicationData")) "SpectralForge/Chimera/ir-library.json"),
    (Join-Path ([Environment]::GetFolderPath("ApplicationData")) "SpectralForge Chimera/SpectralForge Chimera.settings"),
    (Join-Path ([Environment]::GetFolderPath("ApplicationData")) "SpectralForge/Chimera/Presets/CI-preserve.chimera")
)
foreach ($path in $chimeraUserPaths) {
    if (Test-Path -LiteralPath $path) { throw "Refusing to overwrite existing user settings: $path" }
}
foreach ($path in @($app, $vst, $startMenu, $registry, $sharedIRs, $companion, $obsPlugin, $obsScene, $otherObsPlugin)) {
    if (Test-Path -LiteralPath $path) { throw "Refusing to overwrite an existing installation: $path" }
}
$report = [Collections.Generic.List[string]]::new()
$success = $false
$actualPreviousReleaseUpgradeVerified = $false
$observedVersions = [Collections.Generic.List[string]]::new()
$payload = Get-Content -LiteralPath (Join-Path $stagePath "payload-manifest.json") -Raw | ConvertFrom-Json
$obsStage = Join-Path $stagePath "OBS/chimera-obs"
$obsOffered = Test-Path -LiteralPath (Join-Path $obsStage "bin/64bit/chimera-obs.dll")
$obsUserFiles = @{}
$sourceSha = if ($payload.PSObject.Properties.Name -contains "source_sha") { $payload.source_sha } else { (& git rev-parse HEAD).Trim() }
function Pass([string]$Message) {
    Write-Host "PASS: $Message"
    $report.Add("PASS: $Message")
}
function Assert([bool]$Condition, [string]$Message) {
    if (!$Condition) { throw $Message }
}
function Run-SetupProcess([string]$Executable, [string[]]$Parameters, [int]$ExpectedExitCode = 0) {
    # Inno's uninstaller spawns a second phase. Wait for the whole process tree,
    # not just the launcher, before another install can reuse the same directory.
    # The real app launch can legitimately save settings. Compare immediately
    # around each installer operation so only installer mutations are judged.
    $before = @{}
    foreach ($path in $chimeraUserPaths) { $before[$path] = (Get-FileHash -LiteralPath $path).Hash }
    $process = Start-Process -FilePath $Executable -ArgumentList $Parameters -PassThru -Wait
    Assert ($process.ExitCode -eq $ExpectedExitCode) "Installer process exited with $($process.ExitCode); expected $ExpectedExitCode"
    foreach ($path in $chimeraUserPaths) {
        Assert ((Test-Path -LiteralPath $path) -and (Get-FileHash -LiteralPath $path).Hash -ceq $before[$path]) "Installer changed user settings/preset: $path"
    }
}
function Install([string]$Name, [string]$Destination, [string]$Components, [string]$Vst3Directory = "", [string]$ObsTask = "",
                 [int]$ExpectedExitCode = 0) {
    $parameters = @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/SP-", "/LANG=korean",
        "/TYPE=custom", "/COMPONENTS=$Components", "/DIR=`"$Destination`"", "/LOG=`"$logPath/$Name.log`"")
    # "obsfilter" selects and "!obsfilter" deselects; empty keeps the previous answer.
    if ($ObsTask -and $obsOffered) { $parameters += "/MERGETASKS=`"$ObsTask`"" }
    if ($Vst3Directory) {
        $parameters += "/VST3DIR=`"$Vst3Directory`""
        $script:vst = Join-Path $Vst3Directory "SpectralForge Chimera.vst3"
    }
    Run-SetupProcess $installerPath $parameters $ExpectedExitCode
}
function Tree-Digest([string]$Path) {
    if (!(Test-Path -LiteralPath $Path)) { return "" }
    return (@(Get-ChildItem -LiteralPath $Path -Recurse -File | Sort-Object FullName |
        ForEach-Object { "$([IO.Path]::GetRelativePath($Path, $_.FullName))=$((Get-FileHash -LiteralPath $_.FullName).Hash)" }) -join "`n")
}
function Check-ObsUserData {
    foreach ($item in $obsUserFixtures.GetEnumerator()) {
        Assert ((Tree-Digest $item.Key) -eq $item.Value -and (Tree-Digest $item.Key) -ne "") "Setup changed OBS user data or another plugin: $($item.Key)"
    }
}
function Start-ObsProcessFixture {
    # Name-only fixture, not the OBS application or an audio/GUI test.
    $directory = Join-Path $env:RUNNER_TEMP "chimera-setup-running-obs"
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $executable = Join-Path $directory "obs64.exe"
    Copy-Item -LiteralPath (Join-Path $env:SystemRoot "System32\PING.EXE") -Destination $executable -Force
    $process = Start-Process -FilePath $executable -ArgumentList @("-n", "600", "127.0.0.1") -WindowStyle Hidden -PassThru
    Assert (!$process.HasExited) "OBS process fixture exited before the guard test"
    return $process
}
function Install-Snapshot([string]$Destination) {
    $entry = if (Test-Path -LiteralPath $registry) {
        (Get-ItemProperty -LiteralPath $registry | Select-Object * -ExcludeProperty PS* | ConvertTo-Json -Compress)
    } else { "absent" }
    return (@((Tree-Digest $Destination), (Tree-Digest $vst), (Tree-Digest $obsPlugin),
        (Tree-Digest $startMenu), (Tree-Digest $sharedIRs), $entry) -join "`n---`n")
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
function Check-ObsFilter([bool]$Installed) {
    foreach ($path in $obsUserFiles.Keys) {
        Assert (Test-Path -LiteralPath $path -PathType Leaf) "OBS user file was removed: $path"
        Assert ((Get-FileHash -LiteralPath $path).Hash -eq $obsUserFiles[$path]) "OBS user file changed: $path"
    }
    Check-ObsUserData
    if (!$Installed) {
        # Setup owns the staged files and generated path hint, not the entire
        # directory: user presets/IRs/configuration may keep it non-empty.
        if ($obsOffered) {
            foreach ($file in Get-ChildItem -LiteralPath $obsStage -Recurse -File) {
                $installed = Join-Path $obsPlugin ([IO.Path]::GetRelativePath($obsStage, $file.FullName))
                Assert (!(Test-Path -LiteralPath $installed)) "Unselected OBS payload remains: $installed"
            }
        }
        Assert (!(Test-Path -LiteralPath (Join-Path $obsPlugin "data/default-vst3-path.txt"))) "Unselected OBS path hint remains"
        return
    }
    Equal-Tree $obsStage $obsPlugin
    # The filter finds the selected (possibly custom, Unicode) VST3 folder through this file.
    $hintPath = Join-Path $obsPlugin "data/default-vst3-path.txt"
    Assert (Test-Path -LiteralPath $hintPath -PathType Leaf) "OBS filter VST3 path hint is missing"
    $hint = [Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes($hintPath)).TrimStart([char]0xFEFF).TrimEnd()
    Assert ([string]::Equals($hint.TrimEnd([char]92), $vst.TrimEnd([char]92), [StringComparison]::OrdinalIgnoreCase)) "OBS filter VST3 hint '$hint'; expected '$vst'"
}
function Check-Payload([string]$Destination, [bool]$Vst3, [bool]$Standalone, [bool]$Reference, [bool]$Obs = $false) {
    Check-ObsFilter $Obs
    if ($Vst3) { Equal-Tree (Join-Path $stagePath "VST3/SpectralForge Chimera.vst3") $vst }
    else { Assert (!(Test-Path -LiteralPath $vst)) "Unselected VST3 was installed" }
    $exe = Join-Path $Destination "SpectralForge Chimera.exe"
    if ($Standalone) {
        Equal-File (Join-Path $stagePath "Standalone/SpectralForge Chimera.exe") $exe
        $shortcut = Join-Path $startMenu "SpectralForge Chimera.lnk"
        Assert (Test-Path -LiteralPath $shortcut) "Start Menu shortcut is missing"
        # Keep launchable shortcuts on the disposable test machine. The report
        # records the real launch result below; it must not redistribute .lnk files.
    } else {
        Assert (!(Test-Path -LiteralPath $exe)) "Unselected standalone app was installed"
        Assert (!(Test-Path -LiteralPath (Join-Path $startMenu "SpectralForge Chimera.lnk"))) "Unselected standalone shortcut was created"
    }
    foreach ($file in Get-ChildItem -LiteralPath $stagePath -File) {
        if ($file.Extension -eq ".txt") { Equal-File $file.FullName (Join-Path $Destination $file.Name) }
        if ($file.Extension -in ".md", ".html") { Equal-File $file.FullName (Join-Path $Destination "Documentation/$($file.Name)") }
    }
    Equal-File (Join-Path $stagePath "payload-manifest.json") (Join-Path $Destination "Documentation/payload-manifest.json")
    if (Test-Path -LiteralPath (Join-Path $stagePath "chimera-beta-lifecycle.json")) {
        Equal-File (Join-Path $stagePath "chimera-beta-lifecycle.json") (Join-Path $Destination "Documentation/chimera-beta-lifecycle.json")
        Equal-File (Join-Path $stagePath "BETA_LIFECYCLE.txt") (Join-Path $Destination "BETA_LIFECYCLE.txt")
    }
    Assert (Test-Path -LiteralPath (Join-Path $startMenu "Manual.lnk")) "Offline manual shortcut is missing"
    # Public packages exclude research documents and reference audio. These are
    # payload checks only; user-imported IR locations remain outside this scope.
    foreach ($excluded in @(
        @{ Stage = "reference"; Installed = "Documentation/reference" },
        @{ Stage = "reference-audio"; Installed = "reference-audio" }
    )) {
        Assert (!(Test-Path -LiteralPath (Join-Path $stagePath $excluded.Stage))) "Excluded research folder exists in public staging: $($excluded.Stage)"
        Assert (!(Test-Path -LiteralPath (Join-Path $Destination $excluded.Installed))) "Excluded research folder was installed: $($excluded.Installed)"
    }
    if ($Reference) {
        Equal-Tree (Join-Path $stagePath "ReferenceTools") (Join-Path $Destination "ReferenceTools")
    } else { Assert (!(Test-Path -LiteralPath (Join-Path $Destination "ReferenceTools"))) "Unselected reference tools were installed" }
    Assert (Test-Path -LiteralPath $registry) "Windows uninstall entry is missing"
    $entry = Get-ItemProperty -LiteralPath $registry
    Assert ($entry.DisplayName -eq "SpectralForge Chimera") "Wrong Windows app name"
    Assert ($entry.DisplayVersion -ceq $payload.installer_version) "Candidate installer version differs from its payload: registry=$($entry.DisplayVersion), payload=$($payload.installer_version)"
    if ($Standalone) { Assert-ChimeraBinaryVersion $exe $payload.product_version }
    if ($Vst3) { Assert-ChimeraBinaryVersion (Join-Path $vst "Contents/x86_64-win/SpectralForge Chimera.vst3") $payload.product_version }
    if ($Reference) { Assert-ChimeraBinaryVersion (Join-Path $Destination "ReferenceTools/ChimeraRender.exe") $payload.product_version }
    $observedVersions.Add($entry.DisplayVersion)
    Pass "Installed DisplayVersion=$($entry.DisplayVersion) and selected binary versions match $($payload.version)"
    Assert ($entry.InstallLocation.TrimEnd([char]92) -eq $Destination.TrimEnd([char]92)) "Wrong registered install location"
}
function Uninstall([string]$Name, [string]$Destination) {
    # The actual name is assigned by Setup and can be unins001.exe, etc.
    $uninstaller = (Get-ItemProperty -LiteralPath $registry).UninstallString.Trim('"')
    Assert ($uninstaller.StartsWith((Join-Path $Destination "Uninstall"), [StringComparison]::OrdinalIgnoreCase)) "Unexpected uninstaller location"
    Assert (Test-Path -LiteralPath $uninstaller) "Uninstaller is missing"
    Run-SetupProcess $uninstaller @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/LOG=`"$logPath/$Name.log`"")
    Assert (!(Test-Path -LiteralPath $registry)) "Uninstall entry remains"
    Assert (!(Test-Path -LiteralPath (Join-Path $Destination "SpectralForge Chimera.exe"))) "Standalone app remains"
    Assert (!(Test-Path -LiteralPath (Join-Path $vst "Contents/x86_64-win/SpectralForge Chimera.vst3"))) "VST3 binary remains"
    Assert (!(Test-Path -LiteralPath (Join-Path $startMenu "SpectralForge Chimera.lnk"))) "App shortcut remains"
    Check-ObsFilter $false
}
Add-Type @'
using System.Runtime.InteropServices;
using System.Text;
public static class ChimeraInstallProbe {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern uint GetLongPathName(string path, StringBuilder result, uint length);
}
'@
function Long-Path([string]$Path) {
    $buffer = [Text.StringBuilder]::new(32768)
    $length = [ChimeraInstallProbe]::GetLongPathName($Path, $buffer, $buffer.Capacity)
    Assert ($length -gt 0 -and $length -lt $buffer.Capacity) "Cannot resolve installed path: $Path"
    return $buffer.ToString()
}
function Check-AppShortcut([string]$Destination) {
    # Test the user's actual launch path and compare the launched process's
    # canonical filename, including Unicode and 8.3 path handling.
    $shortcut = Join-Path $startMenu "SpectralForge Chimera.lnk"
    $application = Start-Process -FilePath $shortcut -PassThru
    Assert ($null -ne $application) "Shortcut did not start an application process"
    try {
        for ($attempt = 0; $attempt -lt 60; ++$attempt) {
            $application.Refresh()
            if ($application.HasExited -or $application.MainWindowHandle -ne [IntPtr]::Zero) { break }
            Start-Sleep -Milliseconds 250
        }
        Assert (!$application.HasExited -and $application.MainWindowHandle -ne [IntPtr]::Zero) "Shortcut did not open the installed application window"
        $actual = Long-Path $application.MainModule.FileName
        $expected = Long-Path (Join-Path $Destination "SpectralForge Chimera.exe")
        Assert ($actual -eq $expected) "Shortcut launched '$actual'; expected '$expected'"
        Pass "Start Menu shortcut launches the installed app and its window: $actual"
    } finally {
        if (!$application.HasExited) {
            $null = $application.CloseMainWindow()
            if (!$application.WaitForExit(10000)) { $application.Kill($true); $application.WaitForExit() }
        }
    }
}
try {
    foreach ($path in $chimeraUserPaths) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $path) | Out-Null
        $data = if ($path.EndsWith('.settings')) { '<PROPERTIES><VALUE name="installer-preserve" val="fixture"/></PROPERTIES>' }
                elseif ($path.EndsWith('.json')) { '{"version":1,"files":[],"folders":[]}' }
                else { 'User-owned preset fixture' }
        [IO.File]::WriteAllText($path, $data, [Text.UTF8Encoding]::new($false))
    }
    & python $versionTool --manifest (Join-Path $stagePath "payload-manifest.json")
    Assert ($LASTEXITCODE -eq 0) "Installer payload version/source contract failed"
    Assert-ChimeraBinaryVersion $installerPath $payload.product_version
    Pass "Setup version resources match source/payload product version $($payload.product_version)"
    # Simulate the owned binaries from the previous product name. Keep unrelated
    # personal files alongside them to exercise the exact legacy migration scope.
    $legacyExe = Join-Path $app "Chimera Amp Matrix.exe"
    $legacyVst = Join-Path ([Environment]::GetFolderPath("CommonProgramFiles")) "VST3/Chimera Amp Matrix.vst3/Contents/x86_64-win/Chimera Amp Matrix.vst3"
    Assert (!(Test-Path -LiteralPath $legacyVst)) "Refusing to overwrite an existing legacy plugin"
    New-Item -ItemType Directory -Force -Path $app, (Split-Path -Parent $legacyVst) | Out-Null
    Copy-Item -LiteralPath (Join-Path $stagePath "Standalone/SpectralForge Chimera.exe") -Destination $legacyExe
    Copy-Item -LiteralPath (Join-Path $stagePath "VST3/SpectralForge Chimera.vst3/Contents/x86_64-win/SpectralForge Chimera.vst3") -Destination $legacyVst
    $legacyPreset = Join-Path $app "personal-upgrade-preserve.chimera"
    "user-owned legacy preset fixture" | Set-Content -LiteralPath $legacyPreset
    $legacyPresetHash = (Get-FileHash -LiteralPath $legacyPreset).Hash
    New-Item -ItemType Directory -Path "$companion/Bass" -Force | Out-Null
    New-Item -ItemType Directory -Path "$companion/Guitar" -Force | Out-Null
    $fixture = Join-Path $companion "Bass/CI Bass 8x10 MD421.wav"
    Copy-Item -LiteralPath "Assets/IRs/guitar_v30_sm57.wav" -Destination $fixture
    '{"cabinet":"CI synthetic bass metadata","diameter_in":"10","microphone":"MD421"}' | Set-Content -LiteralPath ($fixture + ".json")
    $guitarFixture = Join-Path $companion "Guitar/CI Guitar 4x12 SM57.wav"
    Copy-Item -LiteralPath "Assets/IRs/guitar_jensen_sm57.wav" -Destination $guitarFixture
    '{"cabinet":"CI synthetic guitar metadata","diameter_in":"12","microphone":"SM57"}' | Set-Content -LiteralPath ($guitarFixture + ".json")
    # Synthetic fixtures exercise a private four-file Raven companion without
    # sending the user's original IR recordings to public CI.
    $ravenFixtures = @()
    New-Item -ItemType Directory -Path "$companion/Raven" -Force | Out-Null
    foreach ($label in @("In", "Out", "Ref", "V30 Comparison")) {
        $file = Join-Path $companion "Raven/CI Raven $label.wav"
        Copy-Item -LiteralPath "Assets/IRs/guitar_v30_sm57.wav" -Destination $file
        @{ cabinet = "CI synthetic Raven $label"; microphone = "SM57"; collection = "Raven" } |
            ConvertTo-Json | Set-Content -LiteralPath ($file + ".json")
        $ravenFixtures += $file
    }
    if ($obsOffered) {
        $sceneDirectory = Split-Path -Parent $obsScene
        New-Item -ItemType Directory -Force -Path $sceneDirectory, (Join-Path $otherObsPlugin "bin\64bit") | Out-Null
        # A scene saved by the filter: its Chimera state lives in OBS's scene file, not in Setup's folders.
        '{"name":"Chimera Setup verification","sources":[{"id":"spectralforge_chimera_filter","settings":{"chimera_obs_schema":1,"vst3_component_state":"Q0k="}}]}' |
            Set-Content -LiteralPath $obsScene -Encoding utf8
        "third-party OBS plugin fixture" | Set-Content -LiteralPath (Join-Path $otherObsPlugin "bin\64bit\other-plugin.dll")
        $obsUserFixtures[$sceneDirectory] = Tree-Digest $sceneDirectory
        $obsUserFixtures[$otherObsPlugin] = Tree-Digest $otherObsPlugin
    }
    if ($obsOffered) {
        $runningObs = Start-ObsProcessFixture
        try {
            $before = Install-Snapshot $app
            Install "00-obs-running-fresh" $app "vst3,standalone,reference" -ObsTask "obsfilter" -ExpectedExitCode 7
            Assert ((Install-Snapshot $app) -ceq $before) "Blocked fresh install changed files or registration"
            Check-ObsUserData
            Pass "With OBS running, first-time selected install exits 7 before changing files or registration"
        } finally {
            if (!$runningObs.HasExited) { $runningObs.Kill(); $runningObs.WaitForExit() }
        }
    }
    Install "00-full-without-obs-task" $app "vst3,standalone,reference"
    Check-Payload $app $true $true $true $false
    Pass "Fresh silent install does not opt into the OBS filter without a task selection"
    Install "01-full-install" $app "vst3,standalone,reference" -ObsTask "obsfilter"
    Check-Payload $app $true $true $true $obsOffered
    Assert (!(Test-Path -LiteralPath $legacyExe) -and !(Test-Path -LiteralPath $legacyVst)) "Legacy product binaries were not retired"
    Assert ((Get-FileHash -LiteralPath $legacyPreset).Hash -eq $legacyPresetHash) "Product rename changed a personal preset"
    Pass "Product rename retires only exact old executable/VST3 binaries and preserves personal preset bytes"
    $installedIR = Join-Path $sharedIRs "Bass/CI Bass 8x10 MD421.wav"
    Equal-File $fixture $installedIR
    Equal-File ($fixture + ".json") ($installedIR + ".json")
    $installedGuitarIR = Join-Path $sharedIRs "Guitar/CI Guitar 4x12 SM57.wav"
    Equal-File $guitarFixture $installedGuitarIR
    Equal-File ($guitarFixture + ".json") ($installedGuitarIR + ".json")
    $installedRaven = @()
    foreach ($fixtureFile in $ravenFixtures) {
        $installed = Join-Path $sharedIRs ("Raven/" + [IO.Path]::GetFileName($fixtureFile))
        Equal-File $fixtureFile $installed
        Equal-File ($fixtureFile + ".json") ($installed + ".json")
        $installedRaven += $installed
    }
    foreach ($installedCapture in @($installedIR, $installedGuitarIR) + $installedRaven) {
        & "build/ChimeraUITests_artefacts/Release/ChimeraUITests.exe" --installed-ir-probe $installedCapture
        Assert ($LASTEXITCODE -eq 0) "Application failed to discover, decode or select installed IR: $installedCapture"
    }
    Pass "Companion Bass/Guitar WAVs install recursively, match their source hashes, decode, appear in the cabinet menu and load into a rig"
    Pass "Four Raven companion fixtures and sidecars install automatically, decode, appear in CAB and load into a rig"
    Pass "Full install: standard VST3 path, app, documentation, shortcuts and uninstall registration; payload hashes match"
    if ($obsOffered) { Pass "Selected OBS filter task installs the staged module under ProgramData\obs-studio\plugins and records the VST3 path" }

    # Inspect the imports of all shipped native binaries, not just this runner's installed runtimes.
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/Installer/vswhere.exe"
    $vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    Assert ($LASTEXITCODE -eq 0 -and ![string]::IsNullOrWhiteSpace($vsRoot)) "Visual C++ tools were not located"
    $toolset = Get-ChildItem -LiteralPath (Join-Path $vsRoot "VC/Tools/MSVC") -Directory | Sort-Object Name -Descending | Select-Object -First 1
    $dumpbin = Join-Path $toolset.FullName "bin/Hostx64/x64/dumpbin.exe"
    $vstBinary = Join-Path $vst "Contents/x86_64-win/SpectralForge Chimera.vst3"
    $obsBinary = Join-Path $obsPlugin "bin/64bit/chimera-obs.dll"
    $nativeBinaries = @((Join-Path $app "SpectralForge Chimera.exe"), (Join-Path $app "ReferenceTools/ChimeraRender.exe"), $vstBinary)
    if ($obsOffered) { $nativeBinaries += $obsBinary }
    foreach ($binary in $nativeBinaries) {
        $imports = & $dumpbin /DEPENDENTS $binary
        Assert ($LASTEXITCODE -eq 0) "Cannot inspect dependencies: $binary"
        $imports | Add-Content -LiteralPath (Join-Path $logPath "runtime-dependencies.txt")
        $externalCrt = $imports -match '(?i)^\s*(?:(?:MSVCP|VCRUNTIME|CONCRT)\d[^\s]*|ucrtbase|api-ms-win-crt-[^\s]*)\.dll\s*$'
        Assert (!$externalCrt) "External Visual C++ runtime dependency: $binary"
    }
    Pass "Standalone, VST3 and reference renderer use the static MSVC runtime"
    if ($obsOffered) {
        # obs.dll is OBS's own and only present inside OBS, so inspect the export table instead of loading it.
        $exports = & $dumpbin /EXPORTS $obsBinary
        Assert ($LASTEXITCODE -eq 0) "Cannot inspect exports: $obsBinary"
        Assert ([bool]($exports -match '\sobs_module_load\s*$')) "OBS filter does not export obs_module_load"
        Pass "OBS filter uses the static MSVC runtime and exports obs_module_load"
        # The version users and bug reports see matches the staged module (SOURCE.txt).
        $moduleLine = Select-String -LiteralPath (Join-Path $obsStage "SOURCE.txt") -Pattern '^Module version: (\d+\.\d+\.\d+)$'
        Assert ($null -ne $moduleLine) "SOURCE.txt does not state the module version"
        $moduleVersion = $moduleLine.Matches[0].Groups[1].Value
        $expectedModuleProductVersion = "$moduleVersion ($($sourceSha.Substring(0, 10)))"
        $sourceText = Get-Content -LiteralPath (Join-Path $obsStage "SOURCE.txt") -Raw
        Assert ($sourceText.Contains("Corresponding source: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/tree/$sourceSha/OBS")) `
            "OBS SOURCE.txt source does not match the installer payload source"
        $info = [Diagnostics.FileVersionInfo]::GetVersionInfo($obsBinary)
        Assert ($info.ProductName -eq "SpectralForge Chimera OBS filter" -and $info.FileVersion -eq $moduleVersion -and
                $info.ProductVersion -ceq $expectedModuleProductVersion) `
            "OBS filter version resource: '$($info.ProductName)' $($info.FileVersion) / $($info.ProductVersion); expected module $moduleVersion with its source commit"
        Pass "OBS filter version resource: $($info.ProductName) $($info.ProductVersion)"
    }

    $module = [Runtime.InteropServices.NativeLibrary]::Load($vstBinary)
    try { Assert ([Runtime.InteropServices.NativeLibrary]::GetExport($module, "GetPluginFactory") -ne [IntPtr]::Zero) "VST3 factory export is missing" }
    finally { [Runtime.InteropServices.NativeLibrary]::Free($module) }
    Pass "Installed VST3 loads through the Windows loader and exports GetPluginFactory"

    Check-AppShortcut $app

    # A repeated same-version install must repair package files without claiming user files.
    $userFolder = Join-Path $app "UserFiles"
    New-Item -ItemType Directory -Path $userFolder | Out-Null
    $preset = Join-Path $userFolder "연주 참고.chimera"
    $userIR = Join-Path $userFolder "personal-cab.wav"
    "user-owned preset fixture" | Set-Content -LiteralPath $preset
    "user-owned IR fixture" | Set-Content -LiteralPath $userIR
    $presetHash = (Get-FileHash -LiteralPath $preset).Hash
    $irHash = (Get-FileHash -LiteralPath $userIR).Hash
    if ($obsOffered) {
        foreach ($relative in @("data/user-settings.json", "Presets/연주 참고.chimera", "IRs/personal-cab.wav")) {
            $path = Join-Path $obsPlugin $relative
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $path) | Out-Null
            "OBS user-owned fixture: $relative" | Set-Content -LiteralPath $path -Encoding utf8
            $obsUserFiles[$path] = (Get-FileHash -LiteralPath $path).Hash
        }
    }
    # An already installed personal capture must not be overwritten by repair.
    Copy-Item -LiteralPath "Assets/IRs/guitar_jensen_sm57.wav" -Destination $installedRaven[0] -Force
    $personalCaptureHash = (Get-FileHash -LiteralPath $installedRaven[0]).Hash
    Remove-Item -LiteralPath (Join-Path $app "ReferenceTools/ChimeraRender.exe")
    "damaged documentation fixture" | Set-Content -LiteralPath (Join-Path $app "README.txt")
    $modifiedReadmeHash = (Get-FileHash -LiteralPath (Join-Path $app "README.txt") -Algorithm SHA256).Hash.ToLowerInvariant()
    $preservedReadme = Join-Path $app "PreservedDocuments/$modifiedReadmeHash/README.txt"
    Install "02-repair" $app "vst3,standalone,reference"
    Check-Payload $app $true $true $true $obsOffered
    Assert ((Get-FileHash -LiteralPath $preservedReadme -Algorithm SHA256).Hash.ToLowerInvariant() -eq $modifiedReadmeHash) "Repair did not preserve the modified README byte-for-byte"
    Assert ((Get-FileHash -LiteralPath $preset).Hash -eq $presetHash -and (Get-FileHash -LiteralPath $userIR).Hash -eq $irHash) "Repair modified user files"
    Assert ((Get-FileHash -LiteralPath $installedRaven[0]).Hash -eq $personalCaptureHash) "Repair overwrote a personal Raven IR"
    Pass "Same-version reinstall repairs files and preserves modified documentation copies and personal presets/IRs"
    Uninstall "03-full-uninstall" $app
    Equal-File $fixture $installedIR
    Equal-File $guitarFixture $installedGuitarIR
    Assert ((Get-FileHash -LiteralPath $installedRaven[0]).Hash -eq $personalCaptureHash) "Uninstall removed or changed a personal Raven IR"
    Pass "Personal IR collection is preserved after uninstall"
    Assert ((Get-FileHash -LiteralPath $preset).Hash -eq $presetHash -and (Get-FileHash -LiteralPath $userIR).Hash -eq $irHash) "Uninstall modified user files"
    Assert ((Get-FileHash -LiteralPath $preservedReadme -Algorithm SHA256).Hash.ToLowerInvariant() -eq $modifiedReadmeHash) "Uninstall removed the user's preserved documentation"
    Equal-File (Join-Path $stagePath "THIRD_PARTY_NOTICES.md") (Join-Path $app "Documentation/THIRD_PARTY_NOTICES.md")
    Pass "Uninstall removes owned binaries, shortcut and Windows registration; preserves user files"
    if ($obsOffered) { Pass "Repair keeps the OBS filter answer; uninstall removes owned payload and preserves OBS user settings, presets, IRs, scenes and other plugins" }

    $custom = Join-Path $env:RUNNER_TEMP "Chimera 설치 검증"
    Assert (!(Test-Path -LiteralPath $custom)) "Custom test path already exists"
    $customVst3 = Join-Path $env:RUNNER_TEMP "Chimera VST3 설치 검증"
    Assert (!(Test-Path -LiteralPath $customVst3)) "Custom VST3 test path already exists"
    Install "04-vst3-only" $custom "vst3" $customVst3 -ObsTask "obsfilter"
    Check-Payload $custom $true $false $false $obsOffered
    Pass "Independent custom application and VST3 installation folders accept Unicode paths"
    if ($obsOffered) {
        # OBS has the filter loaded: Setup must stop before changing it, and the
        # uninstaller must refuse, leaving the installation intact.
        $runningObs = Start-ObsProcessFixture
        try {
            $before = Install-Snapshot $custom
            # Exit code 7: the Preparing to Install stage stopped Setup.
            foreach ($case in @(@{ Name = "update"; Task = "obsfilter" }, @{ Name = "repair"; Task = "" },
                               @{ Name = "deselect"; Task = "!obsfilter" })) {
                Install "04-obs-running-$($case.Name)" $custom "vst3" -ObsTask $case.Task -ExpectedExitCode 7
                Assert ((Install-Snapshot $custom) -ceq $before) "Blocked $($case.Name) changed files or registration"
                Check-ObsUserData
            }
            $uninstaller = (Get-ItemProperty -LiteralPath $registry).UninstallString.Trim('"')
            $refused = Start-Process -FilePath $uninstaller -ArgumentList @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART",
                "/LOG=`"$logPath/04-obs-running-uninstall.log`"") -PassThru -Wait
            Assert ($refused.ExitCode -ne 0) "Uninstall reported success while OBS was running"
            Assert ((Install-Snapshot $custom) -ceq $before) "Blocked uninstall changed files or registration"
            Check-Payload $custom $true $false $false $true
            Pass "With OBS running, Setup stops before changing the filter (exit 7) and uninstall refuses (exit $($refused.ExitCode)); nothing changes"
        } finally {
            if (!$runningObs.HasExited) { $runningObs.Kill(); $runningObs.WaitForExit() }
        }
    }
    Install "04b-vst3-remembered-repair" $custom "vst3"
    Check-Payload $custom $true $false $false $obsOffered
    Pass "Repair remembers the selected VST3 folder without a /VST3DIR override"
    $previousVst = $vst
    $vstUserFile = Join-Path $previousVst "personal-preserve.txt"
    "user-owned plugin-folder fixture" | Set-Content -LiteralPath $vstUserFile
    $vstUserHash = (Get-FileHash -LiteralPath $vstUserFile).Hash
    $movedVst3 = Join-Path $env:RUNNER_TEMP "Chimera VST3 이동 검증"
    Install "04c-vst3-path-move" $custom "vst3" $movedVst3
    Check-Payload $custom $true $false $false $obsOffered
    Assert (!(Test-Path -LiteralPath (Join-Path $previousVst "Contents/x86_64-win/SpectralForge Chimera.vst3"))) "Changing VST3 folder left the old plugin binary"
    Assert ((Get-FileHash -LiteralPath $vstUserFile).Hash -eq $vstUserHash) "Changing VST3 folder modified a user file"
    Pass "Changing the selected VST3 folder retires only its exact old plugin binary and preserves user files"
    if ($obsOffered) {
        Pass "OBS filter follows a custom Unicode VST3 folder and a later folder move"
        Install "04d-obs-filter-deselected" $custom "vst3" -ObsTask "!obsfilter"
        Check-Payload $custom $true $false $false $false
        Pass "Deselecting the OBS filter removes the owned payload and preserves OBS user settings, presets and IRs"
    }
    Uninstall "05-vst3-uninstall" $custom
    Assert ((Get-FileHash -LiteralPath $vstUserFile).Hash -eq $vstUserHash) "Uninstall removed a user file from the previous VST3 folder"
    Pass "VST3-only selection and removal; Unicode custom app path"
    Install "06-standalone-only" $custom "standalone"
    Check-Payload $custom $false $true $false
    Check-AppShortcut $custom
    Uninstall "07-standalone-uninstall" $custom
    Pass "Standalone-only selection and removal; no VST3 installed"
    Pass "Every current Setup/repair/removal invocation preserved the existing AppData settings, IR library and preset bytes"

    # Run the separately reported, real public beta.1 Setup upgrade only after
    # every existing current-product installation and launch gate has passed.
    & (Join-Path $PSScriptRoot "Verify-WindowsPreviousReleaseUpgrade.ps1") -Installer $installerPath -Stage $stagePath -LogDirectory $logPath
    $previousUpgrade = Get-Content -LiteralPath (Join-Path $logPath "PreviousReleaseUpgradeVerification.json") -Raw | ConvertFrom-Json
    Assert ($previousUpgrade.success -and $previousUpgrade.actual_previous_release_upgrade_verified) "Actual previous-release upgrade verification did not complete"
    $actualPreviousReleaseUpgradeVerified = $true
    Pass "Actual public beta.1 Setup to current product Setup upgrade, repair and removal preserve user documents and files"
    $success = $true
} catch {
    $report.Add("FAIL: $($_.Exception.Message)")
    throw
} finally {
    # Remove only the OBS fixtures this script created.
    if ($obsUserFixtures.Count) {
        Remove-Item -LiteralPath $obsScene -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $otherObsPlugin -Recurse -Force -ErrorAction SilentlyContinue
    }
    $report | Set-Content -LiteralPath (Join-Path $logPath "InstallerVerification.txt")
    @{ success = $success; source_sha = $sourceSha; run_id = $env:GITHUB_RUN_ID;
       actual_previous_release_upgrade_verified = $actualPreviousReleaseUpgradeVerified;
       version = $payload.version; product_version = $payload.product_version;
       installer_version = $payload.installer_version; build_id = $payload.build_id;
       observed_display_versions = @($observedVersions.ToArray());
       installer = [IO.Path]::GetFileName($installerPath);
       installer_sha256 = (Get-FileHash -LiteralPath $installerPath -Algorithm SHA256).Hash.ToLowerInvariant();
       payload_manifest_sha256 = (Get-FileHash -LiteralPath (Join-Path $stagePath "payload-manifest.json") -Algorithm SHA256).Hash.ToLowerInvariant();
       checks = @($report.ToArray()) } | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath (Join-Path $logPath "InstallerVerification.json") -Encoding utf8
}
