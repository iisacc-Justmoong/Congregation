#include "licensecredentialstore.h"

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
#if __has_include(<qt6keychain/keychain.h>)
#include <qt6keychain/keychain.h>
#else
#include <qtkeychain/keychain.h>
#endif
#endif

#include <utility>

namespace
{
const QString credentialService = QStringLiteral("com.iisacc.congregation.painter");
const QString credentialKey = QStringLiteral("account-license");

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
class QtKeychainLicenseCredentialStore final : public LicenseCredentialStore
{
public:
    explicit QtKeychainLicenseCredentialStore(QObject *parent)
        : LicenseCredentialStore(parent)
    {
    }

    [[nodiscard]] bool persistenceSupported() const override
    {
        return QKeychain::isAvailable();
    }

    void read(ReadCompletion completion) override
    {
        auto *job = new QKeychain::ReadPasswordJob(credentialService, this);
        job->setKey(credentialKey);
        job->setInsecureFallback(false);
        connect(job, &QKeychain::Job::finished, this, [completion = std::move(completion)](QKeychain::Job *baseJob) mutable {
            auto *readJob = static_cast<QKeychain::ReadPasswordJob *>(baseJob);
            if (readJob->error() == QKeychain::NoError) {
                completion(ReadStatus::Found, readJob->binaryData());
                return;
            }
            if (readJob->error() == QKeychain::EntryNotFound) {
                completion(ReadStatus::NotFound, {});
                return;
            }
            completion(ReadStatus::Error, {});
        });
        job->start();
    }

    void write(const QByteArray &credentials, MutationCompletion completion) override
    {
        auto *job = new QKeychain::WritePasswordJob(credentialService, this);
        job->setKey(credentialKey);
        job->setInsecureFallback(false);
        job->setBinaryData(credentials);
        connect(job, &QKeychain::Job::finished, this, [completion = std::move(completion)](QKeychain::Job *baseJob) mutable {
            completion(baseJob->error() == QKeychain::NoError);
        });
        job->start();
    }

    void remove(MutationCompletion completion) override
    {
        auto *job = new QKeychain::DeletePasswordJob(credentialService, this);
        job->setKey(credentialKey);
        job->setInsecureFallback(false);
        connect(job, &QKeychain::Job::finished, this, [completion = std::move(completion)](QKeychain::Job *baseJob) mutable {
            completion(baseJob->error() == QKeychain::NoError
                       || baseJob->error() == QKeychain::EntryNotFound);
        });
        job->start();
    }
};
#else
class UnsupportedLicenseCredentialStore final : public LicenseCredentialStore
{
public:
    explicit UnsupportedLicenseCredentialStore(QObject *parent)
        : LicenseCredentialStore(parent)
    {
    }

    [[nodiscard]] bool persistenceSupported() const override
    {
        return false;
    }

    void read(ReadCompletion completion) override
    {
        completion(ReadStatus::NotFound, {});
    }

    void write(const QByteArray &, MutationCompletion completion) override
    {
        completion(false);
    }

    void remove(MutationCompletion completion) override
    {
        completion(true);
    }
};
#endif
}

LicenseCredentialStore *createPlatformLicenseCredentialStore(QObject *parent)
{
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    return new QtKeychainLicenseCredentialStore(parent);
#else
    return new UnsupportedLicenseCredentialStore(parent);
#endif
}
