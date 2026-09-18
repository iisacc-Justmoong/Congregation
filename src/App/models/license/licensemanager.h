#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>

#include <functional>

class QNetworkReply;
class LicenseCredentialStore;

struct StoredLicenseCredentials final
{
    StoredLicenseCredentials() = default;
    StoredLicenseCredentials(QString credentialEmail, QString credentialLicenseKey);
    StoredLicenseCredentials(const StoredLicenseCredentials &) = delete;
    StoredLicenseCredentials &operator=(const StoredLicenseCredentials &) = delete;
    StoredLicenseCredentials(StoredLicenseCredentials &&other) noexcept;
    StoredLicenseCredentials &operator=(StoredLicenseCredentials &&other) noexcept;
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

    explicit LicenseManager(QObject *parent = nullptr);
    explicit LicenseManager(EnforcementMode enforcementMode, QObject *parent = nullptr);
    LicenseManager(const QUrl &validationEndpoint,
                   int requestTimeoutMilliseconds,
                   QObject *parent = nullptr);
    LicenseManager(const QUrl &validationEndpoint,
                   int requestTimeoutMilliseconds,
                   LicenseCredentialStore *credentialStore,
                   QObject *parent = nullptr);
    LicenseManager(const QUrl &validationEndpoint,
                   int requestTimeoutMilliseconds,
                   LicenseCredentialStore *credentialStore,
                   EnforcementMode enforcementMode,
                   QObject *parent = nullptr);

    [[nodiscard]] QString productId() const;
    [[nodiscard]] bool enforcementEnabled() const;
    [[nodiscard]] bool licensed() const;
    [[nodiscard]] bool verifying() const;
    [[nodiscard]] bool persistenceSupported() const;
    [[nodiscard]] bool hasStoredLicense() const;
    [[nodiscard]] QString resultCode() const;

    Q_INVOKABLE void validateLicense(const QString &email, const QString &licenseKey);
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
    void restoreStoredLicense();
    void startValidation(const QString &normalizedEmail,
                         const QString &normalizedLicenseKey,
                         bool fromStoredCredentials);
    void finishValidation();
    void storeValidatedCredentials();
    void clearStoredCredentials();
    void setHasStoredLicense(bool hasStoredLicense);
    void setLicensed(bool licensed);
    void setVerifying(bool verifying);
    void setResultCode(const QString &resultCode);

    QUrl m_validationEndpoint;
    int m_requestTimeoutMilliseconds = 10000;
    QNetworkAccessManager m_networkAccessManager;
    LicenseCredentialStore *m_credentialStore = nullptr;
    QPointer<QNetworkReply> m_activeReply;
    QTimer m_requestTimeout;
    bool m_responseTooLarge = false;
    bool m_enforcementEnabled = true;
    bool m_licensed = false;
    bool m_verifying = false;
    bool m_hasStoredLicense = false;
    bool m_requestUsesStoredCredentials = false;
    QString m_activeEmail;
    QString m_activeLicenseKey;
    QString m_storedEmail;
    QString m_storedLicenseKey;
    QString m_resultCode;
};
