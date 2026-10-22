# Downloads the Steinberg ASIO SDK for the Windows Standalone build (Issue #113).
# The SDK is not redistributed in this repository: the publisher accepts Steinberg's
# licence and supplies the archive URL and its pinned SHA-256. Writes
# CHIMERA_ASIO_SDK_DIR (the folder containing common/iasiodrv.h) to GITHUB_ENV.
param(
    [Parameter(Mandatory = $true)][string]$Url,
    [Parameter(Mandatory = $true)][string]$Sha256,
    [string]$Destination = (Join-Path $env:RUNNER_TEMP "asiosdk")
)
$ErrorActionPreference = "Stop"
if (-not $Sha256) { throw "CHIMERA_ASIO_SDK_SHA256 must pin the ASIO SDK archive" }
$archive = "$Destination.zip"
Invoke-WebRequest -Uri $Url -OutFile $archive
$actual = (Get-FileHash -Algorithm SHA256 $archive).Hash.ToLowerInvariant()
if ($actual -ne $Sha256.Trim().ToLowerInvariant()) { throw "ASIO SDK SHA-256 mismatch: $actual" }
Expand-Archive -Path $archive -DestinationPath $Destination -Force
$header = Get-ChildItem -Path $Destination -Recurse -Filter iasiodrv.h |
    Where-Object { $_.Directory.Name -eq "common" } | Select-Object -First 1
if (-not $header) { throw "common/iasiodrv.h not found in the ASIO SDK archive" }
$sdk = $header.Directory.Parent.FullName
Write-Host "ASIO SDK: $sdk"
if ($env:GITHUB_ENV) { "CHIMERA_ASIO_SDK_DIR=$sdk" >> $env:GITHUB_ENV }
