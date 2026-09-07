#include <QDir>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QMetaObject>
#include <QMutex>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QWindow>
#include <QQuickStyle>
#include <QScreen>
#include <QtQml>
#include <qqml.h>

#include <iiSharedCanvas.h>

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

QString startupLogPath()
{
    static const QString path = []() {
        QString logDirectory = QDir::tempPath();
        if (logDirectory.isEmpty()) {
            logDirectory = QString::fromLocal8Bit(qgetenv("TEMP"));
        }
        if (logDirectory.isEmpty()) {
            return QString{};
        }

        return QDir(logDirectory).filePath(QStringLiteral("Congregation-startup.log"));
    }();
    return path;
}

QMutex &startupLogMutex()
{
    static QMutex mutex;
    return mutex;
}

QFile &startupLogFile()
{
    static QFile logFile(startupLogPath());
    return logFile;
}

bool startupTraceEnabled()
{
    static const bool enabled = qEnvironmentVariableIntValue("CONGREGATION_STARTUP_TRACE") > 0;
    return enabled;
}

void appendStartupLog(const QString &level, const QString &message, bool flushImmediately = false)
{
    QMutexLocker locker(&startupLogMutex());
    QFile &logFile = startupLogFile();
    if (logFile.fileName().isEmpty()) {
        return;
    }

    if (!logFile.isOpen()
        && !logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }

    const QByteArray entry = QStringLiteral("%1 %2: %3\n")
                                 .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),
                                      level,
                                      message)
                                 .toUtf8();
    logFile.write(entry);
    if (flushImmediately) {
        logFile.flush();
    }
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
    const bool hasBundledLvrs = QDir(bundledQmlPath + QStringLiteral("/LVRS")).exists();

    const QString craftRoot = QString::fromLocal8Bit(qgetenv("CRAFTROOT"));
    if (!craftRoot.isEmpty()) {
        candidateImportPaths << QStringList{
            craftRoot + QStringLiteral("/qml"),
            craftRoot + QStringLiteral("/lib/qml")
        };
    }

    const QString lvrsHostPrefix = QString::fromLocal8Bit(qgetenv("LVRS_HOST_PREFIX"));
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

    QSet<QString> dedupedImportPaths;
    for (const QString &importPath : candidateImportPaths) {
        if (importPath.isEmpty() || dedupedImportPaths.contains(importPath)) {
            continue;
        }

        dedupedImportPaths.insert(importPath);
        if (QDir(importPath).exists()) {
            engine.addImportPath(importPath);
        }
    }
}

void registerViewModels(QQmlApplicationEngine &engine, PaletteUtils *paletteUtils)
{
    QObject *registry = engine.singletonInstance<QObject *>(QStringLiteral("LVRS"),
                                                            QStringLiteral("ViewModels"));
    if (!registry) {
        return;
    }

    auto *documentViewModel = new CanvasDocumentViewModel(paletteUtils, registry);
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
    QElapsedTimer launchTimer;
    launchTimer.start();
    startupLogPath();

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Congregation"));
    QGuiApplication::setApplicationVersion(QStringLiteral(CONGREGATION_VERSION));
    QGuiApplication::setOrganizationName(QStringLiteral("iisacc"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("iisacc.com"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    traceStartup(QStringLiteral("Congregation startup initialized in %1 ms").arg(launchTimer.elapsed()));

    qml_register_types_LVRS();
    iiSharedCanvas::registerIiSharedCanvasQmlTypes();
    traceStartup(QStringLiteral("LVRS QML types registered in %1 ms").arg(launchTimer.elapsed()));

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
    auto *applicationPreferences = new ApplicationPreferences(&engine);
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
    registerViewModels(engine, paletteUtils);

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
    traceStartup(QStringLiteral("Loading Congregation.Main at %1 ms").arg(launchTimer.elapsed()));
    engine.loadFromModule(QStringLiteral("Congregation"), QStringLiteral("Main"));
    traceStartup(QStringLiteral("Loaded Congregation.Main with %1 root object(s) in %2 ms")
                     .arg(engine.rootObjects().size())
                     .arg(launchTimer.elapsed()));
    showLaunchWindow(engine);
    QTimer::singleShot(0, nearbyDiscovery, [applicationPreferences, nearbyDiscovery]() {
        if (applicationPreferences->discoverNearbyCongregationUsers()) {
            nearbyDiscovery->start();
        }
    });
    traceStartup(QStringLiteral("Launch window shown at its final geometry in %1 ms")
                     .arg(launchTimer.elapsed()),
                 true);

    return app.exec();
}
