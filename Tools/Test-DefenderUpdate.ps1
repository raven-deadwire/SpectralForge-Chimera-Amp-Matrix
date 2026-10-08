$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
# Load only the production retry function through the PowerShell AST.
# No scanner entry point, Defender preferences, service or real network is mocked away in CI scanning.
$tokens = $null
$errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    (Join-Path $PSScriptRoot "Scan-WindowsArtifacts.ps1"), [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw "Scanner syntax errors: $errors" }
$function = $ast.Find({ param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
    $node.Name -eq "Update-DefenderIntelligence"
}, $true)
if (!$function) { throw "Retry function missing" }
Invoke-Expression $function.Extent.Text
function Note([string]$Message) {}
function Start-Sleep([int]$Seconds) { $script:delays.Add($Seconds) }
function Defender-Command([string[]]$Arguments, [string]$LogName) {
    if (($Arguments -join " ") -ne "-SignatureUpdate -MMPC") { throw "Changed update source" }
    $script:calls++
    $response = $script:responses[$script:calls - 1]
    $response.Log | Set-Content (Join-Path $reportRoot $LogName)
    if ($response.Throw) { throw "Synthetic command launch failure" }
    return $response.Code
}
$temp = Join-Path ([IO.Path]::GetTempPath()) ("defender-contract-" + [Guid]::NewGuid())
New-Item -ItemType Directory $temp | Out-Null
try {
    $cases = @(
        @{ Name="first success"; Codes=@(0); Expected=1; Delays=@(); Fail=$false },
        @{ Name="timeout recovery"; Codes=@(2,0); Expected=2; Delays=@(15); Fail=$false },
        @{ Name="third attempt recovery"; Codes=@(2,2,0); Expected=3; Delays=@(15,30); Fail=$false },
        @{ Name="exhausted timeout"; Codes=@(2,2,2); Expected=3; Delays=@(15,30); Fail=$true },
        @{ Name="unknown error"; Codes=@(7,7,7); Expected=3; Delays=@(15,30); Fail=$true },
        @{ Name="launch error"; Codes=@(9,9,9); Expected=3; Delays=@(15,30); Fail=$true }
    )
    foreach ($case in $cases) {
        $reportRoot = Join-Path $temp $case.Name
        New-Item -ItemType Directory $reportRoot | Out-Null
        $script:calls = 0
        $script:delays = [Collections.Generic.List[int]]::new()
        $script:responses = @($case.Codes | ForEach-Object {
            @{ Code=$_; Log=$(if ($_ -eq 2) { "failed hr=0x80072ee2" } else { "result $_" }); Throw=($case.Name -eq "launch error") }
        })
        $failed = $false
        try { Update-DefenderIntelligence } catch {
            if ($_.Exception.Message -notmatch "^UPDATE_FAILED:.*Malware scan NOT_RUN") { throw }
            $failed = $true
        }
        if ($failed -ne $case.Fail -or $script:calls -ne $case.Expected -or
            ($script:delays -join ",") -ne ($case.Delays -join ",")) { throw "Policy mismatch: $($case.Name)" }
        $records = @(Get-Content (Join-Path $reportRoot "SignatureUpdateAttempts.json") -Raw | ConvertFrom-Json)
        if ($records.Count -ne $case.Expected) { throw "Missing attempt evidence" }
        for ($i=0; $i -lt $records.Count; ++$i) {
            if (!(Test-Path (Join-Path $reportRoot $records[$i].Log))) { throw "Missing original log" }
            $expectedCause = if ($case.Name -eq "launch error") { "update_command_error" }
                elseif ($case.Codes[$i] -eq 0) { "update_completed" }
                elseif ($case.Codes[$i] -eq 2) { "update_network_timeout" } else { "update_error" }
            if ($records[$i].Cause -ne $expectedCause) { throw "Cause mismatch" }
        }
        Write-Host "PASS: $($case.Name)"
    }
} finally { Remove-Item -LiteralPath $temp -Recurse -Force }
