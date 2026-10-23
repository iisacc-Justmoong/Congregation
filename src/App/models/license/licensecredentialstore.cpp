#include "licensecredentialstore.h"
#include <iiAcountManager/NativeAccount.h>
namespace
{
class PlatformLicenseCredentialStore final : public LicenseCredentialStore
{
    iisacc::accounts::native::SecureStore store_{"com.iisacc.congregation.painter",
                                                 "account-license"};

  public:
    explicit PlatformLicenseCredentialStore(QObject* parent) : LicenseCredentialStore(parent) {}
    bool persistenceSupported() const override { return store_.supported(); }
    void read(ReadCompletion completion) override
    {
        auto result = store_.read();
        auto status = result.status == iisacc::accounts::native::SecretResult::Status::found
                          ? ReadStatus::Found
                      : result.status == iisacc::accounts::native::SecretResult::Status::not_found
                          ? ReadStatus::NotFound
                          : ReadStatus::Error;
        auto bytes = QByteArray::fromStdString(result.bytes);
        std::fill(result.bytes.begin(), result.bytes.end(), '\0');
        completion(status, std::move(bytes));
    }
    void write(const QByteArray& bytes, MutationCompletion completion) override
    {
        completion(store_.write(std::string_view(bytes.constData(), bytes.size())));
    }
    void remove(MutationCompletion completion) override { completion(store_.remove()); }
};
} // namespace
LicenseCredentialStore* createPlatformLicenseCredentialStore(QObject* parent)
{
    return new PlatformLicenseCredentialStore(parent);
}
