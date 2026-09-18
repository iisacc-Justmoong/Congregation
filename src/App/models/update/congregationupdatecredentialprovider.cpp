#include "congregationupdatecredentialprovider.h"

#include "licensemanager.h"

#include <QByteArray>

#include <optional>
#include <utility>

using iisacc::updates::UpdateCredentials;

CongregationUpdateCredentialProvider::CongregationUpdateCredentialProvider(
    LicenseManager *licenseManager,
    QObject *parent)
    : UpdateCredentialProvider(parent)
    , m_licenseManager(licenseManager)
{
}

void CongregationUpdateCredentialProvider::requestCredentials(Completion completion)
{
    if (!completion) {
        return;
    }

    const quint64 generation = ++m_requestGeneration;
    if (!m_licenseManager) {
        completion(std::nullopt,
                   QStringLiteral("Stored license credentials are unavailable."));
        return;
    }

    QPointer<CongregationUpdateCredentialProvider> guard(this);
    m_licenseManager->requestStoredCredentials(
        [guard, generation, completion = std::move(completion)](
            LicenseManager::StoredCredentialStatus status,
            StoredLicenseCredentials credentials) mutable {
            if (!guard || guard->m_requestGeneration != generation) {
                return;
            }

            if (status == LicenseManager::StoredCredentialStatus::Invalid) {
                std::optional<UpdateCredentials> invalidCredentials(std::in_place);
                completion(std::move(invalidCredentials),
                           QStringLiteral("The stored Congregation license is invalid."));
                return;
            }

            if (status != LicenseManager::StoredCredentialStatus::Available) {
                const QString message = status == LicenseManager::StoredCredentialStatus::NotFound
                    ? QStringLiteral("No stored Congregation license is available.")
                    : QStringLiteral("Secure license storage is unavailable.");
                completion(std::nullopt, message);
                return;
            }

            QByteArray licenseKey = credentials.licenseKey.toUtf8();
            QString email = std::move(credentials.email);
            credentials.clear();
            std::optional<UpdateCredentials> updateCredentials(
                std::in_place,
                std::move(email),
                std::move(licenseKey));
            if (!updateCredentials->isValid()) {
                updateCredentials->clear();
                completion(std::nullopt,
                           QStringLiteral("The stored Congregation license is invalid."));
                return;
            }

            completion(std::move(updateCredentials), {});
        });
}

void CongregationUpdateCredentialProvider::cancel() noexcept
{
    ++m_requestGeneration;
}
