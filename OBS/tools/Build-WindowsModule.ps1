# SPDX-License-Identifier: GPL-2.0-or-later
# Builds the Windows x64 OBS filter module against a pinned OBS Studio release
# and lays it out as OBS loads it (bin/64bit + data). Windows has no libobs
# development package: the module links an import library generated from the
# release's own obs.dll, with the libobs headers of the same release tag.
# With -TestBundle, the module's tests run against that Chimera VST3 and the
# release's obs.dll. Used by the OBS add-on, release and candidate workflows.
# -StartupObserverDirectory builds a separate, test-only frontend observer.
# Omit it for release/candidate packages; the fixture is never copied there.
param(
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [string]$TestBundle = "",
    [string]$StartupObserverDirectory = "",
    [string]$ObsVersion = "31.1.2",
    [string]$ObsZipSha256 = "9513cd402936593a6a5500da5d2bd49e6cd04dce05509c7b7f48dd25c391d2d8",
    [string]$WorkDirectory = (Join-Path ([IO.Path]::GetTempPath()) "chimera-obs-build")
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
if (!$IsWindows -or ![Environment]::Is64BitProcess) { throw "The OBS module build requires x64 Windows." }

if ($StartupObserverDirectory) {
    # Keep test fixtures outside the shipped module layout, even if a caller
    # accidentally supplies identical or nested output directories.
    $productPath = [IO.Path]::GetFullPath($OutputDirectory).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    $observerPath = [IO.Path]::GetFullPath($StartupObserverDirectory).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    if ($productPath.StartsWith($observerPath, [StringComparison]::OrdinalIgnoreCase) -or
        $observerPath.StartsWith($productPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Startup observer and product output directories must be separate and non-overlapping."
    }
}

$source = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
New-Item -ItemType Directory -Force -Path $WorkDirectory | Out-Null
$work = (Resolve-Path -LiteralPath $WorkDirectory).Path

# Pinned OBS release: binaries for the import library and tests.
$zip = Join-Path $work "obs-$ObsVersion.zip"
if (!(Test-Path -LiteralPath $zip)) {
    Invoke-WebRequest "https://github.com/obsproject/obs-studio/releases/download/$ObsVersion/OBS-Studio-$ObsVersion-Windows-x64.zip" -OutFile $zip -MaximumRetryCount 5 -RetryIntervalSec 30
}
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
if ($hash -ne $ObsZipSha256) { throw "OBS $ObsVersion release hash mismatch: $hash" }
$obs = Join-Path $work "obs"
python -I -c "import sys, zipfile; z = zipfile.ZipFile(sys.argv[1]); [z.extract(n, sys.argv[2]) for n in z.namelist() if n.startswith(('bin/64bit/', 'data/libobs/'))]" $zip $obs
if ($LASTEXITCODE) { throw "Cannot extract the OBS release." }

# libobs headers of the same tag.
$headers = Join-Path $work "obs-src"
if (!(Test-Path -LiteralPath (Join-Path $headers "libobs/obs-module.h"))) {
    git clone --depth 1 --branch $ObsVersion --filter=blob:none --sparse https://github.com/obsproject/obs-studio $headers
    if ($LASTEXITCODE) { throw "Cannot fetch OBS $ObsVersion headers." }
    git -C $headers sparse-checkout set libobs
    if ($LASTEXITCODE) { throw "Cannot check out the libobs headers." }
}
if ($StartupObserverDirectory) {
    git -C $headers sparse-checkout add frontend/api
    if ($LASTEXITCODE) { throw "Cannot check out the OBS frontend API headers." }
}

# obs.lib from the release obs.dll's export table.
$def = Join-Path $work "obs.def"
$lib = Join-Path $work "obs.lib"
python -I (Join-Path $PSScriptRoot "dll_exports_def.py") (Join-Path $obs "bin/64bit/obs.dll") $def
if ($LASTEXITCODE) { throw "Cannot read obs.dll exports." }
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/Installer/vswhere.exe"
$libExe = & $vswhere -latest -products * -find 'VC\Tools\MSVC\*\bin\Hostx64\x64\lib.exe' | Select-Object -First 1
if (!$libExe) { throw "lib.exe was not found." }
& $libExe /nologo "/def:$def" /machine:x64 "/out:$lib"
if ($LASTEXITCODE) { throw "lib.exe failed: $LASTEXITCODE" }

if ($StartupObserverDirectory) {
    # The observer calls the released frontend C API, not Qt/private UI code.
    $frontendDef = Join-Path $work "obs-frontend-api.def"
    $frontendLib = Join-Path $work "obs-frontend-api.lib"
    python -I (Join-Path $PSScriptRoot "dll_exports_def.py") (Join-Path $obs "bin/64bit/obs-frontend-api.dll") $frontendDef
    if ($LASTEXITCODE) { throw "Cannot read obs-frontend-api.dll exports." }
    & $libExe /nologo "/def:$frontendDef" /machine:x64 "/out:$frontendLib"
    if ($LASTEXITCODE) { throw "Frontend import library generation failed: $LASTEXITCODE" }
}

$build = Join-Path $work "build"
# The module records the commit it was built from (version resource and OBS log).
$sourceSha = (& git -C $source rev-parse HEAD).Trim()
if ($LASTEXITCODE -or $sourceSha -notmatch '^[0-9a-f]{40}$') { throw "Cannot read the module source commit." }
$configure = @("-S", $source, "-B", $build, "-A", "x64",
    "-DCHIMERA_OBS_LIBOBS_INCLUDE_DIR=$(Join-Path $headers 'libobs')", "-DCHIMERA_OBS_LIBOBS_LIBRARY=$lib",
    "-DCHIMERA_OBS_SOURCE_SHA=$sourceSha")
if ($StartupObserverDirectory) {
    $configure += @("-DCHIMERA_OBS_BUILD_STARTUP_OBSERVER=ON",
        "-DCHIMERA_OBS_FRONTEND_INCLUDE_DIR=$(Join-Path $headers 'frontend/api')",
        "-DCHIMERA_OBS_FRONTEND_LIBRARY=$frontendLib")
} else {
    # Explicit OFF also clears an earlier opt-in in a reused build directory.
    $configure += "-DCHIMERA_OBS_BUILD_STARTUP_OBSERVER=OFF"
}
if ($TestBundle) { $configure += "-DCHIMERA_OBS_TEST_BUNDLE=$((Resolve-Path -LiteralPath $TestBundle).Path)" }
cmake @configure
if ($LASTEXITCODE) { throw "OBS module configure failed." }
cmake --build $build --config Release --parallel 4
if ($LASTEXITCODE) { throw "OBS module build failed." }
if ($TestBundle) {
    $previousPath = $env:PATH
    try {
        $env:PATH = "$(Join-Path $obs 'bin/64bit');$env:PATH"
        ctest --test-dir $build -C Release --output-on-failure --no-tests=error
        if ($LASTEXITCODE) { throw "OBS module tests failed." }
    } finally { $env:PATH = $previousPath }
}

# Plugin layout under %ProgramData%\obs-studio\plugins\chimera-obs.
if (Test-Path -LiteralPath $OutputDirectory) { Remove-Item -LiteralPath $OutputDirectory -Recurse -Force }
New-Item -ItemType Directory -Force -Path (Join-Path $OutputDirectory "bin/64bit") | Out-Null
Copy-Item -LiteralPath (Join-Path $build "Release/chimera-obs.dll") -Destination (Join-Path $OutputDirectory "bin/64bit/chimera-obs.dll") -Force
Copy-Item -LiteralPath (Join-Path $source "data") -Destination $OutputDirectory -Recurse -Force
Write-Host "OBS module for OBS ${ObsVersion}: $((Resolve-Path -LiteralPath $OutputDirectory).Path)"

if ($StartupObserverDirectory) {
    if (Test-Path -LiteralPath $StartupObserverDirectory) { Remove-Item -LiteralPath $StartupObserverDirectory -Recurse -Force }
    New-Item -ItemType Directory -Force -Path (Join-Path $StartupObserverDirectory "bin/64bit"), (Join-Path $StartupObserverDirectory "data") | Out-Null
    Copy-Item -LiteralPath (Join-Path $build "Release/chimera-obs-startup-observer.dll") -Destination (Join-Path $StartupObserverDirectory "bin/64bit/chimera-obs-startup-observer.dll") -Force
    Write-Host "Test-only startup observer for OBS ${ObsVersion}: $((Resolve-Path -LiteralPath $StartupObserverDirectory).Path)"
}
