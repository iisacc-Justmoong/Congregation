#Requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateSet("Store", "Development")]
    [string]$Mode = "Development",

    [ValidateSet("Release", "MinSizeRel")]
    [string]$BuildType = "Release",

    [switch]$SkipBuild,
    [switch]$InstallDevelopment,

    [string]$IdentityName = $env:CONGREGATION_STORE_IDENTITY_NAME,
    [string]$Publisher = $env:CONGREGATION_STORE_PUBLISHER,
    [string]$DisplayName = $env:CONGREGATION_STORE_DISPLAY_NAME,
    [string]$PublisherDisplayName = $env:CONGREGATION_STORE_PUBLISHER_DISPLAY_NAME,
    [string]$DevelopmentCertificateThumbprint = $env:CONGREGATION_DEVELOPMENT_SIGNING_CERTIFICATE_THUMBPRINT,
    [string]$CorrespondingSourceUrl = $env:CONGREGATION_CORRESPONDING_SOURCE_URL,
    [string]$CorrespondingSourceSha256 = $env:CONGREGATION_CORRESPONDING_SOURCE_SHA256,
    [string]$TimestampUrl = $(if ($env:CONGREGATION_TIMESTAMP_URL) { $env:CONGREGATION_TIMESTAMP_URL } else { "http://timestamp.digicert.com" }),
    [string]$MakeAppxPath = $env:MAKEAPPX_PATH,
    [string]$SignToolPath = $env:SIGNTOOL_PATH
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Version = "1.0.0"
$packageVersionParts = @($Version -split '\.')
while ($packageVersionParts.Count -lt 4) {
    $packageVersionParts += "0"
}
$PackageVersion = $packageVersionParts[0..3] -join "."
$RepositoryRoot = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$BuildDirectory = Join-Path $RepositoryRoot "build"
$DistDirectory = Join-Path $RepositoryRoot "dist"
$StageDirectory = Join-Path $RepositoryRoot "dist\Congregation-Windows"
$WorkDirectory = Join-Path $BuildDirectory "msix-store"
$ContentDirectory = Join-Path $WorkDirectory "content"
$ManifestTemplatePath = Join-Path $RepositoryRoot "packaging\windows\AppxManifest.xml.in"
$IconSourcePath = Join-Path $RepositoryRoot "packaging\macos\Congregation.xcassets\AppIcon.appiconset\AppIcon-1024.png"

function Write-Step {
    param([string]$Message)
    Write-Host ""
    Write-Host "=== $Message ==="
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

function Assert-StorePackagePolicy {
    param(
        [ValidateSet("Store", "Development")]
        [string]$Mode,
        [string]$IdentityName,
        [string]$Publisher,
        [string]$DisplayName,
        [string]$PublisherDisplayName,
        [string]$PackageVersion,
        [string]$DevelopmentCertificateThumbprint
    )

    if ([string]::IsNullOrWhiteSpace($IdentityName)) {
        throw "Package/Identity/Name is required. Copy it exactly from Partner Center Product identity."
    }
    if ([string]::IsNullOrWhiteSpace($Publisher)) {
        throw "Package/Identity/Publisher is required. Copy it exactly from Partner Center Product identity."
    }
    if ([string]::IsNullOrWhiteSpace($DisplayName)) {
        throw "Package/Properties/DisplayName is required. Copy an exact reserved app name from Partner Center."
    }
    if ([string]::IsNullOrWhiteSpace($PublisherDisplayName)) {
        throw "Package/Properties/PublisherDisplayName is required. Copy it exactly from Partner Center Product identity."
    }

    $versionMatch = [regex]::Match($PackageVersion, '^(?<major>\d+)\.(?<minor>\d+)\.(?<build>\d+)\.(?<revision>\d+)$')
    if (-not $versionMatch.Success) {
        throw "MSIX PackageVersion must contain four numeric fields."
    }
    if ($Mode -eq "Store" -and $versionMatch.Groups["revision"].Value -ne "0") {
        throw "The fourth version field must be 0 for a Microsoft Store package."
    }

    if ($Mode -eq "Development") {
        $normalizedThumbprint = (($DevelopmentCertificateThumbprint | Out-String).Trim() -replace '\s', '').ToUpperInvariant()
        if ($normalizedThumbprint -notmatch '^[0-9A-F]{40}$') {
            throw "Development MSIX signing requires an exact 40-hex certificate thumbprint."
        }
    }
}

function ConvertTo-XmlAttributeValue {
    param([string]$Value)

    if ($null -eq $Value) {
        return ""
    }
    return [System.Security.SecurityElement]::Escape($Value)
}

function Write-StoreAppxManifest {
    param(
        [string]$TemplatePath,
        [string]$OutputPath,
        [string]$IdentityName,
        [string]$Publisher,
        [string]$DisplayName,
        [string]$PublisherDisplayName,
        [string]$PackageVersion
    )

    if (-not (Test-Path -LiteralPath $TemplatePath -PathType Leaf)) {
        throw "MSIX manifest template is missing: $TemplatePath"
    }

    $manifest = Get-Content -LiteralPath $TemplatePath -Raw
    $replacements = [ordered]@{
        "@IDENTITY_NAME@" = ConvertTo-XmlAttributeValue $IdentityName
        "@PUBLISHER@" = ConvertTo-XmlAttributeValue $Publisher
        "@DISPLAY_NAME@" = ConvertTo-XmlAttributeValue $DisplayName
        "@PUBLISHER_DISPLAY_NAME@" = ConvertTo-XmlAttributeValue $PublisherDisplayName
        "@PACKAGE_VERSION@" = ConvertTo-XmlAttributeValue $PackageVersion
    }
    foreach ($entry in $replacements.GetEnumerator()) {
        if (-not $manifest.Contains($entry.Key)) {
            throw "MSIX manifest template token is missing: $($entry.Key)"
        }
        $manifest = $manifest.Replace($entry.Key, $entry.Value)
    }

    $outputDirectory = Split-Path -Parent $OutputPath
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    [System.IO.File]::WriteAllText(
        $OutputPath,
        $manifest,
        (New-Object System.Text.UTF8Encoding($false))
    )

    try {
        [xml](Get-Content -LiteralPath $OutputPath -Raw) | Out-Null
    } catch {
        throw "Generated AppxManifest.xml is not valid XML: $($_.Exception.Message)"
    }
}

function Assert-StorePackageContent {
    param(
        [string]$Directory,
        [bool]$StoreSubmission
    )

    $requiredPaths = @(
        "AppxManifest.xml",
        "Congregation.exe",
        "LICENSE.txt",
        "THIRD_PARTY_NOTICES.txt",
        "SOURCE_OFFER.txt",
        "legal\iiLicenseManager\THIRD_PARTY_NOTICES.md",
        "legal\QtKeychain\COPYING.txt",
        "Assets\StoreLogo.png",
        "Assets\Square44x44Logo.png",
        "Assets\Square150x150Logo.png"
    )
    foreach ($relativePath in $requiredPaths) {
        $path = Join-Path $Directory $relativePath
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "MSIX package content is incomplete: $relativePath"
        }
        if ((Get-Item -LiteralPath $path).Length -le 0) {
            throw "MSIX package content is empty: $relativePath"
        }
    }

    $forbiddenNames = @(
        "Qt6InsightTracker.dll",
        "Qt6Sql.dll",
        "Qt6Quick3DUtils.dll"
    )
    foreach ($forbiddenName in $forbiddenNames) {
        if (Get-ChildItem -LiteralPath $Directory -File -Recurse -ErrorAction Stop |
                Where-Object { $_.Name -ceq $forbiddenName } |
                Select-Object -First 1) {
            throw "MSIX contains a forbidden payload: $forbiddenName"
        }
    }

    if (-not $StoreSubmission) {
        return
    }

    $iiPaintEngineLicense = Join-Path $Directory "legal\iiPaintEngine\LICENSE.txt"
    if (-not (Test-Path -LiteralPath $iiPaintEngineLicense -PathType Leaf) -or
        (Get-Item -LiteralPath $iiPaintEngineLicense -ErrorAction SilentlyContinue).Length -le 0) {
        throw "A public Store submission requires the copyright holder's explicit iiPaintEngine LICENSE."
    }

    $iiSharedCanvasLicense = Join-Path $Directory "legal\iiSharedCanvas\LICENSE.txt"
    if (-not (Test-Path -LiteralPath $iiSharedCanvasLicense -PathType Leaf) -or
        (Get-Item -LiteralPath $iiSharedCanvasLicense -ErrorAction SilentlyContinue).Length -le 0) {
        throw "A public Store submission requires the iiSharedCanvas LICENSE."
    }

    $sourceOffer = Get-Content -LiteralPath (Join-Path $Directory "SOURCE_OFFER.txt") -Raw
    if ($sourceOffer -match '(?i)local test package|\b(TBD|UNKNOWN|UNLICENSED)\b') {
        throw "A Store submission cannot contain a local-test or unresolved corresponding-source offer."
    }
    $urlMatch = [regex]::Match($sourceOffer, '(?im)^URL:\s*(?<url>\S+)\s*$')
    $hashMatch = [regex]::Match($sourceOffer, '(?im)^SHA-256:\s*(?<hash>[0-9A-F]{64})\s*$')
    $sourceUri = $null
    if ((-not $urlMatch.Success) -or
        (-not [System.Uri]::TryCreate($urlMatch.Groups["url"].Value, [System.UriKind]::Absolute, [ref]$sourceUri)) -or
        $sourceUri.Scheme -ne "https") {
        throw "A Store submission requires an absolute HTTPS corresponding-source URL."
    }
    if (-not $hashMatch.Success) {
        throw "A Store submission requires an exact 64-hex corresponding-source SHA-256."
    }
}

function Write-StoreUploadArchive {
    param(
        [string]$PackagePath,
        [string]$OutputPath,
        [string]$EntryName = ""
    )

    if (-not (Test-Path -LiteralPath $PackagePath -PathType Leaf)) {
        throw "MSIX package is missing: $PackagePath"
    }
    if ([string]::IsNullOrWhiteSpace($EntryName)) {
        $EntryName = [System.IO.Path]::GetFileName($PackagePath)
    }

    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $outputDirectory = Split-Path -Parent $OutputPath
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    Remove-Item -LiteralPath $OutputPath -Force -ErrorAction SilentlyContinue

    $outputStream = [System.IO.File]::Open($OutputPath, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::ReadWrite)
    try {
        $archive = New-Object System.IO.Compression.ZipArchive(
            $outputStream,
            [System.IO.Compression.ZipArchiveMode]::Create,
            $false
        )
        try {
            $entry = $archive.CreateEntry($EntryName, [System.IO.Compression.CompressionLevel]::Optimal)
            $entryStream = $entry.Open()
            try {
                $packageStream = [System.IO.File]::OpenRead($PackagePath)
                try {
                    $packageStream.CopyTo($entryStream)
                } finally {
                    $packageStream.Dispose()
                }
            } finally {
                $entryStream.Dispose()
            }
        } finally {
            $archive.Dispose()
        }
    } finally {
        $outputStream.Dispose()
    }
}

function Resolve-WindowsSdkTool {
    param(
        [string]$ToolName,
        [string]$ConfiguredPath
    )

    if (-not [string]::IsNullOrWhiteSpace($ConfiguredPath)) {
        $resolved = [System.IO.Path]::GetFullPath($ConfiguredPath)
        if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
            throw "Configured Windows SDK tool does not exist: $resolved"
        }
        return $resolved
    }

    $kitsRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"
    $candidates = @(Get-ChildItem -LiteralPath $kitsRoot -Directory -ErrorAction SilentlyContinue |
        ForEach-Object {
            $candidate = Join-Path $_.FullName "x64\$ToolName"
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                [pscustomobject]@{
                    Path = $candidate
                    Version = try { [version]$_.Name } catch { [version]"0.0" }
                }
            }
        } |
        Sort-Object Version -Descending)
    if ($candidates.Count -eq 0) {
        throw "The x64 Windows SDK tool '$ToolName' is not installed."
    }
    return $candidates[0].Path
}

function Add-ConfiguredBuildToolsToPath {
    param([string]$BuildDirectory)

    $toolDirectories = @()
    $cachePath = Join-Path $BuildDirectory "CMakeCache.txt"
    if (Test-Path -LiteralPath $cachePath -PathType Leaf) {
        $cache = Get-Content -LiteralPath $cachePath
        foreach ($pattern in @('^CMAKE_COMMAND:INTERNAL=(?<path>.+)$', '^CMAKE_MAKE_PROGRAM:FILEPATH=(?<path>.+)$')) {
            $match = $cache | Select-String -Pattern $pattern | Select-Object -First 1
            if ($match -and $match.Matches[0].Groups["path"].Value) {
                $toolPath = $match.Matches[0].Groups["path"].Value
                if (Test-Path -LiteralPath $toolPath -PathType Leaf) {
                    $toolDirectories += Split-Path -Parent $toolPath
                }
            }
        }
    }

    $clionRoot = Join-Path $env:LOCALAPPDATA "Programs\CLion\bin"
    foreach ($candidate in @(
            (Join-Path $clionRoot "cmake\win\x64\bin"),
            (Join-Path $clionRoot "ninja\win\x64")
        )) {
        if (Test-Path -LiteralPath $candidate -PathType Container) {
            $toolDirectories += $candidate
        }
    }

    foreach ($directory in @($toolDirectories | Select-Object -Unique)) {
        $env:Path = "$directory;$env:Path"
    }
}

function New-StorePngAsset {
    param(
        [string]$SourcePath,
        [string]$OutputPath,
        [int]$Size
    )

    Add-Type -AssemblyName System.Drawing
    $source = [System.Drawing.Image]::FromFile($SourcePath)
    try {
        $bitmap = New-Object System.Drawing.Bitmap(
            $Size,
            $Size,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
        )
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.Clear([System.Drawing.Color]::Transparent)
                $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
                $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
                $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $graphics.DrawImage($source, 0, 0, $Size, $Size)
            } finally {
                $graphics.Dispose()
            }
            $outputDirectory = Split-Path -Parent $OutputPath
            New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
            $bitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
        } finally {
            $bitmap.Dispose()
        }
    } finally {
        $source.Dispose()
    }
}

function Assert-PngAssetDimensions {
    param(
        [string]$Path,
        [int]$ExpectedSize
    )

    Add-Type -AssemblyName System.Drawing
    try {
        $image = [System.Drawing.Image]::FromFile($Path)
        try {
            if ($image.Width -ne $ExpectedSize -or $image.Height -ne $ExpectedSize) {
                throw "MSIX asset has the wrong dimensions: $Path"
            }
        } finally {
            $image.Dispose()
        }
    } catch {
        throw "MSIX asset is not a valid ${ExpectedSize}x${ExpectedSize} PNG: $Path. $($_.Exception.Message)"
    }
}

function Write-StoreCorrespondingSourceOffer {
    param(
        [string]$OutputPath,
        [string]$SourceUrl,
        [string]$SourceSha256,
        [string]$Version
    )

    $sourceUri = $null
    if ((-not [System.Uri]::TryCreate($SourceUrl, [System.UriKind]::Absolute, [ref]$sourceUri)) -or
        $sourceUri.Scheme -ne "https") {
        throw "Store packaging requires CONGREGATION_CORRESPONDING_SOURCE_URL as an absolute HTTPS URL."
    }
    $normalizedHash = (($SourceSha256 | Out-String).Trim() -replace '\s', '').ToUpperInvariant()
    if ($normalizedHash -notmatch '^[0-9A-F]{64}$') {
        throw "Store packaging requires CONGREGATION_CORRESPONDING_SOURCE_SHA256 as an exact 64-hex value."
    }

    $offer = @"
Congregation $Version corresponding source
========================================

The corresponding source for this exact release, including the sources needed
to comply with the GNU AGPL and LGPL components conveyed with Congregation, is
available without charge from the publisher-controlled location below.

URL: $SourceUrl
SHA-256: $normalizedHash
"@
    [System.IO.File]::WriteAllText(
        $OutputPath,
        $offer.Trim() + "`r`n",
        (New-Object System.Text.UTF8Encoding($false))
    )
}

function Resolve-DevelopmentCertificate {
    param(
        [string]$Thumbprint,
        [string]$ExpectedPublisher,
        [bool]$RequireMachineTrust
    )

    $normalizedThumbprint = (($Thumbprint | Out-String).Trim() -replace '\s', '').ToUpperInvariant()
    $certificate = Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert |
        Where-Object { ($_.Thumbprint -replace '\s', '').ToUpperInvariant() -ceq $normalizedThumbprint } |
        Select-Object -First 1
    if (-not $certificate) {
        throw "The development Code Signing certificate was not found in CurrentUser\\My."
    }
    if (-not $certificate.HasPrivateKey) {
        throw "The development certificate does not have an accessible private key."
    }
    if ($certificate.NotBefore -gt [DateTime]::Now -or $certificate.NotAfter -le [DateTime]::Now) {
        throw "The development certificate is not currently valid."
    }
    if ($certificate.Subject -cne $ExpectedPublisher) {
        throw "The development certificate Subject must exactly match the manifest Publisher. Expected '$ExpectedPublisher', got '$($certificate.Subject)'."
    }
    if ($RequireMachineTrust -and
        (-not (Test-Path -LiteralPath "Cert:\LocalMachine\TrustedPeople\$normalizedThumbprint"))) {
        throw "The development certificate must be imported into LocalMachine\\TrustedPeople from an elevated PowerShell session before MSIX installation testing."
    }
    return $certificate
}

function Write-Sha256File {
    param(
        [string]$FilePath,
        [string]$OutputPath,
        [string]$RecordedFileName
    )

    $hash = (Get-FileHash -LiteralPath $FilePath -Algorithm SHA256).Hash.ToUpperInvariant()
    [System.IO.File]::WriteAllText(
        $OutputPath,
        "$hash *$RecordedFileName`r`n",
        (New-Object System.Text.UTF8Encoding($false))
    )
}

function Test-MsixArchiveEntry {
    param(
        [string]$PackagePath,
        [string]$EntryName
    )

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [System.IO.Compression.ZipFile]::OpenRead($PackagePath)
    try {
        return $null -ne ($archive.Entries | Where-Object { $_.FullName -ceq $EntryName } | Select-Object -First 1)
    } finally {
        $archive.Dispose()
    }
}

function Install-AndVerifyDevelopmentMsix {
    param(
        [string]$PackagePath,
        [string]$IdentityName
    )

    $existingPackages = @(Get-AppxPackage | Where-Object { $_.Name -ceq $IdentityName })
    foreach ($existingPackage in $existingPackages) {
        Remove-AppxPackage -Package $existingPackage.PackageFullName -ErrorAction Stop
    }

    $beforeProcessIds = @(Get-Process -Name "Congregation" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id)
    $newProcessIds = @()
    $installedPackage = $null
    try {
        Add-AppxPackage -Path $PackagePath -ErrorAction Stop
        $installedPackage = Get-AppxPackage | Where-Object { $_.Name -ceq $IdentityName } | Select-Object -First 1
        if (-not $installedPackage) {
            throw "Add-AppxPackage completed but the Congregation development package is not registered."
        }
        foreach ($relativePath in @("Congregation.exe", "LICENSE.txt", "THIRD_PARTY_NOTICES.txt", "SOURCE_OFFER.txt")) {
            if (-not (Test-Path -LiteralPath (Join-Path $installedPackage.InstallLocation $relativePath) -PathType Leaf)) {
                throw "The installed MSIX is missing $relativePath."
            }
        }

        Start-Process "explorer.exe" -ArgumentList "shell:AppsFolder\$($installedPackage.PackageFamilyName)!Congregation"
        $deadline = [DateTime]::UtcNow.AddSeconds(45)
        $visibleProcess = $null
        while ([DateTime]::UtcNow -lt $deadline) {
            Start-Sleep -Milliseconds 250
            $candidateProcesses = @(Get-Process -Name "Congregation" -ErrorAction SilentlyContinue |
                Where-Object { $_.Id -notin $beforeProcessIds })
            $newProcessIds = @($candidateProcesses | Select-Object -ExpandProperty Id)
            foreach ($candidate in $candidateProcesses) {
                $candidate.Refresh()
                if ($candidate.MainWindowHandle -ne [IntPtr]::Zero) {
                    $visibleProcess = $candidate
                    break
                }
            }
            if ($visibleProcess) {
                break
            }
        }
        if (-not $visibleProcess) {
            throw "The installed Congregation MSIX did not create a visible application window within 45 seconds."
        }
        Write-Host "Visible packaged Congregation window: PID $($visibleProcess.Id), HWND $($visibleProcess.MainWindowHandle)"
    } finally {
        foreach ($processId in $newProcessIds) {
            Stop-Process -Id $processId -Force -ErrorAction SilentlyContinue
        }
        if ($installedPackage) {
            Remove-AppxPackage -Package $installedPackage.PackageFullName -ErrorAction SilentlyContinue
        }
    }

    if (Get-AppxPackage | Where-Object { $_.Name -ceq $IdentityName } | Select-Object -First 1) {
        throw "The Congregation development package remained registered after the verification cleanup."
    }
}

$isWindowsRuntime = if (Get-Variable IsWindows -ErrorAction SilentlyContinue) {
    $IsWindows
} else {
    $env:OS -eq "Windows_NT"
}
if (-not $isWindowsRuntime) {
    throw "build-windows-store.ps1 must run on Windows."
}

if ($Mode -eq "Development") {
    if ([string]::IsNullOrWhiteSpace($IdentityName)) {
        $IdentityName = "IISACC.Congregation.Development"
    }
    if ([string]::IsNullOrWhiteSpace($Publisher)) {
        $Publisher = "CN=Congregation Development Local Only"
    }
    if ([string]::IsNullOrWhiteSpace($DisplayName)) {
        $DisplayName = "Congregation Development"
    }
    if ([string]::IsNullOrWhiteSpace($PublisherDisplayName)) {
        $PublisherDisplayName = "IISACC Development"
    }
} else {
    if ($SkipBuild) {
        throw "Store mode does not allow -SkipBuild; public output requires a current Release build and complete tests."
    }
    if ($InstallDevelopment) {
        throw "-InstallDevelopment can only be used with -Mode Development."
    }
}

Assert-StorePackagePolicy `
    -Mode $Mode `
    -IdentityName $IdentityName `
    -Publisher $Publisher `
    -DisplayName $DisplayName `
    -PublisherDisplayName $PublisherDisplayName `
    -PackageVersion $PackageVersion `
    -DevelopmentCertificateThumbprint $DevelopmentCertificateThumbprint

$makeAppx = Resolve-WindowsSdkTool -ToolName "makeappx.exe" -ConfiguredPath $MakeAppxPath
$signTool = ""
$developmentCertificate = $null
if ($Mode -eq "Development") {
    $signTool = Resolve-WindowsSdkTool -ToolName "signtool.exe" -ConfiguredPath $SignToolPath
    $developmentCertificate = Resolve-DevelopmentCertificate `
        -Thumbprint $DevelopmentCertificateThumbprint `
        -ExpectedPublisher $Publisher `
        -RequireMachineTrust ([bool]$InstallDevelopment)
}

if (-not $SkipBuild) {
    Write-Step "Building and testing the Windows runtime stage"
    Add-ConfiguredBuildToolsToPath -BuildDirectory $BuildDirectory
    $windowsPowerShell = Join-Path $env:SystemRoot "System32\WindowsPowerShell\v1.0\powershell.exe"
    Invoke-Native $windowsPowerShell @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", (Join-Path $RepositoryRoot "build-windows.ps1"),
        "-BuildType", $BuildType,
        "-AllowUnsignedPackage"
    )
}

if (-not (Test-Path -LiteralPath (Join-Path $StageDirectory "Congregation.exe") -PathType Leaf)) {
    throw "The staged Windows runtime is missing. Run without -SkipBuild first."
}

Write-Step "Preparing MSIX package content"
Remove-Item -LiteralPath $WorkDirectory -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $ContentDirectory -Force | Out-Null
Copy-Item -Path (Join-Path $StageDirectory "*") -Destination $ContentDirectory -Recurse -Force

Write-StoreAppxManifest `
    -TemplatePath $ManifestTemplatePath `
    -OutputPath (Join-Path $ContentDirectory "AppxManifest.xml") `
    -IdentityName $IdentityName `
    -Publisher $Publisher `
    -DisplayName $DisplayName `
    -PublisherDisplayName $PublisherDisplayName `
    -PackageVersion $PackageVersion

if (-not (Test-Path -LiteralPath $IconSourcePath -PathType Leaf)) {
    throw "The 1024x1024 Congregation icon source is missing: $IconSourcePath"
}
$assetDefinitions = @(
    [pscustomobject]@{ Name = "StoreLogo.png"; Size = 50 },
    [pscustomobject]@{ Name = "Square44x44Logo.png"; Size = 44 },
    [pscustomobject]@{ Name = "Square150x150Logo.png"; Size = 150 }
)
foreach ($asset in $assetDefinitions) {
    $assetPath = Join-Path $ContentDirectory "Assets\$($asset.Name)"
    New-StorePngAsset -SourcePath $IconSourcePath -OutputPath $assetPath -Size $asset.Size
    Assert-PngAssetDimensions -Path $assetPath -ExpectedSize $asset.Size
}

if ($Mode -eq "Store") {
    Write-StoreCorrespondingSourceOffer `
        -OutputPath (Join-Path $ContentDirectory "SOURCE_OFFER.txt") `
        -SourceUrl $CorrespondingSourceUrl `
        -SourceSha256 $CorrespondingSourceSha256 `
        -Version $Version
}
Assert-StorePackageContent -Directory $ContentDirectory -StoreSubmission ($Mode -eq "Store")

$developmentOutputDirectory = Join-Path $BuildDirectory "development-only"
$storeBaseName = "Congregation-$Version-Windows-Store-x64"
$developmentBaseName = "Congregation-$Version-Windows-Sideload-Development-x64"
$outputDirectory = if ($Mode -eq "Store") { $DistDirectory } else { $developmentOutputDirectory }
$baseName = if ($Mode -eq "Store") { $storeBaseName } else { $developmentBaseName }
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
if ($Mode -eq "Development") {
    $publicCertificatePath = Join-Path $outputDirectory "Congregation-Development-Local-Only.cer"
    Export-Certificate -Cert $developmentCertificate -FilePath $publicCertificatePath -Force | Out-Null
    Write-Host "Development public certificate: $publicCertificatePath"
}

$finalPackagePath = Join-Path $outputDirectory "$baseName.msix"
$partialPackagePath = Join-Path $WorkDirectory "$baseName.partial.msix"
$finalChecksumPath = "$finalPackagePath.sha256"
$partialChecksumPath = "$partialPackagePath.sha256"
$finalUploadPath = if ($Mode -eq "Store") { Join-Path $outputDirectory "$baseName.msixupload" } else { "" }
$partialUploadPath = if ($Mode -eq "Store") { Join-Path $WorkDirectory "$baseName.partial.msixupload" } else { "" }
$finalUploadChecksumPath = if ($Mode -eq "Store") { "$finalUploadPath.sha256" } else { "" }
$partialUploadChecksumPath = if ($Mode -eq "Store") { "$partialUploadPath.sha256" } else { "" }

Write-Step "Packing MSIX"
Invoke-Native $makeAppx @(
    "pack",
    "/o",
    "/h", "SHA256",
    "/d", $ContentDirectory,
    "/p", $partialPackagePath
)
if (Test-MsixArchiveEntry -PackagePath $partialPackagePath -EntryName "AppxSignature.p7x") {
    throw "MakeAppx unexpectedly produced a signed package before the selected signing step."
}

if ($Mode -eq "Development") {
    Write-Step "Signing the local-development MSIX"
    Invoke-Native $signTool @(
        "sign",
        "/fd", "SHA256",
        "/sha1", $developmentCertificate.Thumbprint,
        "/s", "My",
        "/tr", $TimestampUrl,
        "/td", "SHA256",
        $partialPackagePath
    )
    Invoke-Native $signTool @("verify", "/pa", "/all", "/v", $partialPackagePath)
    if (-not (Test-MsixArchiveEntry -PackagePath $partialPackagePath -EntryName "AppxSignature.p7x")) {
        throw "The development MSIX has no AppxSignature.p7x after SignTool completed."
    }
} else {
    Write-Step "Creating the unsigned Store upload archive"
    Write-StoreUploadArchive `
        -PackagePath $partialPackagePath `
        -OutputPath $partialUploadPath `
        -EntryName ([System.IO.Path]::GetFileName($finalPackagePath))
    Write-Sha256File `
        -FilePath $partialUploadPath `
        -OutputPath $partialUploadChecksumPath `
        -RecordedFileName ([System.IO.Path]::GetFileName($finalUploadPath))
}

Write-Sha256File `
    -FilePath $partialPackagePath `
    -OutputPath $partialChecksumPath `
    -RecordedFileName ([System.IO.Path]::GetFileName($finalPackagePath))

Move-Item -LiteralPath $partialPackagePath -Destination $finalPackagePath -Force
Move-Item -LiteralPath $partialChecksumPath -Destination $finalChecksumPath -Force
if ($Mode -eq "Store") {
    Move-Item -LiteralPath $partialUploadPath -Destination $finalUploadPath -Force
    Move-Item -LiteralPath $partialUploadChecksumPath -Destination $finalUploadChecksumPath -Force
    Write-Host "Store MSIX (intentionally unsigned): $finalPackagePath"
    Write-Host "Store upload: $finalUploadPath"
} else {
    Write-Host "Local-development signed MSIX: $finalPackagePath"
    Write-Warning "This self-signed package is for this trusted development PC only and must not be distributed publicly."
    if ($InstallDevelopment) {
        Write-Step "Installing, launching, and removing the development MSIX"
        Install-AndVerifyDevelopmentMsix -PackagePath $finalPackagePath -IdentityName $IdentityName
        Write-Host "Development MSIX install/visible-window/removal verification passed."
    }
}
