#include "licensemanager.h"
#include "licensecredentialstore.h"
#include "../../host/qtenginehost.h"
#include <Engine/LicenseService.h>
#include <QPointer>
#include <utility>
using congregation::LicenseService;
StoredLicenseCredentials::StoredLicenseCredentials(QString credentialEmail,
                                                   QString credentialLicenseKey)
    : email(std::move(credentialEmail)), licenseKey(std::move(credentialLicenseKey))
{
}

StoredLicenseCredentials::StoredLicenseCredentials(StoredLicenseCredentials&& other) noexcept
    : email(std::move(other.email)), licenseKey(std::move(other.licenseKey))
{
}

StoredLicenseCredentials&
StoredLicenseCredentials::operator=(StoredLicenseCredentials&& other) noexcept
{
    if (this != &other)
    {
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

LicenseManager::LicenseManager(QObject* parent) : LicenseManager(EnforcementMode::Enabled, parent)
{
}
LicenseManager::LicenseManager(EnforcementMode mode, QObject* parent) : QObject(parent)
{
    initialize(QUrl(QStringLiteral("https://iisacc.com/graphql")), 10000,
               createPlatformLicenseCredentialStore(this), mode);
}
LicenseManager::LicenseManager(const QUrl& endpoint, int timeout, QObject* parent) : QObject(parent)
{
    initialize(endpoint, timeout, nullptr, EnforcementMode::Enabled);
}
LicenseManager::LicenseManager(const QUrl& endpoint, int timeout, LicenseCredentialStore* store,
                               QObject* parent)
    : LicenseManager(endpoint, timeout, store, EnforcementMode::Enabled, parent)
{
}
LicenseManager::LicenseManager(const QUrl& endpoint, int timeout, LicenseCredentialStore* store,
                               EnforcementMode mode, QObject* parent)
    : QObject(parent)
{
    initialize(endpoint, timeout, store, mode);
}
void LicenseManager::initialize(const QUrl& endpoint, int timeout, LicenseCredentialStore* store,
                                EnforcementMode mode)
{
    m_events = std::make_unique<ii::runtime::EventLoop>();
    m_host = std::make_unique<QtEngineHost>(*m_events);
    QPointer<LicenseCredentialStore> guarded(store);
    congregation::CredentialPort port;
    port.supported = [guarded] { return guarded && guarded->persistenceSupported(); };
    port.read = [guarded](auto completion)
    {
        if (!guarded)
        {
            completion(congregation::CredentialPort::ReadStatus::error, {});
            return;
        }
        guarded->read(
            [completion = std::move(completion)](auto status, QByteArray bytes)
            {
                auto native = status == LicenseCredentialStore::ReadStatus::Found
                                  ? congregation::CredentialPort::ReadStatus::found
                              : status == LicenseCredentialStore::ReadStatus::NotFound
                                  ? congregation::CredentialPort::ReadStatus::not_found
                                  : congregation::CredentialPort::ReadStatus::error;
                auto data = bytes.toStdString();
                bytes.fill('\0');
                completion(native, std::move(data));
            });
    };
    port.write = [guarded](std::string_view data, auto completion)
    {
        if (!guarded)
        {
            completion(false);
            return;
        }
        QByteArray bytes(data.data(), data.size());
        guarded->write(bytes, std::move(completion));
        bytes.fill('\0');
    };
    port.remove = [guarded](auto completion)
    {
        if (guarded)
            guarded->remove(std::move(completion));
        else
            completion(false);
    };
    m_backend = std::make_unique<LicenseService>(
        *m_events, endpoint.toString(QUrl::FullyEncoded).toStdString(), timeout,
        mode == EnforcementMode::Enabled, std::move(port));
    m_backend->changed = [this](auto notice)
    {
        switch (notice)
        {
        case LicenseService::Notice::licensed:
            emit licensedChanged();
            break;
        case LicenseService::Notice::verifying:
            emit verifyingChanged();
            break;
        case LicenseService::Notice::stored:
            emit hasStoredLicenseChanged();
            break;
        case LicenseService::Notice::code:
            emit resultCodeChanged();
            break;
        }
    };
    m_backend->validated = [this](bool valid) { emit validationFinished(valid); };
    // Restoring is admitted by Core; destruction suppresses pending callbacks.
    QPointer<LicenseManager> guard(this);
    (void) m_events->post(
        [guard]
        {
            if (guard)
                guard->m_backend->restore();
        });
}
LicenseManager::~LicenseManager()
{
    m_backend.reset();
    m_host.reset();
}
QString LicenseManager::productId() const
{
    return QStringLiteral("congregation");
}
bool LicenseManager::enforcementEnabled() const
{
    return m_backend->enforced;
}
bool LicenseManager::licensed() const
{
    return m_backend->licensed;
}
bool LicenseManager::verifying() const
{
    return m_backend->verifying;
}
bool LicenseManager::persistenceSupported() const
{
    return m_backend->persistence();
}
bool LicenseManager::hasStoredLicense() const
{
    return m_backend->stored;
}
QString LicenseManager::resultCode() const
{
    return QString::fromStdString(m_backend->code);
}
void LicenseManager::validateLicense(const QString& email, const QString& key)
{
    m_backend->validate(email.toStdString(), key.toStdString());
}
void LicenseManager::retryStoredLicense()
{
    m_backend->retry();
}
void LicenseManager::forgetLicense()
{
    m_backend->forget();
}
void LicenseManager::requestStoredCredentials(StoredCredentialCompletion completion)
{
    if (!completion)
        return;
    m_backend->credentials(
        [completion = std::move(completion)](auto status, congregation::Credentials c) mutable
        {
            auto mapped = status == LicenseService::StoredStatus::available
                              ? StoredCredentialStatus::Available
                          : status == LicenseService::StoredStatus::not_found
                              ? StoredCredentialStatus::NotFound
                          : status == LicenseService::StoredStatus::invalid
                              ? StoredCredentialStatus::Invalid
                              : StoredCredentialStatus::Unavailable;
            completion(mapped, StoredLicenseCredentials(QString::fromStdString(c.email),
                                                        QString::fromStdString(c.key)));
        });
}
