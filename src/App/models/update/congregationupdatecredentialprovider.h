#pragma once

#include <iiUpdateManager/NativeUpdateManager.h>

#include <QPointer>
#include <QObject>

class LicenseManager;

class CongregationUpdateCredentialProvider final
    : public QObject,
      public iisacc::updates::native::UpdateCredentialProvider
{
  public:
    explicit CongregationUpdateCredentialProvider(LicenseManager* licenseManager,
                                                  QObject* parent = nullptr);

    void requestCredentials(Completion completion) override;
    void cancel() noexcept override;

  private:
    QPointer<LicenseManager> m_licenseManager;
    quint64 m_requestGeneration = 0;
};
