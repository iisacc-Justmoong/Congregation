#Requires -Version 5.1
param(
    [Parameter(Mandatory = $true)]
    [string]$ScriptPath,
    [Parameter(Mandatory = $true)]
    [string]$ManifestTemplatePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Assert-Condition {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw $Message
    }
}

function Assert-Throws {
    param([scriptblock]$Action, [string]$ExpectedText)
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
$scriptSource = Get-Content -LiteralPath $resolvedScriptPath -Raw
Assert-Condition ($scriptSource.Contains('[ValidateSet("Store", "Development")]')) "The MSIX script must separate Store and Development modes."
Assert-Condition ($scriptSource.Contains('CONGREGATION_STORE_IDENTITY_NAME')) "The Store identity name environment contract is missing."
Assert-Condition ($scriptSource.Contains('CONGREGATION_STORE_DISPLAY_NAME')) "The reserved Store display-name environment contract is missing."
Assert-Condition ($scriptSource.Contains('CONGREGATION_STORE_PUBLISHER_DISPLAY_NAME')) "The Store publisher display-name environment contract is missing."
Assert-Condition ($scriptSource.Contains('CONGREGATION_DEVELOPMENT_SIGNING_CERTIFICATE_THUMBPRINT')) "The development certificate selector is missing."
Assert-Condition ($scriptSource.Contains('LocalMachine\TrustedPeople')) "Development MSIX installation must require machine TrustedPeople trust."
Assert-Condition ($scriptSource.Contains('Store mode does not allow -SkipBuild')) "Store output must require a current build and tests."
Assert-Condition ($scriptSource.Contains('Congregation-$Version-Windows-Store-x64')) "The canonical Store artifact name is missing."
Assert-Condition ($scriptSource.Contains('$DistDirectory = Join-Path $RepositoryRoot "dist"')) "Store release artifacts must use the repository dist directory."
Assert-Condition ($scriptSource.Contains('$outputDirectory = if ($Mode -eq "Store") { $DistDirectory } else { $developmentOutputDirectory }')) "Store release artifacts must be published to dist instead of build."
Assert-Condition ($scriptSource.Contains('intentionally unsigned')) "The Store upload must document that it remains unsigned before Store ingestion."
Assert-Condition ($scriptSource.Contains('legal\iiSharedCanvas\LICENSE.txt')) "Store packaging must require the iiSharedCanvas license."

$manifestTemplateSource = Get-Content -LiteralPath $ManifestTemplatePath -Raw
Assert-Condition ($manifestTemplateSource.Contains('ProcessorArchitecture="x64"')) "The Store manifest must declare native x64 architecture."
Assert-Condition ($manifestTemplateSource.Contains('MinVersion="10.0.19041.0"')) "The Store manifest must target the uap10 packaged desktop baseline."
Assert-Condition ($manifestTemplateSource.Contains('IgnorableNamespaces="uap uap10 rescap"')) "The Store manifest namespaces are incomplete."
Assert-Condition ($manifestTemplateSource.Contains('<Capability Name="privateNetworkClientServer" />')) "The Store manifest must declare private local-network access."
Assert-Condition (([regex]::Matches($manifestTemplateSource, '@DISPLAY_NAME@')).Count -eq 2) "The reserved Store display name must drive both package and application display names."
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    $resolvedScriptPath,
    [ref]$tokens,
    [ref]$parseErrors
)
Assert-Condition ($parseErrors.Count -eq 0) "build-windows-store.ps1 contains a PowerShell parse error."
foreach ($functionAst in $ast.FindAll(
        { param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] },
        $true
    )) {
    . ([scriptblock]::Create($functionAst.Extent.Text))
}

$thumbprint = "0123456789ABCDEF0123456789ABCDEF01234567"
Assert-Throws {
    Assert-StorePackagePolicy -Mode Store -IdentityName "" -Publisher "CN=Store Publisher" -DisplayName "Congregation" -PublisherDisplayName "Store Publisher" -PackageVersion "1.0.0.0" -DevelopmentCertificateThumbprint ""
} "Package/Identity/Name"
Assert-Throws {
    Assert-StorePackagePolicy -Mode Store -IdentityName "IISACC.Congregation" -Publisher "" -DisplayName "Congregation" -PublisherDisplayName "Store Publisher" -PackageVersion "1.0.0.0" -DevelopmentCertificateThumbprint ""
} "Package/Identity/Publisher"
Assert-Throws {
    Assert-StorePackagePolicy -Mode Store -IdentityName "IISACC.Congregation" -Publisher "CN=Store Publisher" -DisplayName "" -PublisherDisplayName "Store Publisher" -PackageVersion "1.0.0.0" -DevelopmentCertificateThumbprint ""
} "Package/Properties/DisplayName"
Assert-Throws {
    Assert-StorePackagePolicy -Mode Store -IdentityName "IISACC.Congregation" -Publisher "CN=Store Publisher" -DisplayName "Congregation" -PublisherDisplayName "" -PackageVersion "1.0.0.0" -DevelopmentCertificateThumbprint ""
} "PublisherDisplayName"
Assert-Throws {
    Assert-StorePackagePolicy -Mode Store -IdentityName "IISACC.Congregation" -Publisher "CN=Store Publisher" -DisplayName "Congregation" -PublisherDisplayName "Store Publisher" -PackageVersion "1.0.0.1" -DevelopmentCertificateThumbprint ""
} "fourth version field"
Assert-Throws {
    Assert-StorePackagePolicy -Mode Development -IdentityName "IISACC.Congregation.Development" -Publisher "CN=Congregation Development Local Only" -DisplayName "Congregation Development" -PublisherDisplayName "IISACC Development" -PackageVersion "1.0.0.0" -DevelopmentCertificateThumbprint "1234"
} "40-hex"
Assert-StorePackagePolicy -Mode Store -IdentityName "IISACC.Congregation" -Publisher "CN=Store Publisher" -DisplayName "Congregation" -PublisherDisplayName "Store Publisher" -PackageVersion "1.0.0.0" -DevelopmentCertificateThumbprint ""
Assert-StorePackagePolicy -Mode Development -IdentityName "IISACC.Congregation.Development" -Publisher "CN=Congregation Development Local Only" -DisplayName "Congregation Development" -PublisherDisplayName "IISACC Development" -PackageVersion "1.0.0.0" -DevelopmentCertificateThumbprint $thumbprint

$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("Congregation-StorePackageTest-" + [Guid]::NewGuid().ToString("N"))
$contentRoot = Join-Path $temporaryRoot "content"
$manifestPath = Join-Path $contentRoot "AppxManifest.xml"
$packagePath = Join-Path $temporaryRoot "Congregation.msix"
$uploadPath = Join-Path $temporaryRoot "Congregation.msixupload"
try {
    New-Item -ItemType Directory -Path (Join-Path $contentRoot "Assets") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $contentRoot "legal") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $contentRoot "legal\QtKeychain") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $contentRoot "legal\iiLicenseManager") -Force | Out-Null

    Write-StoreAppxManifest `
        -TemplatePath $ManifestTemplatePath `
        -OutputPath $manifestPath `
        -IdentityName "IISACC.Congregation" `
        -Publisher "CN=IISACC & Co" `
        -DisplayName "Congregation & Paint" `
        -PublisherDisplayName "IISACC & Co" `
        -PackageVersion "1.0.0.0"

    [xml]$manifest = Get-Content -LiteralPath $manifestPath -Raw
    $namespaceManager = New-Object System.Xml.XmlNamespaceManager($manifest.NameTable)
    $namespaceManager.AddNamespace("f", "http://schemas.microsoft.com/appx/manifest/foundation/windows10")
    $namespaceManager.AddNamespace("uap", "http://schemas.microsoft.com/appx/manifest/uap/windows10")
    $namespaceManager.AddNamespace("uap10", "http://schemas.microsoft.com/appx/manifest/uap/windows10/10")
    $namespaceManager.AddNamespace("rescap", "http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities")
    $identity = $manifest.SelectSingleNode("/f:Package/f:Identity", $namespaceManager)
    Assert-Condition ($identity.Name -eq "IISACC.Congregation") "The Store identity name was not written exactly."
    Assert-Condition ($identity.Publisher -eq "CN=IISACC & Co") "The Store publisher was not XML escaped and restored exactly."
    Assert-Condition ($identity.Version -eq "1.0.0.0") "The Store package version changed."
    $packageDisplayName = $manifest.SelectSingleNode("/f:Package/f:Properties/f:DisplayName", $namespaceManager)
    Assert-Condition ($packageDisplayName.InnerText -eq "Congregation & Paint") "The reserved package display name was not written exactly."
    $application = $manifest.SelectSingleNode("/f:Package/f:Applications/f:Application", $namespaceManager)
    Assert-Condition ($application.GetAttribute("RuntimeBehavior", "http://schemas.microsoft.com/appx/manifest/uap/windows10/10") -eq "packagedClassicApp") "The manifest must declare packagedClassicApp."
    Assert-Condition ($application.GetAttribute("TrustLevel", "http://schemas.microsoft.com/appx/manifest/uap/windows10/10") -eq "mediumIL") "The manifest must declare mediumIL."
    $visualElements = $manifest.SelectSingleNode("/f:Package/f:Applications/f:Application/uap:VisualElements", $namespaceManager)
    Assert-Condition ($visualElements.GetAttribute("DisplayName") -eq "Congregation & Paint") "The application display name must match the reserved package display name."
    Assert-Condition ($null -ne $manifest.SelectSingleNode("/f:Package/f:Capabilities/rescap:Capability[@Name='runFullTrust']", $namespaceManager)) "The manifest must declare runFullTrust."
    Assert-Condition ($null -ne $manifest.SelectSingleNode("/f:Package/f:Capabilities/f:Capability[@Name='privateNetworkClientServer']", $namespaceManager)) "The manifest must declare privateNetworkClientServer."

    foreach ($relativePath in @(
            "Congregation.exe",
            "LICENSE.txt",
            "THIRD_PARTY_NOTICES.txt",
            "SOURCE_OFFER.txt",
            "legal\iiLicenseManager\THIRD_PARTY_NOTICES.md",
            "legal\QtKeychain\COPYING.txt",
            "Assets\StoreLogo.png",
            "Assets\Square44x44Logo.png",
            "Assets\Square150x150Logo.png"
        )) {
        [System.IO.File]::WriteAllText((Join-Path $contentRoot $relativePath), "fixture", [System.Text.Encoding]::UTF8)
    }
    Assert-StorePackageContent -Directory $contentRoot -StoreSubmission $false
    Assert-Throws {
        Assert-StorePackageContent -Directory $contentRoot -StoreSubmission $true
    } "iiPaintEngine LICENSE"
    New-Item -ItemType Directory -Path (Join-Path $contentRoot "legal\iiPaintEngine") -Force | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $contentRoot "legal\iiPaintEngine\LICENSE.txt"), "license", [System.Text.Encoding]::UTF8)
    Assert-Throws {
        Assert-StorePackageContent -Directory $contentRoot -StoreSubmission $true
    } "iiSharedCanvas LICENSE"
    New-Item -ItemType Directory -Path (Join-Path $contentRoot "legal\iiSharedCanvas") -Force | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $contentRoot "legal\iiSharedCanvas\LICENSE.txt"), "license", [System.Text.Encoding]::UTF8)
    [System.IO.File]::WriteAllText((Join-Path $contentRoot "SOURCE_OFFER.txt"), "URL: https://example.invalid/source.zip`r`nSHA-256: $([string]'A' * 64)`r`n", [System.Text.Encoding]::UTF8)
    Assert-StorePackageContent -Directory $contentRoot -StoreSubmission $true

    [System.IO.File]::WriteAllText((Join-Path $contentRoot "Qt6InsightTracker.dll"), "forbidden", [System.Text.Encoding]::UTF8)
    Assert-Throws {
        Assert-StorePackageContent -Directory $contentRoot -StoreSubmission $false
    } "forbidden payload"
    Remove-Item -LiteralPath (Join-Path $contentRoot "Qt6InsightTracker.dll") -Force

    [System.IO.File]::WriteAllText($packagePath, "msix fixture", [System.Text.Encoding]::UTF8)
    Write-StoreUploadArchive -PackagePath $packagePath -OutputPath $uploadPath
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [System.IO.Compression.ZipFile]::OpenRead($uploadPath)
    try {
        Assert-Condition ($archive.Entries.Count -eq 1) "The MSIX upload archive must contain exactly one package without symbols."
        Assert-Condition ($archive.Entries[0].FullName -eq "Congregation.msix") "The MSIX upload archive contains the wrong entry name."
    } finally {
        $archive.Dispose()
    }
} finally {
    Remove-Item -LiteralPath $temporaryRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "Windows Store MSIX package contract tests passed."
