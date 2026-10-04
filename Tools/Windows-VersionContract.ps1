# JUCE 8.0.8 ResourceRcOptions emits fixed FILEVERSION and a ProductVersion
# string, but no fixed PRODUCTVERSION (Windows reports its numeric parts as 0).
# Inno emits both. Require the declared product string, and validate the fixed
# product fields whenever present; do not mistake JUCE's absent fields for 0.0.0.
function Assert-ChimeraVersionInfo($Info, [string]$Expected, [string]$Path) {
    $fileVersion = "$($Info.FileMajorPart).$($Info.FileMinorPart).$($Info.FileBuildPart).$($Info.FilePrivatePart)"
    $productFixed = "$($Info.ProductMajorPart).$($Info.ProductMinorPart).$($Info.ProductBuildPart).$($Info.ProductPrivatePart)"
    # Inno pads StringFileInfo values when updating the prebuilt Setup loader.
    $productText = ([string]$Info.ProductVersion).TrimEnd([char[]]@([char]32, [char]0))
    if ($fileVersion -cne "$Expected.0" -or
        $productText -cnotin @($Expected, "$Expected.0") -or
        ($productFixed -cne "0.0.0.0" -and $productFixed -cne "$Expected.0")) {
        throw "Binary version differs from payload: $Path; file=$fileVersion product=$($Info.ProductVersion) fixedProduct=$productFixed expected=$Expected.0"
    }
}

function Assert-ChimeraBinaryVersion([string]$Path, [string]$Expected) {
    Assert-ChimeraVersionInfo ([Diagnostics.FileVersionInfo]::GetVersionInfo($Path)) $Expected $Path
}
