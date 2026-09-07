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
Assert-Condition ($parseErrors.Count -eq 0) "necessary-authenticode.ps1 contains a PowerShell parse error."

$functionAsts = $ast.FindAll(
    { param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] },
    $true
)
foreach ($functionAst in $functionAsts) {
    . ([scriptblock]::Create($functionAst.Extent.Text))
}

foreach ($requiredFunction in @(
    "Get-NecessarySigningEndpoint",
    "Assert-NecessarySigningConfiguration",
    "Invoke-NecessarySigningRequest",
    "Invoke-NecessaryAuthenticodeSigning"
)) {
    Assert-Condition `
        ($null -ne (Get-Command $requiredFunction -CommandType Function -ErrorAction SilentlyContinue)) `
        "Required Necessary Authenticode function is missing: $requiredFunction"
}

$endpoint = Get-NecessarySigningEndpoint
Assert-Condition `
    ($endpoint.AbsoluteUri -ceq "https://sign.necessary.nu/windows/sign") `
    "The Necessary signing endpoint must be fixed to the provider's HTTPS HSM service."

$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("Congregation-NecessaryTest-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $temporaryRoot | Out-Null
try {
    $toolPath = Join-Path $temporaryRoot "osslsigncode.exe"
    $signToolPath = Join-Path $temporaryRoot "signtool.exe"
    $inputPath = Join-Path $temporaryRoot "Congregation.exe"
    [System.IO.File]::WriteAllText($toolPath, "mock tool")
    [System.IO.File]::WriteAllText($signToolPath, "mock SignTool")
    [System.IO.File]::WriteAllText($inputPath, "original")

    Assert-Throws {
        Assert-NecessarySigningConfiguration `
            -OsslSignCodePath $toolPath `
            -SignToolPath $signToolPath `
            -SigningToken "" `
            -TimestampUrl "http://timestamp.digicert.com" `
            -TemporaryRoot $temporaryRoot
    } "NECESSARY_SIGN_TOKEN"
    Assert-Throws {
        Assert-NecessarySigningConfiguration `
            -OsslSignCodePath (Join-Path $temporaryRoot "missing.exe") `
            -SignToolPath $signToolPath `
            -SigningToken "secret-token" `
            -TimestampUrl "http://timestamp.digicert.com" `
            -TemporaryRoot $temporaryRoot
    } "osslsigncode"
    Assert-Throws {
        Assert-NecessarySigningConfiguration `
            -OsslSignCodePath $toolPath `
            -SignToolPath $signToolPath `
            -SigningToken "secret-token" `
            -TimestampUrl "file:///not-a-timestamp-service" `
            -TemporaryRoot $temporaryRoot
    } "TimestampUrl"

    Assert-NecessarySigningConfiguration `
        -OsslSignCodePath $toolPath `
        -SignToolPath $signToolPath `
        -SigningToken "secret-token" `
        -TimestampUrl "http://timestamp.digicert.com" `
        -TemporaryRoot $temporaryRoot

    $script:nativeCalls = @()
    function Invoke-NecessaryNative {
        param(
            [string]$FilePath,
            [string[]]$Arguments
        )

        $script:nativeCalls += [pscustomobject]@{
            FilePath = $FilePath
            Arguments = @($Arguments)
        }
        if ($Arguments[0] -eq "extract-data") {
            $outputIndex = [Array]::IndexOf($Arguments, "-out")
            [System.IO.File]::WriteAllText($Arguments[$outputIndex + 1], "digest")
        } elseif ($Arguments[0] -eq "attach-signature") {
            $outputIndex = [Array]::IndexOf($Arguments, "-out")
            [System.IO.File]::WriteAllText($Arguments[$outputIndex + 1], "attached")
        } elseif ($Arguments[0] -eq "add") {
            $outputIndex = [Array]::IndexOf($Arguments, "-out")
            [System.IO.File]::WriteAllText($Arguments[$outputIndex + 1], "timestamped")
        }
    }

    $script:requestEndpoint = $null
    $script:requestToken = ""
    function Invoke-NecessarySigningRequest {
        param(
            [System.Uri]$Endpoint,
            [string]$SigningToken,
            [string]$InputPath,
            [string]$OutputPath
        )

        $script:requestEndpoint = $Endpoint
        $script:requestToken = $SigningToken
        Assert-Condition ((Get-Content -LiteralPath $InputPath -Raw) -eq "digest") `
            "The signing request did not receive the extracted Authenticode digest."
        [System.IO.File]::WriteAllText($OutputPath, "detached signature")
    }

    $script:mockSignatureStatus = "Valid"
    $script:mockSubject = "CN=Necessary Innovations AB, O=Necessary Innovations AB, C=SE"
    $script:mockThumbprint = "0123456789ABCDEF0123456789ABCDEF01234567"
    $script:mockTimestamp = [pscustomobject]@{ Subject = "CN=RFC 3161 TSA" }
    function Get-AuthenticodeSignature {
        param([string]$LiteralPath)

        return [pscustomobject]@{
            Status = $script:mockSignatureStatus
            SignerCertificate = [pscustomobject]@{
                Subject = $script:mockSubject
                Thumbprint = $script:mockThumbprint
            }
            TimeStamperCertificate = $script:mockTimestamp
        }
    }

    $signingResult = Invoke-NecessaryAuthenticodeSigning `
        -File $inputPath `
        -OsslSignCodePath $toolPath `
        -SignToolPath $signToolPath `
        -SigningToken "secret-token" `
        -TimestampUrl "http://timestamp.digicert.com" `
        -TemporaryRoot $temporaryRoot

    Assert-Condition `
        ((Get-Content -LiteralPath $inputPath -Raw) -eq "timestamped") `
        "The fully verified timestamped output did not replace the original file."
    Assert-Condition `
        ($script:requestEndpoint.AbsoluteUri -ceq "https://sign.necessary.nu/windows/sign") `
        "The detached digest was sent to an unexpected signing endpoint."
    Assert-Condition ($script:requestToken -ceq "secret-token") `
        "The signing token was not passed only to the HTTPS request helper."
    Assert-Condition ($signingResult.Thumbprint -ceq $script:mockThumbprint) `
        "The signing result did not return the verified signer thumbprint."

    $osslCommands = @(
        $script:nativeCalls |
            Where-Object { $_.FilePath -eq $toolPath } |
            ForEach-Object { $_.Arguments[0] }
    )
    Assert-Condition `
        (($osslCommands -join "|") -ceq "extract-data|attach-signature|add") `
        "The detached signing flow must extract, attach, and RFC 3161 timestamp in order."
    $timestampCall = $script:nativeCalls |
        Where-Object { $_.FilePath -eq $toolPath -and $_.Arguments[0] -eq "add" } |
        Select-Object -First 1
    Assert-Condition `
        (($timestampCall.Arguments -join "|").Contains("add|-h|sha256|-ts|http://timestamp.digicert.com")) `
        "The detached signature must receive an SHA-256 RFC 3161 timestamp."
    $signToolCall = $script:nativeCalls |
        Where-Object { $_.FilePath -eq $signToolPath } |
        Select-Object -First 1
    Assert-Condition `
        (($signToolCall.Arguments[0..4] -join "|") -ceq "verify|/pa|/all|/tw|/v") `
        "Windows SignTool must validate public Authenticode policy and timestamp before replacement."

    [System.IO.File]::WriteAllText($inputPath, "stable original")
    $script:mockSubject = "CN=Unexpected Publisher, O=Unexpected Publisher, C=US"
    Assert-Throws {
        Invoke-NecessaryAuthenticodeSigning `
            -File $inputPath `
            -OsslSignCodePath $toolPath `
            -SignToolPath $signToolPath `
            -SigningToken "secret-token" `
            -TimestampUrl "http://timestamp.digicert.com" `
            -TemporaryRoot $temporaryRoot
    } "Necessary Innovations AB"
    Assert-Condition `
        ((Get-Content -LiteralPath $inputPath -Raw) -eq "stable original") `
        "A wrong-publisher signature replaced the original file."

    $script:mockSubject = "CN=Necessary Innovations AB, O=Necessary Innovations AB, C=SE"
    $script:mockTimestamp = $null
    Assert-Throws {
        Invoke-NecessaryAuthenticodeSigning `
            -File $inputPath `
            -OsslSignCodePath $toolPath `
            -SignToolPath $signToolPath `
            -SigningToken "secret-token" `
            -TimestampUrl "http://timestamp.digicert.com" `
            -TemporaryRoot $temporaryRoot
    } "RFC 3161 timestamp"
    Assert-Condition `
        ((Get-Content -LiteralPath $inputPath -Raw) -eq "stable original") `
        "An untimestamped signature replaced the original file."

    $script:mockTimestamp = [pscustomobject]@{ Subject = "CN=RFC 3161 TSA" }
    Assert-Throws {
        Invoke-NecessaryAuthenticodeSigning `
            -File $inputPath `
            -OsslSignCodePath $toolPath `
            -SignToolPath $signToolPath `
            -SigningToken "secret-token" `
            -TimestampUrl "http://timestamp.digicert.com" `
            -TemporaryRoot $temporaryRoot `
            -ExpectedCertificateThumbprint "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
    } "certificate thumbprint"
    Assert-Condition `
        ((Get-Content -LiteralPath $inputPath -Raw) -eq "stable original") `
        "A signature from an unexpected certificate replaced the original file."
} finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}

Write-Host "Necessary Authenticode signing tests passed."
