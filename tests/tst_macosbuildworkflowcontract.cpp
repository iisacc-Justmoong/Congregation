#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QtTest>

namespace
{
bool writeExecutable(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    if (file.write(contents) != contents.size()) {
        return false;
    }
    file.close();

    return file.setPermissions(QFileDevice::ReadOwner
                               | QFileDevice::WriteOwner
                               | QFileDevice::ExeOwner
                               | QFileDevice::ReadGroup
                               | QFileDevice::ExeGroup
                               | QFileDevice::ReadOther
                               | QFileDevice::ExeOther);
}

QString legacyClionBuildTreeName()
{
    return QStringLiteral("cmake")
            + QStringLiteral("-")
            + QStringLiteral("build")
            + QStringLiteral("-")
            + QStringLiteral("debug");
}
}

class tst_MacOSBuildWorkflowContract : public QObject
{
    Q_OBJECT

private slots:
    void cmakeFixesApplicationVersionAt100();
    void cmakeRequiresRepositoryBuildDirectory();
    void cmakeAvoidsRedundantMacOSRuntimeRpaths();
    void localNetworkDiscoveryDeclaresPrivacyAndSandboxAccess();
    void repositoryGuidelinesUseOnlyBuildDirectory();
    void buildGuideSeparatesLocalAndDistributionSigning();
    void platformAppIconsAreBundledFromResources();
    void buildScriptUsesIncrementalBuildsAndStripsDistributionBundles();
    void buildScriptRunsLocalModeWithEmptyOptionalArgumentArrays();
    void buildScriptFailsClosedOnMissingNotaryProfileBeforeBuild();
};

void tst_MacOSBuildWorkflowContract::cmakeFixesApplicationVersionAt100()
{
    const QString cmakePath = QFINDTESTDATA("../CMakeLists.txt");
    QVERIFY2(!cmakePath.isEmpty(), "CMakeLists.txt test data was not found");

    QFile cmakeFile(cmakePath);
    QVERIFY(cmakeFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString cmakeSource = QString::fromUtf8(cmakeFile.readAll());

    const QString infoPlistPath = QFINDTESTDATA("../packaging/macos/Info.plist");
    QVERIFY2(!infoPlistPath.isEmpty(), "packaging/macos/Info.plist test data was not found");

    QFile infoPlist(infoPlistPath);
    QVERIFY(infoPlist.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString infoPlistSource = QString::fromUtf8(infoPlist.readAll());

    QVERIFY(cmakeSource.contains(QStringLiteral("project(Congregation VERSION 1.0.0 LANGUAGES C CXX)")));
    QVERIFY(cmakeSource.contains(QStringLiteral("if(CMAKE_HOST_SYSTEM_NAME STREQUAL \"Darwin\" AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)")));
    const qsizetype deploymentTargetIndex = cmakeSource.indexOf(QStringLiteral("set(CMAKE_OSX_DEPLOYMENT_TARGET"));
    const qsizetype projectIndex = cmakeSource.indexOf(QStringLiteral("project(Congregation VERSION 1.0.0 LANGUAGES C CXX)"));
    QVERIFY(deploymentTargetIndex >= 0);
    QVERIFY(projectIndex > deploymentTargetIndex);
    QVERIFY(cmakeSource.contains(QStringLiteral("set(CONGREGATION_BUNDLE_VERSION \"1\")")));
    QVERIFY(cmakeSource.contains(QStringLiteral("MACOSX_BUNDLE_BUNDLE_VERSION \"${CONGREGATION_BUNDLE_VERSION}\"")));
    QVERIFY(cmakeSource.contains(QStringLiteral("MACOSX_BUNDLE_SHORT_VERSION_STRING \"${PROJECT_VERSION}\"")));
    QVERIFY(infoPlistSource.contains(QStringLiteral("<string>@PROJECT_VERSION@</string>")));
    QVERIFY(infoPlistSource.contains(QStringLiteral("<string>@CONGREGATION_BUNDLE_VERSION@</string>")));
}

void tst_MacOSBuildWorkflowContract::cmakeRequiresRepositoryBuildDirectory()
{
    const QString cmakePath = QFINDTESTDATA("../CMakeLists.txt");
    QVERIFY2(!cmakePath.isEmpty(), "CMakeLists.txt test data was not found");

    QFile cmakeFile(cmakePath);
    QVERIFY(cmakeFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString cmakeSource = QString::fromUtf8(cmakeFile.readAll());

    QVERIFY(cmakeSource.contains(QStringLiteral("repository-local build/ directory")));
    QVERIFY(cmakeSource.contains(QStringLiteral("cmake -S ${CMAKE_SOURCE_DIR} -B ${_congregation_required_build_dir}")));
    QVERIFY(cmakeSource.contains(QStringLiteral("NOT _congregation_actual_build_dir STREQUAL _congregation_required_build_dir")));
}

void tst_MacOSBuildWorkflowContract::cmakeAvoidsRedundantMacOSRuntimeRpaths()
{
    const QString cmakePath = QFINDTESTDATA("../CMakeLists.txt");
    QVERIFY2(!cmakePath.isEmpty(), "CMakeLists.txt test data was not found");

    QFile cmakeFile(cmakePath);
    QVERIFY(cmakeFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = QString::fromUtf8(cmakeFile.readAll());

    QVERIFY(source.contains(QStringLiteral("if(UNIX AND NOT APPLE)\n"
                                           "    list(APPEND _congregation_local_dependency_runtime_candidates")));
    QVERIFY(!source.contains(QStringLiteral("if(UNIX)\n"
                                            "    list(APPEND _congregation_local_dependency_runtime_candidates")));
    QVERIFY(source.contains(QStringLiteral("GIT_TAG 875f77d9f61bd97fd84cca47ce3bc71186dfbd09")));
    QVERIFY(source.contains(QStringLiteral("set(BUILD_TRANSLATIONS OFF)")));
    QVERIFY(source.contains(QStringLiteral("set(BUILD_SHARED_LIBS OFF)")));
    QVERIFY(source.contains(QStringLiteral("set(BUILD_TESTING OFF)")));
    QVERIFY(source.contains(QStringLiteral("set(USE_CREDENTIAL_STORE ON)")));
    QVERIFY(source.contains(QStringLiteral("target_link_libraries(Congregation PRIVATE qt6keychain)")));
    QVERIFY(source.contains(QStringLiteral("MACOSX_PACKAGE_LOCATION \"Resources/legal/QtKeychain\"")));
    QVERIFY(source.contains(QStringLiteral("find_package(iiUpdateManager 0.2 CONFIG REQUIRED)")));
    QVERIFY(source.contains(QStringLiteral("iiUpdateManager::iiUpdateManager")));
    QVERIFY(source.contains(QStringLiteral("find_package(iiLicenseManager 0.2 CONFIG REQUIRED)")));
    QVERIFY(source.contains(QStringLiteral("iiLicenseManager::iiLicenseManager")));
    QVERIFY(source.contains(QStringLiteral(
        "MACOSX_PACKAGE_LOCATION \"Resources/legal/iiLicenseManager\"")));
    QVERIFY(source.contains(QStringLiteral("CONGREGATION_IILICENSEMANAGER_THIRD_PARTY_NOTICES")));
    QVERIFY(source.contains(QStringLiteral("find_package(iiSharedCanvas 0.10.1 EXACT CONFIG REQUIRED)")));
    QVERIFY(source.contains(QStringLiteral("find_package(iiFileProvider 0.5 CONFIG REQUIRED)")));
    QVERIFY(source.contains(QStringLiteral("iiSharedCanvas::iiSharedCanvas")));
}

void tst_MacOSBuildWorkflowContract::localNetworkDiscoveryDeclaresPrivacyAndSandboxAccess()
{
    const QString infoPlistPath = QFINDTESTDATA("../packaging/macos/Info.plist");
    QVERIFY2(!infoPlistPath.isEmpty(), "packaging/macos/Info.plist test data was not found");
    QFile infoPlist(infoPlistPath);
    QVERIFY(infoPlist.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString infoPlistSource = QString::fromUtf8(infoPlist.readAll());
    QVERIFY(infoPlistSource.contains(QStringLiteral("<key>NSLocalNetworkUsageDescription</key>")));
    QVERIFY(infoPlistSource.contains(QStringLiteral(
        "Congregation uses your local network to find nearby Congregation users and share a canvas when "
        "you choose.")));

    const QString entitlementsPath =
        QFINDTESTDATA("../packaging/macos/Congregation.entitlements");
    QVERIFY2(!entitlementsPath.isEmpty(), "Congregation.entitlements test data was not found");
    QFile entitlements(entitlementsPath);
    QVERIFY(entitlements.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString entitlementSource = QString::fromUtf8(entitlements.readAll());
    QVERIFY(entitlementSource.contains(QStringLiteral(
        "<key>com.apple.security.network.client</key>\n\t<true/>")));
    QVERIFY(entitlementSource.contains(QStringLiteral(
        "<key>com.apple.security.network.server</key>\n\t<true/>")));
}

void tst_MacOSBuildWorkflowContract::repositoryGuidelinesUseOnlyBuildDirectory()
{
    const QString agentsPath = QFINDTESTDATA("../AGENTS.md");
    QVERIFY2(!agentsPath.isEmpty(), "AGENTS.md test data was not found");

    QFile agentsFile(agentsPath);
    QVERIFY(agentsFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString agentsSource = QString::fromUtf8(agentsFile.readAll());

    QVERIFY(agentsSource.contains(QStringLiteral("repository-local `build/` directory")));
    QVERIFY(agentsSource.contains(QStringLiteral("alternate build trees are not supported")));
    QVERIFY(!agentsSource.contains(legacyClionBuildTreeName()));
}

void tst_MacOSBuildWorkflowContract::buildGuideSeparatesLocalAndDistributionSigning()
{
    const QString buildGuidePath = QFINDTESTDATA("../docs/BUILD.md");
    QVERIFY2(!buildGuidePath.isEmpty(), "docs/BUILD.md test data was not found");

    QFile buildGuide(buildGuidePath);
    QVERIFY(buildGuide.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = QString::fromUtf8(buildGuide.readAll());

    QVERIFY(source.contains(QStringLiteral("./build.sh local")));
    QVERIFY(source.contains(QStringLiteral("`./build.sh` defaults to the Developer ID distribution flow")));
    QVERIFY(source.contains(QStringLiteral("repository-local `build/` CMake binary directory")));
    QVERIFY(source.contains(QStringLiteral("alternate build trees are rejected")));
    QVERIFY(!source.contains(legacyClionBuildTreeName()));
    QVERIFY(source.contains(QStringLiteral("ctest --test-dir build --output-on-failure")));
    QVERIFY(source.contains(QStringLiteral("Apple Development")));
    QVERIFY(source.contains(QStringLiteral("dist/Congregation.pkg")));
    QVERIFY(source.contains(QStringLiteral("dist/Congregation-appstore.pkg")));
    QVERIFY(source.contains(QStringLiteral("dist/Congregation-local-unsigned.pkg")));
    QVERIFY(source.contains(QStringLiteral("dist/Congregation-appstore-local-unsigned.pkg")));
    QVERIFY(source.contains(QStringLiteral("CONGREGATION_BUILD_MODE=devid ./build.sh")));
    QVERIFY(source.contains(QStringLiteral("CONGREGATION_BUILD_MODE=mas ./build.sh")));
    QVERIFY(source.contains(QStringLiteral("Developer ID Application")));
    QVERIFY(source.contains(QStringLiteral("Apple Distribution")));
    QVERIFY(source.contains(QStringLiteral("NOTARY_APP_PASSWORD")));
    QVERIFY(source.contains(QStringLiteral("validates notarization credentials before configuring")));
    QVERIFY(source.contains(QStringLiteral("Bash 3.2")));
    QVERIFY(source.contains(QStringLiteral("CMAKE_EXTRA_ARGS")));
    QVERIFY(source.contains(QStringLiteral("Contents/Resources/Appicon.icns")));
    QVERIFY(source.contains(QStringLiteral("stale installer package")));
    QVERIFY(source.contains(QStringLiteral("pkgutil --payload-files dist/Congregation.pkg")));
    QVERIFY(source.contains(QStringLiteral("./Congregation.app/Contents/Resources/Appicon.icns")));
    QVERIFY(source.contains(QStringLiteral("./Congregation.app/Contents/Resources/icon.icns")));
    QVERIFY(source.contains(QStringLiteral("Transporter's Active list")));
    QVERIFY(source.contains(QStringLiteral("dist/Congregation-appstore.pkg")));
    QVERIFY(source.contains(QStringLiteral("App Store Connect record")));
    QVERIFY(source.contains(QStringLiteral("cmp resources/Appicon.icns")));
    QVERIFY(source.contains(QStringLiteral("Distribution `product`")));
}

void tst_MacOSBuildWorkflowContract::platformAppIconsAreBundledFromResources()
{
    const QString cmakePath = QFINDTESTDATA("../CMakeLists.txt");
    QVERIFY2(!cmakePath.isEmpty(), "CMakeLists.txt test data was not found");
    QFile cmakeFile(cmakePath);
    QVERIFY(cmakeFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString cmakeSource = QString::fromUtf8(cmakeFile.readAll());

    const QString infoPlistPath = QFINDTESTDATA("../packaging/macos/Info.plist");
    QVERIFY2(!infoPlistPath.isEmpty(), "packaging/macos/Info.plist test data was not found");
    QFile infoPlist(infoPlistPath);
    QVERIFY(infoPlist.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString infoPlistSource = QString::fromUtf8(infoPlist.readAll());

    const QString syncScriptPath = QFINDTESTDATA("../tools/sync_app_icon_assets.sh");
    QVERIFY2(!syncScriptPath.isEmpty(), "tools/sync_app_icon_assets.sh test data was not found");
    QFile syncScript(syncScriptPath);
    QVERIFY(syncScript.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString syncScriptSource = QString::fromUtf8(syncScript.readAll());

    const QString gitignorePath = QFINDTESTDATA("../.gitignore");
    QVERIFY2(!gitignorePath.isEmpty(), ".gitignore test data was not found");
    QFile gitignore(gitignorePath);
    QVERIFY(gitignore.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString gitignoreSource = QString::fromUtf8(gitignore.readAll());

    QVERIFY2(!QFINDTESTDATA("../resources/Appicon.icns").isEmpty(), "macOS app icon is missing");
    QVERIFY2(!QFINDTESTDATA("../resources/Appicon.ico").isEmpty(), "Windows app icon is missing");
    QVERIFY(cmakeSource.contains(QStringLiteral("set(CONGREGATION_MACOS_APP_ICON \"${CMAKE_SOURCE_DIR}/resources/Appicon.icns\")")));
    QVERIFY(cmakeSource.contains(QStringLiteral("set(CONGREGATION_WINDOWS_APP_ICON \"${CMAKE_SOURCE_DIR}/resources/Appicon.ico\")")));
    QVERIFY(cmakeSource.contains(QStringLiteral("set(CONGREGATION_LEGACY_MACOS_APP_ICON_FILE \"icon.icns\")")));
    QVERIFY(cmakeSource.contains(QStringLiteral("MACOSX_PACKAGE_LOCATION \"Resources\"")));
    QVERIFY(cmakeSource.contains(QStringLiteral("MACOSX_BUNDLE_ICON_FILE \"${CONGREGATION_MACOS_APP_ICON_FILE}\"")));
    QVERIFY(cmakeSource.contains(QStringLiteral("$<TARGET_BUNDLE_DIR:Congregation>/Contents/Resources/${CONGREGATION_LEGACY_MACOS_APP_ICON_FILE}")));
    QVERIFY(cmakeSource.contains(QStringLiteral("configure_file(\"${CONGREGATION_WINDOWS_RESOURCE_TEMPLATE}\"")));
    QVERIFY(cmakeSource.contains(QStringLiteral("target_sources(Congregation PRIVATE \"${_congregation_windows_resource_file}\")")));
    QVERIFY(infoPlistSource.contains(QStringLiteral("<string>@CONGREGATION_MACOS_APP_ICON_FILE@</string>")));
    QVERIFY(syncScriptSource.contains(QStringLiteral("resources/Appicon.icns")));
    QVERIFY(syncScriptSource.contains(QStringLiteral("packaging/macos/Congregation.xcassets/AppIcon.appiconset")));
    QVERIFY(syncScriptSource.contains(QStringLiteral("iconutil -c iconset")));
    QVERIFY(syncScriptSource.contains(QStringLiteral("AppIcon-1024.png")));
    QVERIFY(gitignoreSource.contains(QStringLiteral("!/resources/Appicon.icns")));
    QVERIFY(gitignoreSource.contains(QStringLiteral("!/resources/Appicon.ico")));
}

void tst_MacOSBuildWorkflowContract::buildScriptUsesIncrementalBuildsAndStripsDistributionBundles()
{
    const QString buildScriptPath = QFINDTESTDATA("../build.sh");
    QVERIFY2(!buildScriptPath.isEmpty(), "build.sh test data was not found");
    QFile buildScript(buildScriptPath);
    QVERIFY(buildScript.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = QString::fromUtf8(buildScript.readAll());

    QVERIFY(source.contains(QStringLiteral("Usage: ./build.sh [--clean] [local|devid|mas|all]")));
    QVERIFY(source.contains(QStringLiteral("QML_DIR=\"./src/App/qml\"")));
    QVERIFY(source.contains(QStringLiteral("BUILD_MODE=\"${CONGREGATION_BUILD_MODE:-devid}\"")));
    QVERIFY(source.contains(QStringLiteral("OUT_LOCAL_DEVID_PKG=\"${DIST_DIR}/${APP_NAME}-local-unsigned.pkg\"")));
    QVERIFY(source.contains(QStringLiteral("OUT_LOCAL_MAS_PKG=\"${DIST_DIR}/${APP_NAME}-appstore-local-unsigned.pkg\"")));
    QVERIFY(source.contains(QStringLiteral("validate_notary_credentials()")));
    QVERIFY(source.contains(QStringLiteral("xcrun notarytool history \"${NOTARY_AUTH_ARGS[@]}\"")));
    const qsizetype notaryPreflightIndex = source.indexOf(QStringLiteral("validate_notary_credentials\n"));
    const qsizetype configureIndex = source.indexOf(QStringLiteral("say \"=== configure ===\""));
    QVERIFY(notaryPreflightIndex >= 0);
    QVERIFY(configureIndex > notaryPreflightIndex);
    QVERIFY(source.contains(QStringLiteral("CLEAN_BUILD_DIR=\"${CLEAN_BUILD_DIR:-0}\"")));
    QVERIFY(source.contains(QStringLiteral("--clean)")));
    QCOMPARE(source.count(QStringLiteral("CLEAN_BUILD_DIR=\"1\"")), 1);
    QVERIFY(source.contains(QStringLiteral("MACDEPLOYQT_NO_STRIP=\"${MACDEPLOYQT_NO_STRIP:-}\"")));
    QVERIFY(source.contains(QStringLiteral("if [[ -z \"$MACDEPLOYQT_NO_STRIP\" ]]; then")));
    QVERIFY(source.contains(QStringLiteral("MACDEPLOYQT_NO_STRIP=\"1\"")));
    QVERIFY(source.contains(QStringLiteral("MACDEPLOYQT_NO_STRIP=\"0\"")));
    QVERIFY(source.contains(QStringLiteral("if [[ \"$MACDEPLOYQT_NO_STRIP\" == \"1\" ]]; then cmd+=(\"-no-strip\"); fi")));
    QVERIFY(source.contains(QStringLiteral("need_cmd /usr/bin/otool")));
    QVERIFY(source.contains(QStringLiteral("need_cmd /usr/bin/install_name_tool")));
    QVERIFY(source.contains(QStringLiteral("remove_absolute_macho_rpaths()")));
    QVERIFY(source.contains(QStringLiteral("/usr/bin/install_name_tool -delete_rpath")));
    QVERIFY(source.contains(QStringLiteral("assert_portable_macho_links()")));
    QVERIFY(source.contains(QStringLiteral("assert_update_manager_runtime()")));
    QVERIFY(source.contains(QStringLiteral("libiiUpdateManager*.dylib")));
    QVERIFY(source.contains(QStringLiteral("assert_update_manager_runtime \"$out_app\"")));
    QVERIFY(source.contains(QStringLiteral("package payload is missing the iiUpdateManager runtime")));
    QVERIFY(source.contains(QStringLiteral("assert_license_manager_runtime()")));
    QVERIFY(source.contains(QStringLiteral("libiiLicenseManager*.dylib")));
    QVERIFY(source.contains(QStringLiteral("assert_license_manager_runtime \"$out_app\"")));
    QVERIFY(source.contains(QStringLiteral("package payload is missing the iiLicenseManager runtime")));
    QVERIFY(source.contains(QStringLiteral("IISACCDistributionChannel")));
    QVERIFY(source.contains(QStringLiteral("distribution_channel=\"direct\"")));
    QVERIFY(source.contains(QStringLiteral("distribution_channel=\"appstore\"")));
    QVERIFY(source.contains(QStringLiteral("verify_pkg_distribution \"$out_pkg\" \"$pkg_id\" \"pkg-ref\"")));
    QVERIFY(source.contains(QStringLiteral("verify_pkg_distribution \"$out_pkg\" \"$pkg_id\" \"product\"")));
    QVERIFY(source.contains(QStringLiteral("/usr/bin/otool -L \"$binary\"")));
    QVERIFY(source.contains(QStringLiteral("/usr/bin/otool -D \"$binary\"")));
    QVERIFY(source.contains(QStringLiteral("sed -nE 's/^[[:space:]]+([^[:space:]]+).*/\\1/p'")));
    QVERIFY(source.contains(QStringLiteral("grep -Fx \"$dependency\" <<<\"$dylib_ids\"")));
    QVERIFY(source.contains(QStringLiteral("/usr/bin/otool -l \"$binary\"")));
    QVERIFY(source.contains(QStringLiteral("LC_RPATH")));
    QVERIFY(source.contains(QStringLiteral("deployed app contains non-portable absolute Mach-O paths")));
    QVERIFY(source.contains(QStringLiteral("remove_unused_qt_sql_plugins()")));
    QVERIFY(source.contains(QStringLiteral("rm -rf \"$app/Contents/PlugIns/sqldrivers\"")));
    QVERIFY(source.contains(QStringLiteral("remove_unused_qt_sql_plugins \"$out_app\"")));
    const qsizetype normalizeRpathIndex = source.indexOf(
            QStringLiteral("remove_absolute_macho_rpaths \"$out_app\""));
    const qsizetype assertPortableIndex = source.indexOf(
            QStringLiteral("assert_portable_macho_links \"$out_app\""));
    QVERIFY(normalizeRpathIndex >= 0);
    QVERIFY(assertPortableIndex > normalizeRpathIndex);
    QVERIFY(source.contains(QStringLiteral("assert_portable_macho_links \"$out_app\"")));
}

void tst_MacOSBuildWorkflowContract::buildScriptRunsLocalModeWithEmptyOptionalArgumentArrays()
{
#ifndef Q_OS_MACOS
    QSKIP("build.sh depends on macOS packaging tools");
#else
    const QString gitignorePath = QFINDTESTDATA("../.gitignore");
    QVERIFY2(!gitignorePath.isEmpty(), ".gitignore test data was not found");

    QFile gitignoreFile(gitignorePath);
    QVERIFY(gitignoreFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString gitignoreSource = QString::fromUtf8(gitignoreFile.readAll());
    QVERIFY(!gitignoreSource.contains(QStringLiteral("\nbuild.sh\n")));
    QVERIFY(!gitignoreSource.contains(QStringLiteral("\n/build.sh\n")));

    const QString sourceBuildScriptPath = QFINDTESTDATA("../build.sh");
    if (sourceBuildScriptPath.isEmpty()) {
        QSKIP("build.sh is a local workflow script and is not available in this checkout");
    }

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QDir temp(tempDir.path());
    QVERIFY(temp.mkpath(QStringLiteral("bin")));
    QVERIFY(temp.mkpath(QStringLiteral("src/App/qml")));
    const QString updateManagerPrefix = temp.filePath(QStringLiteral("iiUpdateManager"));
    QVERIFY(temp.mkpath(QStringLiteral("iiUpdateManager/lib/cmake/iiUpdateManager")));
    QFile updateManagerConfig(
        temp.filePath(QStringLiteral(
            "iiUpdateManager/lib/cmake/iiUpdateManager/iiUpdateManagerConfig.cmake")));
    QVERIFY(updateManagerConfig.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(updateManagerConfig.write("# fake installed dependency\n") > 0);
    updateManagerConfig.close();
    const QString licenseManagerPrefix = temp.filePath(QStringLiteral("iiLicenseManager"));
    QVERIFY(temp.mkpath(QStringLiteral("iiLicenseManager/lib/cmake/iiLicenseManager")));
    QFile licenseManagerConfig(
        temp.filePath(QStringLiteral(
            "iiLicenseManager/lib/cmake/iiLicenseManager/iiLicenseManagerConfig.cmake")));
    QVERIFY(licenseManagerConfig.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(licenseManagerConfig.write("# fake installed dependency\n") > 0);
    licenseManagerConfig.close();

    const QString buildScriptPath = temp.filePath(QStringLiteral("build.sh"));
    QVERIFY(QFile::copy(sourceBuildScriptPath, buildScriptPath));
    QVERIFY(QFile::setPermissions(buildScriptPath,
                                  QFileDevice::ReadOwner
                                          | QFileDevice::WriteOwner
                                          | QFileDevice::ExeOwner
                                          | QFileDevice::ReadGroup
                                          | QFileDevice::ExeGroup
                                          | QFileDevice::ReadOther
                                          | QFileDevice::ExeOther));

    const QString binDir = temp.filePath(QStringLiteral("bin"));
    const QByteArray fakeCmake = R"SH(#!/bin/sh
set -eu

if [ "${1:-}" = "--build" ]; then
    exit 0
fi

build_dir=""
expect_build_dir=0
for arg in "$@"; do
    if [ "$expect_build_dir" = "1" ]; then
        build_dir="$arg"
        expect_build_dir=0
        continue
    fi
    if [ "$arg" = "-B" ]; then
        expect_build_dir=1
    fi
done

if [ -z "$build_dir" ]; then
    build_dir="./build"
fi

mkdir -p "$build_dir/Congregation.app/Contents/MacOS" "$build_dir/Congregation.app/Contents/Resources"
cat > "$build_dir/Congregation.app/Contents/MacOS/Congregation" <<'APP'
#!/bin/sh
exit 0
APP
chmod +x "$build_dir/Congregation.app/Contents/MacOS/Congregation"
printf 'fake icon\n' > "$build_dir/Congregation.app/Contents/Resources/Appicon.icns"
cat > "$build_dir/Congregation.app/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
        "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleIconFile</key>
    <string>Appicon.icns</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0.0</string>
    <key>CFBundleVersion</key>
    <string>1</string>
</dict>
</plist>
PLIST
)SH";

    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("cmake")), fakeCmake));
    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("macdeployqt")),
                            QByteArray("#!/bin/sh\n"
                                       "set -eu\n"
                                       "mkdir -p \"$1/Contents/PlugIns/sqldrivers\"\n"
                                       "printf 'unused SQL plugin\\n' > \"$1/Contents/PlugIns/sqldrivers/libunused.dylib\"\n")));
    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("codesign")),
                            QByteArray("#!/bin/sh\nexit 0\n")));
    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("security")),
                            QByteArray("#!/bin/sh\nexit 0\n")));
    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("ninja")),
                            QByteArray("#!/bin/sh\nexit 0\n")));
    const QByteArray fakeXcrun = R"SH(#!/bin/sh
set -eu

if [ "${1:-}" = "--find" ]; then
    printf '/usr/bin/%s\n' "$2"
    exit 0
fi

tool="${1:-}"
shift || true
case "$tool" in
    productbuild)
        out=""
        for arg in "$@"; do
            out="$arg"
        done
        mkdir -p "$(dirname "$out")"
        printf 'fake package\n' > "$out"
        ;;
    *)
        printf 'unsupported fake xcrun tool: %s\n' "$tool" >&2
        exit 1
        ;;
esac
)SH";
    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("xcrun")), fakeXcrun));

    const QByteArray fakePkgutil = R"SH(#!/bin/sh
set -eu

case "${1:-}" in
    --check-signature)
        printf 'Package "%s":\n   Status: no signature\n' "$(basename "${2:-}")"
        ;;
    --payload-files)
        printf './Congregation.app/Contents/MacOS/Congregation\n'
        printf './Congregation.app/Contents/Resources/Appicon.icns\n'
        printf './Congregation.app/Contents/Frameworks/libiiUpdateManager.0.dylib\n'
        printf './Congregation.app/Contents/Frameworks/libiiLicenseManager.0.dylib\n'
        ;;
    --expand)
        mkdir -p "${3:-}"
        case "${2:-}" in
            *appstore*) product_id="com.iisacc.congregation.painter" ;;
            *) product_id="com.iisacc.app.congregation.pkg" ;;
        esac
        cat > "${3:-}/Distribution" <<DISTRIBUTION
<installer-gui-script minSpecVersion="2" hostArchitectures="arm64">
<allowed-os-versions><os-version min="12.0"/></allowed-os-versions>
<bundle path="Congregation.app" CFBundleShortVersionString="1.0.0" CFBundleVersion="1"/>
<product id="$product_id" version="1.0.0"/>
<pkg-ref id="com.iisacc.congregation.painter" version="1.0.0"/>
</installer-gui-script>
DISTRIBUTION
        ;;
    *)
        printf 'unsupported fake pkgutil command: %s\n' "${1:-}" >&2
        exit 1
        ;;
esac
)SH";
    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("pkgutil")), fakePkgutil));

    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("PATH"), binDir + QStringLiteral(":") + environment.value(QStringLiteral("PATH")));
    environment.insert(QStringLiteral("RUN_TESTS"), QStringLiteral("0"));
    environment.insert(QStringLiteral("CONGREGATION_BUILD_MODE"), QStringLiteral("local"));
    environment.insert(QStringLiteral("IIUPDATEMANAGER_PREFIX"), updateManagerPrefix);
    environment.insert(QStringLiteral("IILICENSEMANAGER_PREFIX"), licenseManagerPrefix);
    process.setProcessEnvironment(environment);
    process.setWorkingDirectory(tempDir.path());
    process.start(QStringLiteral("/bin/bash"), QStringList{QStringLiteral("./build.sh")});

    QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
    QVERIFY2(process.waitForFinished(30000), qPrintable(process.errorString()));

    const QString output = QString::fromUtf8(process.readAllStandardOutput() + process.readAllStandardError());
    QVERIFY2(process.exitStatus() == QProcess::NormalExit, qPrintable(output));
    QVERIFY2(process.exitCode() == 0, qPrintable(output));
    QVERIFY2(!output.contains(QStringLiteral("unbound variable")), qPrintable(output));
    QVERIFY2(output.contains(QStringLiteral("done")), qPrintable(output));
    QVERIFY2(!QDir(temp.filePath(QStringLiteral("dist/Congregation.app/Contents/PlugIns/sqldrivers"))).exists(),
             qPrintable(output));
    QVERIFY2(QFile::exists(temp.filePath(QStringLiteral("dist/Congregation-local-unsigned.pkg"))), qPrintable(output));
    QVERIFY2(QFile::exists(temp.filePath(QStringLiteral("dist/Congregation-appstore-local-unsigned.pkg"))), qPrintable(output));
    QVERIFY2(!QFile::exists(temp.filePath(QStringLiteral("dist/Congregation.pkg"))), qPrintable(output));
    QVERIFY2(!QFile::exists(temp.filePath(QStringLiteral("dist/Congregation-appstore.pkg"))), qPrintable(output));
    QFile stagedInfoPlist(temp.filePath(QStringLiteral("dist/Congregation.app/Contents/Info.plist")));
    QVERIFY(stagedInfoPlist.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString stagedInfoSource = QString::fromUtf8(stagedInfoPlist.readAll());
    QVERIFY(stagedInfoSource.contains(QStringLiteral("IISACCDistributionChannel")));
    QVERIFY(stagedInfoSource.contains(QStringLiteral("<string>direct</string>")));
#endif
}

void tst_MacOSBuildWorkflowContract::buildScriptFailsClosedOnMissingNotaryProfileBeforeBuild()
{
#ifndef Q_OS_MACOS
    QSKIP("build.sh depends on macOS packaging tools");
#else
    const QString sourceBuildScriptPath = QFINDTESTDATA("../build.sh");
    QVERIFY2(!sourceBuildScriptPath.isEmpty(), "build.sh test data was not found");

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QDir temp(tempDir.path());
    QVERIFY(temp.mkpath(QStringLiteral("bin")));
    QVERIFY(temp.mkpath(QStringLiteral("src/App/qml")));
    const QString updateManagerPrefix = temp.filePath(QStringLiteral("iiUpdateManager"));
    QVERIFY(temp.mkpath(QStringLiteral("iiUpdateManager/lib/cmake/iiUpdateManager")));
    QFile updateManagerConfig(
        temp.filePath(QStringLiteral(
            "iiUpdateManager/lib/cmake/iiUpdateManager/iiUpdateManagerConfig.cmake")));
    QVERIFY(updateManagerConfig.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(updateManagerConfig.write("# fake installed dependency\n") > 0);
    updateManagerConfig.close();
    const QString licenseManagerPrefix = temp.filePath(QStringLiteral("iiLicenseManager"));
    QVERIFY(temp.mkpath(QStringLiteral("iiLicenseManager/lib/cmake/iiLicenseManager")));
    QFile licenseManagerConfig(
        temp.filePath(QStringLiteral(
            "iiLicenseManager/lib/cmake/iiLicenseManager/iiLicenseManagerConfig.cmake")));
    QVERIFY(licenseManagerConfig.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(licenseManagerConfig.write("# fake installed dependency\n") > 0);
    licenseManagerConfig.close();

    const QString buildScriptPath = temp.filePath(QStringLiteral("build.sh"));
    QVERIFY(QFile::copy(sourceBuildScriptPath, buildScriptPath));
    QVERIFY(QFile::setPermissions(buildScriptPath,
                                  QFileDevice::ReadOwner
                                          | QFileDevice::WriteOwner
                                          | QFileDevice::ExeOwner
                                          | QFileDevice::ReadGroup
                                          | QFileDevice::ExeGroup
                                          | QFileDevice::ReadOther
                                          | QFileDevice::ExeOther));

    const QString binDir = temp.filePath(QStringLiteral("bin"));
    const QByteArray fakeCmake = R"SH(#!/bin/sh
set -eu
printf 'cmake must not run before notarization credential validation\n' > cmake-was-run
exit 1
)SH";
    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("cmake")), fakeCmake));
    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("macdeployqt")),
                            QByteArray("#!/bin/sh\nexit 0\n")));
    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("codesign")),
                            QByteArray("#!/bin/sh\nexit 0\n")));
    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("ninja")),
                            QByteArray("#!/bin/sh\nexit 0\n")));
    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("pkgutil")),
                            QByteArray("#!/bin/sh\nexit 0\n")));

    const QByteArray fakeSecurity = R"SH(#!/bin/sh
set -eu

case "$*" in
    *"find-identity -h"*)
        printf 'Supported policies: basic, codesigning, installer\n'
        ;;
    *"-p codesigning"*)
        printf '  1) AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA "Developer ID Application: MUYEONG YUN (5U49ST9XZH)"\n'
        printf '     1 valid identities found\n'
        ;;
    *"-p installer"*)
        printf '  1) BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB "Developer ID Installer: MUYEONG YUN (5U49ST9XZH)"\n'
        printf '     1 valid identities found\n'
        ;;
    *)
        exit 1
        ;;
esac
)SH";
    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("security")), fakeSecurity));

    const QByteArray fakeXcrun = R"SH(#!/bin/sh
set -eu

if [ "${1:-}" = "--find" ]; then
    printf '/usr/bin/%s\n' "$2"
    exit 0
fi

if [ "${1:-}" = "notarytool" ] && [ "${2:-}" = "history" ]; then
    printf 'Error: No Keychain password item found for profile: notary-main\n' >&2
    exit 69
fi

printf 'unsupported fake xcrun invocation: %s\n' "$*" >&2
exit 1
)SH";
    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("xcrun")), fakeXcrun));

    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("PATH"), binDir + QStringLiteral(":") + environment.value(QStringLiteral("PATH")));
    environment.insert(QStringLiteral("RUN_TESTS"), QStringLiteral("0"));
    environment.insert(QStringLiteral("IIUPDATEMANAGER_PREFIX"), updateManagerPrefix);
    environment.insert(QStringLiteral("IILICENSEMANAGER_PREFIX"), licenseManagerPrefix);
    environment.remove(QStringLiteral("CONGREGATION_BUILD_MODE"));
    environment.remove(QStringLiteral("NOTARY_APP_PASSWORD"));
    process.setProcessEnvironment(environment);
    process.setWorkingDirectory(tempDir.path());
    process.start(QStringLiteral("/bin/bash"), QStringList{QStringLiteral("./build.sh")});

    QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
    QVERIFY2(process.waitForFinished(30000), qPrintable(process.errorString()));

    const QString output = QString::fromUtf8(process.readAllStandardOutput() + process.readAllStandardError());
    QVERIFY2(process.exitStatus() == QProcess::NormalExit, qPrintable(output));
    QVERIFY2(process.exitCode() != 0, qPrintable(output));
    QVERIFY2(output.contains(QStringLiteral("notarization credential preflight failed")), qPrintable(output));
    QVERIFY2(!output.contains(QStringLiteral("=== configure ===")), qPrintable(output));
    QVERIFY2(!QFile::exists(temp.filePath(QStringLiteral("cmake-was-run"))), qPrintable(output));
#endif
}

QTEST_APPLESS_MAIN(tst_MacOSBuildWorkflowContract)

#include "tst_macosbuildworkflowcontract.moc"
