#Requires -Version 5.1

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-CongregationNecessaryReleaseVersion {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildDirectory
    )

    $cachePath = Join-Path $BuildDirectory "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) {
        throw "The configured CMake cache is missing: $cachePath"
    }

    $versionEntries = @(
        Get-Content -LiteralPath $cachePath |
            Where-Object { $_ -match '^CMAKE_PROJECT_VERSION:STATIC=' }
    )
    if ($versionEntries.Count -ne 1) {
        throw "CMAKE_PROJECT_VERSION must occur exactly once in the configured CMake cache."
    }

    $version = ($versionEntries[0] -split '=', 2)[1]
    if ($version -notmatch '^\d+\.\d+(?:\.\d+)?$') {
        throw "The configured Congregation release version is invalid: $version"
    }
    return $version
}

function Assert-CongregationNecessaryChildPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root,
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $resolvedRoot = [System.IO.Path]::GetFullPath($Root).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar
    )
    $resolvedPath = [System.IO.Path]::GetFullPath($Path)
    $rootPrefix = $resolvedRoot + [System.IO.Path]::DirectorySeparatorChar
    if (-not $resolvedPath.StartsWith(
        $rootPrefix,
        [System.StringComparison]::OrdinalIgnoreCase
    )) {
        throw "Refusing to use a release working path outside the build directory: $resolvedPath"
    }
    return $resolvedPath
}

function Get-CongregationNecessaryFileSha256 {
    param(
        [Parameter(Mandatory = $true)]
        [string]$File
    )

    if ($null -eq (Get-Command Get-FileHash -CommandType Cmdlet -ErrorAction SilentlyContinue)) {
        $utilityModuleManifest = Join-Path `
            $PSHOME `
            "Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1"
        if (-not (Test-Path -LiteralPath $utilityModuleManifest -PathType Leaf)) {
            throw "The host PowerShell Utility module is missing: $utilityModuleManifest"
        }
        Import-Module $utilityModuleManifest -Force -ErrorAction Stop
    }

    return (Get-FileHash -LiteralPath $File -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Invoke-CongregationNecessaryReleaseNative {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    Write-Host "> $FilePath $($Arguments -join ' ')"
    $nativeOutput = & $FilePath @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    foreach ($line in @($nativeOutput)) {
        Write-Host $line
    }
    if ($exitCode -ne 0) {
        throw "Necessary release command failed with exit code ${exitCode}: $FilePath"
    }
}

function Invoke-CongregationNecessaryMsiDatabaseContract {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepositoryRoot,
        [Parameter(Mandatory = $true)]
        [string]$BuildDirectory,
        [Parameter(Mandatory = $true)]
        [string]$Version,
        [Parameter(Mandatory = $true)]
        [string]$MsiPath
    )

    $contractPath = Join-Path $RepositoryRoot "tests\tst_windowsmsidatabasecontract.ps1"
    if (-not (Test-Path -LiteralPath $contractPath -PathType Leaf)) {
        throw "The MSI database contract test is missing: $contractPath"
    }
    $windowsPowerShell = Get-Command powershell.exe -CommandType Application -ErrorAction Stop |
        Select-Object -First 1
    Invoke-CongregationNecessaryReleaseNative $windowsPowerShell.Source @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $contractPath,
        "-BuildDirectory", $BuildDirectory,
        "-Version", $Version,
        "-MsiPath", $MsiPath
    )
}

function Invoke-CongregationNecessaryAdministrativeExtraction {
    param(
        [Parameter(Mandatory = $true)]
        [string]$MsiPath,
        [Parameter(Mandatory = $true)]
        [string]$OutputDirectory
    )

    $msiExecPath = Join-Path $env:SystemRoot "System32\msiexec.exe"
    if (-not (Test-Path -LiteralPath $msiExecPath -PathType Leaf)) {
        throw "Windows Installer was not found: $msiExecPath"
    }

    New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
    $arguments = @(
        "/a",
        "`"$MsiPath`"",
        "/qn",
        "TARGETDIR=`"$OutputDirectory`""
    )
    $process = Start-Process `
        -FilePath $msiExecPath `
        -ArgumentList $arguments `
        -Wait `
        -PassThru `
        -WindowStyle Hidden
    if ($process.ExitCode -ne 0) {
        throw "Administrative extraction failed with exit code $($process.ExitCode)."
    }
}

function Publish-CongregationNecessaryReleaseFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CandidatePath,
        [Parameter(Mandatory = $true)]
        [string]$FinalPath
    )

    if (-not (Test-Path -LiteralPath $CandidatePath -PathType Leaf)) {
        throw "The verified release candidate is missing: $CandidatePath"
    }

    $resolvedCandidate = [System.IO.Path]::GetFullPath($CandidatePath)
    $resolvedFinal = [System.IO.Path]::GetFullPath($FinalPath)
    $candidateDirectory = [System.IO.Path]::GetDirectoryName($resolvedCandidate)
    $finalDirectory = [System.IO.Path]::GetDirectoryName($resolvedFinal)
    if (-not $candidateDirectory.Equals(
        $finalDirectory,
        [System.StringComparison]::OrdinalIgnoreCase
    )) {
        throw "Atomic Necessary publication requires candidate and final files in one directory."
    }

    if (Test-Path -LiteralPath $resolvedFinal -PathType Leaf) {
        $backupPath = "$resolvedFinal.previous-" + [Guid]::NewGuid().ToString("N")
        [System.IO.File]::Replace($resolvedCandidate, $resolvedFinal, $backupPath, $true)
        if (Test-Path -LiteralPath $backupPath -PathType Leaf) {
            Remove-Item -LiteralPath $backupPath -Force
        }
    } else {
        [System.IO.File]::Move($resolvedCandidate, $resolvedFinal)
    }

    if (-not (Test-Path -LiteralPath $resolvedFinal -PathType Leaf)) {
        throw "Atomic Necessary publication did not produce the final file: $resolvedFinal"
    }
}

function Invoke-CongregationNecessaryWindowsRelease {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepositoryRoot,
        [Parameter(Mandatory = $true)]
        [string]$BuildDirectory,
        [Parameter(Mandatory = $true)]
        [string]$StageDirectory,
        [Parameter(Mandatory = $true)]
        [string]$WixToolsDirectory,
        [Parameter(Mandatory = $true)]
        [string]$OsslSignCodePath,
        [Parameter(Mandatory = $true)]
        [string]$SignToolPath,
        [Parameter(Mandatory = $true)]
        [string]$SigningToken,
        [Parameter(Mandatory = $true)]
        [string]$TimestampUrl,
        [string]$ExpectedPublisher = "Necessary Innovations AB"
    )

    if ($null -eq (Get-Command Invoke-NecessaryAuthenticodeSigning `
            -CommandType Function `
            -ErrorAction SilentlyContinue)) {
        throw "Dot-source tools\necessary-authenticode.ps1 before invoking the release."
    }
    if ([string]::IsNullOrWhiteSpace($SigningToken)) {
        throw "NECESSARY_SIGN_TOKEN is required for public release signing."
    }
    if ([string]::IsNullOrWhiteSpace($ExpectedPublisher)) {
        throw "An expected Necessary Publisher identity is required."
    }

    $resolvedRepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot).Path
    $resolvedBuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
    $expectedBuildDirectory = [System.IO.Path]::GetFullPath(
        (Join-Path $resolvedRepositoryRoot "build")
    )
    if (-not $resolvedBuildDirectory.Equals(
        $expectedBuildDirectory,
        [System.StringComparison]::OrdinalIgnoreCase
    )) {
        throw "Necessary releases must use the repository-local build directory."
    }

    $resolvedStageDirectory = (Resolve-Path -LiteralPath $StageDirectory).Path
    $expectedStageDirectory = [System.IO.Path]::GetFullPath(
        (Join-Path $resolvedRepositoryRoot "dist\Congregation-Windows")
    )
    if (-not $resolvedStageDirectory.Equals(
        $expectedStageDirectory,
        [System.StringComparison]::OrdinalIgnoreCase
    )) {
        throw "The Necessary release stage must be dist\Congregation-Windows."
    }

    $resolvedWixToolsDirectory = (Resolve-Path -LiteralPath $WixToolsDirectory).Path
    $resolvedOsslSignCode = (Resolve-Path -LiteralPath $OsslSignCodePath).Path
    $resolvedSignTool = (Resolve-Path -LiteralPath $SignToolPath).Path
    if ([System.IO.Path]::GetFileName($resolvedOsslSignCode) -ine "osslsigncode.exe") {
        throw "The Necessary release must use the pinned osslsigncode.exe."
    }
    if ([System.IO.Path]::GetFileName($resolvedSignTool) -ine "signtool.exe") {
        throw "The Necessary release must use Windows SDK signtool.exe."
    }

    $candlePath = Join-Path $resolvedWixToolsDirectory "candle.exe"
    $lightPath = Join-Path $resolvedWixToolsDirectory "light.exe"
    $smokePath = Join-Path $resolvedWixToolsDirectory "smoke.exe"
    $daricePath = Join-Path $resolvedWixToolsDirectory "darice.cub"
    foreach ($path in @($candlePath, $lightPath, $smokePath, $daricePath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "A required WiX 3.14 release tool is missing: $path"
        }
    }

    $version = Get-CongregationNecessaryReleaseVersion -BuildDirectory $resolvedBuildDirectory
    $msiWorkDirectory = Join-Path $resolvedBuildDirectory "msi"
    $productPath = Join-Path $msiWorkDirectory "CongregationProduct.wxs"
    $runtimePath = Join-Path $msiWorkDirectory "CongregationRuntime.wxs"
    $licensePath = Join-Path $msiWorkDirectory "CongregationLicense.rtf"
    foreach ($path in @($productPath, $runtimePath, $licensePath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "A verified MSI authoring input is missing: $path"
        }
    }

    $ownedNames = @("Congregation.exe", "LVRS.dll", "libiiPaintEngine.dll")
    $ownedPaths = @{}
    foreach ($ownedName in $ownedNames) {
        $ownedPath = Join-Path $resolvedStageDirectory $ownedName
        if (-not (Test-Path -LiteralPath $ownedPath -PathType Leaf)) {
            throw "A Congregation-owned signing target is missing: $ownedPath"
        }
        $ownedPaths[[System.IO.Path]::GetFullPath($ownedPath).ToUpperInvariant()] = $true
    }

    $stageFiles = @(
        Get-ChildItem -LiteralPath $resolvedStageDirectory -Recurse -File |
            Where-Object { $_.Extension -in @(".exe", ".dll") } |
            Sort-Object -Property FullName
    )
    if ($stageFiles.Count -eq 0) {
        throw "The Necessary release stage contains no PE files."
    }

    $temporaryRoot = Join-Path $resolvedBuildDirectory "necessary-signing-temp"
    New-Item -ItemType Directory -Path $temporaryRoot -Force | Out-Null
    $partialMsi = Assert-CongregationNecessaryChildPath `
        -Root $resolvedBuildDirectory `
        -Path (Join-Path $resolvedBuildDirectory "Congregation-$version-Windows.necessary.partial.msi")
    $candidateMsi = Assert-CongregationNecessaryChildPath `
        -Root $resolvedBuildDirectory `
        -Path (Join-Path $resolvedBuildDirectory "Congregation-$version-Windows.necessary.candidate.msi")
    $finalMsi = Assert-CongregationNecessaryChildPath `
        -Root $resolvedBuildDirectory `
        -Path (Join-Path $resolvedBuildDirectory "Congregation-$version-Windows.msi")
    $partialSidecar = Assert-CongregationNecessaryChildPath `
        -Root $resolvedBuildDirectory `
        -Path "$finalMsi.sha256.partial"
    $administrativeImageDirectory = Assert-CongregationNecessaryChildPath `
        -Root $resolvedBuildDirectory `
        -Path (Join-Path $resolvedBuildDirectory (
            "necessary-administrative-image-" + [Guid]::NewGuid().ToString("N")
        ))

    $releaseCertificateThumbprint = ""
    $finalSigningResult = $null
    try {
        foreach ($file in $stageFiles) {
            $signature = Get-AuthenticodeSignature -LiteralPath $file.FullName
            $pathKey = [System.IO.Path]::GetFullPath($file.FullName).ToUpperInvariant()
            $isCongregationOwned = $ownedPaths.ContainsKey($pathKey)
            if (($signature.Status -eq "Valid") -and (-not $isCongregationOwned)) {
                continue
            }
            if ($signature.Status -notin @("Valid", "NotSigned")) {
                throw "Refusing to replace a non-valid Authenticode signature: $($file.FullName)"
            }

            $signingResult = Invoke-NecessaryAuthenticodeSigning `
                -File $file.FullName `
                -OsslSignCodePath $resolvedOsslSignCode `
                -SignToolPath $resolvedSignTool `
                -SigningToken $SigningToken `
                -TimestampUrl $TimestampUrl `
                -TemporaryRoot $temporaryRoot `
                -ExpectedPublisher $ExpectedPublisher `
                -ExpectedCertificateThumbprint $releaseCertificateThumbprint
            if ([string]::IsNullOrWhiteSpace($releaseCertificateThumbprint)) {
                $releaseCertificateThumbprint = [string]$signingResult.Thumbprint
                if ($releaseCertificateThumbprint -notmatch '^[0-9A-Fa-f]{40}$') {
                    throw "The first Necessary signing result has an invalid certificate thumbprint."
                }
                $releaseCertificateThumbprint = $releaseCertificateThumbprint.ToUpperInvariant()
            }
        }
        if ([string]::IsNullOrWhiteSpace($releaseCertificateThumbprint)) {
            throw "No staged PE was signed by Necessary."
        }

        foreach ($file in $stageFiles) {
            $signature = Get-AuthenticodeSignature -LiteralPath $file.FullName
            if ($signature.Status -ne "Valid") {
                throw "A staged PE failed Windows Authenticode trust validation: $($file.FullName)"
            }
            if (-not $signature.TimeStamperCertificate) {
                throw "A staged PE has no trusted timestamp: $($file.FullName)"
            }

            $pathKey = [System.IO.Path]::GetFullPath($file.FullName).ToUpperInvariant()
            if ($ownedPaths.ContainsKey($pathKey)) {
                if ($signature.SignerCertificate.Subject.IndexOf(
                    $ExpectedPublisher,
                    [System.StringComparison]::OrdinalIgnoreCase
                ) -lt 0) {
                    throw "A Congregation-owned PE has an unexpected Publisher: $($file.FullName)"
                }
                if ($signature.SignerCertificate.Thumbprint.ToUpperInvariant() -cne
                    $releaseCertificateThumbprint) {
                    throw "A Congregation-owned PE was not signed by the release certificate."
                }
            }
            Invoke-CongregationNecessaryReleaseNative $resolvedSignTool @(
                "verify", "/pa", "/all", "/tw", "/v", $file.FullName
            )
        }

        foreach ($path in @(
            (Join-Path $msiWorkDirectory "CongregationProduct.wixobj"),
            (Join-Path $msiWorkDirectory "CongregationRuntime.wixobj"),
            $partialMsi,
            $candidateMsi,
            $partialSidecar
        )) {
            if (Test-Path -LiteralPath $path) {
                Remove-Item -LiteralPath $path -Force
            }
        }

        Invoke-CongregationNecessaryReleaseNative $candlePath @(
            "-wx",
            "-dStageDir=$resolvedStageDirectory",
            "-dSourceDir=$resolvedRepositoryRoot",
            "-dLicenseRtf=$licensePath",
            "-arch", "x64",
            "-out", "$msiWorkDirectory\",
            $productPath,
            $runtimePath
        )
        Invoke-CongregationNecessaryReleaseNative $lightPath @(
            "-wx",
            "-ext", "WixUIExtension",
            "-cultures:en-us",
            "-out", $partialMsi,
            (Join-Path $msiWorkDirectory "CongregationProduct.wixobj"),
            (Join-Path $msiWorkDirectory "CongregationRuntime.wixobj")
        )
        if (-not (Test-Path -LiteralPath $partialMsi -PathType Leaf)) {
            throw "WiX Light did not produce the Necessary MSI candidate."
        }
        Invoke-CongregationNecessaryReleaseNative $smokePath @(
            "-wx", "-nodefault", "-cub", $daricePath, "-ice:ICE105", $partialMsi
        )

        Copy-Item -LiteralPath $partialMsi -Destination $candidateMsi -Force
        $finalSigningResult = Invoke-NecessaryAuthenticodeSigning `
            -File $candidateMsi `
            -OsslSignCodePath $resolvedOsslSignCode `
            -SignToolPath $resolvedSignTool `
            -SigningToken $SigningToken `
            -TimestampUrl $TimestampUrl `
            -TemporaryRoot $temporaryRoot `
            -ExpectedPublisher $ExpectedPublisher `
            -ExpectedCertificateThumbprint $releaseCertificateThumbprint

        $candidateSignature = Get-AuthenticodeSignature -LiteralPath $candidateMsi
        if (($candidateSignature.Status -ne "Valid") -or
            (-not $candidateSignature.TimeStamperCertificate)) {
            throw "The final MSI candidate is not validly signed and timestamped."
        }
        if ($candidateSignature.SignerCertificate.Subject.IndexOf(
            $ExpectedPublisher,
            [System.StringComparison]::OrdinalIgnoreCase
        ) -lt 0) {
            throw "The final MSI candidate has an unexpected Publisher."
        }
        if ($candidateSignature.SignerCertificate.Thumbprint.ToUpperInvariant() -cne
            $releaseCertificateThumbprint) {
            throw "The final MSI candidate was not signed by the release certificate."
        }
        Invoke-CongregationNecessaryReleaseNative $resolvedSignTool @(
            "verify", "/pa", "/all", "/tw", "/v", $candidateMsi
        )

        Invoke-CongregationNecessaryMsiDatabaseContract `
            -RepositoryRoot $resolvedRepositoryRoot `
            -BuildDirectory $resolvedBuildDirectory `
            -Version $version `
            -MsiPath $candidateMsi

        Invoke-CongregationNecessaryAdministrativeExtraction `
            -MsiPath $candidateMsi `
            -OutputDirectory $administrativeImageDirectory
        foreach ($ownedName in $ownedNames) {
            $candidates = @(
                Get-ChildItem `
                    -LiteralPath $administrativeImageDirectory `
                    -Filter $ownedName `
                    -Recurse `
                    -File
            )
            if ($candidates.Count -ne 1) {
                throw "The final MSI must install exactly one $ownedName; found $($candidates.Count)."
            }

            $signature = Get-AuthenticodeSignature -LiteralPath $candidates[0].FullName
            if (($signature.Status -ne "Valid") -or (-not $signature.TimeStamperCertificate)) {
                throw "The installed $ownedName is not validly signed and timestamped."
            }
            if ($signature.SignerCertificate.Subject.IndexOf(
                $ExpectedPublisher,
                [System.StringComparison]::OrdinalIgnoreCase
            ) -lt 0) {
                throw "The installed $ownedName has an unexpected Publisher."
            }
            if ($signature.SignerCertificate.Thumbprint.ToUpperInvariant() -cne
                $releaseCertificateThumbprint) {
                throw "The installed $ownedName was not signed by the release certificate."
            }
            Invoke-CongregationNecessaryReleaseNative $resolvedSignTool @(
                "verify", "/pa", "/all", "/tw", "/v", $candidates[0].FullName
            )
        }

        Publish-CongregationNecessaryReleaseFile `
            -CandidatePath $candidateMsi `
            -FinalPath $finalMsi
        $finalHash = (
            Get-CongregationNecessaryFileSha256 -File $finalMsi
        ).ToLowerInvariant()
        [System.IO.File]::WriteAllText(
            $partialSidecar,
            "$finalHash *$([System.IO.Path]::GetFileName($finalMsi))`r`n",
            [System.Text.Encoding]::ASCII
        )
        Publish-CongregationNecessaryReleaseFile `
            -CandidatePath $partialSidecar `
            -FinalPath "$finalMsi.sha256"

        return [pscustomobject]@{
            File = $finalMsi
            SHA256 = $finalHash.ToUpperInvariant()
            Subject = [string]$finalSigningResult.Subject
            Thumbprint = $releaseCertificateThumbprint
            TimestampSubject = [string]$finalSigningResult.TimestampSubject
            StagePeCount = $stageFiles.Count
        }
    } finally {
        foreach ($path in @($partialMsi, $candidateMsi, $partialSidecar)) {
            if (Test-Path -LiteralPath $path -PathType Leaf) {
                Remove-Item -LiteralPath $path -Force
            }
        }
        if (Test-Path -LiteralPath $administrativeImageDirectory -PathType Container) {
            Remove-Item -LiteralPath $administrativeImageDirectory -Recurse -Force
        }
    }
}
