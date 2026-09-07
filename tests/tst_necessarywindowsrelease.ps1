#Requires -Version 5.1
param(
    [Parameter(Mandatory = $true)]
    [string]$ScriptPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

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
Assert-Condition ($parseErrors.Count -eq 0) "necessary-windows-release.ps1 contains a PowerShell parse error."

$functionAsts = $ast.FindAll(
    { param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] },
    $true
)
foreach ($functionAst in $functionAsts) {
    . ([scriptblock]::Create($functionAst.Extent.Text))
}

foreach ($requiredFunction in @(
    "Get-CongregationNecessaryReleaseVersion",
    "Get-CongregationNecessaryFileSha256",
    "Invoke-CongregationNecessaryReleaseNative",
    "Invoke-CongregationNecessaryMsiDatabaseContract",
    "Invoke-CongregationNecessaryAdministrativeExtraction",
    "Publish-CongregationNecessaryReleaseFile",
    "Invoke-CongregationNecessaryWindowsRelease"
)) {
    Assert-Condition `
        ($null -ne (Get-Command $requiredFunction -CommandType Function -ErrorAction SilentlyContinue)) `
        "Required Necessary Windows release function is missing: $requiredFunction"
}

$temporaryRoot = Join-Path `
    ([System.IO.Path]::GetTempPath()) `
    ("Congregation-NecessaryReleaseTest-" + [Guid]::NewGuid().ToString("N"))
$repositoryRoot = Join-Path $temporaryRoot "repository"
$buildDirectory = Join-Path $repositoryRoot "build"
$stageDirectory = Join-Path $repositoryRoot "dist\Congregation-Windows"
$msiWorkDirectory = Join-Path $buildDirectory "msi"
$wixToolsDirectory = Join-Path $temporaryRoot "wix"
$mockToolsDirectory = Join-Path $temporaryRoot "tools"

try {
    foreach ($directory in @(
        $repositoryRoot,
        $buildDirectory,
        $stageDirectory,
        $msiWorkDirectory,
        $wixToolsDirectory,
        $mockToolsDirectory,
        (Join-Path $repositoryRoot "tests")
    )) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }

    [System.IO.File]::WriteAllText(
        (Join-Path $buildDirectory "CMakeCache.txt"),
        "CMAKE_PROJECT_VERSION:STATIC=1.0.0`r`n"
    )
    foreach ($name in @("CongregationProduct.wxs", "CongregationRuntime.wxs", "CongregationLicense.rtf")) {
        [System.IO.File]::WriteAllText((Join-Path $msiWorkDirectory $name), "verified MSI input")
    }
    [System.IO.File]::WriteAllText(
        (Join-Path $repositoryRoot "tests\tst_windowsmsidatabasecontract.ps1"),
        "mock database contract"
    )

    foreach ($name in @("candle.exe", "light.exe", "smoke.exe", "darice.cub")) {
        [System.IO.File]::WriteAllText((Join-Path $wixToolsDirectory $name), "mock WiX tool")
    }
    $osslSignCodePath = Join-Path $mockToolsDirectory "osslsigncode.exe"
    $signToolPath = Join-Path $mockToolsDirectory "signtool.exe"
    [System.IO.File]::WriteAllText($osslSignCodePath, "mock osslsigncode")
    [System.IO.File]::WriteAllText($signToolPath, "mock SignTool")

    foreach ($name in @(
        "Congregation.exe",
        "LVRS.dll",
        "libiiPaintEngine.dll",
        "libgcc_s_seh-1.dll"
    )) {
        [System.IO.File]::WriteAllText((Join-Path $stageDirectory $name), "unsigned")
    }
    [System.IO.File]::WriteAllText((Join-Path $stageDirectory "Qt6Core.dll"), "valid-vendor")

    $script:necessarySubject = "CN=Necessary Innovations AB, O=Necessary Innovations AB, C=SE"
    $script:necessaryThumbprint = "0123456789ABCDEF0123456789ABCDEF01234567"
    $script:timestamp = [pscustomobject]@{ Subject = "CN=RFC 3161 TSA" }
    $script:signCalls = @()
    $script:nativeCalls = @()
    $script:databaseContractCalls = 0
    $script:administrativeExtractionCalls = 0
    $script:failDatabaseContract = $false

    function Get-AuthenticodeSignature {
        param([string]$LiteralPath)

        $content = if (Test-Path -LiteralPath $LiteralPath -PathType Leaf) {
            Get-Content -LiteralPath $LiteralPath -Raw
        } else {
            ""
        }
        if ($content.StartsWith("signed-by-necessary")) {
            return [pscustomobject]@{
                Status = "Valid"
                SignerCertificate = [pscustomobject]@{
                    Subject = $script:necessarySubject
                    Thumbprint = $script:necessaryThumbprint
                }
                TimeStamperCertificate = $script:timestamp
            }
        }
        if ($content.StartsWith("valid-vendor")) {
            return [pscustomobject]@{
                Status = "Valid"
                SignerCertificate = [pscustomobject]@{
                    Subject = "CN=The Qt Company Oy, O=The Qt Company Oy, C=FI"
                    Thumbprint = "89ABCDEF0123456789ABCDEF0123456789ABCDEF"
                }
                TimeStamperCertificate = $script:timestamp
            }
        }
        return [pscustomobject]@{
            Status = "NotSigned"
            SignerCertificate = $null
            TimeStamperCertificate = $null
        }
    }

    function Invoke-NecessaryAuthenticodeSigning {
        param(
            [string]$File,
            [string]$OsslSignCodePath,
            [string]$SignToolPath,
            [string]$SigningToken,
            [string]$TimestampUrl,
            [string]$TemporaryRoot,
            [string]$ExpectedPublisher = "Necessary Innovations AB",
            [string]$ExpectedCertificateThumbprint = ""
        )

        $script:signCalls += [pscustomobject]@{
            File = $File
            Token = $SigningToken
            ExpectedPublisher = $ExpectedPublisher
            ExpectedCertificateThumbprint = $ExpectedCertificateThumbprint
        }
        [System.IO.File]::WriteAllText($File, "signed-by-necessary")
        return [pscustomobject]@{
            File = $File
            Subject = $script:necessarySubject
            Thumbprint = $script:necessaryThumbprint
            TimestampSubject = $script:timestamp.Subject
        }
    }

    function Invoke-CongregationNecessaryReleaseNative {
        param(
            [string]$FilePath,
            [string[]]$Arguments
        )

        $script:nativeCalls += [pscustomobject]@{
            FilePath = $FilePath
            Arguments = @($Arguments)
        }
        if ([System.IO.Path]::GetFileName($FilePath) -ieq "light.exe") {
            $outputIndex = [Array]::IndexOf($Arguments, "-out")
            [System.IO.File]::WriteAllText($Arguments[$outputIndex + 1], "unsigned MSI")
        }
    }

    function Invoke-CongregationNecessaryMsiDatabaseContract {
        param(
            [string]$RepositoryRoot,
            [string]$BuildDirectory,
            [string]$Version,
            [string]$MsiPath
        )

        $script:databaseContractCalls++
        if ($script:failDatabaseContract) {
            throw "Mock MSI database contract failure."
        }
    }

    function Invoke-CongregationNecessaryAdministrativeExtraction {
        param(
            [string]$MsiPath,
            [string]$OutputDirectory
        )

        $script:administrativeExtractionCalls++
        New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
        foreach ($name in @("Congregation.exe", "LVRS.dll", "libiiPaintEngine.dll")) {
            [System.IO.File]::WriteAllText(
                (Join-Path $OutputDirectory $name),
                "signed-by-necessary"
            )
        }
    }

    $result = Invoke-CongregationNecessaryWindowsRelease `
        -RepositoryRoot $repositoryRoot `
        -BuildDirectory $buildDirectory `
        -StageDirectory $stageDirectory `
        -WixToolsDirectory $wixToolsDirectory `
        -OsslSignCodePath $osslSignCodePath `
        -SignToolPath $signToolPath `
        -SigningToken "secret-token" `
        -TimestampUrl "http://timestamp.digicert.com"

    $expectedFinalMsi = Join-Path $buildDirectory "Congregation-1.0.0-Windows.msi"
    Assert-Condition ($result.File -ceq $expectedFinalMsi) `
        "The verified release was not published to the canonical versioned MSI path."
    Assert-Condition (Test-Path -LiteralPath $expectedFinalMsi -PathType Leaf) `
        "The canonical Necessary MSI was not published."
    Assert-Condition `
        ((Get-Content -LiteralPath $expectedFinalMsi -Raw) -ceq "signed-by-necessary") `
        "The canonical MSI does not contain the verified Necessary-signed candidate."
    Assert-Condition ($result.Thumbprint -ceq $script:necessaryThumbprint) `
        "The release result did not report the bound Necessary certificate."
    Assert-Condition ($script:signCalls.Count -eq 5) `
        "Exactly four unsigned/Congregation-owned PEs and the MSI must be signed."
    Assert-Condition `
        ([string]::IsNullOrWhiteSpace($script:signCalls[0].ExpectedCertificateThumbprint)) `
        "The first signing request must establish the release certificate thumbprint."
    foreach ($call in $script:signCalls | Select-Object -Skip 1) {
        Assert-Condition `
            ($call.ExpectedCertificateThumbprint -ceq $script:necessaryThumbprint) `
            "Every later signing request must be bound to the first release certificate."
    }
    Assert-Condition `
        (-not ($script:signCalls.File | Where-Object { $_.EndsWith("Qt6Core.dll") })) `
        "An existing valid vendor signature was replaced."
    Assert-Condition ($script:databaseContractCalls -eq 1) `
        "The final MSI database contract was not run exactly once."
    Assert-Condition ($script:administrativeExtractionCalls -eq 1) `
        "The final MSI was not administratively extracted exactly once."

    $sidecarPath = "$expectedFinalMsi.sha256"
    Assert-Condition (Test-Path -LiteralPath $sidecarPath -PathType Leaf) `
        "The verified MSI SHA-256 sidecar was not published."
    $expectedHash = (
        Get-CongregationNecessaryFileSha256 -File $expectedFinalMsi
    ).ToLowerInvariant()
    Assert-Condition `
        ((Get-Content -LiteralPath $sidecarPath -Raw).Contains($expectedHash)) `
        "The published MSI SHA-256 sidecar does not match the final file."

    [System.IO.File]::WriteAllText($expectedFinalMsi, "stable previous release")
    [System.IO.File]::WriteAllText($sidecarPath, "stable previous sidecar")
    foreach ($name in @(
        "Congregation.exe",
        "LVRS.dll",
        "libiiPaintEngine.dll",
        "libgcc_s_seh-1.dll"
    )) {
        [System.IO.File]::WriteAllText((Join-Path $stageDirectory $name), "unsigned")
    }
    $script:failDatabaseContract = $true

    Assert-Throws {
        Invoke-CongregationNecessaryWindowsRelease `
            -RepositoryRoot $repositoryRoot `
            -BuildDirectory $buildDirectory `
            -StageDirectory $stageDirectory `
            -WixToolsDirectory $wixToolsDirectory `
            -OsslSignCodePath $osslSignCodePath `
            -SignToolPath $signToolPath `
            -SigningToken "secret-token" `
            -TimestampUrl "http://timestamp.digicert.com"
    } "MSI database contract"
    Assert-Condition `
        ((Get-Content -LiteralPath $expectedFinalMsi -Raw) -ceq "stable previous release") `
        "A candidate that failed the MSI database contract replaced the previous release."
    Assert-Condition `
        ((Get-Content -LiteralPath $sidecarPath -Raw) -ceq "stable previous sidecar") `
        "A failed candidate replaced the previous release sidecar."
} finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}

Write-Host "Necessary Windows release orchestration tests passed."
