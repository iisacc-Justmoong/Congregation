#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

class LicenseManager;
class CongregationUpdateCredentialProvider;

namespace iisacc::updates {
class UpdateManager;
}

class CongregationUpdateManager final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString title READ title NOTIFY presentationChanged)
    Q_PROPERTY(QString message READ message NOTIFY presentationChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY presentationChanged)
    Q_PROPERTY(bool selfUpdateSupported READ selfUpdateSupported CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool canUpdate READ canUpdate NOTIFY stateChanged)
    Q_PROPERTY(bool canCancel READ canCancel NOTIFY stateChanged)

public:
    enum class State
    {
        Idle,
        Checking,
        UpToDate,
        UpdateAvailable,
        Authorizing,
        Downloading,
        Verifying,
        LaunchingInstaller,
        InstallerLaunched,
        Failed
    };
    Q_ENUM(State)

    explicit CongregationUpdateManager(LicenseManager *licenseManager,
                                  QObject *parent = nullptr);
    CongregationUpdateManager(LicenseManager *licenseManager,
                         QString currentVersion,
                         QUrl manifestUrl,
                         QUrl grantUrl,
                         bool selfUpdateSupported,
                         QObject *parent = nullptr);
    ~CongregationUpdateManager() override;

    [[nodiscard]] State state() const noexcept;
    [[nodiscard]] double progress() const noexcept;
    [[nodiscard]] QString title() const;
    [[nodiscard]] QString message() const;
    [[nodiscard]] QString latestVersion() const;
    [[nodiscard]] bool selfUpdateSupported() const noexcept;
    [[nodiscard]] bool busy() const noexcept;
    [[nodiscard]] bool canUpdate() const noexcept;
    [[nodiscard]] bool canCancel() const noexcept;

    Q_INVOKABLE bool checkForUpdates();
    Q_INVOKABLE bool updateNow();
    Q_INVOKABLE bool cancelUpdate();

    [[nodiscard]] static bool selfUpdateSupportedForMacApplicationDirectory(
        const QString &applicationDirectory);

signals:
    void stateChanged();
    void progressChanged();
    void presentationChanged();

private:
    void connectBackend();
    void applyBackendState();
    void setState(State state);
    void setProgress(double progress);
    void setPresentation(QString title, QString message);
    void setLatestVersion(QString version);
    bool rejectStoreManagedSelfUpdate();

    bool m_selfUpdateSupported = true;
    std::unique_ptr<CongregationUpdateCredentialProvider> m_credentialProvider;
    std::unique_ptr<iisacc::updates::UpdateManager> m_backend;
    State m_state = State::Idle;
    double m_progress = 0.0;
    QString m_title;
    QString m_message;
    QString m_latestVersion;
};
