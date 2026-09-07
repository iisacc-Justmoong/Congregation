#Requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern("^\d+\.\d+(?:\.\d+)?$")]
    [string]$Version,

    [string]$CongregationRevision = "",
    [string]$RepositoryRoot = "",
    [string]$LvrsSource = "",
    [string]$IiPaintEngineSource = "",
    [string]$IiSharedCanvasSource = "",
    [string]$IiUpdateManagerSource = "",
    [string]$IiLicenseManagerSource = "",
    [string]$PsdSdkSource = "",
    [string]$QtKeychainSource = "",
    [string]$QtSourceRoot = $env:QT_SOURCE_ROOT,
    [string]$OutputDirectory = "",

    [ValidatePattern("^[0-9a-fA-F]{40}$")]
    [string]$LvrsRevision = "8132a48e4c0788f0548bff20fd7089878a09c760",
    [ValidatePattern("^[0-9a-fA-F]{40}$")]
    [string]$IiPaintEngineRevision = "5ade15e24ce3a7eab25669144fdee668672b22d6",
    [ValidatePattern("^[0-9a-fA-F]{40}$")]
    [string]$IiSharedCanvasRevision = "d2c41bf605725eb07b181587321f0ce910b9924d",
    [Parameter(Mandatory = $true)]
    [ValidatePattern("^[0-9a-fA-F]{40}$")]
    [string]$IiUpdateManagerRevision,
    [Parameter(Mandatory = $true)]
    [ValidatePattern("^[0-9a-fA-F]{40}$")]
    [string]$IiLicenseManagerRevision,
    [ValidatePattern("^[0-9a-fA-F]{40}$")]
    [string]$PsdSdkRevision = "f51449543273cbf12058ae92b230e0c4209f5066",
    [ValidatePattern("^[0-9a-fA-F]{40}$")]
    [string]$QtKeychainRevision = "875f77d9f61bd97fd84cca47ce3bc71186dfbd09"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = Split-Path -Parent $PSScriptRoot
}
$RepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
$parentDirectory = Split-Path -Parent $RepositoryRoot

if ([string]::IsNullOrWhiteSpace($CongregationRevision)) {
    $CongregationRevision = "v$Version"
}
if ([string]::IsNullOrWhiteSpace($LvrsSource)) {
    $LvrsSource = Join-Path $parentDirectory "LVRS"
}
if ([string]::IsNullOrWhiteSpace($IiPaintEngineSource)) {
    $IiPaintEngineSource = Join-Path $parentDirectory "iiPaintEngine"
}
if ([string]::IsNullOrWhiteSpace($IiSharedCanvasSource)) {
    $IiSharedCanvasSource = Join-Path $parentDirectory "iiSharedCanvas"
}
if ([string]::IsNullOrWhiteSpace($IiUpdateManagerSource)) {
    $IiUpdateManagerSource = Join-Path $parentDirectory "iiUpdateManager"
}
if ([string]::IsNullOrWhiteSpace($IiLicenseManagerSource)) {
    $IiLicenseManagerSource = Join-Path $parentDirectory "iiLicenseManager"
}
if ([string]::IsNullOrWhiteSpace($PsdSdkSource)) {
    $PsdSdkSource = Join-Path $RepositoryRoot "build\_deps\psd_sdk-src"
}
if ([string]::IsNullOrWhiteSpace($QtKeychainSource)) {
    $QtKeychainSource = Join-Path $RepositoryRoot "build\_deps\qtkeychain-src"
}
if ([string]::IsNullOrWhiteSpace($QtSourceRoot)) {
    $QtSourceRoot = "C:\Qt\6.8.3\Src"
}

$buildRoot = [System.IO.Path]::GetFullPath((Join-Path $RepositoryRoot "build"))
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $buildRoot "release-source"
}
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
$bundleName = "Congregation-$Version-Corresponding-Source"
$stageDirectory = [System.IO.Path]::GetFullPath((Join-Path $OutputDirectory $bundleName))
$archivePath = [System.IO.Path]::GetFullPath((Join-Path $OutputDirectory "$bundleName.zip"))
$sidecarPath = "$archivePath.sha256"
$utf8 = New-Object System.Text.UTF8Encoding($false)

function Assert-PathWithinRoot {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $fullRoot = [System.IO.Path]::GetFullPath($Root).TrimEnd("\", "/")
    $prefix = $fullRoot + [System.IO.Path]::DirectorySeparatorChar
    if (-not $fullPath.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label must remain below $fullRoot, but resolved to $fullPath"
    }
}

function Get-GitCommandOutput {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][scriptblock]$Command,
        [Parameter(Mandatory = $true)][string]$FailureMessage
    )

    Push-Location -LiteralPath $Source
    try {
        $output = @(& $Command)
        if ($LASTEXITCODE -ne 0) {
            throw $FailureMessage
        }
        return $output
    } finally {
        Pop-Location
    }
}

function Copy-GitRevision {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination,
        [Parameter(Mandatory = $true)][string]$Label,
        [Parameter(Mandatory = $true)][string]$Revision
    )

    $sourceFull = [System.IO.Path]::GetFullPath($Source)
    if (-not (Test-Path -LiteralPath (Join-Path $sourceFull ".git"))) {
        throw "$Label is not a Git working tree: $sourceFull"
    }

    $resolvedOutput = Get-GitCommandOutput -Source $sourceFull -FailureMessage "$Label revision could not be resolved: $Revision" -Command {
        & git rev-parse "$Revision^{commit}"
    }
    $resolvedRevision = ($resolvedOutput -join "").Trim().ToLowerInvariant()
    if ($resolvedRevision -notmatch "^[0-9a-f]{40}$") {
        throw "$Label resolved to an invalid commit: $resolvedRevision"
    }

    $headOutput = Get-GitCommandOutput -Source $sourceFull -FailureMessage "$Label HEAD could not be resolved." -Command {
        & git rev-parse HEAD
    }
    $headRevision = ($headOutput -join "").Trim().ToLowerInvariant()
    if ($headRevision -cne $resolvedRevision) {
        throw "$Label HEAD mismatch. Requested $Revision resolved to $resolvedRevision, but HEAD is $headRevision"
    }

    $dirtyLines = @(Get-GitCommandOutput -Source $sourceFull -FailureMessage "$Label working-tree status failed." -Command {
        & git status --porcelain --untracked-files=all
    })
    if ($dirtyLines.Count -gt 0) {
        Write-Warning "$Label has $($dirtyLines.Count) working-tree change(s); git archive excludes them."
    }

    $trackedFiles = @(Get-GitCommandOutput -Source $sourceFull -FailureMessage "$Label file listing failed." -Command {
        & git ls-tree -r --name-only $resolvedRevision
    })

    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    $temporaryArchive = Join-Path ([System.IO.Path]::GetTempPath()) ("Congregation-source-" + [Guid]::NewGuid().ToString("N") + ".zip")
    try {
        Push-Location -LiteralPath $sourceFull
        try {
            & git archive --format=zip --output=$temporaryArchive $resolvedRevision
            if ($LASTEXITCODE -ne 0) {
                throw "git archive failed for $Label"
            }
        } finally {
            Pop-Location
        }
        Expand-Archive -LiteralPath $temporaryArchive -DestinationPath $Destination -Force
    } finally {
        Remove-Item -LiteralPath $temporaryArchive -Force -ErrorAction SilentlyContinue
    }

    $metadataDirectory = Join-Path $Destination "_release_metadata"
    New-Item -ItemType Directory -Path $metadataDirectory -Force | Out-Null
    $modeLines = @(Get-GitCommandOutput -Source $sourceFull -FailureMessage "$Label tracked-mode listing failed." -Command {
        & git ls-tree -r $resolvedRevision
    })
    [System.IO.File]::WriteAllText(
        (Join-Path $metadataDirectory "REVISION.txt"),
        "Repository: $Label`r`nRequested revision: $Revision`r`nResolved commit: $resolvedRevision`r`nSource snapshot: committed revision via git archive`r`nWorking-tree changes excluded: $($dirtyLines.Count)`r`n",
        $utf8
    )
    [System.IO.File]::WriteAllLines((Join-Path $metadataDirectory "GIT-TREE.txt"), $modeLines, $utf8)

    return [pscustomobject]@{
        Label = $Label
        Source = $sourceFull
        Destination = $Destination
        RequestedRevision = $Revision
        ResolvedCommit = $resolvedRevision
        WorkingTreeChangeCount = $dirtyLines.Count
        FileCount = $trackedFiles.Count
    }
}

if (-not (Test-Path -LiteralPath $RepositoryRoot -PathType Container)) {
    throw "Congregation repository root does not exist: $RepositoryRoot"
}
Assert-PathWithinRoot -Path $OutputDirectory -Root $buildRoot -Label "Corresponding-source output directory"
Assert-PathWithinRoot -Path $stageDirectory -Root $OutputDirectory -Label "Corresponding-source staging directory"
Assert-PathWithinRoot -Path $archivePath -Root $OutputDirectory -Label "Corresponding-source archive"
Assert-PathWithinRoot -Path $sidecarPath -Root $OutputDirectory -Label "Corresponding-source checksum"

Get-Command git.exe -ErrorAction Stop | Out-Null
Get-Command robocopy.exe -ErrorAction Stop | Out-Null
Get-Command tar.exe -ErrorAction Stop | Out-Null

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
foreach ($path in @($stageDirectory, $archivePath, $sidecarPath)) {
    if (Test-Path -LiteralPath $path) {
        Remove-Item -LiteralPath $path -Recurse -Force
    }
}
New-Item -ItemType Directory -Path $stageDirectory -Force | Out-Null

$components = @()
$components += Copy-GitRevision `
    -Source $RepositoryRoot `
    -Destination (Join-Path $stageDirectory "Congregation") `
    -Label "Congregation" `
    -Revision $CongregationRevision
$congregationCMakePath = Join-Path $stageDirectory "Congregation\CMakeLists.txt"
$congregationCMake = Get-Content -LiteralPath $congregationCMakePath -Raw
$declaredVersionPattern = "project\s*\(\s*Congregation\s+VERSION\s+$([regex]::Escape($Version))(?:\s|\))"
if ($congregationCMake -notmatch $declaredVersionPattern) {
    throw "Congregation release revision does not declare version $Version."
}
$components += Copy-GitRevision `
    -Source $LvrsSource `
    -Destination (Join-Path $stageDirectory "LVRS") `
    -Label "LVRS" `
    -Revision $LvrsRevision
$components += Copy-GitRevision `
    -Source $IiPaintEngineSource `
    -Destination (Join-Path $stageDirectory "iiPaintEngine") `
    -Label "iiPaintEngine" `
    -Revision $IiPaintEngineRevision
$components += Copy-GitRevision `
    -Source $IiSharedCanvasSource `
    -Destination (Join-Path $stageDirectory "iiSharedCanvas") `
    -Label "iiSharedCanvas" `
    -Revision $IiSharedCanvasRevision
$components += Copy-GitRevision `
    -Source $IiUpdateManagerSource `
    -Destination (Join-Path $stageDirectory "iiUpdateManager") `
    -Label "iiUpdateManager" `
    -Revision $IiUpdateManagerRevision
$components += Copy-GitRevision `
    -Source $IiLicenseManagerSource `
    -Destination (Join-Path $stageDirectory "iiLicenseManager") `
    -Label "iiLicenseManager" `
    -Revision $IiLicenseManagerRevision
$components += Copy-GitRevision `
    -Source $PsdSdkSource `
    -Destination (Join-Path $stageDirectory "third_party\psd_sdk") `
    -Label "psd_sdk" `
    -Revision $PsdSdkRevision
$components += Copy-GitRevision `
    -Source $QtKeychainSource `
    -Destination (Join-Path $stageDirectory "third_party\qtkeychain") `
    -Label "QtKeychain" `
    -Revision $QtKeychainRevision

$QtSourceRoot = [System.IO.Path]::GetFullPath($QtSourceRoot)
$qtDestination = Join-Path $stageDirectory "third_party\Qt-6.8.3"
New-Item -ItemType Directory -Path $qtDestination -Force | Out-Null
$qtModules = @("qtbase", "qtdeclarative", "qtsvg", "qtimageformats", "qttranslations")
foreach ($module in $qtModules) {
    $source = Join-Path $QtSourceRoot $module
    if (-not (Test-Path -LiteralPath $source -PathType Container)) {
        throw "Required Qt source module is missing: $source"
    }
    $destination = Join-Path $qtDestination $module
    & robocopy.exe $source $destination /E /COPY:DAT /DCOPY:DAT /R:1 /W:1 /XD .git build /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -gt 7) {
        throw "robocopy failed for Qt module $module with exit code $LASTEXITCODE"
    }
}

$componentLines = @(
    "Congregation $Version corresponding-source component manifest",
    "=====================================================",
    "",
    "First-party and pinned source trees:"
)
foreach ($component in $components) {
    $componentLines += "- $($component.Label): requested $($component.RequestedRevision), commit $($component.ResolvedCommit), working-tree changes excluded $($component.WorkingTreeChangeCount), files $($component.FileCount)"
}
$componentLines += @(
    "- Qt modules: $($qtModules -join ', ') from the local Qt 6.8.3 source installation.",
    "- Windows toolchain: Qt MinGW 13.1.0. Runtime license notices and the GCC Runtime Library Exception are under Congregation/packaging/windows.",
    "",
    "Git components are committed revision snapshots. Local working-tree changes and untracked files are excluded."
)
[System.IO.File]::WriteAllLines((Join-Path $stageDirectory "COMPONENTS.txt"), $componentLines, $utf8)

$buildGuideTemplate = @'
# Congregation @@VERSION@@ Corresponding Source

This archive accompanies the Windows website build of Congregation @@VERSION@@. It contains the exact committed first-party source trees used for the release, the pinned psd_sdk and QtKeychain sources, the Qt 6.8.3 module sources conveyed with the application, build and packaging scripts, and license material.

## Windows build

1. Install Qt 6.8.3 MinGW 64-bit and the matching MinGW 13.1.0 and Ninja tools.
2. Install LVRS and iiPaintEngine using their included PowerShell install scripts. Configure, test, and install iiSharedCanvas and iiUpdateManager from each repository-local `build/` directory. Install static libsodium with vcpkg, then configure, test, and install iiLicenseManager from its repository-local `build/` directory with the `x64-mingw-static` triplet.
3. From `Congregation`, configure, build, and test using the repository-local `build/` directory.
4. Set the public URL and SHA-256 of this archive. During the temporary 2026 unsigned policy, run `powershell -ExecutionPolicy Bypass -File .\build-windows.ps1 -BuildType Release -AllowUnsignedPackage -SkipPackage -CreateMsi`. After trusted signing is available, use `-ExternalSigning` instead.

The unsigned MSI is a temporary website release only through 2026. It exposes an authenticated SHA-256 but has no Authenticode publisher identity, so Windows may display Unknown publisher or SmartScreen warnings. The SignPath path remains the preferred signed successor.

Repository metadata under `_release_metadata` records each exact committed release revision. Local working-tree changes are excluded. `ARCHIVE-CONTENTS-SHA256.txt` records every file in this source tree before compression.
'@
$buildGuide = $buildGuideTemplate.Replace("@@VERSION@@", $Version)
[System.IO.File]::WriteAllText((Join-Path $stageDirectory "BUILD-SOURCE.md"), $buildGuide.Trim() + "`r`n", $utf8)

$hashLines = New-Object System.Collections.Generic.List[string]
$allFiles = @(Get-ChildItem -LiteralPath $stageDirectory -Recurse -File | Sort-Object FullName)
foreach ($file in $allFiles) {
    if ($file.Name -eq "ARCHIVE-CONTENTS-SHA256.txt") {
        continue
    }
    $relative = $file.FullName.Substring($stageDirectory.Length + 1).Replace("\", "/")
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $hashLines.Add("$hash *$relative")
}
[System.IO.File]::WriteAllLines((Join-Path $stageDirectory "ARCHIVE-CONTENTS-SHA256.txt"), $hashLines, $utf8)

& tar.exe -a -c -f $archivePath -C $OutputDirectory $bundleName
if ($LASTEXITCODE -ne 0) {
    throw "Archive creation failed with exit code $LASTEXITCODE"
}

$archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToUpperInvariant()
[System.IO.File]::WriteAllText($sidecarPath, "$archiveHash *$bundleName.zip`r`n", $utf8)

$entries = @(& tar.exe -tf $archivePath)
if ($LASTEXITCODE -ne 0) {
    throw "Archive listing failed."
}
$requiredSuffixes = @(
    "/Congregation/CMakeLists.txt",
    "/LVRS/CMakeLists.txt",
    "/iiPaintEngine/LICENSE",
    "/iiSharedCanvas/CMakeLists.txt",
    "/iiSharedCanvas/LICENSE",
    "/iiUpdateManager/CMakeLists.txt",
    "/iiLicenseManager/CMakeLists.txt",
    "/iiLicenseManager/THIRD_PARTY_NOTICES.md",
    "/third_party/psd_sdk/LICENSE",
    "/third_party/qtkeychain/COPYING",
    "/third_party/Qt-6.8.3/qtbase/LICENSES/LGPL-3.0-only.txt",
    "/BUILD-SOURCE.md",
    "/COMPONENTS.txt",
    "/ARCHIVE-CONTENTS-SHA256.txt"
)
$missingRequired = @()
foreach ($suffix in $requiredSuffixes) {
    if (-not ($entries | Where-Object { $_.Replace("\", "/").EndsWith($suffix) })) {
        $missingRequired += $suffix
    }
}
if ($missingRequired.Count -gt 0) {
    throw "Archive verification missing: $($missingRequired -join ', ')"
}

$forbiddenEntries = @($entries | Where-Object {
    $normalized = $_.Replace("\", "/")
    $normalized -cmatch "(^|/)\.git(/|$)"
})
if ($forbiddenEntries.Count -gt 0) {
    throw "Archive contains forbidden repository metadata paths: $($forbiddenEntries -join ', ')"
}

[pscustomobject]@{
    Archive = $archivePath
    Sidecar = $sidecarPath
    Sha256 = $archiveHash
    ArchiveBytes = (Get-Item -LiteralPath $archivePath).Length
    SourceFileCount = $allFiles.Count
    ArchiveEntryCount = $entries.Count
    Components = $components
    MissingRequired = $missingRequired
    ForbiddenEntries = $forbiddenEntries
} | ConvertTo-Json -Depth 6
