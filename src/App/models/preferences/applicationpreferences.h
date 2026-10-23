#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

namespace congregation
{
class Preferences;
}

class ApplicationPreferences final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool startWithRecentCanvas READ startWithRecentCanvas WRITE setStartWithRecentCanvas
                   NOTIFY startWithRecentCanvasChanged)
    Q_PROPERTY(bool discoverNearbyCongregationUsers READ discoverNearbyCongregationUsers WRITE
                   setDiscoverNearbyCongregationUsers NOTIFY discoverNearbyCongregationUsersChanged)
    Q_PROPERTY(QUrl recentCanvasUrl READ recentCanvasUrl NOTIFY recentCanvasUrlChanged)
    Q_PROPERTY(QUrl recentCanvasStorageUrl READ recentCanvasStorageUrl CONSTANT)

  public:
    explicit ApplicationPreferences(QObject* parent = nullptr);
    explicit ApplicationPreferences(const QString& settingsPath, QObject* parent = nullptr);
    ApplicationPreferences(const QString& settingsPath, const QString& storageDirectory,
                           QObject* parent = nullptr);
    ~ApplicationPreferences() override;

    [[nodiscard]] bool startWithRecentCanvas() const noexcept;
    [[nodiscard]] bool discoverNearbyCongregationUsers() const noexcept;
    [[nodiscard]] QUrl recentCanvasUrl() const;
    [[nodiscard]] QUrl recentCanvasStorageUrl() const;

    Q_INVOKABLE void setStartWithRecentCanvas(bool startWithRecentCanvas);
    Q_INVOKABLE void setDiscoverNearbyCongregationUsers(bool discoverNearbyCongregationUsers);
    Q_INVOKABLE bool recordRecentCanvas(const QUrl& fileUrl);
    Q_INVOKABLE bool clearRecentCanvas();

  signals:
    void startWithRecentCanvasChanged();
    void discoverNearbyCongregationUsersChanged();
    void recentCanvasUrlChanged();

  private:
    std::unique_ptr<congregation::Preferences> m_backend;
    QUrl m_recentCanvasUrl;
};
