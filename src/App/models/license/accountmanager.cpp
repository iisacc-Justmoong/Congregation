#include "accountmanager.h"

#include "licensemanager.h"

#include <iiLicenseManager/NativeLicenseClient.h>
#include <Engine/Preferences.h>
#include "../../host/qtenginehost.h"

#include <QPointer>

using iisacc::licensing::native::LicenseClient;

AccountManager::AccountManager(LicenseManager* credentialSource, QObject* parent)
    : QObject(parent), m_credentialSource(credentialSource)
{
    m_events = std::make_unique<ii::runtime::EventLoop>();
    m_host = std::make_unique<QtEngineHost>(*m_events);
    m_ownedClient =
        std::make_unique<LicenseClient>(*m_events, "congregation", "https://iisacc.com/graphql",
                                        congregation::application_data() / "iisacc/licenses");
    m_licenseClient = m_ownedClient.get();
}

AccountManager::AccountManager(LicenseManager* credentialSource, LicenseClient* licenseClient,
                               QObject* parent)
    : QObject(parent), m_credentialSource(credentialSource), m_licenseClient(licenseClient)
{
    Q_ASSERT(m_licenseClient);
}

AccountManager::~AccountManager()
{
    if (m_licenseClient)
        m_licenseClient->validationFinished = {};
    m_ownedClient.reset();
    m_host.reset();
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
    if (m_accountEmailLoading || !m_licenseClient)
    {
        return;
    }

    setAccountEmailLoading(true);
    if (m_licenseClient->loadLicense())
    {
        setAccountEmail(QString::fromStdString(m_licenseClient->accountEmail()));
        setAccountEmailLoading(false);
        return;
    }

    if (!m_credentialSource)
    {
        setAccountEmail({});
        setAccountEmailLoading(false);
        return;
    }

    QPointer<AccountManager> guard(this);
    m_credentialSource->requestStoredCredentials(
        [guard](LicenseManager::StoredCredentialStatus status,
                StoredLicenseCredentials credentials) mutable
        {
            if (!guard)
            {
                return;
            }
            if (status != LicenseManager::StoredCredentialStatus::Available)
            {
                credentials.clear();
                guard->setAccountEmail({});
                guard->setAccountEmailLoading(false);
                return;
            }

            guard->m_licenseClient->validationFinished = [guard](bool valid, const std::string&)
            {
                if (!guard)
                    return;
                guard->setAccountEmail(
                    valid ? QString::fromStdString(guard->m_licenseClient->accountEmail())
                          : QString{});
                guard->setAccountEmailLoading(false);
            };
            guard->m_licenseClient->activate(credentials.email.toStdString(),
                                             credentials.licenseKey.toStdString());
            credentials.clear();
        });
}

void AccountManager::setAccountEmail(const QString& accountEmail)
{
    if (m_accountEmail == accountEmail)
    {
        return;
    }
    m_accountEmail = accountEmail;
    emit accountEmailChanged();
}

void AccountManager::setAccountEmailLoading(bool accountEmailLoading)
{
    if (m_accountEmailLoading == accountEmailLoading)
    {
        return;
    }
    m_accountEmailLoading = accountEmailLoading;
    emit accountEmailLoadingChanged();
}
