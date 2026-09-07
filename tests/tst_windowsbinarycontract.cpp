#include <QByteArray>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QScopeGuard>
#include <QSet>
#include <QSize>
#include <QString>
#include <QtEndian>
#include <QtTest>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winver.h>

namespace
{
constexpr quint16 imageFileMachineAmd64 = 0x8664;
constexpr quint16 imageNtOptionalHdr64Magic = 0x020b;
constexpr quint16 imageSubsystemWindowsGui = 2;
constexpr quint16 requiredDllCharacteristics = IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA
    | IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE | IMAGE_DLLCHARACTERISTICS_NX_COMPAT;

QString executablePath()
{
    return QString::fromUtf8(CONGREGATION_EXECUTABLE_PATH);
}

quint16 readUInt16(const QByteArray &bytes, qsizetype offset)
{
    if (offset < 0 || offset + qsizetype(sizeof(quint16)) > bytes.size()) {
        return 0;
    }
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(bytes.constData() + offset));
}

quint32 readUInt32(const QByteArray &bytes, qsizetype offset)
{
    if (offset < 0 || offset + qsizetype(sizeof(quint32)) > bytes.size()) {
        return 0;
    }
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData() + offset));
}

class ScopedModule
{
public:
    explicit ScopedModule(const QString &path)
        : module(LoadLibraryExW(reinterpret_cast<LPCWSTR>(path.utf16()),
                                nullptr,
                                LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE))
    {
    }

    ~ScopedModule()
    {
        if (module) {
            FreeLibrary(module);
        }
    }

    HMODULE get() const
    {
        return module;
    }

private:
    HMODULE module = nullptr;
};

struct VisibleWindowSearch
{
    DWORD processId = 0;
    HWND window = nullptr;
};

BOOL CALLBACK findVisibleTopLevelWindow(HWND window, LPARAM context)
{
    auto *search = reinterpret_cast<VisibleWindowSearch *>(context);
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId == search->processId && IsWindowVisible(window)
        && GetWindow(window, GW_OWNER) == nullptr) {
        search->window = window;
        return FALSE;
    }
    return TRUE;
}

HWND visibleTopLevelWindowForProcess(qint64 processId)
{
    VisibleWindowSearch search{DWORD(processId), nullptr};
    EnumWindows(findVisibleTopLevelWindow, reinterpret_cast<LPARAM>(&search));
    return search.window;
}
}

class tst_WindowsBinaryContract : public QObject
{
    Q_OBJECT

private slots:
    void peHeaderUsesNativeWindowsApplicationContract();
    void manifestDeclaresNativeWindowsCapabilities();
    void versionResourceMatchesProjectVersion();
    void launchWindowSizeStaysConstant();
};

void tst_WindowsBinaryContract::peHeaderUsesNativeWindowsApplicationContract()
{
    QFile executable(executablePath());
    QVERIFY2(executable.open(QIODevice::ReadOnly), qPrintable(executable.errorString()));
    const QByteArray bytes = executable.readAll();

    QVERIFY(bytes.size() >= 0x100);
    QCOMPARE(readUInt16(bytes, 0), quint16(IMAGE_DOS_SIGNATURE));
    const qsizetype peOffset = readUInt32(bytes, 0x3c);
    QVERIFY(peOffset > 0);
    QVERIFY(peOffset + 96 <= bytes.size());
    QCOMPARE(readUInt32(bytes, peOffset), quint32(IMAGE_NT_SIGNATURE));

    const qsizetype coffHeaderOffset = peOffset + 4;
    const qsizetype optionalHeaderOffset = coffHeaderOffset + 20;
    QCOMPARE(readUInt16(bytes, coffHeaderOffset), imageFileMachineAmd64);
    QCOMPARE(readUInt16(bytes, optionalHeaderOffset), imageNtOptionalHdr64Magic);
    QCOMPARE(readUInt16(bytes, optionalHeaderOffset + 68), imageSubsystemWindowsGui);

    const quint16 dllCharacteristics = readUInt16(bytes, optionalHeaderOffset + 70);
    QCOMPARE(dllCharacteristics & requiredDllCharacteristics, requiredDllCharacteristics);
}

void tst_WindowsBinaryContract::manifestDeclaresNativeWindowsCapabilities()
{
    const QString path = executablePath();
    ACTCTXW activationContext{};
    activationContext.cbSize = sizeof(activationContext);
    activationContext.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID;
    activationContext.lpSource = reinterpret_cast<LPCWSTR>(path.utf16());
    activationContext.lpResourceName = MAKEINTRESOURCEW(1);
    const HANDLE contextHandle = CreateActCtxW(&activationContext);
    QVERIFY2(contextHandle != INVALID_HANDLE_VALUE,
             qPrintable(QStringLiteral("CreateActCtxW failed with Windows error %1").arg(GetLastError())));
    ReleaseActCtx(contextHandle);

    const ScopedModule executable(path);
    QVERIFY2(executable.get(), "Congregation.exe could not be opened as a native resource image");

    const HRSRC manifestResource = FindResourceW(executable.get(), MAKEINTRESOURCEW(1), RT_MANIFEST);
    QVERIFY2(manifestResource, "Congregation.exe does not contain RT_MANIFEST resource 1");
    const HGLOBAL loadedManifest = LoadResource(executable.get(), manifestResource);
    QVERIFY(loadedManifest);
    const DWORD manifestSize = SizeofResource(executable.get(), manifestResource);
    const void *manifestData = LockResource(loadedManifest);
    QVERIFY(manifestData);

    const QString manifest = QString::fromUtf8(static_cast<const char *>(manifestData), manifestSize);
    QVERIFY(manifest.contains(QStringLiteral("requestedExecutionLevel level=\"asInvoker\"")));
    QVERIFY(manifest.contains(QStringLiteral("{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}")));
    QVERIFY(manifest.contains(QStringLiteral("PerMonitorV2,PerMonitor")));
    QVERIFY(manifest.contains(QStringLiteral("longPathAware")));
}

void tst_WindowsBinaryContract::versionResourceMatchesProjectVersion()
{
    const QString path = executablePath();
    DWORD ignoredHandle = 0;
    const DWORD versionInfoSize = GetFileVersionInfoSizeW(reinterpret_cast<LPCWSTR>(path.utf16()),
                                                          &ignoredHandle);
    QVERIFY2(versionInfoSize > 0, "Congregation.exe does not contain a VERSIONINFO resource");

    QByteArray versionData(versionInfoSize, Qt::Uninitialized);
    QVERIFY(GetFileVersionInfoW(reinterpret_cast<LPCWSTR>(path.utf16()),
                               0,
                               versionInfoSize,
                               versionData.data()));

    VS_FIXEDFILEINFO *fixedInfo = nullptr;
    UINT fixedInfoSize = 0;
    QVERIFY(VerQueryValueW(versionData.data(),
                          L"\\",
                          reinterpret_cast<void **>(&fixedInfo),
                          &fixedInfoSize));
    QVERIFY(fixedInfo);
    QVERIFY(fixedInfoSize >= sizeof(VS_FIXEDFILEINFO));
    QCOMPARE(HIWORD(fixedInfo->dwFileVersionMS), WORD(6));
    QCOMPARE(LOWORD(fixedInfo->dwFileVersionMS), WORD(0));
    QCOMPARE(HIWORD(fixedInfo->dwFileVersionLS), WORD(0));
    QCOMPARE(LOWORD(fixedInfo->dwFileVersionLS), WORD(0));
    QCOMPARE(fixedInfo->dwProductVersionMS, fixedInfo->dwFileVersionMS);
    QCOMPARE(fixedInfo->dwProductVersionLS, fixedInfo->dwFileVersionLS);

    wchar_t *productName = nullptr;
    UINT productNameLength = 0;
    QVERIFY(VerQueryValueW(versionData.data(),
                          L"\\StringFileInfo\\040904b0\\ProductName",
                          reinterpret_cast<void **>(&productName),
                          &productNameLength));
    QVERIFY(productName);
    QCOMPARE(QString::fromWCharArray(productName), QStringLiteral("Congregation"));
}

void tst_WindowsBinaryContract::launchWindowSizeStaysConstant()
{
    QProcess process;
    process.setProgram(executablePath());
    process.start();
    QVERIFY2(process.waitForStarted(5000), qPrintable(process.errorString()));

    const auto processCleanup = qScopeGuard([&process]() {
        if (process.state() == QProcess::NotRunning) {
            return;
        }
        process.terminate();
        if (!process.waitForFinished(1000)) {
            process.kill();
            process.waitForFinished(1000);
        }
    });

    HWND window = nullptr;
    QElapsedTimer windowTimeout;
    windowTimeout.start();
    while (!window && process.state() != QProcess::NotRunning
           && windowTimeout.elapsed() < 10000) {
        window = visibleTopLevelWindowForProcess(process.processId());
        QTest::qWait(10);
    }
    QVERIFY2(window, "Congregation did not create a visible top-level window");

    QSet<QSize> observedOuterSizes;
    QSet<QSize> observedClientSizes;
    QElapsedTimer stabilityWindow;
    stabilityWindow.start();
    while (process.state() != QProcess::NotRunning && stabilityWindow.elapsed() < 3000) {
        RECT outerRect{};
        RECT clientRect{};
        QVERIFY(GetWindowRect(window, &outerRect));
        QVERIFY(GetClientRect(window, &clientRect));
        observedOuterSizes.insert(QSize(outerRect.right - outerRect.left,
                                        outerRect.bottom - outerRect.top));
        observedClientSizes.insert(QSize(clientRect.right - clientRect.left,
                                         clientRect.bottom - clientRect.top));
        QTest::qWait(15);
    }

    QCOMPARE(observedOuterSizes.size(), 1);
    QCOMPARE(observedClientSizes.size(), 1);

    QVERIFY(PostMessageW(window, WM_CLOSE, 0, 0));
    QVERIFY(process.waitForFinished(3000));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
}

QTEST_APPLESS_MAIN(tst_WindowsBinaryContract)

#include "tst_windowsbinarycontract.moc"
