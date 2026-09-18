#pragma once

#include <QObject>
#include <QString>

namespace iisacc::licensing
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
    explicit AccountManager(LicenseManager *credentialSource, QObject *parent = nullptr);
    AccountManager(LicenseManager *credentialSource,
                   iisacc::licensing::LicenseClient *licenseClient,
                   QObject *parent = nullptr);

    [[nodiscard]] QString accountEmail() const;
    [[nodiscard]] bool accountEmailLoading() const;

    Q_INVOKABLE void refresh();

signals:
    void accountEmailChanged();
    void accountEmailLoadingChanged();

private:
    void setAccountEmail(const QString &accountEmail);
    void setAccountEmailLoading(bool accountEmailLoading);

    LicenseManager *m_credentialSource = nullptr;
    iisacc::licensing::LicenseClient *m_licenseClient = nullptr;
    QMetaObject::Connection m_activationConnection;
    QString m_accountEmail;
    bool m_accountEmailLoading = false;
};
