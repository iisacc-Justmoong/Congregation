#pragma once

#include <memory>
#include <QObject>

#include <QUrl>

#include <functional>

class QtEngineHost;
namespace ii::runtime
{
class EventLoop;
}
namespace congregation
{
class LicenseService;
}
class LicenseCredentialStore;

struct StoredLicenseCredentials final
{
    StoredLicenseCredentials() = default;
    StoredLicenseCredentials(QString credentialEmail, QString credentialLicenseKey);
    StoredLicenseCredentials(const StoredLicenseCredentials&) = delete;
    StoredLicenseCredentials& operator=(const StoredLicenseCredentials&) = delete;
    StoredLicenseCredentials(StoredLicenseCredentials&& other) noexcept;
    StoredLicenseCredentials& operator=(StoredLicenseCredentials&& other) noexcept;
    ~StoredLicenseCredentials();

    void clear();

    QString email;
    QString licenseKey;
};

class LicenseManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString productId READ productId CONSTANT)
    Q_PROPERTY(bool enforcementEnabled READ enforcementEnabled CONSTANT)
    Q_PROPERTY(bool licensed READ licensed NOTIFY licensedChanged)
    Q_PROPERTY(bool verifying READ verifying NOTIFY verifyingChanged)
    Q_PROPERTY(bool persistenceSupported READ persistenceSupported CONSTANT)
    Q_PROPERTY(bool hasStoredLicense READ hasStoredLicense NOTIFY hasStoredLicenseChanged)
    Q_PROPERTY(QString resultCode READ resultCode NOTIFY resultCodeChanged)

  public:
    enum class EnforcementMode
    {
        Enabled,
        Disabled
    };
    Q_ENUM(EnforcementMode)

    enum class StoredCredentialStatus
    {
        Available,
        NotFound,
        Unavailable,
        Invalid
    };

    using StoredCredentialCompletion =
        std::function<void(StoredCredentialStatus, StoredLicenseCredentials)>;

    explicit LicenseManager(QObject* parent = nullptr);
    explicit LicenseManager(EnforcementMode enforcementMode, QObject* parent = nullptr);
    LicenseManager(const QUrl& validationEndpoint, int requestTimeoutMilliseconds,
                   QObject* parent = nullptr);
    LicenseManager(const QUrl& validationEndpoint, int requestTimeoutMilliseconds,
                   LicenseCredentialStore* credentialStore, QObject* parent = nullptr);
    LicenseManager(const QUrl& validationEndpoint, int requestTimeoutMilliseconds,
                   LicenseCredentialStore* credentialStore, EnforcementMode enforcementMode,
                   QObject* parent = nullptr);

    ~LicenseManager() override;
    [[nodiscard]] QString productId() const;
    [[nodiscard]] bool enforcementEnabled() const;
    [[nodiscard]] bool licensed() const;
    [[nodiscard]] bool verifying() const;
    [[nodiscard]] bool persistenceSupported() const;
    [[nodiscard]] bool hasStoredLicense() const;
    [[nodiscard]] QString resultCode() const;

    Q_INVOKABLE void validateLicense(const QString& email, const QString& licenseKey);
    Q_INVOKABLE void retryStoredLicense();
    Q_INVOKABLE void forgetLicense();

    // C++-only seam for consumers that must authenticate an explicit user action.
    // It intentionally does not expose credentials through the Qt meta-object/QML surface.
    void requestStoredCredentials(StoredCredentialCompletion completion);

  signals:
    void licensedChanged();
    void verifyingChanged();
    void hasStoredLicenseChanged();
    void resultCodeChanged();
    void validationFinished(bool valid);

  private:
    void initialize(const QUrl&, int, LicenseCredentialStore*, EnforcementMode);
    std::unique_ptr<ii::runtime::EventLoop> m_events;
    std::unique_ptr<QtEngineHost> m_host;
    std::unique_ptr<congregation::LicenseService> m_backend;
};
