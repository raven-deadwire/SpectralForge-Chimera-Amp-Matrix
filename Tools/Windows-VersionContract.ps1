# Compare numeric resource fields, not localized FileVersion string formatting.
function Assert-ChimeraBinaryVersion([string]$Path, [string]$Expected) {
    $info = [Diagnostics.FileVersionInfo]::GetVersionInfo($Path)
    $fileVersion = "$($info.FileMajorPart).$($info.FileMinorPart).$($info.FileBuildPart)"
    $productVersion = "$($info.ProductMajorPart).$($info.ProductMinorPart).$($info.ProductBuildPart)"
    if ($fileVersion -cne $Expected -or $productVersion -cne $Expected -or
        $info.FilePrivatePart -ne 0 -or $info.ProductPrivatePart -ne 0) {
        throw "Binary version differs from payload: $Path; file=$fileVersion product=$productVersion expected=$Expected.0"
    }
}
