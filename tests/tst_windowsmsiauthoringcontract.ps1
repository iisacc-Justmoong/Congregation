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

$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("Congregation-MsiAuthoringTest-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $temporaryRoot | Out-Null
try {
    $product100X64A = New-DeterministicProductCode -ProductVersion "1.0.0" -Architecture "x64"
    $product100X64B = New-DeterministicProductCode -ProductVersion "1.0.0" -Architecture "x64"
    $product101X64 = New-DeterministicProductCode -ProductVersion "1.0.1" -Architecture "x64"
    $product100Arm64 = New-DeterministicProductCode -ProductVersion "1.0.0" -Architecture "arm64"
    Assert-Condition ($product100X64A -eq $product100X64B) `
        "The same MSI version and architecture must produce the same ProductCode."
    Assert-Condition ($product100X64A -ne $product101X64) `
        "Different MSI versions must produce different ProductCodes."
    Assert-Condition ($product100X64A -ne $product100Arm64) `
        "Different MSI architectures must produce different ProductCodes."
    $parsedProductCode = [Guid]::Empty
    Assert-Condition ([Guid]::TryParse($product100X64A, [ref]$parsedProductCode)) `
        "The deterministic ProductCode must be a valid GUID."

    $productSourcePath = Join-Path $temporaryRoot "CongregationProduct.wxs"
    Write-MsiProductFile -OutputPath $productSourcePath -ProductVersion "1.0.0" -Architecture "x64"
    $productSource = [System.IO.File]::ReadAllText($productSourcePath)
    Assert-Condition ($productSource.Contains("<Product Id=`"$product100X64A`"")) `
        "The generated MSI source must embed the deterministic ProductCode."
    Assert-Condition (-not $productSource.Contains('<Product Id="*"')) `
        "The generated MSI source must not request a random ProductCode."
    Assert-Condition ($productSource.Contains('Installed OR REMOVE~="ALL" OR NOT (')) `
        "Maintenance and uninstall must bypass cross-context launch guards."
    Assert-Condition ($productSource.Contains('WIX_UPGRADE_DETECTED AND NOT (CONGREGATION_EXISTING_MACHINE_CONTEXT OR CONGREGATION_LEGACY_MACHINE_CONTEXT)')) `
        "A detected markerless legacy upgrade must remain in the current-user scope."
    $msiConditions = [regex]::Matches(
        $productSource,
        '(?s)<(?<tag>Publish|Condition|Custom)\b[^>]*>(?<condition>.*?)</\k<tag>>'
    )
    foreach ($msiCondition in $msiConditions) {
        Assert-Condition ($msiCondition.Groups['condition'].Value.Trim().Length -le 255) `
            "An MSI condition exceeds the 255-character database column limit."
    }

    $sourcePath = Join-Path $temporaryRoot "LICENSE"
    $outputPath = Join-Path $temporaryRoot "License.rtf"
    $hangul = ([string][char]0xD55C) + ([string][char]0xAE00)
    $emoji = [char]::ConvertFromUtf32(0x1F600)
    $sample = "ASCII {braces} \ slash`t$hangul $emoji`r`nsecond line"
    [System.IO.File]::WriteAllText(
        $sourcePath,
        $sample,
        (New-Object System.Text.UTF8Encoding($false))
    )

    Write-MsiLicenseRtf -SourcePath $sourcePath -OutputPath $outputPath
    $rtf = [System.IO.File]::ReadAllText($outputPath, [System.Text.Encoding]::ASCII)

    Assert-Condition $rtf.StartsWith("{\rtf1") "The generated license is not an RTF document."
    Assert-Condition $rtf.Contains("\{braces\}") "RTF braces were not escaped."
    Assert-Condition $rtf.Contains("\\ slash") "RTF backslashes were not escaped."
    Assert-Condition $rtf.Contains("\tab ") "RTF tabs were not encoded."
    Assert-Condition ([regex]::IsMatch($rtf, '\\u-?\d+\?')) "RTF Unicode code units were not encoded."

    Add-Type -AssemblyName System.Windows.Forms
    $richTextBox = New-Object System.Windows.Forms.RichTextBox
    try {
        $richTextBox.Rtf = $rtf
        $rendered = $richTextBox.Text
    } finally {
        $richTextBox.Dispose()
    }

    $normalizedExpected = $sample.Replace("`r`n", "`n").Replace("`r", "`n")
    $normalizedRendered = $rendered.Replace("`r`n", "`n").Replace("`r", "`n")
    Assert-Condition ($normalizedRendered -eq $normalizedExpected) `
        "The generated RTF did not round-trip through the Windows RichEdit control."

    $harvestPath = Join-Path $temporaryRoot "CongregationRuntime.wxs"
    $harvestSource = @'
<Wix>
  <Component Id="CongregationExecutableComponent">
    <File Id="CongregationExecutableFile" KeyPath="yes" Source="$(var.StageDir)\Congregation.exe" />
  </Component>
  <Component Id="OtherComponent">
    <File Id="OtherFile" KeyPath="yes" Source="$(var.StageDir)\Other.dll" />
  </Component>
</Wix>
'@
    [System.IO.File]::WriteAllText(
        $harvestPath,
        $harvestSource,
        (New-Object System.Text.UTF8Encoding($false))
    )
    Add-AdvertisedStartMenuShortcut -HarvestPath $harvestPath
    $transformedHarvest = [System.IO.File]::ReadAllText($harvestPath)
    Assert-Condition ($transformedHarvest.Contains('<File Id="CongregationExecutableFile" KeyPath="yes" Source="$(var.StageDir)\Congregation.exe">')) `
        "The Congregation.exe file row was not converted into a shortcut owner."
    Assert-Condition ($transformedHarvest.Contains('Shortcut Id="ApplicationStartMenuShortcut"')) `
        "The harvested Congregation.exe component does not own the Start Menu shortcut."
    Assert-Condition ($transformedHarvest.Contains('Directory="ApplicationProgramsFolder"')) `
        "The shortcut does not use the context-aware Program Menu directory."
    Assert-Condition ($transformedHarvest.Contains('Advertise="yes"')) `
        "The shortcut must be advertised so dual-context ICE validation remains sound."
    Assert-Condition ($transformedHarvest.Contains('Source="$(var.StageDir)\Other.dll" />')) `
        "The shortcut transform modified an unrelated harvested file."
} finally {
    Remove-Item -LiteralPath $temporaryRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "Windows MSI authoring contract tests passed."
