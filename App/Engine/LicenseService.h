#pragma once
#include "Data.h"
#include <iiNetwork/Network.h>
#include <iiAcountManager/NativeAccount.h>
namespace congregation
{
struct Credentials final
{
    std::string email, key;
    Credentials() = default;
    Credentials(std::string e, std::string k) : email(std::move(e)), key(std::move(k)) {}
    Credentials(const Credentials&) = delete;
    Credentials& operator=(const Credentials&) = delete;
    Credentials(Credentials&&) = default;
    Credentials& operator=(Credentials&&) = default;
    ~Credentials();
    void clear();
};
struct CredentialPort final
{
    using ReadStatus = iisacc::accounts::native::SecretResult::Status;
    std::function<bool()> supported;
    std::function<void(std::function<void(ReadStatus, std::string)>)> read;
    std::function<void(std::string_view, std::function<void(bool)>)> write;
    std::function<void(std::function<void(bool)>)> remove;
};
class LicenseService final
{
  public:
    enum class StoredStatus
    {
        available,
        not_found,
        unavailable,
        invalid
    };
    enum class Notice
    {
        licensed,
        verifying,
        stored,
        code
    };
    LicenseService(ii::runtime::EventLoop&, std::string endpoint, int timeout, bool enforced,
                   CredentialPort);
    ~LicenseService();
    std::function<void(Notice)> changed;
    std::function<void(bool)> validated;
    bool enforced = true, licensed = false, verifying = false, stored = false;
    std::string code;
    [[nodiscard]] bool persistence() const;
    void restore();
    void validate(std::string email, std::string key);
    void retry();
    void forget();
    void credentials(std::function<void(StoredStatus, Credentials)>);

  private:
    void start(std::string, std::string, bool);
    void finish(ii::network::HttpResponse);
    void clear_stored();
    void set_licensed(bool);
    void set_verifying(bool);
    void set_stored(bool);
    void set_code(std::string);
    void complete(bool);
    ii::network::Http http_;
    std::string endpoint_;
    int timeout_;
    CredentialPort store_;
    Credentials active_, saved_;
    bool uses_saved_ = false;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};
} // namespace congregation
