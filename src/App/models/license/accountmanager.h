#pragma once

#include <QObject>
#include <QString>
#include <memory>
namespace ii::runtime
{
class EventLoop;
}
class QtEngineHost;

namespace iisacc::licensing::native
{
class LicenseClient;
}

class LicenseManager;

class AccountManager final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString accountEmail READ accountEmail NOTIFY accountEmailChanged)
    Q_PROPERTY(bool accountEmailLoading READ accountEmailLoading NOTIFY accountEmailLoadingChanged)

  public:
    explicit AccountManager(LicenseManager* credentialSource, QObject* parent = nullptr);
    AccountManager(LicenseManager* credentialSource,
                   iisacc::licensing::native::LicenseClient* licenseClient,
                   QObject* parent = nullptr);

    ~AccountManager() override;
    [[nodiscard]] QString accountEmail() const;
    [[nodiscard]] bool accountEmailLoading() const;

    Q_INVOKABLE void refresh();

  signals:
    void accountEmailChanged();
    void accountEmailLoadingChanged();

  private:
    void setAccountEmail(const QString& accountEmail);
    void setAccountEmailLoading(bool accountEmailLoading);

    LicenseManager* m_credentialSource = nullptr;
    iisacc::licensing::native::LicenseClient* m_licenseClient = nullptr;
    std::unique_ptr<ii::runtime::EventLoop> m_events;
    std::unique_ptr<QtEngineHost> m_host;
    std::unique_ptr<iisacc::licensing::native::LicenseClient> m_ownedClient;
    QString m_accountEmail;
    bool m_accountEmailLoading = false;
};
