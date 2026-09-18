#include "accountmanager.h"

#include "licensemanager.h"

#include <iiLicenseManager/LicenseClient.h>

#include <QPointer>

using iisacc::licensing::LicenseClient;

AccountManager::AccountManager(LicenseManager *credentialSource, QObject *parent)
    : QObject(parent)
    , m_credentialSource(credentialSource)
    , m_licenseClient(new LicenseClient(QStringLiteral("congregation"), this))
{
}

AccountManager::AccountManager(LicenseManager *credentialSource,
                               LicenseClient *licenseClient,
                               QObject *parent)
    : QObject(parent)
    , m_credentialSource(credentialSource)
    , m_licenseClient(licenseClient)
{
    Q_ASSERT(m_licenseClient);
}

QString AccountManager::accountEmail() const
{
    return m_accountEmail;
}

bool AccountManager::accountEmailLoading() const
{
    return m_accountEmailLoading;
}

void AccountManager::refresh()
{
    if (m_accountEmailLoading || !m_licenseClient) {
        return;
    }

    setAccountEmailLoading(true);
    if (m_licenseClient->loadLicense()) {
        setAccountEmail(m_licenseClient->accountEmail());
        setAccountEmailLoading(false);
        return;
    }

    if (!m_credentialSource) {
        setAccountEmail({});
        setAccountEmailLoading(false);
        return;
    }

    QPointer<AccountManager> guard(this);
    m_credentialSource->requestStoredCredentials(
        [guard](LicenseManager::StoredCredentialStatus status,
                StoredLicenseCredentials credentials) mutable {
            if (!guard) {
                return;
            }
            if (status != LicenseManager::StoredCredentialStatus::Available) {
                credentials.clear();
                guard->setAccountEmail({});
                guard->setAccountEmailLoading(false);
                return;
            }

            guard->m_activationConnection =
                QObject::connect(guard->m_licenseClient,
                                 &LicenseClient::validationFinished,
                                 guard,
                                 [guard](bool valid, const QString &) {
                                     if (!guard) {
                                         return;
                                     }
                                     QObject::disconnect(guard->m_activationConnection);
                                     guard->m_activationConnection = {};
                                     guard->setAccountEmail(
                                         valid ? guard->m_licenseClient->accountEmail()
                                               : QString{});
                                     guard->setAccountEmailLoading(false);
                                 });
            guard->m_licenseClient->activate(credentials.email, credentials.licenseKey);
            credentials.clear();
        });
}

void AccountManager::setAccountEmail(const QString &accountEmail)
{
    if (m_accountEmail == accountEmail) {
        return;
    }
    m_accountEmail = accountEmail;
    emit accountEmailChanged();
}

void AccountManager::setAccountEmailLoading(bool accountEmailLoading)
{
    if (m_accountEmailLoading == accountEmailLoading) {
        return;
    }
    m_accountEmailLoading = accountEmailLoading;
    emit accountEmailLoadingChanged();
}
