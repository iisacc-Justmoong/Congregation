#include "applicationpreferences.h"

#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class tst_ApplicationPreferences : public QObject
{
    Q_OBJECT

private slots:
    void defaultsToNewCanvasAndNearbyDiscovery();
    void generalChoicesPersistAcrossInstances();
    void internalContainerBecomesTheOnlyRecentCanvas();
    void missingInternalContainerIsDiscardedOnReload();
    void externalCanvasPathCannotBecomeRecentCanvas();
    void clearingRecentCanvasDeletesTheInternalContainer();
};

void tst_ApplicationPreferences::defaultsToNewCanvasAndNearbyDiscovery()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("preferences.ini")),
                       QSettings::IniFormat);
    const QString storageDirectory = directory.filePath(QStringLiteral("AppData/canvas"));

    ApplicationPreferences preferences(&settings, storageDirectory);

    QVERIFY(!preferences.startWithRecentCanvas());
    QVERIFY(preferences.discoverNearbyCongregationUsers());
    QVERIFY(preferences.recentCanvasUrl().isEmpty());
    QCOMPARE(
        preferences.recentCanvasStorageUrl(),
        QUrl::fromLocalFile(QDir(storageDirectory).filePath(QStringLiteral("recent-canvas.vrc"))));
}

void tst_ApplicationPreferences::generalChoicesPersistAcrossInstances()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("preferences.ini"));

    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        ApplicationPreferences preferences(&settings);
        QSignalSpy startupSpy(&preferences,
                              &ApplicationPreferences::startWithRecentCanvasChanged);
        QSignalSpy discoverySpy(
            &preferences,
            &ApplicationPreferences::discoverNearbyCongregationUsersChanged);

        preferences.setStartWithRecentCanvas(true);
        preferences.setDiscoverNearbyCongregationUsers(false);

        QCOMPARE(startupSpy.size(), 1);
        QCOMPARE(discoverySpy.size(), 1);
        QCOMPARE(settings.status(), QSettings::NoError);

        QFile persistedSettings(settingsPath);
        QVERIFY(persistedSettings.open(QIODevice::ReadOnly | QIODevice::Text));
        const QByteArray persistedData = persistedSettings.readAll();
        QVERIFY(persistedData.contains("startWithRecentCanvas=true"));
        QVERIFY(persistedData.contains("discoverNearbyCongregationUsers=false"));
    }

    QSettings restoredSettings(settingsPath, QSettings::IniFormat);
    ApplicationPreferences restored(&restoredSettings);
    QVERIFY(restored.startWithRecentCanvas());
    QVERIFY(!restored.discoverNearbyCongregationUsers());
}

void tst_ApplicationPreferences::internalContainerBecomesTheOnlyRecentCanvas()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString storageDirectory = directory.filePath(QStringLiteral("AppData/canvas"));
    const QString canvasPath = QDir(storageDirectory).filePath(QStringLiteral("recent-canvas.vrc"));
    QVERIFY(QDir().mkpath(storageDirectory));
    QFile canvas(canvasPath);
    QVERIFY(canvas.open(QIODevice::WriteOnly));
    QVERIFY(canvas.write("canvas") > 0);
    canvas.close();

    const QString settingsPath = directory.filePath(QStringLiteral("preferences.ini"));
    QSettings settings(settingsPath, QSettings::IniFormat);
    ApplicationPreferences preferences(&settings, storageDirectory);
    QSignalSpy recentCanvasSpy(&preferences,
                               &ApplicationPreferences::recentCanvasUrlChanged);

    QVERIFY(preferences.recordRecentCanvas(QUrl::fromLocalFile(canvasPath)));
    QCOMPARE(recentCanvasSpy.size(), 0);
    QCOMPARE(preferences.recentCanvasUrl(),
             QUrl::fromLocalFile(QFileInfo(canvasPath).canonicalFilePath()));
    QVERIFY(!settings.contains(QStringLiteral("General/recentCanvasUrl")));

    settings.sync();
    QSettings restoredSettings(settingsPath, QSettings::IniFormat);
    ApplicationPreferences restored(&restoredSettings, storageDirectory);
    QCOMPARE(restored.recentCanvasUrl(), preferences.recentCanvasUrl());
}

void tst_ApplicationPreferences::missingInternalContainerIsDiscardedOnReload()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("preferences.ini"));
    const QString storageDirectory = directory.filePath(QStringLiteral("AppData/canvas"));
    const QString canvasPath = QDir(storageDirectory).filePath(QStringLiteral("recent-canvas.vrc"));
    QVERIFY(QDir().mkpath(storageDirectory));
    QFile canvas(canvasPath);
    QVERIFY(canvas.open(QIODevice::WriteOnly));
    QVERIFY(canvas.write("canvas") > 0);
    canvas.close();

    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        ApplicationPreferences preferences(&settings, storageDirectory);
        QVERIFY(preferences.recordRecentCanvas(QUrl::fromLocalFile(canvasPath)));
        settings.sync();
    }
    QVERIFY(QFile::remove(canvasPath));

    QSettings restoredSettings(settingsPath, QSettings::IniFormat);
    ApplicationPreferences restored(&restoredSettings, storageDirectory);
    QVERIFY(restored.recentCanvasUrl().isEmpty());
}

void tst_ApplicationPreferences::externalCanvasPathCannotBecomeRecentCanvas()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("preferences.ini")),
                       QSettings::IniFormat);
    const QString storageDirectory = directory.filePath(QStringLiteral("AppData/canvas"));
    ApplicationPreferences preferences(&settings, storageDirectory);

    const QString externalCanvasPath = directory.filePath(QStringLiteral("external.iisc"));
    QFile externalCanvas(externalCanvasPath);
    QVERIFY(externalCanvas.open(QIODevice::WriteOnly));
    QVERIFY(externalCanvas.write("canvas") > 0);
    externalCanvas.close();

    QVERIFY(!preferences.recordRecentCanvas(QUrl::fromLocalFile(externalCanvasPath)));

    QVERIFY(!preferences.recordRecentCanvas(
        QUrl(QStringLiteral("https://iisacc.com/not-a-local-canvas.iisc"))));
    QVERIFY(preferences.recentCanvasUrl().isEmpty());
    QVERIFY(!settings.contains(QStringLiteral("General/recentCanvasUrl")));
}

void tst_ApplicationPreferences::clearingRecentCanvasDeletesTheInternalContainer()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("preferences.ini")), QSettings::IniFormat);
    const QString storageDirectory = directory.filePath(QStringLiteral("AppData/canvas"));
    ApplicationPreferences preferences(&settings, storageDirectory);
    const QString canvasPath = preferences.recentCanvasStorageUrl().toLocalFile();

    QVERIFY(QDir().mkpath(storageDirectory));
    QFile canvas(canvasPath);
    QVERIFY(canvas.open(QIODevice::WriteOnly));
    QVERIFY(canvas.write("canvas") > 0);
    canvas.close();
    QVERIFY(preferences.recordRecentCanvas(QUrl::fromLocalFile(canvasPath)));

    QSignalSpy recentCanvasSpy(&preferences, &ApplicationPreferences::recentCanvasUrlChanged);
    QVERIFY(preferences.clearRecentCanvas());
    QCOMPARE(recentCanvasSpy.size(), 1);
    QVERIFY(preferences.recentCanvasUrl().isEmpty());
    QVERIFY(!QFileInfo::exists(canvasPath));
}

QTEST_APPLESS_MAIN(tst_ApplicationPreferences)

#include "tst_applicationpreferences.moc"
