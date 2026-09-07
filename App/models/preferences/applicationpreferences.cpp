#include "applicationpreferences.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace
{
constexpr auto startWithRecentCanvasKey = "General/startWithRecentCanvas";
constexpr auto discoverNearbyCongregationUsersKey = "General/discoverNearbyCongregationUsers";
constexpr auto legacyRecentCanvasUrlKey = "General/recentCanvasUrl";
constexpr auto recentCanvasFileName = "recent-canvas.vrc";

QString defaultStorageDirectory()
{
    QString applicationData =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (applicationData.isEmpty())
    {
        applicationData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    return applicationData.isEmpty() ? QString{}
                                     : QDir(applicationData).filePath(QStringLiteral("canvas"));
}
} // namespace

ApplicationPreferences::ApplicationPreferences(QObject *parent)
    : QObject(parent)
    , m_ownedSettings(std::make_unique<QSettings>())
    , m_settings(m_ownedSettings.get())
    , m_storageDirectory(defaultStorageDirectory())
{
    load();
}

ApplicationPreferences::ApplicationPreferences(QSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_storageDirectory(defaultStorageDirectory())
{
    Q_ASSERT(m_settings);
    load();
}

ApplicationPreferences::ApplicationPreferences(QSettings *settings,
                                               const QString &storageDirectory,
                                               QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_storageDirectory(storageDirectory.isEmpty() ? QString{}
                                                    : QDir::cleanPath(storageDirectory))
{
    Q_ASSERT(m_settings);
    load();
}

ApplicationPreferences::~ApplicationPreferences() = default;

bool ApplicationPreferences::startWithRecentCanvas() const noexcept
{
    return m_startWithRecentCanvas;
}

bool ApplicationPreferences::discoverNearbyCongregationUsers() const noexcept
{
    return m_discoverNearbyCongregationUsers;
}

QUrl ApplicationPreferences::recentCanvasUrl() const
{
    return m_recentCanvasUrl;
}

QUrl ApplicationPreferences::recentCanvasStorageUrl() const
{
    const QString path = recentCanvasStoragePath();
    return path.isEmpty() ? QUrl{} : QUrl::fromLocalFile(path);
}

void ApplicationPreferences::setStartWithRecentCanvas(bool startWithRecentCanvas)
{
    if (m_startWithRecentCanvas == startWithRecentCanvas) {
        return;
    }

    m_startWithRecentCanvas = startWithRecentCanvas;
    m_settings->setValue(QLatin1String(startWithRecentCanvasKey), startWithRecentCanvas);
    m_settings->sync();
    emit startWithRecentCanvasChanged();
}

void ApplicationPreferences::setDiscoverNearbyCongregationUsers(bool discoverNearbyCongregationUsers)
{
    if (m_discoverNearbyCongregationUsers == discoverNearbyCongregationUsers) {
        return;
    }

    m_discoverNearbyCongregationUsers = discoverNearbyCongregationUsers;
    m_settings->setValue(QLatin1String(discoverNearbyCongregationUsersKey),
                         discoverNearbyCongregationUsers);
    m_settings->sync();
    emit discoverNearbyCongregationUsersChanged();
}

bool ApplicationPreferences::recordRecentCanvas(const QUrl &fileUrl)
{
    if (!fileUrl.isLocalFile()) {
        return false;
    }
    const QString storagePath = recentCanvasStoragePath();
    if (storagePath.isEmpty()) {
        return false;
    }
    const QString expectedPath = QFileInfo(storagePath).absoluteFilePath();
    const QString requestedPath = QFileInfo(fileUrl.toLocalFile()).absoluteFilePath();
    if (requestedPath != expectedPath) {
        return false;
    }

    const QUrl internalUrl = existingInternalCanvasUrl();
    if (internalUrl.isEmpty()) {
        return false;
    }
    if (m_recentCanvasUrl == internalUrl) {
        return true;
    }

    m_recentCanvasUrl = internalUrl;
    emit recentCanvasUrlChanged();
    return true;
}

bool ApplicationPreferences::clearRecentCanvas()
{
    const QString storagePath = recentCanvasStoragePath();
    if (!storagePath.isEmpty() && QFileInfo::exists(storagePath)
        && !QFile::remove(storagePath)) {
        return false;
    }

    const bool changed = !m_recentCanvasUrl.isEmpty();
    m_recentCanvasUrl = QUrl{};
    if (changed) {
        emit recentCanvasUrlChanged();
    }
    return true;
}

void ApplicationPreferences::load()
{
    m_startWithRecentCanvas =
        m_settings->value(QLatin1String(startWithRecentCanvasKey), false).toBool();
    m_discoverNearbyCongregationUsers =
        m_settings->value(QLatin1String(discoverNearbyCongregationUsersKey), true).toBool();

    if (m_settings->contains(QLatin1String(legacyRecentCanvasUrlKey))) {
        m_settings->remove(QLatin1String(legacyRecentCanvasUrlKey));
        m_settings->sync();
    }

    if (!m_storageDirectory.isEmpty()) {
        QDir().mkpath(m_storageDirectory);
        QFile::setPermissions(m_storageDirectory,
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                  | QFileDevice::ExeOwner);
    }
    m_recentCanvasUrl = existingInternalCanvasUrl();
}

QUrl ApplicationPreferences::existingInternalCanvasUrl() const
{
    const QString path = recentCanvasStoragePath();
    if (path.isEmpty()) {
        return {};
    }

    const QFileInfo fileInfo(path);
    if (!fileInfo.exists() || !fileInfo.isFile() || fileInfo.isSymLink()) {
        return {};
    }

    const QString canonicalPath = fileInfo.canonicalFilePath();
    return canonicalPath.isEmpty() ? QUrl{} : QUrl::fromLocalFile(canonicalPath);
}

QString ApplicationPreferences::recentCanvasStoragePath() const
{
    return m_storageDirectory.isEmpty()
               ? QString{}
               : QDir(m_storageDirectory).filePath(QLatin1String(recentCanvasFileName));
}
