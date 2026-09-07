#pragma once

#include <iiUpdateManager/UpdateCredentialProvider.h>

#include <QPointer>

class LicenseManager;

class CongregationUpdateCredentialProvider final
    : public iisacc::updates::UpdateCredentialProvider
{
public:
    explicit CongregationUpdateCredentialProvider(LicenseManager *licenseManager,
                                             QObject *parent = nullptr);

    void requestCredentials(Completion completion) override;
    void cancel() noexcept override;

private:
    QPointer<LicenseManager> m_licenseManager;
    quint64 m_requestGeneration = 0;
};
