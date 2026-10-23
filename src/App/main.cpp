#include <iiFileProviderCoreIO.h>
#include <iiFileProviderNative.h>
#include "host/nativefiles.h"
#include <cstdlib>
#include <charconv>
#include <set>
#include <QGuiApplication>
#include <QMetaObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStringList>
#include <QUrl>
#include <QVariant>
#include <QWindow>
#include <QQuickStyle>
#include <QScreen>
#include <QtQml>
#include <qqml.h>

#include <iiSharedCanvas.h>
#include <Engine/ApplicationEngine.h>
#include "host/qtenginehost.h"
#include <chrono>
#include <mutex>

#include "models/canvas/canvasdocumentviewmodel.h"
#include "models/collaboration/memberprofilelistbuilder.h"
#include "models/input/temporarycamerainput.h"
#include "models/license/accountmanager.h"
#include "models/license/licensemanager.h"
#include "models/network/localcanvassession.h"
#include "models/network/nearbycongregationdiscovery.h"
#include "models/painting/drawingsurfaceitem.h"
#include "models/preferences/applicationpreferences.h"
#include "models/profile/profileimageprocessor.h"
#include "models/brush/paletteutils.h"
#include "models/update/congregationupdatemanager.h"

void qml_register_types_LVRS();

namespace {

constexpr int initialLaunchWindowWidth = 1280;

std::mutex& startupLogMutex(){static std::mutex mutex;return mutex;}
ii::io::Result<ii::io::Channel>& startupLogFile(){
    static auto file=[](){
        const auto* configured=std::getenv("CONGREGATION_STARTUP_LOG");
        auto path=configured?std::filesystem::path(configured):std::filesystem::temp_directory_path()/"Congregation-startup.log";
        return iiFileProvider::core::open_file(path,{ii::io::Access::write,ii::io::Creation::open_or_create});
    }();
    return file;
}

bool startupTraceEnabled()
{
    static const bool enabled = [] {
        const auto* raw = std::getenv("CONGREGATION_STARTUP_TRACE");
        if (!raw) return false;
        const auto input = congregation::trim(raw);
        int value = 0;
        const auto parsed = std::from_chars(input.data(), input.data() + input.size(), value);
        return parsed.ec == std::errc{} && parsed.ptr == input.data() + input.size() && value > 0;
    }();
    return enabled;
}

void appendStartupLog(const QString &level, const QString &message, bool flushImmediately = false)
{
    std::lock_guard locker(startupLogMutex());
    auto& file=startupLogFile();if(!file)return;
    const auto entry=iiFileProvider::native::utcNow()+" "+level.toStdString()+": "+message.toStdString()+"\n";
    if(!file->seek(0,ii::io::SeekOrigin::end))return;
    auto bytes=std::span(reinterpret_cast<const std::byte*>(entry.data()),entry.size());
    while(!bytes.empty()){auto written=file->write(bytes);if(!written||!*written)return;bytes=bytes.subspan(*written);}
    if(flushImmediately)(void)file->flush();
}

void traceStartup(const QString &message, bool flushImmediately = false)
{
    if (startupTraceEnabled()) {
        appendStartupLog(QStringLiteral("startup"), message, flushImmediately);
    }
}

QStringList collectCandidateImportPaths()
{
    QStringList candidateImportPaths;

    const QString bundledQmlPath = QCoreApplication::applicationDirPath() + QStringLiteral("/qml");
    const bool hasBundledLvrs = congregation::presentation::exists(bundledQmlPath + QStringLiteral("/LVRS"));

    const QString craftRoot = QString::fromLocal8Bit(std::getenv("CRAFTROOT"));
    if (!craftRoot.isEmpty()) {
        candidateImportPaths << QStringList{
            craftRoot + QStringLiteral("/qml"),
            craftRoot + QStringLiteral("/lib/qml")
        };
    }

    const QString lvrsHostPrefix = QString::fromLocal8Bit(std::getenv("LVRS_HOST_PREFIX"));
    if (!hasBundledLvrs && !lvrsHostPrefix.isEmpty()) {
        candidateImportPaths << (lvrsHostPrefix + QStringLiteral("/lib/qt6/qml"));
    }

    // addImportPath() prepends, so add the packaged path last to keep it authoritative.
    candidateImportPaths << bundledQmlPath;

    return candidateImportPaths;
}

void configureEngineImports(QQmlApplicationEngine &engine)
{
    const QStringList candidateImportPaths = collectCandidateImportPaths();

    std::set<QString> dedupedImportPaths;
    for (const QString &importPath : candidateImportPaths) {
        if (importPath.isEmpty() || dedupedImportPaths.contains(importPath)) {
            continue;
        }

        dedupedImportPaths.insert(importPath);
        if (congregation::presentation::exists(importPath)) {
            engine.addImportPath(importPath);
        }
    }
}

void registerViewModels(QQmlApplicationEngine &engine, congregation::CanvasDocument &document)
{
    QObject *registry = engine.singletonInstance<QObject *>(QStringLiteral("LVRS"),
                                                            QStringLiteral("ViewModels"));
    if (!registry) {
        return;
    }

    auto *documentViewModel = new CanvasDocumentViewModel(document, registry);
    QMetaObject::invokeMethod(registry,
                              "set",
                              Q_ARG(QString, QStringLiteral("CanvasDocument")),
                              Q_ARG(QObject *, documentViewModel));
}

QSize initialLaunchWindowSize(const QSize &frameworkSize)
{
    if (!frameworkSize.isValid()) {
        return frameworkSize;
    }

    const int initialHeight =
        qRound(qreal(initialLaunchWindowWidth) * frameworkSize.height() / frameworkSize.width());
    return QSize(initialLaunchWindowWidth, initialHeight);
}

QSize finalLaunchWindowSize(const QWindow &window)
{
    const QSize requestedSize = initialLaunchWindowSize(window.size());
    if (!requestedSize.isValid()) {
        return requestedSize;
    }

    QScreen *screen = window.screen();
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return requestedSize;
    }

    const QSize availableSize = screen->availableGeometry().size();
    if (!availableSize.isValid()) {
        return requestedSize;
    }
    if (requestedSize.width() <= availableSize.width()
        && requestedSize.height() <= availableSize.height()) {
        return requestedSize;
    }
    return requestedSize.scaled(availableSize, Qt::KeepAspectRatio);
}

void showLaunchWindow(QQmlApplicationEngine &engine)
{
    const QObjectList rootObjects = engine.rootObjects();
    if (rootObjects.isEmpty()) {
        return;
    }

    auto *window = qobject_cast<QWindow *>(rootObjects.first());
    if (!window) {
        return;
    }

    const QSize finalSize = finalLaunchWindowSize(*window);
    if (window->size() != finalSize) {
        window->resize(finalSize);
    }
    window->showNormal();
}

} // namespace

int main(int argc, char *argv[])
{
    const auto launchStart = std::chrono::steady_clock::now();
    const auto launchElapsed = [&] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - launchStart).count();
    };
    startupLogFile();

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Congregation"));
    QGuiApplication::setApplicationVersion(QStringLiteral(CONGREGATION_VERSION));
    QGuiApplication::setOrganizationName(QStringLiteral("iisacc"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("iisacc.com"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    traceStartup(QStringLiteral("Congregation startup initialized in %1 ms").arg(launchElapsed()));

    const int smokeArgument = app.arguments().indexOf(QStringLiteral("--engine-smoke-test"));
    const QString smokeDirectory = smokeArgument >= 0 ? app.arguments().value(smokeArgument + 1) : QString{};
    if (smokeArgument >= 0) {
        if(smokeDirectory.isEmpty()||!congregation::presentation::exists(smokeDirectory)){qCritical("--engine-smoke-test requires an existing isolated test directory");return 2;}
        const auto path=congregation::presentation::nativePath(smokeDirectory)/"preferences.ini";
        iiFileProvider::native::File::write(path,"[%General]\nstartWithRecentCanvas=false\ndiscoverNearbyCongregationUsers=false\n");
    }

    qml_register_types_LVRS();
    iiSharedCanvas::registerIiSharedCanvasQmlTypes();
    traceStartup(QStringLiteral("LVRS QML types registered in %1 ms").arg(launchElapsed()));

    congregation::ApplicationEngine applicationEngine;
    QtEngineHost coreHost(applicationEngine.events());
    QObject::connect(&coreHost, &QtEngineHost::dispatchFailed, &app, [&](const QString &message) {
        appendStartupLog(QStringLiteral("critical"), message, true);
        QCoreApplication::exit(1);
    });
    QQmlApplicationEngine engine;
    configureEngineImports(engine);
    qmlRegisterType<DrawingSurfaceItem>("Congregation", 1, 0, "DrawingSurfaceItem");

    auto *temporaryCameraInput = new TemporaryCameraInput(&app);
    app.installEventFilter(temporaryCameraInput);
    engine.rootContext()->setContextProperty("CongregationTemporaryCameraInput", temporaryCameraInput);
    auto *paletteUtils = new PaletteUtils(&engine);
    engine.rootContext()->setContextProperty("PaletteUtils", paletteUtils);
    auto *profileImageProcessor = new ProfileImageProcessor(&engine);
    engine.rootContext()->setContextProperty("CongregationProfileImageProcessor",
                                             profileImageProcessor);
    auto *memberProfileListBuilder = new MemberProfileListBuilder(&engine);
    engine.rootContext()->setContextProperty("CongregationMemberProfileListBuilder",
                                             memberProfileListBuilder);
    auto *applicationPreferences = smokeArgument >= 0
        ? new ApplicationPreferences(smokeDirectory + QStringLiteral("/preferences.ini"), smokeDirectory + QStringLiteral("/canvas"), &engine)
        : new ApplicationPreferences(&engine);
    engine.rootContext()->setContextProperty("CongregationApplicationPreferences",
                                             applicationPreferences);
    auto *licenseManager =
        new LicenseManager(LicenseManager::EnforcementMode::Disabled, &engine);
    engine.rootContext()->setContextProperty("CongregationLicenseManager", licenseManager);
    auto *accountManager = new AccountManager(licenseManager, &engine);
    engine.rootContext()->setContextProperty("CongregationAccountManager", accountManager);
    auto *updateManager = new CongregationUpdateManager(licenseManager, &engine);
    engine.rootContext()->setContextProperty("CongregationUpdateManager", updateManager);
    auto *nearbyDiscovery = new NearbyCongregationDiscovery(&engine);
    engine.rootContext()->setContextProperty("CongregationNearbyDiscovery", nearbyDiscovery);
    auto* localCanvasSession = new LocalCanvasSession(nearbyDiscovery, &engine);
    engine.rootContext()->setContextProperty("CongregationLocalCanvasSession", localCanvasSession);
    QObject::connect(applicationPreferences,
                     &ApplicationPreferences::discoverNearbyCongregationUsersChanged,
                     nearbyDiscovery,
                     [applicationPreferences, nearbyDiscovery]() {
                         if (applicationPreferences->discoverNearbyCongregationUsers()) {
                             nearbyDiscovery->start();
                             return;
                         }
                         nearbyDiscovery->stop();
                     });
    registerViewModels(engine, applicationEngine.document());

    QObject::connect(&engine,
                     &QQmlApplicationEngine::objectCreationFailed,
                     &app,
                     []() {
                         appendStartupLog(QStringLiteral("critical"),
                                          QStringLiteral("Congregation.Main object creation failed"),
                                          true);
                         qCritical("Congregation.Main object creation failed");
                         QCoreApplication::exit(-1);
                     },
                     Qt::QueuedConnection);
    traceStartup(QStringLiteral("Loading Congregation.Main at %1 ms").arg(launchElapsed()));
    engine.loadFromModule(QStringLiteral("Congregation"), QStringLiteral("Main"));
    traceStartup(QStringLiteral("Loaded Congregation.Main with %1 root object(s) in %2 ms")
                     .arg(engine.rootObjects().size())
                     .arg(launchElapsed()));
    showLaunchWindow(engine);
    const auto started = applicationEngine.start([applicationPreferences, nearbyDiscovery]() {
        if (applicationPreferences->discoverNearbyCongregationUsers()) {
            nearbyDiscovery->start();
        }
    });
    traceStartup(QStringLiteral("Launch window shown at its final geometry in %1 ms")
                     .arg(launchElapsed()),
                 true);

    if (!started) {
        appendStartupLog(QStringLiteral("critical"), QStringLiteral("Core startup admission failed"), true);
        return 1;
    }
    if (smokeArgument >= 0) {
        const auto smoke = applicationEngine.tasks().at(
            applicationEngine.events(), ii::runtime::Clock::now() + std::chrono::milliseconds(500), [&] {
                const auto roots = engine.rootObjects();
                const auto *window = roots.isEmpty() ? nullptr : qobject_cast<QWindow *>(roots.first());
                const auto state = applicationEngine.document().snapshot();
                const bool valid = window && window->isVisible() && state.canvasWidth > 0
                                   && state.canvasHeight > 0 && applicationEngine.events().stats().executed > 0;
                qInfo("iiSDKCore engine smoke: %s", valid ? "PASS" : "FAIL");
                QCoreApplication::exit(valid ? 0 : 2);
            });
        if (!smoke) {
            qCritical("Core smoke deadline admission failed");
            return 2;
        }
    }
    const int result = app.exec();
    applicationEngine.stop();
    return result;
}
