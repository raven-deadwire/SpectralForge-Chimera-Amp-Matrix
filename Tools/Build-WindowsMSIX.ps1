param(
    [string]$Stage = "dist/SpectralForge-Chimera-1.0.0-beta.1-win64",
    [string]$OutputDirectory = "dist/msix",
    [ValidateSet("Review", "Store", "Signed")][string]$Mode = "Review",
    [string]$IdentityFile,
    [string]$Version = "1.0.0.0",
    [string]$CertificateThumbprint = $env:CHIMERA_SIGNING_THUMBPRINT
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$publisherDisplayName = "RavenForge Luthier Intelligence"
$packageName = "RavenForge.ChimeraAmpMatrix.Review"
$publisher = "CN=$publisherDisplayName"
if ($Version -notmatch '^[1-9][0-9]*\.[0-9]+\.[0-9]+\.0$' -or
    @($Version.Split('.') | Where-Object { [long]$_ -gt 65535 }).Count) {
    throw "Use a four-part MSIX version with values <= 65535 and the Store-reserved last part 0."
}
if ($Mode -eq "Store") {
    if (!$IdentityFile -or !(Test-Path -LiteralPath $IdentityFile -PathType Leaf)) {
        throw "Store packaging requires the real Partner Center identity JSON; no Store identity has been assumed."
    }
    $identity = Get-Content -LiteralPath $IdentityFile -Raw | ConvertFrom-Json
    foreach ($field in @("Name", "Publisher", "PublisherDisplayName")) {
        if (!$identity.PSObject.Properties[$field] -or [string]::IsNullOrWhiteSpace($identity.$field)) {
            throw "Partner Center identity JSON is missing $field."
        }
    }
    $packageName = [string]$identity.Name
    $publisher = [string]$identity.Publisher
    if ([string]$identity.PublisherDisplayName -cne $publisherDisplayName) {
        throw "Partner Center PublisherDisplayName must match the requested RavenForge publisher; resolve the account identity before packaging."
    }
    if ($packageName -eq "RavenForge.ChimeraAmpMatrix.Review" -or $publisher -notmatch '^CN=') {
        throw "Use the exact package Name and Publisher copied from Partner Center."
    }
}
if ($Mode -eq "Signed") {
    if ($CertificateThumbprint -notmatch '^[0-9A-Fa-f]{40}$') { throw "Direct MSIX distribution requires a verified public code-signing certificate." }
    $certificate = Get-Item -LiteralPath "Cert:/CurrentUser/My/$CertificateThumbprint"
    $publisher = $certificate.Subject
    $packageName = "RavenForge.ChimeraAmpMatrix"
}
if ($packageName -notmatch '^[A-Za-z0-9.-]{3,50}$') { throw "Package identity Name has invalid characters or length." }
$stagePath = (Resolve-Path -LiteralPath $Stage).Path
$executable = Join-Path $stagePath "Standalone/SpectralForge Chimera.exe"
if (!(Test-Path -LiteralPath $executable -PathType Leaf)) { throw "Validated standalone executable is missing." }
$command = Get-Command MakeAppx.exe -ErrorAction SilentlyContinue
$makeAppx = if ($command) { $command.Source } else {
    Get-ChildItem -Path "${env:ProgramFiles(x86)}/Windows Kits/10/bin/*/x64/makeappx.exe" -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (!$makeAppx) { throw "Windows SDK MakeAppx.exe is required." }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$outputPath = (Resolve-Path -LiteralPath $OutputDirectory).Path
$work = Join-Path ([IO.Path]::GetTempPath()) ("Chimera-MSIX-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path "$work/App", "$work/Assets", "$work/Docs" -Force | Out-Null
try {
    Copy-Item -LiteralPath $executable -Destination "$work/App/SpectralForge Chimera.exe"
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "../COPYRIGHT.txt") -Destination "$work/Docs/COPYRIGHT.txt"
    foreach ($name in @("MANUAL.html", "OPEN_BETA_RELEASE_NOTES.md", "INSTALLATION.md", "THIRD_PARTY_NOTICES.md", "AMP_VALIDATION.md", "MODELS_AND_REFERENCE.md", "NAM_REFERENCE_RESULTS.md", "WINDOWS_MSIX.md")) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot "../docs/$name") -Destination "$work/Docs/$name"
    }
    # Raster resampling only: use the same generated RavenForge emblem as the app.
    Add-Type -AssemblyName System.Drawing
    $art = [Drawing.Image]::FromFile((Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "../Assets/Artwork/spectralforge-emblem.png")))
    try {
        foreach ($logo in @(@("Square150x150Logo.png",150), @("Square44x44Logo.png",44), @("StoreLogo.png",50))) {
            $bitmap = [Drawing.Bitmap]::new([int]$logo[1], [int]$logo[1])
            $graphics = [Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.Clear([Drawing.Color]::Transparent)
                $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.DrawImage($art, 0, 0, [int]$logo[1], [int]$logo[1])
                $bitmap.Save((Join-Path "$work/Assets" $logo[0]), [Drawing.Imaging.ImageFormat]::Png)
            } finally { $graphics.Dispose(); $bitmap.Dispose() }
        }
    } finally { $art.Dispose() }
    [xml]$manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot "../Installer/MSIX/AppxManifest.xml") -Raw
    $manifest.Package.Identity.SetAttribute("Name", $packageName)
    $manifest.Package.Identity.SetAttribute("Publisher", $publisher)
    $manifest.Package.Identity.SetAttribute("Version", $Version)
    $manifest.Save("$work/AppxManifest.xml")
    $suffix = if ($Mode -eq "Store") { "store-submission" } elseif ($Mode -eq "Signed") { "signed" } else { "review-unsigned" }
    $package = Join-Path $outputPath "Chimera-$Version-x64-$suffix.msix"
    if ($Mode -eq "Signed") {
        & (Join-Path $PSScriptRoot "Sign-WindowsArtifact.ps1") -Path "$work/App/SpectralForge Chimera.exe" -CertificateThumbprint $CertificateThumbprint -ExpectedPublisher $publisherDisplayName
    }
    & $makeAppx pack /d $work /p $package /h SHA256 /o
    if ($LASTEXITCODE -ne 0) { throw "MakeAppx schema/semantic validation or packaging failed." }
    if ($Mode -eq "Signed") {
        & (Join-Path $PSScriptRoot "Sign-WindowsArtifact.ps1") -Path $package -CertificateThumbprint $CertificateThumbprint -ExpectedPublisher $publisherDisplayName
    }
    $hash = (Get-FileHash -LiteralPath $package -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $([IO.Path]::GetFileName($package))" | Set-Content -LiteralPath ($package + ".sha256.txt")
    Copy-Item -LiteralPath "$work/AppxManifest.xml" -Destination (Join-Path $outputPath "AppxManifest.xml")
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "../docs/WINDOWS_MSIX.md") -Destination $outputPath
    Write-Host "MSIX created: $package | mode=$Mode | publisher=$publisherDisplayName"
    if ($Mode -eq "Review") { Write-Host "REVIEW ONLY: unsigned and not Store-certified; not a customer-installable release." }
    if ($Mode -eq "Store") { Write-Host "Submit through the matching Partner Center app. Microsoft signs only after certification." }
} finally {
    # Only this invocation's randomly named temporary staging directory.
    Remove-Item -LiteralPath $work -Recurse -Force
}
