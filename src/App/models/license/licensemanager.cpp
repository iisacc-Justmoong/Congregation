#include <iiAcountManager/GraphQL.h>
#include "licensemanager.h"

#include "licensecredentialstore.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>

#include <utility>

namespace
{
constexpr int maximumResponseSize = 64 * 1024;

const QString congregationProductId = QStringLiteral("congregation");
const QUrl productionValidationEndpoint(
    QStringLiteral("https://iisacc.com/graphql"));

bool isValidEmail(const QString &email)
{
    static const QRegularExpression emailPattern(
        QStringLiteral("^[^\\s@]+@[^\\s@]+\\.[^\\s@]+$"));
    if (email.size() < 3 || email.size() > 254 || !emailPattern.match(email).hasMatch()) {
        return false;
    }

    const QStringList parts = email.split(QLatin1Char('@'));
    return parts.size() == 2 && parts.at(0).size() <= 64 && parts.at(1).size() <= 253;
}

bool isValidLicenseKey(const QString &licenseKey)
{
    static const QRegularExpression licenseKeyPattern(
        QStringLiteral("^IIL([1-9][0-9]{0,4})_[A-Za-z0-9_-]{32}$"));
    const QRegularExpressionMatch match = licenseKeyPattern.match(licenseKey);
    if (!match.hasMatch()) {
        return false;
    }

    bool parsed = false;
    const int version = match.captured(1).toInt(&parsed);
    return parsed && version <= 32767;
}

bool hasJsonContentType(const QNetworkReply &reply)
{
    const QByteArray contentType = reply.rawHeader("Content-Type").trimmed().toLower();
    return contentType == QByteArrayLiteral("application/json")
        || contentType.startsWith(QByteArrayLiteral("application/json;"));
}

QByteArray serializedCredentials(const QString &email, const QString &licenseKey)
{
    QJsonObject credentials;
    credentials.insert(QStringLiteral("schema"), 1);
    credentials.insert(QStringLiteral("email"), email);
    credentials.insert(QStringLiteral("licenseKey"), licenseKey);
    credentials.insert(QStringLiteral("productId"), congregationProductId);
    return QJsonDocument(credentials).toJson(QJsonDocument::Compact);
}

bool parseStoredCredentials(const QByteArray &data, QString *email, QString *licenseKey)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }

    const QJsonObject object = document.object();
    if (object.size() != 4
        || !object.contains(QStringLiteral("schema"))
        || !object.contains(QStringLiteral("email"))
        || !object.contains(QStringLiteral("licenseKey"))
        || !object.contains(QStringLiteral("productId"))) {
        return false;
    }
    const QJsonValue schema = object.value(QStringLiteral("schema"));
    const QJsonValue storedEmail = object.value(QStringLiteral("email"));
    const QJsonValue storedLicenseKey = object.value(QStringLiteral("licenseKey"));
    const QJsonValue productId = object.value(QStringLiteral("productId"));
    if (!schema.isDouble() || schema.toInt(-1) != 1
        || !storedEmail.isString() || !storedLicenseKey.isString()
        || !productId.isString() || productId.toString() != congregationProductId) {
        return false;
    }

    const QString normalizedEmail = storedEmail.toString()
                                        .trimmed()
                                        .normalized(QString::NormalizationForm_KC)
                                        .toLower();
    const QString normalizedLicenseKey = storedLicenseKey.toString().trimmed();
    if (normalizedEmail != storedEmail.toString()
        || normalizedLicenseKey != storedLicenseKey.toString()
        || !isValidEmail(normalizedEmail)
        || !isValidLicenseKey(normalizedLicenseKey)) {
        return false;
    }

    // The secure-store payload is application-owned and always written in compact form.
    // Requiring the canonical representation also rejects duplicate keys and trailing data.
    if (serializedCredentials(normalizedEmail, normalizedLicenseKey) != data) {
        return false;
    }

    *email = normalizedEmail;
    *licenseKey = normalizedLicenseKey;
    return true;
}
}

StoredLicenseCredentials::StoredLicenseCredentials(QString credentialEmail,
                                                   QString credentialLicenseKey)
    : email(std::move(credentialEmail))
    , licenseKey(std::move(credentialLicenseKey))
{
}

StoredLicenseCredentials::StoredLicenseCredentials(StoredLicenseCredentials &&other) noexcept
    : email(std::move(other.email))
    , licenseKey(std::move(other.licenseKey))
{
}

StoredLicenseCredentials &StoredLicenseCredentials::operator=(StoredLicenseCredentials &&other) noexcept
{
    if (this != &other) {
        clear();
        email = std::move(other.email);
        licenseKey = std::move(other.licenseKey);
    }
    return *this;
}

StoredLicenseCredentials::~StoredLicenseCredentials()
{
    clear();
}

void StoredLicenseCredentials::clear()
{
    email.fill(QChar{});
    licenseKey.fill(QChar{});
    email.clear();
    licenseKey.clear();
}

LicenseManager::LicenseManager(QObject *parent)
    : LicenseManager(EnforcementMode::Enabled, parent)
{
}

LicenseManager::LicenseManager(EnforcementMode enforcementMode, QObject *parent)
    : LicenseManager(productionValidationEndpoint, 10000, nullptr, enforcementMode, parent)
{
    m_credentialStore = createPlatformLicenseCredentialStore(this);
    if (m_enforcementEnabled) {
        QTimer::singleShot(0, this, &LicenseManager::restoreStoredLicense);
    }
}

LicenseManager::LicenseManager(const QUrl &validationEndpoint,
                               int requestTimeoutMilliseconds,
                               QObject *parent)
    : QObject(parent)
    , m_validationEndpoint(validationEndpoint)
    , m_requestTimeoutMilliseconds(qMax(1, requestTimeoutMilliseconds))
{
    m_requestTimeout.setSingleShot(true);
    connect(&m_requestTimeout, &QTimer::timeout, this, [this]() {
        if (m_activeReply) {
            m_activeReply->abort();
        }
    });
}

LicenseManager::LicenseManager(const QUrl &validationEndpoint,
                               int requestTimeoutMilliseconds,
                               LicenseCredentialStore *credentialStore,
                               QObject *parent)
    : LicenseManager(validationEndpoint,
                     requestTimeoutMilliseconds,
                     credentialStore,
                     EnforcementMode::Enabled,
                     parent)
{
}

LicenseManager::LicenseManager(const QUrl &validationEndpoint,
                               int requestTimeoutMilliseconds,
                               LicenseCredentialStore *credentialStore,
                               EnforcementMode enforcementMode,
                               QObject *parent)
    : LicenseManager(validationEndpoint, requestTimeoutMilliseconds, parent)
{
    m_credentialStore = credentialStore;
    m_enforcementEnabled = enforcementMode == EnforcementMode::Enabled;
    m_licensed = !m_enforcementEnabled;
    if (m_enforcementEnabled && m_credentialStore) {
        QTimer::singleShot(0, this, &LicenseManager::restoreStoredLicense);
    }
}

QString LicenseManager::productId() const
{
    return congregationProductId;
}

bool LicenseManager::enforcementEnabled() const
{
    return m_enforcementEnabled;
}

bool LicenseManager::licensed() const
{
    return m_licensed;
}

bool LicenseManager::verifying() const
{
    return m_verifying;
}

bool LicenseManager::persistenceSupported() const
{
    return m_credentialStore && m_credentialStore->persistenceSupported();
}

bool LicenseManager::hasStoredLicense() const
{
    return m_hasStoredLicense;
}

QString LicenseManager::resultCode() const
{
    return m_resultCode;
}

void LicenseManager::validateLicense(const QString &email, const QString &licenseKey)
{
    if (!m_enforcementEnabled || m_licensed || m_verifying) {
        return;
    }

    const QString normalizedEmail = email.trimmed().normalized(QString::NormalizationForm_KC).toLower();
    const QString normalizedLicenseKey = licenseKey.trimmed();
    if (!isValidEmail(normalizedEmail) || !isValidLicenseKey(normalizedLicenseKey)) {
        setLicensed(false);
        setResultCode(QStringLiteral("invalid_input"));
        emit validationFinished(false);
        return;
    }

    startValidation(normalizedEmail, normalizedLicenseKey, false);
}

void LicenseManager::retryStoredLicense()
{
    if (!m_enforcementEnabled || m_licensed || m_verifying || !m_hasStoredLicense
        || !isValidEmail(m_storedEmail) || !isValidLicenseKey(m_storedLicenseKey)) {
        return;
    }

    startValidation(m_storedEmail, m_storedLicenseKey, true);
}

void LicenseManager::forgetLicense()
{
    if (!m_enforcementEnabled || m_licensed) {
        return;
    }

    m_requestTimeout.stop();
    if (m_activeReply) {
        QNetworkReply *reply = m_activeReply.data();
        disconnect(reply, nullptr, this, nullptr);
        m_activeReply.clear();
        reply->abort();
        reply->deleteLater();
    }

    setVerifying(false);
    setLicensed(false);
    setResultCode(QString{});
    clearStoredCredentials();
}

void LicenseManager::requestStoredCredentials(StoredCredentialCompletion completion)
{
    if (!completion) {
        return;
    }

    if (!persistenceSupported()) {
        completion(StoredCredentialStatus::Unavailable, StoredLicenseCredentials{});
        return;
    }

    QPointer<LicenseManager> guard(this);
    m_credentialStore->read(
        [guard, completion = std::move(completion)](
            LicenseCredentialStore::ReadStatus status, QByteArray data) mutable {
            if (!guard) {
                data.fill('\0');
                return;
            }

            if (status == LicenseCredentialStore::ReadStatus::Error) {
                data.fill('\0');
                completion(StoredCredentialStatus::Unavailable, StoredLicenseCredentials{});
                return;
            }
            if (status == LicenseCredentialStore::ReadStatus::NotFound) {
                data.fill('\0');
                completion(StoredCredentialStatus::NotFound, StoredLicenseCredentials{});
                return;
            }

            QString email;
            QString licenseKey;
            const bool parsed = parseStoredCredentials(data, &email, &licenseKey);
            data.fill('\0');
            if (!parsed) {
                email.fill(QChar{});
                licenseKey.fill(QChar{});
                completion(StoredCredentialStatus::Invalid, StoredLicenseCredentials{});
                return;
            }

            completion(StoredCredentialStatus::Available,
                       StoredLicenseCredentials(std::move(email), std::move(licenseKey)));
        });
}

void LicenseManager::restoreStoredLicense()
{
    if (!m_enforcementEnabled || !persistenceSupported() || m_verifying || m_licensed) {
        return;
    }

    setVerifying(true);
    QPointer<LicenseManager> guard(this);
    m_credentialStore->read([guard](LicenseCredentialStore::ReadStatus status, QByteArray data) mutable {
        if (!guard) {
            data.fill('\0');
            return;
        }

        guard->setVerifying(false);
        if (status == LicenseCredentialStore::ReadStatus::Error) {
            data.fill('\0');
            guard->setResultCode(QStringLiteral("secure_storage_unavailable"));
            return;
        }
        if (status == LicenseCredentialStore::ReadStatus::NotFound) {
            data.fill('\0');
            return;
        }

        QString email;
        QString licenseKey;
        const bool parsed = parseStoredCredentials(data, &email, &licenseKey);
        data.fill('\0');
        if (!parsed) {
            guard->setResultCode(QStringLiteral("stored_license_removed"));
            guard->clearStoredCredentials();
            return;
        }

        guard->m_storedEmail = email;
        guard->m_storedLicenseKey = licenseKey;
        guard->setHasStoredLicense(true);
        guard->startValidation(email, licenseKey, true);
    });
}

void LicenseManager::startValidation(const QString &normalizedEmail,
                                     const QString &normalizedLicenseKey,
                                     bool fromStoredCredentials)
{
    setResultCode(QString{});
    setVerifying(true);
    m_responseTooLarge = false;
    m_requestUsesStoredCredentials = fromStoredCredentials;
    m_activeEmail = normalizedEmail;
    m_activeLicenseKey = normalizedLicenseKey;

    QNetworkRequest request(m_validationEndpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
    request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
    request.setRawHeader(QByteArrayLiteral("Cache-Control"), QByteArrayLiteral("no-store"));
    request.setRawHeader(QByteArrayLiteral("Pragma"), QByteArrayLiteral("no-cache"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);

    QJsonObject requestObject;
    requestObject.insert(QStringLiteral("email"), normalizedEmail);
    requestObject.insert(QStringLiteral("licenseKey"), normalizedLicenseKey);
    requestObject.insert(QStringLiteral("productId"), congregationProductId);
    QByteArray requestBody = iisacc::accounts::graphql::request(
        QStringLiteral("mutation ValidateLicense($input: LicenseValidationInput!) { validateLicense(input: $input) { valid code productId checkedAt expiresAt } }"), requestObject);

    m_activeReply = m_networkAccessManager.post(request, requestBody);
    requestBody.fill('\0');

    QNetworkReply *reply = m_activeReply.data();
    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
        if (m_activeReply == reply && reply->bytesAvailable() > maximumResponseSize) {
            m_responseTooLarge = true;
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, &LicenseManager::finishValidation);
    m_requestTimeout.start(m_requestTimeoutMilliseconds);
}

void LicenseManager::finishValidation()
{
    QNetworkReply *reply = m_activeReply.data();
    if (!reply) {
        setLicensed(false);
        setResultCode(QStringLiteral("verification_unavailable"));
        setVerifying(false);
        emit validationFinished(false);
        return;
    }

    m_requestTimeout.stop();
    m_activeReply.clear();

    bool authoritativeDecision = false;
    bool valid = false;
    const QVariant redirectTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray responseBody = reply->readAll();

    if (!m_responseTooLarge
        && !redirectTarget.isValid()
        && reply->error() == QNetworkReply::NoError
        && statusCode == 200
        && responseBody.size() <= maximumResponseSize
        && hasJsonContentType(*reply)) {
        QJsonParseError parseError;
        const auto decoded = iisacc::accounts::graphql::decode(responseBody, "validateLicense", statusCode);
        const QJsonDocument responseDocument = QJsonDocument::fromJson(decoded.body, &parseError);
        if (decoded.valid && decoded.status == 200 && parseError.error == QJsonParseError::NoError && responseDocument.isObject()) {
            const QJsonObject responseObject = responseDocument.object();
            const QJsonValue validValue = responseObject.value(QStringLiteral("valid"));
            const QJsonValue responseProductId = responseObject.value(QStringLiteral("productId"));
            const bool productMatches = responseProductId.isNull() || !responseObject.contains(QStringLiteral("productId"))
                || (responseProductId.isString() && responseProductId.toString() == congregationProductId);

            if (validValue.isBool() && productMatches) {
                valid = validValue.toBool();
                authoritativeDecision = !valid
                    || (responseProductId.isString() && responseProductId.toString() == congregationProductId);
            }
        }
    }

    if (authoritativeDecision && valid && !m_requestUsesStoredCredentials
        && persistenceSupported()) {
        reply->deleteLater();
        storeValidatedCredentials();
        return;
    }

    if (authoritativeDecision) {
        setLicensed(valid);
        setResultCode(valid ? QString{} : QStringLiteral("invalid_license"));
        if (valid) {
            m_activeEmail.clear();
            m_activeLicenseKey.clear();
            m_storedEmail.clear();
            m_storedLicenseKey.clear();
        } else if (m_requestUsesStoredCredentials) {
            clearStoredCredentials();
        } else {
            m_activeEmail.clear();
            m_activeLicenseKey.clear();
        }
    } else {
        setLicensed(false);
        setResultCode(QStringLiteral("verification_unavailable"));
        if (!m_requestUsesStoredCredentials) {
            m_activeEmail.clear();
            m_activeLicenseKey.clear();
        }
    }

    setVerifying(false);
    reply->deleteLater();
    emit validationFinished(authoritativeDecision && valid);
}

void LicenseManager::storeValidatedCredentials()
{
    m_storedEmail = m_activeEmail;
    m_storedLicenseKey = m_activeLicenseKey;
    QByteArray credentials = serializedCredentials(m_storedEmail, m_storedLicenseKey);
    QPointer<LicenseManager> guard(this);
    m_credentialStore->write(credentials, [guard](bool stored) {
        if (!guard) {
            return;
        }

        guard->setHasStoredLicense(stored);
        guard->m_storedEmail.clear();
        guard->m_storedLicenseKey.clear();
        guard->setResultCode(stored
                                 ? QString{}
                                 : QStringLiteral("secure_storage_unavailable"));
        guard->setLicensed(true);
        guard->setVerifying(false);
        emit guard->validationFinished(true);
    });
    credentials.fill('\0');
    m_activeEmail.clear();
    m_activeLicenseKey.clear();
}

void LicenseManager::clearStoredCredentials()
{
    m_activeEmail.clear();
    m_activeLicenseKey.clear();
    m_storedEmail.clear();
    m_storedLicenseKey.clear();
    setHasStoredLicense(false);
    if (!persistenceSupported()) {
        return;
    }

    QPointer<LicenseManager> guard(this);
    m_credentialStore->remove([guard](bool removed) {
        if (guard && !removed) {
            guard->setResultCode(QStringLiteral("secure_storage_unavailable"));
        }
    });
}

void LicenseManager::setHasStoredLicense(bool hasStoredLicense)
{
    if (m_hasStoredLicense == hasStoredLicense) {
        return;
    }

    m_hasStoredLicense = hasStoredLicense;
    emit hasStoredLicenseChanged();
}

void LicenseManager::setLicensed(bool licensed)
{
    if (m_licensed == licensed) {
        return;
    }

    m_licensed = licensed;
    emit licensedChanged();
}

void LicenseManager::setVerifying(bool verifying)
{
    if (m_verifying == verifying) {
        return;
    }

    m_verifying = verifying;
    emit verifyingChanged();
}

void LicenseManager::setResultCode(const QString &resultCode)
{
    if (m_resultCode == resultCode) {
        return;
    }

    m_resultCode = resultCode;
    emit resultCodeChanged();
}
