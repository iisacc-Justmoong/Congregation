#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QtTest>

namespace
{
QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    return QString::fromUtf8(file.readAll());
}
}

class tst_WindowsBuildWorkflowContract : public QObject
{
    Q_OBJECT

private slots:
    void windowsBuildScriptDefinesRunnablePackageContract();
    void windowsMsiDefinitionProvidesInstallOptions();
    void windowsBuildScriptDefinesAuthenticodeContract();
    void cmakeBuildsWindowsGuiExecutable();
    void windowsResourcesDefineNativeApplicationContract();
    void cmakeHasWindowsInstallAndPackageRules();
    void appEntryPointAvoidsNonExportedLvrsRuntimeSymbols();
    void appEntryPointShowsFinalGeometryOnlyOnce();
    void correspondingSourceToolDefinesImmutableReleaseContract();
    void windowsWorkflowsAuthenticatePinnedPrivateManagers();
    void signPathWorkflowDefinesFreeWebsiteReleaseContract();
    void buildGuideDocumentsWindowsScript();
    void windowsSdkPathsUseRelocatedInstallRoot();
};

void tst_WindowsBuildWorkflowContract::windowsSdkPathsUseRelocatedInstallRoot()
{
    const QStringList paths = {
        QFINDTESTDATA("../build-windows.ps1"),
        QFINDTESTDATA("../docs/BUILD.md"),
        QFINDTESTDATA("../.github/workflows/windows-signpath-release.yml"),
        QFINDTESTDATA("../.github/workflows/windows-necessary-release.yml"),
    };
    const QStringList packages = {
        QStringLiteral("LVRS"), QStringLiteral("iiPaintEngine"),
        QStringLiteral("iiSharedCanvas"), QStringLiteral("iiUpdateManager"),
        QStringLiteral("iiLicenseManager"),
    };

    for (const QString &path : paths) {
        QVERIFY2(!path.isEmpty(), "Windows SDK path contract input was not found");
        QString source = readTextFile(path);
        QVERIFY(!source.isEmpty());
        source.replace(QLatin1Char('\\'), QLatin1Char('/'));
        QVERIFY2(source.contains(QStringLiteral(".local/SDK/LVRS")), qPrintable(path));
        for (const QString &package : packages) {
            const QString legacyPrefix = QStringLiteral(".local/") + package;
            QVERIFY2(!source.contains(legacyPrefix), qPrintable(path + ": " + legacyPrefix));
        }
    }
}

void tst_WindowsBuildWorkflowContract::correspondingSourceToolDefinesImmutableReleaseContract()
{
    const QString scriptPath = QFINDTESTDATA("../tools/new-corresponding-source.ps1");
    QVERIFY2(!scriptPath.isEmpty(), "tools/new-corresponding-source.ps1 test data was not found");
    const QString source = readTextFile(scriptPath);
    QVERIFY(!source.isEmpty());

    QVERIFY(source.contains(QStringLiteral("#Requires -Version 5.1")));
    QVERIFY(source.contains(QStringLiteral("[ValidatePattern(\"^\\d+\\.\\d+(?:\\.\\d+)?$\")]")));
    QVERIFY(source.contains(QStringLiteral("Join-Path $RepositoryRoot \"build\"")));
    QVERIFY(source.contains(QStringLiteral("Join-Path $buildRoot \"release-source\"")));
    QVERIFY(source.contains(QStringLiteral("Assert-PathWithinRoot")));
    QVERIFY(source.contains(QStringLiteral("git status --porcelain")));
    QVERIFY(source.contains(QStringLiteral("git archive --format=zip")));
    QVERIFY(source.contains(QStringLiteral(
        "throw \"Congregation release revision does not declare version $Version.\"")));
    QVERIFY(source.contains(QStringLiteral("8132a48e4c0788f0548bff20fd7089878a09c760")));
    QVERIFY(source.contains(QStringLiteral("5ade15e24ce3a7eab25669144fdee668672b22d6")));
    QVERIFY(source.contains(QStringLiteral("d2c41bf605725eb07b181587321f0ce910b9924d")));
    QVERIFY(source.contains(QStringLiteral("f51449543273cbf12058ae92b230e0c4209f5066")));
    QVERIFY(source.contains(QStringLiteral("qtbase")));
    QVERIFY(source.contains(QStringLiteral("qtdeclarative")));
    QVERIFY(source.contains(QStringLiteral("qtsvg")));
    QVERIFY(source.contains(QStringLiteral("qtimageformats")));
    QVERIFY(source.contains(QStringLiteral("qttranslations")));
    QVERIFY(source.contains(QStringLiteral("ARCHIVE-CONTENTS-SHA256.txt")));
    QVERIFY(source.contains(QStringLiteral("tar.exe")));
    QVERIFY(source.contains(QStringLiteral("MissingRequired")));
    QVERIFY(source.contains(QStringLiteral("(^|/)\\.git(/|$)")));
    QVERIFY(!source.contains(QStringLiteral("(^|/)build(/|$)")));
    QVERIFY(source.contains(QStringLiteral("$buildGuideTemplate = @'")));
    QVERIFY(source.contains(QStringLiteral(
        "$buildGuide = $buildGuideTemplate.Replace(\"@@VERSION@@\", $Version)")));
    QVERIFY(source.contains(QStringLiteral(
        "From `Congregation`, configure, build, and test using the repository-local `build/` directory.")));

    const QString buildGuidePath = QFINDTESTDATA("../docs/BUILD.md");
    QVERIFY2(!buildGuidePath.isEmpty(), "docs/BUILD.md test data was not found");
    const QString buildGuide = readTextFile(buildGuidePath);
    QVERIFY(buildGuide.contains(QStringLiteral(
        ".\\tools\\new-corresponding-source.ps1 -Version 1.0.0 -CongregationRevision v1.0.0")));
}

void tst_WindowsBuildWorkflowContract::windowsWorkflowsAuthenticatePinnedPrivateManagers()
{
    const QStringList workflowPaths = {
        QFINDTESTDATA("../.github/workflows/windows-corresponding-source.yml"),
        QFINDTESTDATA("../.github/workflows/windows-signpath-release.yml"),
    };

    for (const QString &workflowPath : workflowPaths) {
        QVERIFY2(!workflowPath.isEmpty(), "A Windows release workflow was not found");
        const QString workflow = readTextFile(workflowPath);
        QVERIFY(!workflow.isEmpty());
        QVERIFY(workflow.contains(QStringLiteral("name: Check out pinned iiUpdateManager")));
        QVERIFY(workflow.contains(QStringLiteral("name: Check out pinned iiLicenseManager")));
        QVERIFY(workflow.contains(QStringLiteral("repository: iisacc-Justmoong/iiLicenseManager")));
        QVERIFY(workflow.contains(QStringLiteral("token: ${{ secrets.IIUPDATEMANAGER_READ_TOKEN }}")));
        QVERIFY(workflow.contains(QStringLiteral("path: _private/iiLicenseManager")));
        QVERIFY(workflow.contains(QStringLiteral("persist-credentials: false")));
        QVERIFY(workflow.contains(QStringLiteral(
            "if ($resolvedLicenseManagerCommit -cne $env:IILICENSEMANAGER_COMMIT.ToLowerInvariant())")));
        QVERIFY(!workflow.contains(QStringLiteral(
            "git clone --filter=blob:none https://github.com/iisacc-Justmoong/iiLicenseManager.git")));
    }
}

void tst_WindowsBuildWorkflowContract::windowsBuildScriptDefinesRunnablePackageContract()
{
    const QString scriptPath = QFINDTESTDATA("../build-windows.ps1");
    QVERIFY2(!scriptPath.isEmpty(), "build-windows.ps1 test data was not found");
    const QString source = readTextFile(scriptPath);
    QVERIFY(!source.isEmpty());

    const QString gitignorePath = QFINDTESTDATA("../.gitignore");
    QVERIFY2(!gitignorePath.isEmpty(), ".gitignore test data was not found");
    const QString gitignoreSource = readTextFile(gitignorePath);
    QVERIFY(!gitignoreSource.contains(QStringLiteral("\nbuild-windows.ps1\n")));
    QVERIFY(!gitignoreSource.contains(QStringLiteral("\n/build-windows.ps1\n")));
    QVERIFY(gitignoreSource.contains(QStringLiteral("!/packaging/windows/**")));

    QVERIFY(source.contains(QStringLiteral("#Requires -Version 5.1")));
    QVERIFY(source.contains(QStringLiteral("Set-StrictMode -Version Latest")));
    QVERIFY(source.contains(QStringLiteral("$Version = \"1.0.0\"")));
    QVERIFY(source.contains(QStringLiteral("$BuildDir = Join-Path $RepositoryRoot \"build\"")));
    QVERIFY(source.contains(QStringLiteral("$SignedMsiPath = Join-Path $BuildDir \"Congregation-$Version-Windows.msi\"")));
    QVERIFY(!source.contains(QStringLiteral("cmake-build-debug")));

    QVERIFY(source.contains(QStringLiteral("$env:QT_PREFIX")));
    QVERIFY(source.contains(QStringLiteral("$env:LVRS_PREFIX")));
    QVERIFY(source.contains(QStringLiteral("$env:IIPAINTENGINE_PREFIX")));
    QVERIFY(source.contains(QStringLiteral("Resolve-QtPrefix")));
    QVERIFY(source.contains(QStringLiteral("Resolve-Objdump")));
    QVERIFY(source.contains(QStringLiteral("Assert-MinGwRuntimeCompatibility")));
    QVERIFY(source.contains(QStringLiteral("Import-VisualStudioEnvironment")));
    QVERIFY(source.contains(QStringLiteral("windeployqt.exe")));
    QVERIFY(source.contains(QStringLiteral("--no-system-dxc-compiler")));

    QVERIFY(source.contains(QStringLiteral("Get-RequiredCommand \"cmake.exe\"")));
    QVERIFY(source.contains(QStringLiteral("Get-RequiredCommand \"ninja.exe\"")));
    QVERIFY(source.contains(QStringLiteral("Get-RequiredCommand \"ctest.exe\"")));
    QVERIFY(source.contains(QStringLiteral("[switch]$CreateMsi")));
    QVERIFY(source.contains(QStringLiteral("Resolve-WixTools")));
    QVERIFY(source.contains(QStringLiteral("New-MsiInstaller")));
    QVERIFY(source.contains(QStringLiteral("Write-MsiProductFile")));
    QVERIFY(source.contains(QStringLiteral("Write-MsiLicenseRtf")));
    QVERIFY(source.contains(QStringLiteral("WixUILicenseRtf")));
    QVERIFY(source.contains(QStringLiteral("Join-Path $RepositoryRoot \"LICENSE\"")));
    QVERIFY(!source.contains(QStringLiteral("AllowSameVersionUpgrades")));
    QVERIFY(source.contains(QStringLiteral("Schedule=\"afterInstallInitialize\"")));
    QVERIFY(!source.contains(QStringLiteral("InstallScope=\"perMachine\"")));
    QVERIFY(source.contains(QStringLiteral("ProgramFiles64Folder")));
    QVERIFY(source.contains(QStringLiteral("ProgramMenuFolder")));
    QVERIFY(source.contains(QStringLiteral("ApplicationProgramsFolder")));
    QVERIFY(source.contains(QStringLiteral("ApplicationStartMenuShortcut")));
    QVERIFY(source.contains(QStringLiteral("Root=\"HKCU\"")));
    QVERIFY(source.contains(QStringLiteral("Root=\"HKLM\"")));
    QVERIFY(source.contains(QStringLiteral("ApplicationFolderName")));
    QVERIFY(source.contains(QStringLiteral("WixAppFolder")));
    QVERIFY(source.contains(QStringLiteral("WixPerUserFolder")));
    QVERIFY(source.contains(QStringLiteral("ARPINSTALLLOCATION")));
    QVERIFY(!source.contains(QStringLiteral("-sice:")));
    QVERIFY(source.contains(QStringLiteral("Invoke-Native $heat @(\"dir\", $SourceDirectory, \"-wx\"")));
    QVERIFY(source.contains(QStringLiteral("Invoke-Native $candle @(\"-wx\"")));
    QVERIFY(source.contains(QStringLiteral("Invoke-Native $light @(\"-wx\"")));
    QVERIFY(source.contains(QStringLiteral("\"-ag\"")));
    QVERIFY(!source.contains(QStringLiteral("\"-gg\"")));
    QVERIFY(source.contains(QStringLiteral("\"-S\", $RepositoryRoot")));
    QVERIFY(source.contains(QStringLiteral("\"-B\", $BuildDir")));
    QVERIFY(source.contains(QStringLiteral("\"-DCMAKE_PREFIX_PATH=$prefixPath\"")));
    QVERIFY(source.contains(QStringLiteral("\"--parallel\"")));
    QVERIFY(source.contains(QStringLiteral("Invoke-Native $CTest @(\"--test-dir\", $BuildDir, \"--output-on-failure\"")));
    QVERIFY(source.contains(QStringLiteral("\"--qmldir\", (Join-Path $RepositoryRoot \"App\\qml\")")));
    QVERIFY(source.contains(QStringLiteral("Copy-DependencyRuntimeFiles -Name \"LVRS\"")));
    QVERIFY(source.contains(QStringLiteral("Copy-DependencyRuntimeFiles -Name \"iiPaintEngine\"")));
    QVERIFY(!source.contains(QStringLiteral("Copy-DependencyQmlImports -Name \"LVRS\"")));
    QVERIFY(!source.contains(QStringLiteral("Remove-QmlResourcePreferDirectives -ModuleName \"LVRS\"")));
    QVERIFY(source.contains(QStringLiteral("Remove-EmbeddedDependencyQmlImport -ModuleName \"LVRS\"")));
    QVERIFY(source.contains(QStringLiteral("Remove-ReleaseOnlyQtArtifacts -Directory $StageDir -BuildType $BuildType")));
    QVERIFY(source.contains(QStringLiteral("\"qmltooling\"")));
    QVERIFY(source.contains(QStringLiteral("\"generic\"")));
    QVERIFY(source.contains(QStringLiteral("\"sqldrivers\"")));
    QVERIFY(source.contains(QStringLiteral("\"Qt6InsightTracker.dll\"")));
    QVERIFY(source.contains(QStringLiteral("\"Qt6Sql.dll\"")));
    QVERIFY(source.contains(QStringLiteral("\"Qt6Quick3DUtils.dll\"")));
    QVERIFY(source.contains(QStringLiteral("\"imageformats\\qpdf.dll\"")));
    QVERIFY(source.contains(QStringLiteral("\"qml\\QtQuick\\VirtualKeyboard\"")));
    QVERIFY(!source.contains(QStringLiteral("\"sqldrivers\\qsqlpsql.dll\"")));
    QVERIFY(!source.contains(QStringLiteral("\"sqldrivers\\qsqlmimer.dll\"")));
    QVERIFY(source.contains(QStringLiteral("Get-PeSubsystem")));
    QVERIFY(source.contains(QStringLiteral("Assert-StagedPeImportClosure")));
    QVERIFY(source.contains(QStringLiteral("DLL Name:")));
    QVERIFY(source.contains(QStringLiteral("SpecialFolder]::System")));
    QVERIFY(source.contains(QStringLiteral("Windows GUI subsystem")));
    QVERIFY(source.contains(QStringLiteral("[System.Diagnostics.FileVersionInfo]::GetVersionInfo")));
    QVERIFY(source.contains(QStringLiteral("$_.VersionInfo.IsDebug")));
    QVERIFY(!source.contains(QStringLiteral("-Filter \"Qt6*d.dll\"")));
    QVERIFY(source.contains(QStringLiteral("Resolve-Strip")));
    QVERIFY(source.contains(QStringLiteral("Strip-WindowsRuntimeBinaries")));
    QVERIFY(source.contains(QStringLiteral("--strip-all")));
    QVERIFY(source.contains(QStringLiteral("\"--translations\", \"en,ko\"")));
    QVERIFY(source.contains(QStringLiteral("\"--skip-plugin-types\", \"qmltooling,generic,sqldrivers\"")));
    QVERIFY(source.contains(QStringLiteral("\"--no-system-d3d-compiler\"")));
    QVERIFY(source.contains(QStringLiteral("\"--no-opengl-sw\"")));
    QVERIFY(source.contains(QStringLiteral("\"--exclude-plugins\", \"qpdf,qtvirtualkeyboardplugin\"")));
    QVERIFY(source.contains(QStringLiteral("\"--verbose\", \"0\"")));
    QVERIFY(source.contains(QStringLiteral("Verify-WindowsStage -Directory $StageDir -ResolvedQtPrefix $QtPrefix -ExpectedFileVersion $WindowsFileVersion -BuildType $BuildType")));
    QVERIFY(source.contains(QStringLiteral("qml\\QtQuick\\Shapes\\qmlshapesplugin.dll")));
    QVERIFY(source.contains(QStringLiteral("CONGREGATION_CORRESPONDING_SOURCE_URL")));
    QVERIFY(source.contains(QStringLiteral("CONGREGATION_CORRESPONDING_SOURCE_SHA256")));
    QVERIFY(source.contains(QStringLiteral("Resolve-DependencyLicenseFile")));
    QVERIFY(source.contains(QStringLiteral("Assert-PublicDistributionEvidence")));
    QVERIFY(source.contains(QStringLiteral("Copy-WindowsLegalMaterials")));
    QVERIFY(source.contains(QStringLiteral("Assert-WindowsLegalMaterials")));
    QVERIFY(source.contains(QStringLiteral("Resolve-QtGlobalLicenseDirectory")));
    QVERIFY(source.contains(QStringLiteral("qtbase\\LICENSES")));
    QVERIFY(source.contains(QStringLiteral("Qt-GPL-exception-1.0.txt")));
    QVERIFY(source.contains(QStringLiteral("Resolve-StagedMinGwToolchainRoot")));
    QVERIFY(source.contains(QStringLiteral("Public Windows packaging requires an explicit iiPaintEngine LICENSE")));
    QVERIFY(source.contains(QStringLiteral("qtimageformats")));
    QVERIFY(source.contains(QStringLiteral("COPYING.RUNTIME")));
    QVERIFY(source.contains(QStringLiteral("COPYING.MinGW-w64-runtime.txt")));
    QVERIFY(source.contains(QStringLiteral("winpthreads\\COPYING")));

    const QString noticesPath = QFINDTESTDATA("../packaging/windows/THIRD_PARTY_NOTICES.txt");
    QVERIFY2(!noticesPath.isEmpty(), "Windows third-party notices were not found");
    const QString notices = readTextFile(noticesPath);
    QVERIFY(notices.contains(QStringLiteral("iiPaintEngine")));
    QVERIFY(notices.contains(QStringLiteral("AGPL-3.0-only")));
    QVERIFY(notices.contains(QStringLiteral("legal/iiPaintEngine/LICENSE.txt")));
    QVERIFY(notices.contains(QStringLiteral("psd_sdk")));
    QVERIFY(notices.contains(QStringLiteral("Pretendard 1.3.9")));
    QVERIFY(notices.contains(QStringLiteral("Qt 6.8.3")));
    QVERIFY(notices.contains(QStringLiteral("GCC / MinGW-w64")));

    const QString pretendardLicensePath = QFINDTESTDATA("../packaging/windows/licenses/Pretendard-OFL-1.1.txt");
    QVERIFY2(!pretendardLicensePath.isEmpty(), "Pretendard OFL license was not found");
    QVERIFY(readTextFile(pretendardLicensePath).contains(QStringLiteral("SIL OPEN FONT LICENSE Version 1.1")));

    const QString psdLicensePath = QFINDTESTDATA("../packaging/windows/licenses/psd_sdk-BSD-2-Clause.txt");
    QVERIFY2(!psdLicensePath.isEmpty(), "psd_sdk BSD license was not found");
    QVERIFY(readTextFile(psdLicensePath).contains(QStringLiteral("BSD 2-Clause License")));

    const QString minizLicensePath = QFINDTESTDATA("../packaging/windows/licenses/psd_sdk-miniz-Unlicense.txt");
    QVERIFY2(!minizLicensePath.isEmpty(), "psd_sdk miniz Unlicense was not found");
    QVERIFY(readTextFile(minizLicensePath).contains(QStringLiteral("free and unencumbered software")));
    QVERIFY(source.contains(QStringLiteral("the Qt Quick Shapes QML plugin is missing")));
    QVERIFY(source.contains(QStringLiteral("libstdc++-6.dll")));
    QVERIFY(source.contains(QStringLiteral("__cxa_thread_atexit")));
    QVERIFY(source.contains(QStringLiteral("Rebuild those dependencies with the same MinGW kit as Qt")));
    QVERIFY(source.contains(QStringLiteral("Compress-Archive")));
    QVERIFY(source.contains(QStringLiteral("Congregation-$Version-Windows.zip")));
    QVERIFY(source.contains(QStringLiteral("Congregation-$Version-Windows.msi")));
    QVERIFY(source.contains(QStringLiteral("Remove-NonAsciiHarvestedFiles")));
    QVERIFY(source.contains(QStringLiteral("Install-ForCurrentUser")));
    QVERIFY(source.contains(QStringLiteral("Resolve-SafeCurrentUserInstallDirectory")));
    QVERIFY(source.contains(QStringLiteral("[Environment+SpecialFolder]::LocalApplicationData")));
    QVERIFY(source.contains(QStringLiteral(".congregation-install-root")));
    QVERIFY(source.contains(QStringLiteral("VersionInfo.ProductName -eq \"Congregation\"")));
    QVERIFY(source.contains(QStringLiteral("Remove-Item -LiteralPath $TargetDirectory -Recurse -Force")));
}

void tst_WindowsBuildWorkflowContract::windowsMsiDefinitionProvidesInstallOptions()
{
    const QString scriptPath = QFINDTESTDATA("../build-windows.ps1");
    QVERIFY2(!scriptPath.isEmpty(), "build-windows.ps1 test data was not found");
    const QString source = readTextFile(scriptPath);
    QVERIFY(!source.isEmpty());

    QVERIFY(source.contains(QStringLiteral("<UIRef Id=\"WixUI_Advanced\" />")));
    QVERIFY(!source.contains(QStringLiteral("<UIRef Id=\"WixUI_FeatureTree\" />")));
    QVERIFY(!source.contains(QStringLiteral("<UIRef Id=\"WixUI_InstallDir\" />")));
    QVERIFY(!source.contains(QStringLiteral("ARPNOMODIFY")));
    QVERIFY(source.contains(QStringLiteral("<Property Id=\"ApplicationFolderName\" Value=\"Congregation\" />")));
    QVERIFY(source.contains(QStringLiteral("<Property Id=\"WixAppFolder\" Value=\"WixPerUserFolder\" />")));
    QVERIFY(source.contains(QStringLiteral("<Property Id=\"ALLUSERS\" Value=\"2\" />")));
    QVERIFY(source.contains(QStringLiteral("<Property Id=\"MSIINSTALLPERUSER\" Value=\"1\" />")));
    QVERIFY(source.contains(QStringLiteral("function New-DeterministicProductCode")));
    QVERIFY(source.contains(QStringLiteral("UpgradeCode=\"31432BB7-5B11-55D5-8AE0-CD97E6B1D95F\"")));
    QVERIFY(source.contains(QStringLiteral("$namespaceGuid = [Guid]\"31432BB7-5B11-55D5-8AE0-CD97E6B1D95F\"")));
    QVERIFY(source.contains(QStringLiteral("<Product Id=\"%%PRODUCT_CODE%%\"")));
    QVERIFY(source.contains(QStringLiteral("$productSource.Replace(\"%%PRODUCT_CODE%%\", $productCode)")));
    QVERIFY(!source.contains(QStringLiteral("<Product Id=\"*\"")));
    QVERIFY(source.contains(QStringLiteral("<Property Id=\"CONGREGATION_EXISTING_USER_CONTEXT\" Secure=\"yes\">")));
    QVERIFY(source.contains(QStringLiteral("<Property Id=\"CONGREGATION_EXISTING_MACHINE_CONTEXT\" Secure=\"yes\">")));
    QVERIFY(source.contains(QStringLiteral("<Property Id=\"CONGREGATION_MACHINE_PROGRAMFILES64\" Secure=\"yes\">")));
    QVERIFY(source.contains(QStringLiteral("Name=\"ProgramFilesDir\"")));
    QVERIFY(source.contains(QStringLiteral("<Property Id=\"CONGREGATION_EXISTING_USER_INSTALLLOCATION\" Secure=\"yes\">")));
    QVERIFY(source.contains(QStringLiteral("<Property Id=\"CONGREGATION_EXISTING_MACHINE_INSTALLLOCATION\" Secure=\"yes\">")));
    QVERIFY(source.contains(QStringLiteral("Name=\"installContext\"")));
    QVERIFY(source.contains(QStringLiteral("Name=\"InstallLocation\"")));
    QVERIFY(source.contains(QStringLiteral("Name=\"machineStartMenuShortcut\"")));
    QVERIFY(!source.contains(QStringLiteral("<FindRelatedProducts Suppress=\"yes\"")));
    QVERIFY(source.contains(QStringLiteral("<AppSearch Sequence=\"10\" />")));
    QVERIFY(source.contains(QStringLiteral("<ComponentRef Id=\"InstallContextComponent\" />")));
    QVERIFY(source.contains(QStringLiteral("<Component Id=\"InstallContextComponent\" Guid=\"05DF7FB6-91BE-5013-AE84-E4A6496550E8\"")));
    QVERIFY(source.contains(QStringLiteral("Root=\"HKMU\"")));
    QVERIFY(source.contains(QStringLiteral("Id=\"CongregationSetExistingMachineContext\"")));
    const QRegularExpression existingMachineContextAction(
            QStringLiteral(R"(<CustomAction\s+Id="CongregationSetExistingMachineContext"\s+Property="ALLUSERS"\s+Value="1"\s+Execute="firstSequence"\s*/>)"));
    QVERIFY(existingMachineContextAction.match(source).hasMatch());
    QVERIFY(source.contains(QStringLiteral("Installed OR REMOVE~=\"ALL\" OR NOT (")));
    QVERIFY(source.contains(QStringLiteral("WIX_UPGRADE_DETECTED AND NOT (CONGREGATION_EXISTING_MACHINE_CONTEXT OR CONGREGATION_LEGACY_MACHINE_CONTEXT)")));

    const QRegularExpression coreFeatureExpression(
            QStringLiteral(R"(<Feature\s+Id="CoreFeature"[^>]*>)"));
    const QRegularExpressionMatch coreFeatureMatch = coreFeatureExpression.match(source);
    QVERIFY2(coreFeatureMatch.hasMatch(), "The MSI must define a CoreFeature.");
    const QString coreFeatureTag = coreFeatureMatch.captured();
    QVERIFY(coreFeatureTag.contains(QStringLiteral("Level=\"1\"")));
    QVERIFY(coreFeatureTag.contains(QStringLiteral("Absent=\"disallow\"")));
    QVERIFY(coreFeatureTag.contains(QStringLiteral("ConfigurableDirectory=\"APPLICATIONFOLDER\"")));

    QVERIFY(source.contains(QStringLiteral("<Property Id=\"DISABLEADVTSHORTCUTS\" Value=\"1\" />")));
    QVERIFY(source.contains(QStringLiteral("function Add-AdvertisedStartMenuShortcut")));
    QVERIFY(source.contains(QStringLiteral("Advertise=`\"yes`\"")));
    QVERIFY(source.contains(QStringLiteral("Directory=`\"ApplicationProgramsFolder`\"")));
    QVERIFY(source.contains(QStringLiteral("Add-AdvertisedStartMenuShortcut -HarvestPath $runtimePath")));
    QVERIFY(!source.contains(QStringLiteral("Id=\"StartMenuShortcutFeature\"")));
    QVERIFY(!source.contains(QStringLiteral("ApplicationShortcutComponent")));
    QVERIFY(!source.contains(QStringLiteral("MachineStartMenuShortcutComponent")));
    QVERIFY(!source.contains(QStringLiteral("D3D91B09-F3E7-4CA5-8E42-F5F25C8ACD2A")));
    QVERIFY(!source.contains(QStringLiteral("<Directory Id=\"CommonProgramsFolder\">")));

    QVERIFY(source.contains(QStringLiteral("smoke.exe")));
    QVERIFY(source.contains(QStringLiteral("-ice:ICE105")));
    QVERIFY(source.contains(QStringLiteral("-nodefault")));
    QVERIFY(source.contains(QStringLiteral("darice.cub")));
}

void tst_WindowsBuildWorkflowContract::windowsBuildScriptDefinesAuthenticodeContract()
{
    const QString scriptPath = QFINDTESTDATA("../build-windows.ps1");
    QVERIFY2(!scriptPath.isEmpty(), "build-windows.ps1 test data was not found");
    const QString source = readTextFile(scriptPath);
    QVERIFY(!source.isEmpty());

    QVERIFY(source.contains(QStringLiteral("[switch]$Sign")));
    QVERIFY(source.contains(QStringLiteral("[switch]$AllowUnsignedPackage")));
    QVERIFY(source.contains(QStringLiteral("$env:CONGREGATION_SIGNING_CERTIFICATE_THUMBPRINT")));
    QVERIFY(source.contains(QStringLiteral("$env:SIGNTOOL_PATH")));
    QVERIFY(source.contains(QStringLiteral("http://timestamp.digicert.com")));
    QVERIFY(source.contains(QStringLiteral("1.3.6.1.5.5.7.3.3")));
    QVERIFY(source.contains(QStringLiteral("Resolve-SignTool")));
    QVERIFY(source.contains(QStringLiteral("Resolve-CodeSigningCertificate")));
    QVERIFY(source.contains(QStringLiteral("Assert-PublicCodeSigningChainEvidence")));
    QVERIFY(source.contains(QStringLiteral("Resolve-PublicCodeSigningCertificateTrust")));
    QVERIFY(source.contains(QStringLiteral("X509RevocationMode]::Online")));
    QVERIFY(source.contains(QStringLiteral("X509RevocationFlag]::ExcludeRoot")));
    QVERIFY(source.contains(QStringLiteral("Cert:\\LocalMachine\\AuthRoot")));
    QVERIFY(source.contains(QStringLiteral("Public release signing rejects self-signed certificate chains")));
    QVERIFY(source.contains(QStringLiteral("Microsoft public trust root")));
    QVERIFY(source.contains(QStringLiteral("Sign-AuthenticodeFile")));
    QVERIFY(source.contains(QStringLiteral("Verify-AuthenticodeFile")));
    QVERIFY(source.contains(QStringLiteral("Get-CongregationOwnedStageFiles")));
    QVERIFY(source.contains(QStringLiteral("Sign-WindowsStage")));
    QVERIFY(source.contains(QStringLiteral("Verify-WindowsStageSignatures")));
    QVERIFY(source.contains(QStringLiteral("Clear-WindowsPackageArtifacts")));
    QVERIFY(source.contains(QStringLiteral("Write-Sha256File")));

    QVERIFY(!source.contains(QStringLiteral("\"/as\"")));
    QVERIFY(source.contains(QStringLiteral("\"/fd\", \"SHA256\"")));
    QVERIFY(source.contains(QStringLiteral("\"/tr\", $TimestampUrl")));
    QVERIFY(source.contains(QStringLiteral("\"/td\", \"SHA256\"")));
    QVERIFY(source.contains(QStringLiteral("\"/sha1\", $CertificateThumbprint")));
    QVERIFY(source.contains(QStringLiteral("\"verify\", \"/pa\", \"/all\", \"/tw\"")));
    QVERIFY(!source.contains(QStringLiteral("\"sign\", \"/a\"")));
    QVERIFY(!source.contains(QStringLiteral("\"/p\"")));

    QVERIFY(source.contains(QStringLiteral("-Sign and -AllowUnsignedPackage cannot be used together")));
    QVERIFY(source.contains(QStringLiteral("Public package creation requires -Sign")));
    QVERIFY(source.contains(QStringLiteral("-unsigned")));
    QVERIFY(source.contains(QStringLiteral("$UnsignedZipPath = Join-Path $DistRoot \"Congregation-$Version-Windows-unsigned.zip\"")));
    QVERIFY(source.contains(QStringLiteral("$UnsignedMsiPath = Join-Path $BuildDir \"Congregation-$Version-Windows-unsigned.msi\"")));
    QVERIFY(source.contains(QStringLiteral("$ZipChecksumPath = \"$ZipPath.sha256\"")));
    QVERIFY(source.contains(QStringLiteral("$MsiChecksumPath = \"$MsiPath.sha256\"")));
    QVERIFY(source.contains(QStringLiteral("$MsiPartialDebugPath")));
    QVERIFY(source.contains(QStringLiteral("$CpackIncompleteZipPath")));
    QVERIFY(source.contains(QStringLiteral("Code Signing EKU")));
    QVERIFY(source.contains(QStringLiteral("DigitalSignature")));
    QVERIFY(source.contains(QStringLiteral("Assert-CodeSigningCertificateKeyUsage")));
    QVERIFY(source.contains(QStringLiteral("DigitalSignature")));
    QVERIFY(source.contains(QStringLiteral("private key")));
    QVERIFY(source.contains(QStringLiteral("RFC 3161")));
    QVERIFY(source.contains(QStringLiteral("Authenticode release signing does not allow -SkipTests")));
    QVERIFY(source.contains(QStringLiteral("\"Congregation.exe\", \"LVRS.dll\", \"libiiPaintEngine.dll\"")));
    QVERIFY(source.contains(QStringLiteral("-ExpectedCertificateThumbprint $CertificateThumbprint")));
    QVERIFY(!source.contains(QStringLiteral("Get-Command \"signtool.exe\"")));

    const qsizetype stripIndex = source.lastIndexOf(QStringLiteral("Strip-WindowsRuntimeBinaries -Directory"));
    const qsizetype structuralVerifyIndex = source.lastIndexOf(QStringLiteral("Verify-WindowsStage -Directory"));
    const qsizetype signStageIndex = source.lastIndexOf(QStringLiteral("Sign-WindowsStage -Directory"));
    const qsizetype signatureVerifyIndex = source.lastIndexOf(QStringLiteral("Verify-WindowsStageSignatures -Directory"));
    const qsizetype zipIndex = source.lastIndexOf(QStringLiteral("Compress-Archive"));
    const qsizetype zipChecksumIndex = source.lastIndexOf(QStringLiteral("-File $ZipPartialPath `"));
    const qsizetype groupPublishIndex = source.lastIndexOf(QStringLiteral("-Artifacts $packageArtifacts `"));
    QVERIFY(stripIndex >= 0);
    QVERIFY(structuralVerifyIndex > stripIndex);
    QVERIFY(signStageIndex > structuralVerifyIndex);
    QVERIFY(signatureVerifyIndex > signStageIndex);
    QVERIFY(zipIndex > signatureVerifyIndex);
    QVERIFY(zipChecksumIndex > zipIndex);

    const qsizetype createMsiIndex = source.lastIndexOf(QStringLiteral("New-MsiInstaller -SourceDirectory $StageDir -OutputPath $MsiPartialPath"));
    const qsizetype signMsiIndex = source.lastIndexOf(QStringLiteral("Sign-AuthenticodeFile -File $MsiPartialPath"));
    const qsizetype verifyMsiIndex = source.lastIndexOf(QStringLiteral("Verify-AuthenticodeFile -File $MsiPartialPath"));
    const qsizetype databaseContractIndex = source.lastIndexOf(QStringLiteral("Assert-MsiDatabaseContract -MsiFile $MsiPartialPath"));
    const qsizetype msiChecksumIndex = source.lastIndexOf(QStringLiteral("-File $MsiPartialPath `"));
    QVERIFY(createMsiIndex >= 0);
    QVERIFY(createMsiIndex > zipChecksumIndex);
    QVERIFY(signMsiIndex > createMsiIndex);
    QVERIFY(verifyMsiIndex > signMsiIndex);
    QVERIFY(databaseContractIndex > verifyMsiIndex);
    QVERIFY(source.contains(QStringLiteral("\"-MsiPath\", $MsiFile")));
    QVERIFY(msiChecksumIndex > databaseContractIndex);
    QVERIFY(groupPublishIndex > msiChecksumIndex);

    const qsizetype policyIndex = source.lastIndexOf(QStringLiteral("Assert-AuthenticodePolicy `"));
    const qsizetype signingIdentityIndex = source.lastIndexOf(QStringLiteral("$ResolvedSignTool = Resolve-SignTool -ConfiguredPath $SignToolPath"));
    const qsizetype recoveryIndex = source.lastIndexOf(QStringLiteral("-Artifacts $recoveryArtifacts `"));
    const qsizetype clearArtifactsIndex = source.lastIndexOf(QStringLiteral("Clear-WindowsPackageArtifacts -Paths $partialArtifacts"));
    const qsizetype toolchainIndex = source.lastIndexOf(QStringLiteral("Write-Step \"Resolving toolchain\""));
    QVERIFY(recoveryIndex >= 0);
    QVERIFY(policyIndex > recoveryIndex);
    QVERIFY(signingIdentityIndex > policyIndex);
    QVERIFY(clearArtifactsIndex > signingIdentityIndex);
    QVERIFY(toolchainIndex > clearArtifactsIndex);
    QVERIFY(source.contains(QStringLiteral("$partialArtifacts += @($ZipPartialPath, $ZipChecksumPartialPath)")));
    QVERIFY(source.contains(QStringLiteral("$partialArtifacts += @($MsiPartialPath, $MsiPartialDebugPath, $MsiChecksumPartialPath)")));
    QVERIFY(source.contains(QStringLiteral("function Publish-PackageArtifact")));
    QVERIFY(source.contains(QStringLiteral("function Publish-PackageArtifactSet")));
    QVERIFY(source.contains(QStringLiteral("function Restore-PackageArtifactBackups")));
    QVERIFY(source.contains(QStringLiteral("function Restore-PackageArtifactSetBackups")));
    QVERIFY(source.contains(QStringLiteral("function Write-PackagePublicationJournal")));
    QVERIFY(source.contains(QStringLiteral("function Remove-PackagePublicationJournal")));
    QVERIFY(source.contains(QStringLiteral("function Enter-WindowsBuildMutex")));
    QVERIFY(source.contains(QStringLiteral("Global\\Congregation.BuildWindows.")));
    QVERIFY(source.contains(QStringLiteral("$mutex.WaitOne(0)")));
    QVERIFY(source.contains(QStringLiteral("Another build-windows.ps1 process is already using this repository")));
    QVERIFY(source.contains(QStringLiteral("$WindowsBuildMutex.ReleaseMutex()")));
    QVERIFY(source.contains(QStringLiteral("$PackagePublicationJournalPath")));
    QVERIFY(source.contains(QStringLiteral("$SignedPackagePublicationJournalPath")));
    QVERIFY(source.contains(QStringLiteral("$UnsignedPackagePublicationJournalPath")));
    QVERIFY(source.contains(QStringLiteral("$allowedFinalArtifacts")));
    QVERIFY(source.contains(QStringLiteral("$requestedFinalArtifacts")));
    QVERIFY(source.contains(QStringLiteral("Test-Path -LiteralPath $PackagePublicationJournalPath -PathType Leaf")));
    QVERIFY(source.contains(QStringLiteral("if ($Clean)")));
    QVERIFY(source.contains(QStringLiteral("JournalPath = $SignedPackagePublicationJournalPath")));
    QVERIFY(source.contains(QStringLiteral("JournalPath = $UnsignedPackagePublicationJournalPath")));
    QVERIFY(!source.contains(QStringLiteral("$PackageSelection")));
    QVERIFY(source.contains(QStringLiteral("SchemaVersion = 1")));
    QVERIFY(source.contains(QStringLiteral("-Phase \"prepared\"")));
    QVERIFY(source.contains(QStringLiteral("-Phase \"committed\"")));
    QVERIFY(source.contains(QStringLiteral("-JournalPath $PackagePublicationJournalPath")));
    QVERIFY(source.contains(QStringLiteral("$FinalArtifactPath.previous")));
    QVERIFY(source.contains(QStringLiteral("$FinalChecksumPath.previous")));
    QVERIFY(!source.contains(QStringLiteral("Remove-Item -LiteralPath $ZipPath -Force -ErrorAction SilentlyContinue")));
    QVERIFY(!source.contains(QStringLiteral("Remove-Item -LiteralPath $MsiPath -Force -ErrorAction SilentlyContinue")));
}

void tst_WindowsBuildWorkflowContract::cmakeBuildsWindowsGuiExecutable()
{
    const QString cmakePath = QFINDTESTDATA("../CMakeLists.txt");
    QVERIFY2(!cmakePath.isEmpty(), "CMakeLists.txt test data was not found");
    const QString source = readTextFile(cmakePath);
    QVERIFY(!source.isEmpty());

    QVERIFY2(source.contains(QStringLiteral("qt_add_executable(Congregation\n"
                                            "        MANUAL_FINALIZATION\n"
                                            "        WIN32\n"
                                            "        MACOSX_BUNDLE")),
             "Congregation must use the Windows GUI subsystem so launching it does not create a console window");
}

void tst_WindowsBuildWorkflowContract::windowsResourcesDefineNativeApplicationContract()
{
    const QString manifestPath = QFINDTESTDATA("../resources/windows/Congregation.manifest.in");
    QVERIFY2(!manifestPath.isEmpty(), "Windows application manifest test data was not found");
    const QString manifest = readTextFile(manifestPath);
    QVERIFY(!manifest.isEmpty());

    QVERIFY(manifest.contains(QStringLiteral("requestedExecutionLevel level=\"asInvoker\"")));
    QVERIFY(manifest.contains(QStringLiteral("{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}")));
    QVERIFY(manifest.contains(QStringLiteral("PerMonitorV2,PerMonitor")));
    QVERIFY(manifest.contains(QStringLiteral("<longPathAware")));
    QVERIFY(manifest.contains(QStringLiteral(">true</longPathAware>")));

    const QString resourceTemplatePath = QFINDTESTDATA("../resources/windows/Congregation.rc.in");
    QVERIFY2(!resourceTemplatePath.isEmpty(), "Windows resource template test data was not found");
    const QString resourceTemplate = readTextFile(resourceTemplatePath);
    QVERIFY(!resourceTemplate.isEmpty());

    QVERIFY(resourceTemplate.contains(QStringLiteral("CREATEPROCESS_MANIFEST_RESOURCE_ID RT_MANIFEST")));
    QVERIFY(resourceTemplate.contains(QStringLiteral("VS_VERSION_INFO VERSIONINFO")));
    QVERIFY(resourceTemplate.contains(QStringLiteral("FILEVERSION @CONGREGATION_WINDOWS_VERSION_COMMA@")));
    QVERIFY(resourceTemplate.contains(QStringLiteral("VALUE \"FileDescription\", \"Congregation raster drawing application\\0\"")));
    QVERIFY(resourceTemplate.contains(QStringLiteral("VALUE \"ProductName\", \"Congregation\\0\"")));

    const QString gitIgnorePath = QFINDTESTDATA("../.gitignore");
    QVERIFY2(!gitIgnorePath.isEmpty(), ".gitignore test data was not found");
    const QString gitIgnore = readTextFile(gitIgnorePath);
    QVERIFY(gitIgnore.contains(QStringLiteral("!/resources/windows/")));
}

void tst_WindowsBuildWorkflowContract::cmakeHasWindowsInstallAndPackageRules()
{
    const QString cmakePath = QFINDTESTDATA("../CMakeLists.txt");
    QVERIFY2(!cmakePath.isEmpty(), "CMakeLists.txt test data was not found");
    const QString source = readTextFile(cmakePath);
    QVERIFY(!source.isEmpty());

    QVERIFY(source.contains(QStringLiteral("elseif(WIN32)\n    install(TARGETS Congregation RUNTIME DESTINATION \".\" COMPONENT Runtime)")));
    QVERIFY(source.contains(QStringLiteral("elseif(WIN32)\n    set(CPACK_GENERATOR \"ZIP\")")));
    QVERIFY(source.contains(QStringLiteral("set(CPACK_PACKAGE_FILE_NAME \"Congregation-${CPACK_PACKAGE_VERSION}-Windows-unsigned-cpack-incomplete\")")));
    QVERIFY(source.contains(QStringLiteral("if(WIN32)\n    add_custom_command(TARGET Congregation POST_BUILD")));
    QVERIFY(source.contains(QStringLiteral("if(WIN32 AND MINGW)")));
    QVERIFY(source.contains(QStringLiteral("get_filename_component(_congregation_mingw_runtime_dir \"${CMAKE_CXX_COMPILER}\" DIRECTORY)")));
    QVERIFY(source.contains(QStringLiteral("NO_DEFAULT_PATH")));
    QVERIFY(source.contains(QStringLiteral("libgcc_s_seh-1.dll")));
    QVERIFY(source.contains(QStringLiteral("libstdc++-6.dll")));
    QVERIFY(source.contains(QStringLiteral("libwinpthread-1.dll")));
    QVERIFY(source.contains(QStringLiteral("$<TARGET_FILE:LVRS::LVRS>")));
    QVERIFY(source.contains(QStringLiteral("$<TARGET_FILE:iiPaintEngine::iiPaintEngine>")));
    QVERIFY(source.contains(QStringLiteral("$<TARGET_FILE_DIR:Congregation>")));
    QVERIFY(source.contains(QStringLiteral("Copying Windows runtime DLLs next to Congregation.exe")));
    QVERIFY(source.contains(QStringLiteral("if(APPLE)\n    # productbuild")));
    QVERIFY(source.contains(QStringLiteral("set(CPACK_GENERATOR \"productbuild\")")));
    QVERIFY(source.contains(QStringLiteral("set_source_files_properties(\"${psd_sdk_SOURCE_DIR}/src/Psd/Psdminiz.c\" PROPERTIES LANGUAGE CXX)")));
    QVERIFY(source.contains(QStringLiteral("UPDATE_DISCONNECTED TRUE")));
    QVERIFY(source.contains(QStringLiteral("lvrs_apply_platform_build_optimizations(congregation_psd_sdk)")));
    QVERIFY(source.contains(QStringLiteral("-Wno-pragmas")));
    QVERIFY(source.contains(QStringLiteral("INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE")));
    QVERIFY(!source.contains(QStringLiteral("-flto=4")));
    QVERIFY(source.contains(QStringLiteral("resources/windows/Congregation.rc.in")));
    QVERIFY(source.contains(QStringLiteral("resources/windows/Congregation.manifest.in")));
    QVERIFY(source.contains(QStringLiteral("configure_file(\"${CONGREGATION_WINDOWS_RESOURCE_TEMPLATE}\"")));
    QVERIFY(source.contains(QStringLiteral("-Wl,--gc-sections")));

    const QString testsCmakePath = QFINDTESTDATA("../tests/CMakeLists.txt");
    QVERIFY2(!testsCmakePath.isEmpty(), "tests/CMakeLists.txt test data was not found");
    const QString testsSource = readTextFile(testsCmakePath);
    QVERIFY(testsSource.contains(QStringLiteral("ENVIRONMENT_MODIFICATION")));
    QVERIFY(testsSource.contains(QStringLiteral("PATH=path_list_prepend:$<TARGET_FILE_DIR:iiPaintEngine::iiPaintEngine>")));
    QVERIFY(testsSource.contains(QStringLiteral("QT_QPA_PLATFORM=set:offscreen")));
    QVERIFY(testsSource.contains(QStringLiteral("NAME tests_windowsauthenticodepolicy")));
    QVERIFY(testsSource.contains(QStringLiteral("tst_windowsauthenticodepolicy.ps1")));
    QVERIFY(testsSource.contains(QStringLiteral("NAMES pwsh.exe")));
    QVERIFY(testsSource.contains(QStringLiteral("NAME tests_windowsauthenticodepolicy_pwsh")));
    QVERIFY(testsSource.contains(QStringLiteral("NAME tests_windowsmsiauthoringcontract")));
    QVERIFY(testsSource.contains(QStringLiteral("tst_windowsmsiauthoringcontract.ps1")));
    QVERIFY(testsSource.contains(QStringLiteral("NAME tests_windowsstorepackagecontract")));
    QVERIFY(testsSource.contains(QStringLiteral("tst_windowsstorepackagecontract.ps1")));
    QVERIFY(!testsSource.contains(QStringLiteral("NAME tests_windowsmsidatabasecontract")));
    QVERIFY(!testsSource.contains(QStringLiteral("tst_windowsmsidatabasecontract.ps1")));
    QVERIFY(!testsSource.contains(QStringLiteral("SKIP_RETURN_CODE 77")));

    const QString authenticodePolicyPath = QFINDTESTDATA("tst_windowsauthenticodepolicy.ps1");
    QVERIFY2(!authenticodePolicyPath.isEmpty(), "Authenticode policy test script was not found");
    const QString authenticodePolicySource = readTextFile(authenticodePolicyPath);
    QVERIFY(authenticodePolicySource.contains(QStringLiteral("$PSHOME")));
    QVERIFY(authenticodePolicySource.contains(QStringLiteral("Microsoft.PowerShell.Utility.psd1")));

    const QString databaseContractPath = QFINDTESTDATA("tst_windowsmsidatabasecontract.ps1");
    QVERIFY2(!databaseContractPath.isEmpty(), "MSI database contract script was not found");
    const QString databaseContractSource = readTextFile(databaseContractPath);
    QVERIFY(databaseContractSource.contains(QStringLiteral(
        "[Parameter(Mandatory = $true)]\n    [string]$MsiPath")));
    QVERIFY(!databaseContractSource.contains(QStringLiteral("Congregation-$Version-Windows.msi")));
    QVERIFY(!databaseContractSource.contains(QStringLiteral("Congregation-$Version-Windows-unsigned.msi")));
    QVERIFY(!databaseContractSource.contains(QStringLiteral("skipping database contract")));
    QVERIFY(!databaseContractSource.contains(QStringLiteral("exit 77")));

    const QString buildScriptPath = QFINDTESTDATA("../build-windows.ps1");
    QVERIFY2(!buildScriptPath.isEmpty(), "build-windows.ps1 test data was not found");
    const QString buildScriptSource = readTextFile(buildScriptPath);
    QVERIFY(buildScriptSource.contains(QStringLiteral(
        "Assert-MsiDatabaseContract -MsiFile $MsiPartialPath")));
    QVERIFY(buildScriptSource.contains(QStringLiteral("\"-MsiPath\", $MsiFile")));
}

void tst_WindowsBuildWorkflowContract::appEntryPointAvoidsNonExportedLvrsRuntimeSymbols()
{
    const QString mainPath = QFINDTESTDATA("../src/App/main.cpp");
    QVERIFY2(!mainPath.isEmpty(), "src/App/main.cpp test data was not found");
    const QString source = readTextFile(mainPath);
    QVERIFY(!source.isEmpty());

    QVERIFY(source.contains(QStringLiteral("QGuiApplication app(argc, argv);")));
    QVERIFY(!source.contains(QStringLiteral("qInstallMessageHandler(")));
    QVERIFY(source.contains(QStringLiteral("bool startupTraceEnabled()")));
    QVERIFY(source.contains(QStringLiteral("CONGREGATION_STARTUP_TRACE")));
    QVERIFY(source.contains(QStringLiteral("QFile &startupLogFile()")));
    QVERIFY(source.contains(QStringLiteral("QMutexLocker")));
    QCOMPARE(source.count(QStringLiteral("logFile.open(")), 1);
    QVERIFY(source.contains(QStringLiteral("Congregation-startup.log")));
    QVERIFY(source.contains(QStringLiteral("QDir::tempPath()")));
    QVERIFY(source.contains(QStringLiteral("qgetenv(\"TEMP\")")));
    QVERIFY(source.contains(QStringLiteral("Congregation startup initialized")));
    QVERIFY(source.contains(QStringLiteral("QElapsedTimer launchTimer")));
    QVERIFY(source.contains(QStringLiteral("void qml_register_types_LVRS();")));
    QVERIFY(source.contains(QStringLiteral("qml_register_types_LVRS();")));
    QVERIFY(!source.contains(QStringLiteral("SetWindowPos(")));
    QVERIFY(!source.contains(QStringLiteral("ShowWindow(")));
    QVERIFY(!source.contains(QStringLiteral("#include <windows.h>")));
    QVERIFY(source.contains(QStringLiteral("QCoreApplication::applicationDirPath() + QStringLiteral(\"/qml\")")));
    QVERIFY(source.contains(QStringLiteral("const bool hasBundledLvrs")));
    QVERIFY(source.contains(QStringLiteral("if (!hasBundledLvrs && !lvrsHostPrefix.isEmpty())")));
    QVERIFY(!source.contains(QStringLiteral(".local/SDK/LVRS/platforms")));
    QVERIFY(source.contains(QStringLiteral("engine.loadFromModule(QStringLiteral(\"Congregation\"), QStringLiteral(\"Main\"))")));
    QVERIFY(source.contains(QStringLiteral("engine.singletonInstance<QObject *>(QStringLiteral(\"LVRS\"),")));
    QVERIFY(source.contains(QStringLiteral("QMetaObject::invokeMethod(registry,")));
    QVERIFY(!source.contains(QStringLiteral("runBootstrappedQmlApp")));
    QVERIFY(!source.contains(QStringLiteral("ViewModelRegistry *")));
    QVERIFY(!source.contains(QStringLiteral("#include <backend/runtime/appentry.h>")));
    QVERIFY(!source.contains(QStringLiteral("#include <backend/state/viewmodelregistry.h>")));
}

void tst_WindowsBuildWorkflowContract::appEntryPointShowsFinalGeometryOnlyOnce()
{
    const QString mainPath = QFINDTESTDATA("../src/App/main.cpp");
    QVERIFY2(!mainPath.isEmpty(), "src/App/main.cpp test data was not found");
    const QString source = readTextFile(mainPath);
    QVERIFY(!source.isEmpty());

    QVERIFY(source.contains(QStringLiteral("void showLaunchWindow(QQmlApplicationEngine &engine)")));
    QVERIFY(source.contains(QStringLiteral("QSize finalLaunchWindowSize(const QWindow &window)")));
    QVERIFY(source.contains(QStringLiteral("screen->availableGeometry().size()")));
    QVERIFY(source.contains(QStringLiteral("requestedSize.scaled(availableSize, Qt::KeepAspectRatio)")));
    QVERIFY(source.contains(QStringLiteral("if (window->size() != finalSize)")));
    QVERIFY(source.contains(QStringLiteral("window->resize(finalSize);")));
    QVERIFY(source.contains(QStringLiteral("window->showNormal();")));
    QVERIFY(!source.contains(QStringLiteral("ensureLaunchWindowVisible")));
    QCOMPARE(source.count(QStringLiteral("window->resize(")), 1);

    const qsizetype resizeIndex = source.indexOf(QStringLiteral("window->resize(finalSize);"));
    const qsizetype showNormalIndex = source.indexOf(QStringLiteral("window->showNormal();"));
    QVERIFY(resizeIndex >= 0);
    QVERIFY(showNormalIndex > resizeIndex);

    const qsizetype loadIndex = source.indexOf(QStringLiteral("engine.loadFromModule(QStringLiteral(\"Congregation\"), QStringLiteral(\"Main\"));"));
    const qsizetype showIndex = source.indexOf(QStringLiteral("showLaunchWindow(engine);"));
    const qsizetype eventLoopIndex = source.indexOf(QStringLiteral("return app.exec();"));
    QVERIFY(loadIndex >= 0);
    QVERIFY(showIndex > loadIndex);
    QVERIFY(eventLoopIndex > showIndex);
}

void tst_WindowsBuildWorkflowContract::buildGuideDocumentsWindowsScript()
{
    const QString buildGuidePath = QFINDTESTDATA("../docs/BUILD.md");
    QVERIFY2(!buildGuidePath.isEmpty(), "docs/BUILD.md test data was not found");
    const QString source = readTextFile(buildGuidePath);
    QVERIFY(!source.isEmpty());

    QVERIFY(source.contains(QStringLiteral("## 1b. Windows Build, Package, and Current-User Install Script")));
    QVERIFY(source.contains(QStringLiteral("powershell -ExecutionPolicy Bypass -File .\\build-windows.ps1 -Clean -SkipPackage")));
    QVERIFY(source.contains(QStringLiteral("powershell -ExecutionPolicy Bypass -File .\\build-windows.ps1 -SkipPackage -InstallForCurrentUser")));
    QVERIFY(source.contains(QStringLiteral("QT_PREFIX")));
    QVERIFY(source.contains(QStringLiteral("LVRS_PREFIX")));
    QVERIFY(source.contains(QStringLiteral("IIPAINTENGINE_PREFIX")));
    QVERIFY(source.contains(QStringLiteral("cmake -S . -B build")));
    QVERIFY(source.contains(QStringLiteral("preserves that repository-local tree for incremental work")));
    QVERIFY(source.contains(QStringLiteral("ctest --test-dir build --output-on-failure")));
    QVERIFY(source.contains(QStringLiteral("windeployqt")));
    QVERIFY(source.contains(QStringLiteral("LVRS QML is compiled into the LVRS binary")));
    QVERIFY(source.contains(QStringLiteral("loose `qml/LVRS` directory")));
    QVERIFY(source.contains(QStringLiteral("MinGW ABI")));
    QVERIFY(source.contains(QStringLiteral("PE import closure")));
    QVERIFY(source.contains(QStringLiteral("__cxa_thread_atexit")));
    QVERIFY(source.contains(QStringLiteral("dist/Congregation-Windows")));
    QVERIFY(source.contains(QStringLiteral("dist/Congregation-1.0.0-Windows.zip")));
    QVERIFY(source.contains(QStringLiteral("build/Congregation-1.0.0-Windows.msi")));
    QVERIFY(source.contains(QStringLiteral("Program Files")));
    QVERIFY(source.contains(QStringLiteral("requires elevation")));
    QVERIFY(source.contains(QStringLiteral("defaults to the current user")));
    QVERIFY(source.contains(QStringLiteral("same installation context")));
    QVERIFY(source.contains(QStringLiteral("Unattended upgrades must not override `ALLUSERS` or `MSIINSTALLPERUSER`")));
    QVERIFY(source.contains(QStringLiteral("starts a separate Congregation upgrade family")));
    QVERIFY(source.contains(QStringLiteral("ALLUSERS=2")));
    QVERIFY(source.contains(QStringLiteral("MSIINSTALLPERUSER=1")));
    QVERIFY(source.contains(QStringLiteral("ICE105")));
    QVERIFY(source.contains(QStringLiteral("installation-context marker")));
    QVERIFY(source.contains(QStringLiteral("deterministic ProductCode")));
    QVERIFY(source.contains(QStringLiteral("same version and architecture")));
    QVERIFY(source.contains(QStringLiteral("markerless per-user upgrade")));
    QVERIFY(source.contains(QStringLiteral("maintenance and removal remain available")));
    QVERIFY(source.contains(QStringLiteral("first three ProductVersion fields")));
    QVERIFY(source.contains(QStringLiteral("does not allocate a console window")));
    QVERIFY(source.contains(QStringLiteral("requests a width of 1,280 logical pixels")));
    QVERIFY(source.contains(QStringLiteral("does not resize the window after it becomes visible")));
    QVERIFY(source.contains(QStringLiteral("-Sign")));
    QVERIFY(source.contains(QStringLiteral("-AllowUnsignedPackage")));
    QVERIFY(source.contains(QStringLiteral("CONGREGATION_SIGNING_CERTIFICATE_THUMBPRINT")));
    QVERIFY(source.contains(QStringLiteral("signtool verify /pa /all /tw")));
    QVERIFY(source.contains(QStringLiteral("RFC 3161")));
    QVERIFY(source.contains(QStringLiteral("Code Signing EKU")));
    QVERIFY(source.contains(QStringLiteral("-unsigned")));
    QVERIFY(source.contains(QStringLiteral("`.sha256`")));
    QVERIFY(source.contains(QStringLiteral("SmartScreen also evaluates publisher reputation")));
    QVERIFY(source.contains(QStringLiteral("checksum alone proves equality, not publisher identity")));
    QVERIFY(source.contains(QStringLiteral("online-revocation Code Signing chain")));
    QVERIFY(source.contains(QStringLiteral("LocalMachine\\AuthRoot")));
    QVERIFY(source.contains(QStringLiteral("clean stock Windows machine")));
    QVERIFY(source.contains(QStringLiteral("expected Publisher subject or certificate thumbprint")));
    QVERIFY(source.contains(QStringLiteral("unsigned-cpack-incomplete")));
    QVERIFY(source.contains(QStringLiteral("generated under `.partial` names")));
    QVERIFY(source.contains(QStringLiteral("last-known-good")));
    QVERIFY(source.contains(QStringLiteral("complete verified set")));
    QVERIFY(source.contains(QStringLiteral("root `LICENSE` into the MSI's RTF license control")));
    QVERIFY(source.contains(QStringLiteral("## 1c. Microsoft Store MSIX")));
    QVERIFY(source.contains(QStringLiteral("build-windows-store.ps1 -Mode Development -InstallDevelopment")));
    QVERIFY(source.contains(QStringLiteral("build-windows-store.ps1 -Mode Store")));
    QVERIFY(source.contains(QStringLiteral("CONGREGATION_STORE_IDENTITY_NAME")));
    QVERIFY(source.contains(QStringLiteral("Package/Identity/Publisher")));
    QVERIFY(source.contains(QStringLiteral("LocalMachine\\TrustedPeople")));
    QVERIFY(source.contains(QStringLiteral("Congregation-1.0.0-Windows-Store-x64.msixupload")));
    QVERIFY(source.contains(QStringLiteral("runFullTrust")));
    QVERIFY(source.contains(QStringLiteral("Microsoft re-signs")));
}

void tst_WindowsBuildWorkflowContract::signPathWorkflowDefinesFreeWebsiteReleaseContract()
{
    const QString workflowPath = QFINDTESTDATA("../.github/workflows/windows-signpath-release.yml");
    QVERIFY2(!workflowPath.isEmpty(), "The SignPath Windows release workflow was not found");
    const QString workflow = readTextFile(workflowPath);
    QVERIFY(!workflow.isEmpty());

    QVERIFY(workflow.contains(QStringLiteral("name: Windows website release")));
    QVERIFY(workflow.contains(QStringLiteral("runs-on: windows-2022")));
    QVERIFY(workflow.contains(QStringLiteral("jurplel/install-qt-action")));
    QVERIFY(workflow.contains(QStringLiteral("version: '6.8.3'")));
    QVERIFY(workflow.contains(QStringLiteral("arch: 'win64_mingw'")));
    QVERIFY(workflow.contains(QStringLiteral("modules: 'qtimageformats qtshadertools'")));
    QVERIFY(!workflow.contains(QStringLiteral("modules: 'qtsvg")));
    QVERIFY(workflow.contains(QStringLiteral("source: true")));
    QVERIFY(workflow.contains(QStringLiteral(
        "src-archives: 'qtbase qtdeclarative qtsvg qtimageformats qtshadertools qttranslations'")));
    QVERIFY(workflow.contains(QStringLiteral("iisacc-Justmoong/LVRS")));
    QVERIFY(workflow.contains(QStringLiteral("iisacc-Justmoong/iiPaintEngine")));
    QVERIFY(workflow.contains(QStringLiteral("LVRS_COMMIT: 8132a48e4c0788f0548bff20fd7089878a09c760")));
    QVERIFY(workflow.contains(QStringLiteral("IIPAINTENGINE_COMMIT: 5ade15e24ce3a7eab25669144fdee668672b22d6")));
    QVERIFY(workflow.contains(QStringLiteral("IISHAREDCANVAS_COMMIT: d2c41bf605725eb07b181587321f0ce910b9924d")));
    QVERIFY(workflow.contains(QStringLiteral("Build and install pinned LVRS")));
    QVERIFY(workflow.contains(QStringLiteral("Build and install pinned iiPaintEngine")));
    QVERIFY(workflow.contains(QStringLiteral("-DLVRS_ENABLE_IPO=OFF")));
    QVERIFY(workflow.contains(QStringLiteral("-DLVRS_BOOTSTRAP_LVRS_ENABLE_IPO=OFF")));
    QVERIFY(workflow.contains(QStringLiteral("-DLVRS_ENABLE_PLATFORM_BUILD_OPTIMIZATIONS=OFF")));
    QVERIFY(workflow.contains(QStringLiteral("-DLVRS_BOOTSTRAP_LVRS_ENABLE_PLATFORM_BUILD_OPTIMIZATIONS=OFF")));
    QVERIFY(workflow.contains(QStringLiteral("$lvrsArguments = @(")));
    QVERIFY(workflow.contains(QStringLiteral("@lvrsArguments")));
    QVERIFY(workflow.contains(QStringLiteral("\"--without-examples\",\n              \"--without-tests\",\n              \"--\"")));
    QVERIFY(workflow.contains(QStringLiteral("\"--build-type\", \"MinSizeRel\"")));
    QVERIFY(workflow.contains(QStringLiteral("-ExternalSigning -SkipPackage -CreateMsi")));
    QVERIFY(workflow.contains(QStringLiteral("actions/upload-artifact")));
    QVERIFY(workflow.contains(QStringLiteral("archive: false")));
    QVERIFY(workflow.contains(QStringLiteral("signpath/github-action-submit-signing-request")));
    QVERIFY(workflow.contains(QStringLiteral("SIGNPATH_API_TOKEN")));
    QVERIFY(workflow.contains(QStringLiteral("SIGNPATH_ORGANIZATION_ID")));
    QVERIFY(workflow.contains(QStringLiteral("SIGNPATH_PROJECT_SLUG")));
    QVERIFY(workflow.contains(QStringLiteral("SIGNPATH_SIGNING_POLICY_SLUG")));
    QVERIFY(workflow.contains(QStringLiteral("SIGNPATH_ARTIFACT_CONFIGURATION_SLUG")));
    QVERIFY(workflow.contains(QStringLiteral("Require SignPath configuration")));
    QVERIFY(workflow.contains(QStringLiteral("Require the matching version tag")));
    QVERIFY(workflow.contains(QStringLiteral("$expectedTag = \"v$expectedVersion\"")));
    QVERIFY(workflow.contains(QStringLiteral("Windows Kits\\10\\bin")));
    QVERIFY(workflow.contains(QStringLiteral("signtool.exe")));
    QVERIFY(workflow.contains(QStringLiteral("Get-ChildItem")));
    QVERIFY(workflow.contains(QStringLiteral("Congregation-*-Windows.msi")));
    QVERIFY(!workflow.contains(QStringLiteral("Congregation-1.0.0-Windows.msi")));
    QVERIFY(workflow.contains(QStringLiteral("CMAKE_PROJECT_VERSION:STATIC=")));
    QVERIFY(workflow.contains(QStringLiteral(
        "$msiVersion = if ($expectedVersion -match '^\\d+\\.\\d+$') { "
        "\"$expectedVersion.0\" } else { $expectedVersion }")));
    QVERIFY(workflow.contains(QStringLiteral(
        "powershell -NoProfile -ExecutionPolicy Bypass -File .\\tests\\tst_windowsmsidatabasecontract.ps1 "
        "-BuildDirectory .\\build -Version $msiVersion -MsiPath $msi")));
    QVERIFY(workflow.contains(QStringLiteral(
        "throw \"MSI database contract rejected the signed MSI.\"")));
    QVERIFY(workflow.contains(QStringLiteral("Extract signed MSI payload for nested signature verification")));
    QVERIFY(workflow.contains(QStringLiteral("Congregation.exe")));
    QVERIFY(workflow.contains(QStringLiteral("The installed Congregation executable is not Authenticode-signed")));
    QVERIFY(workflow.contains(QStringLiteral("SignTool rejected the installed Congregation executable")));

    const QString readmePath = QFINDTESTDATA("../README.md");
    QVERIFY2(!readmePath.isEmpty(), "README.md test data was not found");
    const QString readme = readTextFile(readmePath);
    QVERIFY(readme.contains(QStringLiteral("## Code signing policy")));
    QVERIFY(readme.contains(QStringLiteral("No SignPath Foundation certificate is currently active for Congregation")));
    QVERIFY(readme.contains(QStringLiteral("must not submit a release signing request until the Foundation has explicitly approved the project")));
    QVERIFY(readme.contains(QStringLiteral("every self-signed trial artifact are development-only files")));
    QVERIFY(readme.contains(QStringLiteral("Committer and reviewer")));
    QVERIFY(readme.contains(QStringLiteral("Approver")));
    QVERIFY(readme.contains(QStringLiteral("will not transfer any information to other networked systems")));
    QVERIFY(readme.contains(QStringLiteral("## Community and Contributing")));
    QVERIFY(readme.contains(QStringLiteral("https://github.com/iisacc-Justmoong/Congregation/discussions")));
    QVERIFY(readme.contains(QStringLiteral("https://github.com/iisacc-Justmoong/Congregation/issues/new")));

    const QString contributingPath = QFINDTESTDATA("../../CONTRIBUTING.md");
    QVERIFY2(!contributingPath.isEmpty(), "CONTRIBUTING.md test data was not found");
    const QString contributing = readTextFile(contributingPath);
    QVERIFY(contributing.contains(QStringLiteral("repository-local `build/` directory")));
    QVERIFY(contributing.contains(QStringLiteral("QML changes must use the `.local/SDK/LVRS/` framework")));
    QVERIFY(contributing.contains(QStringLiteral("self-signed SignPath trial outputs are development-only artifacts")));
    QVERIFY(contributing.contains(QStringLiteral("outer MSI and nested `Congregation.exe`")));

    const QString buildGuidePath = QFINDTESTDATA("../docs/BUILD.md");
    QVERIFY2(!buildGuidePath.isEmpty(), "docs/BUILD.md test data was not found");
    const QString buildGuide = readTextFile(buildGuidePath);
    QVERIFY(buildGuide.contains(QStringLiteral(
        "passes the exact newly linked `.partial.msi` path and the normalized three-field Windows Installer ProductVersion to the MSI database contract")));
    QVERIFY(buildGuide.contains(QStringLiteral(
        "creates an administrative image and independently verifies the timestamp, Publisher, certificate thumbprint, and SignTool policy for all three Congregation-owned installed binaries")));
}

QTEST_APPLESS_MAIN(tst_WindowsBuildWorkflowContract)

#include "tst_windowsbuildworkflowcontract.moc"
