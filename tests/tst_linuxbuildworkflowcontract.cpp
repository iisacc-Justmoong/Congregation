#include <QFile>
#include <QString>
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

class tst_LinuxBuildWorkflowContract : public QObject
{
    Q_OBJECT

private slots:
    void cmakeBuildsDeployableLinuxArchive();
    void desktopEntryLaunchesOnlyTheGuiApplication();
    void documentationMatchesTheInstalledLinuxLayout();
};

void tst_LinuxBuildWorkflowContract::cmakeBuildsDeployableLinuxArchive()
{
    const QString cmakePath = QFINDTESTDATA("../CMakeLists.txt");
    QVERIFY2(!cmakePath.isEmpty(), "CMakeLists.txt test data was not found");
    const QString source = readTextFile(cmakePath);
    QVERIFY(!source.isEmpty());

    QVERIFY(source.contains(QStringLiteral("if(CMAKE_HOST_SYSTEM_NAME STREQUAL \"Darwin\" AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)")));
    QVERIFY(source.contains(QStringLiteral("elseif(CMAKE_SYSTEM_NAME STREQUAL \"Linux\")\n"
                                            "    list(APPEND _congregation_local_dependency_runtime_candidates")));
    QVERIFY(source.contains(QStringLiteral("$ENV{HOME}/.local/SDK/LVRS/platforms/linux/lib")));
    QVERIFY(source.contains(QStringLiteral("$ENV{HOME}/.local/SDK/iiPaintEngine/platforms/linux/lib")));
    QVERIFY(source.contains(QStringLiteral("$ENV{HOME}/.local/SDK/iiSharedCanvas/lib")));
    QVERIFY(source.contains(QStringLiteral("$ENV{HOME}/.local/SDK/iiUpdateManager/lib")));
    QVERIFY(source.contains(QStringLiteral("$ENV{HOME}/.local/SDK/iiLicenseManager/lib")));
    QVERIFY(!source.contains(QStringLiteral("$ENV{HOME}/.local/SDK/LVRS/platforms/macos\"\n"
                                             "        \"$ENV{HOME}/.local/SDK/LVRS")));

    QVERIFY(source.contains(QStringLiteral("install(TARGETS Congregation RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)")));
    QVERIFY(source.contains(QStringLiteral("set_property(TARGET Congregation PROPERTY\n"
                                            "            INSTALL_RPATH \"$ORIGIN/../${CMAKE_INSTALL_LIBDIR}\")")));
    QVERIFY(source.contains(QStringLiteral("qt_generate_deploy_qml_app_script(")));
    QVERIFY(source.contains(QStringLiteral("TARGET Congregation")));
    QVERIFY(source.contains(QStringLiteral("OUTPUT_SCRIPT _congregation_linux_deploy_script")));
    QVERIFY(source.contains(QStringLiteral("install(SCRIPT \"${_congregation_linux_deploy_script}\" COMPONENT Runtime)")));
    QVERIFY(!source.contains(QStringLiteral("NO_IMPORT_SCAN")));

    QVERIFY(source.contains(QStringLiteral("packaging/linux/com.iisacc.congregation.painter.desktop")));
    QVERIFY(source.contains(QStringLiteral("${CMAKE_INSTALL_DATAROOTDIR}/applications")));
    QVERIFY(source.contains(QStringLiteral("${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps")));
    QVERIFY(source.contains(QStringLiteral(
        "${CMAKE_INSTALL_DATAROOTDIR}/doc/Congregation/legal/iiLicenseManager")));
    QVERIFY(source.contains(QStringLiteral("set(CPACK_GENERATOR \"TGZ\")")));
    QVERIFY(source.contains(QStringLiteral("set(CPACK_PACKAGE_FILE_NAME \"Congregation-${CPACK_PACKAGE_VERSION}-Linux\")")));
    QVERIFY(source.contains(QStringLiteral("set(CPACK_PACKAGING_INSTALL_PREFIX \"/\")")));
}

void tst_LinuxBuildWorkflowContract::desktopEntryLaunchesOnlyTheGuiApplication()
{
    const QString desktopPath = QFINDTESTDATA("../packaging/linux/com.iisacc.congregation.painter.desktop");
    QVERIFY2(!desktopPath.isEmpty(), "Linux desktop entry test data was not found");
    const QString source = readTextFile(desktopPath);
    QVERIFY(!source.isEmpty());

    QVERIFY(source.startsWith(QStringLiteral("[Desktop Entry]\n")));
    QVERIFY(source.contains(QStringLiteral("Type=Application\n")));
    QVERIFY(source.contains(QStringLiteral("Name=Congregation\n")));
    QVERIFY(source.contains(QStringLiteral("Exec=Congregation %F\n")));
    QVERIFY(source.contains(QStringLiteral("Icon=com.iisacc.congregation.painter\n")));
    QVERIFY(source.contains(QStringLiteral("Terminal=false\n")));
    QVERIFY(source.contains(QStringLiteral("Categories=Graphics;2DGraphics;RasterGraphics;\n")));
    QVERIFY(source.contains(QStringLiteral("StartupWMClass=Congregation\n")));

    const QString gitIgnorePath = QFINDTESTDATA("../.gitignore");
    QVERIFY2(!gitIgnorePath.isEmpty(), ".gitignore test data was not found");
    const QString gitIgnore = readTextFile(gitIgnorePath);
    QVERIFY(gitIgnore.contains(QStringLiteral("!/packaging/linux/")));
    QVERIFY(gitIgnore.contains(QStringLiteral("!/packaging/linux/com.iisacc.congregation.painter.desktop")));
}

void tst_LinuxBuildWorkflowContract::documentationMatchesTheInstalledLinuxLayout()
{
    const QString readmePath = QFINDTESTDATA("../README.md");
    const QString buildGuidePath = QFINDTESTDATA("../docs/BUILD.md");
    QVERIFY2(!readmePath.isEmpty(), "README.md test data was not found");
    QVERIFY2(!buildGuidePath.isEmpty(), "docs/BUILD.md test data was not found");
    const QString readme = readTextFile(readmePath);
    const QString buildGuide = readTextFile(buildGuidePath);

    QVERIFY(readme.contains(QStringLiteral("`bin/Congregation`")));
    QVERIFY(readme.contains(QStringLiteral(
        "Qt, LVRS, iiPaintEngine, iiSharedCanvas, iiUpdateManager, and iiLicenseManager shared runtimes")));
    QVERIFY(readme.contains(QStringLiteral("native macOS global menu bar")));
    QVERIFY(readme.contains(QStringLiteral("Windows and Linux use a compact dark in-window menu bar")));
    QVERIFY(buildGuide.contains(QStringLiteral("qt_generate_deploy_qml_app_script")));
    QVERIFY(buildGuide.contains(QStringLiteral("X11 and Wayland")));
    QVERIFY(buildGuide.contains(QStringLiteral("$ORIGIN/../lib")));
}

QTEST_APPLESS_MAIN(tst_LinuxBuildWorkflowContract)

#include "tst_linuxbuildworkflowcontract.moc"
