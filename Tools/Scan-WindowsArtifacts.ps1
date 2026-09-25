param(
    [Parameter(Mandatory = $true)][string[]]$Path,
    [string]$ReportDirectory = "build/windows-security"
)
$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $false
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -ne "true" -or !$IsWindows) {
    throw "This scan is restricted to disposable Windows GitHub runners."
}
New-Item -ItemType Directory -Force -Path $ReportDirectory | Out-Null
$reportRoot = (Resolve-Path -LiteralPath $ReportDirectory).Path
$reportFile = Join-Path $reportRoot "WindowsSecurity.txt"
$lines = [Collections.Generic.List[string]]::new()
function Note([string]$Message) {
    $lines.Add($Message)
    Write-Host $Message
    $lines | Set-Content -LiteralPath $reportFile -Encoding utf8
}
function Defender-Command([string[]]$Arguments, [string]$LogName) {
    $output = & $script:mpCmdRun @Arguments 2>&1
    $result = $LASTEXITCODE
    $output | Set-Content -LiteralPath (Join-Path $reportRoot $LogName) -Encoding utf8
    $output | ForEach-Object { Write-Host "$_" }
    return $result
}
function Inventory([string[]]$Roots) {
    $files = foreach ($root in $Roots) {
        if (Test-Path -LiteralPath $root -PathType Container) {
            Get-ChildItem -LiteralPath $root -Recurse -File
        } else { Get-Item -LiteralPath $root }
    }
    foreach ($file in ($files | Sort-Object FullName -Unique)) {
        $signature = if ($file.Extension -in ".exe", ".dll", ".vst3", ".msix") {
            [string](Get-AuthenticodeSignature -LiteralPath $file.FullName).Status
        } else { "NotApplicable" }
        [pscustomobject]@{
            Path = $file.FullName
            Bytes = $file.Length
            SHA256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            Authenticode = $signature
        }
    }
}
Note "RavenForge Luthier Intelligence / Windows artifact malware inspection"
Note "Started UTC: $([DateTime]::UtcNow.ToString('o'))"
Note "Source revision: $env:GITHUB_SHA"
Note "Scope: local Microsoft Defender scan. Not Google Safe Browsing clearance, Store certification, or a guarantee of safety."
try {
    $roots = @($Path | ForEach-Object { (Resolve-Path -LiteralPath $_).Path })
    if (!$roots.Count) { throw "No scan inputs." }
    foreach ($root in $roots) {
        if ($reportRoot -eq $root -or $reportRoot.StartsWith($root.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw "Reports must be outside the scan inputs."
        }
    }
    $before = @(Inventory $roots)
    if (!$before.Count) { throw "Scan inputs contain no files." }
    $before | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $reportRoot "FileInventory.json") -Encoding utf8
    Note "Input files: $($before.Count); SHA256 and signature status recorded in FileInventory.json."
    $platform = Join-Path $env:ProgramData "Microsoft/Windows Defender/Platform"
    $candidates = @(Get-ChildItem -LiteralPath $platform -Filter MpCmdRun.exe -Recurse -File -ErrorAction SilentlyContinue | Sort-Object FullName -Descending)
    $script:mpCmdRun = if ($candidates.Count) { $candidates[0].FullName } else { Join-Path $env:ProgramFiles "Windows Defender/MpCmdRun.exe" }
    if (!(Test-Path -LiteralPath $mpCmdRun)) { throw "Microsoft Defender command-line scanner is unavailable." }
    $service = Get-Service WinDefend
    if ($service.Status -ne "Running") { Start-Service WinDefend }
    $status = Get-MpComputerStatus
    if (!$status.AMServiceEnabled) { throw "Microsoft Defender scanning service is not active." }
    # Hosted runner images can disable archive/script scans for build speed.
    # Enable those scans; never disable security or add exclusions here.
    Set-MpPreference -DisableArchiveScanning $false -DisableScriptScanning $false -PUAProtection Enabled
    $preferences = Get-MpPreference
    if ($preferences.DisableArchiveScanning -or $preferences.DisableScriptScanning -or $preferences.PUAProtection -ne 1) {
        throw "Required scan settings did not take effect."
    }
    $update = Defender-Command @("-SignatureUpdate", "-MMPC") "SignatureUpdate.log"
    if ($update -ne 0) { throw "Defender security intelligence update failed: $update" }
    $status = Get-MpComputerStatus
    $status | Select-Object AMEngineVersion, AMProductVersion, AMServiceEnabled, AMRunningMode,
        AntivirusSignatureVersion, AntivirusSignatureLastUpdated, RealTimeProtectionEnabled |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $reportRoot "DefenderStatus.json") -Encoding utf8
    if (!$status.AMServiceEnabled -or !$status.AMEngineVersion -or !$status.AntivirusSignatureVersion -or
        ([DateTime]::UtcNow - $status.AntivirusSignatureLastUpdated.ToUniversalTime()).TotalDays -gt 2) {
        throw "Defender is not ready with current security intelligence."
    }
    Note "Engine: $($status.AMEngineVersion); signatures: $($status.AntivirusSignatureVersion); updated: $($status.AntivirusSignatureLastUpdated.ToUniversalTime().ToString('o'))"
    Note "Manual custom scans use -DisableRemediation: ignore path exclusions, scan archives, report detections without altering input files."
    Note "Runner cloud reporting mode: $($preferences.MAPSReporting); automatic sample submission: $($preferences.SubmitSamplesConsent). These settings are not changed."
    $number = 0
    $failures = 0
    foreach ($root in $roots) {
        ++$number
        $result = Defender-Command @("-Scan", "-ScanType", "3", "-File", $root, "-DisableRemediation") "Scan-$number.log"
        if ($result -ne 0) {
            ++$failures
            Note "BLOCKED: scan exit $result for $root. See Scan-$number.log for detection/error details."
        } else { Note "PASS: Defender completed scan with no detections: $root" }
    }
    $after = @(Inventory $roots)
    $difference = Compare-Object $before $after -Property Path, Bytes, SHA256
    if ($difference) { throw "Files changed or disappeared during the scan; the result cannot approve these artifacts." }
    if ($failures) { throw "$failures scan(s) reported detections or errors. Distribution remains blocked." }
    Note "PASS: all input hashes unchanged after scanning."
    Note "PASS: completed local Defender inspection. Unsigned files remain unsigned and browser warnings may remain."
} catch {
    Note "FAIL: $($_.Exception.Message)"
    throw
} finally {
    Note "Finished UTC: $([DateTime]::UtcNow.ToString('o'))"
    if ($env:GITHUB_STEP_SUMMARY) {
        "### Windows artifact inspection`n`n``````text`n$($lines -join "`n")`n```````n" |
            Add-Content -LiteralPath $env:GITHUB_STEP_SUMMARY -Encoding utf8
    }
}
