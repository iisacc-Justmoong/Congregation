#Requires -Version 5.1
param(
    [Parameter(Mandatory = $true)]
    [string]$ScriptPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$utilityModuleManifest = Join-Path $PSHOME "Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1"
Import-Module $utilityModuleManifest -Force -ErrorAction Stop

function Assert-Condition {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Assert-Throws {
    param(
        [scriptblock]$Action,
        [string]$ExpectedText
    )

    try {
        & $Action
    } catch {
        if ($_.Exception.Message.Contains($ExpectedText)) {
            return
        }
        throw "Expected failure containing '$ExpectedText', got: $($_.Exception.Message)"
    }
    throw "Expected failure containing '$ExpectedText', but the action succeeded."
}

$tokens = $null
$parseErrors = $null
$resolvedScriptPath = (Resolve-Path -LiteralPath $ScriptPath).Path
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    $resolvedScriptPath,
    [ref]$tokens,
    [ref]$parseErrors
)
Assert-Condition ($parseErrors.Count -eq 0) "build-windows.ps1 contains a PowerShell parse error."

$functionAsts = $ast.FindAll(
    { param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] },
    $true
)
foreach ($functionAst in $functionAsts) {
    . ([scriptblock]::Create($functionAst.Extent.Text))
}

$thumbprint = "0123456789ABCDEF0123456789ABCDEF01234567"

Assert-Throws {
    Assert-AuthenticodePolicy -SigningRequested $false -UnsignedPackageAllowed $false -ZipPackageSkipped $false -MsiRequested $false -TestsSkipped $false -Configuration Release -CertificateThumbprint "" -Rfc3161TimestampUrl "http://timestamp.digicert.com"
} "Public package creation requires -Sign"
Assert-Throws {
    Assert-AuthenticodePolicy -SigningRequested $true -UnsignedPackageAllowed $true -ZipPackageSkipped $true -MsiRequested $false -TestsSkipped $false -Configuration Release -CertificateThumbprint $thumbprint -Rfc3161TimestampUrl "http://timestamp.digicert.com"
} "cannot be used together"
Assert-Throws {
    Assert-AuthenticodePolicy -SigningRequested $true -UnsignedPackageAllowed $false -ZipPackageSkipped $false -MsiRequested $false -TestsSkipped $true -Configuration Release -CertificateThumbprint $thumbprint -Rfc3161TimestampUrl "http://timestamp.digicert.com"
} "does not allow -SkipTests"
Assert-Throws {
    Assert-AuthenticodePolicy -SigningRequested $true -UnsignedPackageAllowed $false -ZipPackageSkipped $false -MsiRequested $false -TestsSkipped $false -Configuration Debug -CertificateThumbprint $thumbprint -Rfc3161TimestampUrl "http://timestamp.digicert.com"
} "requires Release or MinSizeRel"
Assert-Throws {
    Assert-AuthenticodePolicy -SigningRequested $true -UnsignedPackageAllowed $false -ZipPackageSkipped $false -MsiRequested $false -TestsSkipped $false -Configuration Release -CertificateThumbprint "1234" -Rfc3161TimestampUrl "http://timestamp.digicert.com"
} "exact 40-hex"
Assert-Throws {
    Assert-AuthenticodePolicy -SigningRequested $true -UnsignedPackageAllowed $false -ZipPackageSkipped $false -MsiRequested $false -TestsSkipped $false -Configuration Release -CertificateThumbprint $thumbprint -Rfc3161TimestampUrl "file:///timestamp"
} "RFC 3161 TimestampUrl"
Assert-Throws {
    Assert-AuthenticodePolicy -SigningRequested $false -UnsignedPackageAllowed $false -ExternalSigningRequested $true -ZipPackageSkipped $false -MsiRequested $true -TestsSkipped $false -Configuration Release -CertificateThumbprint "" -Rfc3161TimestampUrl ""
} "MSI-only"
Assert-Throws {
    Assert-AuthenticodePolicy -SigningRequested $false -UnsignedPackageAllowed $false -ExternalSigningRequested $true -ZipPackageSkipped $true -MsiRequested $false -TestsSkipped $false -Configuration Release -CertificateThumbprint "" -Rfc3161TimestampUrl ""
} "requires -CreateMsi"
Assert-Throws {
    Assert-AuthenticodePolicy -SigningRequested $false -UnsignedPackageAllowed $true -ExternalSigningRequested $true -ZipPackageSkipped $true -MsiRequested $true -TestsSkipped $false -Configuration Release -CertificateThumbprint "" -Rfc3161TimestampUrl ""
} "cannot be combined"
Assert-Throws {
    Assert-AuthenticodePolicy -SigningRequested $false -UnsignedPackageAllowed $false -ExternalSigningRequested $true -ZipPackageSkipped $true -MsiRequested $true -TestsSkipped $true -Configuration Release -CertificateThumbprint "" -Rfc3161TimestampUrl ""
} "does not allow -SkipTests"
Assert-Throws {
    Assert-AuthenticodePolicy -SigningRequested $false -UnsignedPackageAllowed $false -ExternalSigningRequested $true -ZipPackageSkipped $true -MsiRequested $true -TestsSkipped $false -Configuration Debug -CertificateThumbprint "" -Rfc3161TimestampUrl ""
} "requires Release or MinSizeRel"

Assert-Throws {
    Assert-AuthenticodePolicy -SigningRequested $false -UnsignedPackageAllowed $true -ZipPackageSkipped $true -MsiRequested $true -TestsSkipped $true -Configuration Release -CertificateThumbprint "" -Rfc3161TimestampUrl ""
} "does not allow -SkipTests"
Assert-Throws {
    Assert-AuthenticodePolicy -SigningRequested $false -UnsignedPackageAllowed $true -ZipPackageSkipped $true -MsiRequested $true -TestsSkipped $false -Configuration Debug -CertificateThumbprint "" -Rfc3161TimestampUrl ""
} "requires Release or MinSizeRel"
Assert-Throws {
    Assert-AuthenticodePolicy -SigningRequested $false -UnsignedPackageAllowed $true -ZipPackageSkipped $true -MsiRequested $true -TestsSkipped $false -Configuration Release -CertificateThumbprint "" -Rfc3161TimestampUrl "" -CurrentTimeUtc ([DateTimeOffset]::Parse("2027-01-01T00:00:00Z"))
} "policy expired"
Assert-AuthenticodePolicy -SigningRequested $false -UnsignedPackageAllowed $true -ZipPackageSkipped $true -MsiRequested $true -TestsSkipped $false -Configuration Release -CertificateThumbprint "" -Rfc3161TimestampUrl "" -CurrentTimeUtc ([DateTimeOffset]::Parse("2026-12-31T23:59:59Z"))
Assert-AuthenticodePolicy -SigningRequested $true -UnsignedPackageAllowed $false -ZipPackageSkipped $false -MsiRequested $true -TestsSkipped $false -Configuration Release -CertificateThumbprint $thumbprint -Rfc3161TimestampUrl "http://timestamp.digicert.com"
Assert-AuthenticodePolicy -SigningRequested $false -UnsignedPackageAllowed $false -ExternalSigningRequested $true -ZipPackageSkipped $true -MsiRequested $true -TestsSkipped $false -Configuration Release -CertificateThumbprint "" -Rfc3161TimestampUrl ""

Assert-PublicDistributionEvidence -PublicRelease $false -IiPaintEngineLicenseFile "" -IiSharedCanvasLicenseFile "" -IiLicenseManagerNoticeFile "" -SourceUrl "" -SourceSha256 ""
Assert-Throws {
    Assert-PublicDistributionEvidence -PublicRelease $true -IiPaintEngineLicenseFile "" -IiSharedCanvasLicenseFile "" -IiLicenseManagerNoticeFile "" -SourceUrl "https://example.invalid/source.zip" -SourceSha256 ("A" * 64)
} "explicit iiPaintEngine LICENSE"
$temporaryLicenseFile = Join-Path ([System.IO.Path]::GetTempPath()) ("Congregation-LicenseTest-" + [Guid]::NewGuid().ToString("N") + ".txt")
try {
    [System.IO.File]::WriteAllText($temporaryLicenseFile, "test license", [System.Text.Encoding]::UTF8)
    Assert-Throws {
        Assert-PublicDistributionEvidence -PublicRelease $true -IiPaintEngineLicenseFile $temporaryLicenseFile -IiSharedCanvasLicenseFile "" -IiLicenseManagerNoticeFile "" -SourceUrl "https://example.invalid/source.zip" -SourceSha256 ("A" * 64)
    } "iiSharedCanvas LICENSE"
    Assert-Throws {
        Assert-PublicDistributionEvidence -PublicRelease $true -IiPaintEngineLicenseFile $temporaryLicenseFile -IiSharedCanvasLicenseFile $temporaryLicenseFile -IiLicenseManagerNoticeFile "" -SourceUrl "https://example.invalid/source.zip" -SourceSha256 ("A" * 64)
    } "iiLicenseManager third-party notices"
    Assert-Throws {
        Assert-PublicDistributionEvidence -PublicRelease $true -IiPaintEngineLicenseFile $temporaryLicenseFile -IiSharedCanvasLicenseFile $temporaryLicenseFile -IiLicenseManagerNoticeFile $temporaryLicenseFile -SourceUrl "http://example.invalid/source.zip" -SourceSha256 ("A" * 64)
    } "absolute HTTPS URL"
    Assert-Throws {
        Assert-PublicDistributionEvidence -PublicRelease $true -IiPaintEngineLicenseFile $temporaryLicenseFile -IiSharedCanvasLicenseFile $temporaryLicenseFile -IiLicenseManagerNoticeFile $temporaryLicenseFile -SourceUrl "https://example.invalid/source.zip" -SourceSha256 "1234"
    } "exact 64-hex"
    Assert-Throws {
        Assert-PublicDistributionEvidence -PublicRelease $true -IiPaintEngineLicenseFile $temporaryLicenseFile -IiSharedCanvasLicenseFile $temporaryLicenseFile -IiLicenseManagerNoticeFile $temporaryLicenseFile -SourceUrl "https://example.invalid/source.zip" -SourceSha256 (("A" * 32) + " " + ("A" * 32))
    } "exact 64-hex"
    Assert-PublicDistributionEvidence -PublicRelease $true -IiPaintEngineLicenseFile $temporaryLicenseFile -IiSharedCanvasLicenseFile $temporaryLicenseFile -IiLicenseManagerNoticeFile $temporaryLicenseFile -SourceUrl "https://example.invalid/source.zip" -SourceSha256 ("A" * 64)
} finally {
    Remove-Item -LiteralPath $temporaryLicenseFile -Force -ErrorAction SilentlyContinue
}

$digitalSignatureKeyUsage = [pscustomobject]@{
    Oid = [pscustomobject]@{ Value = "2.5.29.15" }
    KeyUsages = [System.Security.Cryptography.X509Certificates.X509KeyUsageFlags]::DigitalSignature
}
$keyEnciphermentOnly = [pscustomobject]@{
    Oid = [pscustomobject]@{ Value = "2.5.29.15" }
    KeyUsages = [System.Security.Cryptography.X509Certificates.X509KeyUsageFlags]::KeyEncipherment
}
Assert-CodeSigningCertificateKeyUsage -Certificate ([pscustomobject]@{ Extensions = @() })
Assert-CodeSigningCertificateKeyUsage -Certificate ([pscustomobject]@{ Extensions = @($digitalSignatureKeyUsage) })
Assert-Throws {
    Assert-CodeSigningCertificateKeyUsage -Certificate ([pscustomobject]@{ Extensions = @($keyEnciphermentOnly) })
} "DigitalSignature"

$stringCodeSigningEku = [pscustomobject]@{ ObjectId = "1.3.6.1.5.5.7.3.3" }
$objectCodeSigningEku = [pscustomobject]@{ ObjectId = [pscustomobject]@{ Value = "1.3.6.1.5.5.7.3.3" } }
Assert-Condition `
    ((Get-EnhancedKeyUsageObjectIdentifier -Usage $stringCodeSigningEku) -eq "1.3.6.1.5.5.7.3.3") `
    "String-valued certificate EKU identifiers must be supported."
Assert-Condition `
    ((Get-EnhancedKeyUsageObjectIdentifier -Usage $objectCodeSigningEku) -eq "1.3.6.1.5.5.7.3.3") `
    "Object-valued certificate EKU identifiers must be supported."

$publicLeafCertificate = [pscustomobject]@{
    Subject = "CN=Congregation Publisher"
    Issuer = "CN=Example Public Code Signing CA"
    Thumbprint = "1111111111111111111111111111111111111111"
}
$publicIntermediateCertificate = [pscustomobject]@{
    Subject = "CN=Example Public Code Signing CA"
    Issuer = "CN=Example Public Root CA"
    Thumbprint = "2222222222222222222222222222222222222222"
}
$publicRootCertificate = [pscustomobject]@{
    Subject = "CN=Example Public Root CA"
    Issuer = "CN=Example Public Root CA"
    Thumbprint = "3333333333333333333333333333333333333333"
}
$selfSignedCertificate = [pscustomobject]@{
    Subject = "CN=Congregation Development Local Only"
    Issuer = "CN=Congregation Development Local Only"
    Thumbprint = "4444444444444444444444444444444444444444"
}
$selfIssuedPublicLeafCertificate = [pscustomobject]@{
    Subject = "CN=Public CA Rollover"
    Issuer = "CN=Public CA Rollover"
    Thumbprint = "5555555555555555555555555555555555555555"
}

Assert-Throws {
    Assert-PublicCodeSigningChainEvidence `
        -Certificate $selfSignedCertificate `
        -ChainBuildSucceeded $true `
        -ChainCertificates @($selfSignedCertificate) `
        -MicrosoftTrustedRootThumbprints @($selfSignedCertificate.Thumbprint) `
        -ChainStatusDescriptions @()
} "self-signed"
Assert-Throws {
    Assert-PublicCodeSigningChainEvidence `
        -Certificate $publicLeafCertificate `
        -ChainBuildSucceeded $false `
        -ChainCertificates @($publicLeafCertificate, $publicIntermediateCertificate, $publicRootCertificate) `
        -MicrosoftTrustedRootThumbprints @($publicRootCertificate.Thumbprint) `
        -ChainStatusDescriptions @("RevocationStatusUnknown")
} "consumer-trusted certificate chain validation failed"
Assert-Throws {
    Assert-PublicCodeSigningChainEvidence `
        -Certificate $publicLeafCertificate `
        -ChainBuildSucceeded $true `
        -ChainCertificates @($publicLeafCertificate, $publicIntermediateCertificate, $publicRootCertificate) `
        -MicrosoftTrustedRootThumbprints @("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF") `
        -ChainStatusDescriptions @()
} "Microsoft AuthRoot"
$publicTrustEvidence = Assert-PublicCodeSigningChainEvidence `
    -Certificate $publicLeafCertificate `
    -ChainBuildSucceeded $true `
    -ChainCertificates @($publicLeafCertificate, $publicIntermediateCertificate, $publicRootCertificate) `
    -MicrosoftTrustedRootThumbprints @($publicRootCertificate.Thumbprint.ToLowerInvariant()) `
    -ChainStatusDescriptions @()
Assert-Condition `
    ($publicTrustEvidence.RootThumbprint -eq $publicRootCertificate.Thumbprint) `
    "A valid Microsoft AuthRoot chain must return the normalized public root thumbprint."
$selfIssuedTrustEvidence = Assert-PublicCodeSigningChainEvidence `
    -Certificate $selfIssuedPublicLeafCertificate `
    -ChainBuildSucceeded $true `
    -ChainCertificates @($selfIssuedPublicLeafCertificate, $publicRootCertificate) `
    -MicrosoftTrustedRootThumbprints @($publicRootCertificate.Thumbprint) `
    -ChainStatusDescriptions @()
Assert-Condition `
    ($selfIssuedTrustEvidence.RootThumbprint -eq $publicRootCertificate.Thumbprint) `
    "A self-issued leaf with a distinct Microsoft AuthRoot must not be mistaken for a self-signed chain."

$script:capturedNativePath = ""
$script:capturedNativeArguments = @()
function Invoke-Native {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    $script:capturedNativePath = $FilePath
    $script:capturedNativeArguments = $Arguments
}

Sign-AuthenticodeFile -SignTool "mock-signtool.exe" -File "Congregation.exe" -CertificateThumbprint $thumbprint -StoreLocation LocalMachine -TimestampUrl "http://timestamp.digicert.com"
$expectedSignArguments = @(
    "sign", "/fd", "SHA256", "/tr", "http://timestamp.digicert.com", "/td", "SHA256",
    "/sha1", $thumbprint, "/s", "My", "/d", "Congregation", "/sm", "Congregation.exe"
)
Assert-Condition (($script:capturedNativeArguments -join "|") -eq ($expectedSignArguments -join "|")) "SignTool signing arguments changed."

$script:mockSignerThumbprint = $thumbprint
$script:mockHasTimestamp = $true
function Get-AuthenticodeSignature {
    param([string]$LiteralPath)

    return [pscustomobject]@{
        Status = "Valid"
        SignerCertificate = [pscustomobject]@{ Thumbprint = $script:mockSignerThumbprint }
        TimeStamperCertificate = if ($script:mockHasTimestamp) { [pscustomobject]@{ Subject = "TSA" } } else { $null }
    }
}

Verify-AuthenticodeFile -SignTool "mock-signtool.exe" -File "Congregation.exe" -ExpectedCertificateThumbprint $thumbprint
Assert-Condition (($script:capturedNativeArguments -join "|") -eq "verify|/pa|/all|/tw|/v|Congregation.exe") "SignTool verification arguments changed."

$script:mockHasTimestamp = $false
Assert-Throws {
    Verify-AuthenticodeFile -SignTool "mock-signtool.exe" -File "Congregation.exe" -ExpectedCertificateThumbprint $thumbprint
} "RFC 3161 timestamp is missing"
$script:mockHasTimestamp = $true
$script:mockSignerThumbprint = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
Assert-Throws {
    Verify-AuthenticodeFile -SignTool "mock-signtool.exe" -File "Congregation.exe" -ExpectedCertificateThumbprint $thumbprint
} "does not match the requested certificate"

$script:stageFiles = @(
    [pscustomobject]@{ FullName = "C:\stage\Congregation.exe" },
    [pscustomobject]@{ FullName = "C:\stage\Qt6Core.dll" },
    [pscustomobject]@{ FullName = "C:\stage\libgcc_s_seh-1.dll" }
)
function Get-StagedPeFiles { return $script:stageFiles }
function Get-CongregationOwnedStageFiles { return @($script:stageFiles[0]) }
function Get-AuthenticodeSignature {
    param([string]$LiteralPath)

    $status = if ($LiteralPath.EndsWith("libgcc_s_seh-1.dll")) { "NotSigned" } else { "Valid" }
    return [pscustomobject]@{ Status = $status }
}
$script:signedStageFiles = @()
$script:verifiedStageFiles = @()
function Sign-AuthenticodeFile {
    param(
        [string]$SignTool,
        [string]$File,
        [string]$CertificateThumbprint,
        [string]$StoreLocation,
        [string]$TimestampUrl
    )
    $script:signedStageFiles += $File
}
function Verify-AuthenticodeFile {
    param(
        [string]$SignTool,
        [string]$File,
        [string]$ExpectedCertificateThumbprint = ""
    )
    $script:verifiedStageFiles += [pscustomobject]@{ File = $File; Expected = $ExpectedCertificateThumbprint }
}

Sign-WindowsStage -Directory "C:\stage" -SignTool "mock" -CertificateThumbprint $thumbprint -StoreLocation CurrentUser -TimestampUrl "http://timestamp.digicert.com"
Assert-Condition ($script:signedStageFiles -contains "C:\stage\Congregation.exe") "A pre-signed Congregation.exe was not rebound to the selected publisher."
Assert-Condition ($script:signedStageFiles -contains "C:\stage\libgcc_s_seh-1.dll") "An unsigned staged PE was not signed."
Assert-Condition (-not ($script:signedStageFiles -contains "C:\stage\Qt6Core.dll")) "A valid vendor signature was replaced."

$script:verifiedStageFiles = @()
Verify-WindowsStageSignatures -Directory "C:\stage" -SignTool "mock" -CertificateThumbprint $thumbprint
$ownedVerification = $script:verifiedStageFiles | Where-Object { $_.File -eq "C:\stage\Congregation.exe" } | Select-Object -First 1
$vendorVerification = $script:verifiedStageFiles | Where-Object { $_.File -eq "C:\stage\Qt6Core.dll" } | Select-Object -First 1
Assert-Condition ($ownedVerification.Expected -eq $thumbprint) "Congregation-owned signature verification did not require the selected publisher."
Assert-Condition ([string]::IsNullOrEmpty($vendorVerification.Expected)) "Vendor signature verification was incorrectly bound to Congregation's certificate."

$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("Congregation-AuthenticodeTest-" + [Guid]::NewGuid().ToString("N"))
$artifactPath = Join-Path $temporaryRoot "artifact.partial.zip"
$recordPath = Join-Path $temporaryRoot "artifact.zip.sha256.partial"
$finalArtifactPath = Join-Path $temporaryRoot "artifact.zip"
$finalRecordPath = "$finalArtifactPath.sha256"
New-Item -ItemType Directory -Path $temporaryRoot | Out-Null
try {
    $mutexProbeOutput = Join-Path $temporaryRoot "mutex-probe.out"
    $mutexProbeError = Join-Path $temporaryRoot "mutex-probe.err"
    $mutexFunctionText = ($functionAsts | Where-Object { $_.Name -eq "Enter-WindowsBuildMutex" } | Select-Object -First 1).Extent.Text
    $testMutexName = "Global\Congregation.BuildWindows.Test.$([Guid]::NewGuid().ToString('N'))"
    $mutexProbeCommand = @"
$mutexFunctionText
`$probeMutex = Enter-WindowsBuildMutex -MutexName '$testMutexName'
`$probeMutex.ReleaseMutex()
`$probeMutex.Dispose()
"@
    $mutexProbeEncodedCommand = [Convert]::ToBase64String([System.Text.Encoding]::Unicode.GetBytes($mutexProbeCommand))
    $currentPowerShellExecutable = (Get-Process -Id $PID -ErrorAction Stop).Path
    $heldMutex = Enter-WindowsBuildMutex -MutexName $testMutexName
    try {
        $blockedProbe = Start-Process `
            -FilePath $currentPowerShellExecutable `
            -ArgumentList @("-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-EncodedCommand", $mutexProbeEncodedCommand) `
            -RedirectStandardOutput $mutexProbeOutput `
            -RedirectStandardError $mutexProbeError `
            -Wait `
            -PassThru
        Assert-Condition ($blockedProbe.ExitCode -ne 0) "A concurrent Windows package process acquired the workspace mutex."
    } finally {
        $heldMutex.ReleaseMutex()
        $heldMutex.Dispose()
    }
    $releasedProbe = Start-Process `
        -FilePath $currentPowerShellExecutable `
        -ArgumentList @("-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-EncodedCommand", $mutexProbeEncodedCommand) `
        -RedirectStandardOutput $mutexProbeOutput `
        -RedirectStandardError $mutexProbeError `
        -Wait `
        -PassThru
    Assert-Condition ($releasedProbe.ExitCode -eq 0) "The workspace mutex was not released for the next package process."

    [System.IO.File]::WriteAllText($artifactPath, "Congregation", [System.Text.Encoding]::UTF8)
    Write-Sha256File -File $artifactPath -OutputPath $recordPath -RecordedFileName "artifact.zip"
    $record = (Get-Content -LiteralPath $recordPath -Raw).Trim()
    Assert-Condition $record.EndsWith(" *artifact.zip") "SHA-256 record did not bind the final artifact name."

    Clear-WindowsPackageArtifacts -Paths @($artifactPath, $recordPath)
    Assert-Condition (-not (Test-Path -LiteralPath $artifactPath)) "Artifact cleanup left the old package in place."
    Assert-Condition (-not (Test-Path -LiteralPath $recordPath)) "Artifact cleanup left the old checksum in place."

    [System.IO.File]::WriteAllText($finalArtifactPath, "previous", [System.Text.Encoding]::UTF8)
    Write-Sha256File -File $finalArtifactPath -OutputPath $finalRecordPath -RecordedFileName "artifact.zip"
    [System.IO.File]::WriteAllText($artifactPath, "replacement", [System.Text.Encoding]::UTF8)
    Write-Sha256File -File $artifactPath -OutputPath $recordPath -RecordedFileName "artifact.zip"

    Publish-PackageArtifact `
        -PartialArtifactPath $artifactPath `
        -FinalArtifactPath $finalArtifactPath `
        -PartialChecksumPath $recordPath `
        -FinalChecksumPath $finalRecordPath

    Assert-Condition ((Get-Content -LiteralPath $finalArtifactPath -Raw) -eq "replacement") "The verified replacement was not published."
    Assert-Condition ((Get-Content -LiteralPath $finalRecordPath -Raw).Trim().EndsWith(" *artifact.zip")) "The published checksum does not name the final artifact."
    Assert-Condition (-not (Test-Path -LiteralPath "$finalArtifactPath.previous")) "A successful publication left an artifact backup behind."
    Assert-Condition (-not (Test-Path -LiteralPath "$finalRecordPath.previous")) "A successful publication left a checksum backup behind."

    [System.IO.File]::WriteAllText($artifactPath, "invalid replacement", [System.Text.Encoding]::UTF8)
    [System.IO.File]::WriteAllText($recordPath, (("0" * 64) + " *artifact.zip"), [System.Text.Encoding]::ASCII)
    Assert-Throws {
        Publish-PackageArtifact `
            -PartialArtifactPath $artifactPath `
            -FinalArtifactPath $finalArtifactPath `
            -PartialChecksumPath $recordPath `
            -FinalChecksumPath $finalRecordPath
    } "checksum does not match"
    Assert-Condition ((Get-Content -LiteralPath $finalArtifactPath -Raw) -eq "replacement") "A failed publication destroyed the last-known-good artifact."
    Assert-Condition ((Get-Content -LiteralPath $finalRecordPath -Raw).Trim().EndsWith(" *artifact.zip")) "A failed publication destroyed the last-known-good checksum."

    $artifactBackupPath = "$finalArtifactPath.previous"
    $recordBackupPath = "$finalRecordPath.previous"
    [System.IO.File]::WriteAllText($artifactBackupPath, "crash-safe previous", [System.Text.Encoding]::UTF8)
    Write-Sha256File -File $artifactBackupPath -OutputPath $recordBackupPath -RecordedFileName "artifact.zip"
    [System.IO.File]::WriteAllText($finalArtifactPath, "interrupted replacement", [System.Text.Encoding]::UTF8)
    Remove-Item -LiteralPath $finalRecordPath -Force

    Restore-PackageArtifactBackups `
        -FinalArtifactPath $finalArtifactPath `
        -FinalChecksumPath $finalRecordPath

    Assert-Condition ((Get-Content -LiteralPath $finalArtifactPath -Raw) -eq "crash-safe previous") "Interrupted publication did not restore the previous artifact."
    Assert-PackageArtifactChecksum `
        -ArtifactPath $finalArtifactPath `
        -ChecksumPath $finalRecordPath `
        -RecordedArtifactPath $finalArtifactPath
    Assert-Condition (-not (Test-Path -LiteralPath $artifactBackupPath)) "Artifact crash recovery left its backup behind."
    Assert-Condition (-not (Test-Path -LiteralPath $recordBackupPath)) "Checksum crash recovery left its backup behind."

    [System.IO.File]::WriteAllText($finalArtifactPath, "completed replacement", [System.Text.Encoding]::UTF8)
    Write-Sha256File -File $finalArtifactPath -OutputPath $finalRecordPath -RecordedFileName "artifact.zip"
    [System.IO.File]::WriteAllText($artifactBackupPath, "stale previous", [System.Text.Encoding]::UTF8)
    Write-Sha256File -File $artifactBackupPath -OutputPath $recordBackupPath -RecordedFileName "artifact.zip"
    Remove-Item -LiteralPath $artifactBackupPath -Force
    Restore-PackageArtifactBackups -FinalArtifactPath $finalArtifactPath -FinalChecksumPath $finalRecordPath
    Assert-Condition ((Get-Content -LiteralPath $finalArtifactPath -Raw) -eq "completed replacement") "A checksum-only stale backup replaced a complete artifact."
    Assert-PackageArtifactChecksum -ArtifactPath $finalArtifactPath -ChecksumPath $finalRecordPath -RecordedArtifactPath $finalArtifactPath
    Assert-Condition (-not (Test-Path -LiteralPath $recordBackupPath)) "Checksum-only crash recovery left its stale backup behind."

    [System.IO.File]::WriteAllText($artifactBackupPath, "stale previous", [System.Text.Encoding]::UTF8)
    Restore-PackageArtifactBackups -FinalArtifactPath $finalArtifactPath -FinalChecksumPath $finalRecordPath
    Assert-Condition ((Get-Content -LiteralPath $finalArtifactPath -Raw) -eq "completed replacement") "An artifact-only stale backup replaced a complete artifact pair."
    Assert-Condition (-not (Test-Path -LiteralPath $artifactBackupPath)) "Artifact-only crash recovery left its stale backup behind."

    $groupItems = @()
    foreach ($name in @("bundle.zip", "installer.msi")) {
        $partial = Join-Path $temporaryRoot "$name.partial"
        $final = Join-Path $temporaryRoot $name
        $partialChecksum = Join-Path $temporaryRoot "$name.sha256.partial"
        $finalChecksum = Join-Path $temporaryRoot "$name.sha256"
        [System.IO.File]::WriteAllText($final, "old-$name", [System.Text.Encoding]::UTF8)
        Write-Sha256File -File $final -OutputPath $finalChecksum -RecordedFileName $name
        [System.IO.File]::WriteAllText($partial, "new-$name", [System.Text.Encoding]::UTF8)
        Write-Sha256File -File $partial -OutputPath $partialChecksum -RecordedFileName $name
        $groupItems += [pscustomobject]@{
            PartialArtifactPath = $partial
            FinalArtifactPath = $final
            PartialChecksumPath = $partialChecksum
            FinalChecksumPath = $finalChecksum
        }
    }
    $groupJournalPath = Join-Path $temporaryRoot "group-publication.json"
    Publish-PackageArtifactSet -Artifacts $groupItems -JournalPath $groupJournalPath
    Assert-Condition (-not (Test-Path -LiteralPath $groupJournalPath)) "Successful grouped publication left its transaction journal behind."
    Assert-Condition (-not (Test-Path -LiteralPath "$groupJournalPath.partial")) "Successful grouped publication left its temporary transaction journal behind."
    Assert-Condition (-not (Test-Path -LiteralPath "$groupJournalPath.previous")) "Successful grouped publication left its replaced transaction journal behind."
    foreach ($item in $groupItems) {
        $name = [System.IO.Path]::GetFileName($item.FinalArtifactPath)
        Assert-Condition ((Get-Content -LiteralPath $item.FinalArtifactPath -Raw) -eq "new-$name") "Grouped publication did not commit every verified artifact."
        Assert-PackageArtifactChecksum -ArtifactPath $item.FinalArtifactPath -ChecksumPath $item.FinalChecksumPath -RecordedArtifactPath $item.FinalArtifactPath
    }

    $interruptedGroupItem = $groupItems[1]
    $interruptedArtifactBackup = "$($interruptedGroupItem.FinalArtifactPath).previous"
    $interruptedChecksumBackup = "$($interruptedGroupItem.FinalChecksumPath).previous"
    $interruptedName = [System.IO.Path]::GetFileName($interruptedGroupItem.FinalArtifactPath)
    [System.IO.File]::WriteAllText($interruptedArtifactBackup, "older-$interruptedName", [System.Text.Encoding]::UTF8)
    Write-Sha256File -File $interruptedArtifactBackup -OutputPath $interruptedChecksumBackup -RecordedFileName $interruptedName
    Restore-PackageArtifactSetBackups -Artifacts $groupItems
    foreach ($item in $groupItems) {
        $name = [System.IO.Path]::GetFileName($item.FinalArtifactPath)
        Assert-Condition ((Get-Content -LiteralPath $item.FinalArtifactPath -Raw) -eq "new-$name") "Partial backup cleanup rolled a complete grouped release back to mixed generations."
    }
    Assert-Condition (-not (Test-Path -LiteralPath $interruptedArtifactBackup)) "Grouped recovery left a stale artifact backup behind."
    Assert-Condition (-not (Test-Path -LiteralPath $interruptedChecksumBackup)) "Grouped recovery left a stale checksum backup behind."

    foreach ($item in $groupItems) {
        Remove-Item -LiteralPath $item.FinalArtifactPath, $item.FinalChecksumPath -Force
    }
    $firstPublicationJournal = [ordered]@{
        SchemaVersion = 1
        Phase = "prepared"
        Items = @(
            foreach ($item in $groupItems) {
                [ordered]@{
                    FinalArtifactPath = $item.FinalArtifactPath
                    FinalChecksumPath = $item.FinalChecksumPath
                    ArtifactExisted = $false
                    ChecksumExisted = $false
                }
            }
        )
    }
    [System.IO.File]::WriteAllText(
        $groupJournalPath,
        ($firstPublicationJournal | ConvertTo-Json -Depth 5),
        (New-Object System.Text.UTF8Encoding($false))
    )
    $firstItem = $groupItems[0]
    $secondItem = $groupItems[1]
    [System.IO.File]::WriteAllText($firstItem.FinalArtifactPath, "first-published", [System.Text.Encoding]::UTF8)
    Write-Sha256File -File $firstItem.FinalArtifactPath -OutputPath $firstItem.FinalChecksumPath -RecordedFileName ([System.IO.Path]::GetFileName($firstItem.FinalArtifactPath))
    [System.IO.File]::WriteAllText($secondItem.FinalArtifactPath, "second-interrupted", [System.Text.Encoding]::UTF8)

    Restore-PackageArtifactSetBackups -Artifacts $groupItems -JournalPath $groupJournalPath
    foreach ($item in $groupItems) {
        Assert-Condition (-not (Test-Path -LiteralPath $item.FinalArtifactPath)) "Interrupted first publication left a canonical artifact that did not previously exist."
        Assert-Condition (-not (Test-Path -LiteralPath $item.FinalChecksumPath)) "Interrupted first publication left a canonical checksum that did not previously exist."
    }
    Assert-Condition (-not (Test-Path -LiteralPath $groupJournalPath)) "First-publication recovery left its transaction journal behind."

    $subsetItem = $groupItems[0]
    Write-PackagePublicationJournal -JournalPath $groupJournalPath -Phase "prepared" -Items @(
        [pscustomobject]@{
            FinalArtifactPath = $subsetItem.FinalArtifactPath
            FinalChecksumPath = $subsetItem.FinalChecksumPath
            ArtifactExisted = $false
            ChecksumExisted = $false
        }
    )
    [System.IO.File]::WriteAllText($subsetItem.FinalArtifactPath, "subset-interrupted", [System.Text.Encoding]::UTF8)
    Restore-PackageArtifactSetBackups -Artifacts $groupItems -JournalPath $groupJournalPath
    Assert-Condition (-not (Test-Path -LiteralPath $subsetItem.FinalArtifactPath)) "Allowlist recovery retained a subset publication artifact."
    Assert-Condition (-not (Test-Path -LiteralPath $subsetItem.FinalChecksumPath)) "Allowlist recovery retained a subset publication checksum."
    Assert-Condition (-not (Test-Path -LiteralPath $groupJournalPath)) "Allowlist recovery left its shared transaction journal behind."

    $stableName = [System.IO.Path]::GetFileName($subsetItem.FinalArtifactPath)
    [System.IO.File]::WriteAllText($subsetItem.FinalArtifactPath, "stable-new", [System.Text.Encoding]::UTF8)
    Write-Sha256File -File $subsetItem.FinalArtifactPath -OutputPath $subsetItem.FinalChecksumPath -RecordedFileName $stableName
    [System.IO.File]::WriteAllText("$($subsetItem.FinalArtifactPath).previous", "stale-old", [System.Text.Encoding]::UTF8)
    Write-Sha256File -File "$($subsetItem.FinalArtifactPath).previous" -OutputPath "$($subsetItem.FinalChecksumPath).previous" -RecordedFileName $stableName
    Restore-PackageArtifactSetBackups -Artifacts $groupItems -JournalPath $groupJournalPath
    Assert-Condition ((Get-Content -LiteralPath $subsetItem.FinalArtifactPath -Raw) -eq "stable-new") "An absent allowlisted MSI caused a valid ZIP to roll back."
    Assert-Condition (-not (Test-Path -LiteralPath "$($subsetItem.FinalArtifactPath).previous")) "Consistent-state recovery left a stale artifact backup."
    Assert-Condition (-not (Test-Path -LiteralPath "$($subsetItem.FinalChecksumPath).previous")) "Consistent-state recovery left a stale checksum backup."

    $unrequestedItem = $groupItems[1]
    [System.IO.File]::WriteAllText($unrequestedItem.FinalArtifactPath, "unrequested-incomplete", [System.Text.Encoding]::UTF8)
    Restore-PackageArtifactSetBackups -Artifacts @($subsetItem) -JournalPath $groupJournalPath
    Assert-Condition ((Get-Content -LiteralPath $subsetItem.FinalArtifactPath -Raw) -eq "stable-new") "Journal-free subset recovery changed the requested artifact."
    Assert-Condition ((Get-Content -LiteralPath $unrequestedItem.FinalArtifactPath -Raw) -eq "unrequested-incomplete") "Journal-free subset recovery changed an unrequested artifact."
    Remove-Item -LiteralPath $unrequestedItem.FinalArtifactPath -Force

    $existingItem = $groupItems[0]
    $newItem = $groupItems[1]
    $existingName = [System.IO.Path]::GetFileName($existingItem.FinalArtifactPath)
    [System.IO.File]::WriteAllText($existingItem.FinalArtifactPath, "mixed-old", [System.Text.Encoding]::UTF8)
    Write-Sha256File -File $existingItem.FinalArtifactPath -OutputPath $existingItem.FinalChecksumPath -RecordedFileName $existingName
    Write-PackagePublicationJournal -JournalPath $groupJournalPath -Phase "prepared" -Items @(
        [pscustomobject]@{
            FinalArtifactPath = $existingItem.FinalArtifactPath
            FinalChecksumPath = $existingItem.FinalChecksumPath
            ArtifactExisted = $true
            ChecksumExisted = $true
        },
        [pscustomobject]@{
            FinalArtifactPath = $newItem.FinalArtifactPath
            FinalChecksumPath = $newItem.FinalChecksumPath
            ArtifactExisted = $false
            ChecksumExisted = $false
        }
    )
    Move-Item -LiteralPath $existingItem.FinalArtifactPath -Destination "$($existingItem.FinalArtifactPath).previous"
    Move-Item -LiteralPath $existingItem.FinalChecksumPath -Destination "$($existingItem.FinalChecksumPath).previous"
    [System.IO.File]::WriteAllText($existingItem.FinalArtifactPath, "mixed-new", [System.Text.Encoding]::UTF8)
    Write-Sha256File -File $existingItem.FinalArtifactPath -OutputPath $existingItem.FinalChecksumPath -RecordedFileName $existingName
    [System.IO.File]::WriteAllText($newItem.FinalArtifactPath, "new-msi-interrupted", [System.Text.Encoding]::UTF8)

    Restore-PackageArtifactSetBackups -Artifacts $groupItems -JournalPath $groupJournalPath
    Assert-Condition ((Get-Content -LiteralPath $existingItem.FinalArtifactPath -Raw) -eq "mixed-old") "Mixed-baseline recovery did not restore the existing package generation."
    Assert-PackageArtifactChecksum -ArtifactPath $existingItem.FinalArtifactPath -ChecksumPath $existingItem.FinalChecksumPath -RecordedArtifactPath $existingItem.FinalArtifactPath
    Assert-Condition (-not (Test-Path -LiteralPath $newItem.FinalArtifactPath)) "Mixed-baseline recovery retained a newly introduced artifact."
    Assert-Condition (-not (Test-Path -LiteralPath $newItem.FinalChecksumPath)) "Mixed-baseline recovery retained a newly introduced checksum."

    foreach ($item in $groupItems) {
        $name = [System.IO.Path]::GetFileName($item.FinalArtifactPath)
        [System.IO.File]::WriteAllText($item.FinalArtifactPath, "committed-$name", [System.Text.Encoding]::UTF8)
        Write-Sha256File -File $item.FinalArtifactPath -OutputPath $item.FinalChecksumPath -RecordedFileName $name
        [System.IO.File]::WriteAllText("$($item.FinalArtifactPath).previous", "old-$name", [System.Text.Encoding]::UTF8)
        Write-Sha256File -File "$($item.FinalArtifactPath).previous" -OutputPath "$($item.FinalChecksumPath).previous" -RecordedFileName $name
    }
    Write-PackagePublicationJournal -JournalPath $groupJournalPath -Phase "committed" -Items @(
        foreach ($item in $groupItems) {
            [pscustomobject]@{
                FinalArtifactPath = $item.FinalArtifactPath
                FinalChecksumPath = $item.FinalChecksumPath
                ArtifactExisted = $true
                ChecksumExisted = $true
            }
        }
    )
    Restore-PackageArtifactSetBackups -Artifacts $groupItems -JournalPath $groupJournalPath
    foreach ($item in $groupItems) {
        $name = [System.IO.Path]::GetFileName($item.FinalArtifactPath)
        Assert-Condition ((Get-Content -LiteralPath $item.FinalArtifactPath -Raw) -eq "committed-$name") "Committed recovery replaced the verified new generation."
        Assert-Condition (-not (Test-Path -LiteralPath "$($item.FinalArtifactPath).previous")) "Committed recovery left an artifact backup behind."
        Assert-Condition (-not (Test-Path -LiteralPath "$($item.FinalChecksumPath).previous")) "Committed recovery left a checksum backup behind."
    }

    Write-PackagePublicationJournal -JournalPath $groupJournalPath -Phase "committed" -Items @(
        foreach ($item in $groupItems) {
            [pscustomobject]@{
                FinalArtifactPath = $item.FinalArtifactPath
                FinalChecksumPath = $item.FinalChecksumPath
                ArtifactExisted = $true
                ChecksumExisted = $true
            }
        }
    )
    [System.IO.File]::WriteAllText($newItem.FinalArtifactPath, "corrupted-after-commit", [System.Text.Encoding]::UTF8)
    $existingContentBeforeFailedRecovery = Get-Content -LiteralPath $existingItem.FinalArtifactPath -Raw
    Assert-Throws {
        Restore-PackageArtifactSetBackups -Artifacts $groupItems -JournalPath $groupJournalPath
    } "committed package publication contains an invalid"
    Assert-Condition (Test-Path -LiteralPath $groupJournalPath) "Failed committed recovery discarded the evidence needed for manual recovery."
    Assert-Condition ((Get-Content -LiteralPath $existingItem.FinalArtifactPath -Raw) -eq $existingContentBeforeFailedRecovery) "Failed committed recovery mutated another canonical artifact."
    Assert-Condition ((Get-Content -LiteralPath $newItem.FinalArtifactPath -Raw) -eq "corrupted-after-commit") "Failed committed recovery mutated the invalid canonical artifact."
    Remove-PackagePublicationJournal -JournalPath $groupJournalPath

    $guardArtifactContent = Get-Content -LiteralPath $existingItem.FinalArtifactPath -Raw
    $guardChecksumContent = Get-Content -LiteralPath $existingItem.FinalChecksumPath -Raw
    $guardJournalItems = @(
        [pscustomobject]@{
            FinalArtifactPath = $existingItem.FinalArtifactPath
            FinalChecksumPath = $existingItem.FinalChecksumPath
            ArtifactExisted = $true
            ChecksumExisted = $true
        }
    )
    Write-PackagePublicationJournal -JournalPath $groupJournalPath -Phase "prepared" -Items $guardJournalItems
    $invalidSchemaJson = (Get-Content -LiteralPath $groupJournalPath -Raw) -replace '"SchemaVersion"\s*:\s*1', '"SchemaVersion": 2'
    [System.IO.File]::WriteAllText($groupJournalPath, $invalidSchemaJson, (New-Object System.Text.UTF8Encoding($false)))
    Assert-Throws {
        Restore-PackageArtifactSetBackups -Artifacts $groupItems -JournalPath $groupJournalPath
    } "unsupported schema"
    Assert-Condition ((Get-Content -LiteralPath $existingItem.FinalArtifactPath -Raw) -eq $guardArtifactContent) "Invalid-schema journal mutated a canonical artifact."
    Assert-Condition ((Get-Content -LiteralPath $existingItem.FinalChecksumPath -Raw) -eq $guardChecksumContent) "Invalid-schema journal mutated a canonical checksum."
    Assert-Condition (Test-Path -LiteralPath $groupJournalPath) "Invalid-schema recovery discarded its journal."
    Remove-PackagePublicationJournal -JournalPath $groupJournalPath

    Write-PackagePublicationJournal -JournalPath $groupJournalPath -Phase "prepared" -Items @(
        [pscustomobject]@{
            FinalArtifactPath = (Join-Path $temporaryRoot "unexpected.zip")
            FinalChecksumPath = (Join-Path $temporaryRoot "unexpected.zip.sha256")
            ArtifactExisted = $false
            ChecksumExisted = $false
        }
    )
    Assert-Throws {
        Restore-PackageArtifactSetBackups -Artifacts $groupItems -JournalPath $groupJournalPath
    } "allowed artifact set"
    Assert-Condition ((Get-Content -LiteralPath $existingItem.FinalArtifactPath -Raw) -eq $guardArtifactContent) "Path-mismatched journal mutated a canonical artifact."
    Assert-Condition ((Get-Content -LiteralPath $existingItem.FinalChecksumPath -Raw) -eq $guardChecksumContent) "Path-mismatched journal mutated a canonical checksum."
    Remove-PackagePublicationJournal -JournalPath $groupJournalPath

    $invalidBooleanJournal = [ordered]@{
        SchemaVersion = 1
        Phase = "prepared"
        Items = @(
            [ordered]@{
                FinalArtifactPath = $existingItem.FinalArtifactPath
                FinalChecksumPath = $existingItem.FinalChecksumPath
                ArtifactExisted = "true"
                ChecksumExisted = $true
            }
        )
    }
    [System.IO.File]::WriteAllText(
        $groupJournalPath,
        ($invalidBooleanJournal | ConvertTo-Json -Depth 5),
        (New-Object System.Text.UTF8Encoding($false))
    )
    Assert-Throws {
        Restore-PackageArtifactSetBackups -Artifacts $groupItems -JournalPath $groupJournalPath
    } "Boolean values"
    Assert-Condition ((Get-Content -LiteralPath $existingItem.FinalArtifactPath -Raw) -eq $guardArtifactContent) "Invalid-Boolean journal mutated a canonical artifact."
    Assert-Condition ((Get-Content -LiteralPath $existingItem.FinalChecksumPath -Raw) -eq $guardChecksumContent) "Invalid-Boolean journal mutated a canonical checksum."
    Remove-PackagePublicationJournal -JournalPath $groupJournalPath

    [System.IO.File]::WriteAllText($groupJournalPath, "{", (New-Object System.Text.UTF8Encoding($false)))
    Assert-Throws {
        Restore-PackageArtifactSetBackups -Artifacts $groupItems -JournalPath $groupJournalPath
    } "journal is malformed"
    Assert-Condition ((Get-Content -LiteralPath $existingItem.FinalArtifactPath -Raw) -eq $guardArtifactContent) "Malformed journal mutated a canonical artifact."
    Assert-Condition ((Get-Content -LiteralPath $existingItem.FinalChecksumPath -Raw) -eq $guardChecksumContent) "Malformed journal mutated a canonical checksum."
    Assert-Condition (Test-Path -LiteralPath $groupJournalPath) "Malformed recovery discarded its journal."
    Remove-PackagePublicationJournal -JournalPath $groupJournalPath
} finally {
    Remove-Item -LiteralPath $artifactPath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $recordPath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $finalArtifactPath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $finalRecordPath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath "$finalArtifactPath.previous" -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath "$finalRecordPath.previous" -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $temporaryRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "Windows Authenticode policy tests passed."
