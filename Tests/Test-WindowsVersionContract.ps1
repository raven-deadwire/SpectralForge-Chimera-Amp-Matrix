$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot "../Tools/Windows-VersionContract.ps1")
$juce = @{
    FileMajorPart = 2; FileMinorPart = 17; FileBuildPart = 4; FilePrivatePart = 0
    ProductMajorPart = 0; ProductMinorPart = 0; ProductBuildPart = 0; ProductPrivatePart = 0
    ProductVersion = "2.17.4"
}
Assert-ChimeraVersionInfo ([pscustomobject]$juce) "2.17.4" "JUCE resource fixture"
$inno = $juce.Clone()
$inno.ProductMajorPart = 2; $inno.ProductMinorPart = 17; $inno.ProductBuildPart = 4
Assert-ChimeraVersionInfo ([pscustomobject]$inno) "2.17.4" "Inno resource fixture"
$inno.ProductVersion = "2.17.4.0"
Assert-ChimeraVersionInfo ([pscustomobject]$inno) "2.17.4" "four-part product string"
foreach ($mutation in @(
    @{ FileBuildPart = 3 }, @{ FilePrivatePart = 1 },
    @{ ProductVersion = "2.17.3" }, @{ ProductVersion = "" },
    @{ ProductBuildPart = 3 }, @{ ProductPrivatePart = 1 }
)) {
    $bad = $inno.Clone()
    foreach ($key in $mutation.Keys) { $bad[$key] = $mutation[$key] }
    $rejected = $false
    try { Assert-ChimeraVersionInfo ([pscustomobject]$bad) "2.17.4" "mixed resource fixture" }
    catch {
        if ($_.Exception.Message -notmatch "Binary version differs from payload") { throw }
        $rejected = $true
    }
    if (!$rejected) { throw "Inconsistent resource fixture was accepted: $($mutation | ConvertTo-Json -Compress)" }
}
Write-Host "PASS: JUCE and Inno resources accepted; mixed/missing product and stale file resources rejected"
