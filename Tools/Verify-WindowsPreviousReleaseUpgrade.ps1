param(
    [Parameter(Mandatory = $true)][string]$Installer,
    [Parameter(Mandatory = $true)][string]$Stage,
    [string]$LogDirectory = "build/installer-verification"
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
# This installs a real public product release. Never run against a workstation.
if ($env:GITHUB_ACTIONS -ne "true" -or !$IsWindows -or ![Environment]::Is64BitProcess -or !$env:RUNNER_TEMP) {
    throw "Previous-release upgrade verification requires a disposable x64 Windows GitHub runner."
}
$repository = Split-Path -Parent $PSScriptRoot
$installerPath = (Resolve-Path -LiteralPath $Installer).Path
$stagePath = (Resolve-Path -LiteralPath $Stage).Path
New-Item -ItemType Directory -Force -Path $LogDirectory | Out-Null
$logs = (Resolve-Path -LiteralPath $LogDirectory).Path
$work = Join-Path $env:RUNNER_TEMP ("Chimera public upgrade " + [Guid]::NewGuid().ToString("N"))
$app = Join-Path $work "이전 버전 업그레이드"
$docs = Join-Path $app "Documentation"
$registry = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\SpectralForge.ChimeraAmpMatrix_is1"
$startMenu = Join-Path ([Environment]::GetFolderPath("CommonPrograms")) "SpectralForge/SpectralForge Chimera"
$sharedIRs = Join-Path ([Environment]::GetFolderPath("CommonApplicationData")) "SpectralForge/Chimera/IRs"
foreach ($path in @($work, $registry, (Join-Path $startMenu "SpectralForge Chimera.lnk"))) {
    if (Test-Path -LiteralPath $path) { throw "Refusing to overwrite an existing installation or fixture: $path" }
}
New-Item -ItemType Directory -Path $work | Out-Null
$previousTag = "v1.3.1-beta.1"
$previousName = "SpectralForge-Chimera-1.3.1-beta.1-win64-Setup.exe"
$previousUrl = "https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/$previousTag/$previousName"
$previousExpectedHash = "da0a0c7f70b2ab6f5e0854ba9180a2eb3a8ab847c31c169db9e82c91002ff41a"
$previousExpectedSize = 210803143
$previousSetup = Join-Path $work $previousName
$payload = Get-Content -LiteralPath (Join-Path $stagePath "payload-manifest.json") -Raw | ConvertFrom-Json
$inventory = Get-Content -LiteralPath (Join-Path $repository "Validation/legacy-document-inventory.json") -Raw | ConvertFrom-Json
$checks = [Collections.Generic.List[string]]::new()
$observedDocuments = [Collections.Generic.List[object]]::new()
$observedVersions = [Collections.Generic.List[string]]::new()
$success = $false
$archiveVerified = $false
$previousInstallerExecuted = $false
$previousDocumentsVerified = $false
$installerInvocations = 0
$uninstallPath = $null
$personalHashes = @{}
$licenseHashes = @{}
function Assert([bool]$Condition, [string]$Message) { if (!$Condition) { throw $Message } }
function Pass([string]$Message) { $checks.Add("PASS: $Message"); Write-Host "PASS: $Message" }
function Hash([string]$Path) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Installed-Document([string]$RelativePath) {
    $base = if ([IO.Path]::GetExtension($RelativePath) -eq ".txt" -and !$RelativePath.Contains("/")) { $app } else { $docs }
    return Join-Path $base $RelativePath
}
function Write-Personal([string]$Path, [string]$Contents) {
    Assert (!(Test-Path -LiteralPath $Path)) "Personal sentinel already exists: $Path"
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    [IO.File]::WriteAllText($Path, $Contents, [Text.UTF8Encoding]::new($false))
    $personalHashes[$Path] = Hash $Path
}
function Assert-Personal {
    foreach ($path in $personalHashes.Keys) { Assert ((Hash $path) -eq $personalHashes[$path]) "Installation changed user bytes: $path" }
}
function Assert-NoDeferredDocumentDelete {
    $entry = Get-ItemProperty -LiteralPath "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager"
    foreach ($property in @("PendingFileRenameOperations", "PendingFileRenameOperations2")) {
        if ($entry.PSObject.Properties.Name -contains $property) {
            foreach ($operation in $entry.$property) {
                Assert (!$operation.Contains($work, [StringComparison]::OrdinalIgnoreCase)) "A document was queued for deletion on restart: $operation"
            }
        }
    }
}
function Invoke-ProductSetup([string]$Executable, [string]$Name, [bool]$Uninstall = $false) {
    $arguments = @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/SP-", "/LOG=`"$logs/$Name.log`"")
    if (!$Uninstall) { $arguments += @("/LANG=korean", "/TYPE=custom", "/COMPONENTS=standalone", "/TASKS=", "/DIR=`"$app`"") }
    # -Wait includes Inno's second-phase process tree; never test state while
    # the actual uninstaller is still executing behind its launcher.
    $process = Start-Process -FilePath $Executable -ArgumentList $arguments -PassThru -Wait
    $script:installerInvocations += 1
    if ($Executable -eq $previousSetup) { $script:previousInstallerExecuted = $true }
    Assert ($process.ExitCode -eq 0) "$Name failed with exit code $($process.ExitCode)"
}
function Assert-Registration([string]$ExpectedVersion) {
    Assert (Test-Path -LiteralPath $registry) "Stable Chimera uninstall registration is missing"
    $entry = Get-ItemProperty -LiteralPath $registry
    Assert ($entry.DisplayName -ceq "SpectralForge Chimera") "The upgrade changed the product AppId/name"
    Assert ($entry.DisplayVersion -ceq $ExpectedVersion) "Unexpected registered version: $($entry.DisplayVersion)"
    Assert ($entry.InstallLocation.TrimEnd([char]92) -eq $app.TrimEnd([char]92)) "The upgrade changed the Unicode installation directory"
    $path = $entry.UninstallString.Trim('"')
    Assert ($path.StartsWith((Join-Path $app "Uninstall") + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) "Unexpected uninstaller path: $path"
    Assert (Test-Path -LiteralPath $path -PathType Leaf) "The registered uninstaller is missing"
    if ($script:uninstallPath) { Assert ($path -eq $script:uninstallPath) "Upgrade replaced the stable uninstall-log location" }
    else { $script:uninstallPath = $path }
    $observedVersions.Add($entry.DisplayVersion)
}
function Current-Documents {
    return @(Get-ChildItem -LiteralPath $stagePath -File | Where-Object {
        $_.Extension -in ".txt", ".md", ".html" -or $_.Name -eq "payload-manifest.json"
    })
}
function Assert-CurrentPayload {
    foreach ($file in Current-Documents) {
        Assert ((Hash (Installed-Document $file.Name)) -eq (Hash $file.FullName)) "Current documentation was not installed: $($file.Name)"
    }
    Assert ((Hash (Join-Path $app "SpectralForge Chimera.exe")) -eq (Hash (Join-Path $stagePath "Standalone/SpectralForge Chimera.exe"))) "Current standalone binary differs from the validated payload"
    Assert (!(Test-Path -LiteralPath (Join-Path $app "ReferenceTools"))) "Unselected reference tools were installed"
    Assert (Test-Path -LiteralPath (Join-Path $startMenu "SpectralForge Chimera.lnk")) "Current standalone shortcut is missing"
}
try {
    & python (Join-Path $PSScriptRoot "chimera_version.py") --manifest (Join-Path $stagePath "payload-manifest.json")
    Assert ($LASTEXITCODE -eq 0) "Current product payload/source contract failed"
    Assert ($inventory.schema -ceq "spectralforge.legacy-document-inventory.v1") "Unsupported historical evidence schema"
    # The fixed official asset is executable only after its public release
    # size and SHA256 agree. Never execute a URL or hash from installed files.
    Invoke-WebRequest -Uri $previousUrl -OutFile $previousSetup -MaximumRedirection 5 -TimeoutSec 180
    Assert ((Get-Item -LiteralPath $previousSetup).Length -eq $previousExpectedSize) "Previous public Setup size differs from the reviewed release"
    Assert ((Hash $previousSetup) -ceq $previousExpectedHash) "Previous public Setup SHA256 differs from the reviewed release"
    $archiveVerified = $true
    Pass "Official $previousTag Setup archive size and SHA256 match the reviewed public release"

    # Existing user IRs from the separate current-product checks are also
    # sentinels: every byte must remain unchanged through this real upgrade.
    if (Test-Path -LiteralPath $sharedIRs) {
        foreach ($file in Get-ChildItem -LiteralPath $sharedIRs -Recurse -File) { $personalHashes[$file.FullName] = Hash $file.FullName }
    }
    Invoke-ProductSetup $previousSetup "08-previous-release-install"
    Assert-Registration "1.3.1"
    $latestFiles = @()
    foreach ($file in $inventory.files) {
        if ($file.platforms -notcontains "windows") { continue }
        $variants = @($file.variants | Where-Object { $_.platforms -contains "windows" -and $_.releases -contains $previousTag })
        if (!$variants.Count) { continue }
        Assert ($variants.Count -eq 1) "Ambiguous previous Windows document identity: $($file.path)"
        $path = Installed-Document $file.path
        $actual = Get-Item -LiteralPath $path
        $actualHash = Hash $path
        Assert ($actual.Length -eq $variants[0].size -and $actualHash -ceq $variants[0].sha256) "Actual previous Setup document differs from release-archive evidence: $($file.path)"
        $observedDocuments.Add(@{ path = $file.path; bytes = $actual.Length; sha256 = $actualHash })
        $latestFiles += $file
        if ($file.preserve_always) { $licenseHashes[$file.path] = $actualHash }
    }
    $actualDocuments = @(Get-ChildItem -LiteralPath $app -File | Where-Object Extension -eq ".txt") + @(Get-ChildItem -LiteralPath $docs -Recurse -File)
    Assert ($observedDocuments.Count -eq 24 -and $actualDocuments.Count -eq 24) "The actual previous Setup document list does not match the 24 reviewed Windows files"
    $previousDocumentsVerified = $true
    Assert-Personal
    Pass "Actual $previousTag Setup installs exactly 24 documentation files; every raw size and SHA256 matches historical package evidence"

    $modified = Join-Path $docs "AMP_NATIVE_DSP.md"
    [IO.File]::AppendAllText($modified, "`r`nPersonal research annotations retained by upgrade.`r`n", [Text.UTF8Encoding]::new($false))
    $modifiedHash = Hash $modified
    $manual = Join-Path $docs "MANUAL.html"
    [IO.File]::AppendAllText($manual, "`r`n<p>Personal manual annotations before upgrade.</p>`r`n", [Text.UTF8Encoding]::new($false))
    $manualHash = Hash $manual
    $manualSnapshot = Join-Path $app "PreservedDocuments/$manualHash/Documentation/MANUAL.html"
    foreach ($name in @("UserFiles/개인 프리셋.chimera", "UserFiles/personal-cab.wav", "Sessions/existing-project.song", "Documentation/personal-research.md")) {
        Write-Personal (Join-Path $app $name) "User-owned sentinel: $name`r`n"
    }
    Write-Personal (Join-Path $sharedIRs ("document-upgrade-" + [Guid]::NewGuid().ToString("N") + ".wav")) "Personal IR sentinel outside the application directory.`r`n"
    Invoke-ProductSetup $installerPath "09-previous-release-upgrade"
    Assert-Registration $payload.installer_version
    Assert-CurrentPayload
    $retiredCount = 0
    foreach ($file in $latestFiles) {
        if (!$file.retire -or $file.path -eq "AMP_NATIVE_DSP.md") { continue }
        Assert (!(Test-Path -LiteralPath (Installed-Document $file.path))) "Verified historical document remains after the actual product upgrade: $($file.path)"
        $retiredCount += 1
    }
    Assert ($retiredCount -eq 9) "Unexpected actual-release document retirement coverage"
    Assert ((Hash $modified) -eq $modifiedHash) "Actual product upgrade changed the edited historical document"
    Assert ((Hash $manualSnapshot) -eq $manualHash) "Actual product upgrade failed to preserve the annotated previous manual"
    foreach ($name in $licenseHashes.Keys) {
        $relative = [IO.Path]::GetRelativePath($app, (Installed-Document $name))
        Assert ((Hash (Join-Path $app "PreservedDocuments/$($licenseHashes[$name])/$relative")) -eq $licenseHashes[$name]) "Previous license notice was not preserved byte-for-byte: $name"
    }
    Assert-Personal
    Assert-NoDeferredDocumentDelete
    Pass "Real $previousTag to $($payload.version) upgrade retires nine exact owned research documents, preserves an edited legacy document in place and saves the annotated manual and license notices"

    $readme = Join-Path $app "README.txt"
    [IO.File]::AppendAllText($readme, "`r`nPersonal repair annotations.`r`n", [Text.UTF8Encoding]::new($false))
    $readmeHash = Hash $readme
    $readmeSnapshot = Join-Path $app "PreservedDocuments/$readmeHash/README.txt"
    foreach ($name in @("10-previous-release-repair", "11-previous-release-repeat-repair")) {
        Invoke-ProductSetup $installerPath $name
        Assert-Registration $payload.installer_version
        Assert-CurrentPayload
        Assert ((Hash $modified) -eq $modifiedHash -and (Hash $manualSnapshot) -eq $manualHash -and (Hash $readmeSnapshot) -eq $readmeHash) "Actual product repair changed preserved documentation"
        Assert-Personal
    }
    Pass "Two actual current-Setup repairs restore current payload bytes and preserve all annotated document snapshots and personal files"

    [IO.File]::AppendAllText($manual, "`r`n<p>Personal annotations made after repair.</p>`r`n", [Text.UTF8Encoding]::new($false))
    $finalManualHash = Hash $manual
    Invoke-ProductSetup $uninstallPath "12-previous-release-upgraded-uninstall" $true
    Assert (!(Test-Path -LiteralPath $registry)) "Actual upgraded uninstall registration remains"
    Assert (!(Test-Path -LiteralPath (Join-Path $app "SpectralForge Chimera.exe"))) "Actual upgraded uninstall left its owned standalone binary"
    Assert (!(Test-Path -LiteralPath (Join-Path $startMenu "SpectralForge Chimera.lnk"))) "Actual upgraded uninstall left its app shortcut"
    Assert ((Hash $modified) -eq $modifiedHash -and (Hash $manual) -eq $finalManualHash) "Actual historical uninstall records deleted edited documents"
    Assert ((Hash $manualSnapshot) -eq $manualHash -and (Hash $readmeSnapshot) -eq $readmeHash) "Actual historical uninstall records deleted preserved snapshots"
    foreach ($file in Current-Documents) {
        if ($file.Name -eq "MANUAL.html") { continue }
        if ($licenseHashes.ContainsKey($file.Name)) {
            Assert ((Hash (Installed-Document $file.Name)) -eq (Hash $file.FullName)) "Actual product uninstall removed license notices: $($file.Name)"
        } else { Assert (!(Test-Path -LiteralPath (Installed-Document $file.Name))) "Actual product uninstall left an unmodified owned current document: $($file.Name)" }
    }
    foreach ($name in $licenseHashes.Keys) {
        $relative = [IO.Path]::GetRelativePath($app, (Installed-Document $name))
        Assert ((Hash (Join-Path $app "PreservedDocuments/$($licenseHashes[$name])/$relative")) -eq $licenseHashes[$name]) "Actual product uninstall changed a previous license notice: $name"
    }
    Assert-Personal
    Assert-NoDeferredDocumentDelete
    Pass "Actual previous-release uninstall ownership survives upgrade/repair and preserves edited documents, snapshots, licenses, presets, IRs and projects without reboot deletion"
    $success = $true
} catch {
    $checks.Add("FAIL: $($_.Exception.Message)")
    throw
} finally {
    @{ success = $success; actual_previous_release_upgrade_verified = $success;
        historical_release_installer_executed = $previousInstallerExecuted;
        previous_release_archive_verified = $archiveVerified; previous_installed_documents_verified = $previousDocumentsVerified;
        scope = "actual public beta.1 Setup to current product Setup, standalone component, upgrade/repair/uninstall";
        platform = "windows"; source_sha = $payload.source_sha; run_id = $env:GITHUB_RUN_ID;
        previous_release = $previousTag; previous_installer_url = $previousUrl;
        previous_installer_expected_sha256 = $previousExpectedHash; previous_installer_expected_bytes = $previousExpectedSize;
        installer = [IO.Path]::GetFileName($installerPath); installer_sha256 = (Hash $installerPath);
        version = $payload.version; product_version = $payload.product_version; installer_version = $payload.installer_version;
        installer_invocations = $installerInvocations; observed_display_versions = @($observedVersions.ToArray());
        actual_install_directory = $app; actual_uninstall_registration = $registry;
        observed_previous_documents = @($observedDocuments.ToArray()); checks = @($checks.ToArray()) } | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath (Join-Path $logs "PreviousReleaseUpgradeVerification.json") -Encoding utf8
    $checks | Set-Content -LiteralPath (Join-Path $logs "PreviousReleaseUpgradeVerification.txt") -Encoding utf8
}
