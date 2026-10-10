param([string]$OutputPath = "build/windows-runner-context.json")
$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $false
Set-StrictMode -Version Latest
if (!$IsWindows) { throw "Windows runner context requires Windows PowerShell Core." }

# Observation only: never change power policy, affinity, priority or test load.
# Unavailable optional probes are recorded explicitly, not inferred or invented.
function Observe([scriptblock]$Probe) {
    try { return [ordered]@{ status = "available"; value = (& $Probe) } }
    catch { return [ordered]@{ status = "unavailable"; error = $_.Exception.Message } }
}

$sourceRevision = (& git -C $PSScriptRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw "Cannot resolve the checked-out source revision." }
$report = [ordered]@{
    schema = "chimera.windows-runner-context.v1"
    observed_at_utc = [DateTime]::UtcNow.ToString('o')
    source_commit = $sourceRevision
    workflow = $env:GITHUB_WORKFLOW
    run_id = $env:GITHUB_RUN_ID
    run_attempt = $env:GITHUB_RUN_ATTEMPT
    job = $env:GITHUB_JOB
    runner_os = $env:RUNNER_OS
    runner_arch = $env:RUNNER_ARCH
    image_os = $env:ImageOS
    image_version = $env:ImageVersion
    process_logical_processor_count = [Environment]::ProcessorCount
    processors = Observe {
        @(Get-CimInstance Win32_Processor -OperationTimeoutSec 15 |
            Select-Object Name, Manufacturer, NumberOfCores, NumberOfLogicalProcessors,
                MaxClockSpeed, CurrentClockSpeed)
    }
    operating_system = Observe {
        Get-CimInstance Win32_OperatingSystem -OperationTimeoutSec 15 |
            Select-Object Caption, Version, BuildNumber, OSArchitecture
    }
    computer_system = Observe {
        Get-CimInstance Win32_ComputerSystem -OperationTimeoutSec 15 |
            Select-Object Manufacturer, Model, HypervisorPresent, TotalPhysicalMemory
    }
    active_power_scheme = Observe {
        $output = (& powercfg.exe /GETACTIVESCHEME 2>&1 | Out-String).Trim()
        if ($LASTEXITCODE -ne 0) { throw "powercfg query failed: $output" }
        $output
    }
    collection_process = Observe {
        $process = Get-Process -Id $PID
        [ordered]@{
            priority_class = [string]$process.PriorityClass
            processor_affinity_hex = "0x" + $process.ProcessorAffinity.ToInt64().ToString('X')
        }
    }
    clocks = [ordered]@{
        stopwatch_frequency_hz = [Diagnostics.Stopwatch]::Frequency
        stopwatch_high_resolution = [Diagnostics.Stopwatch]::IsHighResolution
        # CIM MHz values are metadata, not a valid thread-cycle/time conversion.
        processor_clock_units = "MHz as reported by Win32_Processor"
    }
}
$fullPath = [IO.Path]::GetFullPath($OutputPath)
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($fullPath)) | Out-Null
$json = $report | ConvertTo-Json -Depth 8
[IO.File]::WriteAllText($fullPath, $json + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
Write-Host "Windows execution environment (read-only observation):"
Write-Host $json
# Handled optional query failures are report data, not a stale native exit code.
# Source-resolution and report-write failures still terminate before this point.
exit 0
