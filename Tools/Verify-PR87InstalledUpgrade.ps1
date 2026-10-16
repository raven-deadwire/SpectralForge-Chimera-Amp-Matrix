# PR87 pinned-product qualification; assertions copied unchanged from the accepted upgrade harness.
param(
    [Parameter(Mandatory = $true)][string]$Artifacts,
    [string]$Output = "build/pr87-installed-upgrade"
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -ne "true" -or !$IsWindows -or ![Environment]::Is64BitProcess -or !$env:RUNNER_TEMP) {
    throw "Requires a disposable x64 Windows GitHub runner, never a user workstation."
}
$source = "72ecf35fa90293418da8635225435095dbca1135"
$version = "1.3.1-beta.3"
$registry = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\SpectralForge.ChimeraAmpMatrix_is1"
if (Test-Path -LiteralPath $registry) { throw "Refusing to change an existing Chimera installation" }
New-Item -ItemType Directory -Force -Path $Output | Out-Null
$outputPath = (Resolve-Path -LiteralPath $Output).Path
$work = Join-Path $env:RUNNER_TEMP ("Chimera beta2-beta3 " + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $work | Out-Null
$app = Join-Path $work "앱 업데이트"
$vstRoot = Join-Path $work "VST3 업데이트"
$stage = Join-Path $work "stage"
$checks = [Collections.Generic.List[string]]::new()
$success = $false
$started = [DateTime]::UtcNow.ToString("o")
$invocations = 0
function Assert([bool]$Condition, [string]$Message) { if (!$Condition) { throw $Message } }
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Pass([string]$Text) { $checks.Add("PASS: $Text"); Write-Host "PASS: $Text" }
function Single-File([string]$Name) {
    $found = @(Get-ChildItem -LiteralPath $Artifacts -Recurse -File | Where-Object Name -CEQ $Name)
    Assert ($found.Count -eq 1) "Expected exactly one pinned artifact file: $Name"
    $found[0].FullName
}
function Setup([string]$Exe, [string]$Name, [bool]$Remove = $false) {
    $arguments = @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/SP-", "/LOG=`"$outputPath/$Name.log`"")
    if (!$Remove) { $arguments += @("/LANG=korean", "/TYPE=custom", "/COMPONENTS=standalone,vst3", "/TASKS=", "/DIR=`"$app`"", "/VST3DIR=`"$vstRoot`"") }
    $process = Start-Process -FilePath $Exe -ArgumentList $arguments -Wait -PassThru
    $script:invocations += 1
    Assert ($process.ExitCode -eq 0) "$Name exit code $($process.ExitCode)"
}
$personal = @{}
function Sentinel([string]$Path) {
    Assert (!(Test-Path -LiteralPath $Path)) "Sentinel path already exists"
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    [IO.File]::WriteAllText($Path, "File preservation sentinel; not a valid DAW project or IR.", [Text.UTF8Encoding]::new($false))
    $personal[$Path] = Hash $Path
}
function Preserved {
    foreach ($path in $personal.Keys) { Assert ((Hash $path) -ceq $personal[$path]) "User file changed: $path" }
}
function Current-Payload {
    Assert ((Hash (Join-Path $app "SpectralForge Chimera.exe")) -ceq (Hash (Join-Path $stage "Standalone/SpectralForge Chimera.exe"))) "Installed standalone mismatch"
    Assert ((Hash (Join-Path $vstRoot "SpectralForge Chimera.vst3/Contents/x86_64-win/SpectralForge Chimera.vst3")) -ceq (Hash (Join-Path $stage "VST3/SpectralForge Chimera.vst3/Contents/x86_64-win/SpectralForge Chimera.vst3"))) "Installed VST3 mismatch"
    foreach ($name in @("payload-manifest.json", "chimera-beta-lifecycle.json")) {
        Assert ((Hash (Join-Path $app "Documentation/$name")) -ceq (Hash (Join-Path $stage $name))) "Installed metadata mismatch: $name"
    }
    Assert ((Hash (Join-Path $app "BETA_LIFECYCLE.txt")) -ceq (Hash (Join-Path $stage "BETA_LIFECYCLE.txt"))) "Installed lifecycle notice mismatch"
    $registration = Get-ItemProperty -LiteralPath $registry
    Assert ($registration.DisplayVersion -ceq "1.3.1") "Installer numeric version changed"
    Assert ($registration.InstallLocation.TrimEnd([char]92) -ceq $app.TrimEnd([char]92)) "Selected install directory changed"
}
try {
    $installer = Single-File "SpectralForge-Chimera-$version-win64-Setup.exe"
    $portable = Single-File "SpectralForge-Chimera-$version-win64.zip"
    Assert ((Hash $installer) -ceq "ea8c8b90be574a5a0204ecd30afb7cf17ecff4856b97d3227d04cb985e12998e") "Candidate Setup hash mismatch"
    Assert ((Hash $portable) -ceq "c13914acfdd420198a1f4f4f3c59326a13cce0ca7a4d8e9e2f27351cca825300") "Candidate portable hash mismatch"
    Expand-Archive -LiteralPath $portable -DestinationPath $stage
    $payload = Get-Content -LiteralPath (Join-Path $stage "payload-manifest.json") -Raw | ConvertFrom-Json
    $lifecycle = Get-Content -LiteralPath (Join-Path $stage "chimera-beta-lifecycle.json") -Raw | ConvertFrom-Json
    Assert ($payload.source_sha -ceq $source -and $payload.version -ceq $version) "Candidate payload identity mismatch"
    Assert ($lifecycle.source_sha -ceq $source -and $lifecycle.mode -ceq "qualification-only" -and $lifecycle.expiry_approved -eq $false) "Expected nonpublishable qualification-only candidate"
    Pass "Existing CI Setup/portable bytes, source and qualification-only lifecycle are pinned; no rebuild"
    $previous = Join-Path $work "beta2-Setup.exe"
    Invoke-WebRequest -Uri "https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.3.1-beta.2/SpectralForge-Chimera-1.3.1-beta.2-win64-Setup.exe" -OutFile $previous -MaximumRedirection 5 -TimeoutSec 180
    Assert ((Get-Item -LiteralPath $previous).Length -eq 210744980 -and (Hash $previous) -ceq "610b44602b7aec2dd5adb4ac3d90692b4f6dc683ab56ae7012f11bf457a1b4e7") "Released beta.2 size/hash mismatch"
    Setup $previous "01-beta2-install"
    $oldPayload = Get-Content -LiteralPath (Join-Path $app "Documentation/payload-manifest.json") -Raw | ConvertFrom-Json
    Assert ($oldPayload.version -ceq "1.3.1-beta.2" -and $oldPayload.source_sha -ceq "6033236cb81820a43679cbbd34be020804f9658d") "Installed beta.2 identity mismatch"
    Pass "Actual public beta.2 Setup installed in Unicode app and VST3 paths"
    foreach ($relative in @("UserFiles/개인 프리셋.chimera", "UserFiles/personal.wav", "Sessions/project.song")) { Sentinel (Join-Path $app $relative) }
    Sentinel (Join-Path $vstRoot "personal-notes.txt")
    $manual = Join-Path $app "Documentation/MANUAL.html"
    [IO.File]::AppendAllText($manual, "`r`n<p>Personal beta.2 annotation</p>")
    $manualHash = Hash $manual
    $snapshot = Join-Path $app "PreservedDocuments/$manualHash/Documentation/MANUAL.html"
    Setup $installer "02-beta3-upgrade"
    Current-Payload
    Preserved
    Assert ((Hash $snapshot) -ceq $manualHash) "Annotated beta.2 manual was not preserved"
    Pass "Actual beta.2 to beta.3 same-numeric-version upgrade replaces both binaries and metadata while preserving personal sentinels and annotated manual"
    [IO.File]::WriteAllText((Join-Path $app "SpectralForge Chimera.exe"), "intentional repair fault")
    Setup $installer "03-beta3-repair"
    Current-Payload
    Preserved
    Assert ((Hash $snapshot) -ceq $manualHash) "Repair changed preserved manual"
    Pass "Repair restores deliberately damaged standalone to pinned bytes and retains VST3, metadata and user data"
    $uninstaller = (Get-ItemProperty -LiteralPath $registry).UninstallString.Trim('"')
    Assert ($uninstaller.StartsWith((Join-Path $app "Uninstall") + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) "Unexpected uninstall executable"
    Setup $uninstaller "04-beta3-uninstall" $true
    Assert (!(Test-Path -LiteralPath $registry)) "Uninstall registration remains"
    Assert (!(Test-Path -LiteralPath (Join-Path $app "SpectralForge Chimera.exe"))) "Owned standalone remains"
    Assert (!(Test-Path -LiteralPath (Join-Path $vstRoot "SpectralForge Chimera.vst3/Contents/x86_64-win/SpectralForge Chimera.vst3"))) "Owned VST3 remains"
    Preserved
    Assert ((Hash $snapshot) -ceq $manualHash) "Uninstall removed preserved manual"
    Pass "Upgraded uninstall removes owned binaries/registration and retains all personal sentinels and manual snapshot"
    $success = $true
} catch {
    $checks.Add("FAIL: $($_.Exception.Message)")
    throw
} finally {
    @{ started_utc=$started; completed_utc=[DateTime]::UtcNow.ToString("o");
       os=[Environment]::OSVersion.VersionString; powershell=$PSVersionTable.PSVersion.ToString();
       raw_logs=@(Get-ChildItem -LiteralPath $outputPath -Filter "*.log" | ForEach-Object { @{ name=$_.Name; sha256=(Hash $_.FullName) } });
       success=$success; product_source_sha=$source; product_version=$version;
       tool_source_sha=$env:CHIMERA_AUDIT_TOOL_SHA; run_id=$env:GITHUB_RUN_ID; run_attempt=$env:GITHUB_RUN_ATTEMPT;
       previous_release="v1.3.1-beta.2"; candidate_build_run="38887964838";
       qualification_only=$true; publication_approved=$false; daw_project_acceptance=$false;
       scope="Actual pinned Windows Setup install, beta.2 to beta.3 upgrade, repair, uninstall; byte sentinels are not valid projects or IR audio";
       installer_invocations=$invocations; checks=@($checks.ToArray()) } | ConvertTo-Json -Depth 5 |
       Set-Content -LiteralPath (Join-Path $outputPath "result.json") -Encoding utf8
}
