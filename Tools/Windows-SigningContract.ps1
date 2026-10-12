# Shared by payload signing and Inno's separate PowerShell signing process.
function Assert-ChimeraSigningContext {
    if ($env:GITHUB_EVENT_NAME -in @('pull_request', 'pull_request_target', 'merge_group') -or
        $env:GITHUB_REF -like 'refs/pull/*' -or $env:GITHUB_HEAD_REF) {
        throw 'Signing is forbidden in PR and merge-queue contexts.'
    }
}

function Get-ChimeraSigningConfiguration {
    param(
        [ValidateSet('Certificate', 'ArtifactSigning')][string]$SigningProvider = 'Certificate',
        [string]$CertificateThumbprint,
        [string]$ExpectedPublisher,
        [string]$ExpectedSubject,
        [string]$ArtifactSigningDlib,
        [string]$ArtifactSigningMetadata,
        [string]$TimestampUrl,
        [switch]$VerifyOnly
    )
    if ([string]::IsNullOrWhiteSpace($ExpectedPublisher)) { throw 'Expected publisher is required.' }
    if ($SigningProvider -eq 'Certificate') {
        if ($CertificateThumbprint -notmatch '^[0-9A-Fa-f]{40}$') {
            throw 'A verified public code-signing certificate is required. Set CHIMERA_SIGNING_THUMBPRINT to its 40-character thumbprint.'
        }
        if (!$TimestampUrl) { $TimestampUrl = 'http://timestamp.digicert.com' }
        $identityArguments = @('/sha1', $CertificateThumbprint, '/s', 'My')
    } else {
        # Service certificates rotate. Pin the reviewed legal identity, not a leaf thumbprint.
        if ([string]::IsNullOrWhiteSpace($ExpectedSubject)) {
            throw 'Artifact Signing requires the independently reviewed full certificate subject (CHIMERA_SIGNING_SUBJECT).'
        }
        if ($CertificateThumbprint) { throw 'Artifact Signing cannot be combined with a certificate thumbprint.' }
        if (!$TimestampUrl) { $TimestampUrl = 'http://timestamp.acs.microsoft.com' }
        $identityArguments = @()
        if (!$VerifyOnly) {
            foreach ($file in @($ArtifactSigningDlib, $ArtifactSigningMetadata)) {
                if (!$file -or !(Test-Path -LiteralPath $file -PathType Leaf)) {
                    throw 'Artifact Signing requires an installed dlib and metadata JSON file.'
                }
            }
            $ArtifactSigningDlib = (Resolve-Path -LiteralPath $ArtifactSigningDlib).Path
            $ArtifactSigningMetadata = (Resolve-Path -LiteralPath $ArtifactSigningMetadata).Path
            $metadata = Get-Content -LiteralPath $ArtifactSigningMetadata -Raw | ConvertFrom-Json
            foreach ($field in @('Endpoint', 'CodeSigningAccountName', 'CertificateProfileName')) {
                if ($metadata.PSObject.Properties.Name -notcontains $field -or
                    $metadata.$field -isnot [string] -or [string]::IsNullOrWhiteSpace($metadata.$field)) {
                    throw "Artifact Signing metadata requires $field."
                }
            }
            # Azure public-cloud signing endpoints only; no embedded credentials or arbitrary URL.
            if ($metadata.Endpoint -notmatch '^https://[a-z0-9]+\.codesigning\.azure\.net/?$') {
                throw 'Artifact Signing endpoint must be the HTTPS endpoint for the account region.'
            }
            $identityArguments = @('/dlib', $ArtifactSigningDlib, '/dmdf', $ArtifactSigningMetadata)
        }
    }
    $timestampUri = $null
    if (![Uri]::TryCreate($TimestampUrl, [UriKind]::Absolute, [ref]$timestampUri) -or
        $timestampUri.Scheme -notin @('http', 'https') -or $timestampUri.UserInfo) {
        throw 'An HTTP(S) RFC 3161 timestamp endpoint is required.'
    }
    [pscustomobject]@{
        SigningProvider = $SigningProvider
        CertificateThumbprint = $CertificateThumbprint
        ExpectedPublisher = $ExpectedPublisher
        ExpectedSubject = $ExpectedSubject
        ArtifactSigningDlib = $ArtifactSigningDlib
        ArtifactSigningMetadata = $ArtifactSigningMetadata
        TimestampUrl = $TimestampUrl
        SignArguments = @('sign') + $identityArguments + @('/fd', 'SHA256', '/tr', $TimestampUrl,
            '/td', 'SHA256', '/d', 'Chimera Amp Matrix')
    }
}

function Assert-ChimeraSignedIdentity($Signature, $Configuration, [string]$Path) {
    if ($Signature.Status -ne 'Valid' -or $Signature.SignatureType -ne 'Authenticode' -or
        !$Signature.TimeStamperCertificate -or !$Signature.SignerCertificate) {
        throw "A valid, timestamped embedded signature is required: $Path"
    }
    $certificate = $Signature.SignerCertificate
    if ($certificate.GetNameInfo([Security.Cryptography.X509Certificates.X509NameType]::SimpleName, $false) -cne $Configuration.ExpectedPublisher -or
        ($Configuration.SigningProvider -eq 'Certificate' -and $certificate.Thumbprint -ine $Configuration.CertificateThumbprint) -or
        ($Configuration.SigningProvider -eq 'ArtifactSigning' -and $certificate.Subject -cne $Configuration.ExpectedSubject)) {
        throw "Signed publisher does not match the pinned release identity: $Path"
    }
}

function Assert-ChimeraUnsignedPayload([string]$StagePath) {
    # Do not bless tampered/stale inputs by rewriting their hashes after signing.
    $root = [IO.Path]::GetFullPath($StagePath).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    $manifestPath = Join-Path $root 'payload-manifest.json'
    $payload = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $seen = @{}
    foreach ($entry in $payload.files) {
        $file = [IO.Path]::GetFullPath((Join-Path $root $entry.path))
        if (!$file.StartsWith($root, [StringComparison]::OrdinalIgnoreCase) -or
            $file -eq $manifestPath -or $seen.ContainsKey($file) -or
            !(Test-Path -LiteralPath $file -PathType Leaf)) {
            throw "Invalid signed payload inventory entry: $($entry.path)"
        }
        $seen[$file] = $true
        if ((Get-Item -LiteralPath $file).Length -ne $entry.bytes -or
            (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ine $entry.sha256) {
            throw "Unsigned payload hash/size differs from reviewed manifest: $($entry.path)"
        }
    }
    foreach ($file in Get-ChildItem -LiteralPath $root -File -Recurse) {
        if ($file.FullName -ne $manifestPath -and !$seen.ContainsKey($file.FullName)) {
            throw "Unlisted file in signed payload: $($file.FullName)"
        }
    }
}
