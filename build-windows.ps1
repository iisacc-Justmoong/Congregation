#Requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$BuildType = "Release",

    [string]$QtPrefix = $env:QT_PREFIX,
    [string]$LVRSPrefix = $env:LVRS_PREFIX,
    [string]$IiPaintEnginePrefix = $env:IIPAINTENGINE_PREFIX,
    [string]$IiSharedCanvasPrefix = $env:IISHAREDCANVAS_PREFIX,
    [string]$IiUpdateManagerPrefix = $env:IIUPDATEMANAGER_PREFIX,
    [string]$IiLicenseManagerPrefix = $env:IILICENSEMANAGER_PREFIX,

    [ValidateSet("Ninja", "Visual Studio 17 2022")]
    [string]$Generator = "Ninja",

    [switch]$Clean,
    [switch]$SkipTests,
    [switch]$SkipPackage,
    [switch]$Sign,
    [switch]$AllowUnsignedPackage,
    [switch]$ExternalSigning,
    [string]$SigningCertificateThumbprint = $env:CONGREGATION_SIGNING_CERTIFICATE_THUMBPRINT,
    [ValidateSet("CurrentUser", "LocalMachine")]
    [string]$SigningCertificateStoreLocation = "CurrentUser",
    [string]$TimestampUrl = $(if ($env:CONGREGATION_TIMESTAMP_URL) { $env:CONGREGATION_TIMESTAMP_URL } else { "http://timestamp.digicert.com" }),
    [string]$SignToolPath = $env:SIGNTOOL_PATH,
    [string]$CorrespondingSourceUrl = $env:CONGREGATION_CORRESPONDING_SOURCE_URL,
    [string]$CorrespondingSourceSha256 = $env:CONGREGATION_CORRESPONDING_SOURCE_SHA256,
    [switch]$CreateMsi,
    [string]$WixToolsDir = $env:WIX_TOOLS_DIR,
    [switch]$InstallForCurrentUser,
    [string]$InstallDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Version = "1.0.0"
$UnsignedPublicReleaseExpiresAtUtc = [DateTimeOffset]::Parse("2027-01-01T00:00:00Z")
$windowsVersionParts = @($Version -split '\.')
while ($windowsVersionParts.Count -lt 4) {
    $windowsVersionParts += "0"
}
$WindowsFileVersion = $windowsVersionParts[0..3] -join "."
$RepositoryRoot = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$BuildDir = Join-Path $RepositoryRoot "build"
$DistRoot = Join-Path $RepositoryRoot "dist"
$StageDir = Join-Path $DistRoot "Congregation-Windows"
$SignedZipPath = Join-Path $DistRoot "Congregation-$Version-Windows.zip"
$UnsignedZipPath = Join-Path $DistRoot "Congregation-$Version-Windows-unsigned.zip"
$ZipPath = if ($AllowUnsignedPackage) { $UnsignedZipPath } else { $SignedZipPath }
$ZipChecksumPath = "$ZipPath.sha256"
$SignedZipPartialPath = Join-Path $DistRoot "Congregation-$Version-Windows.partial.zip"
$UnsignedZipPartialPath = Join-Path $DistRoot "Congregation-$Version-Windows-unsigned.partial.zip"
$ZipPartialPath = if ($AllowUnsignedPackage) { $UnsignedZipPartialPath } else { $SignedZipPartialPath }
$ZipChecksumPartialPath = "$ZipChecksumPath.partial"
$MsiWorkDir = Join-Path $BuildDir "msi"
$ExternalSigningInputDir = Join-Path $BuildDir "signpath-input"
$SignedMsiPath = Join-Path $BuildDir "Congregation-$Version-Windows.msi"
$UnsignedMsiPath = Join-Path $BuildDir "Congregation-$Version-Windows-unsigned.msi"
$ExternalSigningMsiPath = Join-Path $ExternalSigningInputDir "Congregation-$Version-Windows.msi"
$MsiPath = if ($ExternalSigning) { $ExternalSigningMsiPath } elseif ($AllowUnsignedPackage) { $UnsignedMsiPath } else { $SignedMsiPath }
$MsiChecksumPath = "$MsiPath.sha256"
$SignedMsiPartialPath = Join-Path $BuildDir "Congregation-$Version-Windows.partial.msi"
$UnsignedMsiPartialPath = Join-Path $BuildDir "Congregation-$Version-Windows-unsigned.partial.msi"
$ExternalSigningMsiPartialPath = Join-Path $ExternalSigningInputDir "Congregation-$Version-Windows.partial.msi"
$MsiPartialPath = if ($ExternalSigning) { $ExternalSigningMsiPartialPath } elseif ($AllowUnsignedPackage) { $UnsignedMsiPartialPath } else { $SignedMsiPartialPath }
$SignedMsiPartialDebugPath = [System.IO.Path]::ChangeExtension($SignedMsiPartialPath, ".wixpdb")
$UnsignedMsiPartialDebugPath = [System.IO.Path]::ChangeExtension($UnsignedMsiPartialPath, ".wixpdb")
$ExternalSigningMsiPartialDebugPath = [System.IO.Path]::ChangeExtension($ExternalSigningMsiPartialPath, ".wixpdb")
$MsiPartialDebugPath = if ($ExternalSigning) { $ExternalSigningMsiPartialDebugPath } elseif ($AllowUnsignedPackage) { $UnsignedMsiPartialDebugPath } else { $SignedMsiPartialDebugPath }
$MsiChecksumPartialPath = "$MsiChecksumPath.partial"
$CpackIncompleteZipPath = Join-Path $BuildDir "Congregation-$Version-Windows-unsigned-cpack-incomplete.zip"
$SignedPackagePublicationJournalPath = Join-Path $DistRoot ".Congregation-$Version-Windows.publication.json"
$UnsignedPackagePublicationJournalPath = Join-Path $DistRoot ".Congregation-$Version-Windows-unsigned.publication.json"
$ExternalSigningPublicationJournalPath = Join-Path $ExternalSigningInputDir ".Congregation-$Version-Windows-signpath-input.publication.json"
$PackagePublicationJournalPath = if ($ExternalSigning) {
    $ExternalSigningPublicationJournalPath
} elseif ($AllowUnsignedPackage) {
    $UnsignedPackagePublicationJournalPath
} else {
    $SignedPackagePublicationJournalPath
}

function Write-Step {
    param([string]$Message)
    Write-Host ""
    Write-Host "=== $Message ==="
}

function Enter-WindowsBuildMutex {
    param([string]$MutexName = "Global\Congregation.BuildWindows.PackagePipeline.v1")

    $mutex = [System.Threading.Mutex]::new($false, $MutexName)
    $acquired = $false
    try {
        $acquired = $mutex.WaitOne(0)
    } catch [System.Threading.AbandonedMutexException] {
        $acquired = $true
    }
    if (-not $acquired) {
        $mutex.Dispose()
        throw "Another build-windows.ps1 process is already using this repository."
    }

    return $mutex
}

function Get-RequiredCommand {
    param([string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $command) {
        throw "Required command '$Name' was not found in PATH."
    }

    return $command.Source
}

function Invoke-Native {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    Write-Host "> $FilePath $($Arguments -join ' ')"
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $FilePath"
    }
}

function Assert-AuthenticodePolicy {
    param(
        [bool]$SigningRequested,
        [bool]$UnsignedPackageAllowed,
        [bool]$ExternalSigningRequested = $false,
        [bool]$ZipPackageSkipped,
        [bool]$MsiRequested,
        [bool]$TestsSkipped,
        [string]$Configuration,
        [string]$CertificateThumbprint,
        [string]$Rfc3161TimestampUrl,
        [DateTimeOffset]$CurrentTimeUtc = [DateTimeOffset]::UtcNow,
        [DateTimeOffset]$UnsignedReleaseExpiresAtUtc = [DateTimeOffset]::Parse("2027-01-01T00:00:00Z")
    )

    if ($SigningRequested -and $UnsignedPackageAllowed) {
        throw "-Sign and -AllowUnsignedPackage cannot be used together."
    }
    if ($ExternalSigningRequested -and ($SigningRequested -or $UnsignedPackageAllowed)) {
        throw "-ExternalSigning cannot be combined with -Sign or -AllowUnsignedPackage."
    }
    if ($ExternalSigningRequested -and (-not $ZipPackageSkipped)) {
        throw "External signing input is MSI-only; use -SkipPackage."
    }
    if ($ExternalSigningRequested -and (-not $MsiRequested)) {
        throw "External signing requires -CreateMsi."
    }

    if ($UnsignedPackageAllowed) {
        if ($Configuration -notin @("Release", "MinSizeRel")) {
            throw "The temporary unsigned public release requires Release or MinSizeRel, not $Configuration."
        }
        if ($TestsSkipped) {
            throw "The temporary unsigned public release does not allow -SkipTests."
        }
        if ($CurrentTimeUtc.ToUniversalTime() -ge $UnsignedReleaseExpiresAtUtc.ToUniversalTime()) {
            throw "The temporary unsigned public release policy expired at $($UnsignedReleaseExpiresAtUtc.ToString('u'))."
        }
    }

    $packageRequested = (-not $ZipPackageSkipped) -or $MsiRequested
    if ($packageRequested -and (-not $SigningRequested) -and (-not $UnsignedPackageAllowed) -and (-not $ExternalSigningRequested)) {
        throw "Public package creation requires -Sign, -ExternalSigning, or -AllowUnsignedPackage under the temporary 2026 policy."
    }

    if ($ExternalSigningRequested) {
        if ($Configuration -notin @("Release", "MinSizeRel")) {
            throw "External signing requires Release or MinSizeRel, not $Configuration."
        }
        if ($TestsSkipped) {
            throw "External signing does not allow -SkipTests."
        }
        return
    }
    if (-not $SigningRequested) {
        return
    }
    if ($Configuration -notin @("Release", "MinSizeRel")) {
        throw "Authenticode release signing requires Release or MinSizeRel, not $Configuration."
    }
    if ($TestsSkipped) {
        throw "Authenticode release signing does not allow -SkipTests."
    }

    $thumbprintText = if ($null -eq $CertificateThumbprint) { "" } else { [string]$CertificateThumbprint }
    $normalizedThumbprint = ($thumbprintText -replace '\s', '').ToUpperInvariant()
    if ($normalizedThumbprint -notmatch '^[0-9A-F]{40}$') {
        throw "-Sign requires an exact 40-hex SigningCertificateThumbprint."
    }

    $timestampUri = $null
    $validTimestampUri = [System.Uri]::TryCreate(
        $Rfc3161TimestampUrl,
        [System.UriKind]::Absolute,
        [ref]$timestampUri
    )
    if ((-not $validTimestampUri) -or ($timestampUri.Scheme -notin @("http", "https"))) {
        throw "RFC 3161 TimestampUrl must be an absolute HTTP or HTTPS URL."
    }
}

function Clear-WindowsPackageArtifacts {
    param([string[]]$Paths)

    foreach ($path in $Paths) {
        if ($path -and (Test-Path -LiteralPath $path -PathType Leaf)) {
            Remove-Item -LiteralPath $path -Force
        }
    }
}

function Assert-PackageArtifactChecksum {
    param(
        [string]$ArtifactPath,
        [string]$ChecksumPath,
        [string]$RecordedArtifactPath
    )

    if (-not (Test-Path -LiteralPath $ArtifactPath -PathType Leaf)) {
        throw "Package artifact is missing: $ArtifactPath"
    }
    if (-not (Test-Path -LiteralPath $ChecksumPath -PathType Leaf)) {
        throw "Package checksum is missing: $ChecksumPath"
    }

    $record = (Get-Content -LiteralPath $ChecksumPath -Raw).Trim()
    $match = [regex]::Match($record, '^(?<hash>[0-9A-Fa-f]{64}) \*(?<name>.+)$')
    if (-not $match.Success) {
        throw "Package checksum record has an invalid format: $ChecksumPath"
    }

    $expectedFileName = [System.IO.Path]::GetFileName($RecordedArtifactPath)
    if ($match.Groups['name'].Value -cne $expectedFileName) {
        throw "Package checksum records '$($match.Groups['name'].Value)' instead of '$expectedFileName'."
    }

    $actualHash = (Get-FileHash -LiteralPath $ArtifactPath -Algorithm SHA256).Hash
    if ($match.Groups['hash'].Value.ToUpperInvariant() -cne $actualHash.ToUpperInvariant()) {
        throw "Package checksum does not match the artifact: $ArtifactPath"
    }
}

function Test-PackageArtifactChecksum {
    param(
        [string]$ArtifactPath,
        [string]$ChecksumPath,
        [string]$RecordedArtifactPath
    )

    try {
        Assert-PackageArtifactChecksum `
            -ArtifactPath $ArtifactPath `
            -ChecksumPath $ChecksumPath `
            -RecordedArtifactPath $RecordedArtifactPath
        return $true
    } catch {
        return $false
    }
}

function Remove-PackagePublicationJournal {
    param([string]$JournalPath)

    if (-not $JournalPath) {
        return
    }

    foreach ($path in @("$JournalPath.partial", "$JournalPath.previous", $JournalPath)) {
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            Remove-Item -LiteralPath $path -Force -ErrorAction Stop
        }
    }
}

function Write-PackagePublicationJournal {
    param(
        [string]$JournalPath,
        [ValidateSet("prepared", "committed")]
        [string]$Phase,
        [object[]]$Items
    )

    if (-not $JournalPath) {
        throw "A package publication journal path is required."
    }
    if ((-not $Items) -or $Items.Count -eq 0) {
        throw "A package publication journal requires at least one item."
    }

    $resolvedJournalPath = [System.IO.Path]::GetFullPath($JournalPath)
    $journalDirectory = [System.IO.Path]::GetDirectoryName($resolvedJournalPath)
    New-Item -ItemType Directory -Path $journalDirectory -Force | Out-Null

    $normalizedItems = @(
        foreach ($item in $Items) {
            if (($item.ArtifactExisted -isnot [bool]) -or ($item.ChecksumExisted -isnot [bool])) {
                throw "Package publication journal existence flags must be Boolean values."
            }
            if ($item.ArtifactExisted -ne $item.ChecksumExisted) {
                throw "A package publication baseline must contain both the artifact and checksum, or neither."
            }

            [ordered]@{
                FinalArtifactPath = [System.IO.Path]::GetFullPath([string]$item.FinalArtifactPath)
                FinalChecksumPath = [System.IO.Path]::GetFullPath([string]$item.FinalChecksumPath)
                ArtifactExisted = [bool]$item.ArtifactExisted
                ChecksumExisted = [bool]$item.ChecksumExisted
            }
        }
    )
    $normalizedPaths = @(
        $normalizedItems | ForEach-Object {
            $_.FinalArtifactPath.ToUpperInvariant()
            $_.FinalChecksumPath.ToUpperInvariant()
        }
    )
    if (@($normalizedPaths | Select-Object -Unique).Count -ne $normalizedPaths.Count) {
        throw "Package publication journal paths must be unique."
    }

    $journal = [ordered]@{
        SchemaVersion = 1
        Phase = $Phase
        Items = $normalizedItems
    }
    $json = $journal | ConvertTo-Json -Depth 5
    $temporaryJournalPath = "$resolvedJournalPath.partial"
    Remove-Item -LiteralPath $temporaryJournalPath -Force -ErrorAction SilentlyContinue
    if (-not (Test-Path -LiteralPath $resolvedJournalPath -PathType Leaf)) {
        Remove-Item -LiteralPath "$resolvedJournalPath.previous" -Force -ErrorAction SilentlyContinue
    }

    $encoding = New-Object System.Text.UTF8Encoding($false)
    $bytes = $encoding.GetBytes($json)
    $stream = New-Object System.IO.FileStream(
        $temporaryJournalPath,
        [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::None,
        4096,
        [System.IO.FileOptions]::WriteThrough
    )
    try {
        $stream.Write($bytes, 0, $bytes.Length)
        $stream.Flush($true)
    } finally {
        $stream.Dispose()
    }

    if (Test-Path -LiteralPath $resolvedJournalPath -PathType Leaf) {
        $previousJournalPath = "$resolvedJournalPath.previous"
        if (Test-Path -LiteralPath $previousJournalPath -PathType Leaf) {
            Remove-Item -LiteralPath $previousJournalPath -Force -ErrorAction Stop
        }
        [System.IO.File]::Replace($temporaryJournalPath, $resolvedJournalPath, $previousJournalPath, $true)
    } else {
        [System.IO.File]::Move($temporaryJournalPath, $resolvedJournalPath)
    }
}

function Restore-PackageArtifactBackups {
    param(
        [string]$FinalArtifactPath,
        [string]$FinalChecksumPath
    )

    $artifactBackupPath = "$FinalArtifactPath.previous"
    $checksumBackupPath = "$FinalChecksumPath.previous"

    $hasArtifactBackup = Test-Path -LiteralPath $artifactBackupPath -PathType Leaf
    $hasChecksumBackup = Test-Path -LiteralPath $checksumBackupPath -PathType Leaf
    if ((-not $hasArtifactBackup) -and (-not $hasChecksumBackup)) {
        return
    }

    $candidates = @(
        [pscustomobject]@{ Artifact = $artifactBackupPath; Checksum = $checksumBackupPath },
        [pscustomobject]@{ Artifact = $FinalArtifactPath; Checksum = $FinalChecksumPath },
        [pscustomobject]@{ Artifact = $artifactBackupPath; Checksum = $FinalChecksumPath },
        [pscustomobject]@{ Artifact = $FinalArtifactPath; Checksum = $checksumBackupPath }
    )
    $selected = $null
    foreach ($candidate in $candidates) {
        if (Test-PackageArtifactChecksum `
                -ArtifactPath $candidate.Artifact `
                -ChecksumPath $candidate.Checksum `
                -RecordedArtifactPath $FinalArtifactPath) {
            $selected = $candidate
            break
        }
    }
    if (-not $selected) {
        throw "No internally consistent package artifact and checksum pair could be recovered for $FinalArtifactPath."
    }

    if (-not [string]::Equals($selected.Artifact, $FinalArtifactPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $FinalArtifactPath -Force -ErrorAction SilentlyContinue
        Move-Item -LiteralPath $selected.Artifact -Destination $FinalArtifactPath -Force
    }
    if (-not [string]::Equals($selected.Checksum, $FinalChecksumPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $FinalChecksumPath -Force -ErrorAction SilentlyContinue
        Move-Item -LiteralPath $selected.Checksum -Destination $FinalChecksumPath -Force
    }

    if (Test-Path -LiteralPath $artifactBackupPath -PathType Leaf) {
        Remove-Item -LiteralPath $artifactBackupPath -Force -ErrorAction Stop
    }
    if (Test-Path -LiteralPath $checksumBackupPath -PathType Leaf) {
        Remove-Item -LiteralPath $checksumBackupPath -Force -ErrorAction Stop
    }
    Assert-PackageArtifactChecksum `
        -ArtifactPath $FinalArtifactPath `
        -ChecksumPath $FinalChecksumPath `
        -RecordedArtifactPath $FinalArtifactPath
}

function Restore-PackageArtifactSetBackups {
    param(
        [object[]]$Artifacts,
        [string]$JournalPath = ""
    )

    if ((-not $Artifacts) -or $Artifacts.Count -eq 0) {
        throw "At least one package artifact is required for recovery."
    }

    if ($JournalPath -and (Test-Path -LiteralPath $JournalPath -PathType Leaf)) {
        try {
            $journal = Get-Content -LiteralPath $JournalPath -Raw -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop
        } catch {
            throw "Package publication journal is malformed: $JournalPath"
        }

        $schemaVersionIsInteger = ($journal.SchemaVersion -is [int]) -or ($journal.SchemaVersion -is [long])
        if ((-not $schemaVersionIsInteger) -or $journal.SchemaVersion -ne 1) {
            throw "Package publication journal has an unsupported schema version."
        }
        if ($journal.Phase -notin @("prepared", "committed")) {
            throw "Package publication journal has an unknown phase: $($journal.Phase)"
        }

        $journalItems = @($journal.Items)
        if ($journalItems.Count -eq 0 -or $journalItems.Count -gt $Artifacts.Count) {
            throw "Package publication journal does not match the allowed artifact set."
        }

        $journalPaths = @(
            foreach ($journalItem in $journalItems) {
                [System.IO.Path]::GetFullPath([string]$journalItem.FinalArtifactPath).ToUpperInvariant()
                [System.IO.Path]::GetFullPath([string]$journalItem.FinalChecksumPath).ToUpperInvariant()
            }
        )
        if (@($journalPaths | Select-Object -Unique).Count -ne $journalPaths.Count) {
            throw "Package publication journal contains duplicate artifact paths."
        }

        $matchedItems = @()
        $matchedKeys = @{}
        foreach ($journalItem in $journalItems) {
            $journalArtifactPath = [System.IO.Path]::GetFullPath([string]$journalItem.FinalArtifactPath)
            $journalChecksumPath = [System.IO.Path]::GetFullPath([string]$journalItem.FinalChecksumPath)
            $matches = @(
                $Artifacts | Where-Object {
                    [string]::Equals(
                        [System.IO.Path]::GetFullPath([string]$_.FinalArtifactPath),
                        $journalArtifactPath,
                        [System.StringComparison]::OrdinalIgnoreCase
                    ) -and [string]::Equals(
                        [System.IO.Path]::GetFullPath([string]$_.FinalChecksumPath),
                        $journalChecksumPath,
                        [System.StringComparison]::OrdinalIgnoreCase
                    )
                }
            )
            if ($matches.Count -ne 1) {
                throw "Package publication journal paths do not match the allowed artifact set."
            }

            if (($journalItem.ArtifactExisted -isnot [bool]) -or ($journalItem.ChecksumExisted -isnot [bool])) {
                throw "Package publication journal existence flags must be Boolean values."
            }
            if ($journalItem.ArtifactExisted -ne $journalItem.ChecksumExisted) {
                throw "Package publication journal records an incomplete baseline pair."
            }

            $itemKey = "$($journalArtifactPath.ToUpperInvariant())|$($journalChecksumPath.ToUpperInvariant())"
            if ($matchedKeys.ContainsKey($itemKey)) {
                throw "Package publication journal contains duplicate artifact paths."
            }
            $matchedKeys[$itemKey] = $true
            $matchedItems += [pscustomobject]@{
                Artifact = $matches[0]
                ArtifactExisted = [bool]$journalItem.ArtifactExisted
                ChecksumExisted = [bool]$journalItem.ChecksumExisted
            }
        }

        $allFinalPairsValid = $true
        foreach ($matchedItem in $matchedItems) {
            if (-not (Test-PackageArtifactChecksum `
                    -ArtifactPath $matchedItem.Artifact.FinalArtifactPath `
                    -ChecksumPath $matchedItem.Artifact.FinalChecksumPath `
                    -RecordedArtifactPath $matchedItem.Artifact.FinalArtifactPath)) {
                $allFinalPairsValid = $false
                break
            }
        }

        if ($journal.Phase -eq "committed") {
            if (-not $allFinalPairsValid) {
                throw "A committed package publication contains an invalid canonical artifact pair."
            }
            foreach ($matchedItem in $matchedItems) {
                foreach ($backupPath in @(
                        "$($matchedItem.Artifact.FinalArtifactPath).previous",
                        "$($matchedItem.Artifact.FinalChecksumPath).previous"
                    )) {
                    if (Test-Path -LiteralPath $backupPath -PathType Leaf) {
                        Remove-Item -LiteralPath $backupPath -Force -ErrorAction Stop
                    }
                }
            }
            Remove-PackagePublicationJournal -JournalPath $JournalPath
            return
        }

        foreach ($matchedItem in $matchedItems) {
            if ($matchedItem.ArtifactExisted) {
                Restore-PackageArtifactBackups `
                    -FinalArtifactPath $matchedItem.Artifact.FinalArtifactPath `
                    -FinalChecksumPath $matchedItem.Artifact.FinalChecksumPath
                Assert-PackageArtifactChecksum `
                    -ArtifactPath $matchedItem.Artifact.FinalArtifactPath `
                    -ChecksumPath $matchedItem.Artifact.FinalChecksumPath `
                    -RecordedArtifactPath $matchedItem.Artifact.FinalArtifactPath
            } else {
                $pathsToRemove = @(
                    $matchedItem.Artifact.FinalArtifactPath,
                    $matchedItem.Artifact.FinalChecksumPath,
                    "$($matchedItem.Artifact.FinalArtifactPath).previous",
                    "$($matchedItem.Artifact.FinalChecksumPath).previous"
                )
                foreach ($pathToRemove in $pathsToRemove) {
                    if (Test-Path -LiteralPath $pathToRemove -PathType Leaf) {
                        Remove-Item -LiteralPath $pathToRemove -Force -ErrorAction Stop
                    }
                }
                if (@($pathsToRemove | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }).Count -gt 0) {
                    throw "A package artifact that was originally absent could not be removed."
                }
            }
        }
        Remove-PackagePublicationJournal -JournalPath $JournalPath
        return
    }

    if ($JournalPath) {
        Remove-Item -LiteralPath "$JournalPath.partial" -Force -ErrorAction SilentlyContinue
    }

    $allFinalStatesConsistent = $true
    foreach ($artifact in $Artifacts) {
        $artifactExists = Test-Path -LiteralPath $artifact.FinalArtifactPath -PathType Leaf
        $checksumExists = Test-Path -LiteralPath $artifact.FinalChecksumPath -PathType Leaf
        $artifactBackupExists = Test-Path -LiteralPath "$($artifact.FinalArtifactPath).previous" -PathType Leaf
        $checksumBackupExists = Test-Path -LiteralPath "$($artifact.FinalChecksumPath).previous" -PathType Leaf
        if ((-not $artifactExists) -and (-not $checksumExists)) {
            if ($artifactBackupExists -or $checksumBackupExists) {
                $allFinalStatesConsistent = $false
                break
            }
            continue
        }
        if (($artifactExists -ne $checksumExists) -or (-not (Test-PackageArtifactChecksum `
                    -ArtifactPath $artifact.FinalArtifactPath `
                    -ChecksumPath $artifact.FinalChecksumPath `
                    -RecordedArtifactPath $artifact.FinalArtifactPath))) {
            $allFinalStatesConsistent = $false
            break
        }
    }

    if ($allFinalStatesConsistent) {
        foreach ($artifact in $Artifacts) {
            Remove-Item -LiteralPath "$($artifact.FinalArtifactPath).previous" -Force -ErrorAction SilentlyContinue
            Remove-Item -LiteralPath "$($artifact.FinalChecksumPath).previous" -Force -ErrorAction SilentlyContinue
        }
        return
    }

    foreach ($artifact in $Artifacts) {
        Restore-PackageArtifactBackups `
            -FinalArtifactPath $artifact.FinalArtifactPath `
            -FinalChecksumPath $artifact.FinalChecksumPath
        $artifactExists = Test-Path -LiteralPath $artifact.FinalArtifactPath -PathType Leaf
        $checksumExists = Test-Path -LiteralPath $artifact.FinalChecksumPath -PathType Leaf
        if ($artifactExists -ne $checksumExists) {
            throw "Package recovery left an incomplete artifact and checksum pair for $($artifact.FinalArtifactPath)."
        }
        if ($artifactExists) {
            Assert-PackageArtifactChecksum `
                -ArtifactPath $artifact.FinalArtifactPath `
                -ChecksumPath $artifact.FinalChecksumPath `
                -RecordedArtifactPath $artifact.FinalArtifactPath
        }
    }
}

function Publish-PackageArtifactSet {
    param(
        [object[]]$Artifacts,
        [string]$JournalPath = ""
    )

    if ((-not $Artifacts) -or $Artifacts.Count -eq 0) {
        throw "At least one package artifact is required for publication."
    }

    foreach ($artifact in $Artifacts) {
        Assert-PackageArtifactChecksum `
            -ArtifactPath $artifact.PartialArtifactPath `
            -ChecksumPath $artifact.PartialChecksumPath `
            -RecordedArtifactPath $artifact.FinalArtifactPath
    }
    if (-not $JournalPath) {
        $firstArtifactDirectory = [System.IO.Path]::GetDirectoryName(
            [System.IO.Path]::GetFullPath([string]$Artifacts[0].FinalArtifactPath)
        )
        $JournalPath = Join-Path $firstArtifactDirectory ".congregation-package-publication.json"
    }
    Restore-PackageArtifactSetBackups -Artifacts $Artifacts -JournalPath $JournalPath

    $states = @(
        foreach ($artifact in $Artifacts) {
            $artifactExisted = Test-Path -LiteralPath $artifact.FinalArtifactPath -PathType Leaf
            $checksumExisted = Test-Path -LiteralPath $artifact.FinalChecksumPath -PathType Leaf
            if ($artifactExisted -ne $checksumExisted) {
                throw "The existing package publication baseline is incomplete: $($artifact.FinalArtifactPath)"
            }
            if ($artifactExisted) {
                Assert-PackageArtifactChecksum `
                    -ArtifactPath $artifact.FinalArtifactPath `
                    -ChecksumPath $artifact.FinalChecksumPath `
                    -RecordedArtifactPath $artifact.FinalArtifactPath
            }

            [pscustomobject]@{
                Artifact = $artifact
                ArtifactBackupPath = "$($artifact.FinalArtifactPath).previous"
                ChecksumBackupPath = "$($artifact.FinalChecksumPath).previous"
                ArtifactExisted = $artifactExisted
                ChecksumExisted = $checksumExisted
                ArtifactBackedUp = $false
                ChecksumBackedUp = $false
                ArtifactPublished = $false
                ChecksumPublished = $false
            }
        }
    )

    $journalItems = @(
        foreach ($state in $states) {
            [pscustomobject]@{
                FinalArtifactPath = $state.Artifact.FinalArtifactPath
                FinalChecksumPath = $state.Artifact.FinalChecksumPath
                ArtifactExisted = $state.ArtifactExisted
                ChecksumExisted = $state.ChecksumExisted
            }
        }
    )
    Write-PackagePublicationJournal -JournalPath $JournalPath -Phase "prepared" -Items $journalItems

    $committed = $false
    try {
        foreach ($state in $states) {
            if (Test-Path -LiteralPath $state.Artifact.FinalArtifactPath -PathType Leaf) {
                Move-Item -LiteralPath $state.Artifact.FinalArtifactPath -Destination $state.ArtifactBackupPath
                $state.ArtifactBackedUp = $true
            }
            if (Test-Path -LiteralPath $state.Artifact.FinalChecksumPath -PathType Leaf) {
                Move-Item -LiteralPath $state.Artifact.FinalChecksumPath -Destination $state.ChecksumBackupPath
                $state.ChecksumBackedUp = $true
            }
        }

        foreach ($state in $states) {
            Move-Item -LiteralPath $state.Artifact.PartialArtifactPath -Destination $state.Artifact.FinalArtifactPath
            $state.ArtifactPublished = $true
            Move-Item -LiteralPath $state.Artifact.PartialChecksumPath -Destination $state.Artifact.FinalChecksumPath
            $state.ChecksumPublished = $true
        }

        foreach ($state in $states) {
            Assert-PackageArtifactChecksum `
                -ArtifactPath $state.Artifact.FinalArtifactPath `
                -ChecksumPath $state.Artifact.FinalChecksumPath `
                -RecordedArtifactPath $state.Artifact.FinalArtifactPath
        }
        Write-PackagePublicationJournal -JournalPath $JournalPath -Phase "committed" -Items $journalItems
        $committed = $true

        foreach ($state in $states) {
            foreach ($backupPath in @($state.ArtifactBackupPath, $state.ChecksumBackupPath)) {
                if (Test-Path -LiteralPath $backupPath -PathType Leaf) {
                    Remove-Item -LiteralPath $backupPath -Force -ErrorAction Stop
                }
            }
        }
        Remove-PackagePublicationJournal -JournalPath $JournalPath
    } catch {
        $publicationError = $_
        if (-not $committed) {
            try {
                Restore-PackageArtifactSetBackups -Artifacts $Artifacts -JournalPath $JournalPath
            } catch {
                throw "Package publication failed and automatic rollback also failed. The recovery journal was preserved at '$JournalPath'. Original error: $($publicationError.Exception.Message) Rollback error: $($_.Exception.Message)"
            }
        }
        throw $publicationError
    }
}

function Publish-PackageArtifact {
    param(
        [string]$PartialArtifactPath,
        [string]$FinalArtifactPath,
        [string]$PartialChecksumPath,
        [string]$FinalChecksumPath
    )

    Publish-PackageArtifactSet -Artifacts @(
        [pscustomobject]@{
            PartialArtifactPath = $PartialArtifactPath
            FinalArtifactPath = $FinalArtifactPath
            PartialChecksumPath = $PartialChecksumPath
            FinalChecksumPath = $FinalChecksumPath
        }
    )
}

function Resolve-SignTool {
    param([string]$ConfiguredPath)

    if ($ConfiguredPath) {
        $resolvedConfiguredPath = [System.IO.Path]::GetFullPath($ConfiguredPath)
        if (-not (Test-Path -LiteralPath $resolvedConfiguredPath -PathType Leaf)) {
            throw "Configured SignTool was not found: $resolvedConfiguredPath"
        }
        return $resolvedConfiguredPath
    }

    $windowsKitRoots = @()
    foreach ($registryPath in @(
        "HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots",
        "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows Kits\Installed Roots"
    )) {
        $installedRoots = Get-ItemProperty -LiteralPath $registryPath -ErrorAction SilentlyContinue
        if ($installedRoots -and
            ($installedRoots.PSObject.Properties.Name -contains "KitsRoot10") -and
            $installedRoots.KitsRoot10) {
            $windowsKitRoots += $installedRoots.KitsRoot10
        }
    }

    $programFilesX86 = [Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)
    if ($programFilesX86) {
        $windowsKitRoots += Join-Path $programFilesX86 "Windows Kits\10"
    }

    $candidates = @()
    foreach ($kitRoot in @($windowsKitRoots | Select-Object -Unique)) {
        $binRoot = Join-Path $kitRoot "bin"
        if (Test-Path -LiteralPath $binRoot -PathType Container) {
            foreach ($versionDirectory in @(Get-ChildItem -LiteralPath $binRoot -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending)) {
                $candidates += Join-Path $versionDirectory.FullName "x64\signtool.exe"
            }
        }
        $candidates += Join-Path $binRoot "x64\signtool.exe"
    }

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw "SignTool was not found. Install the Windows SDK Signing Tools or set SIGNTOOL_PATH."
}

function Normalize-CertificateThumbprint {
    param([string]$Thumbprint)

    $thumbprintText = if ($null -eq $Thumbprint) { "" } else { [string]$Thumbprint }
    $normalized = ($thumbprintText -replace '\s', '').ToUpperInvariant()
    if ($normalized -notmatch '^[0-9A-F]{40}$') {
        throw "Code-signing certificate thumbprint must contain exactly 40 hexadecimal characters."
    }
    return $normalized
}

function Assert-CodeSigningCertificateKeyUsage {
    param($Certificate)

    $keyUsageExtension = @(
        $Certificate.Extensions |
            Where-Object { $_.Oid.Value -eq "2.5.29.15" }
    ) | Select-Object -First 1
    if (-not $keyUsageExtension) {
        return
    }

    $digitalSignature = [System.Security.Cryptography.X509Certificates.X509KeyUsageFlags]::DigitalSignature
    if (($keyUsageExtension.KeyUsages -band $digitalSignature) -eq 0) {
        throw "The selected certificate Key Usage does not allow DigitalSignature."
    }
}

function Get-EnhancedKeyUsageObjectIdentifier {
    param($Usage)

    if ($null -eq $Usage) {
        return ""
    }

    $objectIdProperty = $Usage.PSObject.Properties["ObjectId"]
    if (-not $objectIdProperty) {
        return ""
    }

    $objectId = $objectIdProperty.Value
    if ($null -eq $objectId) {
        return ""
    }

    $valueProperty = $objectId.PSObject.Properties["Value"]
    if ($valueProperty) {
        return [string]$valueProperty.Value
    }

    return [string]$objectId
}

function Assert-PublicCodeSigningChainEvidence {
    param(
        $Certificate,
        [bool]$ChainBuildSucceeded,
        [object[]]$ChainCertificates,
        [string[]]$MicrosoftTrustedRootThumbprints,
        [string[]]$ChainStatusDescriptions
    )

    if (-not $ChainBuildSucceeded) {
        $statusText = @($ChainStatusDescriptions | Where-Object { $_ }) -join "; "
        if (-not $statusText) {
            $statusText = "No chain status was reported."
        }
        throw "The consumer-trusted certificate chain validation failed: $statusText"
    }

    $leafThumbprint = Normalize-CertificateThumbprint -Thumbprint $Certificate.Thumbprint
    $rootCertificate = $ChainCertificates[$ChainCertificates.Count - 1]
    $rootThumbprint = Normalize-CertificateThumbprint -Thumbprint $rootCertificate.Thumbprint
    if ($leafThumbprint -eq $rootThumbprint) {
        throw "Public release signing rejects self-signed certificate chains. Use -AllowUnsignedPackage for local testing."
    }

    if ($ChainCertificates.Count -lt 2) {
        throw "The consumer-trusted certificate chain must contain a publisher certificate and a public trust root."
    }

    $trustedRootThumbprints = @(
        $MicrosoftTrustedRootThumbprints |
            Where-Object { $_ } |
            ForEach-Object { Normalize-CertificateThumbprint -Thumbprint $_ }
    )
    if ($trustedRootThumbprints -notcontains $rootThumbprint) {
        throw "The signing certificate chain root is not present in the LocalMachine Microsoft AuthRoot store."
    }

    return [pscustomobject]@{
        RootSubject = $rootCertificate.Subject
        RootThumbprint = $rootThumbprint
        ChainLength = $ChainCertificates.Count
    }
}

function Resolve-PublicCodeSigningCertificateTrust {
    param($Certificate)

    $chain = [System.Security.Cryptography.X509Certificates.X509Chain]::new()
    try {
        $chain.ChainPolicy.RevocationMode = [System.Security.Cryptography.X509Certificates.X509RevocationMode]::Online
        $chain.ChainPolicy.RevocationFlag = [System.Security.Cryptography.X509Certificates.X509RevocationFlag]::ExcludeRoot
        $chain.ChainPolicy.VerificationFlags = [System.Security.Cryptography.X509Certificates.X509VerificationFlags]::NoFlag
        $chain.ChainPolicy.UrlRetrievalTimeout = [TimeSpan]::FromSeconds(30)
        $chain.ChainPolicy.VerificationTime = Get-Date
        [void]$chain.ChainPolicy.ApplicationPolicy.Add(
            [System.Security.Cryptography.Oid]::new("1.3.6.1.5.5.7.3.3")
        )

        $chainBuildSucceeded = $chain.Build($Certificate)
        $chainCertificates = @(
            $chain.ChainElements |
                ForEach-Object { $_.Certificate }
        )
        $chainStatusDescriptions = @(
            $chain.ChainStatus |
                ForEach-Object {
                    "$($_.Status): $($_.StatusInformation.Trim())"
                }
        )
        $microsoftTrustedRootThumbprints = @(
            Get-ChildItem -LiteralPath "Cert:\LocalMachine\AuthRoot" -ErrorAction Stop |
                ForEach-Object { $_.Thumbprint }
        )

        return Assert-PublicCodeSigningChainEvidence `
            -Certificate $Certificate `
            -ChainBuildSucceeded $chainBuildSucceeded `
            -ChainCertificates $chainCertificates `
            -MicrosoftTrustedRootThumbprints $microsoftTrustedRootThumbprints `
            -ChainStatusDescriptions $chainStatusDescriptions
    } finally {
        $chain.Dispose()
    }
}

function Resolve-CodeSigningCertificate {
    param(
        [string]$CertificateThumbprint,
        [ValidateSet("CurrentUser", "LocalMachine")]
        [string]$StoreLocation
    )

    $normalizedThumbprint = Normalize-CertificateThumbprint -Thumbprint $CertificateThumbprint
    $certificatePath = "Cert:\$StoreLocation\My\$normalizedThumbprint"
    $certificate = Get-Item -LiteralPath $certificatePath -ErrorAction SilentlyContinue
    if (-not $certificate) {
        throw "Code-signing certificate was not found in $StoreLocation\\My."
    }
    if (-not $certificate.HasPrivateKey) {
        throw "The selected code-signing certificate has no accessible private key."
    }

    $now = Get-Date
    if (($certificate.NotBefore -gt $now) -or ($certificate.NotAfter -le $now)) {
        throw "The selected code-signing certificate is outside its validity period."
    }

    $hasCodeSigningEku = @(
        $certificate.EnhancedKeyUsageList |
            Where-Object {
                (Get-EnhancedKeyUsageObjectIdentifier -Usage $_) -eq "1.3.6.1.5.5.7.3.3"
            }
    ).Count -gt 0
    if (-not $hasCodeSigningEku) {
        throw "The selected certificate does not contain the Code Signing EKU."
    }
    Assert-CodeSigningCertificateKeyUsage -Certificate $certificate

    $publicTrustEvidence = Resolve-PublicCodeSigningCertificateTrust -Certificate $certificate
    Write-Host "Publisher subject: $($certificate.Subject)"
    Write-Host "Microsoft public trust root: $($publicTrustEvidence.RootSubject)"

    return $certificate
}

function Sign-AuthenticodeFile {
    param(
        [string]$SignTool,
        [string]$File,
        [string]$CertificateThumbprint,
        [ValidateSet("CurrentUser", "LocalMachine")]
        [string]$StoreLocation,
        [string]$TimestampUrl
    )

    $arguments = @(
        "sign",
        "/fd", "SHA256",
        "/tr", $TimestampUrl,
        "/td", "SHA256",
        "/sha1", $CertificateThumbprint,
        "/s", "My",
        "/d", "Congregation"
    )
    if ($StoreLocation -eq "LocalMachine") {
        $arguments += "/sm"
    }
    $arguments += $File
    Invoke-Native $SignTool $arguments
}

function Verify-AuthenticodeFile {
    param(
        [string]$SignTool,
        [string]$File,
        [string]$ExpectedCertificateThumbprint = ""
    )

    $signature = Get-AuthenticodeSignature -LiteralPath $File
    if ($signature.Status -ne "Valid") {
        throw "Authenticode trust validation failed for ${File}: $($signature.Status)"
    }
    if (-not $signature.SignerCertificate) {
        throw "Authenticode signer certificate is missing: $File"
    }
    if ($ExpectedCertificateThumbprint) {
        $expected = Normalize-CertificateThumbprint -Thumbprint $ExpectedCertificateThumbprint
        if ($signature.SignerCertificate.Thumbprint -ne $expected) {
            throw "Authenticode signer does not match the requested certificate: $File"
        }
    }
    if (-not $signature.TimeStamperCertificate) {
        throw "RFC 3161 timestamp is missing: $File"
    }

    Invoke-Native $SignTool @("verify", "/pa", "/all", "/tw", "/v", $File)
}

function Get-StagedPeFiles {
    param([string]$Directory)

    return @(
        Get-ChildItem -LiteralPath $Directory -Recurse -File -ErrorAction Stop |
            Where-Object { $_.Extension -in @(".exe", ".dll") } |
            Sort-Object FullName
    )
}

function Get-CongregationOwnedStageFiles {
    param([string]$Directory)

    return @(
        foreach ($fileName in @("Congregation.exe", "LVRS.dll", "libiiPaintEngine.dll")) {
            $path = Join-Path $Directory $fileName
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
                throw "Congregation-owned signing target is missing: $path"
            }
            Get-Item -LiteralPath $path
        }

        $sharedCanvasCandidates = @(
            @("iiSharedCanvas.dll", "libiiSharedCanvas.dll") |
                ForEach-Object { Join-Path $Directory $_ } |
                Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
        )
        if ($sharedCanvasCandidates.Count -ne 1) {
            throw "Congregation-owned signing target must contain exactly one iiSharedCanvas runtime DLL."
        }
        Get-Item -LiteralPath $sharedCanvasCandidates[0]

        $updateManagerCandidates = @(
            @("iiUpdateManager.dll", "libiiUpdateManager.dll") |
                ForEach-Object { Join-Path $Directory $_ } |
                Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
        )
        if ($updateManagerCandidates.Count -ne 1) {
            throw "Congregation-owned signing target must contain exactly one iiUpdateManager runtime DLL."
        }
        Get-Item -LiteralPath $updateManagerCandidates[0]

        $licenseManagerCandidates = @(
            @("iiLicenseManager.dll", "libiiLicenseManager.dll") |
                ForEach-Object { Join-Path $Directory $_ } |
                Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
        )
        if ($licenseManagerCandidates.Count -ne 1) {
            throw "Congregation-owned signing target must contain exactly one iiLicenseManager runtime DLL."
        }
        Get-Item -LiteralPath $licenseManagerCandidates[0]
    )
}

function Sign-WindowsStage {
    param(
        [string]$Directory,
        [string]$SignTool,
        [string]$CertificateThumbprint,
        [ValidateSet("CurrentUser", "LocalMachine")]
        [string]$StoreLocation,
        [string]$TimestampUrl
    )

    $ownedPaths = @{}
    foreach ($ownedFile in Get-CongregationOwnedStageFiles -Directory $Directory) {
        $ownedPaths[$ownedFile.FullName] = $true
    }

    foreach ($file in Get-StagedPeFiles -Directory $Directory) {
        $signature = Get-AuthenticodeSignature -LiteralPath $file.FullName
        $isCongregationOwned = $ownedPaths.ContainsKey($file.FullName)
        if (($signature.Status -eq "Valid") -and (-not $isCongregationOwned)) {
            continue
        }
        if (($signature.Status -ne "NotSigned") -and ($signature.Status -ne "Valid")) {
            throw "Refusing to replace a non-valid existing Authenticode signature on $($file.FullName): $($signature.Status)"
        }

        Sign-AuthenticodeFile `
            -SignTool $SignTool `
            -File $file.FullName `
            -CertificateThumbprint $CertificateThumbprint `
            -StoreLocation $StoreLocation `
            -TimestampUrl $TimestampUrl
        Verify-AuthenticodeFile `
            -SignTool $SignTool `
            -File $file.FullName `
            -ExpectedCertificateThumbprint $CertificateThumbprint
    }
}

function Verify-WindowsStageSignatures {
    param(
        [string]$Directory,
        [string]$SignTool,
        [string]$CertificateThumbprint
    )

    $ownedPaths = @{}
    foreach ($ownedFile in Get-CongregationOwnedStageFiles -Directory $Directory) {
        $ownedPaths[$ownedFile.FullName] = $true
    }

    foreach ($file in Get-StagedPeFiles -Directory $Directory) {
        if ($ownedPaths.ContainsKey($file.FullName)) {
            Verify-AuthenticodeFile -SignTool $SignTool -File $file.FullName -ExpectedCertificateThumbprint $CertificateThumbprint
        } else {
            Verify-AuthenticodeFile -SignTool $SignTool -File $file.FullName
        }
    }
}

function Write-Sha256File {
    param(
        [string]$File,
        [string]$OutputPath,
        [string]$RecordedFileName = ""
    )

    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $File).Hash.ToLowerInvariant()
    $fileName = if ($RecordedFileName) { $RecordedFileName } else { [System.IO.Path]::GetFileName($File) }
    [System.IO.File]::WriteAllText(
        $OutputPath,
        "$hash *$fileName`r`n",
        [System.Text.Encoding]::ASCII
    )
    Write-Host "SHA-256: $OutputPath"
}

function Test-AnyPath {
    param([string[]]$Paths)

    foreach ($path in $Paths) {
        if ($path -and (Test-Path $path)) {
            return $path
        }
    }

    return ""
}

function Resolve-QtPrefix {
    param([string]$ConfiguredPrefix)

    $candidates = @()
    if ($ConfiguredPrefix) {
        $candidates += $ConfiguredPrefix
    }
    if ($env:QTDIR) {
        $candidates += $env:QTDIR
    }

    $windeployqtCommand = Get-Command "windeployqt.exe" -ErrorAction SilentlyContinue
    if ($windeployqtCommand) {
        $candidates += Split-Path -Parent (Split-Path -Parent $windeployqtCommand.Source)
    }

    if (Test-Path "C:\Qt") {
        $qtKits = Get-ChildItem -Path "C:\Qt" -Directory -ErrorAction SilentlyContinue |
            ForEach-Object {
                Get-ChildItem -Path $_.FullName -Directory -ErrorAction SilentlyContinue |
                    Where-Object {
                        Test-Path (Join-Path $_.FullName "bin\windeployqt.exe")
                    }
            } |
            Sort-Object FullName -Descending
        foreach ($kit in $qtKits) {
            $candidates += $kit.FullName
        }
    }

    foreach ($prefix in $candidates) {
        $windeployqt = Join-Path $prefix "bin\windeployqt.exe"
        $qtConfig = Join-Path $prefix "lib\cmake\Qt6\Qt6Config.cmake"
        if ((Test-Path $windeployqt) -and (Test-Path $qtConfig)) {
            return (Resolve-Path $prefix).Path
        }
    }

    throw "Qt for Windows was not found. Set QT_PREFIX to a Qt kit such as C:\Qt\6.8.3\msvc2022_64."
}

function Resolve-DependencyPrefix {
    param(
        [string]$Name,
        [string]$ConfiguredPrefix,
        [string]$DefaultPrefix,
        [string[]]$ConfigRelativePaths
    )

    $candidates = @()
    if ($ConfiguredPrefix) {
        $candidates += $ConfiguredPrefix
    }
    if ($DefaultPrefix) {
        $candidates += $DefaultPrefix
    }

    foreach ($prefix in $candidates) {
        foreach ($relativePath in $ConfigRelativePaths) {
            if (Test-Path (Join-Path $prefix $relativePath)) {
                return (Resolve-Path $prefix).Path
            }
        }
    }

    throw "$Name was not found. Install the Windows build of $Name and set the matching environment variable or script parameter."
}

function Resolve-DependencyLicenseFile {
    param(
        [string]$Name,
        [string]$Prefix,
        [string[]]$RelativeCandidates,
        [string[]]$AdditionalCandidates = @()
    )

    $candidates = @(
        foreach ($relativePath in $RelativeCandidates) {
            if ($Prefix) {
                Join-Path $Prefix $relativePath
            }
        }
        $AdditionalCandidates
    )
    foreach ($candidate in $candidates) {
        if (-not $candidate -or (-not (Test-Path -LiteralPath $candidate -PathType Leaf))) {
            continue
        }
        $item = Get-Item -LiteralPath $candidate -ErrorAction Stop
        if ($item.Length -le 0) {
            throw "$Name license file is empty: $candidate"
        }
        return $item.FullName
    }

    return ""
}

function Assert-PublicDistributionEvidence {
    param(
        [bool]$PublicRelease,
        [string]$IiPaintEngineLicenseFile,
        [string]$IiSharedCanvasLicenseFile,
        [string]$IiLicenseManagerNoticeFile,
        [string]$SourceUrl,
        [string]$SourceSha256
    )

    if (-not $PublicRelease) {
        return
    }
    if (-not $IiPaintEngineLicenseFile -or (-not (Test-Path -LiteralPath $IiPaintEngineLicenseFile -PathType Leaf))) {
        throw "Public Windows packaging requires an explicit iiPaintEngine LICENSE supplied by its copyright holder."
    }
    if ((Get-Item -LiteralPath $IiPaintEngineLicenseFile).Length -le 0) {
        throw "Public Windows packaging rejects an empty iiPaintEngine LICENSE."
    }
    if (-not $IiSharedCanvasLicenseFile -or (-not (Test-Path -LiteralPath $IiSharedCanvasLicenseFile -PathType Leaf))) {
        throw "Public Windows packaging requires the iiSharedCanvas LICENSE."
    }
    if ((Get-Item -LiteralPath $IiSharedCanvasLicenseFile).Length -le 0) {
        throw "Public Windows packaging rejects an empty iiSharedCanvas LICENSE."
    }
    if (-not $IiLicenseManagerNoticeFile -or (-not (Test-Path -LiteralPath $IiLicenseManagerNoticeFile -PathType Leaf))) {
        throw "Public Windows packaging requires iiLicenseManager third-party notices."
    }
    if ((Get-Item -LiteralPath $IiLicenseManagerNoticeFile).Length -le 0) {
        throw "Public Windows packaging rejects empty iiLicenseManager third-party notices."
    }

    $sourceUri = $null
    $validSourceUri = [System.Uri]::TryCreate($SourceUrl, [System.UriKind]::Absolute, [ref]$sourceUri)
    if ((-not $validSourceUri) -or $sourceUri.Scheme -ne "https") {
        throw "Public Windows packaging requires CONGREGATION_CORRESPONDING_SOURCE_URL as an absolute HTTPS URL controlled by the publisher."
    }

    $normalizedSourceHash = ($SourceSha256 | Out-String).Trim().ToUpperInvariant()
    if ($normalizedSourceHash -notmatch '^[0-9A-F]{64}$') {
        throw "Public Windows packaging requires the exact 64-hex CONGREGATION_CORRESPONDING_SOURCE_SHA256 value."
    }
}

function Copy-LegalFile {
    param(
        [string]$Source,
        [string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Required legal material is missing: $Source"
    }
    if ((Get-Item -LiteralPath $Source).Length -le 0) {
        throw "Required legal material is empty: $Source"
    }

    $destinationDirectory = Split-Path -Parent $Destination
    New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
    $sourceHash = (Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash
    $destinationHash = (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash
    if ($sourceHash -ne $destinationHash) {
        throw "A staged legal file differs from its source: $Destination"
    }
}

function Copy-LegalDirectory {
    param(
        [string]$Source,
        [string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Container)) {
        throw "Required legal material directory is missing: $Source"
    }
    $sourceFiles = @(Get-ChildItem -LiteralPath $Source -File -Recurse -ErrorAction Stop)
    if ($sourceFiles.Count -eq 0) {
        throw "Required legal material directory is empty: $Source"
    }

    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    foreach ($sourceFile in $sourceFiles) {
        $relativePath = $sourceFile.FullName.Substring($Source.TrimEnd('\').Length).TrimStart('\')
        Copy-LegalFile -Source $sourceFile.FullName -Destination (Join-Path $Destination $relativePath)
    }
}

function Resolve-QtGlobalLicenseDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$QtSourceRoot
    )

    $requiredFiles = @(
        "GPL-2.0-only.txt",
        "GPL-3.0-only.txt",
        "LGPL-3.0-only.txt",
        "LicenseRef-Qt-Commercial.txt",
        "Qt-GPL-exception-1.0.txt"
    )
    $candidates = @(
        (Join-Path $QtSourceRoot "LICENSES"),
        (Join-Path $QtSourceRoot "qtbase\LICENSES")
    )

    foreach ($candidate in $candidates) {
        if (-not (Test-Path -LiteralPath $candidate -PathType Container)) {
            continue
        }

        $complete = $true
        foreach ($requiredFile in $requiredFiles) {
            $path = Join-Path $candidate $requiredFile
            if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
                (Get-Item -LiteralPath $path).Length -le 0) {
                $complete = $false
                break
            }
        }
        if ($complete) {
            return $candidate
        }
    }

    throw "The matching Qt source tree does not contain a complete global license set under LICENSES or qtbase\LICENSES: $QtSourceRoot"
}

function Resolve-StagedMinGwToolchainRoot {
    param(
        [string]$Directory,
        [string]$ResolvedQtPrefix
    )

    $qtInstallRoot = Split-Path -Parent (Split-Path -Parent $ResolvedQtPrefix)
    $toolsDirectory = Join-Path $qtInstallRoot "Tools"
    $runtimeNames = @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")
    $stagedRuntimeNames = @($runtimeNames | Where-Object { Test-Path -LiteralPath (Join-Path $Directory $_) })
    if ($stagedRuntimeNames.Count -eq 0) {
        throw "The staged MinGW runtime DLLs were not found for legal-material matching."
    }

    foreach ($candidate in Get-ChildItem -LiteralPath $toolsDirectory -Directory -ErrorAction SilentlyContinue) {
        $allMatch = $true
        foreach ($runtimeName in $stagedRuntimeNames) {
            $candidateRuntime = Join-Path $candidate.FullName "bin\$runtimeName"
            if ((-not (Test-Path -LiteralPath $candidateRuntime -PathType Leaf)) -or
                ((Get-FileHash -LiteralPath $candidateRuntime -Algorithm SHA256).Hash -ne
                    (Get-FileHash -LiteralPath (Join-Path $Directory $runtimeName) -Algorithm SHA256).Hash)) {
                $allMatch = $false
                break
            }
        }
        if ($allMatch -and
            (Test-Path -LiteralPath (Join-Path $candidate.FullName "licenses\gcc\COPYING.RUNTIME")) -and
            (Test-Path -LiteralPath (Join-Path $candidate.FullName "licenses\mingw-w64\COPYING.MinGW-w64-runtime.txt")) -and
            (Test-Path -LiteralPath (Join-Path $candidate.FullName "licenses\winpthreads\COPYING"))) {
            return $candidate.FullName
        }
    }

    throw "The exact MinGW toolchain license directory matching the staged runtime DLLs was not found under $toolsDirectory."
}

function Assert-WindowsLegalMaterials {
    param(
        [string]$Directory,
        [bool]$PublicRelease,
        [bool]$MinGwRuntime
    )

    $requiredPaths = @(
        "LICENSE.txt",
        "THIRD_PARTY_NOTICES.txt",
        "SOURCE_OFFER.txt",
        "legal\LVRS\LICENSE.txt",
        "legal\iiSharedCanvas\LICENSE.txt",
        "legal\iiLicenseManager\THIRD_PARTY_NOTICES.md",
        "legal\QtKeychain\COPYING.txt",
        "legal\psd_sdk\BSD-2-Clause.txt",
        "legal\psd_sdk\miniz-Unlicense.txt",
        "legal\Pretendard\OFL-1.1.txt",
        "legal\Qt\LICENSES\global\LGPL-3.0-only.txt",
        "legal\Qt\SBOM\qtbase.spdx.json",
        "legal\Qt\SBOM\qtdeclarative.spdx.json"
    )
    if ($MinGwRuntime) {
        $requiredPaths += @(
            "legal\MinGW\GPL-3.0.txt",
            "legal\MinGW\GCC-Runtime-Library-Exception-3.1.txt",
            "legal\MinGW\COPYING.MinGW-w64-runtime.txt",
            "legal\MinGW\winpthreads-COPYING.txt"
        )
    }
    if ($PublicRelease) {
        $requiredPaths += "legal\iiPaintEngine\LICENSE.txt"
    }

    foreach ($relativePath in $requiredPaths) {
        $path = Join-Path $Directory $relativePath
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Windows legal-material staging is incomplete: $relativePath"
        }
        if ((Get-Item -LiteralPath $path).Length -le 0) {
            throw "Windows legal material is empty: $relativePath"
        }
    }

    if ($PublicRelease) {
        $releaseEvidence = (Get-Content -LiteralPath (Join-Path $Directory "SOURCE_OFFER.txt") -Raw) +
            (Get-Content -LiteralPath (Join-Path $Directory "THIRD_PARTY_NOTICES.txt") -Raw)
        if ($releaseEvidence -match '(?i)\b(TBD|UNKNOWN|UNLICENSED)\b|local test package') {
            throw "Public Windows legal materials contain an unresolved placeholder."
        }
    }
}

function Copy-WindowsLegalMaterials {
    param(
        [string]$Directory,
        [string]$ResolvedQtPrefix,
        [string]$LvrsLicenseFile,
        [string]$IiPaintEngineLicenseFile,
        [string]$IiSharedCanvasLicenseFile,
        [string]$IiLicenseManagerNoticeFile,
        [bool]$PublicRelease,
        [string]$SourceUrl,
        [string]$SourceSha256
    )

    $legalRoot = Join-Path $Directory "legal"
    $packagingRoot = Join-Path $RepositoryRoot "packaging\windows"
    Remove-Item -LiteralPath $legalRoot -Recurse -Force -ErrorAction SilentlyContinue

    Copy-LegalFile -Source (Join-Path $RepositoryRoot "LICENSE") -Destination (Join-Path $Directory "LICENSE.txt")
    Copy-LegalFile -Source (Join-Path $packagingRoot "THIRD_PARTY_NOTICES.txt") -Destination (Join-Path $Directory "THIRD_PARTY_NOTICES.txt")
    Copy-LegalFile -Source $LvrsLicenseFile -Destination (Join-Path $legalRoot "LVRS\LICENSE.txt")
    Copy-LegalFile -Source $IiSharedCanvasLicenseFile -Destination (Join-Path $legalRoot "iiSharedCanvas\LICENSE.txt")
    Copy-LegalFile -Source $IiLicenseManagerNoticeFile -Destination (Join-Path $legalRoot "iiLicenseManager\THIRD_PARTY_NOTICES.md")
    Copy-LegalFile `
        -Source (Join-Path $RepositoryRoot "packaging\common\licenses\QtKeychain-BSD-3-Clause.txt") `
        -Destination (Join-Path $legalRoot "QtKeychain\COPYING.txt")
    if ($IiPaintEngineLicenseFile) {
        Copy-LegalFile -Source $IiPaintEngineLicenseFile -Destination (Join-Path $legalRoot "iiPaintEngine\LICENSE.txt")
    } else {
        Write-Warning "iiPaintEngine has no explicit license. The unsigned package remains local-test-only."
    }
    Copy-LegalFile `
        -Source (Join-Path $packagingRoot "licenses\psd_sdk-BSD-2-Clause.txt") `
        -Destination (Join-Path $legalRoot "psd_sdk\BSD-2-Clause.txt")
    Copy-LegalFile `
        -Source (Join-Path $packagingRoot "licenses\psd_sdk-miniz-Unlicense.txt") `
        -Destination (Join-Path $legalRoot "psd_sdk\miniz-Unlicense.txt")
    Copy-LegalFile `
        -Source (Join-Path $packagingRoot "licenses\Pretendard-OFL-1.1.txt") `
        -Destination (Join-Path $legalRoot "Pretendard\OFL-1.1.txt")

    $qtVersionRoot = Split-Path -Parent $ResolvedQtPrefix
    $qtVersion = Split-Path -Leaf $qtVersionRoot
    $qtSourceRoot = Join-Path $qtVersionRoot "Src"
    $qtGlobalLicenseDirectory = Resolve-QtGlobalLicenseDirectory -QtSourceRoot $qtSourceRoot
    Copy-LegalDirectory `
        -Source $qtGlobalLicenseDirectory `
        -Destination (Join-Path $legalRoot "Qt\LICENSES\global")
    $qtModules = @("qtbase", "qtdeclarative", "qtsvg", "qtimageformats", "qttranslations")
    foreach ($qtModule in $qtModules) {
        Copy-LegalDirectory `
            -Source (Join-Path $qtSourceRoot "$qtModule\LICENSES") `
            -Destination (Join-Path $legalRoot "Qt\LICENSES\$qtModule")
        Copy-LegalFile `
            -Source (Join-Path $ResolvedQtPrefix "sbom\$qtModule-$qtVersion.spdx.json") `
            -Destination (Join-Path $legalRoot "Qt\SBOM\$qtModule.spdx.json")
    }

    $minGwRuntime = $ResolvedQtPrefix -match 'mingw'
    if ($minGwRuntime) {
        $minGwRoot = Resolve-StagedMinGwToolchainRoot -Directory $Directory -ResolvedQtPrefix $ResolvedQtPrefix
        Copy-LegalFile -Source (Join-Path $minGwRoot "licenses\gcc\COPYING3") -Destination (Join-Path $legalRoot "MinGW\GPL-3.0.txt")
        Copy-LegalFile -Source (Join-Path $minGwRoot "licenses\gcc\COPYING.RUNTIME") -Destination (Join-Path $legalRoot "MinGW\GCC-Runtime-Library-Exception-3.1.txt")
        Copy-LegalFile -Source (Join-Path $minGwRoot "licenses\mingw-w64\COPYING.MinGW-w64-runtime.txt") -Destination (Join-Path $legalRoot "MinGW\COPYING.MinGW-w64-runtime.txt")
        Copy-LegalFile -Source (Join-Path $minGwRoot "licenses\winpthreads\COPYING") -Destination (Join-Path $legalRoot "MinGW\winpthreads-COPYING.txt")
    }

    $sourceOffer = if ($PublicRelease) {
        @"
Congregation $Version corresponding source
========================================

The corresponding source for this exact release, including the sources needed
to comply with the GNU AGPL and LGPL components conveyed with Congregation, is
available without charge from the publisher-controlled location below.

URL: $SourceUrl
SHA-256: $($SourceSha256.ToUpperInvariant())
"@
    } else {
        @"
LOCAL TEST PACKAGE - NOT FOR DISTRIBUTION
=========================================

This unsigned package exists only for local installation testing. No public
corresponding-source offer is finalized for this artifact.
"@
    }
    [System.IO.File]::WriteAllText(
        (Join-Path $Directory "SOURCE_OFFER.txt"),
        $sourceOffer.Trim() + "`r`n",
        (New-Object System.Text.UTF8Encoding($false))
    )

    Assert-WindowsLegalMaterials -Directory $Directory -PublicRelease $PublicRelease -MinGwRuntime $minGwRuntime
}

function Resolve-Objdump {
    param([string]$ResolvedQtPrefix)

    $candidates = @()
    $command = Get-Command "objdump.exe" -ErrorAction SilentlyContinue
    if ($command) {
        $candidates += $command.Source
    }

    if ($ResolvedQtPrefix -match "mingw") {
        $qtRoot = Split-Path -Parent (Split-Path -Parent $ResolvedQtPrefix)
        if ($qtRoot) {
            $candidates += Get-ChildItem -Path (Join-Path $qtRoot "Tools") -Filter "objdump.exe" -Recurse -File -ErrorAction SilentlyContinue |
                Select-Object -ExpandProperty FullName
        }
    }

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    return ""
}

function Resolve-Strip {
    param([string]$ResolvedQtPrefix)

    $candidates = @()
    $command = Get-Command "strip.exe" -ErrorAction SilentlyContinue
    if ($command) {
        $candidates += $command.Source
    }

    $objdump = Resolve-Objdump -ResolvedQtPrefix $ResolvedQtPrefix
    if ($objdump) {
        $candidates += Join-Path (Split-Path -Parent $objdump) "strip.exe"
    }

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    return ""
}

function Test-BinaryMentionsSymbol {
    param(
        [string]$Objdump,
        [string]$Binary,
        [string]$Symbol
    )

    $output = & $Objdump -p $Binary 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to inspect binary imports: $Binary"
    }

    return @($output | Select-String -SimpleMatch $Symbol).Count -gt 0
}

function Get-PeImportedDllNames {
    param(
        [string]$Objdump,
        [string]$Binary
    )

    $output = & $Objdump -p $Binary 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to inspect PE imports: $Binary"
    }

    return @(
        foreach ($line in $output) {
            if ($line -match 'DLL Name:\s*(?<Name>\S+)\s*$') {
                $Matches.Name
            }
        }
    ) | Sort-Object -Unique
}

function Get-PeHeader {
    param([string]$Binary)

    [byte[]]$bytes = [System.IO.File]::ReadAllBytes($Binary)
    if (($bytes.Length -lt 0x40) -or ($bytes[0] -ne 0x4d) -or ($bytes[1] -ne 0x5a)) {
        throw "Not a valid PE image: $Binary"
    }

    $peOffset = [System.BitConverter]::ToInt32($bytes, 0x3c)
    if (($peOffset -lt 0) -or ($peOffset + 96 -gt $bytes.Length)) {
        throw "Invalid PE header offset: $Binary"
    }
    if ([System.BitConverter]::ToUInt32($bytes, $peOffset) -ne 0x00004550) {
        throw "PE signature is missing: $Binary"
    }

    $coffHeaderOffset = $peOffset + 4
    $optionalHeaderOffset = $coffHeaderOffset + 20
    return [pscustomobject]@{
        Machine = [System.BitConverter]::ToUInt16($bytes, $coffHeaderOffset)
        PointerToSymbolTable = [System.BitConverter]::ToUInt32($bytes, $coffHeaderOffset + 8)
        OptionalHeaderMagic = [System.BitConverter]::ToUInt16($bytes, $optionalHeaderOffset)
        MajorOperatingSystemVersion = [System.BitConverter]::ToUInt16($bytes, $optionalHeaderOffset + 40)
        MinorOperatingSystemVersion = [System.BitConverter]::ToUInt16($bytes, $optionalHeaderOffset + 42)
        MajorSubsystemVersion = [System.BitConverter]::ToUInt16($bytes, $optionalHeaderOffset + 48)
        MinorSubsystemVersion = [System.BitConverter]::ToUInt16($bytes, $optionalHeaderOffset + 50)
        Subsystem = [System.BitConverter]::ToUInt16($bytes, $optionalHeaderOffset + 68)
        DllCharacteristics = [System.BitConverter]::ToUInt16($bytes, $optionalHeaderOffset + 70)
    }
}

function Get-PeSubsystem {
    param([string]$Binary)

    return (Get-PeHeader -Binary $Binary).Subsystem
}

function Assert-MinGwRuntimeCompatibility {
    param(
        [string]$Directory,
        [string]$ResolvedQtPrefix
    )

    if ($ResolvedQtPrefix -notmatch "mingw") {
        return
    }

    $objdump = Resolve-Objdump -ResolvedQtPrefix $ResolvedQtPrefix
    if (-not $objdump) {
        throw "objdump.exe was not found. MinGW Windows packages require objdump-based DLL compatibility checks."
    }

    $libstdcpp = Join-Path $Directory "libstdc++-6.dll"
    if (-not (Test-Path $libstdcpp)) {
        throw "MinGW runtime deployment is incomplete: libstdc++-6.dll is missing."
    }

    $symbol = "__cxa_thread_atexit"
    $runtimeExportsSymbol = Test-BinaryMentionsSymbol -Objdump $objdump -Binary $libstdcpp -Symbol $symbol
    $incompatibleDlls = @(
        Get-ChildItem -Path $Directory -Filter "*.dll" -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -ne "libstdc++-6.dll" } |
            Where-Object { Test-BinaryMentionsSymbol -Objdump $objdump -Binary $_.FullName -Symbol $symbol }
    )

    if (($incompatibleDlls.Count -gt 0) -and (-not $runtimeExportsSymbol)) {
        $names = ($incompatibleDlls | ForEach-Object { $_.Name }) -join ", "
        throw "MinGW ABI mismatch: $names imports $symbol, but staged libstdc++-6.dll does not export it. Rebuild those dependencies with the same MinGW kit as Qt: $ResolvedQtPrefix"
    }
}

function Assert-StagedPeImportClosure {
    param(
        [string]$Directory,
        [string]$ResolvedQtPrefix
    )

    if ($ResolvedQtPrefix -notmatch "mingw") {
        return
    }

    $objdump = Resolve-Objdump -ResolvedQtPrefix $ResolvedQtPrefix
    if (-not $objdump) {
        throw "objdump.exe was not found. MinGW Windows packages require PE import-closure checks."
    }

    $availableDllNames = @{}
    Get-ChildItem -LiteralPath $Directory -Filter "*.dll" -File -Recurse -ErrorAction SilentlyContinue |
        ForEach-Object { $availableDllNames[$_.Name.ToUpperInvariant()] = $true }

    $systemDirectories = @(
        [Environment]::GetFolderPath([Environment+SpecialFolder]::System),
        $env:SystemRoot
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -Unique
    foreach ($systemDirectory in $systemDirectories) {
        Get-ChildItem -LiteralPath $systemDirectory -Filter "*.dll" -File -ErrorAction SilentlyContinue |
            ForEach-Object { $availableDllNames[$_.Name.ToUpperInvariant()] = $true }
    }

    $stageRoot = [System.IO.Path]::GetFullPath($Directory).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar
    ) + [System.IO.Path]::DirectorySeparatorChar
    $unresolvedImports = @(
        Get-ChildItem -LiteralPath $Directory -File -Recurse -ErrorAction SilentlyContinue |
            Where-Object { $_.Extension -in @(".exe", ".dll") } |
            ForEach-Object {
                $relativeBinary = $_.FullName.Substring($stageRoot.Length)
                foreach ($importName in Get-PeImportedDllNames -Objdump $objdump -Binary $_.FullName) {
                    $normalizedImport = $importName.ToUpperInvariant()
                    if (($normalizedImport -notlike "API-MS-WIN-*") -and
                        ($normalizedImport -notlike "EXT-MS-*") -and
                        (-not $availableDllNames.ContainsKey($normalizedImport))) {
                        "$relativeBinary -> $importName"
                    }
                }
            }
    )

    if ($unresolvedImports.Count -gt 0) {
        throw "Staged PE import closure is incomplete: $($unresolvedImports -join '; ')"
    }
}

function Resolve-WixTools {
    param([string]$ConfiguredToolsDir)

    $candidates = @()
    if ($ConfiguredToolsDir) {
        $candidates += $ConfiguredToolsDir
    }
    if ($env:WIX) {
        $candidates += $env:WIX
        $candidates += Join-Path $env:WIX "bin"
    }
    $candidates += Join-Path $RepositoryRoot "build\tools\wix314"

    $heatCommand = Get-Command "heat.exe" -ErrorAction SilentlyContinue
    if ($heatCommand) {
        $candidates += Split-Path -Parent $heatCommand.Source
    }

    foreach ($candidate in $candidates) {
        if (-not $candidate) {
            continue
        }
        if ((Test-Path (Join-Path $candidate "heat.exe")) -and
            (Test-Path (Join-Path $candidate "candle.exe")) -and
            (Test-Path (Join-Path $candidate "light.exe")) -and
            (Test-Path (Join-Path $candidate "smoke.exe")) -and
            (Test-Path (Join-Path $candidate "darice.cub"))) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "WiX Toolset was not found. Set WIX_TOOLS_DIR to a directory containing heat.exe, candle.exe, light.exe, smoke.exe, and darice.cub."
}

function Write-MsiLicenseRtf {
    param(
        [string]$SourcePath,
        [string]$OutputPath
    )

    if (-not (Test-Path -LiteralPath $SourcePath -PathType Leaf)) {
        throw "MSI license source was not found: $SourcePath"
    }

    $licenseText = [System.IO.File]::ReadAllText(
        (Resolve-Path -LiteralPath $SourcePath).Path,
        [System.Text.Encoding]::UTF8
    )
    if ([string]::IsNullOrWhiteSpace($licenseText)) {
        throw "MSI license source is empty: $SourcePath"
    }

    $body = New-Object System.Text.StringBuilder
    foreach ($character in $licenseText.ToCharArray()) {
        switch ($character) {
            "\" {
                [void]$body.Append("\\")
                break
            }
            "{" {
                [void]$body.Append("\{")
                break
            }
            "}" {
                [void]$body.Append("\}")
                break
            }
            "`r" {
                break
            }
            "`n" {
                [void]$body.Append("\par`r`n")
                break
            }
            "`t" {
                [void]$body.Append("\tab ")
                break
            }
            default {
                $codeUnit = [int][char]$character
                if ($codeUnit -le 127) {
                    [void]$body.Append($character)
                } else {
                    $signedCodeUnit = if ($codeUnit -gt 32767) { $codeUnit - 65536 } else { $codeUnit }
                    [void]$body.Append("\u${signedCodeUnit}?")
                }
            }
        }
    }

    $rtf = "{\rtf1\ansi\ansicpg1252\deff0{\fonttbl{\f0\fnil Segoe UI;}}\viewkind4\uc1\pard\f0\fs18`r`n$body`r`n}"
    [System.IO.File]::WriteAllText($OutputPath, $rtf, [System.Text.Encoding]::ASCII)
}

function New-DeterministicProductCode {
    param(
        [Parameter(Mandatory = $true)]
        [ValidatePattern('^\d+\.\d+\.\d+$')]
        [string]$ProductVersion,

        [Parameter(Mandatory = $true)]
        [ValidatePattern('^[A-Za-z0-9_-]+$')]
        [string]$Architecture
    )

    # A stable UUIDv5 keeps a rebuilt package for the same release in the same
    # Windows Installer product identity. Version and architecture remain part
    # of the name so a real release or architecture change gets a new product.
    $namespaceGuid = [Guid]"31432BB7-5B11-55D5-8AE0-CD97E6B1D95F"
    $namespaceBytes = $namespaceGuid.ToByteArray()
    [Array]::Reverse($namespaceBytes, 0, 4)
    [Array]::Reverse($namespaceBytes, 4, 2)
    [Array]::Reverse($namespaceBytes, 6, 2)

    $name = "Congregation|Windows|$($Architecture.ToLowerInvariant())|$ProductVersion"
    $nameBytes = [System.Text.Encoding]::UTF8.GetBytes($name)
    $inputBytes = New-Object byte[] ($namespaceBytes.Length + $nameBytes.Length)
    [Buffer]::BlockCopy($namespaceBytes, 0, $inputBytes, 0, $namespaceBytes.Length)
    [Buffer]::BlockCopy($nameBytes, 0, $inputBytes, $namespaceBytes.Length, $nameBytes.Length)

    $sha1 = [System.Security.Cryptography.SHA1]::Create()
    try {
        $hash = $sha1.ComputeHash($inputBytes)
    } finally {
        $sha1.Dispose()
    }

    $guidBytes = New-Object byte[] 16
    [Buffer]::BlockCopy($hash, 0, $guidBytes, 0, $guidBytes.Length)
    $guidBytes[6] = ($guidBytes[6] -band 0x0F) -bor 0x50
    $guidBytes[8] = ($guidBytes[8] -band 0x3F) -bor 0x80
    [Array]::Reverse($guidBytes, 0, 4)
    [Array]::Reverse($guidBytes, 4, 2)
    [Array]::Reverse($guidBytes, 6, 2)

    return (New-Object Guid (,$guidBytes)).ToString("B").ToUpperInvariant()
}

function Write-MsiProductFile {
    param(
        [string]$OutputPath,
        [string]$ProductVersion,
        [string]$Architecture = "x64"
    )

    $productCode = New-DeterministicProductCode `
        -ProductVersion $ProductVersion `
        -Architecture $Architecture
    $productSource = @'
<?xml version="1.0" encoding="UTF-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">
  <Product Id="%%PRODUCT_CODE%%"
           Codepage="1252"
           Name="Congregation"
           Language="1033"
           Version="%%PRODUCT_VERSION%%"
           Manufacturer="IISACC"
           UpgradeCode="31432BB7-5B11-55D5-8AE0-CD97E6B1D95F">
    <Package InstallerVersion="500"
             Compressed="yes"
             SummaryCodepage="1252"
             Description="Congregation %%PRODUCT_VERSION%% Windows Installer" />
    <MajorUpgrade Schedule="afterInstallInitialize"
                  DowngradeErrorMessage="A newer version of Congregation is already installed." />
    <MediaTemplate EmbedCab="yes" />
    <Icon Id="CongregationIcon" SourceFile="$(var.SourceDir)\resources\Appicon.ico" />
    <Property Id="ARPPRODUCTICON" Value="CongregationIcon" />
    <Property Id="ALLUSERS" Value="2" />
    <Property Id="MSIINSTALLPERUSER" Value="1" />
    <Property Id="DISABLEADVTSHORTCUTS" Value="1" />
    <Property Id="CONGREGATION_MACHINE_PROGRAMFILES64" Secure="yes">
      <RegistrySearch Id="CongregationMachineProgramFiles64Search"
                      Root="HKLM"
                      Key="SOFTWARE\Microsoft\Windows\CurrentVersion"
                      Name="ProgramFilesDir"
                      Type="raw"
                      Win64="yes" />
    </Property>
    <Property Id="CONGREGATION_EXISTING_USER_CONTEXT" Secure="yes">
      <RegistrySearch Id="CongregationExistingUserContextSearch"
                      Root="HKCU"
                      Key="Software\IISACC\Congregation"
                      Name="installContext"
                      Type="raw"
                      Win64="yes" />
    </Property>
    <Property Id="CONGREGATION_LEGACY_USER_CONTEXT" Secure="yes">
      <RegistrySearch Id="CongregationLegacyUserContextSearch"
                      Root="HKCU"
                      Key="Software\IISACC\Congregation"
                      Name="installed"
                      Type="raw"
                      Win64="yes" />
    </Property>
    <Property Id="CONGREGATION_EXISTING_USER_INSTALLLOCATION" Secure="yes">
      <RegistrySearch Id="CongregationExistingUserInstallLocationSearch"
                      Root="HKCU"
                      Key="Software\IISACC\Congregation"
                      Name="InstallLocation"
                      Type="raw"
                      Win64="yes" />
    </Property>
    <Property Id="CONGREGATION_EXISTING_MACHINE_CONTEXT" Secure="yes">
      <RegistrySearch Id="CongregationExistingMachineContextSearch"
                      Root="HKLM"
                      Key="Software\IISACC\Congregation"
                      Name="installContext"
                      Type="raw"
                      Win64="yes" />
    </Property>
    <Property Id="CONGREGATION_LEGACY_MACHINE_CONTEXT" Secure="yes">
      <RegistrySearch Id="CongregationLegacyMachineContextSearch"
                      Root="HKLM"
                      Key="Software\IISACC\Congregation"
                      Name="machineStartMenuShortcut"
                      Type="raw"
                      Win64="yes" />
    </Property>
    <Property Id="CONGREGATION_EXISTING_MACHINE_INSTALLLOCATION" Secure="yes">
      <RegistrySearch Id="CongregationExistingMachineInstallLocationSearch"
                      Root="HKLM"
                      Key="Software\IISACC\Congregation"
                      Name="InstallLocation"
                      Type="raw"
                      Win64="yes" />
    </Property>
    <Property Id="ApplicationFolderName" Value="Congregation" />
    <Property Id="WixAppFolder" Value="WixPerUserFolder" />
    <Condition Message="Congregation is registered for both the current user and all users. Remove one installation before continuing.">
      Installed OR REMOVE~="ALL" OR NOT ((CONGREGATION_EXISTING_USER_CONTEXT OR CONGREGATION_LEGACY_USER_CONTEXT) AND (CONGREGATION_EXISTING_MACHINE_CONTEXT OR CONGREGATION_LEGACY_MACHINE_CONTEXT))
    </Condition>
    <Condition Message="This Congregation installation is registered for the current user. Run the installer in the current-user context.">
      Installed OR REMOVE~="ALL" OR NOT (CONGREGATION_EXISTING_USER_CONTEXT OR CONGREGATION_LEGACY_USER_CONTEXT OR (WIX_UPGRADE_DETECTED AND NOT (CONGREGATION_EXISTING_MACHINE_CONTEXT OR CONGREGATION_LEGACY_MACHINE_CONTEXT))) OR NOT ALLUSERS
    </Condition>
    <Condition Message="The native 64-bit Program Files path could not be resolved.">
      CONGREGATION_MACHINE_PROGRAMFILES64
    </Condition>
    <SetProperty Id="ARPINSTALLLOCATION"
                 Value="[APPLICATIONFOLDER]"
                 After="CostFinalize"
                 Sequence="execute" />
    <SetProperty Id="WixPerMachineFolder"
                 Value="[CONGREGATION_MACHINE_PROGRAMFILES64]\[ApplicationFolderName]"
                 After="WixSetDefaultPerMachineFolder"
                 Sequence="both" />
    <CustomAction Id="CongregationSetExistingUserUiScope"
                  Property="WixAppFolder"
                  Value="WixPerUserFolder"
                  Execute="firstSequence" />
    <CustomAction Id="CongregationSetExistingMachineUiScope"
                  Property="WixAppFolder"
                  Value="WixPerMachineFolder"
                  Execute="firstSequence" />
    <CustomAction Id="CongregationSetDetectedUserUiScope"
                  Property="WixAppFolder"
                  Value="WixPerUserFolder"
                  Execute="firstSequence" />
    <CustomAction Id="CongregationSetExistingMachineContext"
                  Property="ALLUSERS"
                  Value="1"
                  Execute="firstSequence" />
    <CustomAction Id="CongregationSetExistingUserInstallFolder"
                  Property="APPLICATIONFOLDER"
                  Value="[CONGREGATION_EXISTING_USER_INSTALLLOCATION]"
                  Execute="firstSequence" />
    <CustomAction Id="CongregationSetExistingMachineInstallFolder"
                  Property="APPLICATIONFOLDER"
                  Value="[CONGREGATION_EXISTING_MACHINE_INSTALLLOCATION]"
                  Execute="firstSequence" />
    <InstallUISequence>
      <AppSearch Sequence="10" />
      <Custom Action="CongregationSetExistingUserUiScope" Sequence="11">(CONGREGATION_EXISTING_USER_CONTEXT OR CONGREGATION_LEGACY_USER_CONTEXT) AND NOT (CONGREGATION_EXISTING_MACHINE_CONTEXT OR CONGREGATION_LEGACY_MACHINE_CONTEXT)</Custom>
      <Custom Action="CongregationSetExistingMachineUiScope" Sequence="12">(CONGREGATION_EXISTING_MACHINE_CONTEXT OR CONGREGATION_LEGACY_MACHINE_CONTEXT) AND NOT (CONGREGATION_EXISTING_USER_CONTEXT OR CONGREGATION_LEGACY_USER_CONTEXT)</Custom>
      <Custom Action="CongregationSetExistingMachineContext" Sequence="13">(CONGREGATION_EXISTING_MACHINE_CONTEXT OR CONGREGATION_LEGACY_MACHINE_CONTEXT) AND NOT (CONGREGATION_EXISTING_USER_CONTEXT OR CONGREGATION_LEGACY_USER_CONTEXT)</Custom>
      <Custom Action="CongregationSetExistingUserInstallFolder" Sequence="14">CONGREGATION_EXISTING_USER_INSTALLLOCATION AND (CONGREGATION_EXISTING_USER_CONTEXT OR CONGREGATION_LEGACY_USER_CONTEXT) AND NOT (CONGREGATION_EXISTING_MACHINE_CONTEXT OR CONGREGATION_LEGACY_MACHINE_CONTEXT)</Custom>
      <Custom Action="CongregationSetExistingMachineInstallFolder" Sequence="15">CONGREGATION_EXISTING_MACHINE_INSTALLLOCATION AND (CONGREGATION_EXISTING_MACHINE_CONTEXT OR CONGREGATION_LEGACY_MACHINE_CONTEXT) AND NOT (CONGREGATION_EXISTING_USER_CONTEXT OR CONGREGATION_LEGACY_USER_CONTEXT)</Custom>
      <Custom Action="CongregationSetDetectedUserUiScope" Sequence="26">WIX_UPGRADE_DETECTED AND NOT (CONGREGATION_EXISTING_MACHINE_CONTEXT OR CONGREGATION_LEGACY_MACHINE_CONTEXT)</Custom>
    </InstallUISequence>
    <InstallExecuteSequence>
      <AppSearch Sequence="10" />
      <Custom Action="CongregationSetExistingMachineContext" Sequence="11">(CONGREGATION_EXISTING_MACHINE_CONTEXT OR CONGREGATION_LEGACY_MACHINE_CONTEXT) AND NOT (CONGREGATION_EXISTING_USER_CONTEXT OR CONGREGATION_LEGACY_USER_CONTEXT)</Custom>
      <Custom Action="CongregationSetExistingUserInstallFolder" Sequence="12">CONGREGATION_EXISTING_USER_INSTALLLOCATION AND (CONGREGATION_EXISTING_USER_CONTEXT OR CONGREGATION_LEGACY_USER_CONTEXT) AND NOT (CONGREGATION_EXISTING_MACHINE_CONTEXT OR CONGREGATION_LEGACY_MACHINE_CONTEXT)</Custom>
      <Custom Action="CongregationSetExistingMachineInstallFolder" Sequence="13">CONGREGATION_EXISTING_MACHINE_INSTALLLOCATION AND (CONGREGATION_EXISTING_MACHINE_CONTEXT OR CONGREGATION_LEGACY_MACHINE_CONTEXT) AND NOT (CONGREGATION_EXISTING_USER_CONTEXT OR CONGREGATION_LEGACY_USER_CONTEXT)</Custom>
    </InstallExecuteSequence>
    <WixVariable Id="WixUILicenseRtf" Value="$(var.LicenseRtf)" />
    <UIRef Id="WixUI_Advanced" />
    <UIRef Id="WixUI_ErrorProgressText" />
    <UI>
      <Publish Dialog="InstallScopeDlg" Control="Next" Property="WixAppFolder" Value="WixPerUserFolder" Order="0">CONGREGATION_EXISTING_USER_CONTEXT OR CONGREGATION_LEGACY_USER_CONTEXT OR (WIX_UPGRADE_DETECTED AND NOT (CONGREGATION_EXISTING_MACHINE_CONTEXT OR CONGREGATION_LEGACY_MACHINE_CONTEXT))</Publish>
      <Publish Dialog="InstallScopeDlg" Control="Next" Property="WixAppFolder" Value="WixPerMachineFolder" Order="0">CONGREGATION_EXISTING_MACHINE_CONTEXT OR CONGREGATION_LEGACY_MACHINE_CONTEXT</Publish>
      <Publish Dialog="InstallScopeDlg" Control="Next" Property="APPLICATIONFOLDER" Value="[CONGREGATION_EXISTING_USER_INSTALLLOCATION]" Order="5">CONGREGATION_EXISTING_USER_INSTALLLOCATION AND (CONGREGATION_EXISTING_USER_CONTEXT OR CONGREGATION_LEGACY_USER_CONTEXT)</Publish>
      <Publish Dialog="InstallScopeDlg" Control="Next" Property="APPLICATIONFOLDER" Value="[CONGREGATION_EXISTING_MACHINE_INSTALLLOCATION]" Order="6">CONGREGATION_EXISTING_MACHINE_INSTALLLOCATION AND (CONGREGATION_EXISTING_MACHINE_CONTEXT OR CONGREGATION_LEGACY_MACHINE_CONTEXT)</Publish>
      <Publish Dialog="FeaturesDlg" Control="Install" Property="MSIINSTALLPERUSER" Value="1" Order="1">NOT Installed AND WixAppFolder = "WixPerUserFolder"</Publish>
      <Publish Dialog="FeaturesDlg" Control="Install" Property="MSIINSTALLPERUSER" Value="{}" Order="1">NOT Installed AND WixAppFolder = "WixPerMachineFolder"</Publish>
      <Publish Dialog="FeaturesDlg" Control="Install" Property="ALLUSERS" Value="2" Order="1">NOT Installed</Publish>
      <Publish Dialog="FeaturesDlg" Control="Install" Property="APPLICATIONFOLDER" Value="[CONGREGATION_EXISTING_USER_INSTALLLOCATION]" Order="1">NOT Installed AND CONGREGATION_EXISTING_USER_INSTALLLOCATION AND (CONGREGATION_EXISTING_USER_CONTEXT OR CONGREGATION_LEGACY_USER_CONTEXT)</Publish>
      <Publish Dialog="FeaturesDlg" Control="Install" Property="APPLICATIONFOLDER" Value="[CONGREGATION_EXISTING_MACHINE_INSTALLLOCATION]" Order="1">NOT Installed AND CONGREGATION_EXISTING_MACHINE_INSTALLLOCATION AND (CONGREGATION_EXISTING_MACHINE_CONTEXT OR CONGREGATION_LEGACY_MACHINE_CONTEXT)</Publish>
      <Publish Dialog="FeaturesDlg" Control="InstallNoShield" Property="MSIINSTALLPERUSER" Value="1" Order="1">NOT Installed AND WixAppFolder = "WixPerUserFolder"</Publish>
      <Publish Dialog="FeaturesDlg" Control="InstallNoShield" Property="MSIINSTALLPERUSER" Value="{}" Order="1">NOT Installed AND WixAppFolder = "WixPerMachineFolder"</Publish>
      <Publish Dialog="FeaturesDlg" Control="InstallNoShield" Property="ALLUSERS" Value="2" Order="1">NOT Installed</Publish>
      <Publish Dialog="FeaturesDlg" Control="InstallNoShield" Property="APPLICATIONFOLDER" Value="[CONGREGATION_EXISTING_USER_INSTALLLOCATION]" Order="1">NOT Installed AND CONGREGATION_EXISTING_USER_INSTALLLOCATION AND (CONGREGATION_EXISTING_USER_CONTEXT OR CONGREGATION_LEGACY_USER_CONTEXT)</Publish>
      <Publish Dialog="FeaturesDlg" Control="InstallNoShield" Property="APPLICATIONFOLDER" Value="[CONGREGATION_EXISTING_MACHINE_INSTALLLOCATION]" Order="1">NOT Installed AND CONGREGATION_EXISTING_MACHINE_INSTALLLOCATION AND (CONGREGATION_EXISTING_MACHINE_CONTEXT OR CONGREGATION_LEGACY_MACHINE_CONTEXT)</Publish>
    </UI>

    <Feature Id="CoreFeature"
             Title="Congregation application files"
             Description="Installs the files required to run Congregation."
             Level="1"
             Absent="disallow"
             ConfigurableDirectory="APPLICATIONFOLDER"
             Display="expand">
      <ComponentGroupRef Id="CongregationRuntime" />
      <ComponentRef Id="InstallContextComponent" />
    </Feature>
  </Product>

  <Fragment>
    <Directory Id="TARGETDIR" Name="SourceDir">
      <Directory Id="ProgramFiles64Folder">
        <Directory Id="APPLICATIONFOLDER" Name="Congregation" />
      </Directory>
      <Directory Id="ProgramMenuFolder">
        <Directory Id="ApplicationProgramsFolder" Name="Congregation" />
      </Directory>
    </Directory>
  </Fragment>

  <Fragment>
    <DirectoryRef Id="APPLICATIONFOLDER">
      <Component Id="InstallContextComponent" Guid="05DF7FB6-91BE-5013-AE84-E4A6496550E8" Win64="yes">
        <RegistryValue Root="HKMU"
                       Key="Software\IISACC\Congregation"
                       Name="installContext"
                       Type="integer"
                       Value="1"
                       KeyPath="yes" />
        <RegistryValue Root="HKMU"
                       Key="Software\IISACC\Congregation"
                       Name="InstallLocation"
                       Type="string"
                       Value="[APPLICATIONFOLDER]" />
      </Component>
    </DirectoryRef>
  </Fragment>
</Wix>
'@

    $productSource = $productSource.Replace("%%PRODUCT_VERSION%%", $ProductVersion)
    $productSource = $productSource.Replace("%%PRODUCT_CODE%%", $productCode)
    Set-Content -Path $OutputPath -Value $productSource -Encoding UTF8
}

function Remove-NonAsciiHarvestedFiles {
    param([string]$HarvestPath)

    $content = Get-Content -Path $HarvestPath -Raw
    $nonAsciiComponentIds = [regex]::Matches($content, '(?ms)<Component Id="([^"]+)"[^>]*>\s*<File [^>]*Source="[^"]*[^\x00-\x7F][^"]*"[^>]*/>\s*</Component>') |
        ForEach-Object { $_.Groups[1].Value } |
        Sort-Object -Unique

    foreach ($id in $nonAsciiComponentIds) {
        $componentPattern = '(?ms)^\s*<Component Id="' + [regex]::Escape($id) + '".*?</Component>\r?\n'
        $refPattern = '(?m)^\s*<ComponentRef Id="' + [regex]::Escape($id) + '" />\r?\n'
        $content = [regex]::Replace($content, $componentPattern, "")
        $content = [regex]::Replace($content, $refPattern, "")
    }

    Set-Content -Path $HarvestPath -Value $content -Encoding UTF8
}

function Add-AdvertisedStartMenuShortcut {
    param([string]$HarvestPath)

    $content = Get-Content -LiteralPath $HarvestPath -Raw
    $congregationFilePattern = '(?m)^(?<indent>\s*)<File (?<attributes>[^>\r\n]*\bSource="\$\(var\.StageDir\)\\Congregation\.exe"[^>\r\n]*) />\r?$'
    $congregationFiles = [regex]::Matches($content, $congregationFilePattern)
    if ($congregationFiles.Count -ne 1) {
        throw "Expected exactly one harvested Congregation.exe file row, found $($congregationFiles.Count)."
    }

    $content = [regex]::Replace(
        $content,
        $congregationFilePattern,
        {
            param($match)
            $indent = $match.Groups["indent"].Value
            $attributes = $match.Groups["attributes"].Value
            return @(
                "${indent}<File $attributes>",
                "${indent}    <Shortcut Id=`"ApplicationStartMenuShortcut`"",
                "${indent}              Directory=`"ApplicationProgramsFolder`"",
                "${indent}              Name=`"Congregation`"",
                "${indent}              Description=`"Launch Congregation`"",
                "${indent}              WorkingDirectory=`"APPLICATIONFOLDER`"",
                "${indent}              Advertise=`"yes`" />",
                "${indent}</File>",
                "${indent}<RemoveFolder Id=`"ApplicationProgramsFolder`"",
                "${indent}              Directory=`"ApplicationProgramsFolder`"",
                "${indent}              On=`"uninstall`" />"
            ) -join "`r`n"
        },
        1
    )

    Set-Content -LiteralPath $HarvestPath -Value $content -Encoding UTF8
}

function New-MsiInstaller {
    param(
        [string]$SourceDirectory,
        [string]$OutputPath,
        [string]$WorkDirectory,
        [string]$ToolsDirectory,
        [string]$ProductVersion,
        [string]$Architecture = "x64"
    )

    $wixTools = Resolve-WixTools -ConfiguredToolsDir $ToolsDirectory
    $heat = Join-Path $wixTools "heat.exe"
    $candle = Join-Path $wixTools "candle.exe"
    $light = Join-Path $wixTools "light.exe"
    $smoke = Join-Path $wixTools "smoke.exe"
    $darice = Join-Path $wixTools "darice.cub"
    $productPath = Join-Path $WorkDirectory "CongregationProduct.wxs"
    $runtimePath = Join-Path $WorkDirectory "CongregationRuntime.wxs"
    $licensePath = Join-Path $WorkDirectory "CongregationLicense.rtf"

    New-Item -ItemType Directory -Force $WorkDirectory | Out-Null
    Write-MsiLicenseRtf -SourcePath (Join-Path $RepositoryRoot "LICENSE") -OutputPath $licensePath
    Write-MsiProductFile -OutputPath $productPath -ProductVersion $ProductVersion -Architecture $Architecture
    Invoke-Native $heat @("dir", $SourceDirectory, "-wx", "-cg", "CongregationRuntime", "-dr", "APPLICATIONFOLDER", "-srd", "-sreg", "-sfrag", "-ag", "-var", "var.StageDir", "-out", $runtimePath)
    Remove-NonAsciiHarvestedFiles -HarvestPath $runtimePath
    Add-AdvertisedStartMenuShortcut -HarvestPath $runtimePath

    Remove-Item -Path (Join-Path $WorkDirectory "*.wixobj"), $OutputPath -Force -ErrorAction SilentlyContinue
    Invoke-Native $candle @("-wx", "-dStageDir=$SourceDirectory", "-dSourceDir=$RepositoryRoot", "-dLicenseRtf=$licensePath", "-arch", $Architecture, "-out", "$WorkDirectory\", $productPath, $runtimePath)
    Invoke-Native $light @("-wx", "-ext", "WixUIExtension", "-cultures:en-us", "-out", $OutputPath, (Join-Path $WorkDirectory "CongregationProduct.wixobj"), (Join-Path $WorkDirectory "CongregationRuntime.wixobj"))
    Invoke-Native $smoke @("-wx", "-nodefault", "-cub", $darice, "-ice:ICE105", $OutputPath)
}

function Assert-MsiDatabaseContract {
    param(
        [string]$MsiFile,
        [string]$ProductVersion
    )

    $contractScript = Join-Path $RepositoryRoot "tests\tst_windowsmsidatabasecontract.ps1"
    if (-not (Test-Path -LiteralPath $contractScript -PathType Leaf)) {
        throw "MSI database contract test was not found: $contractScript"
    }

    $powerShell = Get-RequiredCommand "powershell.exe"
    Invoke-Native $powerShell @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $contractScript,
        "-BuildDirectory", $BuildDir,
        "-Version", $ProductVersion,
        "-MsiPath", $MsiFile
    )
}

function Import-VisualStudioEnvironment {
    param([string]$ResolvedQtPrefix)

    if ($ResolvedQtPrefix -match "mingw") {
        return
    }
    if (Get-Command "cl.exe" -ErrorAction SilentlyContinue) {
        return
    }

    $programFilesX86 = ${env:ProgramFiles(x86)}
    if (-not $programFilesX86) {
        $programFilesX86 = $env:ProgramFiles
    }

    $vsWhere = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vsWhere)) {
        Write-Warning "cl.exe is not in PATH and vswhere.exe was not found. Run this script from a Developer PowerShell for VS if CMake cannot find MSVC."
        return
    }

    $installationPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $installationPath) {
        Write-Warning "Visual Studio C++ tools were not found. Run this script after installing the Desktop development with C++ workload."
        return
    }

    $vsDevCmd = Join-Path $installationPath "Common7\Tools\VsDevCmd.bat"
    if (-not (Test-Path $vsDevCmd)) {
        Write-Warning "VsDevCmd.bat was not found under $installationPath."
        return
    }

    Write-Step "Loading Visual Studio build environment"
    $environmentDump = cmd.exe /s /c "`"$vsDevCmd`" -arch=x64 -host_arch=x64 -no_logo && set"
    foreach ($line in $environmentDump) {
        if ($line -match "^(.*?)=(.*)$") {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }
}

function Copy-DependencyRuntimeFiles {
    param(
        [string]$Name,
        [string]$Prefix,
        [string]$Destination
    )

    $candidateDirs = @(
        (Join-Path $Prefix "bin"),
        (Join-Path $Prefix "lib"),
        (Join-Path $Prefix "platforms\windows\bin")
    )

    $copied = 0
    foreach ($dir in $candidateDirs) {
        if (-not (Test-Path $dir)) {
            continue
        }

        Get-ChildItem -Path $dir -Filter "*.dll" -File -ErrorAction SilentlyContinue | ForEach-Object {
            Copy-Item $_.FullName -Destination $Destination -Force
            $copied += 1
        }
    }

    if ($copied -eq 0) {
        Write-Warning "No runtime DLLs were copied for $Name from $Prefix. This is acceptable only when the dependency is static."
    }
}

function Remove-EmbeddedDependencyQmlImport {
    param(
        [string]$ModuleName,
        [string]$Destination
    )

    if ($ModuleName -notmatch '^[A-Za-z0-9_.-]+$') {
        throw "Invalid QML module name: $ModuleName"
    }

    $destinationRoot = [System.IO.Path]::GetFullPath($Destination)
    $qmlRoot = [System.IO.Path]::GetFullPath((Join-Path $destinationRoot "qml"))
    $moduleRoot = [System.IO.Path]::GetFullPath((Join-Path $qmlRoot $ModuleName))
    $qmlPrefix = $qmlRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
    if (-not $moduleRoot.StartsWith($qmlPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove a QML module outside the staged qml directory: $moduleRoot"
    }

    if (-not (Test-Path $moduleRoot)) {
        return
    }

    Remove-Item -LiteralPath $moduleRoot -Recurse -Force
}

function Get-ReleaseOnlyQtArtifactRelativePaths {
    return @(
        "qmltooling",
        "generic",
        "sqldrivers",
        "Qt6InsightTracker.dll",
        "Qt6Sql.dll",
        "Qt6Quick3DUtils.dll",
        "imageformats\qpdf.dll",
        "Qt6Pdf.dll",
        "qml\QtQuick\Pdf",
        "platforminputcontexts\qtvirtualkeyboardplugin.dll",
        "Qt6VirtualKeyboard.dll",
        "qml\QtQuick\VirtualKeyboard"
    )
}

function Remove-ReleaseOnlyQtArtifacts {
    param(
        [string]$Directory,
        [string]$BuildType
    )

    if ($BuildType -eq "Debug") {
        return
    }

    $stageRoot = [System.IO.Path]::GetFullPath($Directory).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar
    ) + [System.IO.Path]::DirectorySeparatorChar
    foreach ($relativePath in Get-ReleaseOnlyQtArtifactRelativePaths) {
        $artifactPath = [System.IO.Path]::GetFullPath((Join-Path $Directory $relativePath))
        if (-not $artifactPath.StartsWith($stageRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove a Qt artifact outside the staged directory: $artifactPath"
        }
        if (Test-Path $artifactPath) {
            Remove-Item -LiteralPath $artifactPath -Recurse -Force
        }
    }
}

function Strip-WindowsRuntimeBinaries {
    param(
        [string]$Directory,
        [string]$ResolvedQtPrefix,
        [string]$Configuration
    )

    if (($ResolvedQtPrefix -notmatch "mingw") -or ($Configuration -notin @("Release", "MinSizeRel"))) {
        return
    }

    $strip = Resolve-Strip -ResolvedQtPrefix $ResolvedQtPrefix
    if (-not $strip) {
        throw "strip.exe was not found. MinGW release packages require symbol stripping."
    }

    foreach ($runtimeName in @(
        "Congregation.exe",
        "LVRS.dll",
        "libiiPaintEngine.dll",
        "iiSharedCanvas.dll",
        "libiiSharedCanvas.dll",
        "iiUpdateManager.dll",
        "libiiUpdateManager.dll",
        "iiLicenseManager.dll",
        "libiiLicenseManager.dll"
    )) {
        $runtimePath = Join-Path $Directory $runtimeName
        if (Test-Path $runtimePath) {
            Invoke-Native $strip @("--strip-all", $runtimePath)
        }
    }
}

function Resolve-CongregationExecutable {
    param(
        [string]$BuildDirectory,
        [string]$Configuration
    )

    $candidates = @(
        (Join-Path $BuildDirectory "Congregation.exe"),
        (Join-Path $BuildDirectory "$Configuration\Congregation.exe")
    )

    $executable = Test-AnyPath $candidates
    if (-not $executable) {
        throw "Congregation.exe was not produced in build/. Checked: $($candidates -join ', ')"
    }

    return $executable
}

function Verify-WindowsStage {
    param(
        [string]$Directory,
        [string]$ResolvedQtPrefix,
        [string]$ExpectedFileVersion,
        [string]$BuildType
    )

    $congregationExe = Join-Path $Directory "Congregation.exe"
    if (-not (Test-Path $congregationExe)) {
        throw "Staged Congregation.exe is missing."
    }

    $sharedCanvasRuntime = @(
        @("iiSharedCanvas.dll", "libiiSharedCanvas.dll") |
            ForEach-Object { Join-Path $Directory $_ } |
            Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
    )
    if ($sharedCanvasRuntime.Count -ne 1) {
        throw "iiSharedCanvas runtime deployment must contain exactly one platform DLL."
    }

    $updateManagerRuntime = @(
        @("iiUpdateManager.dll", "libiiUpdateManager.dll") |
            ForEach-Object { Join-Path $Directory $_ } |
            Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
    )
    if ($updateManagerRuntime.Count -ne 1) {
        throw "iiUpdateManager runtime deployment must contain exactly one platform DLL."
    }

    $licenseManagerRuntime = @(
        @("iiLicenseManager.dll", "libiiLicenseManager.dll") |
            ForEach-Object { Join-Path $Directory $_ } |
            Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
    )
    if ($licenseManagerRuntime.Count -ne 1) {
        throw "iiLicenseManager runtime deployment must contain exactly one platform DLL."
    }

    $peHeader = Get-PeHeader -Binary $congregationExe
    if ($peHeader.Machine -ne 0x8664) {
        throw "Staged Congregation.exe is not an AMD64 native binary (machine=0x$('{0:x4}' -f $peHeader.Machine))."
    }
    if ($peHeader.OptionalHeaderMagic -ne 0x020b) {
        throw "Staged Congregation.exe is not PE32+ (magic=0x$('{0:x4}' -f $peHeader.OptionalHeaderMagic))."
    }
    if ((Get-PeSubsystem -Binary $congregationExe) -ne 2) {
        throw "Staged Congregation.exe must use the Windows GUI subsystem."
    }
    $requiredDllCharacteristics = 0x0160
    if (($peHeader.DllCharacteristics -band $requiredDllCharacteristics) -ne $requiredDllCharacteristics) {
        throw "Staged Congregation.exe is missing required ASLR/DEP PE security flags."
    }
    if (($ResolvedQtPrefix -match "mingw") -and
        ($BuildType -in @("Release", "MinSizeRel")) -and
        ($peHeader.PointerToSymbolTable -ne 0)) {
        throw "Staged MinGW Congregation.exe still contains a COFF symbol table."
    }

    $versionInfo = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($congregationExe)
    if ($versionInfo.FileVersion -ne $ExpectedFileVersion) {
        throw "Staged Congregation.exe file version '$($versionInfo.FileVersion)' does not match '$ExpectedFileVersion'."
    }
    if ($versionInfo.ProductVersion -ne $ExpectedFileVersion) {
        throw "Staged Congregation.exe product version '$($versionInfo.ProductVersion)' does not match '$ExpectedFileVersion'."
    }
    if ($versionInfo.ProductName -ne "Congregation") {
        throw "Staged Congregation.exe product name is not Congregation."
    }

    if (-not (Get-ChildItem -Path $Directory -Filter "Qt6Core*.dll" -File -ErrorAction SilentlyContinue)) {
        throw "Qt runtime deployment is incomplete: Qt6Core*.dll is missing."
    }

    $windowsPlatformPlugin = Join-Path $Directory "platforms\qwindows.dll"
    if (-not (Test-Path $windowsPlatformPlugin)) {
        throw "Qt runtime deployment is incomplete: platforms\qwindows.dll is missing."
    }

    $quickShapesPlugin = Join-Path $Directory "qml\QtQuick\Shapes\qmlshapesplugin.dll"
    if (-not (Test-Path $quickShapesPlugin)) {
        throw "Qt runtime deployment is incomplete: the Qt Quick Shapes QML plugin is missing."
    }

    $debugQtRuntimes = @(
        Get-ChildItem -Path $Directory -Filter "Qt6*.dll" -File -ErrorAction SilentlyContinue |
            Where-Object { $_.VersionInfo.IsDebug }
    )
    if (($BuildType -ne "Debug") -and ($debugQtRuntimes.Count -gt 0)) {
        throw "Release staging contains debug Qt runtime DLLs: $($debugQtRuntimes.Name -join ', ')."
    }

    if ($BuildType -ne "Debug") {
        foreach ($developmentOnlyPath in Get-ReleaseOnlyQtArtifactRelativePaths) {
            if (Test-Path (Join-Path $Directory $developmentOnlyPath)) {
                throw "Release staging contains an excluded development or unused plugin: $developmentOnlyPath"
            }
        }
    }

    Assert-MinGwRuntimeCompatibility -Directory $Directory -ResolvedQtPrefix $ResolvedQtPrefix
    Assert-StagedPeImportClosure -Directory $Directory -ResolvedQtPrefix $ResolvedQtPrefix
}

function Resolve-SafeCurrentUserInstallDirectory {
    param(
        [string]$TargetDirectory,
        [string]$SourceDirectory
    )

    $localAppData = [Environment]::GetFolderPath([Environment+SpecialFolder]::LocalApplicationData)
    if (-not $localAppData) {
        throw "The current user's Local Application Data directory could not be resolved."
    }
    if (-not $TargetDirectory) {
        $TargetDirectory = Join-Path $localAppData "Programs\Congregation"
    }

    $localAppDataRoot = [System.IO.Path]::GetFullPath($localAppData).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar
    )
    $localAppDataPrefix = $localAppDataRoot + [System.IO.Path]::DirectorySeparatorChar
    $resolvedTarget = [System.IO.Path]::GetFullPath($TargetDirectory).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar
    )
    if (-not $resolvedTarget.StartsWith($localAppDataPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "The current-user install directory must be below $localAppDataRoot."
    }

    if ($SourceDirectory) {
        $resolvedSource = [System.IO.Path]::GetFullPath($SourceDirectory).TrimEnd(
            [System.IO.Path]::DirectorySeparatorChar
        )
        $targetPrefix = $resolvedTarget + [System.IO.Path]::DirectorySeparatorChar
        $sourcePrefix = $resolvedSource + [System.IO.Path]::DirectorySeparatorChar
        if ($resolvedTarget.Equals($resolvedSource, [System.StringComparison]::OrdinalIgnoreCase) -or
            $resolvedTarget.StartsWith($sourcePrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
            $resolvedSource.StartsWith($targetPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "The current-user install directory must not overlap the staged source directory."
        }
    }

    if (Test-Path -LiteralPath $resolvedTarget) {
        $targetItem = Get-Item -LiteralPath $resolvedTarget -Force
        if (-not $targetItem.PSIsContainer) {
            throw "The current-user install target is not a directory: $resolvedTarget"
        }
        if (($targetItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "The current-user install directory must not be a reparse point: $resolvedTarget"
        }

        $existingEntries = @(Get-ChildItem -LiteralPath $resolvedTarget -Force -ErrorAction Stop)
        if ($existingEntries.Count -gt 0) {
            $ownershipMarker = Join-Path $resolvedTarget ".congregation-install-root"
            $installedExecutable = Join-Path $resolvedTarget "Congregation.exe"
            $isExistingCongregationInstall = (Test-Path -LiteralPath $ownershipMarker)
            if ((-not $isExistingCongregationInstall) -and (Test-Path -LiteralPath $installedExecutable)) {
                $isExistingCongregationInstall = (Get-Item -LiteralPath $installedExecutable).VersionInfo.ProductName -eq "Congregation"
            }
            if (-not $isExistingCongregationInstall) {
                throw "Refusing to replace a non-empty directory that is not an existing Congregation installation: $resolvedTarget"
            }
        }
    }

    return $resolvedTarget
}

function Install-ForCurrentUser {
    param(
        [string]$SourceDirectory,
        [string]$TargetDirectory
    )

    $TargetDirectory = Resolve-SafeCurrentUserInstallDirectory `
        -TargetDirectory $TargetDirectory `
        -SourceDirectory $SourceDirectory

    Write-Step "Installing Congregation for the current user"
    if (Test-Path -LiteralPath $TargetDirectory) {
        Remove-Item -LiteralPath $TargetDirectory -Recurse -Force
    }
    New-Item -ItemType Directory -Force $TargetDirectory | Out-Null
    Copy-Item (Join-Path $SourceDirectory "*") -Destination $TargetDirectory -Recurse -Force
    Set-Content -LiteralPath (Join-Path $TargetDirectory ".congregation-install-root") `
        -Value "Congregation current-user installation" `
        -Encoding ASCII

    $programsMenu = [Environment]::GetFolderPath("Programs")
    $shortcutPath = Join-Path $programsMenu "Congregation.lnk"
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($shortcutPath)
    $shortcut.TargetPath = Join-Path $TargetDirectory "Congregation.exe"
    $shortcut.WorkingDirectory = $TargetDirectory
    $shortcut.IconLocation = Join-Path $TargetDirectory "Congregation.exe"
    $shortcut.Save()

    Write-Host "Installed to: $TargetDirectory"
    Write-Host "Start Menu shortcut: $shortcutPath"
}

$isWindowsRuntime = if (Get-Variable IsWindows -ErrorAction SilentlyContinue) {
    $IsWindows
} else {
    $env:OS -eq "Windows_NT"
}
if (-not $isWindowsRuntime) {
    throw "build-windows.ps1 must be run on Windows."
}

$WindowsBuildMutex = Enter-WindowsBuildMutex
try {
$packageRequested = (-not $SkipPackage) -or $CreateMsi
$signedFinalArtifacts = @(
    [pscustomobject]@{
        FinalArtifactPath = $SignedZipPath
        FinalChecksumPath = "$SignedZipPath.sha256"
    },
    [pscustomobject]@{
        FinalArtifactPath = $SignedMsiPath
        FinalChecksumPath = "$SignedMsiPath.sha256"
    }
)
$unsignedFinalArtifacts = @(
    [pscustomobject]@{
        FinalArtifactPath = $UnsignedZipPath
        FinalChecksumPath = "$UnsignedZipPath.sha256"
    },
    [pscustomobject]@{
        FinalArtifactPath = $UnsignedMsiPath
        FinalChecksumPath = "$UnsignedMsiPath.sha256"
    }
)
$externalSigningFinalArtifacts = @(
    [pscustomobject]@{
        FinalArtifactPath = $ExternalSigningMsiPath
        FinalChecksumPath = "$ExternalSigningMsiPath.sha256"
    }
)
$allowedFinalArtifacts = if ($ExternalSigning) { $externalSigningFinalArtifacts } elseif ($AllowUnsignedPackage) { $unsignedFinalArtifacts } else { $signedFinalArtifacts }
$requestedFinalArtifacts = @()
if ($ExternalSigning) {
    if ($CreateMsi) {
        $requestedFinalArtifacts += $externalSigningFinalArtifacts[0]
    }
} else {
    if (-not $SkipPackage) {
        $requestedFinalArtifacts += $allowedFinalArtifacts[0]
    }
    if ($CreateMsi) {
        $requestedFinalArtifacts += $allowedFinalArtifacts[1]
    }
}

if ($Clean) {
    foreach ($recoverySet in @(
            [pscustomobject]@{
                JournalPath = $SignedPackagePublicationJournalPath
                Artifacts = $signedFinalArtifacts
            },
            [pscustomobject]@{
                JournalPath = $UnsignedPackagePublicationJournalPath
                Artifacts = $unsignedFinalArtifacts
            },
            [pscustomobject]@{
                JournalPath = $ExternalSigningPublicationJournalPath
                Artifacts = $externalSigningFinalArtifacts
            }
        )) {
        if (Test-Path -LiteralPath $recoverySet.JournalPath -PathType Leaf) {
            Restore-PackageArtifactSetBackups `
                -Artifacts $recoverySet.Artifacts `
                -JournalPath $recoverySet.JournalPath
        }
    }
} elseif ($packageRequested) {
    $recoveryArtifacts = if (Test-Path -LiteralPath $PackagePublicationJournalPath -PathType Leaf) {
        $allowedFinalArtifacts
    } else {
        $requestedFinalArtifacts
    }
    Restore-PackageArtifactSetBackups `
        -Artifacts $recoveryArtifacts `
        -JournalPath $PackagePublicationJournalPath
}

Assert-AuthenticodePolicy `
    -SigningRequested ([bool]$Sign) `
    -UnsignedPackageAllowed ([bool]$AllowUnsignedPackage) `
    -ExternalSigningRequested ([bool]$ExternalSigning) `
    -ZipPackageSkipped ([bool]$SkipPackage) `
    -MsiRequested ([bool]$CreateMsi) `
    -TestsSkipped ([bool]$SkipTests) `
    -Configuration $BuildType `
    -CertificateThumbprint $SigningCertificateThumbprint `
    -Rfc3161TimestampUrl $TimestampUrl `
    -UnsignedReleaseExpiresAtUtc $UnsignedPublicReleaseExpiresAtUtc

$ResolvedSignTool = ""
$ResolvedSigningCertificateThumbprint = ""
if ($Sign) {
    Write-Step "Resolving Authenticode identity"
    $ResolvedSignTool = Resolve-SignTool -ConfiguredPath $SignToolPath
    $signingCertificate = Resolve-CodeSigningCertificate `
        -CertificateThumbprint $SigningCertificateThumbprint `
        -StoreLocation $SigningCertificateStoreLocation
    $ResolvedSigningCertificateThumbprint = Normalize-CertificateThumbprint -Thumbprint $signingCertificate.Thumbprint
    Write-Host "SignTool: $ResolvedSignTool"
    Write-Host "Certificate store: $SigningCertificateStoreLocation\My"
    Write-Host "Certificate expires: $($signingCertificate.NotAfter.ToString('u'))"
}

if ((-not $SkipPackage) -or $CreateMsi) {
    $partialArtifacts = @()
    if (-not $SkipPackage) {
        $partialArtifacts += @($ZipPartialPath, $ZipChecksumPartialPath)
    }
    if ($CreateMsi) {
        $partialArtifacts += @($MsiPartialPath, $MsiPartialDebugPath, $MsiChecksumPartialPath)
    }
    $partialArtifacts += "$PackagePublicationJournalPath.partial"
    Clear-WindowsPackageArtifacts -Paths $partialArtifacts
}

Write-Step "Resolving toolchain"
$QtPrefix = Resolve-QtPrefix $QtPrefix
Import-VisualStudioEnvironment $QtPrefix
$CMake = Get-RequiredCommand "cmake.exe"
if ($Generator -eq "Ninja") {
    Get-RequiredCommand "ninja.exe" | Out-Null
}
$CTest = Get-RequiredCommand "ctest.exe"
$WindeployQt = Join-Path $QtPrefix "bin\windeployqt.exe"

$LVRSPrefix = Resolve-DependencyPrefix `
    -Name "LVRS" `
    -ConfiguredPrefix $LVRSPrefix `
    -DefaultPrefix (Join-Path $HOME ".local\SDK\LVRS") `
    -ConfigRelativePaths @("LVRSConfig.cmake", "lib\cmake\LVRS\LVRSConfig.cmake", "share\cmake\LVRS\LVRSConfig.cmake")

$IiPaintEnginePrefix = Resolve-DependencyPrefix `
    -Name "iiPaintEngine" `
    -ConfiguredPrefix $IiPaintEnginePrefix `
    -DefaultPrefix (Join-Path $HOME ".local\SDK\iiPaintEngine") `
    -ConfigRelativePaths @("lib\cmake\iiPaintEngine\iiPaintEngineConfig.cmake", "iiPaintEngineConfig.cmake")

$IiSharedCanvasPrefix = Resolve-DependencyPrefix `
    -Name "iiSharedCanvas" `
    -ConfiguredPrefix $IiSharedCanvasPrefix `
    -DefaultPrefix (Join-Path $HOME ".local\SDK\iiSharedCanvas") `
    -ConfigRelativePaths @("lib\cmake\iiSharedCanvas\iiSharedCanvasConfig.cmake", "iiSharedCanvasConfig.cmake")

$IiUpdateManagerPrefix = Resolve-DependencyPrefix `
    -Name "iiUpdateManager" `
    -ConfiguredPrefix $IiUpdateManagerPrefix `
    -DefaultPrefix (Join-Path $HOME ".local\SDK\iiUpdateManager") `
    -ConfigRelativePaths @("lib\cmake\iiUpdateManager\iiUpdateManagerConfig.cmake", "iiUpdateManagerConfig.cmake")

$IiLicenseManagerPrefix = Resolve-DependencyPrefix `
    -Name "iiLicenseManager" `
    -ConfiguredPrefix $IiLicenseManagerPrefix `
    -DefaultPrefix (Join-Path $HOME ".local\SDK\iiLicenseManager") `
    -ConfigRelativePaths @("lib\cmake\iiLicenseManager\iiLicenseManagerConfig.cmake", "iiLicenseManagerConfig.cmake")

Write-Host "Qt: $QtPrefix"
Write-Host "LVRS: $LVRSPrefix"
Write-Host "iiPaintEngine: $IiPaintEnginePrefix"
Write-Host "iiSharedCanvas: $IiSharedCanvasPrefix"
Write-Host "iiUpdateManager: $IiUpdateManagerPrefix"
Write-Host "iiLicenseManager: $IiLicenseManagerPrefix"

$windowsPackageRequested = (-not $SkipPackage) -or $CreateMsi
$lvrsLicenseFile = ""
$iiPaintEngineLicenseFile = ""
$iiSharedCanvasLicenseFile = ""
$iiLicenseManagerNoticeFile = ""
if ($windowsPackageRequested) {
    $dependencySourceRoot = Split-Path -Parent $RepositoryRoot
    $lvrsLicenseFile = Resolve-DependencyLicenseFile `
        -Name "LVRS" `
        -Prefix $LVRSPrefix `
        -RelativeCandidates @(
            "share\licenses\LVRS\LICENSE",
            "share\licenses\LVRS\LICENSE.txt",
            "LICENSE"
        ) `
        -AdditionalCandidates @((Join-Path $dependencySourceRoot "LVRS\LICENSE"))
    if (-not $lvrsLicenseFile) {
        throw "Windows packaging requires the LVRS AGPL license file."
    }

    $iiPaintEngineLicenseFile = Resolve-DependencyLicenseFile `
        -Name "iiPaintEngine" `
        -Prefix $IiPaintEnginePrefix `
        -RelativeCandidates @(
            "share\licenses\iiPaintEngine\LICENSE",
            "share\licenses\iiPaintEngine\LICENSE.txt",
            "LICENSE"
        ) `
        -AdditionalCandidates @((Join-Path $dependencySourceRoot "iiPaintEngine\LICENSE"))
    $iiSharedCanvasLicenseFile = Resolve-DependencyLicenseFile `
        -Name "iiSharedCanvas" `
        -Prefix $IiSharedCanvasPrefix `
        -RelativeCandidates @(
            "share\licenses\iiSharedCanvas\LICENSE",
            "share\licenses\iiSharedCanvas\LICENSE.txt",
            "LICENSE"
        ) `
        -AdditionalCandidates @((Join-Path $dependencySourceRoot "iiSharedCanvas\LICENSE"))
    if (-not $iiSharedCanvasLicenseFile) {
        throw "Windows packaging requires the iiSharedCanvas AGPL license file."
    }
    $iiLicenseManagerNoticeFile = Resolve-DependencyLicenseFile `
        -Name "iiLicenseManager third-party notices" `
        -Prefix $IiLicenseManagerPrefix `
        -RelativeCandidates @(
            "share\iiLicenseManager\THIRD_PARTY_NOTICES.md",
            "THIRD_PARTY_NOTICES.md"
        )
    if (-not $iiLicenseManagerNoticeFile) {
        throw "Windows packaging requires iiLicenseManager third-party notices."
    }
    Assert-PublicDistributionEvidence `
        -PublicRelease ([bool]($Sign -or $ExternalSigning -or $AllowUnsignedPackage)) `
        -IiPaintEngineLicenseFile $iiPaintEngineLicenseFile `
        -IiSharedCanvasLicenseFile $iiSharedCanvasLicenseFile `
        -IiLicenseManagerNoticeFile $iiLicenseManagerNoticeFile `
        -SourceUrl $CorrespondingSourceUrl `
        -SourceSha256 $CorrespondingSourceSha256
}

if ($Clean) {
    Write-Step "Cleaning generated Windows artifacts"
    $cleanPackagePaths = @(
        $SignedZipPath,
        "$SignedZipPath.sha256",
        $SignedZipPartialPath,
        "$SignedZipPath.sha256.partial",
        "$SignedZipPath.previous",
        "$SignedZipPath.sha256.previous",
        $UnsignedZipPath,
        "$UnsignedZipPath.sha256",
        $UnsignedZipPartialPath,
        "$UnsignedZipPath.sha256.partial",
        "$UnsignedZipPath.previous",
        "$UnsignedZipPath.sha256.previous",
        $SignedPackagePublicationJournalPath,
        "$SignedPackagePublicationJournalPath.partial",
        "$SignedPackagePublicationJournalPath.previous",
        $UnsignedPackagePublicationJournalPath,
        "$UnsignedPackagePublicationJournalPath.partial",
        "$UnsignedPackagePublicationJournalPath.previous",
        $ExternalSigningMsiPath,
        "$ExternalSigningMsiPath.sha256",
        $ExternalSigningMsiPartialPath,
        $ExternalSigningMsiPartialDebugPath,
        "$ExternalSigningMsiPath.sha256.partial",
        "$ExternalSigningMsiPath.previous",
        "$ExternalSigningMsiPath.sha256.previous",
        $ExternalSigningPublicationJournalPath,
        "$ExternalSigningPublicationJournalPath.partial",
        "$ExternalSigningPublicationJournalPath.previous"
    )
    Clear-WindowsPackageArtifacts -Paths $cleanPackagePaths
    Remove-Item $BuildDir -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item $StageDir -Recurse -Force -ErrorAction SilentlyContinue
}

$prefixPath = @($QtPrefix, $LVRSPrefix, $IiPaintEnginePrefix, $IiSharedCanvasPrefix, $IiUpdateManagerPrefix, $IiLicenseManagerPrefix) -join ";"
$buildTesting = if ($SkipTests) { "OFF" } else { "ON" }

Write-Step "Configuring Congregation"
Invoke-Native $CMake @(
    "-S", $RepositoryRoot,
    "-B", $BuildDir,
    "-G", $Generator,
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DBUILD_TESTING=$buildTesting",
    "-DCMAKE_PREFIX_PATH=$prefixPath"
)

Write-Step "Building Congregation"
$buildArguments = @("--build", $BuildDir, "--config", $BuildType, "--parallel")
if ($SkipTests) {
    $buildArguments += @("--target", "Congregation")
}
Invoke-Native $CMake $buildArguments

if (-not $SkipTests) {
    Write-Step "Running tests"
    Invoke-Native $CTest @("--test-dir", $BuildDir, "--output-on-failure", "-C", $BuildType)
}

Write-Step "Staging Windows runtime"
New-Item -ItemType Directory -Force $DistRoot | Out-Null
Remove-Item $StageDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $StageDir | Out-Null

$CongregationExecutable = Resolve-CongregationExecutable -BuildDirectory $BuildDir -Configuration $BuildType
$StagedCongregationExecutable = Join-Path $StageDir "Congregation.exe"
Copy-Item $CongregationExecutable -Destination $StagedCongregationExecutable -Force
if ((Get-FileHash -Algorithm SHA256 $CongregationExecutable).Hash -ne (Get-FileHash -Algorithm SHA256 $StagedCongregationExecutable).Hash) {
    throw "Staged Congregation.exe does not match the executable produced by the current build."
}

$deployMode = if ($BuildType -eq "Debug") { "--debug" } else { "--release" }
Invoke-Native $WindeployQt @(
    $deployMode,
    "--force",
    "--compiler-runtime",
    "--no-system-d3d-compiler",
    "--no-opengl-sw",
    "--no-system-dxc-compiler",
    "--translations", "en,ko",
    "--skip-plugin-types", "qmltooling,generic,sqldrivers",
    "--exclude-plugins", "qpdf,qtvirtualkeyboardplugin",
    "--verbose", "0",
    "--qmldir", (Join-Path $RepositoryRoot "App\qml"),
    $StagedCongregationExecutable
)

Copy-DependencyRuntimeFiles -Name "LVRS" -Prefix $LVRSPrefix -Destination $StageDir
Copy-DependencyRuntimeFiles -Name "iiPaintEngine" -Prefix $IiPaintEnginePrefix -Destination $StageDir
Copy-DependencyRuntimeFiles -Name "iiSharedCanvas" -Prefix $IiSharedCanvasPrefix -Destination $StageDir
Copy-DependencyRuntimeFiles -Name "iiUpdateManager" -Prefix $IiUpdateManagerPrefix -Destination $StageDir
Copy-DependencyRuntimeFiles -Name "iiLicenseManager" -Prefix $IiLicenseManagerPrefix -Destination $StageDir
Remove-EmbeddedDependencyQmlImport -ModuleName "LVRS" -Destination $StageDir
Remove-ReleaseOnlyQtArtifacts -Directory $StageDir -BuildType $BuildType
if ($windowsPackageRequested) {
    Copy-WindowsLegalMaterials `
        -Directory $StageDir `
        -ResolvedQtPrefix $QtPrefix `
        -LvrsLicenseFile $lvrsLicenseFile `
        -IiPaintEngineLicenseFile $iiPaintEngineLicenseFile `
        -IiSharedCanvasLicenseFile $iiSharedCanvasLicenseFile `
        -IiLicenseManagerNoticeFile $iiLicenseManagerNoticeFile `
        -PublicRelease ([bool]($Sign -or $ExternalSigning -or $AllowUnsignedPackage)) `
        -SourceUrl $CorrespondingSourceUrl `
        -SourceSha256 $CorrespondingSourceSha256
}
Strip-WindowsRuntimeBinaries -Directory $StageDir -ResolvedQtPrefix $QtPrefix -Configuration $BuildType
Verify-WindowsStage -Directory $StageDir -ResolvedQtPrefix $QtPrefix -ExpectedFileVersion $WindowsFileVersion -BuildType $BuildType

if ($Sign) {
    Write-Step "Signing and verifying staged Windows binaries"
    Sign-WindowsStage -Directory $StageDir -SignTool $ResolvedSignTool -CertificateThumbprint $ResolvedSigningCertificateThumbprint -StoreLocation $SigningCertificateStoreLocation -TimestampUrl $TimestampUrl
    Verify-WindowsStageSignatures -Directory $StageDir -SignTool $ResolvedSignTool -CertificateThumbprint $ResolvedSigningCertificateThumbprint
} elseif ($ExternalSigning) {
    Write-Warning "The staged application is an unsigned SignPath input and must not be distributed before external signing completes."
} else {
    Write-Warning "The staged application is an unsigned public release under the temporary 2026 policy. Windows may show Unknown publisher or SmartScreen warnings."
}

$packageArtifacts = @()
try {
    if (-not $SkipPackage) {
        Write-Step "Creating ZIP package"
        Compress-Archive -Path (Join-Path $StageDir "*") -DestinationPath $ZipPartialPath -Force
        Write-Sha256File `
            -File $ZipPartialPath `
            -OutputPath $ZipChecksumPartialPath `
            -RecordedFileName ([System.IO.Path]::GetFileName($ZipPath))
        $packageArtifacts += [pscustomobject]@{
            PartialArtifactPath = $ZipPartialPath
            FinalArtifactPath = $ZipPath
            PartialChecksumPath = $ZipChecksumPartialPath
            FinalChecksumPath = $ZipChecksumPath
        }
    }

    if ($CreateMsi) {
        Write-Step "Creating MSI installer"
        if ($ExternalSigning) {
            New-Item -ItemType Directory -Path $ExternalSigningInputDir -Force | Out-Null
        }
        $msiVersion = if ($Version -match '^\d+\.\d+$') { "$Version.0" } else { $Version }
        New-MsiInstaller -SourceDirectory $StageDir -OutputPath $MsiPartialPath -WorkDirectory $MsiWorkDir -ToolsDirectory $WixToolsDir -ProductVersion $msiVersion
        if ($Sign) {
            Sign-AuthenticodeFile -File $MsiPartialPath -SignTool $ResolvedSignTool -CertificateThumbprint $ResolvedSigningCertificateThumbprint -StoreLocation $SigningCertificateStoreLocation -TimestampUrl $TimestampUrl
            Verify-AuthenticodeFile -File $MsiPartialPath -SignTool $ResolvedSignTool -ExpectedCertificateThumbprint $ResolvedSigningCertificateThumbprint
        } elseif ($ExternalSigning) {
            Write-Warning "The MSI is an unsigned SignPath input and must not be distributed before external signing completes."
        } else {
            Write-Warning "The MSI is an unsigned public release under the temporary 2026 policy. Publish its authenticated SHA-256 with the download."
        }
        Assert-MsiDatabaseContract -MsiFile $MsiPartialPath -ProductVersion $msiVersion
        Write-Sha256File `
            -File $MsiPartialPath `
            -OutputPath $MsiChecksumPartialPath `
            -RecordedFileName ([System.IO.Path]::GetFileName($MsiPath))
        $packageArtifacts += [pscustomobject]@{
            PartialArtifactPath = $MsiPartialPath
            FinalArtifactPath = $MsiPath
            PartialChecksumPath = $MsiChecksumPartialPath
            FinalChecksumPath = $MsiChecksumPath
        }
    }

    if ($packageArtifacts.Count -gt 0) {
        Write-Step "Publishing verified package artifacts"
        Publish-PackageArtifactSet `
            -Artifacts $packageArtifacts `
            -JournalPath $PackagePublicationJournalPath
    }
} finally {
    if (-not $SkipPackage) {
        Remove-Item -LiteralPath $ZipPartialPath -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $ZipChecksumPartialPath -Force -ErrorAction SilentlyContinue
    }
    if ($CreateMsi) {
        Remove-Item -LiteralPath $MsiPartialPath -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $MsiPartialDebugPath -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $MsiChecksumPartialPath -Force -ErrorAction SilentlyContinue
    }
}

if (-not $SkipPackage) {
    Write-Host "Package: $ZipPath"
}
if ($CreateMsi) {
    Write-Host "MSI: $MsiPath"
}

if ($InstallForCurrentUser) {
    Install-ForCurrentUser -SourceDirectory $StageDir -TargetDirectory $InstallDir
}

Write-Step "Done"
Write-Host "Staged app: $StageDir"
} finally {
    if ($null -ne $WindowsBuildMutex) {
        $WindowsBuildMutex.ReleaseMutex()
        $WindowsBuildMutex.Dispose()
    }
}
