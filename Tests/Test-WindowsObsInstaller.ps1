param([string]$LogDirectory = "build/windows-obs-installer-verification")
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -ne "true" -or !$IsWindows -or ![Environment]::Is64BitProcess) {
    throw "OBS installer fixtures require a disposable x64 Windows GitHub runner."
}
$repository = Split-Path -Parent $PSScriptRoot
$iscc = Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6/ISCC.exe"
$work = Join-Path $env:RUNNER_TEMP ("Chimera OBS guard " + [Guid]::NewGuid().ToString("N"))
$stage = Join-Path $work "stage"
$app = Join-Path $work "설치 경로"
$vst = Join-Path $work "VST3 경로"
$obs = Join-Path $work "programdata/obs-studio/plugins/chimera-obs"
$registry = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\SpectralForge.Chimera.ObsGuardFixture_is1"
if (Test-Path -LiteralPath $registry) { throw "OBS guard fixture already installed" }
New-Item -ItemType Directory -Force -Path $stage, $LogDirectory | Out-Null
$logs = (Resolve-Path -LiteralPath $LogDirectory).Path
$checks = [Collections.Generic.List[string]]::new()
$success = $false
$runningObs = $null
function Assert([bool]$Condition, [string]$Message) { if (!$Condition) { throw $Message } }
function Pass([string]$Message) { $checks.Add($Message); Write-Host "PASS: $Message" }
function Write-Fixture([string]$Path, [string]$Text) {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}
function Digest([string]$Path) {
    if (!(Test-Path -LiteralPath $Path)) { return "absent" }
    return (@(Get-ChildItem -LiteralPath $Path -Recurse -File | Sort-Object FullName | ForEach-Object {
        "$([IO.Path]::GetRelativePath($Path, $_.FullName))=$((Get-FileHash -LiteralPath $_.FullName).Hash)"
    }) -join "`n")
}
function Snapshot {
    $registration = if (Test-Path -LiteralPath $registry) {
        Get-ItemProperty -LiteralPath $registry | Select-Object * -ExcludeProperty PS* | ConvertTo-Json -Compress
    } else { "absent" }
    return (@((Digest $app), (Digest $vst), (Digest (Join-Path $work "programdata")),
        (Digest (Join-Path $work "shortcuts")), (Digest (Join-Path $work "userdata")), $registration) -join "`n---`n")
}
function Invoke-Setup([string]$Exe, [string]$Name, [string]$Task, [int]$Expected = 0) {
    $arguments = @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/SP-", "/LANG=korean",
        "/TYPE=custom", "/COMPONENTS=vst3", "/DIR=`"$app`"", "/VST3DIR=`"$vst`"", "/LOG=`"$logs/$Name.log`"")
    if ($Task) { $arguments += "/MERGETASKS=`"$Task`"" }
    $process = Start-Process -FilePath $Exe -ArgumentList $arguments -PassThru -Wait
    Assert ($process.ExitCode -eq $Expected) "$Name returned $($process.ExitCode), expected $Expected"
}
function Invoke-Uninstall([string]$Name, [bool]$Blocked) {
    $exe = (Get-ItemProperty -LiteralPath $registry).UninstallString.Trim('"')
    $process = Start-Process -FilePath $exe -ArgumentList @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART",
        "/LOG=`"$logs/$Name.log`"") -PassThru -Wait
    if ($Blocked) { Assert ($process.ExitCode -ne 0) "$Name unexpectedly succeeded" }
    else { Assert ($process.ExitCode -eq 0 -and !(Test-Path -LiteralPath $registry)) "$Name failed" }
}
function Start-ObsFixture {
    $exe = Join-Path $work "obs64.exe"
    Copy-Item -LiteralPath (Join-Path $env:SystemRoot "System32/PING.EXE") -Destination $exe -Force
    $script:runningObs = Start-Process -FilePath $exe -ArgumentList @("-n", "600", "127.0.0.1") -WindowStyle Hidden -PassThru
    Assert (!$runningObs.HasExited) "obs64.exe fixture did not stay alive"
}
function Stop-ObsFixture {
    if ($null -ne $runningObs -and !$runningObs.HasExited) { $runningObs.Kill(); $runningObs.WaitForExit() }
    $script:runningObs = $null
}
try {
    # Compile the entire production installer, with synthetic payload bytes and
    # isolated destinations/identity. No guard code is reimplemented here.
    foreach ($name in @("Standalone/SpectralForge Chimera.exe", "ReferenceTools/ChimeraRender.exe",
        "VST3/SpectralForge Chimera.vst3/Contents/x86_64-win/SpectralForge Chimera.vst3",
        "README.txt", "MANUAL.html", "THIRD_PARTY_NOTICES.md", "payload-manifest.json",
        "OBS/chimera-obs/bin/64bit/chimera-obs.dll", "OBS/chimera-obs/data/locale/en-US.ini",
        "OBS/chimera-obs/data/locale/ko-KR.ini", "OBS/chimera-obs/SOURCE.txt",
        "OBS/chimera-obs/README.txt", "OBS/chimera-obs/LICENSE.txt")) {
        Write-Fixture (Join-Path $stage $name) "Synthetic fixture: $name"
    }
    $rules = Join-Path $work "rules.iss"
    & python (Join-Path $repository "Tools/windows_document_rules.py") --stage $stage --output $rules
    Assert ($LASTEXITCODE -eq 0) "Cannot generate document rules"
    $source = Get-Content -LiteralPath (Join-Path $repository "Installer/Chimera.iss") -Raw
    $source = $source.Replace("AppId=SpectralForge.ChimeraAmpMatrix", "AppId=SpectralForge.Chimera.ObsGuardFixture")
    $source = $source.Replace('{commonappdata}', (Join-Path $work "programdata"))
    $source = $source.Replace('{commoncf64}', (Join-Path $work "commonfiles"))
    $source = $source.Replace('{commonprograms}', (Join-Path $work "programs"))
    $source = $source.Replace('{group}', (Join-Path $work "shortcuts"))
    $source = $source.Replace('..\Assets\Artwork\spectralforge-emblem.ico', (Join-Path $repository "Assets/Artwork/spectralforge-emblem.ico"))
    $source = $source.Replace('#include "LegacyDocumentCleanup.iss"', '#include "' + (Join-Path $repository "Installer/LegacyDocumentCleanup.iss") + '"')
    $variants = [ordered]@{
        healthy = $source
        activation = $source.Replace("'WbemScripting.SWbemLocator'", "'Chimera.NonexistentWmiFixture'")
        query = $source.Replace("FROM Win32_Process WHERE", "FROM Chimera_NonexistentProcess WHERE")
        enumeration = $source.Replace("if Processes.Count > 0", "Processes := 0;`r`n    if Processes.Count > 0")
        noPayload = $source
    }
    $installers = @{}
    foreach ($name in $variants.Keys) {
        if ($name -notin @("healthy", "noPayload")) { Assert ($variants[$name] -cne $source) "Fault injection did not match: $name" }
        $selectedStage = $stage
        if ($name -eq "noPayload") {
            $selectedStage = Join-Path $work "stage-no-obs"
            Copy-Item -LiteralPath $stage -Destination $selectedStage -Recurse
            Remove-Item -LiteralPath (Join-Path $selectedStage "OBS") -Recurse -Force
        }
        $script = Join-Path $work "$name.iss"
        Write-Fixture $script $variants[$name]
        $output = Join-Path $work "output-$name"
        & $iscc "/DStageDir=$selectedStage" "/DOutputPath=$output" "/DProductVersion=1.3.2" "/DPackageVersion=1.3.2-fixture" "/DDocumentRulesPath=$rules" $script > "$logs/compile-$name.log"
        Assert ($LASTEXITCODE -eq 0) "Inno compile failed: $name"
        $installers[$name] = Join-Path $output "SpectralForge-Chimera-1.3.2-fixture-win64-Setup.exe"
    }
    $protected = @("userdata/obs-studio/basic/scenes/scene.json", "userdata/SpectralForge/Chimera/ir-library.json",
        "userdata/SpectralForge Chimera/SpectralForge Chimera.settings", "userdata/Presets/연주.chimera",
        "programdata/obs-studio/plugins/other-plugin/bin/64bit/other.dll")
    foreach ($name in $protected) { Write-Fixture (Join-Path $work $name) "User-owned fixture: $name" }
    $userBefore = Digest (Join-Path $work "userdata")
    $otherBefore = Digest (Join-Path $work "programdata/obs-studio/plugins/other-plugin")
    Start-ObsFixture
    $before = Snapshot
    Invoke-Setup $installers.healthy "running-fresh" "obsfilter" 7
    Assert ((Snapshot) -ceq $before) "Blocked fresh install changed data"
    Stop-ObsFixture
    foreach ($fault in @("activation", "query", "enumeration")) {
        $before = Snapshot
        Invoke-Setup $installers[$fault] "$fault-fresh" "obsfilter" 7
        Assert ((Snapshot) -ceq $before) "Blocked $fault fresh install changed data"
        Assert ((Get-Content "$logs/$fault-fresh.log" -Raw).Contains("process query failed; mutation blocked")) "Missing failure diagnostic"
    }
    Pass "Fresh selected install: running OBS and all three WMI failures exit 7 without mutation"
    Invoke-Setup $installers.healthy "install" "obsfilter"
    foreach ($name in @("data/user-settings.json", "Presets/연주.chimera", "IRs/personal.wav")) {
        Write-Fixture (Join-Path $obs $name) "OBS user file: $name"
    }
    $userObs = @{}
    foreach ($file in Get-ChildItem -LiteralPath $obs -Recurse -File | Where-Object { $_.Name -in @("user-settings.json", "연주.chimera", "personal.wav") }) {
        $userObs[$file.FullName] = (Get-FileHash -LiteralPath $file.FullName).Hash
    }
    foreach ($variant in @("healthy", "activation", "query", "enumeration", "noPayload")) {
        if ($variant -in @("healthy", "noPayload")) { Start-ObsFixture }
        $before = Snapshot
        foreach ($case in @(@{ Name = "update"; Task = "obsfilter" }, @{ Name = "repair"; Task = "" }, @{ Name = "deselect"; Task = "!obsfilter" })) {
            Invoke-Setup $installers[$variant] "$variant-$($case.Name)" $case.Task 7
            Assert ((Snapshot) -ceq $before) "Blocked $variant $($case.Name) changed data"
        }
        if ($variant -eq "healthy") {
            Invoke-Uninstall "running-uninstall" $true
            Assert ((Snapshot) -ceq $before) "Blocked uninstall changed data"
            # Missing DLL must not disable the guard if the generated path hint remains.
            $dll = Join-Path $obs "bin/64bit/chimera-obs.dll"
            Remove-Item -LiteralPath $dll
            $damagedBefore = Snapshot
            Invoke-Uninstall "running-damaged-uninstall" $true
            Assert ((Snapshot) -ceq $damagedBefore) "Damaged-install guard changed data"
            Copy-Item -LiteralPath (Join-Path $stage "OBS/chimera-obs/bin/64bit/chimera-obs.dll") -Destination $dll
        }
        Stop-ObsFixture
    }
    Pass "Update/repair/deselect: running and unknown states block; no-OBS-payload and damaged installs stay protected"
    # Confirm normal repair, deselection and removal still work and preserve all
    # user bytes. Ownership is exactly staged files plus the generated path hint.
    Remove-Item -LiteralPath (Join-Path $obs "bin/64bit/chimera-obs.dll")
    Invoke-Setup $installers.healthy "repair-stopped" ""
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $stage "OBS/chimera-obs") -Recurse -File) {
        $installed = Join-Path $obs ([IO.Path]::GetRelativePath((Join-Path $stage "OBS/chimera-obs"), $file.FullName))
        Assert ((Get-FileHash $file.FullName).Hash -ceq (Get-FileHash $installed).Hash) "Repair payload differs"
    }
    Invoke-Setup $installers.healthy "deselect-stopped" "!obsfilter"
    Invoke-Uninstall "uninstall-stopped" $false
    foreach ($item in $userObs.GetEnumerator()) { Assert ((Get-FileHash -LiteralPath $item.Key).Hash -ceq $item.Value) "User OBS file changed" }
    $remaining = @(Get-ChildItem -LiteralPath $obs -Recurse -File)
    Assert ($remaining.Count -eq $userObs.Count) "Installer-owned OBS files remain after removal"
    Assert ((Digest (Join-Path $work "userdata")) -ceq $userBefore) "User settings/scenes/presets changed"
    Assert ((Digest (Join-Path $work "programdata/obs-studio/plugins/other-plugin")) -ceq $otherBefore) "Other plugin changed"
    Pass "Healthy repair/deselection/uninstall preserve user bytes and remove only owned OBS files"
    # Install the faulty uninstaller while no filter is selected or present.
    # Then simulate a manually installed module to exercise its real callback.
    foreach ($fault in @("activation", "query", "enumeration")) {
        Invoke-Setup $installers[$fault] "$fault-no-filter-install" "!obsfilter"
        $dll = Join-Path $obs "bin/64bit/chimera-obs.dll"
        Write-Fixture $dll "Manual module fixture"
        $before = Snapshot
        Invoke-Uninstall "$fault-uninstall" $true
        Assert ((Snapshot) -ceq $before) "WMI failure uninstall changed data: $fault"
        Remove-Item -LiteralPath $dll
        Invoke-Uninstall "$fault-cleanup" $false
    }
    Pass "Real uninstaller refuses COM activation/query/enumeration failure; unrelated installs remain usable"
    $success = $true
} finally {
    Stop-ObsFixture
    @{ success = $success; source_sha = (& git -C $repository rev-parse HEAD).Trim(); run_id = $env:GITHUB_RUN_ID;
       scope = "Production Inno script with isolated identity/paths, synthetic payload and name-only OBS process; test-only source mutations inject WMI faults";
       actual_obs_app_verified = $false; interactive_installer_verified = $false; product_binary_verified = $false;
       checks = @($checks.ToArray()) } | ConvertTo-Json -Depth 5 | Set-Content "$logs/ObsInstallerVerification.json" -Encoding utf8
}
