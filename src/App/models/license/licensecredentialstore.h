#pragma once

#include <QByteArray>
#include <QObject>

#include <functional>

class LicenseCredentialStore : public QObject
{
public:
    enum class ReadStatus
    {
        Found,
        NotFound,
        Error
    };

    using ReadCompletion = std::function<void(ReadStatus, QByteArray)>;
    using MutationCompletion = std::function<void(bool)>;

    explicit LicenseCredentialStore(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~LicenseCredentialStore() override = default;

    [[nodiscard]] virtual bool persistenceSupported() const = 0;
    virtual void read(ReadCompletion completion) = 0;
    virtual void write(const QByteArray &credentials, MutationCompletion completion) = 0;
    virtual void remove(MutationCompletion completion) = 0;
};

LicenseCredentialStore *createPlatformLicenseCredentialStore(QObject *parent);
