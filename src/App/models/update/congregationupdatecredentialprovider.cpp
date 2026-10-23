#include "congregationupdatecredentialprovider.h"

#include "licensemanager.h"

#include <QByteArray>

#include <optional>
#include <utility>

using iisacc::updates::native::UpdateCredentials;

CongregationUpdateCredentialProvider::CongregationUpdateCredentialProvider(
    LicenseManager* licenseManager, QObject* parent)
    : QObject(parent), m_licenseManager(licenseManager)
{
}

void CongregationUpdateCredentialProvider::requestCredentials(Completion completion)
{
    if (!completion)
    {
        return;
    }

    const quint64 generation = ++m_requestGeneration;
    if (!m_licenseManager)
    {
        completion(std::nullopt, "Stored license credentials are unavailable.");
        return;
    }

    QPointer<CongregationUpdateCredentialProvider> guard(this);
    m_licenseManager->requestStoredCredentials(
        [guard, generation,
         completion = std::move(completion)](LicenseManager::StoredCredentialStatus status,
                                             StoredLicenseCredentials credentials) mutable
        {
            if (!guard || guard->m_requestGeneration != generation)
            {
                return;
            }

            if (status == LicenseManager::StoredCredentialStatus::Invalid)
            {
                std::optional<UpdateCredentials> invalidCredentials(std::in_place);
                completion(std::move(invalidCredentials),
                           "The stored Congregation license is invalid.");
                return;
            }

            if (status != LicenseManager::StoredCredentialStatus::Available)
            {
                const std::string message =
                    status == LicenseManager::StoredCredentialStatus::NotFound
                        ? "No stored Congregation license is available."
                        : "Secure license storage is unavailable.";
                completion(std::nullopt, message);
                return;
            }

            std::optional<UpdateCredentials> updateCredentials(std::in_place);
            updateCredentials->email = credentials.email.toStdString();
            updateCredentials->licenseKey = credentials.licenseKey.toStdString();
            credentials.clear();
            completion(std::move(updateCredentials), {});
        });
}

void CongregationUpdateCredentialProvider::cancel() noexcept
{
    ++m_requestGeneration;
}
