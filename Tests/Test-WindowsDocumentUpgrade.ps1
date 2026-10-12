param([string]$LogDirectory = "build/windows-document-upgrade-verification")
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -ne "true" -or !$IsWindows -or ![Environment]::Is64BitProcess) {
    throw "Native document migration verification requires a disposable x64 Windows GitHub runner."
}
$repository = Split-Path -Parent $PSScriptRoot
$isccCommand = Get-Command ISCC.exe -ErrorAction SilentlyContinue
$iscc = if ($isccCommand) { $isccCommand.Source } else { Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6/ISCC.exe" }
if (!(Test-Path -LiteralPath $iscc)) { throw "Inno Setup is required for native historical uninstall-log verification." }
New-Item -ItemType Directory -Force -Path $LogDirectory | Out-Null
$logs = (Resolve-Path -LiteralPath $LogDirectory).Path
$work = Join-Path $env:RUNNER_TEMP ("Chimera document lifecycle " + [Guid]::NewGuid().ToString("N"))
$oldStage = Join-Path $work "old"
$newStage = Join-Path $work "new"
$output = Join-Path $work "installers"
New-Item -ItemType Directory -Force -Path $oldStage, $newStage, $output | Out-Null
$app = Join-Path $work "설치 경로"
$fresh = Join-Path $work "신규 설치"
$registry = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\SpectralForge.Chimera.DocumentUpgradeFixture_is1"
if (Test-Path -LiteralPath $registry) { throw "A document fixture installation already exists; refusing to overwrite it." }
$checks = [Collections.Generic.List[string]]::new()
$success = $false
$installerInvocations = 0
$nativeCompilerPassed = $false
$compilerVersion = $null
$guardCancellationAndTerminationVerified = $false
function Assert([bool]$Condition, [string]$Message) { if (!$Condition) { throw $Message } }
function Pass([string]$Message) { $checks.Add("PASS: $Message"); Write-Host "PASS: $Message" }
function Hash([string]$Path) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Write-Fixture([string]$Path, [string]$Value) {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    [IO.File]::WriteAllText($Path, $Value, [Text.UTF8Encoding]::new($false))
}
function Invoke-Setup([string]$Executable, [string]$Name, [string]$Destination, [bool]$ExpectSuccess = $true, [string[]]$ExtraArguments = @()) {
    $arguments = @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/SP-", "/LOG=`"$logs/$Name.log`"")
    if ($Destination) { $arguments += "/DIR=`"$Destination`"" }
    $arguments += $ExtraArguments
    $process = Start-Process -FilePath $Executable -ArgumentList $arguments -PassThru -Wait
    $script:installerInvocations += 1
    if ($ExpectSuccess) { Assert ($process.ExitCode -eq 0) "$Name failed with exit code $($process.ExitCode)" }
    else { Assert ($process.ExitCode -ne 0) "$Name should have refused the unsafe uninstall" }
}
function Uninstaller {
    return (Get-ItemProperty -LiteralPath $registry).UninstallString.Trim('"')
}
function Assert-NoDeferredDocumentDelete {
    $key = "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager"
    $entry = Get-ItemProperty -LiteralPath $key
    foreach ($property in @("PendingFileRenameOperations", "PendingFileRenameOperations2")) {
        if ($entry.PSObject.Properties.Name -contains $property) {
            foreach ($operation in $entry.$property) {
                Assert (!$operation.Contains($work, [StringComparison]::OrdinalIgnoreCase)) "Document was queued for deletion on restart: $operation"
            }
        }
    }
}
try {
    # Synthetic payloads create real Inno ownership records. They intentionally
    # do not masquerade as an actual previous Chimera release Setup executable.
    $legacyNames = @("AMP_VALIDATION.md", "AMP_NATIVE_DSP.md", "POST_NATIVE_DSP.md", "PEDAL_BOARD_DSP.md",
        "ARTWORK_PROMPTS.json", "GRAPHICS_DSP_UPDATE.md", "VALIDATION_STATUS.md", "reference/microphone-tags.json")
    $entries = @()
    foreach ($name in $legacyNames) {
        $path = Join-Path $oldStage $name
        Write-Fixture $path "reviewed legacy document $name`r`n"
        $entries += @{ path = $name; platforms = @("windows"); retire = $true; preserve_always = $false;
            variants = @(@{ size = (Get-Item -LiteralPath $path).Length; sha256 = (Hash $path);
                releases = @("native-fixture-old"); platforms = @("windows");
                evidence = @(@{ kind = "native-test-fixture"; source_path = $name }) }) }
    }
    foreach ($stage in @($oldStage, $newStage)) {
        Write-Fixture (Join-Path $stage "README.txt") "package repair documentation`r`n"
        Write-Fixture (Join-Path $stage "MANUAL.html") "<h1>Chimera fixture manual</h1>`r`n"
        Write-Fixture (Join-Path $stage "THIRD_PARTY_NOTICES.md") "Retain all license notices.`r`n"
        Write-Fixture (Join-Path $stage "payload-manifest.json") '{"fixture":true}'
    }
    $inventory = Join-Path $work "inventory.json"
    @{ schema = "spectralforge.legacy-document-inventory.v1"; effective_release = "native-test"; files = $entries } |
        ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $inventory -Encoding utf8
    $rules = Join-Path $work "rules.iss"
    & python (Join-Path $repository "Tools/windows_document_rules.py") --inventory $inventory --stage $newStage --output $rules
    Assert ($LASTEXITCODE -eq 0) "Cannot generate native fixture document rules"
    $common = @'
[Setup]
AppId=SpectralForge.Chimera.DocumentUpgradeFixture
AppName=Chimera document lifecycle fixture
AppVersion=1.3.1
DefaultDirName={autopf}\SpectralForge\Document lifecycle fixture
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallFilesDir={app}\Uninstall
UninstallLogMode=append
CloseApplications=no
RestartApplications=no
SetupLogging=yes
OutputDir={#OutputPath}
OutputBaseFilename={#OutputName}
[Files]
'@
    $oldScript = Join-Path $work "old.iss"
    $newScript = Join-Path $work "new.iss"
    $oldFiles = @'
Source: "{#StageDir}\*.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\*.md"; DestDir: "{app}\Documentation"; Flags: ignoreversion
Source: "{#StageDir}\*.html"; DestDir: "{app}\Documentation"; Flags: ignoreversion
Source: "{#StageDir}\*.json"; DestDir: "{app}\Documentation"; Flags: ignoreversion
Source: "{#StageDir}\reference\*"; DestDir: "{app}\Documentation\reference"; Flags: ignoreversion recursesubdirs createallsubdirs
'@
    $newFiles = @'
Source: "{#StageDir}\*.txt"; DestDir: "{app}"; Flags: ignoreversion; Check: ShouldInstallManagedDocument
Source: "{#StageDir}\*.md"; DestDir: "{app}\Documentation"; Flags: ignoreversion; Check: ShouldInstallManagedDocument
Source: "{#StageDir}\*.html"; DestDir: "{app}\Documentation"; Flags: ignoreversion; Check: ShouldInstallManagedDocument
Source: "{#StageDir}\payload-manifest.json"; DestDir: "{app}\Documentation"; Flags: ignoreversion; Check: ShouldInstallManagedDocument
[Code]
#include CleanupSource
function FixtureCurrentProcessId: Cardinal;
  external 'GetCurrentProcessId@kernel32.dll stdcall';
function InitializeSetup: Boolean;
var
  Parent, Filename: String;
  Parents: TDocumentHandleArray;
  Handle: THandle;
begin
  // Exercise the production locking functions against actual Windows rename
  // and delete calls, with no second handle masking an ineffective lock.
  Parent := ExpandConstant('{tmp}') + '\document-lock-probe';
  Filename := Parent + '\probe.md';
  try
    if not LockDocumentParents(Filename, True, Parents) then
      RaiseException('Cannot acquire native parent locks for the fixture.');
    if RenameFile(Parent, Parent + '-moved') then
      RaiseException('Production parent locks failed to block directory rename.');
  finally
    CloseDocumentHandles(Parents);
  end;
  if not RenameFile(Parent, Parent + '-moved') then
    RaiseException('Parent rename probe was invalid after locks were released.');
  if not RenameFile(Parent + '-moved', Parent) then
    RaiseException('Cannot restore the parent rename probe.');
  if not SaveStringToFile(Filename, 'native document lock probe', False) then
    RaiseException('Cannot create the preservation handle probe.');
  Handle := OpenDocumentPreservationHandle(Filename);
  if not DocValidHandle(Handle) then
    RaiseException('Cannot acquire the production preservation handle.');
  try
    if DeleteFile(Filename) then
      RaiseException('Production preservation handle failed to block deletion.');
  finally
    DocCloseHandle(Handle);
  end;
  if not DeleteFile(Filename) then
    RaiseException('Deletion probe was invalid after the handle was released.');
  RemoveDir(Parent);
  Result := True;
end;
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then CleanupRetiredDocuments;
end;
procedure DeinitializeSetup;
begin
  ReleaseDocumentPreservation;
end;
function InitializeUninstall: Boolean;
var
  Probe, Marker: String;
begin
  Result := InitializeDocumentUninstall;
  if not Result then Exit;
  Probe := ExpandConstant('{param:DOCGUARDPROBE|}');
  if Probe <> '' then begin
    if SaveStringToFile(ExpandConstant('{app}\Documentation\AMP_VALIDATION.md'),
      'A new user file must not replace an absent-path guard.', False) then
      RaiseException('Missing historical pathname was not guarded.');
    if SaveStringToFile(ExpandConstant('{app}\Documentation\reference\microphone-tags.json'),
      'A new user file must not replace a nested absent-path guard.', False) then
      RaiseException('Missing nested historical pathname was not guarded.');
    if Probe = 'hold' then begin
      Marker := ExpandConstant('{param:DOCGUARDMARKER|}');
      if not SaveStringToFile(Marker, IntToStr(FixtureCurrentProcessId) + ':ready', False) then
        RaiseException('Cannot signal the active guard termination probe.');
      Sleep(30000);
    end;
    // Exercise cancellation before usUninstall: no owned bytes are deleted.
    Result := False;
  end;
end;
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then begin
    DeleteVerifiedUninstallDocuments;
    if SaveStringToFile(ExpandConstant('{app}\README.txt'),
      'A new user file must not replace a delete-pending verified object.', False) then
      RaiseException('Verified deleted pathname was available to historical log deletion.');
  end;
end;
procedure DeinitializeUninstall;
begin
  ReleaseDocumentPreservation;
end;
'@
    Write-Fixture $oldScript ($common + "`r`n" + $oldFiles)
    Write-Fixture $newScript ($common + "`r`n" + $newFiles)
    & $iscc "/DStageDir=$oldStage" "/DOutputPath=$output" "/DOutputName=old" $oldScript
    Assert ($LASTEXITCODE -eq 0) "Historical Inno fixture failed to compile"
    $compileOutput = @(& $iscc "/DStageDir=$newStage" "/DOutputPath=$output" "/DOutputName=new" "/DDocumentRulesPath=$rules" "/DCleanupSource=$(Join-Path $repository 'Installer/LegacyDocumentCleanup.iss')" $newScript 2>&1)
    $compileExitCode = $LASTEXITCODE
    foreach ($line in $compileOutput) {
        Write-Host $line
        if ($line -match '^Compiler engine version: Inno Setup (\d+\.\d+\.\d+)') { $compilerVersion = $Matches[1] }
    }
    Assert ($compileExitCode -eq 0) "Current Inno document migration code failed to compile"
    Assert ($null -ne $compilerVersion) "The compiler did not identify its engine version"
    $nativeCompilerPassed = $true
    $oldSetup = Join-Path $output "old.exe"
    $newSetup = Join-Path $output "new.exe"
    Invoke-Setup $newSetup "01-fresh-install" $fresh
    Pass "Production parent handles block directory rename and preservation handles block deletion independently of external application locks"
    Assert ((Hash (Join-Path $fresh "README.txt")) -eq (Hash (Join-Path $newStage "README.txt"))) "Fresh payload mismatch"
    Invoke-Setup (Uninstaller) "02-fresh-uninstall" ""
    Assert (!(Test-Path -LiteralPath $registry)) "Fresh uninstall registration remains"
    Assert (Test-Path -LiteralPath (Join-Path $fresh "Documentation/THIRD_PARTY_NOTICES.md")) "Fresh uninstall removed license notices"
    foreach ($name in $legacyNames) {
        Assert (!(Test-Path -LiteralPath (Join-Path $fresh "Documentation/$name"))) "Fresh uninstall left a temporary historical-path guard"
    }
    Pass "Real Inno fresh installation and removal preserve license notices"

    Invoke-Setup $oldSetup "03-historical-install" $app
    $docs = Join-Path $app "Documentation"
    $modified = Join-Path $docs "AMP_NATIVE_DSP.md"
    Write-Fixture $modified "user-edited research notes`r`n"
    $modifiedHash = Hash $modified
    $sameSize = Join-Path $docs "POST_NATIVE_DSP.md"
    $sameBytes = [IO.File]::ReadAllBytes($sameSize)
    $sameBytes[0] = $sameBytes[0] -bxor 1
    [IO.File]::WriteAllBytes($sameSize, $sameBytes)
    $sameSizeHash = Hash $sameSize
    $manual = Join-Path $docs "MANUAL.html"
    Write-Fixture $manual "<p>User annotations before upgrade</p>`r`n"
    $manualHash = Hash $manual
    $personal = @("personal-preset.chimera", "personal-IR.wav", "existing-session.song", "Documentation/user-research.md")
    $personalHashes = @{}
    foreach ($name in $personal) {
        $path = Join-Path $app $name
        Write-Fixture $path "user-owned $name`r`n"
        $personalHashes[$name] = Hash $path
    }
    $outside = Join-Path $work "outside"
    New-Item -ItemType Directory -Path $outside | Out-Null
    $hardTarget = Join-Path $outside "hard.md"
    $hardPath = Join-Path $docs "PEDAL_BOARD_DSP.md"
    Copy-Item -LiteralPath $hardPath -Destination $hardTarget
    Remove-Item -LiteralPath $hardPath
    New-Item -ItemType HardLink -Path $hardPath -Target $hardTarget | Out-Null
    $hardHash = Hash $hardTarget
    $symlinkTarget = Join-Path $outside "symbolic.json"
    $symlinkPath = Join-Path $docs "ARTWORK_PROMPTS.json"
    Copy-Item -LiteralPath $symlinkPath -Destination $symlinkTarget
    Remove-Item -LiteralPath $symlinkPath
    New-Item -ItemType SymbolicLink -Path $symlinkPath -Target $symlinkTarget | Out-Null
    $symlinkHash = Hash $symlinkTarget
    $readonly = Join-Path $docs "GRAPHICS_DSP_UPDATE.md"
    (Get-Item -LiteralPath $readonly).IsReadOnly = $true
    $readOnlyHash = Hash $readonly
    $unknown = Join-Path $docs "VALIDATION_STATUS.md"
    Write-Fixture $unknown "unrecognized custom validation file`r`n"
    $unknownHash = Hash $unknown
    Invoke-Setup $newSetup "04-upgrade" $app
    Assert (!(Test-Path -LiteralPath (Join-Path $docs "AMP_VALIDATION.md"))) "Reviewed retired document remains"
    Assert (!(Test-Path -LiteralPath (Join-Path $docs "reference/microphone-tags.json"))) "Reviewed nested document remains"
    Assert ((Hash $modified) -eq $modifiedHash -and (Hash $sameSize) -eq $sameSizeHash) "Upgrade changed edited documents"
    Assert ((Hash $hardPath) -eq $hardHash -and (Hash $symlinkTarget) -eq $symlinkHash) "Upgrade changed a linked target"
    Assert ((Hash $readonly) -eq $readOnlyHash -and (Hash $unknown) -eq $unknownHash) "Upgrade changed readonly/unknown files"
    $manualSnapshot = Join-Path $app "PreservedDocuments/$manualHash/Documentation/MANUAL.html"
    Assert ((Hash $manualSnapshot) -eq $manualHash) "Modified current manual was not preserved byte-for-byte"
    Assert ((Hash $manual) -eq (Hash (Join-Path $newStage "MANUAL.html"))) "Upgrade did not install current manual"
    Assert-NoDeferredDocumentDelete
    Pass "Upgrade retires only exact hash+size identities; edits, same-size edits, unknown files, hardlinks, symlinks and readonly documents survive"

    $readme = Join-Path $app "README.txt"
    Write-Fixture $readme "damaged documentation fixture"
    $readmeHash = Hash $readme
    Invoke-Setup $newSetup "05-repair" $app
    Invoke-Setup $newSetup "06-repeated-repair" $app
    Assert ((Hash $readme) -eq (Hash (Join-Path $newStage "README.txt"))) "Repair did not restore current README"
    $readmeSnapshot = Join-Path $app "PreservedDocuments/$readmeHash/README.txt"
    Assert ((Hash $readmeSnapshot) -eq $readmeHash -and (Hash $manualSnapshot) -eq $manualHash) "Repair changed a preserved snapshot"
    Pass "Same-version repair and repeat repair restore owned documentation while retaining exact modified copies"

    # An existing directory junction is preserved during cleanup and prevents
    # historical uninstall traversal. This fail-closed result is intentional.
    $reference = Join-Path $docs "reference"
    $referenceSaved = Join-Path $docs "reference-saved"
    Rename-Item -LiteralPath $reference -NewName "reference-saved"
    $junctionTarget = Join-Path $outside "reference-target"
    New-Item -ItemType Directory -Path $junctionTarget | Out-Null
    $junctionFile = Join-Path $junctionTarget "microphone-tags.json"
    Copy-Item -LiteralPath (Join-Path $oldStage "reference/microphone-tags.json") -Destination $junctionFile
    $junctionHash = Hash $junctionFile
    New-Item -ItemType Junction -Path $reference -Target $junctionTarget | Out-Null
    Invoke-Setup $newSetup "07-junction-upgrade" $app
    Assert ((Hash $junctionFile) -eq $junctionHash) "Cleanup traversed a junction"
    Invoke-Setup (Uninstaller) "08-junction-uninstall-refused" "" $false
    Assert ((Hash $junctionFile) -eq $junctionHash -and (Test-Path -LiteralPath $registry)) "Unsafe uninstall was not refused before deletion"
    Assert-NoDeferredDocumentDelete
    (Get-Item -LiteralPath $reference -Force).Delete()
    Rename-Item -LiteralPath $referenceSaved -NewName "reference"
    Pass "Junction targets remain unchanged and uninstall refuses historical traversal before deleting files"

    $guardPaths = @((Join-Path $docs "AMP_VALIDATION.md"), (Join-Path $docs "reference/microphone-tags.json"))
    $beforeCancelReadme = Hash $readme
    # Force an absent parent as well as an absent file. Delete only the empty
    # directory owned by this synthetic fixture, never recurse into a tree.
    [IO.Directory]::Delete($reference, $false)
    Invoke-Setup (Uninstaller) "08a-cancelled-guard-uninstall" "" $false @("/DOCGUARDPROBE=cancel")
    Assert (Test-Path -LiteralPath $registry) "Cancelled uninstall removed registration"
    Assert ((Hash $readme) -eq $beforeCancelReadme -and (Hash $modified) -eq $modifiedHash) "Cancelled uninstall changed owned or user bytes"
    foreach ($path in $guardPaths) { Assert (!(Test-Path -LiteralPath $path)) "Cancelled uninstall left an empty guard file" }
    Assert (!(Test-Path -LiteralPath $reference)) "Cancelled uninstall left its newly created empty guard directory"
    Assert-NoDeferredDocumentDelete

    $marker = Join-Path $outside "guard-process.txt"
    $arguments = @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/SP-", "/DOCGUARDPROBE=hold",
        "/DOCGUARDMARKER=`"$marker`"", "/LOG=`"$logs/08b-terminated-guard-uninstall.log`"")
    $guardProcess = Start-Process -FilePath (Uninstaller) -ArgumentList $arguments -PassThru
    $installerInvocations += 1
    $phaseProcess = $null
    try {
        $phaseProcessId = 0
        for ($attempt = 0; $attempt -lt 100; ++$attempt) {
            if ((Test-Path -LiteralPath $marker) -and (Get-Content -LiteralPath $marker -Raw) -match '^([1-9][0-9]*):ready$') {
                $phaseProcessId = [int]$Matches[1]
                break
            }
            Start-Sleep -Milliseconds 100
        }
        Assert ($phaseProcessId -gt 0) "The actual uninstaller did not acquire its absent-path guards before termination"
        $phase = Get-CimInstance Win32_Process -Filter "ProcessId=$phaseProcessId"
        Assert ($null -ne $phase -and $phase.CommandLine.Contains($app, [StringComparison]::OrdinalIgnoreCase)) "Refusing to terminate a process outside this exact fixture installation"
        $phaseProcess = [Diagnostics.Process]::GetProcessById($phaseProcessId)
        & taskkill.exe /F /T /PID $phaseProcessId | Out-Host
        Assert ($LASTEXITCODE -eq 0) "Cannot terminate the guarded native uninstaller fixture"
        Assert ($phaseProcess.WaitForExit(10000)) "The guarded native uninstaller did not finish termination"
    } finally {
        if (!$guardProcess.HasExited) { $guardProcess.Kill($true); $guardProcess.WaitForExit() }
        if ($null -ne $phaseProcess) { $phaseProcess.Dispose() }
        $guardProcess.Dispose()
    }
    Assert (Test-Path -LiteralPath $registry) "Terminated pre-confirmation uninstall removed registration"
    Assert ((Hash $readme) -eq $beforeCancelReadme -and (Hash $modified) -eq $modifiedHash) "Terminated pre-confirmation uninstall changed owned or user bytes"
    Assert (@(Get-ChildItem -LiteralPath $reference -File -Recurse).Count -eq 0) "Terminated guard left document files in its newly created parent"
    foreach ($path in $guardPaths) {
        Assert (!(Test-Path -LiteralPath $path)) "Kernel delete-on-close did not remove its own empty guard after process termination"
        # Prove the pathname can be created again once the guard owner exits.
        Write-Fixture $path "creation after terminated guard release"
        Remove-Item -LiteralPath $path
    }
    Assert-NoDeferredDocumentDelete
    $guardCancellationAndTerminationVerified = $true
    Pass "Absent historical path guards prevent creation during uninstall, disappear on cancellation and forced process termination, and never queue reboot deletion"

    # A concurrent writer prevents even the read-only preservation handle:
    # our FILE_SHARE_READ intentionally does not share the writer's access.
    $lock = [IO.File]::Open($modified, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try { Invoke-Setup (Uninstaller) "09-locked-uninstall-refused" "" $false }
    finally { $lock.Dispose() }
    Assert ((Hash $modified) -eq $modifiedHash) "Refused uninstall changed edited bytes"
    Assert (Test-Path -LiteralPath $registry) "Refused uninstall removed registration"
    Assert-NoDeferredDocumentDelete
    Pass "An exclusive writer refuses uninstall before deleting files or registration when a preservation handle cannot be acquired"
    # Edit a currently shipped document after repair: old and new log entries
    # must both leave its current in-place bytes untouched during uninstall.
    Write-Fixture $manual "<p>New user annotations after repair</p>`r`n"
    $finalManualHash = Hash $manual
    # A reader that shares READ but denies DELETE exercises the production
    # read-only fallback. The fallback must provide its own deny-delete lock.
    $lock = [IO.File]::Open($modified, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try { Invoke-Setup (Uninstaller) "10-upgraded-uninstall-with-reader" "" }
    finally { $lock.Dispose() }
    Assert (!(Test-Path -LiteralPath $registry)) "Upgraded uninstall registration remains"
    foreach ($pair in @(@($modified, $modifiedHash), @($sameSize, $sameSizeHash), @($hardPath, $hardHash),
        @($readonly, $readOnlyHash), @($unknown, $unknownHash), @($manual, $finalManualHash),
        @($manualSnapshot, $manualHash), @($readmeSnapshot, $readmeHash))) {
        Assert ((Hash $pair[0]) -eq $pair[1]) "Historical uninstall deleted or changed preserved document $($pair[0])"
    }
    Assert ((Get-Item -LiteralPath $symlinkPath -Force).LinkType -eq "SymbolicLink") "Historical uninstall removed the user's symbolic link"
    Assert ((Hash $symlinkTarget) -eq $symlinkHash) "Historical uninstall changed symbolic link target"
    Assert (Test-Path -LiteralPath (Join-Path $docs "THIRD_PARTY_NOTICES.md")) "Historical uninstall removed license notices"
    foreach ($name in $personal) {
        Assert ((Hash (Join-Path $app $name)) -eq $personalHashes[$name]) "Uninstall changed user file $name"
    }
    Assert-NoDeferredDocumentDelete
    Pass "Appended historical uninstall logs preserve edited docs in place, snapshots, licenses, linked files, presets, IRs and projects without reboot deletion"
    $success = $true
} catch {
    $checks.Add("FAIL: $($_.Exception.Message)")
    throw
} finally {
    @{ success = $success; platform = "windows"; native_inno_execution = ($installerInvocations -gt 0);
        native_compiler_passed = $nativeCompilerPassed; installer_invocations = $installerInvocations;
        compiler_version = $compilerVersion;
        guard_cancellation_and_termination_verified = $guardCancellationAndTerminationVerified;
        forced_termination_may_leave_empty_guard_directories = $true;
        historical_release_installer_executed = $false; scope = "real Inno installers with synthetic historical document ownership";
        source_sha = (& git -C $repository rev-parse HEAD).Trim(); run_id = $env:GITHUB_RUN_ID;
        checks = @($checks.ToArray()) } | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath (Join-Path $logs "DocumentUpgradeVerification.json") -Encoding utf8
    $checks | Set-Content -LiteralPath (Join-Path $logs "DocumentUpgradeVerification.txt") -Encoding utf8
}
